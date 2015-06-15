#include <unordered_map>
#include <vector>
#include "TcgGenerator.h"
#include "CompilerState.h"
#include "IntrinsicRepository.h"
#include "Output.h"
#include "cpu.h"

namespace jit {

const static size_t allocate_unit = 4096 * 16;
uint8_t* g_currentBufferPointer;
uint8_t* g_currentBufferEnd;

static LBasicBlock g_currentBB;
static CompilerState* g_state;
static Output* g_output;

static PlatformDesc g_desc = {
    sizeof(CPUARMState),
    static_cast<size_t>(offsetof(CPUARMState, regs[15])), /* offset of pc */
    11, /* prologue size */
    17, /* direct size */
    17, /* indirect size */
    17, /* assist size */
};

static LType argType()
{
    LType globalInt8Type = llvmAPI->Int8Type();
    static LType myargType = pointerType(arrayType(globalInt8Type, g_desc.m_contextSize));
    return myargType;
}

static inline LBasicBlock appendBasicBlock(const char* name)
{
    return jit::appendBasicBlock(g_state->m_context, g_state->m_function, name);
}

void llvm_tcg_init(void)
{
    g_state = new CompilerState("qemu", g_desc);
    g_output = new Output(*g_state);
}

void llvm_tcg_deinit(void)
{
    llvmAPI->DeleteFunction(g_state->m_function);
    delete g_output;
    g_output = nullptr;
    delete g_state;
    g_state = nullptr;
}
}

using namespace jit;

template <typename TCGType>
TCGType wrapPointer(LValue v)
{
    TCGType ret = reinterpret_cast<TCGType>(v);
    return ret;
}

template <typename TCGType>
TCGType wrapValue(LValue v)
{
    LValue alloca = g_output->buildAlloca(jit::typeOf(v));
    g_output->buildStore(v, alloca);
    TCGType ret = reinterpret_cast<TCGType>(alloca);
    return ret;
}

template <typename TCGType>
LValue unwrapPointer(TCGType v)
{
    return reinterpret_cast<LValue>(v);
}

template <typename TCGType>
LValue unwrapValue(TCGType v)
{
    return g_output->buildLoad(reinterpret_cast<LValue>(v));
}

void extract_64_32(LValue my64, LValue rl, LValue rh)
{
    LValue thirtytwo = g_output->repo().int32ThirtyTwo;
    LValue negativeOne = g_output->repo().int32NegativeOne;
    LValue rhUnwrap = g_output->buildCast(LLVMTrunc, g_output->buildLShr(my64, thirtytwo), g_output->repo().int32);
    LValue rlUnwrap = g_output->buildCast(LLVMTrunc, my64, g_output->repo().int32);
    g_output->buildStore(rhUnwrap, rh);
    g_output->buildStore(rlUnwrap, rl);
}

TCGv_i64 tcg_global_mem_new_i64(int reg, intptr_t offset, const char* name)
{
    LValue v = g_output->buildAdd(g_output->arg(), g_output->constInt32(offset));
    LValue v2 = g_output->buildPointerCast(v, g_output->repo().ref64);

    return wrapPointer<TCGv_i64>(v2);
}

TCGv_i32 tcg_const_i32(int32_t val)
{
    LValue v = g_output->constInt32(val);
    return wrapValue<TCGv_i32>(v);
}

TCGv_i64 tcg_const_i64(int64_t val)
{
    return wrapValue<TCGv_i64>(g_output->constInt64(val));
}

void tcg_gen_add2_i32(TCGv_i32 rl, TCGv_i32 rh, TCGv_i32 al,
    TCGv_i32 ah, TCGv_i32 bl, TCGv_i32 bh)
{
    LValue t0 = g_output->buildCast(LLVMZExt, unwrapValue(al), g_output->repo().int64);
    LValue t1 = g_output->buildCast(LLVMZExt, unwrapValue(ah), g_output->repo().int64);
    LValue t2 = g_output->buildCast(LLVMZExt, unwrapValue(bl), g_output->repo().int64);
    LValue t3 = g_output->buildCast(LLVMZExt, unwrapValue(bh), g_output->repo().int64);
    LValue thirtytwo = g_output->repo().int32ThirtyTwo;

    LValue t01 = g_output->buildShl(t1, thirtytwo);
    t01 = g_output->buildOr(t01, t0);
    LValue t23 = g_output->buildShl(t3, thirtytwo);
    t23 = g_output->buildOr(t23, t2);
    LValue t0123 = g_output->buildAdd(t01, t23);

    extract_64_32(t0123, unwrapPointer(rl), unwrapPointer(rh));
}

void tcg_gen_add_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue v = g_output->buildAdd(unwrapValue(arg1), unwrapValue(arg2));
    g_output->buildStore(v, unwrapPointer(ret));
}

void tcg_gen_add_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    LValue v = g_output->buildAdd(unwrapValue(arg1), unwrapValue(arg2));
    g_output->buildStore(v, unwrapPointer(ret));
}

void tcg_gen_addi_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue v;
    if (arg2 != 0) {
        v = g_output->buildAdd(unwrapValue(arg1), g_output->constInt32(arg2));
    } else {
        v = unwrapValue(arg1);
    }
    g_output->buildStore(v, unwrapPointer(ret));
}

void tcg_gen_addi_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    LValue v;
    if (arg2 != 0) {
        v = g_output->buildAdd(unwrapValue(arg1), g_output->constInt64(arg2));
    } else {
        v = unwrapValue(arg1);
    }
    g_output->buildStore(v, unwrapPointer(ret));
}

void tcg_gen_andc_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue t0 = g_output->buildNot(unwrapValue(arg2));
    LValue v = g_output->buildAnd(unwrapValue(arg1), t0);
    g_output->buildStore(v, unwrapPointer(ret));
}

void tcg_gen_and_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue v = g_output->buildAnd(unwrapValue(arg1), unwrapValue(arg2));
    g_output->buildStore(v, unwrapPointer(ret));
}

void tcg_gen_and_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    LValue v = g_output->buildAnd(unwrapValue(arg1), unwrapValue(arg2));
    g_output->buildStore(v, unwrapPointer(ret));
}

void tcg_gen_andi_i32(TCGv_i32 ret, TCGv_i32 arg1, uint32_t arg2)
{
    LValue v = g_output->buildAnd(unwrapValue(arg1), g_output->constInt32(arg2));
    g_output->buildStore(v, unwrapPointer(ret));
}

void tcg_gen_andi_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    LValue v = g_output->buildAnd(unwrapValue(arg1), g_output->constInt64(arg2));
    g_output->buildStore(v, unwrapPointer(ret));
}
