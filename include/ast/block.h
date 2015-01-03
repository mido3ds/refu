#ifndef LFR_AST_BLOCK_H
#define LFR_AST_BLOCK_H

#include <ast/ast.h>
struct ast_node *ast_block_create();

struct analyzer;

i_INLINE_DECL bool ast_block_symbol_table_init(struct ast_node *n,
                                               struct analyzer *a)
{
    AST_NODE_ASSERT_TYPE(n, AST_BLOCK);
    return symbol_table_init(&n->block.st, a);
}

i_INLINE_DECL struct symbol_table* ast_block_symbol_table_get(struct ast_node *n)
{
    AST_NODE_ASSERT_TYPE(n, AST_BLOCK);
    return &n->block.st;
}

i_INLINE_DECL struct ast_node* ast_block_valueexpr_get(struct ast_node *n)
{
    AST_NODE_ASSERT_TYPE(n, AST_BLOCK);
    return n->block.value_expr;
}

i_INLINE_DECL void ast_block_valueexpr_set(struct ast_node *n, struct ast_node *expr)
{
    AST_NODE_ASSERT_TYPE(n, AST_BLOCK);
    n->block.value_expr = expr;
}


i_INLINE_DECL void ast_block_add_element(struct ast_node *n, struct ast_node *element)
{
    AST_NODE_ASSERT_TYPE(n, AST_BLOCK);
    if (ast_node_type(n) == AST_RETURN_STATEMENT) {
        ast_block_valueexpr_set(n, element);
    }
    ast_node_add_child(n, element);
}
#endif
