#include <string.h>
#include "cpuinit.h"
#include "compatglib.h"

#define REGINFO_SENTINEL        \
    {                           \
        .type = ARM_CP_SENTINEL \
    }

static inline void set_feature(CPUARMState* env, int feature)
{
    env->features |= 1ULL << feature;
}

static const ARMCPRegInfo cortexa15_cp_reginfo[] = {
    {.name = "L2ECTLR", .cp = 15, .crn = 9, .crm = 0, .opc1 = 1, .opc2 = 3, .access = PL1_RW, .type = ARM_CP_CONST, .resetvalue = 0 },
    REGINFO_SENTINEL
};

static void arm_cpu_reset(CPUState* s);

void cortex_a15_initfn(ARMCPU* cpu)
{
    cpu->cp_regs = g_hash_table_new_full(g_int_hash, g_int_equal,
        g_free, g_free);
    set_feature(&cpu->env, ARM_FEATURE_V4T);
    set_feature(&cpu->env, ARM_FEATURE_V5);
    set_feature(&cpu->env, ARM_FEATURE_V6);
    set_feature(&cpu->env, ARM_FEATURE_THUMB2);
    set_feature(&cpu->env, ARM_FEATURE_V7);
    set_feature(&cpu->env, ARM_FEATURE_VFP4);
    set_feature(&cpu->env, ARM_FEATURE_VFP3);
    set_feature(&cpu->env, ARM_FEATURE_VFP);
    set_feature(&cpu->env, ARM_FEATURE_VFP_FP16);
    set_feature(&cpu->env, ARM_FEATURE_NEON);
    set_feature(&cpu->env, ARM_FEATURE_THUMB2EE);
    set_feature(&cpu->env, ARM_FEATURE_ARM_DIV);
    set_feature(&cpu->env, ARM_FEATURE_GENERIC_TIMER);
    set_feature(&cpu->env, ARM_FEATURE_DUMMY_C15_REGS);
    set_feature(&cpu->env, ARM_FEATURE_CBAR_RO);
    set_feature(&cpu->env, ARM_FEATURE_LPAE);
    set_feature(&cpu->env, ARM_FEATURE_V8);
    cpu->midr = 0x412fc0f1;
    cpu->reset_fpsid = 0x410430f0;
    cpu->mvfr0 = 0x10110222;
    cpu->mvfr1 = 0x11111111;
    cpu->ctr = 0x8444c004;
    cpu->reset_sctlr = 0x00c50078;
    cpu->id_pfr0 = 0x00001131;
    cpu->id_pfr1 = 0x00011011;
    cpu->id_dfr0 = 0x02010555;
    cpu->id_afr0 = 0x00000000;
    cpu->id_mmfr0 = 0x10201105;
    cpu->id_mmfr1 = 0x20000000;
    cpu->id_mmfr2 = 0x01240000;
    cpu->id_mmfr3 = 0x02102211;
    cpu->id_isar0 = 0x02101110;
    cpu->id_isar1 = 0x13112111;
    cpu->id_isar2 = 0x21232041;
    cpu->id_isar3 = 0x11112131;
    cpu->id_isar4 = 0x10011142;
    cpu->dbgdidr = 0x3515f021;
    cpu->clidr = 0x0a200023;
    cpu->ccsidr[0] = 0x701fe00a; /* 32K L1 dcache */
    cpu->ccsidr[1] = 0x201fe00a; /* 32K L1 icache */
    cpu->ccsidr[2] = 0x711fe07a; /* 4096K L2 unified cache */
    define_arm_cp_regs(cpu, cortexa15_cp_reginfo);

    arm_cpu_reset(&cpu->state);
}

static void cp_reg_reset(gpointer key, gpointer value, gpointer opaque)
{
    /* Reset a single ARMCPRegInfo register */
    ARMCPRegInfo* ri = value;
    ARMCPU* cpu = opaque;

    if (ri->type & ARM_CP_SPECIAL) {
        return;
    }

    if (ri->resetfn) {
        ri->resetfn(&cpu->env, ri);
        return;
    }

    /* A zero offset is never possible as it would be regs[0]
     * so we use it to indicate that reset is being handled elsewhere.
     * This is basically only used for fields in non-core coprocessors
     * (like the pxa2xx ones).
     */
    if (!ri->fieldoffset) {
        return;
    }

    if (cpreg_field_is_64bit(ri)) {
        CPREG_FIELD64(&cpu->env, ri) = ri->resetvalue;
    }
    else {
        CPREG_FIELD32(&cpu->env, ri) = ri->resetvalue;
    }
}

void arm_cpu_reset(CPUState* s)
{
    ARMCPU* cpu = ARM_CPU(s);
    CPUARMState* env = &cpu->env;

    memset(env, 0, offsetof(CPUARMState, features));
    g_hash_table_foreach(cpu->cp_regs, cp_reg_reset, cpu);
    env->vfp.xregs[ARM_VFP_FPSID] = cpu->reset_fpsid;
    env->vfp.xregs[ARM_VFP_MVFR0] = cpu->mvfr0;
    env->vfp.xregs[ARM_VFP_MVFR1] = cpu->mvfr1;
    env->vfp.xregs[ARM_VFP_MVFR2] = cpu->mvfr2;

    if (arm_feature(env, ARM_FEATURE_IWMMXT)) {
        env->iwmmxt.cregs[ARM_IWMMXT_wCID] = 0x69051000 | 'Q';
    }

    if (arm_feature(env, ARM_FEATURE_AARCH64)) {
        /* 64 bit CPUs always start in 64 bit mode */
        env->aarch64 = 1;
        env->pstate = PSTATE_MODE_EL0t;
        /* Userspace expects access to DC ZVA, CTL_EL0 and the cache ops */
        env->cp15.c1_sys |= SCTLR_UCT | SCTLR_UCI | SCTLR_DZE;
        /* and to the FP/Neon instructions */
        env->cp15.c1_coproc = deposit64(env->cp15.c1_coproc, 20, 2, 3);
    }
    else {
        /* Userspace expects access to cp10 and cp11 for FP/Neon */
        env->cp15.c1_coproc = deposit64(env->cp15.c1_coproc, 20, 4, 0xf);
    }

    env->uncached_cpsr = ARM_CPU_MODE_USR;
    /* For user mode we must enable access to coprocessors */
    env->vfp.xregs[ARM_VFP_FPEXC] = 1 << 30;
    if (arm_feature(env, ARM_FEATURE_IWMMXT)) {
        env->cp15.c15_cpar = 3;
    }
    else if (arm_feature(env, ARM_FEATURE_XSCALE)) {
        env->cp15.c15_cpar = 1;
    }
    set_flush_to_zero(1, &env->vfp.standard_fp_status);
    set_flush_inputs_to_zero(1, &env->vfp.standard_fp_status);
    set_default_nan_mode(1, &env->vfp.standard_fp_status);
    set_float_detect_tininess(float_tininess_before_rounding,
        &env->vfp.fp_status);
    set_float_detect_tininess(float_tininess_before_rounding,
        &env->vfp.standard_fp_status);
}
