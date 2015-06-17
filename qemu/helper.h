#ifndef HELPER_H
#define HELPER_H
#include <stdio.h>
#include <stddef.h>
#include "cpu.h"
#include "tcg_functions.h"
#ifdef __cplusplus
extern "C" {
#endif
#ifdef __x86_64__
#define FASTCALL
#else
#define FASTCALL __attribute__((fastcall))
#endif //__x86_64__
#define DEF_HELPER_3(fun_name, ret, param1, param2, param3)                                      \
    ret helper_##fun_name(CPUARMState* env, void* p2, void* p3) FASTCALL;                        \
    static inline ret gen_helper_##fun_name(CPUARMState* p1, TCGv_##param2 p2, TCGv_##param3 p3) \
    {                                                                                            \
        tcg_gen_helper_3((void*)helper_##fun_name, p2, p3);                                      \
    }

DEF_HELPER_3(access_check_cp_reg, void, env, ptr, i32)

#ifdef __cplusplus
}
#endif
#endif /* HELPER_H */
