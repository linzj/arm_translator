#include <unordered_map>
#include <vector>
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

extern "C" {
void trampolineForHelper32Ret64_0(void);
void trampolineForHelper32Ret64_1(void);
void trampolineForHelper32Ret64_2(void);
void trampolineForHelper32Ret64_3(void);
void trampolineForHelper32Ret64_4(void);
void trampolineForHelper32Ret64_5(void);

void trampolineForHelper32Ret32_0(void);
void trampolineForHelper32Ret32_1(void);
void trampolineForHelper32Ret32_2(void);
void trampolineForHelper32Ret32_3(void);
void trampolineForHelper32Ret32_4(void);
void trampolineForHelper32Ret32_5(void);
}

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

const static size_t allocate_unit = 4096 * 16;
typedef std::vector<void*> TcgBufferList;
static TcgBufferList g_bufferList;
uint8_t* g_currentBufferPointer;
uint8_t* g_currentBufferEnd;

static CompilerState* g_state;
static Output* g_output;

typedef std::unordered_map<int, LBasicBlock> LabelMap;
LabelMap g_labelMap;

static PlatformDesc g_desc = {
    sizeof(CPUARMState),
    static_cast<size_t>(offsetof(CPUARMState, regs[15])), /* offset of pc */
    2, /* prologue size */
    10, /* assist size */
    10, /* tcg size */
};

template <typename Type>
static Type allocateTcg()
{
    if (g_currentBufferPointer >= g_currentBufferEnd) {
        g_currentBufferPointer = static_cast<uint8_t*>(malloc(allocate_unit));
        g_currentBufferEnd = g_currentBufferPointer + allocate_unit;
        g_bufferList.push_back(g_currentBufferPointer);
    }
    Type r = reinterpret_cast<Type>(g_currentBufferPointer);
    g_currentBufferPointer += sizeof(*r);
    r->m_value = nullptr;
    r->m_size = TcgSizeTrait<Type>::m_size;
    r->m_isMem = false;
    return r;
}

static void clearTcgBuffer()
{
    for (void* b : g_bufferList) {
        free(b);
    }
    g_bufferList.clear();
    g_currentBufferPointer = nullptr;
    g_currentBufferEnd = nullptr;
}

static pthread_once_t initLLVMOnce = PTHREAD_ONCE_INIT;

static void llvm_tcg_init(void)
{
    pthread_once(&initLLVMOnce, initLLVM);
    g_state = new CompilerState("qemu", g_desc);
    g_output = new Output(*g_state);
}

static void llvm_tcg_deinit(void)
{
    delete g_output;
    g_output = nullptr;
    delete g_state;
    g_state = nullptr;
    g_labelMap.clear();
    clearTcgBuffer();
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

void translate(CPUARMState* env, const TranslateDesc& desc, void** buffer, size_t* s)
{
    llvm_tcg_init();
    ARMCPU* cpu = arm_env_get_cpu(env);
    target_ulong pc;
    uint64_t flags;
    cpu_get_tb_cpu_state(env, &pc, &flags);
    TranslationBlock tb = { pc, flags };

    arm_translate_init();
    gen_intermediate_code_internal(cpu, &tb);
    dumpModule(g_state->m_module);
    compile(*g_state);
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
    link(*g_state, linkDesc);
    const void* codeBuffer = g_state->m_codeSectionList.front().data();
    size_t codeSize = g_state->m_codeSectionList.front().size();
    *buffer = malloc(codeSize);
    *s = codeSize;
    memcpy(*buffer, codeBuffer, codeSize);
    llvm_tcg_deinit();
}
}

using namespace jit;

template <typename TCGType>
static TCGType wrap(LValue v)
{
    TCGType ret = allocateTcg<TCGType>();
    ret->m_value = v;
    ret->m_isMem = false;
    return ret;
}

template <typename TCGType>
static TCGType wrapMem(LValue v)
{
    TCGType ret = allocateTcg<TCGType>();
    ret->m_value = v;
    ret->m_isMem = true;
    return ret;
}

template <typename TCGType>
static LValue unwrap(TCGType v)
{
    EMASSERT(v->m_value != nullptr);
    if (v->m_isMem) {
        return g_output->buildLoad(v->m_value);
    }
    else {
        return v->m_value;
    }
}

template <typename TCGType>
static void storeToTCG(LValue v, TCGType ret)
{
    if (!ret->m_isMem) {
        ret->m_value = v;
    }
    else {
        EMASSERT(ret->m_value != nullptr);
        g_output->buildStore(v, ret->m_value);
    }
}

static void extract_64_32(LValue my64, TCGv_i32 rl, TCGv_i32 rh)
{
    LValue thirtytwo = g_output->repo().int32ThirtyTwo;
    LValue negativeOne = g_output->repo().int32NegativeOne;
    LValue rhUnwrap = g_output->buildCast(LLVMTrunc, g_output->buildLShr(my64, thirtytwo), g_output->repo().int32);
    LValue rlUnwrap = g_output->buildCast(LLVMTrunc, my64, g_output->repo().int32);
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

static LValue tcgPointerToLLVM(TCGMemOp op, TCGv pointer)
{
    int opInt = op;
    opInt &= ~MO_SIGN;
    LValue pointerBeforeCast = unwrap(pointer);
    EMASSERT(jit::typeOf(pointerBeforeCast) != g_output->repo().ref32);

    switch (op) {
    case MO_8:
        return g_output->buildCast(LLVMIntToPtr, pointerBeforeCast, g_output->repo().ref8);
    case MO_16:
        return g_output->buildCast(LLVMIntToPtr, pointerBeforeCast, g_output->repo().ref16);
    case MO_32:
        return g_output->buildCast(LLVMIntToPtr, pointerBeforeCast, g_output->repo().ref32);
    case MO_64:
        return g_output->buildCast(LLVMIntToPtr, pointerBeforeCast, g_output->repo().ref64);
    default:
        EMASSERT("unknown pointer type." && false);
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
        EMASSERT("unknown pointer type." && false);
    }
}

TCGv_i64 tcg_global_mem_new_i64(int, intptr_t offset, const char* name)
{
    LValue v = g_output->buildArgGEP(offset / sizeof(target_ulong));
    LValue v2 = g_output->buildPointerCast(v, g_output->repo().ref64);

    return wrapMem<TCGv_i64>(v2);
}

TCGv_i32 tcg_global_mem_new_i32(int, intptr_t offset, const char* name)
{
    LValue v = g_output->buildArgGEP(offset / sizeof(target_ulong));
    LValue v2 = g_output->buildPointerCast(v, g_output->repo().ref32);

    return wrapMem<TCGv_i32>(v2);
}

TCGv_ptr tcg_global_reg_new_ptr(int, const char* name)
{
    LValue v = g_output->buildArgGEP(0);
    return wrap<TCGv_ptr>(v);
}

TCGv_i32 tcg_const_i32(int32_t val)
{
    LValue v = g_output->constInt32(val);
    return wrap<TCGv_i32>(v);
}

TCGv_ptr tcg_const_ptr(const void* val)
{
    LValue v = g_output->constIntPtr(reinterpret_cast<uintptr_t>(val));
    v = g_output->buildCast(LLVMIntToPtr, v, g_output->repo().ref8);
    return wrap<TCGv_ptr>(v);
}

TCGv_i64 tcg_const_i64(int64_t val)
{
    return wrap<TCGv_i64>(g_output->constInt64(val));
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
    LValue t0 = g_output->buildCast(LLVMZExt, unwrap(al), g_output->repo().int64);
    LValue t1 = g_output->buildCast(LLVMZExt, unwrap(ah), g_output->repo().int64);
    LValue t2 = g_output->buildCast(LLVMZExt, unwrap(bl), g_output->repo().int64);
    LValue t3 = g_output->buildCast(LLVMZExt, unwrap(bh), g_output->repo().int64);
    LValue thirtytwo = g_output->repo().int32ThirtyTwo;

    LValue t01 = g_output->buildShl(t1, thirtytwo);
    t01 = g_output->buildOr(t01, t0);
    LValue t23 = g_output->buildShl(t3, thirtytwo);
    t23 = g_output->buildOr(t23, t2);
    LValue t0123 = g_output->buildAdd(t01, t23);

    extract_64_32(t0123, rl, rh);
}

void tcg_gen_add_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue v = g_output->buildAdd(unwrap(arg1), unwrap(arg2));
    storeToTCG(v, ret);
}

void tcg_gen_add_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    LValue v = g_output->buildAdd(unwrap(arg1), unwrap(arg2));
    storeToTCG(v, ret);
}

void tcg_gen_addi_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue v;
    if (arg2 != 0) {
        v = g_output->buildAdd(unwrap(arg1), g_output->constInt32(arg2));
    }
    else {
        v = unwrap(arg1);
    }
    storeToTCG(v, ret);
}

void tcg_gen_addi_ptr(TCGv_ptr ret, TCGv_ptr arg1, int32_t arg2)
{
    LValue constant = g_output->constInt32(arg2);
    LValue arg1V = unwrap(arg1);
    arg1V = g_output->buildCast(LLVMPtrToInt, arg1V, g_output->repo().intPtr);
    LValue retVal = g_output->buildAdd(arg1V, constant);
    storeToTCG(g_output->buildCast(LLVMIntToPtr, retVal, g_output->repo().ref8), ret);
}

void tcg_gen_addi_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    LValue v;
    if (arg2 != 0) {
        v = g_output->buildAdd(unwrap(arg1), g_output->constInt64(arg2));
    }
    else {
        v = unwrap(arg1);
    }
    storeToTCG(v, ret);
}

void tcg_gen_andc_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue t0 = g_output->buildNot(unwrap(arg2));
    LValue v = g_output->buildAnd(unwrap(arg1), t0);
    storeToTCG(v, ret);
}

void tcg_gen_and_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue v = g_output->buildAnd(unwrap(arg1), unwrap(arg2));
    storeToTCG(v, ret);
}

void tcg_gen_and_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    LValue v = g_output->buildAnd(unwrap(arg1), unwrap(arg2));
    storeToTCG(v, ret);
}

void tcg_gen_andi_i32(TCGv_i32 ret, TCGv_i32 arg1, uint32_t arg2)
{
    LValue v = g_output->buildAnd(unwrap(arg1), g_output->constInt32(arg2));
    storeToTCG(v, ret);
}

void tcg_gen_andi_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    LValue v = g_output->buildAnd(unwrap(arg1), g_output->constInt64(arg2));
    storeToTCG(v, ret);
}

void tcg_gen_brcondi_i32(TCGCond cond, TCGv_i32 arg1,
    int32_t arg2, int label_index)
{
    LBasicBlock taken = labelToBB(label_index);
    LBasicBlock nottaken = g_output->appendBasicBlock("notTaken");
    LValue v1 = unwrap(arg1);
    LValue v2 = g_output->constInt32(arg2);
    LValue condVal = g_output->buildICmp(tcgCondToLLVM(cond), v1, v2);
    g_output->buildCondBr(condVal, taken, nottaken);
    g_output->positionToBBEnd(nottaken);
}

void tcg_gen_bswap16_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue v = unwrap(arg);
    LValue lower = g_output->buildAnd(v, g_output->repo().int32TwoFiveFive);
    LValue higher = g_output->buildShl(v, g_output->repo().int32Eight);
    LValue valret = g_output->buildOr(higher, lower);
    storeToTCG(valret, ret);
}

void tcg_gen_bswap32_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue v = unwrap(arg);
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
    LValue lo = unwrap(low);
    LValue hi = unwrap(high);
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
        v = g_output->buildAnd(unwrap(arg2), g_output->constInt32(mask));
        v = g_output->buildShl(v, g_output->constInt32(ofs));
    }
    else {
        v = g_output->buildShl(unwrap(arg2), g_output->constInt32(ofs));
    }
    LValue retVal = g_output->buildAnd(unwrap(arg1), g_output->constInt32(~(mask << ofs)));
    retVal = g_output->buildOr(retVal, v);
    storeToTCG(retVal, ret);
}

void tcg_gen_mov_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    if (arg == ret)
        return;
    storeToTCG(unwrap(arg), ret);
}

void tcg_gen_mov_i64(TCGv_i64 ret, TCGv_i64 arg)
{
    if (ret == arg)
        return;
    storeToTCG(unwrap(arg), ret);
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
    LValue retVal = g_output->buildShl(unwrap(arg), g_output->repo().int32Sixteen);
    retVal = g_output->buildAShr(retVal, g_output->repo().int32Sixteen);
    storeToTCG(retVal, ret);
}

void tcg_gen_ext16u_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue retVal = g_output->buildAnd(unwrap(arg), g_output->constInt32(0xffff));
    storeToTCG(retVal, ret);
}

void tcg_gen_ext32u_i64(TCGv_i64 ret, TCGv_i64 arg)
{
    LValue retVal = g_output->buildAnd(unwrap(arg), g_output->constInt64(0xffffffffu));
    storeToTCG(retVal, ret);
}

void tcg_gen_ext8s_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue constant = g_output->constInt32(24);
    LValue retVal = g_output->buildShl(unwrap(arg), constant);
    retVal = g_output->buildAShr(retVal, constant);
    storeToTCG(retVal, ret);
}

void tcg_gen_ext8u_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue retVal = g_output->buildAnd(unwrap(arg), g_output->constInt32(0xff));
    storeToTCG(retVal, ret);
}

void tcg_gen_ext_i32_i64(TCGv_i64 ret, TCGv_i32 arg)
{
    LValue retVal = g_output->buildCast(LLVMSExt, unwrap(arg), g_output->repo().int64);
    storeToTCG(retVal, ret);
}

void tcg_gen_extu_i32_i64(TCGv_i64 ret, TCGv_i32 arg)
{
    LValue retVal = g_output->buildCast(LLVMZExt, unwrap(arg), g_output->repo().int64);
    storeToTCG(retVal, ret);
}

void tcg_gen_ld_i32(TCGv_i32 ret, TCGv_ptr arg2, tcg_target_long offset)
{
    LValue pointer = unwrap(arg2);
    pointer = g_output->buildPointerCast(pointer, g_output->repo().ref8);
    pointer = g_output->buildGEP(pointer, offset);
    pointer = g_output->buildPointerCast(pointer, g_output->repo().ref32);
    LValue retVal = g_output->buildLoad(pointer);
    storeToTCG(retVal, ret);
}

void tcg_gen_ld_i64(TCGv_i64 ret, TCGv_ptr arg2,
    tcg_target_long offset)
{
    LValue pointer = unwrap(arg2);
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
        t0 = g_output->buildICmp(tcgCondToLLVM(cond), unwrap(c1), unwrap(c2));
    }

    LValue retVal = g_output->buildSelect(t0, unwrap(v1), unwrap(v2));
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
        t0 = g_output->buildICmp(tcgCondToLLVM(cond), unwrap(c1), unwrap(c2));
    }

    LValue retVal = g_output->buildSelect(t0, unwrap(v1), unwrap(v2));
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
    LValue retVal = g_output->buildMul(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_muls2_i32(TCGv_i32 rl, TCGv_i32 rh,
    TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue t0 = g_output->buildCast(LLVMSExt, unwrap(arg1), g_output->repo().int64);
    LValue t1 = g_output->buildCast(LLVMSExt, unwrap(arg2), g_output->repo().int64);
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
    LValue t0 = g_output->buildCast(LLVMZExt, unwrap(arg1), g_output->repo().int64);
    LValue t1 = g_output->buildCast(LLVMZExt, unwrap(arg2), g_output->repo().int64);
    LValue t3 = g_output->buildMul(t0, t1);
    LValue low = g_output->buildCast(LLVMTrunc, t3, g_output->repo().int32);
    LValue high = g_output->buildLShr(t3, g_output->repo().int32ThirtyTwo);
    high = g_output->buildCast(LLVMTrunc, high, g_output->repo().int32);
    storeToTCG(low, rl);
    storeToTCG(high, rh);
}

void tcg_gen_neg_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue retVal = g_output->buildNeg(unwrap(arg));
    storeToTCG(retVal, ret);
}

void tcg_gen_neg_i64(TCGv_i64 ret, TCGv_i64 arg)
{
    LValue retVal = g_output->buildNeg(unwrap(arg));
    storeToTCG(retVal, ret);
}

void tcg_gen_not_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    LValue retVal = g_output->buildNot(unwrap(arg));
    storeToTCG(retVal, ret);
}

void tcg_gen_orc_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue t0 = g_output->buildNot(unwrap(arg2));
    LValue retVal = g_output->buildOr(unwrap(arg1), t0);
    storeToTCG(retVal, ret);
}

void tcg_gen_or_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue retVal = g_output->buildOr(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_or_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    LValue retVal = g_output->buildOr(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_ori_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue retVal = g_output->buildOr(unwrap(arg1), g_output->constInt32(arg2));
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
        EMASSERT("unknown pointer type." && false);
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
        EMASSERT("unknown pointer type." && false);
    }
    storeToTCG(retVal, val);
}

void tcg_gen_qemu_st_i32(TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
    EMASSERT(idx == 0);
    LValue pointer = tcgPointerToLLVM(memop, addr);
    LValue valToStore = unwrap(val);
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
        EMASSERT("unknown memop" && false);
    }
    g_output->buildStore(valToStore, pointer);
}

void tcg_gen_qemu_st_i64(TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
    EMASSERT(idx == 0);
    LValue pointer = tcgPointerToLLVM(memop, addr);
    LValue valToStore = unwrap(val);
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
        EMASSERT("unknown memop" && false);
    }
    g_output->buildStore(valToStore, pointer);
}

void tcg_gen_rotr_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue arg1U = unwrap(arg1);
    LValue arg2U = unwrap(arg2);
    LValue t0 = g_output->buildLShr(arg1U, arg2U);
    LValue t1 = g_output->buildSub(g_output->repo().int32ThirtyTwo, arg2U);
    t1 = g_output->buildShl(arg1U, t1);
    LValue retVal = g_output->buildOr(t0, t1);
    storeToTCG(retVal, ret);
}

void tcg_gen_rotri_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    if (arg2 == 0) {
        storeToTCG(unwrap(arg1), ret);
    }
    else {
        LValue arg1U = unwrap(arg1);
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
    LValue retVal = g_output->buildAShr(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_sari_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue retVal = g_output->buildAShr(unwrap(arg1), g_output->constInt32(arg2));
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
        LValue comp = g_output->buildICmp(condLLVM, unwrap(arg1), unwrap(arg2));
        LValue retVal = g_output->buildCast(LLVMZExt, comp, g_output->repo().int32);
        storeToTCG(retVal, ret);
    }
}

void tcg_gen_shl_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue retVal = g_output->buildShl(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_shli_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue retVal = g_output->buildShl(unwrap(arg1), g_output->constInt32(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_shli_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    LValue retVal = g_output->buildShl(unwrap(arg1), g_output->constInt64(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_shr_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue retVal = g_output->buildLShr(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_shri_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue retVal = g_output->buildLShr(unwrap(arg1), g_output->constInt32(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_shri_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    LValue retVal = g_output->buildLShr(unwrap(arg1), g_output->constInt64(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_st_i32(TCGv_i32 arg1, TCGv_ptr arg2, tcg_target_long offset)
{
    LValue pointer = g_output->buildPointerCast(unwrap(arg2), g_output->repo().ref8);
    pointer = g_output->buildGEP(pointer, offset);
    pointer = g_output->buildPointerCast(pointer, g_output->repo().ref32);
    g_output->buildStore(unwrap(arg1), pointer);
}

void tcg_gen_st_i64(TCGv_i64 arg1, TCGv_ptr arg2,
    tcg_target_long offset)
{
    LValue pointer = g_output->buildPointerCast(unwrap(arg2), g_output->repo().ref8);
    pointer = g_output->buildGEP(pointer, offset);
    pointer = g_output->buildPointerCast(pointer, g_output->repo().ref64);
    g_output->buildStore(unwrap(arg1), pointer);
}

void tcg_gen_sub_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue retVal = g_output->buildSub(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_sub_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    LValue retVal = g_output->buildSub(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_subi_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue retVal = g_output->buildSub(unwrap(arg1), g_output->constInt32(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_trunc_i64_i32(TCGv_i32 ret, TCGv_i64 arg)
{
    LValue retVal = g_output->buildCast(LLVMTrunc, unwrap(arg), g_output->repo().int32);
    storeToTCG(retVal, ret);
}

void tcg_gen_xor_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    LValue retVal = g_output->buildXor(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_xor_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    LValue retVal = g_output->buildXor(unwrap(arg1), unwrap(arg2));
    storeToTCG(retVal, ret);
}

void tcg_gen_xori_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    LValue retVal = g_output->buildXor(unwrap(arg1), g_output->constInt32(arg2));
    storeToTCG(retVal, ret);
}

TCGv_i32 tcg_temp_local_new_i32(void)
{
    return allocateTcg<TCGv_i32>();
}

TCGv_i32 tcg_temp_new_i32(void)
{
    return tcg_temp_local_new_i32();
}

TCGv_ptr tcg_temp_new_ptr(void)
{
    return allocateTcg<TCGv_ptr>();
}

TCGv_i64 tcg_temp_new_i64(void)
{
    return allocateTcg<TCGv_i64>();
}

static void myhandleCallRet64(void* func, TCGArg ret,
    int nargs, TCGArg* args)
{
    // function retval other parameters
    LValue argsV[2 + nargs];
    for (int i = 2; i < 2 + nargs; ++i) {
        argsV[i] = unwrap(reinterpret_cast<TCGCommonStruct*>(args[i - 2]));
    }
    LValue retVal = g_output->buildAlloca(g_output->repo().int64);
    argsV[0] = g_output->constIntPtr(reinterpret_cast<uintptr_t>(func));
    argsV[1] = retVal;
    void* trampoline;
    switch (nargs) {
    case 0:
        trampoline = reinterpret_cast<void*>(trampolineForHelper32Ret64_0);
        break;
    case 1:
        trampoline = reinterpret_cast<void*>(trampolineForHelper32Ret64_1);
        break;
    case 2:
        trampoline = reinterpret_cast<void*>(trampolineForHelper32Ret64_2);
        break;
    case 3:
        trampoline = reinterpret_cast<void*>(trampolineForHelper32Ret64_3);
        break;
    case 4:
        trampoline = reinterpret_cast<void*>(trampolineForHelper32Ret64_4);
        break;
    case 5:
        trampoline = reinterpret_cast<void*>(trampolineForHelper32Ret64_5);
        break;
    default:
        EMASSERT("unsupported arg number." && false);
    }
    g_output->buildTcgHelperCall(trampoline, nargs + 2, argsV);
    storeToTCG(g_output->buildLoad(retVal), reinterpret_cast<TCGv_ptr>(ret));
}

static void myhandleCallRet32(void* func, TCGArg ret,
    int nargs, TCGArg* args)
{
    // function retval other parameters
    LValue argsV[2 + nargs];
    for (int i = 2; i < 2 + nargs; ++i) {
        argsV[i] = unwrap(reinterpret_cast<TCGCommonStruct*>(args[i - 2]));
    }
    LValue retVal = g_output->buildAlloca(g_output->repo().int32);
    argsV[0] = g_output->constIntPtr(reinterpret_cast<uintptr_t>(func));
    argsV[1] = retVal;
    void* trampoline;
    switch (nargs) {
    case 0:
        trampoline = reinterpret_cast<void*>(trampolineForHelper32Ret32_0);
        break;
    case 1:
        trampoline = reinterpret_cast<void*>(trampolineForHelper32Ret32_1);
        break;
    case 2:
        trampoline = reinterpret_cast<void*>(trampolineForHelper32Ret32_2);
        break;
    case 3:
        trampoline = reinterpret_cast<void*>(trampolineForHelper32Ret32_3);
        break;
    case 4:
        trampoline = reinterpret_cast<void*>(trampolineForHelper32Ret32_4);
        break;
    case 5:
        trampoline = reinterpret_cast<void*>(trampolineForHelper32Ret32_5);
        break;
    default:
        EMASSERT("unsupported arg number." && false);
    }
    g_output->buildTcgHelperCall(trampoline, nargs + 2, argsV);
    storeToTCG(g_output->buildLoad(retVal), reinterpret_cast<TCGv_ptr>(ret));
}

static void myhandleCallRetNone(void* func, int nargs, TCGArg* args)
{
    LValue argsV[nargs];
    for (int i = 0; i < nargs; ++i) {
        argsV[i] = unwrap(reinterpret_cast<TCGCommonStruct*>(args[i]));
    }
    g_output->buildTcgHelperCall(func, nargs, argsV);
}

void tcg_gen_callN(void*, void* func, TCGArg ret,
    int nargs, TCGArg* args)
{
    if (ret != TCG_CALL_DUMMY_ARG) {
        if (reinterpret_cast<TCGv_ptr>(ret)->m_size == 64) {
            myhandleCallRet64(func, ret, nargs, args);
        }
        else if (reinterpret_cast<TCGv_ptr>(ret)->m_size == 32) {
            myhandleCallRet32(func, ret, nargs, args);
        }
        else {
            EMASSERT("ret can only be 32/64" && false);
        }
    }
    else {
        myhandleCallRetNone(func, nargs, args);
    }
}
