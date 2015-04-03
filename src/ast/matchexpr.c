#include <ast/matchexpr.h>

struct ast_node *ast_matchcase_create(const struct inplocation_mark *start,
                                      const struct inplocation_mark *end,
                                      struct ast_node *pattern,
                                      struct ast_node *expression)
{
    struct ast_node *ret;
    ret = ast_node_create_marks(AST_MATCH_CASE, start, end);
    if (!ret) {
        return NULL;
    }
   
    ast_node_register_child(ret, pattern, matchcase.pattern);
    ast_node_register_child(ret, expression, matchcase.expression);

    return ret;
}

i_INLINE_INS struct ast_node *ast_matchcase_pattern(const struct ast_node *n);
i_INLINE_INS struct ast_node *ast_matchcase_expression(const struct ast_node *n);

struct ast_node *ast_matchexpr_create(const struct inplocation_mark *start,
                                      const struct inplocation_mark *end,
                                      struct ast_node *id)
{
    struct ast_node *ret;
    ret = ast_node_create_marks(AST_MATCH_EXPRESSION, start, end);
    if (!ret) {
        return NULL;
    }
    ast_node_register_child(ret, id, matchexpr.identifier);
    return ret;
}

i_INLINE_INS bool ast_matchexpr_is_bodyless(const struct ast_node *n);
i_INLINE_INS struct ast_node *ast_matchexpr_identifier(const struct ast_node *n);

bool ast_matchexpr_foreach_case(const struct ast_node *n,
                                matchexpr_foreach_cb cb,
                                void *user_arg)
{
    AST_NODE_ASSERT_TYPE(n, AST_MATCH_EXPRESSION);
    unsigned int i = 0;
    struct ast_node *child;
    rf_ilist_for_each(&n->children, child, lh) {
        if (i >= 1) {
            AST_NODE_ASSERT_TYPE(n, AST_MATCH_CASE);
            if (!cb(child, user_arg)) {
                return false;
            }
        }
        ++i;
    }
    return true;
}