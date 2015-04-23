#ifndef LFR_TYPECHECK_MATCHEXPR_H
#define LFR_TYPECHECK_MATCHEXPR_H

#include <analyzer/type_set.h>
#include <utils/traversal.h>

struct ast_node;
struct analyzer_traversal_ctx;

enum traversal_cb_res typecheck_matchcase(struct ast_node *n,
                                          struct analyzer_traversal_ctx* ctx);
enum traversal_cb_res typecheck_matchexpr(struct ast_node *n,
                                          struct analyzer_traversal_ctx *ctx);

struct pattern_matching_ctx {
    //! A set of types that constitute discovered parts of the type
    struct rf_objset_type parts;
    //! A set of type parts that have been matched
    struct rf_objset_type matched;
};
void pattern_matching_ctx_init(struct pattern_matching_ctx *ctx);
void pattern_matching_ctx_deinit(struct pattern_matching_ctx *ctx);

#endif