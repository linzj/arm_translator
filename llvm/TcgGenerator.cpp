#include <unordered_map>
#include <vector>
#include <memory>
#include <pthread.h>
#include "Registers.h"
#include "Compile.h"
#include "Link.h"
#include "TcgGenerator.h"
#include "CompilerState.h"
#include "IntrinsicRepository.h"
#include "InitializeLLVM.h"
#include "Output.h"
#include "cpu.h"
#include "tb.h"
#include "translate.h"
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

using namespace jit;
namespace {

class MyDisCtx : public DisasContext {
public:
    MyDisCtx();
    ~MyDisCtx();
    inline Output* output() { return m_output.get(); }
    inline CompilerState* state() { return m_state.get(); }
    template <typename Type>
    Type allocateTcg();
    LBasicBlock labelToBB(int n);
    int newLabel();

private:
    uint8_t* m_currentBufferPointer;
    uint8_t* m_currentBufferEnd;
    const static size_t allocate_unit = 4096 * 16;
    typedef std::vector<void*> TcgBufferList;
    TcgBufferList m_bufferList;
    std::unique_ptr<CompilerState> m_state;
    std::unique_ptr<Output> m_output;
    typedef std::unordered_map<int, LBasicBlock> LabelMap;
    LabelMap m_labelMap;
    int m_labelCount;
};

static PlatformDesc g_desc = {
    sizeof(CPUARMState),
    static_cast<size_t>(offsetof(CPUARMState, regs[15])), /* offset of pc */
    2, /* prologue size */
    10, /* assist size */
    10, /* tcg size */
};
static pthread_once_t initLLVMOnce = PTHREAD_ONCE_INIT;

MyDisCtx::MyDisCtx(void)
    : m_currentBufferPointer(nullptr)
    , m_currentBufferEnd(nullptr)
    , m_labelCount(0)
{
    pthread_once(&initLLVMOnce, initLLVM);
    m_state.reset(new CompilerState("qemu", g_desc));
    m_output.reset(new Output(*m_state));
}

MyDisCtx::~MyDisCtx(void)
{
    m_labelMap.clear();
    for (void* b : m_bufferList) {
        free(b);
    }
    m_bufferList.clear();
}

template <typename Type>
Type MyDisCtx::allocateTcg()
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

LBasicBlock MyDisCtx::labelToBB(int n)
{
    auto found = m_labelMap.find(n);
    EMASSERT(found != m_labelMap.end());
    LBasicBlock bb = found->second;
    return bb;
}

int MyDisCtx::newLabel()
{
    LBasicBlock bb = output()->appendBasicBlock("");
    m_labelMap.insert(std::make_pair(m_labelCount, bb));
    return m_labelCount++;
}

static inline void cpu_get_tb_cpu_state(CPUARMState* env, target_ulong* pc,
    uint64_t* flags)
{
    int fpen;

    if (arm_feature(env, ARM_FEATURE_V6)) {
        fpen = extract32(env->cp15.c1_coproc, 20, 2);
    }
    else {
        /* CPACR doesn't exist before v6, so VFP is always accessible */
        fpen = 3;
    }

    if (is_a64(env)) {
        *pc = env->pc;
        *flags = ARM_TBFLAG_AARCH64_STATE_MASK
            | (arm_current_el(env) << ARM_TBFLAG_AA64_EL_SHIFT);
        if (fpen == 3 || (fpen == 1 && arm_current_el(env) != 0)) {
            *flags |= ARM_TBFLAG_AA64_FPEN_MASK;
        }
        /* The SS_ACTIVE and PSTATE_SS bits correspond to the state machine
         * states defined in the ARM ARM for software singlestep:
         *  SS_ACTIVE   PSTATE.SS   State
         *     0            x       Inactive (the TB flag for SS is always 0)
         *     1            0       Active-pending
         *     1            1       Active-not-pending
         */
        if (arm_singlestep_active(env)) {
            *flags |= ARM_TBFLAG_AA64_SS_ACTIVE_MASK;
            if (env->pstate & PSTATE_SS) {
                *flags |= ARM_TBFLAG_AA64_PSTATE_SS_MASK;
            }
        }
    }
    else {
        int privmode;
        *pc = env->regs[15];
        *flags = (env->thumb << ARM_TBFLAG_THUMB_SHIFT)
            | (env->vfp.vec_len << ARM_TBFLAG_VECLEN_SHIFT)
            | (env->vfp.vec_stride << ARM_TBFLAG_VECSTRIDE_SHIFT)
            | (env->condexec_bits << ARM_TBFLAG_CONDEXEC_SHIFT)
            | (env->bswap_code << ARM_TBFLAG_BSWAP_CODE_SHIFT);
        if (arm_feature(env, ARM_FEATURE_M)) {
            privmode = !((env->v7m.exception == 0) && (env->v7m.control & 1));
        }
        else {
            privmode = (env->uncached_cpsr & CPSR_M) != ARM_CPU_MODE_USR;
        }
        if (privmode) {
            *flags |= ARM_TBFLAG_PRIV_MASK;
        }
        if (env->vfp.xregs[ARM_VFP_FPEXC] & (1 << 30)
            || arm_el_is_aa64(env, 1)) {
            *flags |= ARM_TBFLAG_VFPEN_MASK;
        }
        if (fpen == 3 || (fpen == 1 && arm_current_el(env) != 0)) {
            *flags |= ARM_TBFLAG_CPACR_FPEN_MASK;
        }
        /* The SS_ACTIVE and PSTATE_SS bits correspond to the state machine
         * states defined in the ARM ARM for software singlestep:
         *  SS_ACTIVE   PSTATE.SS   State
         *     0            x       Inactive (the TB flag for SS is always 0)
         *     1            0       Active-pending
         *     1            1       Active-not-pending
         */
        if (arm_singlestep_active(env)) {
            *flags |= ARM_TBFLAG_SS_ACTIVE_MASK;
            if (env->uncached_cpsr & PSTATE_SS) {
                *flags |= ARM_TBFLAG_PSTATE_SS_MASK;
            }
        }
        *flags |= (extract32(env->cp15.c15_cpar, 0, 2)
            << ARM_TBFLAG_XSCALE_CPAR_SHIFT);
    }
}

static inline unsigned iregEnc3210(unsigned in)
{
    return in;
}

inline static uint8_t mkModRegRM(unsigned mod, unsigned reg, unsigned regmem)
{
    return (uint8_t)(((mod & 3) << 6) | ((reg & 7) << 3) | (regmem & 7));
}

inline static uint8_t* doAMode_R__wrk(uint8_t* p, unsigned gregEnc3210, unsigned eregEnc3210)
{
    *p++ = mkModRegRM(3, gregEnc3210 & 7, eregEnc3210 & 7);
    return p;
}

static uint8_t* doAMode_R(uint8_t* p, unsigned greg, unsigned ereg)
{
    return doAMode_R__wrk(p, iregEnc3210(greg), iregEnc3210(ereg));
}

static uint8_t* emit32(uint8_t* p, uint32_t w32)
{
    *reinterpret_cast<uint32_t*>(p) = w32;
    return p + sizeof(w32);
}

static void patchProloge(void*, uint8_t* start)
{
    uint8_t* p = start;
    // 2 bytes
    *p++ = 0x89;
    p = doAMode_R(p, jit::RBP,
        jit::RCX);
}

static void patchDirect(void*, uint8_t* p, void* entry)
{
    // epilogue

    // 2 bytes
    *p++ = 0x89;
    p = doAMode_R(p, jit::RBP,
        jit::RSP);
    // 1 bytes pop rbp
    *p++ = 0x5d;

    /* 5 bytes: mov $target, %eax */
    *p++ = 0xB8;
    p = emit32(p, reinterpret_cast<uintptr_t>(entry));

    /* 2 bytes: call*%eax */
    *p++ = 0xff;
    *p++ = 0xd0;
}

void patchIndirect(void*, uint8_t* p, void* entry)
{
    // epilogue

    // 2 bytes
    *p++ = 0x89;
    p = doAMode_R(p, jit::RBP,
        jit::RSP);
    // 1 bytes pop rbp
    *p++ = 0x5d;

    /* 5 bytes: mov $target, %eax */
    *p++ = 0xB8;
    p = emit32(p, reinterpret_cast<uintptr_t>(entry));

    /* 2 bytes: jmp *%eax */
    *p++ = 0xff;
    *p++ = 0xe0;
}
}
namespace jit {

void translate(CPUARMState* env, const TranslateDesc& desc, void** buffer, size_t* s)
{
    MyDisCtx ctx;
    ARMCPU* cpu = arm_env_get_cpu(env);
    target_ulong pc;
    uint64_t flags;
    cpu_get_tb_cpu_state(env, &pc, &flags);
    TranslationBlock tb = { pc, flags };

    gen_intermediate_code_internal(cpu, &tb, &ctx);
#ifdef ENABLE_DUMP_LLVM_MODULE
    dumpModule(ctx.state()->m_module);
#endif // ENABLE_DUMP_LLVM_MODULE
    compile(*ctx.state());
    LinkDesc linkDesc = {
        nullptr,
        desc.m_dispAssist,
        desc.m_dispDirect,
        desc.m_dispIndirect,
        patchProloge,
        patchDirect,
        patchDirect,
        patchIndirect,
    };
    link(*ctx.state(), linkDesc);
    const void* codeBuffer = ctx.state()->m_codeSectionList.front().data();
    size_t codeSize = ctx.state()->m_codeSectionList.front().size();
    *buffer = malloc(codeSize);
    *s = codeSize;
    memcpy(*buffer, codeBuffer, codeSize);
}
}

template <typename Type>
static Type allocateTcg(DisasContext* s)
{
    return static_cast<MyDisCtx*>(s)->allocateTcg<Type>();
}

template <typename TCGType>
static TCGType wrap(DisasContext* s, LValue v)
{
    TCGType ret = allocateTcg<TCGType>(s);
    ret->m_value = v;
    ret->m_isMem = false;
    return ret;
}

template <typename TCGType>
static TCGType wrapMem(DisasContext* s, LValue v)
{
    TCGType ret = allocateTcg<TCGType>(s);
    ret->m_value = v;
    ret->m_isMem = true;
    return ret;
}

template <typename TCGType>
static LValue unwrap(DisasContext* s, TCGType v)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    EMASSERT(v->m_value != nullptr);
    if (v->m_isMem) {
        return myctx->output()->buildLoad(v->m_value);
    }
    else {
        return v->m_value;
    }
}

template <typename TCGType>
static void storeToTCG(DisasContext* s, LValue v, TCGType ret)
{
    if (!ret->m_isMem) {
        ret->m_value = v;
    }
    else {
        MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
        EMASSERT(ret->m_value != nullptr);
        myctx->output()->buildStore(v, ret->m_value);
    }
}

static void extract_64_32(DisasContext* s, LValue my64, LValue thirtytwo, TCGv_i32 rl, TCGv_i32 rh)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue rhUnwrap = myctx->output()->buildCast(LLVMTrunc, myctx->output()->buildLShr(my64, thirtytwo), myctx->output()->repo().int32);
    LValue rlUnwrap = myctx->output()->buildCast(LLVMTrunc, my64, myctx->output()->repo().int32);
    storeToTCG(s, rhUnwrap, rh);
    storeToTCG(s, rlUnwrap, rl);
}

static LLVMIntPredicate tcgCondToLLVM(DisasContext* s, TCGCond cond)
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

static LValue tcgPointerToLLVM(DisasContext* s, TCGMemOp op, TCGv pointer)
{
    int opInt = op;
    opInt &= ~MO_SIGN;
    LValue pointerBeforeCast = unwrap(s, pointer);
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    EMASSERT(jit::typeOf(pointerBeforeCast) != myctx->output()->repo().ref32);

    switch (op) {
    case MO_8:
        return myctx->output()->buildCast(LLVMIntToPtr, pointerBeforeCast, myctx->output()->repo().ref8);
    case MO_16:
        return myctx->output()->buildCast(LLVMIntToPtr, pointerBeforeCast, myctx->output()->repo().ref16);
    case MO_32:
        return myctx->output()->buildCast(LLVMIntToPtr, pointerBeforeCast, myctx->output()->repo().ref32);
    case MO_64:
        return myctx->output()->buildCast(LLVMIntToPtr, pointerBeforeCast, myctx->output()->repo().ref64);
    default:
        EMASSERT("unknown pointer type." && false);
    }
}

static LValue tcgMemCastTo32(DisasContext* s, TCGMemOp op, LValue val)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    switch (op) {
    case MO_8:
        return myctx->output()->buildCast(LLVMZExt, val, myctx->output()->repo().int32);
    case MO_SB:
        return myctx->output()->buildCast(LLVMSExt, val, myctx->output()->repo().int32);
    case MO_16:
        return myctx->output()->buildCast(LLVMZExt, val, myctx->output()->repo().int32);
    case MO_SW:
        return myctx->output()->buildCast(LLVMSExt, val, myctx->output()->repo().int32);
    case MO_32:
        return val;
    case MO_64:
        return myctx->output()->buildCast(LLVMTrunc, val, myctx->output()->repo().int32);
    default:
        EMASSERT("unknown pointer type." && false);
    }
}

TCGv_i64 tcg_global_mem_new_i64(DisasContext* s, int, intptr_t offset, const char* name)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue v = myctx->output()->buildArgGEP(offset / sizeof(target_ulong));
    LValue v2 = myctx->output()->buildPointerCast(v, myctx->output()->repo().ref64);

    return wrapMem<TCGv_i64>(s, v2);
}

TCGv_i32 tcg_global_mem_new_i32(DisasContext* s, int, intptr_t offset, const char* name)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue v = myctx->output()->buildArgGEP(offset / sizeof(target_ulong));
    LValue v2 = myctx->output()->buildPointerCast(v, myctx->output()->repo().ref32);

    return wrapMem<TCGv_i32>(s, v2);
}

TCGv_ptr tcg_global_reg_new_ptr(DisasContext* s, int, const char* name)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue v = myctx->output()->buildArgGEP(0);
    return wrap<TCGv_ptr>(s, v);
}

TCGv_i32 tcg_const_i32(DisasContext* s, int32_t val)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue v = myctx->output()->constInt32(val);
    return wrap<TCGv_i32>(s, v);
}

TCGv_ptr tcg_const_ptr(DisasContext* s, const void* val)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue v = myctx->output()->constIntPtr(reinterpret_cast<uintptr_t>(val));
    v = myctx->output()->buildCast(LLVMIntToPtr, v, myctx->output()->repo().ref8);
    return wrap<TCGv_ptr>(s, v);
}

TCGv_i64 tcg_const_i64(DisasContext* s, int64_t val)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    return wrap<TCGv_i64>(s, myctx->output()->constInt64(val));
}

int gen_new_label(DisasContext* s)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    return myctx->newLabel();
}

void gen_set_label(DisasContext* s, int n)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LBasicBlock bb = myctx->labelToBB(n);
    if (!myctx->output()->currentBlockTerminated()) {
        myctx->output()->buildBr(bb);
    }
    myctx->output()->positionToBBEnd(bb);
}

void tcg_gen_add2_i32(DisasContext* s, TCGv_i32 rl, TCGv_i32 rh, TCGv_i32 al,
    TCGv_i32 ah, TCGv_i32 bl, TCGv_i32 bh)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue t0 = myctx->output()->buildCast(LLVMZExt, unwrap(s, al), myctx->output()->repo().int64);
    LValue t1 = myctx->output()->buildCast(LLVMZExt, unwrap(s, ah), myctx->output()->repo().int64);
    LValue t2 = myctx->output()->buildCast(LLVMZExt, unwrap(s, bl), myctx->output()->repo().int64);
    LValue t3 = myctx->output()->buildCast(LLVMZExt, unwrap(s, bh), myctx->output()->repo().int64);
    LValue thirtytwo = myctx->output()->constInt64(32);

    LValue t01 = myctx->output()->buildShl(t1, thirtytwo);
    t01 = myctx->output()->buildOr(t01, t0);
    LValue t23 = myctx->output()->buildShl(t3, thirtytwo);
    t23 = myctx->output()->buildOr(t23, t2);
    LValue t0123 = myctx->output()->buildAdd(t01, t23);

    extract_64_32(s, t0123, thirtytwo, rl, rh);
}

void tcg_gen_add_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue v = myctx->output()->buildAdd(unwrap(s, arg1), unwrap(s, arg2));
    storeToTCG(s, v, ret);
}

void tcg_gen_add_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue v = myctx->output()->buildAdd(unwrap(s, arg1), unwrap(s, arg2));
    storeToTCG(s, v, ret);
}

void tcg_gen_addi_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue v;
    if (arg2 != 0) {
        MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
        v = myctx->output()->buildAdd(unwrap(s, arg1), myctx->output()->constInt32(arg2));
    }
    else {
        v = unwrap(s, arg1);
    }
    storeToTCG(s, v, ret);
}

void tcg_gen_addi_ptr(DisasContext* s, TCGv_ptr ret, TCGv_ptr arg1, int32_t arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue constant = myctx->output()->constInt32(arg2);
    LValue arg1V = unwrap(s, arg1);
    arg1V = myctx->output()->buildCast(LLVMPtrToInt, arg1V, myctx->output()->repo().intPtr);
    LValue retVal = myctx->output()->buildAdd(arg1V, constant);
    storeToTCG(s, myctx->output()->buildCast(LLVMIntToPtr, retVal, myctx->output()->repo().ref8), ret);
}

void tcg_gen_addi_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    LValue v;
    if (arg2 != 0) {
        MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
        v = myctx->output()->buildAdd(unwrap(s, arg1), myctx->output()->constInt64(arg2));
    }
    else {
        v = unwrap(s, arg1);
    }
    storeToTCG(s, v, ret);
}

void tcg_gen_andc_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue t0 = myctx->output()->buildNot(unwrap(s, arg2));
    LValue v = myctx->output()->buildAnd(unwrap(s, arg1), t0);
    storeToTCG(s, v, ret);
}

void tcg_gen_and_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue v = myctx->output()->buildAnd(unwrap(s, arg1), unwrap(s, arg2));
    storeToTCG(s, v, ret);
}

void tcg_gen_and_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue v = myctx->output()->buildAnd(unwrap(s, arg1), unwrap(s, arg2));
    storeToTCG(s, v, ret);
}

void tcg_gen_andi_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, uint32_t arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue v = myctx->output()->buildAnd(unwrap(s, arg1), myctx->output()->constInt32(arg2));
    storeToTCG(s, v, ret);
}

void tcg_gen_andi_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue v = myctx->output()->buildAnd(unwrap(s, arg1), myctx->output()->constInt64(arg2));
    storeToTCG(s, v, ret);
}

void tcg_gen_brcondi_i32(DisasContext* s, TCGCond cond, TCGv_i32 arg1,
    int32_t arg2, int label_index)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LBasicBlock taken = myctx->labelToBB(label_index);
    LBasicBlock nottaken = myctx->output()->appendBasicBlock("notTaken");
    LValue v1 = unwrap(s, arg1);
    LValue v2 = myctx->output()->constInt32(arg2);
    LValue condVal = myctx->output()->buildICmp(tcgCondToLLVM(s, cond), v1, v2);
    myctx->output()->buildCondBr(condVal, taken, nottaken);
    myctx->output()->positionToBBEnd(nottaken);
}

void tcg_gen_bswap16_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg)
{
    LValue v = unwrap(s, arg);
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue lower = myctx->output()->buildAnd(v, myctx->output()->repo().int32TwoFiveFive);
    LValue higher = myctx->output()->buildShl(v, myctx->output()->repo().int32Eight);
    LValue valret = myctx->output()->buildOr(higher, lower);
    storeToTCG(s, valret, ret);
}

void tcg_gen_bswap32_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg)
{
    LValue v = unwrap(s, arg);
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue twentyFour = myctx->output()->constInt32(24);
    LValue hi24 = myctx->output()->buildShl(v, twentyFour);
    LValue hi16 = myctx->output()->buildAnd(v, myctx->output()->constInt32(0xff00));
    hi16 = myctx->output()->buildShl(hi16, myctx->output()->repo().int32Eight);
    LValue lo8 = myctx->output()->buildAnd(v, myctx->output()->constInt32(0xff0000));
    lo8 = myctx->output()->buildLShr(lo8, myctx->output()->repo().int32Eight);
    LValue lo0 = myctx->output()->buildAnd(v, myctx->output()->constInt32(0xff000000));
    lo0 = myctx->output()->buildLShr(lo0, twentyFour);
    LValue ret1 = myctx->output()->buildOr(hi24, hi16);
    LValue ret2 = myctx->output()->buildOr(lo0, lo8);
    LValue retVal = myctx->output()->buildOr(ret1, ret2);
    storeToTCG(s, retVal, ret);
}

void tcg_gen_concat_i32_i64(DisasContext* s, TCGv_i64 dest, TCGv_i32 low,
    TCGv_i32 high)
{
    LValue lo = unwrap(s, low);
    LValue hi = unwrap(s, high);
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue low64 = myctx->output()->buildCast(LLVMZExt, lo, myctx->output()->repo().int64);
    LValue hi64 = myctx->output()->buildCast(LLVMZExt, hi64, myctx->output()->repo().int64);
    hi64 = myctx->output()->buildShl(hi64, myctx->output()->repo().int32ThirtyTwo);
    LValue ret = myctx->output()->buildOr(hi64, low64);
    storeToTCG(s, ret, dest);
}

void tcg_gen_deposit_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1,
    TCGv_i32 arg2, unsigned int ofs,
    unsigned int len)
{
    if (ofs == 0 && len == 32) {
        tcg_gen_mov_i32(s, ret, arg2);
        return;
    }
    LValue v;
    unsigned mask = (1u << len) - 1;
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    if (ofs + len < 32) {
        v = myctx->output()->buildAnd(unwrap(s, arg2), myctx->output()->constInt32(mask));
        v = myctx->output()->buildShl(v, myctx->output()->constInt32(ofs));
    }
    else {
        v = myctx->output()->buildShl(unwrap(s, arg2), myctx->output()->constInt32(ofs));
    }
    LValue retVal = myctx->output()->buildAnd(unwrap(s, arg1), myctx->output()->constInt32(~(mask << ofs)));
    retVal = myctx->output()->buildOr(retVal, v);
    storeToTCG(s, retVal, ret);
}

void tcg_gen_mov_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg)
{
    if (arg == ret)
        return;
    storeToTCG(s, unwrap(s, arg), ret);
}

void tcg_gen_mov_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg)
{
    if (ret == arg)
        return;
    storeToTCG(s, unwrap(s, arg), ret);
}

void tcg_gen_exit_tb(DisasContext* s, int direct)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    if (direct)
        myctx->output()->buildTcgDirectPatch();
    else
        myctx->output()->buildTcgIndirectPatch();
}

void tcg_gen_ext16s_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildShl(unwrap(s, arg), myctx->output()->repo().int32Sixteen);
    retVal = myctx->output()->buildAShr(retVal, myctx->output()->repo().int32Sixteen);
    storeToTCG(s, retVal, ret);
}

void tcg_gen_ext16u_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildAnd(unwrap(s, arg), myctx->output()->constInt32(0xffff));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_ext32u_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildAnd(unwrap(s, arg), myctx->output()->constInt64(0xffffffffu));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_ext8s_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue constant = myctx->output()->constInt32(24);
    LValue retVal = myctx->output()->buildShl(unwrap(s, arg), constant);
    retVal = myctx->output()->buildAShr(retVal, constant);
    storeToTCG(s, retVal, ret);
}

void tcg_gen_ext8u_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildAnd(unwrap(s, arg), myctx->output()->constInt32(0xff));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_ext_i32_i64(DisasContext* s, TCGv_i64 ret, TCGv_i32 arg)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildCast(LLVMSExt, unwrap(s, arg), myctx->output()->repo().int64);
    storeToTCG(s, retVal, ret);
}

void tcg_gen_extu_i32_i64(DisasContext* s, TCGv_i64 ret, TCGv_i32 arg)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildCast(LLVMZExt, unwrap(s, arg), myctx->output()->repo().int64);
    storeToTCG(s, retVal, ret);
}

void tcg_gen_ld_i32(DisasContext* s, TCGv_i32 ret, TCGv_ptr arg2, tcg_target_long offset)
{
    LValue pointer = unwrap(s, arg2);
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    pointer = myctx->output()->buildPointerCast(pointer, myctx->output()->repo().ref8);
    pointer = myctx->output()->buildGEP(pointer, offset);
    pointer = myctx->output()->buildPointerCast(pointer, myctx->output()->repo().ref32);
    LValue retVal = myctx->output()->buildLoad(pointer);
    storeToTCG(s, retVal, ret);
}

void tcg_gen_ld_i64(DisasContext* s, TCGv_i64 ret, TCGv_ptr arg2,
    tcg_target_long offset)
{
    LValue pointer = unwrap(s, arg2);
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    pointer = myctx->output()->buildPointerCast(pointer, myctx->output()->repo().ref8);
    pointer = myctx->output()->buildGEP(pointer, offset);
    pointer = myctx->output()->buildPointerCast(pointer, myctx->output()->repo().ref64);
    LValue retVal = myctx->output()->buildLoad(pointer);
    storeToTCG(s, retVal, ret);
}

void tcg_gen_movcond_i32(DisasContext* s, TCGCond cond, TCGv_i32 ret,
    TCGv_i32 c1, TCGv_i32 c2,
    TCGv_i32 v1, TCGv_i32 v2)
{
    LValue t0;
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    switch (cond) {
    case TCG_COND_ALWAYS:
        t0 = myctx->output()->repo().int32One;
        break;
    case TCG_COND_NEVER:
        t0 = myctx->output()->repo().int32Zero;
        break;
    default:
        t0 = myctx->output()->buildICmp(tcgCondToLLVM(s, cond), unwrap(s, c1), unwrap(s, c2));
    }

    LValue retVal = myctx->output()->buildSelect(t0, unwrap(s, v1), unwrap(s, v2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_movcond_i64(DisasContext* s, TCGCond cond, TCGv_i64 ret,
    TCGv_i64 c1, TCGv_i64 c2,
    TCGv_i64 v1, TCGv_i64 v2)
{
    LValue t0;
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    switch (cond) {
    case TCG_COND_ALWAYS:
        t0 = myctx->output()->repo().int32One;
        break;
    case TCG_COND_NEVER:
        t0 = myctx->output()->repo().int32Zero;
        break;
    default:
        t0 = myctx->output()->buildICmp(tcgCondToLLVM(s, cond), unwrap(s, c1), unwrap(s, c2));
    }

    LValue retVal = myctx->output()->buildSelect(t0, unwrap(s, v1), unwrap(s, v2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_movi_i32(DisasContext* s, TCGv_i32 ret, int32_t arg)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->constInt32(arg);
    storeToTCG(s, retVal, ret);
}

void tcg_gen_movi_i64(DisasContext* s, TCGv_i64 ret, int64_t arg)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->constInt64(arg);
    storeToTCG(s, retVal, ret);
}

void tcg_gen_mul_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildMul(unwrap(s, arg1), unwrap(s, arg2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_muls2_i32(DisasContext* s, TCGv_i32 rl, TCGv_i32 rh,
    TCGv_i32 arg1, TCGv_i32 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue t0 = myctx->output()->buildCast(LLVMSExt, unwrap(s, arg1), myctx->output()->repo().int64);
    LValue t1 = myctx->output()->buildCast(LLVMSExt, unwrap(s, arg2), myctx->output()->repo().int64);
    LValue t3 = myctx->output()->buildMul(t0, t1);
    LValue low = myctx->output()->buildCast(LLVMTrunc, t3, myctx->output()->repo().int32);
    LValue high = myctx->output()->buildAShr(t3, myctx->output()->repo().int32ThirtyTwo);
    high = myctx->output()->buildCast(LLVMTrunc, high, myctx->output()->repo().int32);
    storeToTCG(s, low, rl);
    storeToTCG(s, high, rh);
}

void tcg_gen_mulu2_i32(DisasContext* s, TCGv_i32 rl, TCGv_i32 rh,
    TCGv_i32 arg1, TCGv_i32 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue t0 = myctx->output()->buildCast(LLVMZExt, unwrap(s, arg1), myctx->output()->repo().int64);
    LValue t1 = myctx->output()->buildCast(LLVMZExt, unwrap(s, arg2), myctx->output()->repo().int64);
    LValue t3 = myctx->output()->buildMul(t0, t1);
    LValue low = myctx->output()->buildCast(LLVMTrunc, t3, myctx->output()->repo().int32);
    LValue high = myctx->output()->buildLShr(t3, myctx->output()->repo().int32ThirtyTwo);
    high = myctx->output()->buildCast(LLVMTrunc, high, myctx->output()->repo().int32);
    storeToTCG(s, low, rl);
    storeToTCG(s, high, rh);
}

void tcg_gen_neg_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildNeg(unwrap(s, arg));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_neg_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildNeg(unwrap(s, arg));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_not_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildNot(unwrap(s, arg));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_orc_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue t0 = myctx->output()->buildNot(unwrap(s, arg2));
    LValue retVal = myctx->output()->buildOr(unwrap(s, arg1), t0);
    storeToTCG(s, retVal, ret);
}

void tcg_gen_or_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildOr(unwrap(s, arg1), unwrap(s, arg2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_or_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildOr(unwrap(s, arg1), unwrap(s, arg2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_ori_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildOr(unwrap(s, arg1), myctx->output()->constInt32(arg2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_qemu_ld_i32(DisasContext* s, TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
    EMASSERT(idx == 0);
    LValue pointer = tcgPointerToLLVM(s, memop, addr);
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildLoad(pointer);
    retVal = tcgMemCastTo32(s, memop, retVal);
    switch (memop) {
    case MO_UB:
        retVal = myctx->output()->buildCast(LLVMZExt, retVal, myctx->output()->repo().int32);
        break;
    case MO_SB:
        retVal = myctx->output()->buildCast(LLVMSExt, retVal, myctx->output()->repo().int32);
        break;
    case MO_UW:
        retVal = myctx->output()->buildCast(LLVMZExt, retVal, myctx->output()->repo().int32);
        break;
    case MO_SW:
        retVal = myctx->output()->buildCast(LLVMSExt, retVal, myctx->output()->repo().int32);
        break;
    case MO_SL:
    case MO_UL:
        break;
    case MO_Q:
    case (MO_Q | MO_SIGN):
        retVal = myctx->output()->buildCast(LLVMTrunc, retVal, myctx->output()->repo().int32);
    default:
        EMASSERT("unknown pointer type." && false);
    }
    storeToTCG(s, retVal, val);
}

void tcg_gen_qemu_ld_i64(DisasContext* s, TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
    EMASSERT(idx == 0);
    LValue pointer = tcgPointerToLLVM(s, memop, addr);
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildLoad(pointer);
    switch (memop) {
    case MO_UB:
        retVal = myctx->output()->buildCast(LLVMZExt, retVal, myctx->output()->repo().int64);
        break;
    case MO_SB:
        retVal = myctx->output()->buildCast(LLVMSExt, retVal, myctx->output()->repo().int64);
        break;
    case MO_UW:
        retVal = myctx->output()->buildCast(LLVMZExt, retVal, myctx->output()->repo().int64);
        break;
    case MO_SW:
        retVal = myctx->output()->buildCast(LLVMSExt, retVal, myctx->output()->repo().int64);
        break;
    case MO_SL:
        retVal = myctx->output()->buildCast(LLVMSExt, retVal, myctx->output()->repo().int64);
        break;
    case MO_UL:
        retVal = myctx->output()->buildCast(LLVMZExt, retVal, myctx->output()->repo().int64);
        break;
    case MO_Q:
    case (MO_Q | MO_SIGN):
        break;
    default:
        EMASSERT("unknown pointer type." && false);
    }
    storeToTCG(s, retVal, val);
}

void tcg_gen_qemu_st_i32(DisasContext* s, TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
    EMASSERT(idx == 0);
    LValue pointer = tcgPointerToLLVM(s, memop, addr);
    LValue valToStore = unwrap(s, val);
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    switch (memop) {
    case MO_UB:
    case MO_SB:
        valToStore = myctx->output()->buildCast(LLVMTrunc, valToStore, myctx->output()->repo().int8);
        break;
    case MO_UW:
    case MO_SW:
        valToStore = myctx->output()->buildCast(LLVMTrunc, valToStore, myctx->output()->repo().int16);
        break;
    case MO_UL:
    case MO_SL:
        break;
    case MO_Q:
        valToStore = myctx->output()->buildCast(LLVMZExt, valToStore, myctx->output()->repo().int32);
        break;
    case (MO_Q | MO_SIGN):
        valToStore = myctx->output()->buildCast(LLVMSExt, valToStore, myctx->output()->repo().int32);
        break;
    default:
        EMASSERT("unknown memop" && false);
    }
    myctx->output()->buildStore(valToStore, pointer);
}

void tcg_gen_qemu_st_i64(DisasContext* s, TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
    EMASSERT(idx == 0);
    LValue pointer = tcgPointerToLLVM(s, memop, addr);
    LValue valToStore = unwrap(s, val);
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    switch (memop) {
    case MO_UB:
    case MO_SB:
        valToStore = myctx->output()->buildCast(LLVMTrunc, valToStore, myctx->output()->repo().int8);
        break;
    case MO_UW:
    case MO_SW:
        valToStore = myctx->output()->buildCast(LLVMTrunc, valToStore, myctx->output()->repo().int16);
        break;
    case MO_UL:
    case MO_SL:
        valToStore = myctx->output()->buildCast(LLVMTrunc, valToStore, myctx->output()->repo().int32);
        break;
    case MO_Q:
    case (MO_Q | MO_SIGN):
        break;
    default:
        EMASSERT("unknown memop" && false);
    }
    myctx->output()->buildStore(valToStore, pointer);
}

void tcg_gen_rotr_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue arg1U = unwrap(s, arg1);
    LValue arg2U = unwrap(s, arg2);
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue t0 = myctx->output()->buildLShr(arg1U, arg2U);
    LValue t1 = myctx->output()->buildSub(myctx->output()->repo().int32ThirtyTwo, arg2U);
    t1 = myctx->output()->buildShl(arg1U, t1);
    LValue retVal = myctx->output()->buildOr(t0, t1);
    storeToTCG(s, retVal, ret);
}

void tcg_gen_rotri_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    if (arg2 == 0) {
        storeToTCG(s, unwrap(s, arg1), ret);
    }
    else {
        LValue arg1U = unwrap(s, arg1);
        MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
        LValue arg2U = myctx->output()->constInt32(arg2);
        LValue t0 = myctx->output()->buildLShr(arg1U, arg2U);
        LValue t1 = myctx->output()->buildSub(myctx->output()->repo().int32ThirtyTwo, arg2U);
        t1 = myctx->output()->buildShl(arg1U, t1);
        LValue retVal = myctx->output()->buildOr(t0, t1);
        storeToTCG(s, retVal, ret);
    }
}

void tcg_gen_sar_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildAShr(unwrap(s, arg1), unwrap(s, arg2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_sari_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildAShr(unwrap(s, arg1), myctx->output()->constInt32(arg2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_setcond_i32(DisasContext* s, TCGCond cond, TCGv_i32 ret,
    TCGv_i32 arg1, TCGv_i32 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    if (cond == TCG_COND_ALWAYS) {
        storeToTCG(s, myctx->output()->repo().int32One, ret);
    }
    else if (cond == TCG_COND_NEVER) {
        storeToTCG(s, myctx->output()->repo().int32Zero, ret);
    }
    else {
        LLVMIntPredicate condLLVM = tcgCondToLLVM(s, cond);
        LValue comp = myctx->output()->buildICmp(condLLVM, unwrap(s, arg1), unwrap(s, arg2));
        LValue retVal = myctx->output()->buildCast(LLVMZExt, comp, myctx->output()->repo().int32);
        storeToTCG(s, retVal, ret);
    }
}

void tcg_gen_shl_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildShl(unwrap(s, arg1), unwrap(s, arg2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_shli_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildShl(unwrap(s, arg1), myctx->output()->constInt32(arg2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_shli_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildShl(unwrap(s, arg1), myctx->output()->constInt64(arg2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_shr_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildLShr(unwrap(s, arg1), unwrap(s, arg2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_shri_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildLShr(unwrap(s, arg1), myctx->output()->constInt32(arg2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_shri_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildLShr(unwrap(s, arg1), myctx->output()->constInt64(arg2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_st_i32(DisasContext* s, TCGv_i32 arg1, TCGv_ptr arg2, tcg_target_long offset)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue pointer = myctx->output()->buildPointerCast(unwrap(s, arg2), myctx->output()->repo().ref8);
    pointer = myctx->output()->buildGEP(pointer, offset);
    pointer = myctx->output()->buildPointerCast(pointer, myctx->output()->repo().ref32);
    myctx->output()->buildStore(unwrap(s, arg1), pointer);
}

void tcg_gen_st_i64(DisasContext* s, TCGv_i64 arg1, TCGv_ptr arg2,
    tcg_target_long offset)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue pointer = myctx->output()->buildPointerCast(unwrap(s, arg2), myctx->output()->repo().ref8);
    pointer = myctx->output()->buildGEP(pointer, offset);
    pointer = myctx->output()->buildPointerCast(pointer, myctx->output()->repo().ref64);
    myctx->output()->buildStore(unwrap(s, arg1), pointer);
}

void tcg_gen_sub_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildSub(unwrap(s, arg1), unwrap(s, arg2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_sub_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildSub(unwrap(s, arg1), unwrap(s, arg2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_subi_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildSub(unwrap(s, arg1), myctx->output()->constInt32(arg2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_trunc_i64_i32(DisasContext* s, TCGv_i32 ret, TCGv_i64 arg)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildCast(LLVMTrunc, unwrap(s, arg), myctx->output()->repo().int32);
    storeToTCG(s, retVal, ret);
}

void tcg_gen_xor_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildXor(unwrap(s, arg1), unwrap(s, arg2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_xor_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildXor(unwrap(s, arg1), unwrap(s, arg2));
    storeToTCG(s, retVal, ret);
}

void tcg_gen_xori_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildXor(unwrap(s, arg1), myctx->output()->constInt32(arg2));
    storeToTCG(s, retVal, ret);
}

TCGv_i32 tcg_temp_local_new_i32(DisasContext* s)
{
    return allocateTcg<TCGv_i32>(s);
}

TCGv_i32 tcg_temp_new_i32(DisasContext* s)
{
    return tcg_temp_local_new_i32(s);
}

TCGv_ptr tcg_temp_new_ptr(DisasContext* s)
{
    return allocateTcg<TCGv_ptr>(s);
}

TCGv_i64 tcg_temp_new_i64(DisasContext* s)
{
    return allocateTcg<TCGv_i64>(s);
}

static LValue myhandleCallRet(DisasContext* s, void* func, TCGArg ret,
    int nargs, TCGArg* args)
{
    // function retval other parameters
    LValue argsV[nargs];
    for (int i = 0; i < nargs; ++i) {
        argsV[i] = unwrap(s, reinterpret_cast<TCGCommonStruct*>(args[i]));
    }
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LValue retVal = myctx->output()->buildTcgHelperCall(reinterpret_cast<void*>(func), nargs, argsV);
    return retVal;
}

static void myhandleCallRetNone(DisasContext* s, void* func, int nargs, TCGArg* args)
{
    LValue argsV[nargs];
    for (int i = 0; i < nargs; ++i) {
        argsV[i] = unwrap(s, reinterpret_cast<TCGCommonStruct*>(args[i]));
    }
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    myctx->output()->buildTcgHelperCallNotRet(func, nargs, argsV);
}

void tcg_gen_callN(DisasContext* s, void* func, TCGArg ret,
    int nargs, TCGArg* args)
{
    if (ret != TCG_CALL_DUMMY_ARG) {
        LValue retVal = myhandleCallRet(s, func, ret, nargs, args);
        int size = reinterpret_cast<TCGCommonStruct*>(ret)->m_size;
        MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
        if (size == 64) {
            storeToTCG(s, retVal, reinterpret_cast<TCGv_ptr>(ret));
        }
        else {
            retVal = myctx->output()->buildCast(LLVMTrunc, retVal, myctx->output()->repo().int32);
            storeToTCG(s, retVal, reinterpret_cast<TCGv_ptr>(ret));
        }
    }
    else {
        myhandleCallRetNone(s, func, nargs, args);
    }
}

void tcg_gen_sdiv(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue num = unwrap(s, arg1);
    LValue den = unwrap(s, arg2);
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LBasicBlock denZeroTaken = myctx->output()->appendBasicBlock("denZeroTaken");
    LBasicBlock denZeroNotTaken = myctx->output()->appendBasicBlock("denZeroNotTaken");

    LBasicBlock minTaken = myctx->output()->appendBasicBlock("minTaken");
    LBasicBlock minNotTaken = myctx->output()->appendBasicBlock("minNotTaken");
    LBasicBlock merge = myctx->output()->appendBasicBlock("merge");
    LValue denCompZero = myctx->output()->buildICmp(LLVMIntEQ, den, myctx->output()->repo().int32Zero);
    myctx->output()->buildCondBr(denCompZero, denZeroTaken, denZeroNotTaken);
    myctx->output()->positionToBBEnd(denZeroNotTaken);
    LValue intMin = myctx->output()->constInt32(INT_MIN);
    LValue intMinCmp = myctx->output()->buildICmp(LLVMIntEQ, num, intMin);
    LValue denCompNegOne = myctx->output()->buildICmp(LLVMIntEQ, den, myctx->output()->repo().int32NegativeOne);
    LValue andBoth = myctx->output()->buildAnd(intMinCmp, denCompNegOne);

    myctx->output()->buildCondBr(andBoth, minTaken, minNotTaken);
    myctx->output()->positionToBBEnd(minNotTaken);
    LValue signDiv = myctx->output()->buildSDiv(num, den);
    myctx->output()->buildBr(merge);
    myctx->output()->positionToBBEnd(denZeroTaken);
    myctx->output()->buildBr(merge);
    myctx->output()->positionToBBEnd(minTaken);
    myctx->output()->buildBr(merge);
    myctx->output()->positionToBBEnd(merge);
    LValue phi = myctx->output()->buildPhi(myctx->output()->repo().int32);
    LValue zero = myctx->output()->repo().int32Zero;
    jit::addIncoming(phi, &signDiv, &minNotTaken, 1);
    jit::addIncoming(phi, &zero, &denZeroTaken, 1);
    jit::addIncoming(phi, &intMin, &minTaken, 1);
    storeToTCG(s, phi, ret);
}

void tcg_gen_udiv(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue num = unwrap(s, arg1);
    LValue den = unwrap(s, arg2);
    MyDisCtx* myctx = static_cast<MyDisCtx*>(s);
    LBasicBlock denZeroTaken = myctx->output()->appendBasicBlock("denZeroTaken");
    LBasicBlock denZeroNotTaken = myctx->output()->appendBasicBlock("denZeroNotTaken");

    LBasicBlock merge = myctx->output()->appendBasicBlock("merge");
    LValue denCompZero = myctx->output()->buildICmp(LLVMIntEQ, den, myctx->output()->repo().int32Zero);
    myctx->output()->buildCondBr(denCompZero, denZeroTaken, denZeroNotTaken);
    myctx->output()->positionToBBEnd(denZeroNotTaken);

    LValue signDiv = myctx->output()->buildUDiv(num, den);
    myctx->output()->buildBr(merge);
    myctx->output()->positionToBBEnd(denZeroTaken);
    myctx->output()->buildBr(merge);
    myctx->output()->positionToBBEnd(merge);
    LValue phi = myctx->output()->buildPhi(myctx->output()->repo().int32);
    LValue zero = myctx->output()->repo().int32Zero;
    jit::addIncoming(phi, &signDiv, &denZeroNotTaken, 1);
    jit::addIncoming(phi, &zero, &denZeroTaken, 1);
    storeToTCG(s, phi, ret);
}

#define DEFINE_VFP_OP(name1, name2, type, size)                                                        \
    void tcg_gen_vfp_##name1(DisasContext* s, TCGv_i##size ret, TCGv_i##size arg1, TCGv_i##size arg2)  \
    {                                                                                                  \
        MyDisCtx* myctx = static_cast<MyDisCtx*>(s);                                                   \
        EMASSERT(ret->m_size == arg1->m_size && arg1->m_size == arg2->m_size && arg2->m_size == size); \
        LValue arg1V = unwrap(s, arg1);                                                                \
        LValue arg2V = unwrap(s, arg2);                                                                \
        arg1V = myctx->output()->buildBitCast(arg1V, myctx->output()->repo().type);                    \
        arg2V = myctx->output()->buildBitCast(arg2V, myctx->output()->repo().type);                    \
        LValue retVal = myctx->output()->build##name2(arg1V, arg2V);                                   \
        retVal = myctx->output()->buildBitCast(retVal, myctx->output()->repo().int##size);             \
        storeToTCG(s, retVal, ret);                                                                    \
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

#define DEFINE_VFP_OP(name, op, type, size)                                                      \
    void tcg_gen_vfp_##name(DisasContext* s, TCGv_i32 ret, TCGv_i##size arg)                     \
    {                                                                                            \
        MyDisCtx* myctx = static_cast<MyDisCtx*>(s);                                             \
        LValue argV = unwrap(s, arg);                                                            \
        argV = myctx->output()->buildBitCast(argV, myctx->output()->repo().type);                \
        LValue retVal = myctx->output()->buildCast(op, argV, myctx->output()->repo().int##size); \
        storeToTCG(s, retVal, ret);                                                              \
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

#define DEFINE_VFP_OP(name, op, type, size)                                                 \
    void tcg_gen_vfp_##name(DisasContext* s, TCGv_i##size ret, TCGv_i32 arg)                \
    {                                                                                       \
        MyDisCtx* myctx = static_cast<MyDisCtx*>(s);                                        \
        LValue argV = unwrap(s, arg);                                                       \
        LValue retVal = myctx->output()->buildCast(op, argV, myctx->output()->repo().type); \
        retVal = myctx->output()->buildBitCast(retVal, myctx->output()->repo().int##size);  \
        storeToTCG(s, retVal, ret);                                                         \
    }

DEFINE_VFP_OP(sitos, LLVMSIToFP, floatType, 32)
DEFINE_VFP_OP(uitos, LLVMUIToFP, floatType, 32)
DEFINE_VFP_OP(sitod, LLVMSIToFP, doubleType, 64)
DEFINE_VFP_OP(uitod, LLVMUIToFP, doubleType, 64)
#undef DEFINE_VFP_OP
