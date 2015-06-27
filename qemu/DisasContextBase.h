#ifndef DISASCONTEXTBASE_H
#define DISASCONTEXTBASE_H
#include "cpu.h"
#include "translate.h"
class DisasContextBase : public DisasContext {
public:
    DisasContextBase() {}
    virtual ~DisasContextBase() {}
    DisasContextBase(const DisasContextBase&) = delete;
    const DisasContextBase& operator=(const DisasContextBase&) = delete;

    virtual void compile() = 0;
    virtual void link() = 0;

    virtual int gen_new_label() = 0;
    virtual void gen_set_label(int n) = 0;
    virtual TCGv_i64 global_mem_new_i64(int reg, intptr_t offset, const char* name) = 0;
    virtual TCGv_i32 global_mem_new_i32(int reg, intptr_t offset, const char* name) = 0;
    virtual TCGv_ptr global_reg_new_ptr(int reg, const char* name) = 0;

    virtual TCGv_i32 const_i32(int32_t val) = 0;
    virtual TCGv_ptr const_ptr(const void* val) = 0;
    virtual TCGv_i64 const_i64(int64_t val) = 0;

    virtual void gen_add2_i32(TCGv_i32 rl, TCGv_i32 rh, TCGv_i32 al,
        TCGv_i32 ah, TCGv_i32 bl, TCGv_i32 bh)
        = 0;

    virtual void gen_add_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) = 0;
    virtual void gen_add_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2) = 0;
    virtual void gen_addi_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2) = 0;
    virtual void gen_addi_ptr(TCGv_ptr ret, TCGv_ptr arg1, int32_t arg2) = 0;
    virtual void gen_addi_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2) = 0;
    virtual void gen_andc_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) = 0;
    virtual void gen_and_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) = 0;
    virtual void gen_and_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2) = 0;
    virtual void gen_andi_i32(TCGv_i32 ret, TCGv_i32 arg1, uint32_t arg2) = 0;
    virtual void gen_andi_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2) = 0;
    virtual void gen_brcondi_i32(TCGCond cond, TCGv_i32 arg1,
        int32_t arg2, int label_index)
        = 0;
    virtual void gen_bswap16_i32(TCGv_i32 ret, TCGv_i32 arg) = 0;
    virtual void gen_bswap32_i32(TCGv_i32 ret, TCGv_i32 arg) = 0;
    virtual void gen_concat_i32_i64(TCGv_i64 dest, TCGv_i32 low,
        TCGv_i32 high)
        = 0;

    virtual void gen_deposit_i32(TCGv_i32 ret, TCGv_i32 arg1,
        TCGv_i32 arg2, unsigned int ofs,
        unsigned int len)
        = 0;
    virtual void gen_mov_i32(TCGv_i32 ret, TCGv_i32 arg) = 0;
    virtual void gen_exit_tb(int direct) = 0;
    virtual void gen_ext16s_i32(TCGv_i32 ret, TCGv_i32 arg) = 0;
    virtual void gen_ext16u_i32(TCGv_i32 ret, TCGv_i32 arg) = 0;
    virtual void gen_ext32u_i64(TCGv_i64 ret, TCGv_i64 arg) = 0;
    virtual void gen_ext8s_i32(TCGv_i32 ret, TCGv_i32 arg) = 0;
    virtual void gen_ext8u_i32(TCGv_i32 ret, TCGv_i32 arg) = 0;
    virtual void gen_ext_i32_i64(TCGv_i64 ret, TCGv_i32 arg) = 0;
    virtual void gen_extu_i32_i64(TCGv_i64 ret, TCGv_i32 arg) = 0;
    virtual void gen_ld_i32(TCGv_i32 ret, TCGv_ptr arg2, tcg_target_long offset) = 0;
    virtual void gen_ld_i64(TCGv_i64 ret, TCGv_ptr arg2,
        target_long offset)
        = 0;
    virtual void gen_movcond_i32(TCGCond cond, TCGv_i32 ret,
        TCGv_i32 c1, TCGv_i32 c2,
        TCGv_i32 v1, TCGv_i32 v2)
        = 0;
    virtual void gen_movcond_i64(TCGCond cond, TCGv_i64 ret,
        TCGv_i64 c1, TCGv_i64 c2,
        TCGv_i64 v1, TCGv_i64 v2)
        = 0;
    virtual void gen_mov_i64(TCGv_i64 ret, TCGv_i64 arg) = 0;
    virtual void gen_movi_i32(TCGv_i32 ret, int32_t arg) = 0;
    virtual void gen_movi_i64(TCGv_i64 ret, int64_t arg) = 0;
    virtual void gen_mul_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) = 0;
    virtual void gen_muls2_i32(TCGv_i32 rl, TCGv_i32 rh,
        TCGv_i32 arg1, TCGv_i32 arg2)
        = 0;
    virtual void gen_mulu2_i32(TCGv_i32 rl, TCGv_i32 rh,
        TCGv_i32 arg1, TCGv_i32 arg2)
        = 0;
    virtual void gen_neg_i32(TCGv_i32 ret, TCGv_i32 arg) = 0;
    virtual void gen_neg_i64(TCGv_i64 ret, TCGv_i64 arg) = 0;
    virtual void gen_not_i32(TCGv_i32 ret, TCGv_i32 arg) = 0;
    virtual void gen_orc_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) = 0;
    virtual void gen_or_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) = 0;
    virtual void gen_or_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2) = 0;
    virtual void gen_ori_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2) = 0;
    virtual void gen_qemu_ld_i32(TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop) = 0;
    virtual void gen_qemu_ld_i64(TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop) = 0;
    virtual void gen_qemu_st_i32(TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop) = 0;
    virtual void gen_qemu_st_i64(TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop) = 0;
    virtual void gen_rotr_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) = 0;
    virtual void gen_rotri_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2) = 0;
    virtual void gen_sar_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) = 0;
    virtual void gen_sari_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2) = 0;
    virtual void gen_setcond_i32(TCGCond cond, TCGv_i32 ret,
        TCGv_i32 arg1, TCGv_i32 arg2)
        = 0;
    virtual void gen_shl_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) = 0;
    virtual void gen_shli_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2) = 0;
    virtual void gen_shli_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2) = 0;
    virtual void gen_shr_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) = 0;
    virtual void gen_shri_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2) = 0;
    virtual void gen_shri_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2) = 0;
    virtual void gen_st_i32(TCGv_i32 arg1, TCGv_ptr arg2, tcg_target_long offset) = 0;
    virtual void gen_st_i64(TCGv_i64 arg1, TCGv_ptr arg2,
        target_long offset)
        = 0;
    virtual void gen_sub_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) = 0;
    virtual void gen_sub_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2) = 0;
    virtual void gen_subi_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2) = 0;
    virtual void gen_trunc_i64_i32(TCGv_i32 ret, TCGv_i64 arg) = 0;
    virtual void gen_xor_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) = 0;
    virtual void gen_xor_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2) = 0;
    virtual void gen_xori_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2) = 0;
    virtual TCGv_i32 temp_local_new_i32() = 0;
    virtual TCGv_i32 temp_new_i32() = 0;
    virtual TCGv_ptr temp_new_ptr() = 0;
    virtual TCGv_i64 temp_new_i64() = 0;
    virtual void temp_free_i32(TCGv_i32 a) = 0;
    virtual void temp_free_i64(TCGv_i64 a) = 0;
    virtual void temp_free_ptr(TCGv_ptr a) = 0;
    virtual void gen_callN(void* func, TCGArg ret,
        int nargs, TCGArg* args)
        = 0;
    virtual void func_start() = 0;

    virtual void gen_sdiv(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) = 0;
    virtual void gen_udiv(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) = 0;

    virtual void gen_vfp_adds(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2, TCGv_ptr fpstatus) = 0;
    virtual void gen_vfp_subs(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2, TCGv_ptr fpstatus) = 0;
    virtual void gen_vfp_muls(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2, TCGv_ptr fpstatus) = 0;
    virtual void gen_vfp_divs(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2, TCGv_ptr fpstatus) = 0;

    virtual void gen_vfp_addd(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2, TCGv_ptr fpstatus) = 0;
    virtual void gen_vfp_subd(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2, TCGv_ptr fpstatus) = 0;
    virtual void gen_vfp_muld(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2, TCGv_ptr fpstatus) = 0;
    virtual void gen_vfp_divd(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2, TCGv_ptr fpstatus) = 0;

    virtual void gen_vfp_touis(TCGv_i32 ret, TCGv_i32 arg, TCGv_ptr fpstatus) = 0;
    virtual void gen_vfp_touizs(TCGv_i32 ret, TCGv_i32 arg, TCGv_ptr fpstatus) = 0;
    virtual void gen_vfp_tosis(TCGv_i32 ret, TCGv_i32 arg, TCGv_ptr fpstatus) = 0;
    virtual void gen_vfp_tosizs(TCGv_i32 ret, TCGv_i32 arg, TCGv_ptr fpstatus) = 0;

    virtual void gen_vfp_touid(TCGv_i32 ret, TCGv_i64 arg, TCGv_ptr fpstatus) = 0;
    virtual void gen_vfp_touizd(TCGv_i32 ret, TCGv_i64 arg, TCGv_ptr fpstatus) = 0;
    virtual void gen_vfp_tosid(TCGv_i32 ret, TCGv_i64 arg, TCGv_ptr fpstatus) = 0;
    virtual void gen_vfp_tosizd(TCGv_i32 ret, TCGv_i64 arg, TCGv_ptr fpstatus) = 0;

    virtual void gen_vfp_sitos(TCGv_i32 ret, TCGv_i32 arg, TCGv_ptr fpstatus) = 0;
    virtual void gen_vfp_uitos(TCGv_i32 ret, TCGv_i32 arg, TCGv_ptr fpstatus) = 0;

    virtual void gen_vfp_sitod(TCGv_i64 ret, TCGv_i32 arg, TCGv_ptr fpstatus) = 0;
    virtual void gen_vfp_uitod(TCGv_i64 ret, TCGv_i32 arg, TCGv_ptr fpstatus) = 0;
};
#endif /* DISASCONTEXTBASE_H */
