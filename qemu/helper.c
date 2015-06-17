#include "helper-proto.h"
#include "log.h"

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
