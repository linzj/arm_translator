#ifndef CPUINIT_H
#define CPUINIT_H
#include "cpu.h"
#ifdef __cplusplus
extern "C" {
#endif
void cortex_a15_initfn(ARMCPU* cpu);
void cortex_a15_deinitfn(ARMCPU* cpu);
#ifdef __cplusplus
}
#endif
#endif /* CPUINIT_H */
