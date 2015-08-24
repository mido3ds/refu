#ifndef LFR_IR_RIR_BINARYOP_H
#define LFR_IR_RIR_BINARYOP_H

#include <ir/rir_expression.h>


struct ast_binaryop;

struct rir_expression *rir_binaryop_create(const struct ast_binaryop *op,
                                           const struct rir_value *a,
                                           const struct rir_value *b,
                                           struct rir_ctx *ctx);

bool rir_process_binaryop(const struct ast_binaryop *n,
                          struct rir_ctx *ctx);
#endif
