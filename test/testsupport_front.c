#include "testsupport_front.h"

#include <refu.h>
#include <Definitions/threadspecific.h>

#include <compiler.h>
#include <ast/type.h>
#include <ast/string_literal.h>
#include <ast/constants.h>
#include <ast/operators.h>
#include <ast/vardecl.h>
#include <ast/block.h>
#include <ast/function.h>
#include <ast/module.h>
#include <types/type_comparisons.h>
#include <lexer/lexer.h>
#include <parser/parser.h>
#include <analyzer/analyzer.h>

#include <stdarg.h>

i_THREAD__ struct front_testdriver __front_testdriver;

struct front_testdriver *get_front_testdriver()
{
    return &__front_testdriver;
}

struct front_ctx *front_testdriver_curr_front()
{
    struct front_ctx *front = get_front_testdriver()->current_front;
    if (front) {
        return front;
    }

    ck_assert_msg(!rf_ilist_is_empty(&get_front_testdriver()->compiler->front_ctxs),
                  "Attempted to request a front_ctx while the compiler has an empty list of fronts");
    
    return get_front_testdriver()->current_front = rf_ilist_top(&get_front_testdriver()->compiler->front_ctxs, struct front_ctx, ln);
}

void front_testdriver_set_curr_front(unsigned i)
{
    unsigned count = 0;
    struct front_ctx *front;
    rf_ilist_for_each(&get_front_testdriver()->compiler->front_ctxs, front, ln) {
        if (i == count) {
            get_front_testdriver()->current_front = front;
            return;
        }
    }
    ck_abort_msg("Attempted to set non-existant front_ctx with index %u as current.", i);
}

void front_testdriver_set_curr_module(unsigned i)
{
    unsigned count = 0;
    struct module **mod;
    darray_foreach(mod, get_front_testdriver()->compiler->modules) {
        if (i == count) {
            get_front_testdriver()->current_module = *mod;
            return;
        }
    }
    ck_abort_msg("Attempted to set non-existant module with index %u as current.", i);
}

struct analyzer *front_testdriver_analyzer()
{
    return front_testdriver_module()->analyzer;
}

struct module *front_testdriver_module()
{
    struct module *m;
    if ((m = get_front_testdriver()->current_module)) {
        return m;
    }
    if (darray_size(get_front_testdriver()->compiler->modules) == 0) {
        int a =5;
        (void)a;
    }
    ck_assert_msg(darray_size(get_front_testdriver()->compiler->modules) >= 1,
                  "Attempted to request a module from a front wih empty modules array");
    
    return get_front_testdriver()->current_module = darray_item(get_front_testdriver()->compiler->modules, 0);
}

struct parser *front_testdriver_parser()
{
    return get_front_testdriver()->current_front->parser;
}

struct lexer *front_testdriver_lexer()
{
    return get_front_testdriver()->current_front->lexer;
}

struct ast_node *front_testdriver_root()
{
    return get_front_testdriver()->current_front->root;
}

struct inpfile *front_testdriver_file()
{
    return get_front_testdriver()->current_front->file;
}

struct RFstringx *front_testdriver_geterrors(struct front_testdriver *d)
{
    return compiler_get_errors(d->compiler);
}

bool front_testdriver_init(struct front_testdriver *d, bool with_stdlib, int rf_logtype)
{
    RF_STRUCT_ZERO(d);
    darray_init(d->nodes);
    if (!(d->compiler = compiler_create(rf_logtype, with_stdlib))) {
        return false;
    }

    struct front_ctx *stdlib_front;
    if (with_stdlib) {
        const struct RFstring stdlib = RF_STRING_STATIC_INIT(RF_LANG_CORE_ROOT"/stdlib/io.rf");
        if (!(stdlib_front = compiler_new_front(d->compiler, &stdlib, false))) {
            RF_ERROR("Failed to add standard library to the front_ctxs");
            return false;
        }
    }
    // set driver's current front
    d->current_front = stdlib_front;

    return true;
}
void front_testdriver_deinit(struct front_testdriver *d)
{
    if (d->compiler) {
        compiler_destroy(d->compiler);
    }

    struct ast_node **n;
    darray_foreach(n, d->nodes) {
        ast_node_destroy(*n);
    }
    darray_free(d->nodes);
}

struct front_ctx *front_testdriver_new_source(const struct RFstring *s)
{
    struct front_testdriver *d = get_front_testdriver();
    const struct RFstring name = RF_STRING_STATIC_INIT("test_filename");
    struct front_ctx *front = compiler_new_front_from_source(d->compiler, &name, s, true);
    ck_assert_msg(front, "Could not add a new file to the driver");
    // set new front as current
    d->current_front = front;
    return front;
}


static inline struct ast_node *front_testdriver_node_from_loc(
    enum ast_type type, unsigned int sl, unsigned int sc, unsigned int el, unsigned int ec)
{
    struct inplocation temp_loc = LOC_INIT(get_front_testdriver()->current_front->file, sl, sc, el, ec);
    struct ast_node *ret = ast_node_create_loc(type, &temp_loc);
    // since this is testing code change owner so that it gets properly freed
    ret->state = AST_NODE_STATE_AFTER_PARSING;
    return ret;
}

struct ast_node *front_testdriver_generate_identifier(
    unsigned int sl, unsigned int sc, unsigned int el, unsigned int ec,
    const char *s)
{
    struct ast_node *ret;
    ret = front_testdriver_node_from_loc(AST_IDENTIFIER, sl, sc, el, ec);
    if (!ret) {
        return NULL;
    }
    ret->state = AST_NODE_STATE_AFTER_PARSING;
    RF_STRING_SHALLOW_INIT(&ret->identifier.string, (char*)s, strlen(s));
    darray_append(get_front_testdriver()->nodes, ret);
    return ret;
}

struct ast_node *front_testdriver_generate_string_literal(
    unsigned int sl, unsigned int sc, unsigned int el, unsigned int ec,
    unsigned int sl_byte_off, unsigned int el_byte_off,
    const char *s)
{
    struct ast_node *ret;
    struct front_testdriver *d = get_front_testdriver();
    struct inplocation temp_loc = LOC_INIT_FULL(
        sl, sc, el, ec,
        inpfile_line_p(d->current_front->file, sl) + sl_byte_off,
        inpfile_line_p(d->current_front->file, el) + el_byte_off);
    ret = ast_string_literal_create(&temp_loc);
    if (!ret) {
        return NULL;
    }
    ret->state = AST_NODE_STATE_AFTER_PARSING;
    darray_append(d->nodes, ret);
    return ret;
}

struct ast_node *front_testdriver_generate_constant_float(
    unsigned int sl, unsigned int sc, unsigned int el, unsigned int ec,
    double val)
{
    struct ast_node *ret;
    struct front_testdriver *d = get_front_testdriver();
    struct inplocation temp_loc = LOC_INIT(d->current_front->file, sl, sc, el, ec);
    ret = ast_constant_create_float(&temp_loc, val);
    if (!ret) {
        return NULL;
    }
    ret->state = AST_NODE_STATE_AFTER_PARSING;
    darray_append(d->nodes, ret);
    return ret;
}

struct ast_node *front_testdriver_generate_constant_integer(
    unsigned int sl, unsigned int sc, unsigned int el, unsigned int ec,
    uint64_t val)
{
    struct ast_node *ret;
    struct front_testdriver *d = get_front_testdriver();
    struct inplocation temp_loc = LOC_INIT(d->current_front->file, sl, sc, el, ec);
    ret = ast_constant_create_integer(&temp_loc, val);
    if (!ret) {
        return NULL;
    }
    ret->state = AST_NODE_STATE_AFTER_PARSING;
    darray_append(d->nodes, ret);
    return ret;
}

void front_testdriver_node_remove_children_from_array(struct front_testdriver *d,
                                                      struct ast_node *n)
{
    // extremey inefficient but this is just testing code
    struct ast_node *child;
    struct ast_node **arr_n;
    unsigned int i;
    rf_ilist_for_each(&n->children, child, lh) {

        front_testdriver_node_remove_children_from_array(d, child);

        i = 0;
        darray_foreach(arr_n, d->nodes) {
            if (*arr_n == child) {
                darray_remove(d->nodes, i);
                break;
            }
            ++i;
        }
    }
}

struct ast_node *do_front_testdriver_generate_node(
    unsigned int sl, unsigned int sc, unsigned int el, unsigned int ec,
    enum ast_type type, unsigned int args_num, ...)
{
    va_list args;
    struct ast_node *ret;
    struct ast_node *n1 = NULL;
    struct ast_node *n2 = NULL;
    struct front_testdriver *d = get_front_testdriver();
    struct inplocation temp_loc = LOC_INIT(d->current_front->file, sl, sc, el, ec);
    struct inplocation_mark *smark = &temp_loc.start;
    struct inplocation_mark *emark = &temp_loc.end;
    bool is_constant = false;

    va_start(args, args_num);

    switch(type) {
    case AST_XIDENTIFIER:
        ck_assert_uint_gt(args_num, 0);
        n1 = front_testdriver_generate_identifier(sl, sc, el, ec,
                                                  va_arg(args, const char *));
        if (args_num > 1) {
            is_constant = va_arg(args, int);
        }

        if (args_num > 2) {
            n2 = va_arg(args, struct ast_node *);
        }
        ret = ast_xidentifier_create(smark, emark, n1, is_constant, n2);
        break;
    case AST_TYPE_DESCRIPTION:
        ck_assert_uint_gt(args_num, 0);
        n1 = va_arg(args, struct ast_node *);
        ret = ast_typedesc_create(n1);
        break;
    case AST_TYPE_LEAF:
        ck_assert_uint_gt(args_num, 1);
        n1 = va_arg(args, struct ast_node *);
        n2 = va_arg(args, struct ast_node *);
        ret = ast_typeleaf_create(smark, emark, n1, n2);
        break;
    case AST_VARIABLE_DECLARATION:
        ck_assert_uint_gt(args_num, 0);
        n1 = va_arg(args, struct ast_node *);
        ret = ast_vardecl_create(smark, emark, n1);
        break;
    default:
        ck_assert_msg("invalid type provided to front test driver node generation");
        return NULL;
    }
    va_end(args);

    // extremey inefficient but this is just testing code
    front_testdriver_node_remove_children_from_array(d, ret);

    darray_append(d->nodes, ret);
    return ret;
}


void setup_front_tests()
{
    ck_assert_msg(front_testdriver_init(&__front_testdriver, true, LOG_TARGET_STDOUT),
                  "Failed to initialize front end test driver");
}

void setup_front_tests_no_stdlib()
{
    ck_assert_msg(front_testdriver_init(&__front_testdriver, false, LOG_TARGET_STDOUT),
                  "Failed to initialize front end test driver");
}

void setup_front_tests_with_file_log()
{
    ck_assert_msg(front_testdriver_init(&__front_testdriver, true, LOG_TARGET_FILE),
                  "Failed to initialize front end test driver");
}

void teardown_front_tests()
{
    front_testdriver_deinit(&__front_testdriver);
}


#define ck_astcheck_abort(...)                                      \
    RP_SELECT_FUNC_IF_NARGIS(i_ck_astcheck_abort, 3, __VA_ARGS__)
#define i_ck_astcheck_abort1(file_, line_, msg_)                        \
    ck_abort_msg("Checking ast trees from: %s:%u\n\t"msg_, file_, line_)
#define i_ck_astcheck_abort0(file_, line_, msg_, ...)       \
    ck_abort_msg("Checking ast trees from: %s:%u\n\t"msg_,  \
                 file_, line_, __VA_ARGS__)


static bool check_nodes(struct ast_node *got, struct ast_node *expect,
                        struct inpfile *ifile,
                        const char* filename,
                        unsigned int line)
{
    enum constant_type ctype;
    struct ast_node *child;
    int got_children = 0;
    int expect_children = 0;
    if (got->type != expect->type) {
        ck_astcheck_abort(
            filename, line,
            "2 ast nodes have different type: Got \""RF_STR_PF_FMT"\" "
            " at location "INPLOCATION_FMT2" != expected \""RF_STR_PF_FMT"\" at "
            "location "INPLOCATION_FMT2,
            RF_STR_PF_ARG(ast_node_str(got)),
            INPLOCATION_ARG2(ifile, &got->location),
            RF_STR_PF_ARG(ast_node_str(expect)),
            INPLOCATION_ARG2(ifile, &expect->location)
        );
        return false;
    }

    if (got->type == AST_BINARY_OPERATOR &&
        ast_binaryop_op(got) != ast_binaryop_op(expect)) {
        ck_astcheck_abort(
            filename, line,
            "2 ast binary operator nodes have different type: Got \""RF_STR_PF_FMT"\" "
            " at location "INPLOCATION_FMT2" != expected \""RF_STR_PF_FMT"\" at "
            "location "INPLOCATION_FMT2,
            RF_STR_PF_ARG(ast_binaryop_opstr(got)),
            INPLOCATION_ARG2(ifile, &got->location),
            RF_STR_PF_ARG(ast_binaryop_opstr(expect)),
            INPLOCATION_ARG2(ifile, &expect->location)
        );
        return false;
    }

    rf_ilist_for_each(&got->children, child, lh) {
        got_children ++;
    }
    rf_ilist_for_each(&expect->children, child, lh) {
        expect_children ++;
    }

    if (got_children != expect_children) {
        ck_astcheck_abort(filename, line,
                          "2 \""RF_STR_PF_FMT"\" ast nodes have different "
                          "number of children.\n Got %d != Expected %d",
                          RF_STR_PF_ARG(ast_node_str(got)),
                          got_children, expect_children);
        return false;
    }

    if(!inplocation_equal(&got->location, &expect->location)) {
        ck_astcheck_abort(
            filename, line,
            "2 \""RF_STR_PF_FMT"\" ast nodes have different location: "
            "Got "INPLOCATION_FMT2 " but"
            " expected "INPLOCATION_FMT2, RF_STR_PF_ARG(ast_node_str(got)),
            INPLOCATION_ARG2(ifile, &got->location),
            INPLOCATION_ARG2(ifile, &expect->location)
        );
        return false;
    }

    switch(got->type) {
    case AST_FUNCTION_DECLARATION:
        if (ast_fndecl_position_get(got) != ast_fndecl_position_get(expect)) {
            ck_astcheck_abort(
                filename, line, "Function declaration expected code position mismatch."
            );
            return false;
        }
        break;
    case AST_IDENTIFIER:
        if (!rf_string_equal(ast_identifier_str(got),
                             ast_identifier_str(expect))) {
            ck_astcheck_abort(
                filename, line,
                "identifiers mismatch: Got \""RF_STR_PF_FMT"\" != expected "
                "\""RF_STR_PF_FMT"\"",
                RF_STR_PF_ARG(ast_identifier_str(got)),
                RF_STR_PF_ARG(ast_identifier_str(expect))
            );
            return false;
        }
        break;
    case AST_IMPORT:
        if (ast_import_is_foreign(got) != ast_import_is_foreign(expect)) {

            ck_astcheck_abort(
                filename, line,
                "import statement type mismatch: Got %s but expected %s",
                ast_import_is_foreign(got) ? "foreign" : "normal",
                ast_import_is_foreign(expect) ? "foreign" : "normal"
            );
                
        }
        break;
    case AST_TYPE_OPERATOR:
        if (ast_typeop_op(got) != ast_typeop_op(expect)) {

            ck_astcheck_abort(
                filename, line,
                "type operator mismatch: Got \""RF_STR_PF_FMT"\" != expected "
                "\""RF_STR_PF_FMT"\"",
                RF_STR_PF_ARG(ast_typeop_opstr(got)),
                RF_STR_PF_ARG(ast_typeop_opstr(expect)));
        }
        break;
    case AST_BINARY_OPERATOR:
        if (ast_binaryop_op(got) != ast_binaryop_op(expect)) {

            ck_astcheck_abort(
                filename, line,
                "binary operator mismatch: Got \""RF_STR_PF_FMT"\" != expected "
                "\""RF_STR_PF_FMT"\"",
                RF_STR_PF_ARG(ast_binaryop_opstr(got)),
                RF_STR_PF_ARG(ast_binaryop_opstr(expect)));
        }
        break;
    case AST_STRING_LITERAL:
        if (!rf_string_equal(ast_string_literal_get_str(expect),
                             ast_string_literal_get_str(got))) {

            ck_astcheck_abort(
                filename, line,
                "string literal mismatch: Got \""RF_STR_PF_FMT"\" != expected "
                "\""RF_STR_PF_FMT"\"",
                RF_STR_PF_ARG(ast_string_literal_get_str(got)),
                RF_STR_PF_ARG(ast_string_literal_get_str(expect)));
        }
        break;
    case AST_CONSTANT:
        ctype = ast_constant_get_type(got);
        if (ctype != ast_constant_get_type(expect)) {
            ck_astcheck_abort(filename, line,
                              "constant number type mismatch for token at "
                              INPLOCATION_FMT2,
                              INPLOCATION_ARG2(ifile, &got->location));
        }

        switch (ctype) {
        case CONSTANT_NUMBER_INTEGER:
        {
            int64_t expect_v;
            int64_t got_v;
            ck_assert(ast_constant_get_integer(expect, &expect_v));
            ck_assert(ast_constant_get_integer(got, &got_v));
            if (expect_v != got_v) {
                ck_astcheck_abort(
                    filename, line,
                    "constant integer mismatch: Got \"%"PRIu64"\" != expected \""
                    "%"PRIu64"\"",
                    got_v, expect_v);
            }
        }
        break;
        case CONSTANT_NUMBER_FLOAT:
        {
            double expect_v;
            double got_v;
            ck_assert(ast_constant_get_float(expect, &expect_v));
            ck_assert(ast_constant_get_float(got, &got_v));
            if (!DBLCMP_EQ(expect_v, got_v)) {
                ck_astcheck_abort(
                    filename, line,
                    "constant float mismatch: Got \"%f\" != expected \"%f\"",
                    got_v, expect_v);
            }
        }
        break;
        case CONSTANT_BOOLEAN:
        {
            bool expect_v = ast_constant_get_bool(expect);
            bool got_v = ast_constant_get_bool(got);
            if (expect_v != got_v) {
                ck_astcheck_abort(
                    filename, line,
                    "constant boolean mismatch: Got \"%s\" but expected \"%s\"",
                    got_v ? "true" : "false",
                    expect_v ? "true" : "false");
            }
        }
        break;
        default:
            ck_astcheck_abort(filename, line, "unexpected constant number "
                              "type for token at "INPLOCATION_FMT2,
                              INPLOCATION_ARG2(ifile, &got->location));
        }
        break;
    default:
        break;
    }

    return true;
}

bool check_ast_match_impl(struct ast_node *got,
                          struct ast_node *expect,
                          struct inpfile *ifile,
                          const char* file,
                          unsigned int line)
{
    struct ast_node *got_child;
    struct ast_node *expect_child;
    int i = 0;
    int j = 0;

    if (!check_nodes(got, expect, ifile, file, line)) {
        return false;
    }

    rf_ilist_for_each(&got->children, got_child, lh) {

        j = 0;
        rf_ilist_for_each(&expect->children, expect_child, lh) {
            if (i == j &&
                !check_ast_match_impl(got_child, expect_child, ifile, file, line)) {
                return false;
            }
            ++j;
        }
        ++i;
    }
    return true;
}
