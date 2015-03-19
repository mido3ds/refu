#ifndef LFR_BACKEND_LLVM_GLOBALS_H
#define LFR_BACKEND_LLVM_GLOBALS_H

#include <stdbool.h>
#include <stdint.h>

struct llvm_traversal_ctx;
struct RFstring;
struct LLVMOpaqueValue;

bool backend_llvm_create_globals(struct llvm_traversal_ctx *ctx);
struct LLVMOpaqueValue *backend_llvm_create_global_const_string(const struct RFstring *string,
                                                                struct llvm_traversal_ctx *ctx);
struct LLVMOpaqueValue *backend_llvm_create_global_const_string_with_hash(
    const struct RFstring *string,
    uint32_t hash,
    struct llvm_traversal_ctx *ctx);

/**
 * @returns the global string for "true" or "false"
 */
struct LLVMOpaqueValue *backend_llvm_get_boolean_str(bool boolean,
                                                     struct llvm_traversal_ctx *ctx);
#endif
