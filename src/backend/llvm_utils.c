#include "llvm_utils.h"

#include <Utils/sanity.h>

#include <stdio.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>

#include "llvm_ast.h"

void bllvm_val_debug(LLVMValueRef v, const char *val_name)
{
    char *str = LLVMPrintValueToString(v);
    printf("[DEBUG]: Value of \"%s\" is %s\n", val_name, str);
    fflush(stdout);
    LLVMDisposeMessage(str);
}

void bllvm_type_debug(LLVMTypeRef t, const char *type_name, struct llvm_traversal_ctx *ctx)
{
    char *str = LLVMPrintTypeToString(t);
    printf("[DEBUG]: Type \"%s\" is %s with store size %llu \n",
           type_name,
           str,
           LLVMStoreSizeOfType(ctx->target_data, t));
    fflush(stdout);
    LLVMDisposeMessage(str);
}

void bllvm_mod_debug(LLVMModuleRef m, const char *mod_name)
{
    char *str = LLVMPrintModuleToString(m);
    printf("[DEBUG]: Module \"%s\" is\n %s\n", mod_name, str);
    fflush(stdout);
    LLVMDisposeMessage(str);
}


void bllvm_assign_to_string(LLVMValueRef string_alloca,
                            LLVMValueRef length,
                            LLVMValueRef string_data,
                            struct llvm_traversal_ctx *ctx)
{
    // store string length
    LLVMValueRef gep_to_strlen = bllvm_gep_to_struct(string_alloca, 0, ctx);
    LLVMBuildStore(ctx->builder, length, gep_to_strlen);
    // store string data
    LLVMValueRef gep_to_strdata = bllvm_gep_to_struct(string_alloca, 1, ctx);
    LLVMBuildStore(ctx->builder, string_data, gep_to_strdata);
}

void bllvm_load_from_string(LLVMValueRef string_alloca,
                            LLVMValueRef *length,
                            LLVMValueRef *string_data,
                            struct llvm_traversal_ctx *ctx)
{
    // load strlen
    LLVMValueRef gep_to_strlen = bllvm_gep_to_struct(string_alloca, 0, ctx);
    *length = LLVMBuildLoad(ctx->builder, gep_to_strlen, "loaded_str_len");
    // load strdata pointer TODO:load string again?
    LLVMValueRef gep_to_strdata = bllvm_gep_to_struct(string_alloca, 1, ctx);
    *string_data = LLVMBuildLoad(ctx->builder, gep_to_strdata, "loaded_str_data");
}

void bllvm_copy_string(LLVMValueRef from,
                       LLVMValueRef to,
                       struct llvm_traversal_ctx *ctx)
{
    LLVMValueRef length;
    LLVMValueRef string_data;
    bllvm_load_from_string(from, &length, &string_data, ctx);
    bllvm_assign_to_string(to, length, string_data, ctx);
}

LLVMValueRef bllvm_cast_value_to_type_maybe(LLVMValueRef val,
                                            LLVMTypeRef type,
                                            struct llvm_traversal_ctx *ctx)
{
    LLVMTypeRef val_type = LLVMTypeOf(val);
    if (val_type != type) {
        // we have to do typecasts
        if (bllvm_type_is_floating(val_type)) {
            val = LLVMBuildFPCast(ctx->builder, val, type, "");
        } else if (bllvm_type_is_int(val_type)) {
            uint32_t val_size = LLVMStoreSizeOfType(ctx->target_data, val_type);
            uint32_t to_type_size =  LLVMStoreSizeOfType(ctx->target_data, type);
            if (val_size < to_type_size) {
                val = LLVMBuildZExt(ctx->builder, val, type, "");
            } else { // greater or equal size
                val = LLVMBuildTruncOrBitCast(ctx->builder, val, type, "");
            }
        } else if (LLVMPointerType(type, 0) == val_type &&
                   bllvm_type_is_elementary(LLVMGetElementType(val_type))) {
            // if we are trying to assign a pointer to an elementary value, just load it
            val = LLVMBuildLoad(ctx->builder, val, "");
        } else {
            // in this case if we got here it's probably an assignment to a sum type
            val = LLVMBuildBitCast(ctx->builder, val, type, "");
        }
    }
    return val;
}

void bllvm_store(LLVMValueRef val,
                 LLVMValueRef ptr,
                 struct llvm_traversal_ctx *ctx)
{
    LLVMTypeRef ptr_element_type = LLVMGetElementType(LLVMTypeOf(ptr));
    if (LLVMTypeOf(val) == LLVMTypeOf(ptr)) {
        // if we are storing a pointer to another
        if (ptr_element_type == LLVMGetTypeByName(ctx->mod, "string")) {
            bllvm_copy_string(val, ptr, ctx);
        } else {
            // just memcpy
            bllvm_memcpy(ptr, val, ctx);
        }
        return;
    }
    val = bllvm_cast_value_to_type_maybe(val, ptr_element_type, ctx);
    LLVMBuildStore(ctx->builder, val, ptr);
}

LLVMBasicBlockRef bllvm_add_block_before_funcend(struct llvm_traversal_ctx *ctx)
{
    return LLVMInsertBasicBlock(LLVMGetLastBasicBlock(ctx->current_function), "");
}

void bllvm_enter_block(struct llvm_traversal_ctx *ctx,
                       struct LLVMOpaqueBasicBlock *block)
{
    LLVMPositionBuilderAtEnd(ctx->builder, block);
    ctx->current_block = block;
}

struct LLVMOpaqueBasicBlock *bllvm_add_fatal_block_before(struct LLVMOpaqueBasicBlock *target,
                                                          int exit_code,
                                                          struct llvm_traversal_ctx *ctx)
{
    LLVMBasicBlockRef prev_block = ctx->current_block;
    LLVMBasicBlockRef ret = LLVMInsertBasicBlock(target, "");
    bllvm_enter_block(ctx, ret);
    LLVMValueRef exit_fn = LLVMGetNamedFunction(ctx->mod, "exit");
    LLVMValueRef call_args[] = { LLVMConstInt(LLVMInt32Type(), exit_code, 0) };
    LLVMBuildCall(ctx->builder, exit_fn, call_args, 1, "");
    LLVMBuildBr(ctx->builder, target);
    bllvm_enter_block(ctx, prev_block);
    return ret;
}

LLVMValueRef bllvm_add_br(struct LLVMOpaqueBasicBlock *target,
                          struct llvm_traversal_ctx *ctx)
{
    LLVMValueRef last_instruction = LLVMGetLastInstruction(ctx->current_block);
    return (!last_instruction || !LLVMIsATerminatorInst(last_instruction))
        ? LLVMBuildBr(ctx->builder, target) : NULL;
}
 
void bllvm_memcpy(struct LLVMOpaqueValue *from,
                  struct LLVMOpaqueValue *to,
                  struct llvm_traversal_ctx *ctx)
{
    LLVMTypeRef from_elem_type = LLVMGetElementType(LLVMTypeOf(from));
    RF_ASSERT(from_elem_type, "Non-pointer value provided for memcpy from");
    LLVMTypeRef to_elem_type = LLVMGetElementType(LLVMTypeOf(to));
    RF_ASSERT(to_elem_type, "Non-pointer value provided for memcpy to");
    uint32_t from_size = LLVMStoreSizeOfType(ctx->target_data, from_elem_type);
    uint32_t to_size =  LLVMStoreSizeOfType(ctx->target_data, to_elem_type);
    RF_ASSERT(to_size >= from_size, "Called memcpy with to_size < from_size");
    bllvm_memcpyn(from, to, from_size, ctx);
}

void bllvm_memcpyn(LLVMValueRef from,
                   LLVMValueRef to,
                   uint32_t bytes,
                   struct llvm_traversal_ctx *ctx)
{
    LLVMValueRef dst_cast = LLVMBuildBitCast(ctx->builder, to,
                                             LLVMPointerType(LLVMInt8Type(), 0), "");
    LLVMValueRef src_cast = LLVMBuildBitCast(ctx->builder, from,
                                             LLVMPointerType(LLVMInt8Type(), 0), "");
    LLVMValueRef llvm_memcpy = LLVMGetNamedFunction(ctx->mod, "llvm.memcpy.p0i8.p0i8.i64");

    LLVMValueRef call_args[] = { dst_cast, src_cast,
                                 LLVMConstInt(LLVMInt64Type(), bytes, 0),
                                 LLVMConstInt(LLVMInt32Type(), 0, 0),
                                 LLVMConstInt(LLVMInt1Type(), 0, 0) };
    LLVMBuildCall(ctx->builder, llvm_memcpy, call_args, 5, "");
}

void bllvm_nop(struct llvm_traversal_ctx *ctx)
{
    LLVMBuildCall(ctx->builder,
                  LLVMGetNamedFunction(ctx->mod, "llvm.donothing"),
                  NULL,
                  0,
                  ""
    );
}

struct LLVMOpaqueValue *bllvm_gep_to_struct(struct LLVMOpaqueValue *ptr,
                                            unsigned int member_num,
                                            struct llvm_traversal_ctx *ctx)
{
    LLVMValueRef indices[] = { LLVMConstInt(LLVMInt32Type(), 0, 0), LLVMConstInt(LLVMInt32Type(), member_num, 0) };
    return LLVMBuildGEP(ctx->builder, ptr, indices, 2, "");    
}
