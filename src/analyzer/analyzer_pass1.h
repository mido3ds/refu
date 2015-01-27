#ifndef LFR_ANALYZER_PASS1_H
#define LFR_ANALYZER_PASS1_H

#include <stdbool.h>

struct analyzer;
struct ast_node;
struct analyzer_traversal_ctx;

/**
 * This is the first pass of the analyzer phase.
 *
 * Initializes the symbol tables for the nodes where this is needed and also
 * populates them with values. Since this is the first pass of the analyzer
 * phase this function also changes the ownership of the ast nodes.
 *
 * Also creates a hash for all identifiers and string literals and adds
 * them to global string tables.
 * This way they can all be disassociated from the file.
 *
 * @param a     The analyzer handle
 * @return      True for success, false otherwise
 */
bool analyzer_first_pass(struct analyzer *a);

/**
 * A function to to be called for a node while traversing the AST
 * during analysis. Switches the traversal context's current symbol table
 * to the parent's node symbol table dependin on various condition.
 *
 * @param n           The node the callback is called for
 * @param ctx         The traversal context
 */
bool analyzer_handle_symbol_table_ascending(struct ast_node *n,
                                            struct analyzer_traversal_ctx *ctx);

/**
 * A function to to be called for a node while traversing the AST
 * during analysis. Switches the traversal context's current symbol table to the
 * new symbol table of child @c n if it has one and if all conditions are met
 *
 * @param n           The node the callback is called for
 * @param ctx         The traversal context
 */
bool analyzer_handle_symbol_table_descending(struct ast_node *n,
                                             struct analyzer_traversal_ctx *ctx);
#endif