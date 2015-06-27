#ifndef TCG_FUNCTIONS_H
#define TCG_FUNCTIONS_H
#include "tgtypes.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct DisasContext DisasContext;

int gen_new_label(DisasContext* s);
void gen_set_label(DisasContext* s, int n);
TCGv_i64 tcg_global_mem_new_i64(DisasContext* s, int reg, intptr_t offset, const char* name);
TCGv_i32 tcg_global_mem_new_i32(DisasContext* s, int reg, intptr_t offset, const char* name);
TCGv_ptr tcg_global_reg_new_ptr(DisasContext* s, int reg, const char* name);

TCGv_i32 tcg_const_i32(DisasContext* s, int32_t val);
TCGv_ptr tcg_const_ptr(DisasContext* s, const void* val);
TCGv_i64 tcg_const_i64(DisasContext* s, int64_t val);

void tcg_gen_add2_i32(DisasContext* s, TCGv_i32 rl, TCGv_i32 rh, TCGv_i32 al,
    TCGv_i32 ah, TCGv_i32 bl, TCGv_i32 bh);

void tcg_gen_add_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_add_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2);
void tcg_gen_addi_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2);
void tcg_gen_addi_ptr(DisasContext* s, TCGv_ptr ret, TCGv_ptr arg1, int32_t arg2);
void tcg_gen_addi_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2);
void tcg_gen_andc_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_and_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_and_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2);
void tcg_gen_andi_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, uint32_t arg2);
void tcg_gen_andi_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2);
void tcg_gen_brcondi_i32(DisasContext* s, TCGCond cond, TCGv_i32 arg1,
    int32_t arg2, int label_index);
void tcg_gen_bswap16_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg);
void tcg_gen_bswap32_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg);
void tcg_gen_concat_i32_i64(DisasContext* s, TCGv_i64 dest, TCGv_i32 low,
    TCGv_i32 high);

void tcg_gen_deposit_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1,
    TCGv_i32 arg2, unsigned int ofs,
    unsigned int len);
void tcg_gen_mov_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg);
void tcg_gen_exit_tb(DisasContext* s, int direct);
void tcg_gen_ext16s_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg);
void tcg_gen_ext16u_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg);
void tcg_gen_ext32u_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg);
void tcg_gen_ext8s_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg);
void tcg_gen_ext8u_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg);
void tcg_gen_ext_i32_i64(DisasContext* s, TCGv_i64 ret, TCGv_i32 arg);
void tcg_gen_extu_i32_i64(DisasContext* s, TCGv_i64 ret, TCGv_i32 arg);
void tcg_gen_ld_i32(DisasContext* s, TCGv_i32 ret, TCGv_ptr arg2, tcg_target_long offset);
void tcg_gen_ld_i64(DisasContext* s, TCGv_i64 ret, TCGv_ptr arg2,
    tcg_target_long offset);
void tcg_gen_movcond_i32(DisasContext* s, TCGCond cond, TCGv_i32 ret,
    TCGv_i32 c1, TCGv_i32 c2,
    TCGv_i32 v1, TCGv_i32 v2);
void tcg_gen_movcond_i64(DisasContext* s, TCGCond cond, TCGv_i64 ret,
    TCGv_i64 c1, TCGv_i64 c2,
    TCGv_i64 v1, TCGv_i64 v2);
void tcg_gen_mov_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg);
void tcg_gen_movi_i32(DisasContext* s, TCGv_i32 ret, int32_t arg);
void tcg_gen_movi_i64(DisasContext* s, TCGv_i64 ret, int64_t arg);
void tcg_gen_mul_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_muls2_i32(DisasContext* s, TCGv_i32 rl, TCGv_i32 rh,
    TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_mulu2_i32(DisasContext* s, TCGv_i32 rl, TCGv_i32 rh,
    TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_neg_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg);
void tcg_gen_neg_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg);
void tcg_gen_not_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg);
void tcg_gen_orc_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_or_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_or_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2);
void tcg_gen_ori_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2);
void tcg_gen_qemu_ld_i32(DisasContext* s, TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop);
void tcg_gen_qemu_ld_i64(DisasContext* s, TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop);
void tcg_gen_qemu_st_i32(DisasContext* s, TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop);
void tcg_gen_qemu_st_i64(DisasContext* s, TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop);
void tcg_gen_rotr_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_rotri_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2);
void tcg_gen_sar_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_sari_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2);
void tcg_gen_setcond_i32(DisasContext* s, TCGCond cond, TCGv_i32 ret,
    TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_shl_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_shli_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2);
void tcg_gen_shli_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2);
void tcg_gen_shr_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_shri_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2);
void tcg_gen_shri_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2);
void tcg_gen_st_i32(DisasContext* s, TCGv_i32 arg1, TCGv_ptr arg2, tcg_target_long offset);
void tcg_gen_st_i64(DisasContext* s, TCGv_i64 arg1, TCGv_ptr arg2,
    tcg_target_long offset);
void tcg_gen_sub_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_sub_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2);
void tcg_gen_subi_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2);
void tcg_gen_trunc_i64_i32(DisasContext* s, TCGv_i32 ret, TCGv_i64 arg);
void tcg_gen_xor_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_xor_i64(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2);
void tcg_gen_xori_i32(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2);
TCGv_i32 tcg_temp_local_new_i32(DisasContext* s);
TCGv_i32 tcg_temp_new_i32(DisasContext* s);
TCGv_ptr tcg_temp_new_ptr(DisasContext* s);
TCGv_i64 tcg_temp_new_i64(DisasContext* s);
void tcg_temp_free_i32(DisasContext* s, TCGv_i32 a);
void tcg_temp_free_i64(DisasContext* s, TCGv_i64 a);
void tcg_temp_free_ptr(DisasContext* s, TCGv_ptr ptr);
void tcg_gen_callN(DisasContext* s, void* func, TCGArg ret,
    int nargs, TCGArg* args);
void tcg_func_start(DisasContext*s);

void tcg_gen_sdiv(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);
void tcg_gen_udiv(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2);

void tcg_gen_vfp_adds(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2, TCGv_ptr fpstatus);
void tcg_gen_vfp_subs(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2, TCGv_ptr fpstatus);
void tcg_gen_vfp_muls(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2, TCGv_ptr fpstatus);
void tcg_gen_vfp_divs(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2, TCGv_ptr fpstatus);

void tcg_gen_vfp_addd(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2, TCGv_ptr fpstatus);
void tcg_gen_vfp_subd(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2, TCGv_ptr fpstatus);
void tcg_gen_vfp_muld(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2, TCGv_ptr fpstatus);
void tcg_gen_vfp_divd(DisasContext* s, TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2, TCGv_ptr fpstatus);

void tcg_gen_vfp_touis(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg, TCGv_ptr fpstatus);
void tcg_gen_vfp_touizs(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg, TCGv_ptr fpstatus);
void tcg_gen_vfp_tosis(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg, TCGv_ptr fpstatus);
void tcg_gen_vfp_tosizs(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg, TCGv_ptr fpstatus);

void tcg_gen_vfp_touid(DisasContext* s, TCGv_i32 ret, TCGv_i64 arg, TCGv_ptr fpstatus);
void tcg_gen_vfp_touizd(DisasContext* s, TCGv_i32 ret, TCGv_i64 arg, TCGv_ptr fpstatus);
void tcg_gen_vfp_tosid(DisasContext* s, TCGv_i32 ret, TCGv_i64 arg, TCGv_ptr fpstatus);
void tcg_gen_vfp_tosizd(DisasContext* s, TCGv_i32 ret, TCGv_i64 arg, TCGv_ptr fpstatus);

void tcg_gen_vfp_sitos(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg, TCGv_ptr fpstatus);
void tcg_gen_vfp_uitos(DisasContext* s, TCGv_i32 ret, TCGv_i32 arg, TCGv_ptr fpstatus);

void tcg_gen_vfp_sitod(DisasContext* s, TCGv_i64 ret, TCGv_i32 arg, TCGv_ptr fpstatus);
void tcg_gen_vfp_uitod(DisasContext* s, TCGv_i64 ret, TCGv_i32 arg, TCGv_ptr fpstatus);
#ifdef __cplusplus
}
#endif
#endif /* TCG_FUNCTIONS_H */
