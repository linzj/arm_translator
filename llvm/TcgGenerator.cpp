#include <unordered_map>
#include <vector>
#include <memory>
#include <pthread.h>
#include "Registers.h"
#include "TcgGenerator.h"
#include "CompilerState.h"
#include "IntrinsicRepository.h"
#include "InitializeLLVM.h"
#include "LLVMDisasContext.h"
#include "QEMUDisasContext.h"
#include "X86Assembler.h"
#include "Output.h"
#include "cpu.h"
#include "tb.h"
#include "translate.h"
#include "log.h"
#include "DisasContextBase.h"

using namespace jit;
namespace {

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
}
namespace jit {

void translate(CPUARMState* env, TranslateDesc& desc)
{
    std::unique_ptr<DisasContextBase> ctxptr;
    if (desc.m_optimal) {
        ctxptr.reset(new jit::LLVMDisasContext(desc.m_executableMemAllocator, desc.m_dispDirect, desc.m_dispIndirect));
    }
    else {
        ctxptr.reset(new qemu::QEMUDisasContext(desc.m_executableMemAllocator, desc.m_dispDirect, desc.m_dispIndirect, reinterpret_cast<void*>(desc.m_dispHot), desc.m_hotObject));
    }
    DisasContextBase& ctx = *ctxptr;
    ARMCPU* cpu = arm_env_get_cpu(env);
    target_ulong pc;
    uint64_t flags;
    cpu_get_tb_cpu_state(env, &pc, &flags);
    TranslationBlock tb = { pc, flags };

    gen_intermediate_code_internal(cpu, &tb, &ctx);
    ctx.compile();
    ctx.link();
    desc.m_guestExtents = tb.size;
}

void patchDirectJump(uintptr_t from, uintptr_t to)
{
    JSC::X86Assembler assembler(reinterpret_cast<char*>(from), 7);
    assembler.movl_i32r(to, JSC::X86Registers::eax);
    assembler.jmp_r(JSC::X86Registers::eax);
}

void unpatchDirectJump(uintptr_t from, uintptr_t to)
{
    JSC::X86Assembler assembler(reinterpret_cast<char*>(from), 7);
    assembler.movl_i32r(to, JSC::X86Registers::eax);
    assembler.call(JSC::X86Registers::eax);
}
}
#ifdef ENABLE_ASAN
extern "C" {
void helper_asan_bad_load(void* addr, int bytes);
void helper_asan_bad_store(void* addr, int bytes);
}

static bool check_bad(void* addr, int bytes)
{
    uintptr_t addri = reinterpret_cast<uintptr_t>(addr);
    if (addri >= 0xffff0000) {
        return false;
    }
    uintptr_t addri_1 = (addri >> 3);
#ifndef __ANDROID__
    addri_1 += (1 << 29);
#endif
    uint32_t sb;
    asm volatile("movzbl (%[addri_1]), %[sb]\n"
                 : [sb] "=r"(sb)
                 : [addri_1] "r"(addri_1));
    if (sb == 0) {
        return false;
    }
    addri &= 7;
    addri += bytes - 1;
    if (sb > addri) {
        return false;
    }
    return true;
}
void helper_asan_bad_load(void* addr, int bytes)
{
    if (check_bad(addr, bytes))
        LOGE("%s: bad addr: %p, bytes: %d.\n", __FUNCTION__, addr, bytes);
}

void helper_asan_bad_store(void* addr, int bytes)
{
    if (check_bad(addr, bytes))
        LOGE("%s: bad addr: %p, bytes: %d.\n", __FUNCTION__, addr, bytes);
}

static void check_mem(DisasContext* s, TCGv addr, tcg_target_long offset, int bytes, bool isload)
{
    // call bad helpers
    void* func = isload ? reinterpret_cast<void*>(helper_asan_bad_load) : reinterpret_cast<void*>(helper_asan_bad_store);
    TCGv_i32 tcgBytes = tcg_const_i32(s, bytes);
    TCGv_i32 addr2 = tcg_temp_new_i32(s);
    tcg_gen_mov_i32(s, addr2, addr);
    tcg_gen_addi_i32(s, addr2, addr2, offset);
    TCGArg args[] = { reinterpret_cast<TCGArg>(addr2), reinterpret_cast<TCGArg>(tcgBytes) };
    static_cast<DisasContextBase*>(s)->gen_callN(func, reinterpret_cast<TCGArg>(-1), 2, args);
    tcg_temp_free_i32(s, addr2);
}
#endif //ENABLE_ASAN

int gen_new_label(DisasContext* s)
{
    return static_cast<DisasContextBase*>(s)->gen_new_label();
}

void gen_set_label(DisasContext* s, int n)
{
    static_cast<DisasContextBase*>(s)->gen_set_label(n);
}

TCGv_i64 tcg_global_mem_new_i64(DisasContext* s, int reg, intptr_t offset, const char* name)
{
    return static_cast<DisasContextBase*>(s)->global_mem_new_i64(reg, offset, name);
}

TCGv_i32 tcg_global_mem_new_i32(DisasContext* s, int reg, intptr_t offset, const char* name)
{
    return static_cast<DisasContextBase*>(s)->global_mem_new_i32(reg, offset, name);
}

TCGv_ptr tcg_global_reg_new_ptr(DisasContext* s, int reg, const char* name)
{
    return static_cast<DisasContextBase*>(s)->global_reg_new_ptr(reg, name);
}

TCGv_i32 tcg_const_i32(DisasContext* s, int32_t val)
{
    return static_cast<DisasContextBase*>(s)->const_i32(val);
}

TCGv_ptr tcg_const_ptr(DisasContext* s, const void* val)
{
    return static_cast<DisasContextBase*>(s)->const_ptr(val);
}

TCGv_i64 tcg_const_i64(DisasContext* s, int64_t val)
{
    return static_cast<DisasContextBase*>(s)->const_i64(val);
}

void tcg_gen_add2_i32(DisasContext* s, TCGv_i32 rl, TCGv_i32 rh, TCGv_i32 al,
    TCGv_i32 ah, TCGv_i32 bl, TCGv_i32 bh)
{
    static_cast<DisasContextBase*>(s)->gen_add2_i32(rl, rh, al,
        ah, bl, bh);
}

void tcg_gen_add_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_add_i32(ret, arg1, arg2);
}

void tcg_gen_add_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_add_i64(ret, arg1, arg2);
}

void tcg_gen_addi_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    static_cast<DisasContextBase*>(s)->gen_addi_i32(ret, arg1, arg2);
}

void tcg_gen_addi_ptr(DisasContext* s, TCGv_ptr ret, TCGv_ptr arg1, int32_t arg2)
{
    static_cast<DisasContextBase*>(s)->gen_addi_ptr(ret, arg1, arg2);
}

void tcg_gen_addi_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    static_cast<DisasContextBase*>(s)->gen_addi_i64(ret, arg1, arg2);
}

void tcg_gen_andc_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_andc_i32(ret, arg1, arg2);
}

void tcg_gen_and_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_and_i32(ret, arg1, arg2);
}

void tcg_gen_and_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_and_i64(ret, arg1, arg2);
}

void tcg_gen_andi_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, uint32_t arg2)
{
    static_cast<DisasContextBase*>(s)->gen_andi_i32(ret, arg1, arg2);
}

void tcg_gen_andi_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    static_cast<DisasContextBase*>(s)->gen_andi_i64(ret, arg1, arg2);
}

void tcg_gen_brcondi_i32(DisasContext* s, TCGCond cond, TCGv_i32 arg1,
    int32_t arg2, int label_index)
{
    static_cast<DisasContextBase*>(s)->gen_brcondi_i32(cond, arg1,
        arg2, label_index);
}

void tcg_gen_bswap16_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg)
{
    static_cast<DisasContextBase*>(s)->gen_bswap16_i32(ret, arg);
}

void tcg_gen_bswap32_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg)
{
    static_cast<DisasContextBase*>(s)->gen_bswap32_i32(ret, arg);
}

void tcg_gen_concat_i32_i64(DisasContext* s, TCGv_i64 dest, TCGv_i32 low,
    TCGv_i32 high)
{
    static_cast<DisasContextBase*>(s)->gen_concat_i32_i64(dest, low,
        high);
}

void tcg_gen_deposit_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1,
    TCGv_i32 arg2, unsigned int ofs,
    unsigned int len)
{
    static_cast<DisasContextBase*>(s)->gen_deposit_i32(ret, arg1,
        arg2, ofs,
        len);
}

void tcg_gen_mov_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg)
{
    static_cast<DisasContextBase*>(s)->gen_mov_i32(ret, arg);
}

void tcg_gen_exit_tb(DisasContext* s, int direct)
{
    static_cast<DisasContextBase*>(s)->gen_exit_tb(direct);
}

void tcg_gen_ext16s_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg)
{
    static_cast<DisasContextBase*>(s)->gen_ext16s_i32(ret, arg);
}

void tcg_gen_ext16u_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg)
{
    static_cast<DisasContextBase*>(s)->gen_ext16u_i32(ret, arg);
}

void tcg_gen_ext32u_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg)
{
    static_cast<DisasContextBase*>(s)->gen_ext32u_i64(ret, arg);
}

void tcg_gen_ext8s_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg)
{
    static_cast<DisasContextBase*>(s)->gen_ext8s_i32(ret, arg);
}

void tcg_gen_ext8u_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg)
{
    static_cast<DisasContextBase*>(s)->gen_ext8u_i32(ret, arg);
}

void tcg_gen_ext_i32_i64(DisasContext* s, TCGv_i64 ret, TCGv_i32 arg)
{
    static_cast<DisasContextBase*>(s)->gen_ext_i32_i64(ret, arg);
}

void tcg_gen_extu_i32_i64(DisasContext* s, TCGv_i64 ret, TCGv_i32 arg)
{
    static_cast<DisasContextBase*>(s)->gen_extu_i32_i64(ret, arg);
}

void tcg_gen_ld_i32(DisasContext* s, TCGv_i32 ret, TCGv_ptr arg2, tcg_target_long offset)
{
#ifdef ENABLE_ASAN
    check_mem(s, (TCGv)arg2, offset, 4, true);
#endif
    static_cast<DisasContextBase*>(s)->gen_ld_i32(ret, arg2, offset);
}

void tcg_gen_ld_i64(DisasContext* s, TCGv_i64 ret, TCGv_ptr arg2,
    tcg_target_long offset)
{
#ifdef ENABLE_ASAN
    check_mem(s, (TCGv)arg2, offset, 8, true);
#endif
    static_cast<DisasContextBase*>(s)->gen_ld_i64(ret, arg2,
        offset);
}

void tcg_gen_movcond_i32(DisasContext* s, TCGCond cond, TCGv_i32 ret,
    TCGv_i32 c1, TCGv_i32 c2,
    TCGv_i32 v1, TCGv_i32 v2)
{
    static_cast<DisasContextBase*>(s)->gen_movcond_i32(cond, ret,
        c1, c2,
        v1, v2);
}

void tcg_gen_movcond_i64(DisasContext* s, TCGCond cond, TCGv_i64 ret,
    TCGv_i64 c1, TCGv_i64 c2,
    TCGv_i64 v1, TCGv_i64 v2)
{
    static_cast<DisasContextBase*>(s)->gen_movcond_i64(cond, ret,
        c1, c2,
        v1, v2);
}

void tcg_gen_mov_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg)
{
    static_cast<DisasContextBase*>(s)->gen_mov_i64(ret, arg);
}

void tcg_gen_movi_i32(DisasContext* s, TCGv_i32 ret, int32_t arg)
{
    static_cast<DisasContextBase*>(s)->gen_movi_i32(ret, arg);
}

void tcg_gen_movi_i64(DisasContext* s, TCGv_i64 ret, int64_t arg)
{
    static_cast<DisasContextBase*>(s)->gen_movi_i64(ret, arg);
}

void tcg_gen_mul_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_mul_i32(ret, arg1, arg2);
}

void tcg_gen_muls2_i32(DisasContext* s, TCGv_i32 rl, TCGv_i32 rh,
    TCGv_i32 arg1, TCGv_i32 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_muls2_i32(rl, rh,
        arg1, arg2);
}

void tcg_gen_mulu2_i32(DisasContext* s, TCGv_i32 rl, TCGv_i32 rh,
    TCGv_i32 arg1, TCGv_i32 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_mulu2_i32(rl, rh,
        arg1, arg2);
}

void tcg_gen_neg_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg)
{
    static_cast<DisasContextBase*>(s)->gen_neg_i32(ret, arg);
}

void tcg_gen_neg_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg)
{
    static_cast<DisasContextBase*>(s)->gen_neg_i64(ret, arg);
}

void tcg_gen_not_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg)
{
    static_cast<DisasContextBase*>(s)->gen_not_i32(ret, arg);
}

void tcg_gen_orc_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_orc_i32(ret, arg1, arg2);
}

void tcg_gen_or_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_or_i32(ret, arg1, arg2);
}

void tcg_gen_or_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_or_i64(ret, arg1, arg2);
}

void tcg_gen_ori_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    return static_cast<DisasContextBase*>(s)->gen_ori_i32(ret, arg1, arg2);
}

void tcg_gen_qemu_ld_i32(DisasContext* s, TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
#ifdef ENABLE_ASAN
    int size = static_cast<int>(memop) & static_cast<int>(~MO_SIGN);
    size = 1 << size;
    check_mem(s, addr, 0, size, true);
#endif
    static_cast<DisasContextBase*>(s)->gen_qemu_ld_i32(val, addr, idx, memop);
}

void tcg_gen_qemu_ld_i64(DisasContext* s, TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
#ifdef ENABLE_ASAN
    int size = static_cast<int>(memop) & static_cast<int>(~MO_SIGN);
    size = 1 << size;
    check_mem(s, addr, 0, size, true);
#endif
    static_cast<DisasContextBase*>(s)->gen_qemu_ld_i64(val, addr, idx, memop);
}

void tcg_gen_qemu_st_i32(DisasContext* s, TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
#ifdef ENABLE_ASAN
    int size = static_cast<int>(memop) & static_cast<int>(~MO_SIGN);
    size = 1 << size;
    check_mem(s, addr, 0, size, false);
#endif
    static_cast<DisasContextBase*>(s)->gen_qemu_st_i32(val, addr, idx, memop);
}

void tcg_gen_qemu_st_i64(DisasContext* s, TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
#ifdef ENABLE_ASAN
    int size = static_cast<int>(memop) & static_cast<int>(~MO_SIGN);
    size = 1 << size;
    check_mem(s, addr, 0, size, false);
#endif
    static_cast<DisasContextBase*>(s)->gen_qemu_st_i64(val, addr, idx, memop);
}

void tcg_gen_rotr_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_rotr_i32(ret, arg1, arg2);
}

void tcg_gen_rotri_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    static_cast<DisasContextBase*>(s)->gen_rotri_i32(ret, arg1, arg2);
}

void tcg_gen_sar_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_sar_i32(ret, arg1, arg2);
}

void tcg_gen_sari_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    static_cast<DisasContextBase*>(s)->gen_sari_i32(ret, arg1, arg2);
}

void tcg_gen_setcond_i32(DisasContext* s, TCGCond cond, TCGv_i32 ret,
    TCGv_i32 arg1, TCGv_i32 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_setcond_i32(cond, ret,
        arg1, arg2);
}

void tcg_gen_shl_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_shl_i32(ret, arg1, arg2);
}

void tcg_gen_shli_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    static_cast<DisasContextBase*>(s)->gen_shli_i32(ret, arg1, arg2);
}

void tcg_gen_shli_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    static_cast<DisasContextBase*>(s)->gen_shli_i64(ret, arg1, arg2);
}

void tcg_gen_shr_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_shr_i32(ret, arg1, arg2);
}

void tcg_gen_shri_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    static_cast<DisasContextBase*>(s)->gen_shri_i32(ret, arg1, arg2);
}

void tcg_gen_shri_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    static_cast<DisasContextBase*>(s)->gen_shri_i64(ret, arg1, arg2);
}

void tcg_gen_st_i32(DisasContext* s, TCGv_i32 arg1, TCGv_ptr arg2, tcg_target_long offset)
{
#ifdef ENABLE_ASAN
    check_mem(s, (TCGv)arg2, offset, 4, false);
#endif
    static_cast<DisasContextBase*>(s)->gen_st_i32(arg1, arg2, offset);
}

void tcg_gen_st_i64(DisasContext* s, TCGv_i64 arg1, TCGv_ptr arg2,
    tcg_target_long offset)
{
#ifdef ENABLE_ASAN
    check_mem(s, (TCGv)arg2, offset, 8, false);
#endif
    static_cast<DisasContextBase*>(s)->gen_st_i64(arg1, arg2,
        offset);
}

void tcg_gen_sub_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_sub_i32(ret, arg1, arg2);
}

void tcg_gen_sub_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_sub_i64(ret, arg1, arg2);
}

void tcg_gen_subi_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    static_cast<DisasContextBase*>(s)->gen_subi_i32(ret, arg1, arg2);
}

void tcg_gen_trunc_i64_i32(DisasContext* s, TCGv_i32 ret, TCGv_i64 arg)
{
    static_cast<DisasContextBase*>(s)->gen_trunc_i64_i32(ret, arg);
}

void tcg_gen_xor_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_xor_i32(ret, arg1, arg2);
}

void tcg_gen_xor_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    static_cast<DisasContextBase*>(s)->gen_xor_i64(ret, arg1, arg2);
}

void tcg_gen_xori_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    static_cast<DisasContextBase*>(s)->gen_xori_i32(ret, arg1, arg2);
}

TCGv_i32 tcg_temp_local_new_i32(DisasContext* s)
{
    return static_cast<DisasContextBase*>(s)->temp_local_new_i32();
}

TCGv_i32 tcg_temp_new_i32(DisasContext* s)
{
    return static_cast<DisasContextBase*>(s)->temp_new_i32();
}

TCGv_ptr tcg_temp_new_ptr(DisasContext* s)
{
    return static_cast<DisasContextBase*>(s)->temp_new_ptr();
}

TCGv_i64 tcg_temp_new_i64(DisasContext* s)
{
    return static_cast<DisasContextBase*>(s)->temp_new_i64();
}

void tcg_temp_free_i32(DisasContext* s, TCGv_i32 a)
{
    static_cast<DisasContextBase*>(s)->temp_free_i32(a);
}

void tcg_temp_free_i64(DisasContext* s, TCGv_i64 a)
{
    static_cast<DisasContextBase*>(s)->temp_free_i64(a);
}

void tcg_temp_free_ptr(DisasContext* s, TCGv_ptr ptr)
{
    static_cast<DisasContextBase*>(s)->temp_free_ptr(ptr);
}

void tcg_gen_callN(DisasContext* s, void* func, TCGArg ret,
    int nargs, TCGArg* args)
{
    static_cast<DisasContextBase*>(s)->gen_callN(func, ret,
        nargs, args);
}

void tcg_func_start(DisasContext* s)
{
    static_cast<DisasContextBase*>(s)->func_start();
}

bool tcg_should_continue(DisasContext* s)
{
    return static_cast<DisasContextBase*>(s)->should_continue();
}
