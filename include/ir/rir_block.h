#ifndef LFR_IR_RIR_BLOCK_H
#define LFR_IR_RIR_BLOCK_H

#include <rfbase/datastructs/intrusive_list.h>

#include <ir/rir_branch.h>
#include <ir/rir_strmap.h>
#include <ir/rir_expression.h>


struct ast_node;
struct rir_ctx;
struct rir;

enum rir_block_exit_type {
    RIR_BLOCK_EXIT_INVALID = 0,
    RIR_BLOCK_EXIT_BRANCH,
    RIR_BLOCK_EXIT_CONDBRANCH,
    RIR_BLOCK_EXIT_RETURN,
};

struct rir_block_exit {
    enum rir_block_exit_type type;
    union {
        struct rir_return retstmt;
        struct rir_branch branch;
        struct rir_condbranch condbranch;
    };
};

const struct RFstring *rir_blockexit_type_str(enum rir_block_exit_type type);
i_INLINE_DECL const struct RFstring *rir_block_exit_type_str(const struct rir_block_exit *b)
{
    return rir_blockexit_type_str(b->type);
}

bool rir_block_exit_init_branch(
    struct rir_block_exit *exit,
    struct rir_value *branch_dst
);
bool rir_block_exit_init_condbranch(
    struct rir_block_exit *exit,
    const struct rir_value *cond,
    struct rir_value *taken,
    struct rir_value *fallthrough
);
void rir_block_exit_return_init(
    struct rir_block_exit *exit,
    const struct rir_value *val
);

struct rir_block {
    struct rir_block_exit exit;
    //! The block's label value
    struct rir_value label;
    //! List of rir expressions
    struct RFilist_head expressions;
    //! A pointer to the symbol table of the block
    struct symbol_table *st;
};

enum block_position {
    BLOCK_POSITION_NORMAL, // generic value, not used for anything yet
    BLOCK_POSITION_FUNCTIONSTART,
    BLOCK_POSITION_LOOP,
    BLOCK_POSITION_MATCH, // not really used for anything yet
    BLOCK_POSITION_IF // not really used for anything yet
};

/**
 * Create a new rir basic block directly from AST
 *
 * @param n               If given the basic block will start being populated
 *                        with expressions taken out of this ast block. If not,
 *                        a new empty basic block will be created. Additionally
 *                        if this is given then the basic block will be added
 *                        to the function's basic blocks. If not it's left up
 *                        to the caller to add it himself.
 * @param position        Where the block is located. @see enum block_position
 *                        for possible values
 * @param ctx             The rir ctx for traversal
 */
struct rir_object *rir_block_create_obj_from_ast(
    const struct ast_node *n,
    enum block_position pos,
    struct rir_ctx *ctx
);
struct rir_block *rir_block_create_from_ast(
    const struct ast_node *n,
    enum block_position pos,
    struct rir_ctx *ctx
);

/**
 * Create a new rir basic block
 */
struct rir_object *rir_block_create_obj(
    const struct RFstring *name,
    enum rir_pos pos,
    rir_data data
);
struct rir_block *rir_block_create(
    const struct RFstring *name,
    enum rir_pos pos,
    rir_data data
);

struct rir_block *rir_block_functionend_create(bool has_return, struct rir_ctx *ctx);
struct rir_block *rir_block_matchcase_create(const struct ast_node *mcase,
                                             struct rir_object *matched_rir_obj,
                                             struct rir_ctx *ctx);

void rir_block_destroy(struct rir_block* b);
void rir_block_deinit(struct rir_block* b);

bool rir_process_ast_node(const struct ast_node *n, struct rir_ctx *ctx);
bool rir_process_identifier(const struct ast_node *n, struct rir_ctx *ctx);

bool rir_block_tostring(struct rirtostr_ctx *ctx, const struct rir_block *b);

i_INLINE_DECL bool rir_block_exit_initialized(const struct rir_block *b)
{
    return b->exit.type != RIR_BLOCK_EXIT_INVALID;
}

/**
 * @note: Should be enclosed in RFS_PUSH() and RFS_POP()
 * @return the string for the rir block label or the empty string if there is none
 */
const struct RFstring *rir_block_label_str(const struct rir_block *b);

/**
 * @return True if the block is the first block of a function
 */
bool rir_block_is_first(const struct rir_block *b);

i_INLINE_DECL void rir_block_add_expr(struct rir_block *b, struct rir_expression *e)
{
    rf_ilist_add_tail(&b->expressions, &e->ln);
}
#endif
