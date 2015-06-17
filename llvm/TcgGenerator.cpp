#include <unordered_map>
#include <vector>
#include "TcgGenerator.h"
#include "CompilerState.h"
#include "IntrinsicRepository.h"
#include "Output.h"
#include "cpu.h"
#include "log.h"

namespace jit {

const static size_t allocate_unit = 4096 * 16;
uint8_t* g_currentBufferPointer;
uint8_t* g_currentBufferEnd;

static LBasicBlock g_currentBB;
static CompilerState* g_state;
static Output* g_output;

typedef std::unordered_map<int, LBasicBlock> LabelMap;
LabelMap g_labelMap;

static PlatformDesc g_desc = {
    sizeof(CPUARMState),
    static_cast<size_t>(offsetof(CPUARMState, regs[15])), /* offset of pc */
    11, /* prologue size */
    17, /* direct size */
    17, /* indirect size */
    17, /* assist size */
    17, /* tcg size */
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
    g_labelMap.clear();
}
}

using namespace jit;

template <typename TCGType>
static TCGType wrapPointer(LValue v)
{
    TCGType ret = reinterpret_cast<TCGType>(v);
    return ret;
}

template <typename TCGType>
static TCGType wrapValue(LValue v)
{
    LValue alloca = g_output->buildAlloca(jit::typeOf(v));
    g_output->buildStore(v, alloca);
    TCGType ret = reinterpret_cast<TCGType>(alloca);
    return ret;
}

template <typename TCGType>
static LValue unwrapPointer(TCGType v)
{
    return reinterpret_cast<LValue>(v);
}

template <typename TCGType>
static LValue unwrapValue(TCGType v)
{
    return g_output->buildLoad(reinterpret_cast<LValue>(v));
}

template <typename TCGType>
void storeToTCG(LValue v, TCGType ret)
{
    g_output->buildStore(v, unwrapPointer(ret));
}

static void extract_64_32(LValue my64, LValue rl, LValue rh)
{
    LValue thirtytwo = g_output->repo().int32ThirtyTwo;
    LValue negativeOne = g_output->repo().int32NegativeOne;
    LValue rhUnwrap = g_output->buildCast(LLVMTrunc, g_output->buildLShr(my64, thirtytwo), g_output->repo().int32);
    LValue rlUnwrap = g_output->buildCast(LLVMTrunc, my64, g_output->repo().int32);
    g_output->buildStore(rhUnwrap, rh);
    g_output->buildStore(rlUnwrap, rl);
}

static LLVMIntPredicate tcgCondToLLVM(TCGCond cond)
{
    switch (cond) {
    case TCG_COND_EQ:
        return LLVMIntEQ;
    case TCG_COND_NE:
        return LLVMIntNE;
    /* signed */
    case TCG_COND_LT:
        return LLVMIntSLT;
    case TCG_COND_GE:
        return LLVMIntSGE;
    case TCG_COND_LE:
        return LLVMIntSLE;
    case TCG_COND_GT:
        return LLVMIntSGT;
    /* unsigned */
    case TCG_COND_LTU:
        return LLVMIntULT;
    case TCG_COND_GEU:
        return LLVMIntUGE;
    case TCG_COND_LEU:
        return LLVMIntULE;
    case TCG_COND_GTU:
        return LLVMIntUGT;
    default:
        EMASSERT("unsupported compare" && false);
    }
}

static LValue tcgPointerToLLVM(TCGMemOp op, TCGv pointer)
{
    int opInt = op;
    opInt &= ~MO_SIGN;
    LValue pointerBeforeCast = unwrapValue(pointer);
    switch (op) {
    case MO_8:
        return g_output->buildPointerCast(pointerBeforeCast, g_output->repo().ref8);
    case MO_16:
        return g_output->buildPointerCast(pointerBeforeCast, g_output->repo().ref16);
    case MO_32:
        return g_output->buildPointerCast(pointerBeforeCast, g_output->repo().ref32);
    case MO_64:
        return g_output->buildPointerCast(pointerBeforeCast, g_output->repo().ref64);
    default:
        EMASSERT("unknow pointer type." && false);
    }
}

static LValue tcgMemCastTo32(TCGMemOp op, LValue val)
{
    switch (op) {
    case MO_8:
        return g_output->buildCast(LLVMZExt, val, g_output->repo().int32);
    case MO_SB:
        return g_output->buildCast(LLVMSExt, val, g_output->repo().int32);
    case MO_16:
        return g_output->buildCast(LLVMZExt, val, g_output->repo().int32);
    case MO_SW:
        return g_output->buildCast(LLVMSExt, val, g_output->repo().int32);
    case MO_32:
        return val;
    case MO_64:
        return g_output->buildCast(LLVMTrunc, val, g_output->repo().int32);
    default:
        EMASSERT("unknow pointer type." && false);
    }
}

TCGv_i64 tcg_global_mem_new_i64(int reg, intptr_t offset, const char* name)
{
    LValue v = g_output->buildAdd(g_output->arg(), g_output->constInt32(offset / sizeof(target_ulong)));
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

static LBasicBlock labelToBB(int n)
{
    auto found = g_labelMap.find(n);
    EMASSERT(found != g_labelMap.end());
    LBasicBlock bb = found->second;
    return bb;
}

int gen_new_label(void)
{
    static int count = 0;
    LBasicBlock bb = g_output->appendBasicBlock("");
    g_labelMap.insert(std::make_pair(count, bb));
    return count++;
}

void gen_set_label(int n)
{
    LBasicBlock bb = labelToBB(n);
    g_output->buildBr(bb);
    g_output->positionToBBEnd(bb);
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
    storeToTCG(v, ret);
}

void tcg_gen_add_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    LValue v = g_output->buildAdd(unwrapValue(arg1), unwrapValue(arg2));
    storeToTCG(v, ret);
}

void tcg_gen_addi_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue v;
    if (arg2 != 0) {
        v = g_output->buildAdd(unwrapValue(arg1), g_output->constInt32(arg2));
    }
    else {
        v = unwrapValue(arg1);
    }
    storeToTCG(v, ret);
}

void tcg_gen_addi_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    LValue v;
    if (arg2 != 0) {
        v = g_output->buildAdd(unwrapValue(arg1), g_output->constInt64(arg2));
    }
    else {
        v = unwrapValue(arg1);
    }
    storeToTCG(v, ret);
}

void tcg_gen_andc_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue t0 = g_output->buildNot(unwrapValue(arg2));
    LValue v = g_output->buildAnd(unwrapValue(arg1), t0);
    storeToTCG(v, ret);
}

void tcg_gen_and_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue v = g_output->buildAnd(unwrapValue(arg1), unwrapValue(arg2));
    storeToTCG(v, ret);
}

void tcg_gen_and_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    LValue v = g_output->buildAnd(unwrapValue(arg1), unwrapValue(arg2));
    storeToTCG(v, ret);
}

void tcg_gen_andi_i32(TCGv_i32 ret, TCGv_i32 arg1, uint32_t arg2)
{
    LValue v = g_output->buildAnd(unwrapValue(arg1), g_output->constInt32(arg2));
    storeToTCG(v, ret);
}

void tcg_gen_andi_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    LValue v = g_output->buildAnd(unwrapValue(arg1), g_output->constInt64(arg2));
    storeToTCG(v, ret);
}

void tcg_gen_brcondi_i32(TCGCond cond, TCGv_i32 arg1,
    int32_t arg2, int label_index)
{
    LBasicBlock taken = labelToBB(label_index);
    LBasicBlock nottaken = g_output->appendBasicBlock("notTaken");
    LValue v1 = unwrapValue(arg1);
    LValue v2 = g_output->constInt32(arg2);
    LValue condVal = g_output->buildICmp(tcgCondToLLVM(cond), v1, v2);
    g_output->buildCondBr(condVal, taken, nottaken);
    g_output->positionToBBEnd(nottaken);
}

void tcg_gen_bswap16_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue v = unwrapValue(arg);
    LValue lower = g_output->buildAnd(v, g_output->repo().int32TwoFiveFive);
    LValue higher = g_output->buildShl(v, g_output->repo().int32Eight);
    LValue valret = g_output->buildOr(higher, lower);
    storeToTCG(valret, ret);
}

void tcg_gen_bswap32_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue v = unwrapValue(arg);
    LValue twentyFour = g_output->constInt32(24);
    LValue hi24 = g_output->buildShl(v, twentyFour);
    LValue hi16 = g_output->buildAnd(v, g_output->constInt32(0xff00));
    hi16 = g_output->buildShl(hi16, g_output->repo().int32Eight);
    LValue lo8 = g_output->buildAnd(v, g_output->constInt32(0xff0000));
    lo8 = g_output->buildLShr(lo8, g_output->repo().int32Eight);
    LValue lo0 = g_output->buildAnd(v, g_output->constInt32(0xff000000));
    lo0 = g_output->buildLShr(lo0, twentyFour);
    LValue ret1 = g_output->buildOr(hi24, hi16);
    LValue ret2 = g_output->buildOr(lo0, lo8);
    LValue retVal = g_output->buildOr(ret1, ret2);
    storeToTCG(retVal, ret);
}

void tcg_gen_concat_i32_i64(TCGv_i64 dest, TCGv_i32 low,
    TCGv_i32 high)
{
    LValue lo = unwrapValue(low);
    LValue hi = unwrapValue(high);
    LValue low64 = g_output->buildCast(LLVMZExt, lo, g_output->repo().int64);
    LValue hi64 = g_output->buildCast(LLVMZExt, hi64, g_output->repo().int64);
    hi64 = g_output->buildShl(hi64, g_output->repo().int32ThirtyTwo);
    LValue ret = g_output->buildOr(hi64, low64);
    storeToTCG(ret, dest);
}

void tcg_gen_deposit_i32(TCGv_i32 ret, TCGv_i32 arg1,
    TCGv_i32 arg2, unsigned int ofs,
    unsigned int len)
{
    if (ofs == 0 && len == 32) {
        tcg_gen_mov_i32(ret, arg2);
        return;
    }
    LValue v;
    unsigned mask = (1u << len) - 1;
    if (ofs + len < 32) {
        v = g_output->buildAnd(unwrapValue(arg2), g_output->constInt32(mask));
        v = g_output->buildShl(v, g_output->constInt32(ofs));
    }
    else {
        v = g_output->buildShl(unwrapValue(arg2), g_output->constInt32(ofs));
    }
    LValue retVal = g_output->buildAnd(unwrapValue(arg1), g_output->constInt32(~(mask << ofs)));
    retVal = g_output->buildOr(retVal, v);
    storeToTCG(retVal, ret);
}

void tcg_gen_mov_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    if (arg == ret)
        return;
    LValue v = unwrapValue(arg);
    storeToTCG(v, ret);
}

void tcg_gen_exit_tb(int direct)
{
    if (direct)
        g_output->buildTcgDirectPatch();
    else
        g_output->buildTcgIndirectPatch();
}

void tcg_gen_ext16s_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue retVal = g_output->buildShl(unwrapValue(arg), g_output->repo().int32Sixteen);
    retVal = g_output->buildAShr(retVal, g_output->repo().int32Sixteen);
    storeToTCG(retVal, ret);
}

void tcg_gen_ext16u_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue retVal = g_output->buildAnd(unwrapValue(arg), g_output->constInt32(0xffff));
    storeToTCG(retVal, ret);
}

void tcg_gen_ext32u_i64(TCGv_i64 ret, TCGv_i64 arg)
{
    LValue retVal = g_output->buildAnd(unwrapValue(arg), g_output->constInt64(0xffffffffu));
    storeToTCG(retVal, ret);
}

void tcg_gen_ext8s_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue constant = g_output->constInt32(24);
    LValue retVal = g_output->buildShl(unwrapValue(arg), constant);
    retVal = g_output->buildAShr(retVal, constant);
    storeToTCG(retVal, ret);
}

void tcg_gen_ext8u_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue retVal = g_output->buildAnd(unwrapValue(arg), g_output->constInt32(0xff));
    storeToTCG(retVal, ret);
}

void tcg_gen_ext_i32_i64(TCGv_i64 ret, TCGv_i32 arg)
{
    LValue retVal = g_output->buildCast(LLVMSExt, unwrapValue(arg), g_output->repo().int64);
    storeToTCG(retVal, ret);
}

void tcg_gen_extu_i32_i64(TCGv_i64 ret, TCGv_i32 arg)
{
    LValue retVal = g_output->buildCast(LLVMZExt, unwrapValue(arg), g_output->repo().int64);
    storeToTCG(retVal, ret);
}

void tcg_gen_ld_i32(TCGv_i32 ret, TCGv_ptr arg2, tcg_target_long offset)
{
    LValue pointer = unwrapValue(arg2);
    pointer = g_output->buildPointerCast(pointer, g_output->repo().ref8);
    pointer = g_output->buildGEP(pointer, offset);
    pointer = g_output->buildPointerCast(pointer, g_output->repo().ref32);
    LValue retVal = g_output->buildLoad(pointer);
    storeToTCG(retVal, ret);
}

void tcg_gen_ld_i64(TCGv_i64 ret, TCGv_ptr arg2,
    tcg_target_long offset)
{
    LValue pointer = unwrapValue(arg2);
    pointer = g_output->buildPointerCast(pointer, g_output->repo().ref8);
    pointer = g_output->buildGEP(pointer, offset);
    pointer = g_output->buildPointerCast(pointer, g_output->repo().ref64);
    LValue retVal = g_output->buildLoad(pointer);
    storeToTCG(retVal, ret);
}

void tcg_gen_movcond_i32(TCGCond cond, TCGv_i32 ret,
    TCGv_i32 c1, TCGv_i32 c2,
    TCGv_i32 v1, TCGv_i32 v2)
{
    LValue t0;
    switch (cond) {
    case TCG_COND_ALWAYS:
        t0 = g_output->repo().int32One;
        break;
    case TCG_COND_NEVER:
        t0 = g_output->repo().int32Zero;
        break;
    default:
        t0 = g_output->buildICmp(tcgCondToLLVM(cond), unwrapValue(c1), unwrapValue(c2));
    }

    LValue retVal = g_output->buildSelect(t0, unwrapValue(v1), unwrapValue(v2));
    storeToTCG(retVal, ret);
}

void tcg_gen_movcond_i64(TCGCond cond, TCGv_i64 ret,
    TCGv_i64 c1, TCGv_i64 c2,
    TCGv_i64 v1, TCGv_i64 v2)
{
    LValue t0;
    switch (cond) {
    case TCG_COND_ALWAYS:
        t0 = g_output->repo().int32One;
        break;
    case TCG_COND_NEVER:
        t0 = g_output->repo().int32Zero;
        break;
    default:
        t0 = g_output->buildICmp(tcgCondToLLVM(cond), unwrapValue(c1), unwrapValue(c2));
    }

    LValue retVal = g_output->buildSelect(t0, unwrapValue(v1), unwrapValue(v2));
    storeToTCG(retVal, ret);
}

void tcg_gen_mov_i64(TCGv_i64 ret, TCGv_i64 arg)
{
    LValue retVal = unwrapValue(arg);
    storeToTCG(retVal, ret);
}

void tcg_gen_movi_i32(TCGv_i32 ret, int32_t arg)
{
    LValue retVal = g_output->constInt32(arg);
    storeToTCG(retVal, ret);
}

void tcg_gen_movi_i64(TCGv_i64 ret, int64_t arg)
{
    LValue retVal = g_output->constInt64(arg);
    storeToTCG(retVal, ret);
}

void tcg_gen_mul_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue retVal = g_output->buildMul(unwrapValue(arg1), unwrapValue(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_muls2_i32(TCGv_i32 rl, TCGv_i32 rh,
    TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue t0 = g_output->buildCast(LLVMSExt, unwrapValue(arg1), g_output->repo().int64);
    LValue t1 = g_output->buildCast(LLVMSExt, unwrapValue(arg2), g_output->repo().int64);
    LValue t3 = g_output->buildMul(t0, t1);
    LValue low = g_output->buildCast(LLVMTrunc, t3, g_output->repo().int32);
    LValue high = g_output->buildAShr(t3, g_output->repo().int32ThirtyTwo);
    high = g_output->buildCast(LLVMTrunc, high, g_output->repo().int32);
    storeToTCG(low, rl);
    storeToTCG(high, rh);
}

void tcg_gen_mulu2_i32(TCGv_i32 rl, TCGv_i32 rh,
    TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue t0 = g_output->buildCast(LLVMZExt, unwrapValue(arg1), g_output->repo().int64);
    LValue t1 = g_output->buildCast(LLVMZExt, unwrapValue(arg2), g_output->repo().int64);
    LValue t3 = g_output->buildMul(t0, t1);
    LValue low = g_output->buildCast(LLVMTrunc, t3, g_output->repo().int32);
    LValue high = g_output->buildLShr(t3, g_output->repo().int32ThirtyTwo);
    high = g_output->buildCast(LLVMTrunc, high, g_output->repo().int32);
    storeToTCG(low, rl);
    storeToTCG(high, rh);
}

void tcg_gen_neg_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue retVal = g_output->buildNeg(unwrapValue(arg));
    storeToTCG(retVal, ret);
}

void tcg_gen_neg_i64(TCGv_i64 ret, TCGv_i64 arg)
{
    LValue retVal = g_output->buildNeg(unwrapValue(arg));
    storeToTCG(retVal, ret);
}

void tcg_gen_not_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue retVal = g_output->buildNot(unwrapValue(arg));
    storeToTCG(retVal, ret);
}

void tcg_gen_orc_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue t0 = g_output->buildNot(unwrapValue(arg2));
    LValue retVal = g_output->buildOr(unwrapValue(arg1), t0);
    storeToTCG(retVal, ret);
}

void tcg_gen_or_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue retVal = g_output->buildOr(unwrapValue(arg1), unwrapValue(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_or_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    LValue retVal = g_output->buildOr(unwrapValue(arg1), unwrapValue(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_ori_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue retVal = g_output->buildOr(unwrapValue(arg1), g_output->constInt32(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_qemu_ld_i32(TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
    EMASSERT(idx == 0);
    LValue pointer = tcgPointerToLLVM(memop, addr);
    LValue retVal = g_output->buildLoad(pointer);
    retVal = tcgMemCastTo32(memop, retVal);
    switch (memop) {
    case MO_UB:
        retVal = g_output->buildCast(LLVMZExt, retVal, g_output->repo().int32);
        break;
    case MO_SB:
        retVal = g_output->buildCast(LLVMSExt, retVal, g_output->repo().int32);
        break;
    case MO_UW:
        retVal = g_output->buildCast(LLVMZExt, retVal, g_output->repo().int32);
        break;
    case MO_SW:
        retVal = g_output->buildCast(LLVMSExt, retVal, g_output->repo().int32);
        break;
    case MO_SL:
    case MO_UL:
        break;
    case MO_Q:
    case (MO_Q | MO_SIGN):
        retVal = g_output->buildCast(LLVMTrunc, retVal, g_output->repo().int32);
    default:
        EMASSERT("unknow pointer type." && false);
    }
    storeToTCG(retVal, val);
}

void tcg_gen_qemu_ld_i64(TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
    EMASSERT(idx == 0);
    LValue pointer = tcgPointerToLLVM(memop, addr);
    LValue retVal = g_output->buildLoad(pointer);
    switch (memop) {
    case MO_UB:
        retVal = g_output->buildCast(LLVMZExt, retVal, g_output->repo().int64);
        break;
    case MO_SB:
        retVal = g_output->buildCast(LLVMSExt, retVal, g_output->repo().int64);
        break;
    case MO_UW:
        retVal = g_output->buildCast(LLVMZExt, retVal, g_output->repo().int64);
        break;
    case MO_SW:
        retVal = g_output->buildCast(LLVMSExt, retVal, g_output->repo().int64);
        break;
    case MO_SL:
        retVal = g_output->buildCast(LLVMSExt, retVal, g_output->repo().int64);
        break;
    case MO_UL:
        retVal = g_output->buildCast(LLVMZExt, retVal, g_output->repo().int64);
        break;
    case MO_Q:
    case (MO_Q | MO_SIGN):
        break;
    default:
        EMASSERT("unknow pointer type." && false);
    }
    storeToTCG(retVal, val);
}

void tcg_gen_qemu_st_i32(TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
    EMASSERT(idx == 0);
    LValue pointer = tcgPointerToLLVM(memop, addr);
    LValue valToStore = unwrapValue(val);
    switch (memop) {
    case MO_UB:
    case MO_SB:
        valToStore = g_output->buildCast(LLVMTrunc, valToStore, g_output->repo().int8);
        break;
    case MO_UW:
    case MO_SW:
        valToStore = g_output->buildCast(LLVMTrunc, valToStore, g_output->repo().int16);
        break;
    case MO_UL:
    case MO_SL:
        break;
    case MO_Q:
        valToStore = g_output->buildCast(LLVMZExt, valToStore, g_output->repo().int32);
        break;
    case (MO_Q | MO_SIGN):
        valToStore = g_output->buildCast(LLVMSExt, valToStore, g_output->repo().int32);
        break;
    default:
        EMASSERT("unknow memop" && false);
    }
    g_output->buildStore(valToStore, pointer);
}

void tcg_gen_qemu_st_i64(TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
    EMASSERT(idx == 0);
    LValue pointer = tcgPointerToLLVM(memop, addr);
    LValue valToStore = unwrapValue(val);
    switch (memop) {
    case MO_UB:
    case MO_SB:
        valToStore = g_output->buildCast(LLVMTrunc, valToStore, g_output->repo().int8);
        break;
    case MO_UW:
    case MO_SW:
        valToStore = g_output->buildCast(LLVMTrunc, valToStore, g_output->repo().int16);
        break;
    case MO_UL:
    case MO_SL:
        valToStore = g_output->buildCast(LLVMTrunc, valToStore, g_output->repo().int32);
        break;
    case MO_Q:
    case (MO_Q | MO_SIGN):
        break;
    default:
        EMASSERT("unknow memop" && false);
    }
    g_output->buildStore(valToStore, pointer);
}

void tcg_gen_rotr_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue arg1U = unwrapValue(arg1);
    LValue arg2U = unwrapValue(arg2);
    LValue t0 = g_output->buildLShr(arg1U, arg2U);
    LValue t1 = g_output->buildSub(g_output->repo().int32ThirtyTwo, arg2U);
    t1 = g_output->buildShl(arg1U, t1);
    LValue retVal = g_output->buildOr(t0, t1);
    storeToTCG(retVal, ret);
}

void tcg_gen_rotri_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    if (arg2 == 0) {
        storeToTCG(unwrapValue(arg1), ret);
    }
    else {
        LValue arg1U = unwrapValue(arg1);
        LValue arg2U = g_output->constInt32(arg2);
        LValue t0 = g_output->buildLShr(arg1U, arg2U);
        LValue t1 = g_output->buildSub(g_output->repo().int32ThirtyTwo, arg2U);
        t1 = g_output->buildShl(arg1U, t1);
        LValue retVal = g_output->buildOr(t0, t1);
        storeToTCG(retVal, ret);
    }
}

void tcg_gen_sar_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue retVal = g_output->buildAShr(unwrapValue(arg1), unwrapValue(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_sari_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue retVal = g_output->buildAShr(unwrapValue(arg1), g_output->constInt32(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_setcond_i32(TCGCond cond, TCGv_i32 ret,
    TCGv_i32 arg1, TCGv_i32 arg2)
{
    if (cond == TCG_COND_ALWAYS) {
        storeToTCG(g_output->repo().int32One, ret);
    }
    else if (cond == TCG_COND_NEVER) {
        storeToTCG(g_output->repo().int32Zero, ret);
    }
    else {
        LLVMIntPredicate condLLVM = tcgCondToLLVM(cond);
        LValue comp = g_output->buildICmp(condLLVM, unwrapValue(arg1), unwrapValue(arg2));
        LValue retVal = g_output->buildCast(LLVMZExt, comp, g_output->repo().int32);
        storeToTCG(retVal, ret);
    }
}

void tcg_gen_shl_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue retVal = g_output->buildShl(unwrapValue(arg1), unwrapValue(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_shli_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue retVal = g_output->buildShl(unwrapValue(arg1), g_output->constInt32(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_shli_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    LValue retVal = g_output->buildShl(unwrapValue(arg1), g_output->constInt64(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_shr_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue retVal = g_output->buildLShr(unwrapValue(arg1), unwrapValue(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_shri_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue retVal = g_output->buildLShr(unwrapValue(arg1), g_output->constInt32(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_shri_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    LValue retVal = g_output->buildLShr(unwrapValue(arg1), g_output->constInt64(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_st_i32(TCGv_i32 arg1, TCGv_ptr arg2, tcg_target_long offset)
{
    LValue pointer = g_output->buildPointerCast(unwrapValue(arg2), g_output->repo().ref8);
    pointer = g_output->buildGEP(pointer, offset);
    pointer = g_output->buildPointerCast(pointer, g_output->repo().int32);
    g_output->buildStore(unwrapValue(arg1), pointer);
}

void tcg_gen_st_i64(TCGv_i64 arg1, TCGv_ptr arg2,
    tcg_target_long offset)
{
    LValue pointer = g_output->buildPointerCast(unwrapValue(arg2), g_output->repo().ref8);
    pointer = g_output->buildGEP(pointer, offset);
    pointer = g_output->buildPointerCast(pointer, g_output->repo().int64);
    g_output->buildStore(unwrapValue(arg1), pointer);
}
