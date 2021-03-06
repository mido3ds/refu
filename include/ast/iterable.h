#ifndef LFR_AST_ITERABLE_H
#define LFR_AST_ITERABLE_H

#include <rfbase/defs/inline.h>
#include <rfbase/utils/sanity.h>
#include <ast/ast.h>
#include <stdbool.h>

struct ast_node;
struct inplocation_mark;

struct ast_node *ast_iterable_create_identifier(struct ast_node *identifier);

struct ast_node *ast_iterable_create_range(
    struct ast_node *start_node,
    struct ast_node *step_node,
    struct ast_node *end_node
);

/**
 * Return (if possible) the number of loops the range will create
 * @param n The iterable ast node
 * @return The number of loops the range will create or -1 if it's not possible
 *         to determine in compile time.
 */
int64_t ast_iterable_range_number_of_loops(const struct ast_node *n);

i_INLINE_DECL struct ast_node* ast_iterable_identifier_get(const struct ast_node *n)
{
    RF_ASSERT(
        n->type == AST_ITERABLE && n->iterable.type == ITERABLE_COLLECTION,
        "Illegal ast node type. Expected a collection iterable"
    );
    return n->iterable.identifier;
}

i_INLINE_DECL enum iterable_type ast_iterable_type_get(const struct ast_node *n)
{
    AST_NODE_ASSERT_TYPE(n, AST_ITERABLE);
    return n->iterable.type;
}

bool ast_iterable_range_start_get(const struct ast_node *n, int64_t *ret);
bool ast_iterable_range_step_get(const struct ast_node *n, int64_t *ret);
bool ast_iterable_range_end_get(const struct ast_node *n, int64_t *ret);

bool ast_iterable_range_compiletime_computable(
    const struct ast_node *n,
    int64_t *start,
    int64_t *step,
    int64_t* end
);


#endif
