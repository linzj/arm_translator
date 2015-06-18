#include "cpu.h"

#define REGINFO_SENTINEL { .type = ARM_CP_SENTINEL }

static inline void set_feature(CPUARMState *env, int feature)
{
    env->features |= 1ULL << feature;
}

static const ARMCPRegInfo cortexa15_cp_reginfo[] = {
    { .name = "L2ECTLR", .cp = 15, .crn = 9, .crm = 0, .opc1 = 1, .opc2 = 3,
      .access = PL1_RW, .type = ARM_CP_CONST, .resetvalue = 0 },
    REGINFO_SENTINEL
};

void cortex_a15_initfn(ARMCPU* cpu)
{
    set_feature(&cpu->env, ARM_FEATURE_V7);
    set_feature(&cpu->env, ARM_FEATURE_VFP4);
    set_feature(&cpu->env, ARM_FEATURE_NEON);
    set_feature(&cpu->env, ARM_FEATURE_THUMB2EE);
    set_feature(&cpu->env, ARM_FEATURE_ARM_DIV);
    set_feature(&cpu->env, ARM_FEATURE_GENERIC_TIMER);
    set_feature(&cpu->env, ARM_FEATURE_DUMMY_C15_REGS);
    set_feature(&cpu->env, ARM_FEATURE_CBAR_RO);
    set_feature(&cpu->env, ARM_FEATURE_LPAE);
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
}
