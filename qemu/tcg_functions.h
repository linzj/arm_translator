#ifndef TCG_FUNCTIONS_H
#define TCG_FUNCTIONS_H
#include "tgtypes.h"
#ifdef __cplusplus
extern "C" {
#endif

int gen_new_label(void);
void gen_set_label(int n);
TCGv_i64 tcg_global_mem_new_i64(int reg, intptr_t offset, const char* name);
TCGv_i32 tcg_const_i32(int32_t val);
TCGv_i64 tcg_const_i64(int64_t val);
#define TCGV_NAT_TO_PTR(a) ((TCGv_ptr)a)
#define tcg_const_ptr(V) TCGV_NAT_TO_PTR(tcg_const_i32((intptr_t)(V)))
#define tcg_gen_addi_ptr(R, A, B) \
    tcg_gen_addi_i32(TCGV_PTR_TO_NAT(R), TCGV_PTR_TO_NAT(A), (B))

void tcg_gen_add2_i32(TCGv_i32 rl, TCGv_i32 rh, TCGv_i32 al,
    TCGv_i32 ah, TCGv_i32 bl, TCGv_i32 bh);

void tcg_gen_add_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_add_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2);
void tcg_gen_addi_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2);
void tcg_gen_addi_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2);
void tcg_gen_andc_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_and_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_and_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2);
void tcg_gen_andi_i32(TCGv_i32 ret, TCGv_i32 arg1, uint32_t arg2);
void tcg_gen_andi_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2);
void tcg_gen_brcondi_i32(TCGCond cond, TCGv_i32 arg1,
    int32_t arg2, int label_index);
void tcg_gen_bswap16_i32(TCGv_i32 ret, TCGv_i32 arg);
void tcg_gen_bswap32_i32(TCGv_i32 ret, TCGv_i32 arg);
void tcg_gen_concat_i32_i64(TCGv_i64 dest, TCGv_i32 low,
    TCGv_i32 high);

void tcg_gen_deposit_i32(TCGv_i32 ret, TCGv_i32 arg1,
    TCGv_i32 arg2, unsigned int ofs,
    unsigned int len);
void tcg_gen_mov_i32(TCGv_i32 ret, TCGv_i32 arg);
void tcg_gen_exit_tb(int direct);
void tcg_gen_ext16s_i32(TCGv_i32 ret, TCGv_i32 arg);
void tcg_gen_ext16u_i32(TCGv_i32 ret, TCGv_i32 arg);
void tcg_gen_ext32u_i64(TCGv_i64 ret, TCGv_i64 arg);
void tcg_gen_ext8s_i32(TCGv_i32 ret, TCGv_i32 arg);
void tcg_gen_ext8u_i32(TCGv_i32 ret, TCGv_i32 arg);
void tcg_gen_ext_i32_i64(TCGv_i64 ret, TCGv_i32 arg);
void tcg_gen_extu_i32_i64(TCGv_i64 ret, TCGv_i32 arg);
void tcg_gen_ld_i32(TCGv_i32 ret, TCGv_ptr arg2, tcg_target_long offset);
void tcg_gen_ld_i64(TCGv_i64 ret, TCGv_ptr arg2,
    tcg_target_long offset);
void tcg_gen_movcond_i32(TCGCond cond, TCGv_i32 ret,
    TCGv_i32 c1, TCGv_i32 c2,
    TCGv_i32 v1, TCGv_i32 v2);
void tcg_gen_movcond_i64(TCGCond cond, TCGv_i64 ret,
    TCGv_i64 c1, TCGv_i64 c2,
    TCGv_i64 v1, TCGv_i64 v2);
void tcg_gen_mov_i64(TCGv_i64 ret, TCGv_i64 arg);
void tcg_gen_movi_i32(TCGv_i32 ret, int32_t arg);
void tcg_gen_movi_i64(TCGv_i64 ret, int64_t arg);
void tcg_gen_mul_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_muls2_i32(TCGv_i32 rl, TCGv_i32 rh,
    TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_mulu2_i32(TCGv_i32 rl, TCGv_i32 rh,
    TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_neg_i32(TCGv_i32 ret, TCGv_i32 arg);
void tcg_gen_neg_i64(TCGv_i64 ret, TCGv_i64 arg);
void tcg_gen_not_i32(TCGv_i32 ret, TCGv_i32 arg);
void tcg_gen_orc_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_or_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_or_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2);
void tcg_gen_ori_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2);
void tcg_gen_qemu_ld_i32(TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop);
void tcg_gen_qemu_ld_i64(TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop);
void tcg_gen_qemu_st_i32(TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop);
void tcg_gen_qemu_st_i64(TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop);
void tcg_gen_rotr_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_rotri_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2);
void tcg_gen_sar_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_sari_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2);
void tcg_gen_setcond_i32(TCGCond cond, TCGv_i32 ret,
    TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_shl_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_shli_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2);
void tcg_gen_shli_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2);
void tcg_gen_shr_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_shri_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2);
void tcg_gen_shri_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2);
void tcg_gen_st_i32(TCGv_i32 arg1, TCGv_ptr arg2, tcg_target_long offset);
void tcg_gen_st_i64(TCGv_i64 arg1, TCGv_ptr arg2,
    tcg_target_long offset);
#ifdef __cplusplus
}
#endif
#endif /* TCG_FUNCTIONS_H */
