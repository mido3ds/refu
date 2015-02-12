#ifndef LFR_IR_ELEMENTS_H
#define LFR_IR_ELEMENTS_H

#include <Data_Structures/darray.h>
#include <Utils/struct_utils.h>
#include <Data_Structures/intrusive_list.h>

struct RFstring;
struct ast_node;
struct symbol_table;

/**
 * Representation of a function for the Refu IR
 */
struct rir_function {
    //! Function symbols
    struct symbol_table *symbols;
    struct rir_type *arg_type;
    struct rir_type *ret_type;
    struct rir_basic_block *entry;
    struct RFstring name;

    /*-- control for module list --*/
    struct RFilist_node ln_for_module;
};
RF_STRUCT_COMMON_SIGS_NO_ALLOC(rir_function, struct ast_node *fn_impl);


struct rir_simple_branch {
    struct rir_basic_block *dst;
};

struct rir_cond_branch {
    struct rir_simple_branch true_br;
    struct rir_simple_branch false_br;
    struct ast_expression *cond;
};

/**
 * Represents a branching point in the Refu IR.
 *
 * A branching can either be conditional or unconditional in which
 * case only a single destination needs to be specified.
 */
struct rir_branch {
    bool is_conditional;
    union {
        struct rir_cond_branch cond_branch;
        struct rir_simple_branch simple_branch;
    };
};

/**
 * Represents a basic block in the Refu IR.
 *
 * A basic block can only have some uninterrupted expressions and statements
 * and has only one single exit.
 */
struct rir_basic_block {
    //! Block symbols
    struct symbol_table *symbols;
    //! List of expressions
    struct RFilist_head lh;
    //! exit branch
    struct rir_branch exit;
};
RF_STRUCT_COMMON_SIGS_NO_ALLOC(rir_basic_block);
struct rir_basic_block *rir_basic_blocks_create_from_ast_block(struct ast_node *n);

void rir_handle_block_expression(struct ast_node *n, struct rir_basic_block *b);

/**
 * Represents a module in the IR
 *
 * It's basically (for now) a set of functions and one main function
 */
struct rir_module {
    //! Global symbols of the module
    struct symbol_table *symbols;
    //! List of functions of the module
    struct RFilist_head functions;
    //! Name of a module
    struct RFstring name;
};
RF_STRUCT_COMMON_SIGS_NO_ALLOC(rir_module, struct ast_node *n, const struct RFstring *name);

#endif
