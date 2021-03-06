#include <ir/rir_call.h>

#include <rfbase/string/core.h>
#include <rfbase/string/manipulationx.h>

#include <ir/rir.h>
#include <ir/rir_argument.h>
#include <ir/rir_block.h>
#include <ir/rir_binaryop.h>
#include <ir/rir_constant.h>
#include <ir/rir_convert.h>
#include <ir/rir_function.h>
#include <ir/rir_object.h>
#include <ir/rir_process.h>
#include <ast/function.h>
#include <types/type.h>
#include <types/type_operators.h>
#include <types/type_function.h>

static bool ctor_args_to_value(
    struct ast_node *call,
    struct rir_value *lhs,
    struct rir_ctx *ctx)
{
    struct arr_ast_nodes *args = ast_fncall_arguments(call);
    struct ast_node **n;
    unsigned int index = 0;
    darray_foreach(n, *args) {
        // for each argument, process the ast node and create the rir arg expression
        const struct rir_value *argexprval = rir_process_ast_node_getreadval(*n, ctx);
        if (!argexprval) {
            RF_ERROR("Could not create rir expression from constructor argument");
            return false;
        }

        struct rir_expression *e;
        // find the target value to write to
        struct rir_value *targetval;
        switch (lhs->type->category) {
        case RIR_TYPE_COMPOSITE:
            // if lhs type is composite create expression to read its index from the lhs composite type
            if (!(e = rir_objmemberat_create(lhs, index, RIRPOS_AST, ctx))) {
                RF_ERROR("Failed to create rir expression to get rir object's member memory");
                return false;
            }
            rir_common_block_add(&ctx->common, e);
            if (!(targetval = rirctx_getread_exprval(e, ctx))) {
                RF_ERROR("Failed to create rir expression to read an object's value");
                return false;
            }
            break;
        case RIR_TYPE_ELEMENTARY:
            RF_ASSERT(
                index == 0,
                "Only at 0 index can a non composite type have been found."
            );
            targetval = lhs;
            break;
        case RIR_TYPE_ARRAY:
            RF_ASSERT(false, "TODO");
            break;
        }

        // write the arg expression to the position
        if (!(e = rir_write_create(targetval, argexprval, RIRPOS_AST, ctx))) {
            RF_ERROR("Failed to create expression to write to an object's member");
            return false;
        }
        rir_common_block_add(&ctx->common, e);

        ++index;
    }
    return true;
}

/**
 * Populate an object's memory from arguments of an ast function call
 * @param objmemory        The rir value of the object's memory to populate
 * @param ast_call         The ast function call to populate from
 * @param ctx              The rir ctx
 * @return                 True for success
 */
static bool rir_populate_from_astcall(
    struct rir_value *objmemory,
    const struct ast_node *ast_call,
    struct rir_ctx *ctx)
{
    if (type_is_sumtype(ast_fncall_type(ast_call))) {
        RF_ASSERT(rir_type_is_composite(objmemory->type), "Constructor should assign to a composite type");
        int union_idx = rir_type_union_matched_type_from_fncall(objmemory->type, ast_call, ctx);
        if (union_idx == -1) {
            RF_ERROR("RIR sum constructor not matching any part of the original type");
            return false;
        }
        // create code to set the  union's index with the matching type
        struct rir_value *rir_idx_const = rir_constantval_create_fromint32(union_idx, ctx->common.rir);
        struct rir_expression *e = rir_setunionidx_create(objmemory, rir_idx_const, RIRPOS_AST, ctx);
        if (!e) {
            return false;
        }
        rir_common_block_add(&ctx->common, e);
        // create code to load the appropriate union subtype for reading
        struct rir_expression *ummbr_ptr = rir_unionmemberat_create(objmemory, union_idx, RIRPOS_AST, ctx);
        if (!ummbr_ptr) {
            return false;
        }
        rir_common_block_add(&ctx->common, ummbr_ptr);
        if (!(objmemory = rirctx_getread_exprval(ummbr_ptr, ctx))) {
            return false;
        }
    }
    // now for whichever object (normal type, or union type sumtype) is loaded
    // as left hand side assign from constructor's arguments
    return ctor_args_to_value((struct ast_node*)ast_call, objmemory, ctx);
}

static bool rir_process_ctorcall(const struct ast_node *n, struct rir_ctx *ctx)
{
    struct rir_value *lhs = rir_ctx_lastassignval_get(ctx);
    struct rir_object *obj = ctx->returned_obj;
    if (!lhs) {
        // that means it's a constructor call nested inside something else
        struct rir_type *calltype = rir_type_create_from_type(
            ast_fncall_type(n),
            RIR_LOC_SYMBOL_TABLE_ALLOCA,
            ctx
        );
        if (!(obj = rir_alloca_create_obj(calltype, NULL, RIRPOS_AST, ctx))) {
            RF_ERROR("Failed to create a rir alloca instruction");
            return false;
        }
        rir_common_block_add(&ctx->common, &obj->expr);
        lhs = &obj->expr.val;
    }
    if (!rir_populate_from_astcall(lhs, n, ctx)) {
        return false;
    }
    RIRCTX_RETURN_EXPR(ctx, true, obj);
}

/* -- code to process a normal function call -- */
/**
 * Turn the ast node arguments of a function call into a rir value array
 *
 * @param call[in]         The function call ast node to use
 * @param arr[out]         Pass an unitinialized rir value array here to get
 *                         it initialized and populated by this function
 * @param ctx[in]          The rir context
 */
static bool ast_fncall_args_toarr(
    struct ast_node *call,
    struct value_arr *arr,
    struct rir_ctx *ctx)
{

    struct arr_ast_nodes *args = ast_fncall_arguments(call);
    const struct type *fndecl_type = ast_fncall_type(call);
    struct ast_node **n;
    darray_init(*arr);
    darray_foreach(n, *args) {
        const struct rir_value *argexprval = rir_process_ast_node_getreadval(*n, ctx);
        if (!argexprval) {
            RF_ERROR("Could not create rir expression from fncall argument");
            goto fail;
        }

        const struct type *argtype = fndecl_type->category == TYPE_CATEGORY_OPERATOR
            ? type_get_subtype(fndecl_type, darray_size(*arr))
            : fndecl_type;
        argexprval = rir_maybe_convert(
            argexprval,
            rir_type_create_from_type(argtype, RIR_LOC_FNCALL_MAYBE_CONVERT, ctx),
            RIRPOS_AST,
            ctx
        );
        if (!argexprval) {
            RF_ERROR("Could not create conversion for rir call argument");
            goto fail;
        }
        darray_append(*arr, (struct rir_value*)argexprval);
    }

    return true;

fail:
    darray_free(*arr);
    return false;
}

struct rir_object *rir_call_create_obj(
    const struct RFstring *name,
    struct value_arr *args,
    bool is_foreign,
    enum rir_pos pos,
    rir_data data
)
{
    struct rir_object *ret = rir_object_create(RIR_OBJ_EXPRESSION, rir_data_rir(data));
    if (!ret) {
        return NULL;
    }

    if (!rf_string_copy_in(&ret->expr.call.name, name)) {
        goto fail;
    }
    darray_shallow_copy(ret->expr.call.args, *args);
    ret->expr.call.foreign = is_foreign;

    // now initialize the rir expression part of the struct
    if (!rir_object_expression_init(ret, RIR_EXPRESSION_CALL, pos, data)) {
        goto fail;
    }

    return ret;

fail:
    free(ret);
    return NULL;
}

struct rir_object *rir_call_create_obj_from_ast(const struct ast_node *n, struct rir_ctx *ctx)
{
    struct value_arr arr;
    struct rir_object *ret;
    if (ast_fncall_is_sum(n)) {
        // if it's a call to a function with a sum type, get the type the call matched
        struct rir_type *sumtype = rir_type_create_from_type(
            ast_fncall_type(n),
            RIR_LOC_FNCALL_SUM_CALL,
            ctx
        );
        if (!sumtype) {
            RF_ERROR("Could not get the rir type of a sum function call");
            return NULL;
        }
        // create an alloca for that type
        struct rir_expression *e = rir_alloca_create(sumtype, NULL, RIRPOS_AST, ctx);
        if (!e) {
            RF_ERROR("Failed to create a rir alloca instruction");
            return NULL;
        }
        rir_common_block_add(&ctx->common, e);
        // populate the memory of the sumtype
        if (!rir_populate_from_astcall(&e->val, n, ctx)) {
            RF_ERROR("Failed to rir union type's memory");
            return NULL;
        }
        // then pass that as the only argument to the call
        darray_init(arr);
        darray_append(arr, &e->val);
    } else {
        // turn the function call args into a rir value array
        if (!ast_fncall_args_toarr((struct ast_node*)n, &arr, ctx)) {
            return NULL;
        }
    }

    ret = rir_call_create_obj(
        ast_fncall_name(n),
        &arr,
        ast_fncall_is_foreign(n),
        RIRPOS_AST,
        ctx
    );
    return ret;
}

bool rir_process_fncall(const struct ast_node *n, struct rir_ctx *ctx)
{
    const struct RFstring *fn_name = ast_fncall_name(n);
    const struct type *fn_type;
    // if we are in a block start check from there. If not simply search in the module
    fn_type = type_lookup_identifier_string(fn_name, rir_ctx_curr_st(ctx));
    if (!fn_type) {
        RF_ERROR("No function call of a given name could be found");
        return false;
    }

    if (fn_type->category == TYPE_CATEGORY_DEFINED) { // a constructor
        return rir_process_ctorcall(n, ctx);
    } else if (ast_fncall_is_conversion(n)) {
        return rir_process_convertcall(n, ctx);
    }
    // else normal function call
    struct rir_object *cobj = rir_call_create_obj_from_ast(n, ctx);
    if (!cobj) {
        RF_ERROR("Could not create a rir function call instruction");
        return false;
    }
    rir_common_block_add(&ctx->common, &cobj->expr);
    RIRCTX_RETURN_EXPR(ctx, true, cobj);
}

bool rir_call_tostring(struct rirtostr_ctx *ctx, const struct rir_expression *cexpr)
{
    bool ret = false;
    RFS_PUSH();
    if (rir_value_is_nil(&cexpr->val)) {
        if (!rf_stringx_append(
                ctx->rir->buff,
                RFS(
                    RIRTOSTR_INDENT"call("RFS_PF", %s",
                    RFS_PA(&cexpr->call.name),
                    cexpr->call.foreign ? "foreign" : "defined"
                )
            )) {
            goto end;
        }
    } else {
        if (!rf_stringx_append(
                ctx->rir->buff,
                RFS(
                    RIRTOSTR_INDENT RFS_PF" = call("RFS_PF", %s",
                    RFS_PA(rir_value_string(&cexpr->val)),
                    RFS_PA(&cexpr->call.name),
                    cexpr->call.foreign ? "foreign" : "defined"
                )
            )) {
            goto end;
        }
    }

    if (!rir_valuearr_tostring_close(ctx, &cexpr->call.args)) {
        goto end;
    }
    // success
    ret = true;

end:
    RFS_POP();
    return ret;
}

struct rir_type *rir_call_return_type(struct rir_call *c, struct rir_common *cm)
{
    struct rir_fndecl *decl = rir_fndecl_byname(cm->rir, &c->name);
    RF_ASSERT(decl, "At this point in the RIR the function declaration should have been found");
    return decl->return_type;
}

void rir_call_deinit(struct rir_call *c)
{
    rf_string_deinit(&c->name);
    darray_free(c->args);
    // the args are rir objects themselves so will be freed from global list
}

struct rir_expression *rir_call_to_expr(const struct rir_call *c)
{
    return container_of(c, struct rir_expression, call);
}
