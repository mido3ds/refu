#include <analyzer/typecheck.h>

#include <Utils/build_assert.h>
#include <Utils/bits.h>
#include <Persistent/buffers.h>
#include <String/rf_str_core.h>

#include <ast/ast.h>
#include <ast/operators.h>
#include <ast/function.h>
#include <ast/block.h>
#include <ast/constants.h>
#include <ast/vardecl.h>
#include <ast/ast_utils.h>
#include <ast/returnstmt.h>
#include <ast/type.h>

#include <types/type.h>
#include <types/type_comparisons.h>
#include <types/type_elementary.h>
#include <types/type_function.h>

#include <analyzer/analyzer.h>
#include <analyzer/symbol_table.h>
#include "analyzer_pass1.h" // for analyzer symbol table change functions
#include "typecheck_matchexpr.h"

// convenience function to set the type of a node and remember last node type during traversal
static inline void traversal_node_set_type(struct ast_node *n,
                                           const struct type *t,
                                           struct analyzer_traversal_ctx *ctx)
{
    n->expression_type = t;
    ctx->last_node_type = t;
}

static bool typecheck_binaryop_get_operands(struct ast_node *n, struct ast_node *left,
                                            const struct type **tleft,
                                            struct ast_node *right, const struct type **tright,
                                            struct analyzer_traversal_ctx *ctx)
{
    *tleft = ast_expression_get_type(left);
    if (!*tleft) {
        analyzer_err(ctx->a, ast_node_startmark(left), ast_node_endmark(left),
                     "Type of left side of \""RF_STR_PF_FMT"\" can not be determined",
                     RF_STR_PF_ARG(ast_binaryop_opstr(n)));
        return false;
    }
    *tright = ast_expression_get_type(right);
    if (!*tright) {
        analyzer_err(ctx->a, ast_node_startmark(right), ast_node_endmark(right),
                     "Type of right side of \""RF_STR_PF_FMT"\" can not be determined",
                     RF_STR_PF_ARG(ast_binaryop_opstr(n)));
        return false;
    }
    return true;
}

static bool i_should_be_changed(struct ast_node *left,
                                struct ast_node *right,
                                struct analyzer_traversal_ctx *ctx)
{
    // TODO: this is just a callback which should never exist. Should check if op is applicable
    // Wherever it's used it's just temporary and should be changed to
    // either a callback per binary operator or something else
    (void)left;
    (void)right;
    (void)ctx;
    return false;
}

static const struct type *typecheck_do_type_conversion(const struct type *left,
                                                       const struct type *right,
                                                       struct analyzer_traversal_ctx *ctx)
{
    // if both are elementary always try to convert the smaller to the bigger size
    if (left->category == TYPE_CATEGORY_ELEMENTARY &&
        right->category == TYPE_CATEGORY_ELEMENTARY) {
        if (right->elementary.etype >= left->elementary.etype &&
            type_compare(left, right, TYPECMP_IMPLICIT_CONVERSION)) {
            // return right but without constant type
            return
                type_is_constant_elementary(right) ?
                type_elementary_get_type(right->elementary.etype) : right;
        } else if (type_compare(right, left, TYPECMP_IMPLICIT_CONVERSION)) {
            // return left but without constant type
            return
                type_is_constant_elementary(left) ?
                type_elementary_get_type(left->elementary.etype) : left;
        }
    //  else try to see if either side can be implicitly converted to another
    } else if (type_compare(left, right, TYPECMP_IMPLICIT_CONVERSION)) {
        return right;
    } else if (type_compare(right, left, TYPECMP_IMPLICIT_CONVERSION)) {
        return left;
    }

    return NULL;
}

static enum traversal_cb_res typecheck_unaryop_generic(struct ast_node *n,
                                                       const struct type *operand_type,
                                                       const char *error_intro,
                                                       const char *error_conj,
                                                       struct analyzer_traversal_ctx *ctx)
{
    const struct type *final_type = NULL;

    // for now only applicable to numeric elementary types
    if (!type_is_numeric_elementary(operand_type)) {
        goto fail;
    }
    final_type = operand_type;
    traversal_node_set_type(n, final_type, ctx);
    return TRAVERSAL_CB_OK;

fail:
    n->expression_type = NULL;
    RFS_buffer_push();
    analyzer_err(ctx->a, ast_node_startmark(n), ast_node_endmark(n),
                 "%s \""RF_STR_PF_FMT"\" %s \""RF_STR_PF_FMT"\"",
                 error_intro, RF_STR_PF_ARG(ast_unaryop_opstr(n)), 
                 error_conj, RF_STR_PF_ARG(type_str(operand_type, false)));
    RFS_buffer_pop();
    return TRAVERSAL_CB_ERROR;
}

// Generic typecheck function for binary operations. If special functionality
// needs to be implemented for an operator do that in a separate function
static enum traversal_cb_res typecheck_binaryop_generic(struct ast_node *n,
                                                        struct ast_node *left,
                                                        struct ast_node *right,
                                                        struct analyzer_traversal_ctx *ctx,
                                                        enum binaryop_type operation,
                                                        bool(*operator_applicable_cb)(struct ast_node*, struct ast_node*, struct analyzer_traversal_ctx*),
                                                        const char *error_intro,
                                                        const char *error_conj)
{
    const struct type *tright;
    const struct type *tleft;
    const struct type *final_type = NULL;

    if (!typecheck_binaryop_get_operands(n, left, &tleft, right, &tright, ctx)) {
        return TRAVERSAL_CB_ERROR;
    }

    final_type = typecheck_do_type_conversion(tleft, tright, ctx);
    if (!final_type && !operator_applicable_cb(left, right, ctx)) {
        RFS_buffer_push();
        analyzer_err(ctx->a, ast_node_startmark(n), ast_node_endmark(n),
                     "%s \""RF_STR_PF_FMT"\" %s \""RF_STR_PF_FMT"\"",
                     error_intro, RF_STR_PF_ARG(type_str(tright, false)),
                     error_conj, RF_STR_PF_ARG(type_str(tleft, false)));
        RFS_buffer_pop();
        return TRAVERSAL_CB_ERROR;
    }

    n->binaryop.common_type = final_type;
    traversal_node_set_type(n, final_type, ctx);
    return TRAVERSAL_CB_OK;
}

// Generic typecheck function for binary operations whose type should be a boolean
static enum traversal_cb_res typecheck_bool_binaryop_generic(struct ast_node *n,
                                                             struct ast_node *left,
                                                             struct ast_node *right,
                                                             struct analyzer_traversal_ctx *ctx,
                                                             enum binaryop_type operation,
                                                             bool(*operator_applicable_cb)(struct ast_node*, struct ast_node*, struct analyzer_traversal_ctx*),
                                                             const char *error_intro,
                                                             const char *error_conj)
{
    const struct type *tright;
    const struct type *tleft;
    const struct type *final_type = NULL;

    if (!typecheck_binaryop_get_operands(n, left, &tleft, right, &tright, ctx)) {
        return TRAVERSAL_CB_ERROR;
    }

    final_type = typecheck_do_type_conversion(tleft, tright, ctx);
    if (!final_type && !operator_applicable_cb(left, right, ctx)) {
        RFS_buffer_push();
        analyzer_err(ctx->a, ast_node_startmark(n), ast_node_endmark(n),
                     "%s \""RF_STR_PF_FMT"\" %s \""RF_STR_PF_FMT"\"",
                     error_intro, RF_STR_PF_ARG(type_str(tright, false)),
                     error_conj, RF_STR_PF_ARG(type_str(tleft, false)));
        RFS_buffer_pop();
        return TRAVERSAL_CB_ERROR;
    }

    n->binaryop.common_type = final_type;
    traversal_node_set_type(n, type_elementary_get_type(ELEMENTARY_TYPE_BOOL), ctx);

    return TRAVERSAL_CB_OK;
}

static enum traversal_cb_res typecheck_arrayref(struct ast_node *n,
                                                struct ast_node *left,
                                                struct ast_node *right,
                                                struct analyzer_traversal_ctx *ctx)
{
    // TODO
    (void)n;
    (void)left;
    (void)right;
    (void)ctx;
    return TRAVERSAL_CB_OK;
}

struct typecheck_member_access_iter_ctx {
    struct ast_node *member_identifier;
    struct type *member_type;
};

static inline void typecheck_member_access_iter_ctx_init(struct typecheck_member_access_iter_ctx *ctx,
                                                         struct ast_node *member_identifier)
{
    ctx->member_identifier = member_identifier;
    ctx->member_type = NULL;
}

static enum traversal_cb_res typecheck_member_access_iter_cb(const struct type_leaf *t,
                                                             struct typecheck_member_access_iter_ctx *ctx)
{
    // for now assuming all right parts of member access are identifiers
    if (rf_string_equal(t->id, ast_identifier_str(ctx->member_identifier))) {
        ctx->member_type = t->type;
        ctx->member_identifier->expression_type = t->type;
        return TRAVERSAL_CB_OK_AND_STOP;
    }

    return TRAVERSAL_CB_ERROR;
}

static enum traversal_cb_res typecheck_member_access(struct ast_node *n,
                                                     struct ast_node *left,
                                                     struct ast_node *right,
                                                     struct analyzer_traversal_ctx *ctx)
{
    const struct type *tleft;
    struct typecheck_member_access_iter_ctx member_access_iter_ctx;
    enum traversal_cb_res rc;

    RFS_buffer_push();
    tleft = ast_expression_get_type(left);

    if (!tleft) {
        analyzer_err(ctx->a, ast_node_startmark(left),
                     ast_node_endmark(left),
                     "Undeclared identifier \""RF_STR_PF_FMT"\" as left part of "
                     "member access operator",
                     RF_STR_PF_ARG(ast_identifier_str(left)));
        return TRAVERSAL_CB_ERROR;
    }

    // left type of member access should be a custom defined type
    if (!type_category_equals(tleft, TYPE_CATEGORY_DEFINED)) {
        analyzer_err(ctx->a, ast_node_startmark(left),
                     ast_node_endmark(left),
                     "Left part of member access operator \""RF_STR_PF_FMT"\"is "
                     "not a user defined type",
                     RF_STR_PF_ARG(ast_node_str(left)));
        return TRAVERSAL_CB_ERROR;
    }

    // right type of member access should be an identifier (at least for now)
    if (right->type != AST_IDENTIFIER) {
        analyzer_err(ctx->a, ast_node_startmark(right),
                     ast_node_endmark(right),
                     "Right part of member access operator is not an identifier"
                     " but is \""RF_STR_PF_FMT"\"",
                     RF_STR_PF_ARG(ast_node_str(right)));
        return TRAVERSAL_CB_ERROR;
    }

    typecheck_member_access_iter_ctx_init(&member_access_iter_ctx, right);
    rc = type_for_each_leaf_nostop(tleft->defined.type,
                                   (leaf_type_nostop_cb)typecheck_member_access_iter_cb,
                                   &member_access_iter_ctx);
    if (!traversal_success(rc)) {
        analyzer_err(ctx->a, ast_node_startmark(n),
                     ast_node_endmark(n),
                     "Could not find member \""RF_STR_PF_FMT"\" in type \""
                     RF_STR_PF_FMT"\"",
                     RF_STR_PF_ARG(ast_identifier_str(right)),
                     RF_STR_PF_ARG(type_str(tleft, false)));
        return TRAVERSAL_CB_ERROR;
    }

    traversal_node_set_type(n, member_access_iter_ctx.member_type, ctx);

    RFS_buffer_pop();
    return TRAVERSAL_CB_OK;
}

static enum traversal_cb_res typecheck_constant(struct ast_node *n,
                                                struct analyzer_traversal_ctx *ctx)
{
    traversal_node_set_type(n, ast_constant_get_storagetype(n), ctx);
    return (n->expression_type) ? TRAVERSAL_CB_OK : TRAVERSAL_CB_ERROR;
}

// figure out if any of the wildcard's close parents is a matchcase
static bool wilcard_parent_is_matchcase(const struct ast_node *n, void *user_arg)
{
    return n->type == AST_MATCH_CASE;
}

static enum traversal_cb_res typecheck_identifier(struct ast_node *n,
                                                  struct analyzer_traversal_ctx *ctx)
{
    AST_NODE_ASSERT_TYPE(n, AST_IDENTIFIER);
    struct ast_node *parent = analyzer_traversal_ctx_get_current_parent(ctx);
    if (ast_identifier_is_wildcard(n)) {
        if (!analyzer_traversal_ctx_traverse_parents(ctx,
                                                    wilcard_parent_is_matchcase,
                                                    NULL)) {
            analyzer_err(ctx->a, ast_node_startmark(n),
                         ast_node_endmark(n),
                         "Reserved wildcard identifier '_' used outside of "
                         " a match expression");
            return TRAVERSAL_CB_ERROR;
        }
        traversal_node_set_type(n, type_get_wildcard(), ctx);
        return TRAVERSAL_CB_OK;
    }
    
    traversal_node_set_type(n,
                            type_lookup_identifier_string(ast_identifier_str(n),
                                                          ctx->current_st),
                            ctx);

    // for some identifiers, like for the right part of a member access it's
    // impossible to determine type at this stage, for the rest it's an error
    if (!n->expression_type &&
        !ast_node_is_specific_binaryop(parent, BINARYOP_MEMBER_ACCESS)) {
        analyzer_err(ctx->a, ast_node_startmark(n),
                     ast_node_endmark(n),
                     "Undeclared identifier \""RF_STR_PF_FMT"\"",
                     RF_STR_PF_ARG(ast_identifier_str(n)));
        return TRAVERSAL_CB_ERROR;
    }

    return TRAVERSAL_CB_OK;
}

static enum traversal_cb_res typecheck_xidentifier(struct ast_node *n,
                                                   struct analyzer_traversal_ctx *ctx)
{
    traversal_node_set_type(n, n->xidentifier.id->expression_type, ctx);
    return TRAVERSAL_CB_OK;
}

static enum traversal_cb_res typecheck_typedesc(struct ast_node *n,
                                                struct analyzer_traversal_ctx *ctx)
{
    // a type descriptions's type is the type of the right part of the description
    traversal_node_set_type(n, ast_typedesc_right(n)->expression_type, ctx);
    return TRAVERSAL_CB_OK;
}

static enum traversal_cb_res typecheck_typedecl(struct ast_node *n,
                                                struct analyzer_traversal_ctx *ctx)
{
    traversal_node_set_type(n, ast_typedecl_typedesc_get(n)->expression_type, ctx);
    return TRAVERSAL_CB_OK;
}


static bool analyzer_types_assignable(struct ast_node *left,
                                      struct ast_node *right,
                                      struct analyzer_traversal_ctx *ctx)
{
    // TODO: Check if two types that are not strictly equal can still be assignable
    // if for example there is an assignment typeclass instance
    (void)left;
    (void)right;
    (void)ctx;
    return false;
}

static enum traversal_cb_res typecheck_assignment(struct ast_node *n,
                                                  struct ast_node *left,
                                                  struct ast_node *right,
                                                  struct analyzer_traversal_ctx *ctx)
{
    const struct type *tright;
    const struct type *tleft;
    const struct type *final_type = NULL;

    // left side of an assignment should be an identifier or a variable declaration
    if (left->type != AST_IDENTIFIER && left->type != AST_VARIABLE_DECLARATION) {
        analyzer_err(ctx->a, ast_node_startmark(left),
                     ast_node_endmark(left),
                     "Expected an identifier or a variable declaration "
                     "as left part of the assignment "
                     "but found a \""RF_STR_PF_FMT"\"",
                     RF_STR_PF_ARG(ast_node_str(left)));
        return TRAVERSAL_CB_ERROR;
    }


    if (!typecheck_binaryop_get_operands(n, left, &tleft, right, &tright, ctx)) {
        return TRAVERSAL_CB_ERROR;
    }

    typecmp_ctx_set_flags(TYPECMP_FLAG_ASSIGNMENT);
    if (type_compare(tright, tleft, TYPECMP_IMPLICIT_CONVERSION)) {
        final_type = tleft;
        if (typecmp_ctx_have_warning()) {
            const struct RFstring *warning;
            while ((warning = typecmp_ctx_get_next_warning())) {
                analyzer_warn(ctx->a, ast_node_startmark(n), ast_node_endmark(n),
                              RF_STR_PF_FMT" during assignment.",
                              RF_STR_PF_ARG(warning));
            }
        }
    } else {
        if (!analyzer_types_assignable(left, right, ctx)) {
            RFS_buffer_push();
            analyzer_err(ctx->a, ast_node_startmark(n), ast_node_endmark(n),
                         "Assignment between incompatible types. Can't assign "
                         "\""RF_STR_PF_FMT"\" to \""RF_STR_PF_FMT"\""
                         "%s"RF_STR_PF_FMT".",
                         RF_STR_PF_ARG(type_str(tright, false)),
                         RF_STR_PF_ARG(type_str(tleft, false)),
                         typecmp_ctx_have_error() ? ". " : "",
                         RF_STR_PF_ARG(typecmp_ctx_get_error()));
            RFS_buffer_pop();
            return TRAVERSAL_CB_ERROR;
        }
        // temporary
        RF_ASSERT(false, "analyzer_types_assignable() should never return true for now");
    }

    // set the type of assignment as the type of the left operand
    traversal_node_set_type(n, final_type, ctx);
    return TRAVERSAL_CB_OK;
}

static enum traversal_cb_res typecheck_comma(struct ast_node *n,
                                             struct ast_node *left,
                                             struct ast_node *right,
                                             struct analyzer_traversal_ctx *ctx)
{
    const struct type *tright;
    const struct type *tleft;

    if (!typecheck_binaryop_get_operands(n, left, &tleft, right, &tright, ctx)) {
        return TRAVERSAL_CB_ERROR;
    }

    // for now at least let's assume that all types can have the comma
    // operator applied to them

    // create the comma type
    traversal_node_set_type(n,
                            type_create_from_operation(TYPEOP_PRODUCT,
                                                       (struct type*)tleft,
                                                       (struct type*)tright,
                                                       ctx->a),
                            ctx);
    if (!n->expression_type) {
        RF_ERROR("Could not create a type as a product of 2 other types.");
        return TRAVERSAL_CB_FATAL_ERROR;
    }

    return TRAVERSAL_CB_OK;
}

static enum traversal_cb_res typecheck_function_call(struct ast_node *n,
                                                     struct analyzer_traversal_ctx *ctx)
{
    const struct RFstring *fn_name;
    const struct type *fn_type;
    const struct type *fn_declared_args_type;
    const struct type *fn_found_args_type;
    struct ast_node *fn_call_args = ast_fncall_args(n);

    // check for existence of function
    fn_name = ast_fncall_name(n);
    fn_type = type_lookup_identifier_string(fn_name, ctx->current_st);
    if (!fn_type || !type_is_callable(fn_type)) {
        analyzer_err(ctx->a, ast_node_startmark(n),
                     ast_node_endmark(n),
                     "Undefined function call \""RF_STR_PF_FMT"\" detected",
                     RF_STR_PF_ARG(fn_name));
        goto fail;
    }

    fn_found_args_type = (fn_call_args) ? ast_expression_get_type(fn_call_args)
        : type_elementary_get_type(ELEMENTARY_TYPE_NIL);

    if (!fn_found_args_type) { // argument typechecking failed
        goto fail;
    }
    
    if (type_is_explicitly_convertable_elementary(fn_type)) {
        // silly way to check if it's only 1 argument. Maybe figure out safer way?
        if (!fn_call_args || fn_found_args_type->category == TYPE_CATEGORY_OPERATOR) {
            analyzer_err(ctx->a, ast_node_startmark(n),
                         ast_node_endmark(n),
                         "Invalid arguments for explicit conversion to \""
                         RF_STR_PF_FMT"\".",
                         RF_STR_PF_ARG(fn_name));
            goto fail;
        }

        // check if the explicit conversion is valid
        if (!type_compare(fn_found_args_type, fn_type, TYPECMP_EXPLICIT_CONVERSION)) {
            analyzer_err(ctx->a, ast_node_startmark(n), ast_node_endmark(n),
                         "Invalid explicit conversion. "RF_STR_PF_FMT".",
                         RF_STR_PF_ARG(typecmp_ctx_get_error()));
            goto fail;
        }
        
    } else {
        //check that the types of its arguments do indeed match
        fn_declared_args_type = type_callable_get_argtype(fn_type);
        typecmp_ctx_set_flags(TYPECMP_FLAG_FUNCTION_CALL);
        if (!type_compare(fn_found_args_type, fn_declared_args_type, TYPECMP_IMPLICIT_CONVERSION)) {
            RFS_buffer_push();
            analyzer_err(ctx->a, ast_node_startmark(n), ast_node_endmark(n),
                         RF_STR_PF_FMT" "RF_STR_PF_FMT"() is called with argument type of "
                         "\""RF_STR_PF_FMT"\" which does not match the expected "
                         "type of \""RF_STR_PF_FMT"\"%s"RF_STR_PF_FMT".",
                         RF_STR_PF_ARG(type_callable_category_str(fn_type)),
                         RF_STR_PF_ARG(fn_name),
                         RF_STR_PF_ARG(type_str(fn_found_args_type, false)),
                         RF_STR_PF_ARG(type_str(fn_declared_args_type, false)),
                         typecmp_ctx_have_error() ? ". " : "",
                         RF_STR_PF_ARG(typecmp_ctx_get_error()));
            RFS_buffer_pop();
            goto fail;
        }
    }

    traversal_node_set_type(n, type_callable_get_rettype(fn_type), ctx);
    return TRAVERSAL_CB_OK;

fail:
    traversal_node_set_type(n, NULL, ctx);
    return TRAVERSAL_CB_ERROR;
}

static enum traversal_cb_res typecheck_return_stmt(struct ast_node *n,
                                                   struct analyzer_traversal_ctx *ctx)
{
    struct ast_node *fn_decl = symbol_table_get_fndecl(ctx->current_st);
    enum traversal_cb_res ret = TRAVERSAL_CB_OK;
    const struct type *fn_type;
    if (!fn_decl) {
        analyzer_err(ctx->a, ast_node_startmark(n), ast_node_endmark(n),
                     "Return statement outside of function body");
        return TRAVERSAL_CB_ERROR;
    }

    // at this stage the function declaration won't have been typechecked.
    // Do it now .. which is not optimal since it will happen again when going upwards
    // TODO: perhaps add a check in the typecheck reverse tree traversal to not visit
    // certain subtrees if they are already typechecked.
    if (!analyzer_typecheck(ctx->a, fn_decl)) {
        return TRAVERSAL_CB_ERROR;
    }

    fn_type = ast_expression_get_type(fn_decl);
    const struct type *fn_ret_type = type_function_get_rettype(fn_type);
    const struct type *found_ret_type = ast_expression_get_type(ast_returnstmt_expr_get(n));

    if (!found_ret_type) {
        return TRAVERSAL_CB_ERROR;
    }

    if (!type_compare(fn_ret_type, found_ret_type, TYPECMP_IMPLICIT_CONVERSION)) {
        RFS_buffer_push();
        analyzer_err(ctx->a, ast_node_startmark(n), ast_node_endmark(n),
                     "Return statement type \""RF_STR_PF_FMT"\" does not match "
                     "the expected return type of \""RF_STR_PF_FMT"\"",
                     RF_STR_PF_ARG(type_str(found_ret_type, false)),
                     RF_STR_PF_ARG(type_str(fn_ret_type, false)));
        RFS_buffer_pop();
        ret = TRAVERSAL_CB_ERROR;
    }
    traversal_node_set_type(n, found_ret_type, ctx);
    return ret;
}

static enum traversal_cb_res typecheck_block(struct ast_node *n,
                                             struct analyzer_traversal_ctx *ctx)
{
    // a block's type is the type of it's last expression
    n->expression_type = ctx->last_node_type;
    return TRAVERSAL_CB_OK;
}

static enum traversal_cb_res typecheck_binaryop(struct ast_node *n,
                                                struct analyzer_traversal_ctx *ctx)
{
    struct ast_node *left = ast_binaryop_left(n);
    struct ast_node *right = ast_binaryop_right(n);
    enum binaryop_type bop_type = ast_binaryop_op(n);

    switch (bop_type) {
    case BINARYOP_ASSIGN:
        return typecheck_assignment(n, left, right, ctx);
    case BINARYOP_COMMA:
        return typecheck_comma(n, left, right, ctx);

    case BINARYOP_ADD:
        return typecheck_binaryop_generic(
            n, left, right, ctx, bop_type, i_should_be_changed,
            "Can't add", "to");
    case BINARYOP_SUB:
        return typecheck_binaryop_generic(
            n, left, right, ctx, bop_type, i_should_be_changed,
            "Can't subtract", "from");
    case BINARYOP_MUL:
        return typecheck_binaryop_generic(
            n, left, right, ctx, bop_type, i_should_be_changed,
            "Can't multiply", "by");
    case BINARYOP_DIV:
        return typecheck_binaryop_generic(
            n, left, right, ctx, bop_type, i_should_be_changed,
            "Can't divide", "by");

    case BINARYOP_CMP_EQ:
        return typecheck_bool_binaryop_generic(
            n, left, right, ctx, bop_type, i_should_be_changed,
            "Can't compare if", "is equal to");
    case BINARYOP_CMP_NEQ:
        return typecheck_bool_binaryop_generic(
            n, left, right, ctx, bop_type, i_should_be_changed,
            "Can't compare if", "is not equal to");
    case BINARYOP_CMP_GT:
        return typecheck_bool_binaryop_generic(
            n, left, right, ctx, bop_type, i_should_be_changed,
            "Can't compare if", "is greater than");
    case BINARYOP_CMP_GTEQ:
        return typecheck_bool_binaryop_generic(
            n, left, right, ctx, bop_type, i_should_be_changed,
            "Can't compare if", "is greater than or equal");
    case BINARYOP_CMP_LT:
        return typecheck_bool_binaryop_generic(
            n, left, right, ctx, bop_type, i_should_be_changed,
            "Can't compare if", "is less than");
    case BINARYOP_CMP_LTEQ:
        return typecheck_bool_binaryop_generic(
            n, left, right, ctx, bop_type, i_should_be_changed,
            "Can't compare if", "is less than or equal");

    case BINARYOP_LOGIC_AND:
        return typecheck_bool_binaryop_generic(
            n, left, right, ctx, bop_type, i_should_be_changed,
            "Can't apply logic operator && between", "and");
    case BINARYOP_LOGIC_OR:
        return typecheck_bool_binaryop_generic(
            n, left, right, ctx, bop_type, i_should_be_changed,
            "Can't apply logic operator || between", "and");

    case BINARYOP_BITWISE_OR:
        return typecheck_binaryop_generic(
            n, left, right, ctx, bop_type, i_should_be_changed,
            "Can't apply bitwise OR to", "and");
    case BINARYOP_BITWISE_AND:
        return typecheck_binaryop_generic(
            n, left, right, ctx, bop_type, i_should_be_changed,
            "Can't apply bitwise AND to", "and");
    case BINARYOP_BITWISE_XOR:
        return typecheck_binaryop_generic(
            n, left, right, ctx, bop_type, i_should_be_changed,
            "Can't apply bitwise XOR to", "and");

    case BINARYOP_ARRAY_REFERENCE:
        return typecheck_arrayref(n, left, right, ctx);
    case BINARYOP_MEMBER_ACCESS:
        return typecheck_member_access(n, left, right, ctx);

    default:
        RF_ASSERT(false, "Typechecking for unimplemented binary "
                  "operator "RF_STR_PF_FMT,
                  RF_STR_PF_ARG(ast_binaryop_opstr(n)));
        return TRAVERSAL_CB_FATAL_ERROR;
    }

    return TRAVERSAL_CB_OK;
}

static enum traversal_cb_res typecheck_unaryop(struct ast_node *n,
                                               struct analyzer_traversal_ctx *ctx)
{
    enum unaryop_type uop_type = ast_unaryop_op(n);
    const struct type *operand_type;

    operand_type = ast_expression_get_type(ast_unaryop_operand(n));
    if (!operand_type) {
        analyzer_err(ctx->a, ast_node_startmark(n), ast_node_endmark(n),
                     "Type for operand of \""RF_STR_PF_FMT"\" can not be determined",
                     RF_STR_PF_ARG(ast_unaryop_opstr(n)));
        return TRAVERSAL_CB_ERROR;
    }

    switch (uop_type) {
    case UNARYOP_MINUS:
        return typecheck_unaryop_generic(n, operand_type, "Can't apply", "to", ctx);
    case UNARYOP_PLUS:
        return typecheck_unaryop_generic(n, operand_type, "Can't apply", "to", ctx);
    case UNARYOP_INC:
        return typecheck_unaryop_generic(n, operand_type, "Can't apply", "to", ctx);
    case UNARYOP_DEC:
        return typecheck_unaryop_generic(n, operand_type, "Can't apply", "to", ctx);
    default:
        RF_ASSERT(false, "Typechecking for unimplemented unary "
                  "operator "RF_STR_PF_FMT,
                  RF_STR_PF_ARG(ast_unaryop_opstr(n)));
        return TRAVERSAL_CB_FATAL_ERROR;
    }

    return TRAVERSAL_CB_OK;
}

static enum traversal_cb_res typecheck_fndecl(struct ast_node *n,
                                              struct analyzer_traversal_ctx *ctx)
{
    struct type *t;
    t = type_lookup_identifier_string(ast_fndecl_name_str(n), ctx->current_st);
    if (!t) {
        RF_ERROR("Function declaration name not found in the symbol table at impossible point");
        return TRAVERSAL_CB_ERROR;
    }
    traversal_node_set_type(n, t, ctx);
    return TRAVERSAL_CB_OK;
}


static enum traversal_cb_res typecheck_do(struct ast_node *n,
                                          void *user_arg)
{
    struct analyzer_traversal_ctx *ctx = (struct analyzer_traversal_ctx*)user_arg;
    enum traversal_cb_res ret = TRAVERSAL_CB_OK;
    switch(n->type) {
    case AST_BINARY_OPERATOR:
        ret = typecheck_binaryop(n, ctx);
        break;
    case AST_UNARY_OPERATOR:
        ret = typecheck_unaryop(n, ctx);
        break;
    case AST_IDENTIFIER:
        ret = typecheck_identifier(n, ctx);
        break;
    case AST_XIDENTIFIER:
        ret = typecheck_xidentifier(n, ctx);
        break;
    case AST_TYPE_DECLARATION:
        ret = typecheck_typedecl(n, ctx);
        break;
    case AST_TYPE_DESCRIPTION:
        ret = typecheck_typedesc(n, ctx);
        break;
    case AST_CONSTANT:
        ret = typecheck_constant(n, ctx);
        break;
    case AST_STRING_LITERAL:
        traversal_node_set_type(n,
                                type_elementary_get_type_constant(ELEMENTARY_TYPE_STRING),
                                ctx);
        break;
    case AST_VARIABLE_DECLARATION:
        // for a variable definition, the variable's type description should be
        // the type of the expression
        traversal_node_set_type(n,
                                ast_vardecl_desc_get(n)->expression_type,
                                ctx);
        break;
    case AST_FUNCTION_CALL:
        ret = typecheck_function_call(n, ctx);
        break;
    case AST_FUNCTION_DECLARATION:
        ret = typecheck_fndecl(n, ctx);
        break;
    case AST_FUNCTION_IMPLEMENTATION:
        traversal_node_set_type(n,
                                ast_expression_get_type(ast_fnimpl_fndecl_get(n)),
                                ctx);
        break;
    case AST_RETURN_STATEMENT:
        ret = typecheck_return_stmt(n, ctx);
        break;
    case AST_BLOCK:
        ret = typecheck_block(n, ctx);
        break;
    case AST_MATCH_EXPRESSION:
        ret = typecheck_matchexpr(n, ctx);
        break;
    default:
        // do nothing. Think what to do for the remaining nodes if anything ...
        break;
    }

    // go back to previous parent
    (void)darray_pop(ctx->parent_nodes);
    // also change symbol table upwards if needed
    analyzer_handle_symbol_table_ascending(n, ctx);
    return ret;
}

bool analyzer_typecheck(struct analyzer *a, struct ast_node *root)
{
    struct analyzer_traversal_ctx ctx;
    analyzer_traversal_ctx_init(&ctx, a);

    bool ret = (TRAVERSAL_CB_OK == ast_traverse_tree_nostop_post_cb(
                    root,
                    (ast_node_cb)analyzer_handle_symbol_table_descending,
                    &ctx,
                    typecheck_do,
                    &ctx));

    analyzer_traversal_ctx_deinit(&ctx);
    return ret;
}


