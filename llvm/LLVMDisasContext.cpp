#include <pthread.h>
#include "LLVMDisasContext.h"
#include "Registers.h"
#include "TcgGenerator.h"
#include "CompilerState.h"
#include "IntrinsicRepository.h"
#include "InitializeLLVM.h"
#include "Output.h"
#include "cpu.h"
#include "tb.h"
#include "log.h"

struct TCGCommonStruct {
    jit::LValue m_value;
    int m_size; // 32 or 64
    bool m_isMem;
};

struct TCGv_i32__ : public TCGCommonStruct {
};
struct TCGv_i64__ : public TCGCommonStruct {
};
struct TCGv_ptr__ : public TCGCommonStruct {
};

template <typename Ty>
struct TcgSizeTrait {
};

template <>
struct TcgSizeTrait<TCGv_i32> {
    static const size_t m_size = 32;
};

template <>
struct TcgSizeTrait<TCGv_ptr> {
    static const size_t m_size = 64;
};

template <>
struct TcgSizeTrait<TCGv_i64> {
    static const size_t m_size = 64;
};
namespace jit {

static PlatformDesc g_desc = {
    sizeof(CPUARMState),
    static_cast<size_t>(offsetof(CPUARMState, regs[15])), /* offset of pc */
    2, /* prologue size */
    10, /* assist size */
    10, /* tcg size */
};
static pthread_once_t initLLVMOnce = PTHREAD_ONCE_INIT;

LLVMDisasContext::LLVMDisasContext(ExecutableMemoryAllocator* executableMemAllocator, void* dispDirect, void* dispIndirect)
    : m_currentBufferPointer(nullptr)
    , m_currentBufferEnd(nullptr)
    , m_labelCount(0)
    , m_dispDirect(dispDirect)
    , m_dispIndirect(dispIndirect)
{
    pthread_once(&initLLVMOnce, initLLVM);
    m_state.reset(new CompilerState("qemu", g_desc));
    m_output.reset(new Output(*m_state));
    m_state->m_executableMemAllocator = executableMemAllocator;
}

LLVMDisasContext::~LLVMDisasContext(void)
{
    m_labelMap.clear();
    for (void* b : m_bufferList) {
        free(b);
    }
    m_bufferList.clear();
}

template <typename Type>
Type LLVMDisasContext::allocateTcg()
{
    if (m_currentBufferPointer >= m_currentBufferEnd) {
        m_currentBufferPointer = static_cast<uint8_t*>(malloc(allocate_unit));
        m_currentBufferEnd = m_currentBufferPointer + allocate_unit;
        m_bufferList.push_back(m_currentBufferPointer);
    }
    Type r = reinterpret_cast<Type>(m_currentBufferPointer);
    m_currentBufferPointer += sizeof(*r);
    r->m_value = nullptr;
    r->m_size = TcgSizeTrait<Type>::m_size;
    r->m_isMem = false;
    return r;
}

LBasicBlock LLVMDisasContext::labelToBB(int n)
{
    auto found = m_labelMap.find(n);
    EMASSERT(found != m_labelMap.end());
    LBasicBlock bb = found->second;
    return bb;
}

int LLVMDisasContext::newLabel()
{
    LBasicBlock bb = output()->appendBasicBlock("");
    m_labelMap.insert(std::make_pair(m_labelCount, bb));
    return m_labelCount++;
}

template <typename TCGType>
TCGType LLVMDisasContext::wrap(LValue v)
{
    TCGType ret = allocateTcg<TCGType>();
    ret->m_value = v;
    ret->m_isMem = false;
    return ret;
}

template <typename TCGType>
TCGType LLVMDisasContext::wrapMem(LValue v)
{
    TCGType ret = allocateTcg<TCGType>();
    ret->m_value = v;
    ret->m_isMem = true;
    return ret;
}

template <typename TCGType>
LValue LLVMDisasContext::unwrap(TCGType v)
{
    EMASSERT(v->m_value != nullptr);
    if (v->m_isMem) {
        return output()->buildLoad(v->m_value);
    }
    else {
        return v->m_value;
    }
}

template <typename TCGType>
void LLVMDisasContext::storeToTCG(LValue v, TCGType ret)
{
    if (!ret->m_isMem) {
        ret->m_value = v;
    }
    else {
        EMASSERT(ret->m_value != nullptr);
        output()->buildStore(v, ret->m_value);
    }
}

void LLVMDisasContext::extract_64_32(LValue my64, LValue thirtytwo, TCGv_i32 rl, TCGv_i32 rh)
{
    LValue rhUnwrap = output()->buildCast(LLVMTrunc, output()->buildLShr(my64, thirtytwo), output()->repo().int32);
    LValue rlUnwrap = output()->buildCast(LLVMTrunc, my64, output()->repo().int32);
    storeToTCG(rhUnwrap, rh);
    storeToTCG(rlUnwrap, rl);
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

LValue LLVMDisasContext::tcgPointerToLLVM(TCGMemOp op, TCGv pointer)
{
    int opInt = op;
    opInt &= ~MO_SIGN;
    LValue pointerBeforeCast = unwrap(pointer);
    EMASSERT(jit::typeOf(pointerBeforeCast) != output()->repo().ref32);

    switch (opInt) {
    case MO_8:
        return output()->buildCast(LLVMIntToPtr, pointerBeforeCast, output()->repo().ref8);
    case MO_16:
        return output()->buildCast(LLVMIntToPtr, pointerBeforeCast, output()->repo().ref16);
    case MO_32:
        return output()->buildCast(LLVMIntToPtr, pointerBeforeCast, output()->repo().ref32);
    case MO_64:
        return output()->buildCast(LLVMIntToPtr, pointerBeforeCast, output()->repo().ref64);
    default:
        EMASSERT("unknown pointer type." && false);
    }
}

LValue LLVMDisasContext::tcgMemCastTo32(TCGMemOp op, LValue val)
{
    switch (op) {
    case MO_8:
        return output()->buildCast(LLVMZExt, val, output()->repo().int32);
    case MO_SB:
        return output()->buildCast(LLVMSExt, val, output()->repo().int32);
    case MO_16:
        return output()->buildCast(LLVMZExt, val, output()->repo().int32);
    case MO_SW:
        return output()->buildCast(LLVMSExt, val, output()->repo().int32);
    case MO_32:
        return val;
    case MO_64:
        return output()->buildCast(LLVMTrunc, val, output()->repo().int32);
    default:
        EMASSERT("unknown pointer type." && false);
    }
}

int LLVMDisasContext::gen_new_label()
{
    return newLabel();
}

void LLVMDisasContext::gen_set_label(int n)
{
    LBasicBlock bb = labelToBB(n);
    if (!output()->currentBlockTerminated()) {
        output()->buildBr(bb);
    }
    output()->positionToBBEnd(bb);
}

TCGv_i64 LLVMDisasContext::global_mem_new_i64(int reg, intptr_t offset, const char* name)
{
    LValue v = output()->buildArgGEP(offset / sizeof(target_ulong));
    LValue v2 = output()->buildPointerCast(v, output()->repo().ref64);

    return wrapMem<TCGv_i64>(v2);
}

TCGv_i32 LLVMDisasContext::global_mem_new_i32(int reg, intptr_t offset, const char* name)
{
    LValue v = output()->buildArgGEP(offset / sizeof(target_ulong));
    LValue v2 = output()->buildPointerCast(v, output()->repo().ref32);

    return wrapMem<TCGv_i32>(v2);
}

TCGv_ptr LLVMDisasContext::global_reg_new_ptr(int reg, const char* name)
{
    LValue v = output()->buildArgGEP(0);
    return wrap<TCGv_ptr>(v);
}

TCGv_i32 LLVMDisasContext::const_i32(int32_t val)
{
    LValue v = output()->constInt32(val);
    return wrap<TCGv_i32>(v);
}

TCGv_ptr LLVMDisasContext::const_ptr(const void* val)
{
    LValue v = output()->constIntPtr(reinterpret_cast<uintptr_t>(val));
    v = output()->buildCast(LLVMIntToPtr, v, output()->repo().ref8);
    return wrap<TCGv_ptr>(v);
}

TCGv_i64 LLVMDisasContext::const_i64(int64_t val)
{
    return wrap<TCGv_i64>(output()->constInt64(val));
}

void LLVMDisasContext::gen_add2_i32(TCGv_i32 rl, TCGv_i32 rh, TCGv_i32 al,
    TCGv_i32 ah, TCGv_i32 bl, TCGv_i32 bh)
{
    LValue t0 = output()->buildCast(LLVMZExt, unwrap(al), output()->repo().int64);
    LValue t1 = output()->buildCast(LLVMZExt, unwrap(ah), output()->repo().int64);
    LValue t2 = output()->buildCast(LLVMZExt, unwrap(bl), output()->repo().int64);
    LValue t3 = output()->buildCast(LLVMZExt, unwrap(bh), output()->repo().int64);
    LValue thirtytwo = output()->constInt64(32);

    LValue t01 = output()->buildShl(t1, thirtytwo);
    t01 = output()->buildOr(t01, t0);
    LValue t23 = output()->buildShl(t3, thirtytwo);
    t23 = output()->buildOr(t23, t2);
    LValue t0123 = output()->buildAdd(t01, t23);

    extract_64_32(t0123, thirtytwo, rl, rh);
}

void LLVMDisasContext::gen_add_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue v = output()->buildAdd(unwrap(arg1), unwrap(arg2));
    storeToTCG(v, ret);
}

void LLVMDisasContext::gen_add_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    LValue v = output()->buildAdd(unwrap(arg1), unwrap(arg2));
    storeToTCG(v, ret);
}

void LLVMDisasContext::gen_addi_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue v;
    if (arg2 != 0) {
        v = output()->buildAdd(unwrap(arg1), output()->constInt32(arg2));
    }
    else {
        v = unwrap(arg1);
    }
    storeToTCG(v, ret);
}

void LLVMDisasContext::gen_addi_ptr(TCGv_ptr ret, TCGv_ptr arg1, int32_t arg2)
{
    LValue constant = output()->constInt32(arg2);
    LValue arg1V = unwrap(arg1);
    arg1V = output()->buildCast(LLVMPtrToInt, arg1V, output()->repo().intPtr);
    LValue retVal = output()->buildAdd(arg1V, constant);
    storeToTCG(output()->buildCast(LLVMIntToPtr, retVal, output()->repo().ref8), ret);
}

void LLVMDisasContext::gen_addi_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    LValue v;
    if (arg2 != 0) {
        v = output()->buildAdd(unwrap(arg1), output()->constInt64(arg2));
    }
    else {
        v = unwrap(arg1);
    }
    storeToTCG(v, ret);
}

void LLVMDisasContext::gen_andc_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue t0 = output()->buildNot(unwrap(arg2));
    LValue v = output()->buildAnd(unwrap(arg1), t0);
    storeToTCG(v, ret);
}

void LLVMDisasContext::gen_and_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue v = output()->buildAnd(unwrap(arg1), unwrap(arg2));
    storeToTCG(v, ret);
}

void LLVMDisasContext::gen_and_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    LValue v = output()->buildAnd(unwrap(arg1), unwrap(arg2));
    storeToTCG(v, ret);
}

void LLVMDisasContext::gen_andi_i32(TCGv_i32 ret, TCGv_i32 arg1, uint32_t arg2)
{
    LValue v = output()->buildAnd(unwrap(arg1), output()->constInt32(arg2));
    storeToTCG(v, ret);
}

void LLVMDisasContext::gen_andi_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    LValue v = output()->buildAnd(unwrap(arg1), output()->constInt64(arg2));
    storeToTCG(v, ret);
}

void LLVMDisasContext::gen_brcondi_i32(TCGCond cond, TCGv_i32 arg1,
    int32_t arg2, int label_index)
{
    LBasicBlock taken = labelToBB(label_index);
    LBasicBlock nottaken = output()->appendBasicBlock("notTaken");
    LValue v1 = unwrap(arg1);
    LValue v2 = output()->constInt32(arg2);
    LValue condVal = output()->buildICmp(tcgCondToLLVM(cond), v1, v2);
    output()->buildCondBr(condVal, taken, nottaken);
    output()->positionToBBEnd(nottaken);
}

void LLVMDisasContext::gen_bswap16_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue v = unwrap(arg);
    LValue t0 = output()->buildAnd(v, output()->repo().int32TwoFiveFive);
    t0 = output()->buildShl(t0, output()->repo().int32Eight);
    LValue retVal = output()->buildLShr(v, output()->repo().int32Eight);
    retVal = output()->buildOr(retVal, t0);
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_bswap32_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue v = unwrap(arg);
    LValue twentyFour = output()->constInt32(24);
    LValue hi24 = output()->buildShl(v, twentyFour);
    LValue hi16 = output()->buildAnd(v, output()->constInt32(0xff00));
    hi16 = output()->buildShl(hi16, output()->repo().int32Eight);
    LValue lo8 = output()->buildAnd(v, output()->constInt32(0xff0000));
    lo8 = output()->buildLShr(lo8, output()->repo().int32Eight);
    LValue lo0 = output()->buildAnd(v, output()->constInt32(0xff000000));
    lo0 = output()->buildLShr(lo0, twentyFour);
    LValue ret1 = output()->buildOr(hi24, hi16);
    LValue ret2 = output()->buildOr(lo0, lo8);
    LValue retVal = output()->buildOr(ret1, ret2);
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_concat_i32_i64(TCGv_i64 dest, TCGv_i32 low,
    TCGv_i32 high)
{
    LValue lo = unwrap(low);
    LValue hi = unwrap(high);
    LValue low64 = output()->buildCast(LLVMZExt, lo, output()->repo().int64);
    LValue hi64 = output()->buildCast(LLVMZExt, hi, output()->repo().int64);
    hi64 = output()->buildShl(hi64, output()->constInt64(32));
    LValue ret = output()->buildOr(hi64, low64);
    storeToTCG(ret, dest);
}

void LLVMDisasContext::gen_deposit_i32(TCGv_i32 ret, TCGv_i32 arg1,
    TCGv_i32 arg2, unsigned int ofs,
    unsigned int len)
{
    if (ofs == 0 && len == 32) {
        gen_mov_i32(ret, arg2);
        return;
    }
    LValue v;
    unsigned mask = (1u << len) - 1;
    if (ofs + len < 32) {
        v = output()->buildAnd(unwrap(arg2), output()->constInt32(mask));
        v = output()->buildShl(v, output()->constInt32(ofs));
    }
    else {
        v = output()->buildShl(unwrap(arg2), output()->constInt32(ofs));
    }
    LValue retVal = output()->buildAnd(unwrap(arg1), output()->constInt32(~(mask << ofs)));
    retVal = output()->buildOr(retVal, v);
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_mov_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    if (arg == ret)
        return;
    storeToTCG(unwrap(arg), ret);
}

void LLVMDisasContext::gen_exit_tb(int direct)
{
    if (direct)
        output()->buildTcgDirectPatch();
    else
        output()->buildTcgIndirectPatch();
}

void LLVMDisasContext::gen_ext16s_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue retVal = output()->buildShl(unwrap(arg), output()->repo().int32Sixteen);
    retVal = output()->buildAShr(retVal, output()->repo().int32Sixteen);
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_ext16u_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue retVal = output()->buildAnd(unwrap(arg), output()->constInt32(0xffff));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_ext32u_i64(TCGv_i64 ret, TCGv_i64 arg)
{
    LValue retVal = output()->buildAnd(unwrap(arg), output()->constInt64(0xffffffffu));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_ext8s_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue constant = output()->constInt32(24);
    LValue retVal = output()->buildShl(unwrap(arg), constant);
    retVal = output()->buildAShr(retVal, constant);
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_ext8u_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue retVal = output()->buildAnd(unwrap(arg), output()->constInt32(0xff));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_ext_i32_i64(TCGv_i64 ret, TCGv_i32 arg)
{
    LValue retVal = output()->buildCast(LLVMSExt, unwrap(arg), output()->repo().int64);
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_extu_i32_i64(TCGv_i64 ret, TCGv_i32 arg)
{
    LValue retVal = output()->buildCast(LLVMZExt, unwrap(arg), output()->repo().int64);
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_ld_i32(TCGv_i32 ret, TCGv_ptr arg2, tcg_target_long offset)
{
    LValue pointer = unwrap(arg2);
    pointer = output()->buildPointerCast(pointer, output()->repo().ref8);
    pointer = output()->buildGEP(pointer, offset);
    pointer = output()->buildPointerCast(pointer, output()->repo().ref32);
    LValue retVal = output()->buildLoad(pointer);
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_ld_i64(TCGv_i64 ret, TCGv_ptr arg2,
    target_long offset)
{
    LValue pointer = unwrap(arg2);
    pointer = output()->buildPointerCast(pointer, output()->repo().ref8);
    pointer = output()->buildGEP(pointer, offset);
    pointer = output()->buildPointerCast(pointer, output()->repo().ref64);
    LValue retVal = output()->buildLoad(pointer);
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_movcond_i32(TCGCond cond, TCGv_i32 ret,
    TCGv_i32 c1, TCGv_i32 c2,
    TCGv_i32 v1, TCGv_i32 v2)
{
    LValue t0;
    switch (cond) {
    case TCG_COND_ALWAYS:
        t0 = output()->repo().int32One;
        break;
    case TCG_COND_NEVER:
        t0 = output()->repo().int32Zero;
        break;
    default:
        t0 = output()->buildICmp(tcgCondToLLVM(cond), unwrap(c1), unwrap(c2));
    }

    LValue retVal = output()->buildSelect(t0, unwrap(v1), unwrap(v2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_movcond_i64(TCGCond cond, TCGv_i64 ret,
    TCGv_i64 c1, TCGv_i64 c2,
    TCGv_i64 v1, TCGv_i64 v2)
{
    LValue t0;
    switch (cond) {
    case TCG_COND_ALWAYS:
        t0 = output()->repo().int32One;
        break;
    case TCG_COND_NEVER:
        t0 = output()->repo().int32Zero;
        break;
    default:
        t0 = output()->buildICmp(tcgCondToLLVM(cond), unwrap(c1), unwrap(c2));
    }

    LValue retVal = output()->buildSelect(t0, unwrap(v1), unwrap(v2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_mov_i64(TCGv_i64 ret, TCGv_i64 arg)
{
    if (ret == arg)
        return;
    storeToTCG(unwrap(arg), ret);
}

void LLVMDisasContext::gen_movi_i32(TCGv_i32 ret, int32_t arg)
{
    LValue retVal = output()->constInt32(arg);
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_movi_i64(TCGv_i64 ret, int64_t arg)
{
    LValue retVal = output()->constInt64(arg);
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_mul_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue retVal = output()->buildMul(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_muls2_i32(TCGv_i32 rl, TCGv_i32 rh,
    TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue t0 = output()->buildCast(LLVMSExt, unwrap(arg1), output()->repo().int64);
    LValue t1 = output()->buildCast(LLVMSExt, unwrap(arg2), output()->repo().int64);
    LValue t3 = output()->buildMul(t0, t1);
    LValue low = output()->buildCast(LLVMTrunc, t3, output()->repo().int32);
    LValue high = output()->buildAShr(t3, output()->constInt64(32));
    high = output()->buildCast(LLVMTrunc, high, output()->repo().int32);
    storeToTCG(low, rl);
    storeToTCG(high, rh);
}

void LLVMDisasContext::gen_mulu2_i32(TCGv_i32 rl, TCGv_i32 rh,
    TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue t0 = output()->buildCast(LLVMZExt, unwrap(arg1), output()->repo().int64);
    LValue t1 = output()->buildCast(LLVMZExt, unwrap(arg2), output()->repo().int64);
    LValue t3 = output()->buildMul(t0, t1);
    LValue low = output()->buildCast(LLVMTrunc, t3, output()->repo().int32);
    LValue high = output()->buildLShr(t3, output()->constInt64(32));
    high = output()->buildCast(LLVMTrunc, high, output()->repo().int32);
    storeToTCG(low, rl);
    storeToTCG(high, rh);
}

void LLVMDisasContext::gen_neg_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue retVal = output()->buildNeg(unwrap(arg));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_neg_i64(TCGv_i64 ret, TCGv_i64 arg)
{
    LValue retVal = output()->buildNeg(unwrap(arg));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_not_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue retVal = output()->buildNot(unwrap(arg));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_orc_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue t0 = output()->buildNot(unwrap(arg2));
    LValue retVal = output()->buildOr(unwrap(arg1), t0);
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_or_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue retVal = output()->buildOr(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_or_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    LValue retVal = output()->buildOr(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_ori_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue retVal = output()->buildOr(unwrap(arg1), output()->constInt32(arg2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_qemu_ld_i32(TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
    EMASSERT(idx == 0);
    LValue pointer = tcgPointerToLLVM(memop, addr);
    LValue retVal = output()->buildLoad(pointer);
    retVal = tcgMemCastTo32(memop, retVal);
    switch (memop) {
    case MO_UB:
        retVal = output()->buildCast(LLVMZExt, retVal, output()->repo().int32);
        break;
    case MO_SB:
        retVal = output()->buildCast(LLVMSExt, retVal, output()->repo().int32);
        break;
    case MO_UW:
        retVal = output()->buildCast(LLVMZExt, retVal, output()->repo().int32);
        break;
    case MO_SW:
        retVal = output()->buildCast(LLVMSExt, retVal, output()->repo().int32);
        break;
    case MO_SL:
    case MO_UL:
        break;
    case MO_Q:
    case (MO_Q | MO_SIGN):
        retVal = output()->buildCast(LLVMTrunc, retVal, output()->repo().int32);
    default:
        EMASSERT("unknown pointer type." && false);
    }
    storeToTCG(retVal, val);
}

void LLVMDisasContext::gen_qemu_ld_i64(TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
    EMASSERT(idx == 0);
    LValue pointer = tcgPointerToLLVM(memop, addr);
    LValue retVal = output()->buildLoad(pointer);
    switch (memop) {
    case MO_UB:
        retVal = output()->buildCast(LLVMZExt, retVal, output()->repo().int64);
        break;
    case MO_SB:
        retVal = output()->buildCast(LLVMSExt, retVal, output()->repo().int64);
        break;
    case MO_UW:
        retVal = output()->buildCast(LLVMZExt, retVal, output()->repo().int64);
        break;
    case MO_SW:
        retVal = output()->buildCast(LLVMSExt, retVal, output()->repo().int64);
        break;
    case MO_SL:
        retVal = output()->buildCast(LLVMSExt, retVal, output()->repo().int64);
        break;
    case MO_UL:
        retVal = output()->buildCast(LLVMZExt, retVal, output()->repo().int64);
        break;
    case MO_Q:
    case (MO_Q | MO_SIGN):
        break;
    default:
        EMASSERT("unknown pointer type." && false);
    }
    storeToTCG(retVal, val);
}

void LLVMDisasContext::gen_qemu_st_i32(TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
    EMASSERT(idx == 0);
    LValue pointer = tcgPointerToLLVM(memop, addr);
    LValue valToStore = unwrap(val);
    switch (memop) {
    case MO_UB:
    case MO_SB:
        valToStore = output()->buildCast(LLVMTrunc, valToStore, output()->repo().int8);
        break;
    case MO_UW:
    case MO_SW:
        valToStore = output()->buildCast(LLVMTrunc, valToStore, output()->repo().int16);
        break;
    case MO_UL:
    case MO_SL:
        break;
    case MO_Q:
        valToStore = output()->buildCast(LLVMZExt, valToStore, output()->repo().int32);
        break;
    case (MO_Q | MO_SIGN):
        valToStore = output()->buildCast(LLVMSExt, valToStore, output()->repo().int32);
        break;
    default:
        EMASSERT("unknown memop" && false);
    }
    output()->buildStore(valToStore, pointer);
}

void LLVMDisasContext::gen_qemu_st_i64(TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
    EMASSERT(idx == 0);
    LValue pointer = tcgPointerToLLVM(memop, addr);
    LValue valToStore = unwrap(val);
    switch (memop) {
    case MO_UB:
    case MO_SB:
        valToStore = output()->buildCast(LLVMTrunc, valToStore, output()->repo().int8);
        break;
    case MO_UW:
    case MO_SW:
        valToStore = output()->buildCast(LLVMTrunc, valToStore, output()->repo().int16);
        break;
    case MO_UL:
    case MO_SL:
        valToStore = output()->buildCast(LLVMTrunc, valToStore, output()->repo().int32);
        break;
    case MO_Q:
    case (MO_Q | MO_SIGN):
        break;
    default:
        EMASSERT("unknown memop" && false);
    }
    output()->buildStore(valToStore, pointer);
}

void LLVMDisasContext::gen_rotr_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue arg1U = unwrap(arg1);
    LValue arg2U = unwrap(arg2);
    LValue t0 = output()->buildLShr(arg1U, arg2U);
    LValue t1 = output()->buildSub(output()->repo().int32ThirtyTwo, arg2U);
    t1 = output()->buildShl(arg1U, t1);
    LValue retVal = output()->buildOr(t0, t1);
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_rotri_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    if (arg2 == 0) {
        storeToTCG(unwrap(arg1), ret);
    }
    else {
        LValue arg1U = unwrap(arg1);
        LValue arg2U = output()->constInt32(arg2);
        LValue t0 = output()->buildLShr(arg1U, arg2U);
        LValue t1 = output()->buildSub(output()->repo().int32ThirtyTwo, arg2U);
        t1 = output()->buildShl(arg1U, t1);
        LValue retVal = output()->buildOr(t0, t1);
        storeToTCG(retVal, ret);
    }
}

void LLVMDisasContext::gen_sar_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue retVal = output()->buildAShr(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_sari_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue retVal = output()->buildAShr(unwrap(arg1), output()->constInt32(arg2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_setcond_i32(TCGCond cond, TCGv_i32 ret,
    TCGv_i32 arg1, TCGv_i32 arg2)
{
    if (cond == TCG_COND_ALWAYS) {
        storeToTCG(output()->repo().int32One, ret);
    }
    else if (cond == TCG_COND_NEVER) {
        storeToTCG(output()->repo().int32Zero, ret);
    }
    else {
        LLVMIntPredicate condLLVM = tcgCondToLLVM(cond);
        LValue comp = output()->buildICmp(condLLVM, unwrap(arg1), unwrap(arg2));
        LValue retVal = output()->buildCast(LLVMZExt, comp, output()->repo().int32);
        storeToTCG(retVal, ret);
    }
}

void LLVMDisasContext::gen_shl_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue retVal = output()->buildShl(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_shli_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue retVal = output()->buildShl(unwrap(arg1), output()->constInt32(arg2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_shli_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    LValue retVal = output()->buildShl(unwrap(arg1), output()->constInt64(arg2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_shr_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue retVal = output()->buildLShr(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_shri_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue retVal = output()->buildLShr(unwrap(arg1), output()->constInt32(arg2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_shri_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    LValue retVal = output()->buildLShr(unwrap(arg1), output()->constInt64(arg2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_st_i32(TCGv_i32 arg1, TCGv_ptr arg2, tcg_target_long offset)
{
    LValue pointer = output()->buildPointerCast(unwrap(arg2), output()->repo().ref8);
    pointer = output()->buildGEP(pointer, offset);
    pointer = output()->buildPointerCast(pointer, output()->repo().ref32);
    output()->buildStore(unwrap(arg1), pointer);
}

void LLVMDisasContext::gen_st_i64(TCGv_i64 arg1, TCGv_ptr arg2,
    tcg_target_long offset)
{
    LValue pointer = output()->buildPointerCast(unwrap(arg2), output()->repo().ref8);
    pointer = output()->buildGEP(pointer, offset);
    pointer = output()->buildPointerCast(pointer, output()->repo().ref64);
    output()->buildStore(unwrap(arg1), pointer);
}

void LLVMDisasContext::gen_sub_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue retVal = output()->buildSub(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_sub_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    LValue retVal = output()->buildSub(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_subi_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue retVal = output()->buildSub(unwrap(arg1), output()->constInt32(arg2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_trunc_i64_i32(TCGv_i32 ret, TCGv_i64 arg)
{
    LValue retVal = output()->buildCast(LLVMTrunc, unwrap(arg), output()->repo().int32);
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_xor_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue retVal = output()->buildXor(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_xor_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    LValue retVal = output()->buildXor(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void LLVMDisasContext::gen_xori_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue retVal = output()->buildXor(unwrap(arg1), output()->constInt32(arg2));
    storeToTCG(retVal, ret);
}

TCGv_i32 LLVMDisasContext::temp_local_new_i32()
{
    return allocateTcg<TCGv_i32>();
}

TCGv_i32 LLVMDisasContext::temp_new_i32()
{
    return temp_local_new_i32();
}

TCGv_ptr LLVMDisasContext::temp_new_ptr()
{
    return allocateTcg<TCGv_ptr>();
}

TCGv_i64 LLVMDisasContext::temp_new_i64()
{
    return allocateTcg<TCGv_i64>();
}

void LLVMDisasContext::temp_free_i32(TCGv_i32 a)
{
}

void LLVMDisasContext::temp_free_i64(TCGv_i64 a)
{
}

void LLVMDisasContext::temp_free_ptr(TCGv_ptr a)
{
}

LValue LLVMDisasContext::myhandleCallRet(void* func, TCGArg ret,
    int nargs, TCGArg* args)
{
    // function retval other parameters
    LValue argsV[nargs];
    for (int i = 0; i < nargs; ++i) {
        argsV[i] = unwrap(reinterpret_cast<TCGCommonStruct*>(args[i]));
    }
    LValue retVal = output()->buildTcgHelperCall(reinterpret_cast<void*>(func), nargs, argsV);
    return retVal;
}

void LLVMDisasContext::myhandleCallRetNone(void* func, int nargs, TCGArg* args)
{
    LValue argsV[nargs];
    for (int i = 0; i < nargs; ++i) {
        argsV[i] = unwrap(reinterpret_cast<TCGCommonStruct*>(args[i]));
    }
    output()->buildTcgHelperCallNotRet(func, nargs, argsV);
}

void LLVMDisasContext::gen_callN(void* func, TCGArg ret,
    int nargs, TCGArg* args)
{
    if (ret != TCG_CALL_DUMMY_ARG) {
        LValue retVal = myhandleCallRet(func, ret, nargs, args);
        int size = reinterpret_cast<TCGCommonStruct*>(ret)->m_size;
        if (size == 64) {
            storeToTCG(retVal, reinterpret_cast<TCGv_ptr>(ret));
        }
        else {
            retVal = output()->buildCast(LLVMTrunc, retVal, output()->repo().int32);
            storeToTCG(retVal, reinterpret_cast<TCGv_ptr>(ret));
        }
    }
    else {
        myhandleCallRetNone(func, nargs, args);
    }
}

void LLVMDisasContext::func_start()
{
}

void LLVMDisasContext::gen_sdiv(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue num = unwrap(arg1);
    LValue den = unwrap(arg2);
    LBasicBlock denZeroTaken = output()->appendBasicBlock("denZeroTaken");
    LBasicBlock denZeroNotTaken = output()->appendBasicBlock("denZeroNotTaken");

    LBasicBlock minTaken = output()->appendBasicBlock("minTaken");
    LBasicBlock minNotTaken = output()->appendBasicBlock("minNotTaken");
    LBasicBlock merge = output()->appendBasicBlock("merge");
    LValue denCompZero = output()->buildICmp(LLVMIntEQ, den, output()->repo().int32Zero);
    output()->buildCondBr(denCompZero, denZeroTaken, denZeroNotTaken);
    output()->positionToBBEnd(denZeroNotTaken);
    LValue intMin = output()->constInt32(INT_MIN);
    LValue intMinCmp = output()->buildICmp(LLVMIntEQ, num, intMin);
    LValue denCompNegOne = output()->buildICmp(LLVMIntEQ, den, output()->repo().int32NegativeOne);
    LValue andBoth = output()->buildAnd(intMinCmp, denCompNegOne);

    output()->buildCondBr(andBoth, minTaken, minNotTaken);
    output()->positionToBBEnd(minNotTaken);
    LValue signDiv = output()->buildSDiv(num, den);
    output()->buildBr(merge);
    output()->positionToBBEnd(denZeroTaken);
    output()->buildBr(merge);
    output()->positionToBBEnd(minTaken);
    output()->buildBr(merge);
    output()->positionToBBEnd(merge);
    LValue phi = output()->buildPhi(output()->repo().int32);
    LValue zero = output()->repo().int32Zero;
    jit::addIncoming(phi, &signDiv, &minNotTaken, 1);
    jit::addIncoming(phi, &zero, &denZeroTaken, 1);
    jit::addIncoming(phi, &intMin, &minTaken, 1);
    storeToTCG(phi, ret);
}

void LLVMDisasContext::gen_udiv(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue num = unwrap(arg1);
    LValue den = unwrap(arg2);
    LBasicBlock denZeroTaken = output()->appendBasicBlock("denZeroTaken");
    LBasicBlock denZeroNotTaken = output()->appendBasicBlock("denZeroNotTaken");

    LBasicBlock merge = output()->appendBasicBlock("merge");
    LValue denCompZero = output()->buildICmp(LLVMIntEQ, den, output()->repo().int32Zero);
    output()->buildCondBr(denCompZero, denZeroTaken, denZeroNotTaken);
    output()->positionToBBEnd(denZeroNotTaken);

    LValue signDiv = output()->buildUDiv(num, den);
    output()->buildBr(merge);
    output()->positionToBBEnd(denZeroTaken);
    output()->buildBr(merge);
    output()->positionToBBEnd(merge);
    LValue phi = output()->buildPhi(output()->repo().int32);
    LValue zero = output()->repo().int32Zero;
    jit::addIncoming(phi, &signDiv, &denZeroNotTaken, 1);
    jit::addIncoming(phi, &zero, &denZeroTaken, 1);
    storeToTCG(phi, ret);
}

#define DEFINE_VFP_OP(name1, name2, type, size)                                                              \
    void LLVMDisasContext::gen_vfp_##name1(TCGv_i##size ret, TCGv_i##size arg1, TCGv_i##size arg2, TCGv_ptr) \
    {                                                                                                        \
        EMASSERT(ret->m_size == arg1->m_size && arg1->m_size == arg2->m_size && arg2->m_size == size);       \
        LValue arg1V = unwrap(arg1);                                                                         \
        LValue arg2V = unwrap(arg2);                                                                         \
        arg1V = output()->buildBitCast(arg1V, output()->repo().type);                                        \
        arg2V = output()->buildBitCast(arg2V, output()->repo().type);                                        \
        LValue retVal = output()->build##name2(arg1V, arg2V);                                                \
        retVal = output()->buildBitCast(retVal, output()->repo().int##size);                                 \
        storeToTCG(retVal, ret);                                                                             \
    }

DEFINE_VFP_OP(adds, FAdd, floatType, 32)
DEFINE_VFP_OP(subs, FSub, floatType, 32)
DEFINE_VFP_OP(muls, FMul, floatType, 32)
DEFINE_VFP_OP(divs, FDiv, floatType, 32)

DEFINE_VFP_OP(addd, FAdd, doubleType, 64)
DEFINE_VFP_OP(subd, FSub, doubleType, 64)
DEFINE_VFP_OP(muld, FMul, doubleType, 64)
DEFINE_VFP_OP(divd, FDiv, doubleType, 64)
#undef DEFINE_VFP_OP

#define DEFINE_VFP_OP(name, op, type, size)                                         \
    void LLVMDisasContext::gen_vfp_##name(TCGv_i32 ret, TCGv_i##size arg, TCGv_ptr) \
    {                                                                               \
        LValue argV = unwrap(arg);                                                  \
        argV = output()->buildBitCast(argV, output()->repo().type);                 \
        LValue retVal = output()->buildCast(op, argV, output()->repo().int##size);  \
        storeToTCG(retVal, ret);                                                    \
    }

DEFINE_VFP_OP(touis, LLVMFPToUI, floatType, 32)
DEFINE_VFP_OP(touizs, LLVMFPToUI, floatType, 32)
DEFINE_VFP_OP(tosis, LLVMFPToSI, floatType, 32)
DEFINE_VFP_OP(tosizs, LLVMFPToSI, floatType, 32)

DEFINE_VFP_OP(touid, LLVMFPToUI, doubleType, 64)
DEFINE_VFP_OP(touizd, LLVMFPToUI, doubleType, 64)
DEFINE_VFP_OP(tosid, LLVMFPToSI, doubleType, 64)
DEFINE_VFP_OP(tosizd, LLVMFPToSI, doubleType, 64)
#undef DEFINE_VFP_OP

#define DEFINE_VFP_OP(name, op, type, size)                                         \
    void LLVMDisasContext::gen_vfp_##name(TCGv_i##size ret, TCGv_i32 arg, TCGv_ptr) \
    {                                                                               \
        LValue argV = unwrap(arg);                                                  \
        LValue retVal = output()->buildCast(op, argV, output()->repo().type);       \
        retVal = output()->buildBitCast(retVal, output()->repo().int##size);        \
        storeToTCG(retVal, ret);                                                    \
    }

DEFINE_VFP_OP(sitos, LLVMSIToFP, floatType, 32)
DEFINE_VFP_OP(uitos, LLVMUIToFP, floatType, 32)
DEFINE_VFP_OP(sitod, LLVMSIToFP, doubleType, 64)
DEFINE_VFP_OP(uitod, LLVMUIToFP, doubleType, 64)
#undef DEFINE_VFP_OP
}
