#ifndef TCG_FUNCTIONS_H
#define TCG_FUNCTIONS_H
#include "tgtypes.h"
#ifdef __cplusplus
extern "C" {
#endif

TCGv_i64 tcg_global_mem_new_i64(int reg, intptr_t offset, const char* name);
TCGv_i32 tcg_const_i32(int32_t val);
TCGv_i64 tcg_const_i64(int64_t val);
#define TCGV_NAT_TO_PTR(a) ((TCGv_ptr)a)
#define tcg_const_ptr(V) TCGV_NAT_TO_PTR(tcg_const_i32((intptr_t)(V)))

void tcg_gen_add2_i32(TCGv_i32 rl, TCGv_i32 rh, TCGv_i32 al,
    TCGv_i32 ah, TCGv_i32 bl, TCGv_i32 bh);

void tcg_gen_add_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);

#ifdef __cplusplus
}
#endif
#endif /* TCG_FUNCTIONS_H */
