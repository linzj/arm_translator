#include "helper-proto.h"
#include "log.h"

#define SIGNBIT (uint32_t)0x80000000
#define SIGNBIT64 ((uint64_t)1 << 63)
static void cpu_loop_exit(CPU* cpu)
{
    longjmp(cpu->exit_buf, 1);
}

static void raise_exception(CPUARMState* env, int tt)
{
    CPU* cpu = arm_env_get_cpu(env);

    cpu->exception_index = tt;
    cpu_loop_exit(cpu);
}

void HELPER(access_check_cp_reg)(CPUARMState* env, void* rip, uint32_t syndrome)
{
    const ARMCPRegInfo* ri = rip;
    if (arm_feature(env, ARM_FEATURE_XSCALE) && ri->cp < 14
        && extract32(env->cp15.c15_cpar, ri->cp, 1) == 0) {
        env->exception.syndrome = syndrome;
        raise_exception(env, EXCP_UDEF);
    }

    if (!ri->accessfn) {
        return;
    }

    switch (ri->accessfn(env, ri)) {
    case CP_ACCESS_OK:
        return;
    case CP_ACCESS_TRAP:
        env->exception.syndrome = syndrome;
        break;
    case CP_ACCESS_TRAP_UNCATEGORIZED:
        env->exception.syndrome = syn_uncategorized();
        break;
    default:
        EMUNREACHABLE();
    }
    raise_exception(env, EXCP_UDEF);
}

uint32_t HELPER(neon_tbl)(CPUARMState* env, uint32_t ireg, uint32_t def,
    uint32_t rn, uint32_t maxindex)
{
    uint32_t val;
    uint32_t tmp;
    int index;
    int shift;
    uint64_t* table;
    table = (uint64_t*)&env->vfp.regs[rn];
    val = 0;
    for (shift = 0; shift < 32; shift += 8) {
        index = (ireg >> shift) & 0xff;
        if (index < maxindex) {
            tmp = (table[index >> 3] >> ((index & 7) << 3)) & 0xff;
            val |= tmp << shift;
        }
        else {
            val |= def & (0xff << shift);
        }
    }
    return val;
}

uint32_t HELPER(add_saturate)(CPUARMState* env, uint32_t a, uint32_t b)
{
    uint32_t res = a + b;
    if (((res ^ a) & SIGNBIT) && !((a ^ b) & SIGNBIT)) {
        env->QF = 1;
        res = ~(((int32_t)a >> 31) ^ SIGNBIT);
    }
    return res;
}

uint32_t HELPER(add_setq)(CPUARMState* env, uint32_t a, uint32_t b)
{
    uint32_t res = a + b;
    if (((res ^ a) & SIGNBIT) && !((a ^ b) & SIGNBIT))
        env->QF = 1;
    return res;
}

uint32_t HELPER(sub_saturate)(CPUARMState* env, uint32_t a, uint32_t b)
{
    uint32_t res = a - b;
    if (((res ^ a) & SIGNBIT) && ((a ^ b) & SIGNBIT)) {
        env->QF = 1;
        res = ~(((int32_t)a >> 31) ^ SIGNBIT);
    }
    return res;
}

uint32_t HELPER(double_saturate)(CPUARMState* env, int32_t val)
{
    uint32_t res;
    if (val >= 0x40000000) {
        res = ~SIGNBIT;
        env->QF = 1;
    }
    else if (val <= (int32_t)0xc0000000) {
        res = SIGNBIT;
        env->QF = 1;
    }
    else {
        res = val << 1;
    }
    return res;
}

uint32_t HELPER(add_usaturate)(CPUARMState* env, uint32_t a, uint32_t b)
{
    uint32_t res = a + b;
    if (res < a) {
        env->QF = 1;
        res = ~0;
    }
    return res;
}
