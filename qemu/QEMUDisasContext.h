#ifndef QEMUDISASCONTEXT_H
#define QEMUDISASCONTEXT_H
#include <memory>
#include "DisasContextBase.h"
#include "tcg.h"
class ExecutableMemoryAllocator;
namespace qemu {

class QEMUDisasContext : public DisasContextBase {
public:
    QEMUDisasContext(ExecutableMemoryAllocator* allocate);

    virtual void compile() override;
    virtual void link(void* dispDirect, void* dispIndirect) override;

    virtual int gen_new_label() override;
    virtual void gen_set_label(int n) override;
    virtual TCGv_i64 global_mem_new_i64(int reg, intptr_t offset, const char* name) override;
    virtual TCGv_i32 global_mem_new_i32(int reg, intptr_t offset, const char* name) override;
    virtual TCGv_ptr global_reg_new_ptr(int reg, const char* name) override;

    virtual TCGv_i32 const_i32(int32_t val) override;
    virtual TCGv_ptr const_ptr(const void* val) override;
    virtual TCGv_i64 const_i64(int64_t val) override;

    virtual void gen_add2_i32(TCGv_i32 rl, TCGv_i32 rh, TCGv_i32 al,
        TCGv_i32 ah, TCGv_i32 bl, TCGv_i32 bh)
        override;

    virtual void gen_add_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) override;
    virtual void gen_add_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2) override;
    virtual void gen_addi_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2) override;
    virtual void gen_addi_ptr(TCGv_ptr ret, TCGv_ptr arg1, int32_t arg2) override;
    virtual void gen_addi_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2) override;
    virtual void gen_andc_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) override;
    virtual void gen_and_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) override;
    virtual void gen_and_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2) override;
    virtual void gen_andi_i32(TCGv_i32 ret, TCGv_i32 arg1, uint32_t arg2) override;
    virtual void gen_andi_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2) override;
    virtual void gen_brcondi_i32(TCGCond cond, TCGv_i32 arg1,
        int32_t arg2, int label_index)
        override;
    virtual void gen_bswap16_i32(TCGv_i32 ret, TCGv_i32 arg) override;
    virtual void gen_bswap32_i32(TCGv_i32 ret, TCGv_i32 arg) override;
    virtual void gen_concat_i32_i64(TCGv_i64 dest, TCGv_i32 low,
        TCGv_i32 high)
        override;

    virtual void gen_deposit_i32(TCGv_i32 ret, TCGv_i32 arg1,
        TCGv_i32 arg2, unsigned int ofs,
        unsigned int len)
        override;
    virtual void gen_mov_i32(TCGv_i32 ret, TCGv_i32 arg) override;
    virtual void gen_exit_tb(int direct) override;
    virtual void gen_ext16s_i32(TCGv_i32 ret, TCGv_i32 arg) override;
    virtual void gen_ext16u_i32(TCGv_i32 ret, TCGv_i32 arg) override;
    virtual void gen_ext32u_i64(TCGv_i64 ret, TCGv_i64 arg) override;
    virtual void gen_ext8s_i32(TCGv_i32 ret, TCGv_i32 arg) override;
    virtual void gen_ext8u_i32(TCGv_i32 ret, TCGv_i32 arg) override;
    virtual void gen_ext_i32_i64(TCGv_i64 ret, TCGv_i32 arg) override;
    virtual void gen_extu_i32_i64(TCGv_i64 ret, TCGv_i32 arg) override;
    virtual void gen_ld_i32(TCGv_i32 ret, TCGv_ptr arg2, tcg_target_long offset) override;
    virtual void gen_ld_i64(TCGv_i64 ret, TCGv_ptr arg2,
        target_long offset)
        override;
    virtual void gen_movcond_i32(TCGCond cond, TCGv_i32 ret,
        TCGv_i32 c1, TCGv_i32 c2,
        TCGv_i32 v1, TCGv_i32 v2)
        override;
    virtual void gen_movcond_i64(TCGCond cond, TCGv_i64 ret,
        TCGv_i64 c1, TCGv_i64 c2,
        TCGv_i64 v1, TCGv_i64 v2)
        override;
    virtual void gen_mov_i64(TCGv_i64 ret, TCGv_i64 arg) override;
    virtual void gen_movi_i32(TCGv_i32 ret, int32_t arg) override;
    virtual void gen_movi_i64(TCGv_i64 ret, int64_t arg) override;
    virtual void gen_mul_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) override;
    virtual void gen_muls2_i32(TCGv_i32 rl, TCGv_i32 rh,
        TCGv_i32 arg1, TCGv_i32 arg2)
        override;
    virtual void gen_mulu2_i32(TCGv_i32 rl, TCGv_i32 rh,
        TCGv_i32 arg1, TCGv_i32 arg2)
        override;
    virtual void gen_neg_i32(TCGv_i32 ret, TCGv_i32 arg) override;
    virtual void gen_neg_i64(TCGv_i64 ret, TCGv_i64 arg) override;
    virtual void gen_not_i32(TCGv_i32 ret, TCGv_i32 arg) override;
    virtual void gen_orc_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) override;
    virtual void gen_or_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) override;
    virtual void gen_or_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2) override;
    virtual void gen_ori_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2) override;
    virtual void gen_qemu_ld_i32(TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop) override;
    virtual void gen_qemu_ld_i64(TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop) override;
    virtual void gen_qemu_st_i32(TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop) override;
    virtual void gen_qemu_st_i64(TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop) override;
    virtual void gen_rotr_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) override;
    virtual void gen_rotri_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2) override;
    virtual void gen_sar_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) override;
    virtual void gen_sari_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2) override;
    virtual void gen_setcond_i32(TCGCond cond, TCGv_i32 ret,
        TCGv_i32 arg1, TCGv_i32 arg2)
        override;
    virtual void gen_shl_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) override;
    virtual void gen_shli_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2) override;
    virtual void gen_shli_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2) override;
    virtual void gen_shr_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) override;
    virtual void gen_shri_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2) override;
    virtual void gen_shri_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2) override;
    virtual void gen_st_i32(TCGv_i32 arg1, TCGv_ptr arg2, tcg_target_long offset) override;
    virtual void gen_st_i64(TCGv_i64 arg1, TCGv_ptr arg2,
        target_long offset)
        override;
    virtual void gen_sub_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) override;
    virtual void gen_sub_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2) override;
    virtual void gen_subi_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2) override;
    virtual void gen_trunc_i64_i32(TCGv_i32 ret, TCGv_i64 arg) override;
    virtual void gen_xor_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) override;
    virtual void gen_xor_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2) override;
    virtual void gen_xori_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2) override;
    virtual TCGv_i32 temp_local_new_i32() override;
    virtual TCGv_i32 temp_new_i32() override;
    virtual TCGv_ptr temp_new_ptr() override;
    virtual TCGv_i64 temp_new_i64() override;
    virtual void gen_callN(void* func, TCGArg ret,
        int nargs, TCGArg* args)
        override;

    virtual void gen_sdiv(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) override;
    virtual void gen_udiv(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) override;

    virtual void gen_vfp_adds(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) override;
    virtual void gen_vfp_subs(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) override;
    virtual void gen_vfp_muls(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) override;
    virtual void gen_vfp_divs(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2) override;

    virtual void gen_vfp_addd(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2) override;
    virtual void gen_vfp_subd(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2) override;
    virtual void gen_vfp_muld(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2) override;
    virtual void gen_vfp_divd(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2) override;

    virtual void gen_vfp_touis(TCGv_i32 ret, TCGv_i32 arg) override;
    virtual void gen_vfp_touizs(TCGv_i32 ret, TCGv_i32 arg) override;
    virtual void gen_vfp_tosis(TCGv_i32 ret, TCGv_i32 arg) override;
    virtual void gen_vfp_tosizs(TCGv_i32 ret, TCGv_i32 arg) override;

    virtual void gen_vfp_touid(TCGv_i32 ret, TCGv_i64 arg) override;
    virtual void gen_vfp_touizd(TCGv_i32 ret, TCGv_i64 arg) override;
    virtual void gen_vfp_tosid(TCGv_i32 ret, TCGv_i64 arg) override;
    virtual void gen_vfp_tosizd(TCGv_i32 ret, TCGv_i64 arg) override;

    virtual void gen_vfp_sitos(TCGv_i32 ret, TCGv_i32 arg) override;
    virtual void gen_vfp_uitos(TCGv_i32 ret, TCGv_i32 arg) override;

    virtual void gen_vfp_sitod(TCGv_i64 ret, TCGv_i32 arg) override;
    virtual void gen_vfp_uitod(TCGv_i64 ret, TCGv_i32 arg) override;

private:
    int tcg_global_mem_new_internal(TCGType type, int reg,
        intptr_t offset,
        const char* name);
    void tcg_gen_op0(TCGOpcode opc);

    void tcg_gen_op1_i32(TCGOpcode opc, TCGv_i32 arg1);

    void tcg_gen_op1_i64(TCGOpcode opc, TCGv_i64 arg1);

    void tcg_gen_op1i(TCGOpcode opc, TCGArg arg1);

    void tcg_gen_op2_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2);

    void tcg_gen_op2_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2);

    void tcg_gen_op2i_i32(TCGOpcode opc, TCGv_i32 arg1, TCGArg arg2);

    void tcg_gen_op2i_i64(TCGOpcode opc, TCGv_i64 arg1, TCGArg arg2);

    void tcg_gen_op2ii(TCGOpcode opc, TCGArg arg1, TCGArg arg2);

    void tcg_gen_op3_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
        TCGv_i32 arg3);

    void tcg_gen_op3_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
        TCGv_i64 arg3);

    void tcg_gen_op3i_i32(TCGOpcode opc, TCGv_i32 arg1,
        TCGv_i32 arg2, TCGArg arg3);

    void tcg_gen_op3i_i64(TCGOpcode opc, TCGv_i64 arg1,
        TCGv_i64 arg2, TCGArg arg3);

    void tcg_gen_ldst_op_i32(TCGOpcode opc, TCGv_i32 val,
        TCGv_ptr base, TCGArg offset);

    void tcg_gen_ldst_op_i64(TCGOpcode opc, TCGv_i64 val,
        TCGv_ptr base, TCGArg offset);

    void tcg_gen_op4_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
        TCGv_i32 arg3, TCGv_i32 arg4);

    void tcg_gen_op4_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
        TCGv_i64 arg3, TCGv_i64 arg4);

    void tcg_gen_op4i_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
        TCGv_i32 arg3, TCGArg arg4);

    void tcg_gen_op4i_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
        TCGv_i64 arg3, TCGArg arg4);

    void tcg_gen_op4ii_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
        TCGArg arg3, TCGArg arg4);

    void tcg_gen_op4ii_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
        TCGArg arg3, TCGArg arg4);

    void tcg_gen_op5_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
        TCGv_i32 arg3, TCGv_i32 arg4, TCGv_i32 arg5);

    void tcg_gen_op5_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
        TCGv_i64 arg3, TCGv_i64 arg4, TCGv_i64 arg5);

    void tcg_gen_op5i_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
        TCGv_i32 arg3, TCGv_i32 arg4, TCGArg arg5);

    void tcg_gen_op5i_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
        TCGv_i64 arg3, TCGv_i64 arg4, TCGArg arg5);

    void tcg_gen_op5ii_i32(TCGOpcode opc, TCGv_i32 arg1,
        TCGv_i32 arg2, TCGv_i32 arg3,
        TCGArg arg4, TCGArg arg5);

    void tcg_gen_op5ii_i64(TCGOpcode opc, TCGv_i64 arg1,
        TCGv_i64 arg2, TCGv_i64 arg3,
        TCGArg arg4, TCGArg arg5);

    void tcg_gen_op6_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
        TCGv_i32 arg3, TCGv_i32 arg4, TCGv_i32 arg5,
        TCGv_i32 arg6);

    void tcg_gen_op6_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
        TCGv_i64 arg3, TCGv_i64 arg4, TCGv_i64 arg5,
        TCGv_i64 arg6);

    void tcg_gen_op6i_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
        TCGv_i32 arg3, TCGv_i32 arg4,
        TCGv_i32 arg5, TCGArg arg6);

    void tcg_gen_op6i_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
        TCGv_i64 arg3, TCGv_i64 arg4,
        TCGv_i64 arg5, TCGArg arg6);

    void tcg_gen_op6ii_i32(TCGOpcode opc, TCGv_i32 arg1,
        TCGv_i32 arg2, TCGv_i32 arg3,
        TCGv_i32 arg4, TCGArg arg5, TCGArg arg6);

    void tcg_gen_op6ii_i64(TCGOpcode opc, TCGv_i64 arg1,
        TCGv_i64 arg2, TCGv_i64 arg3,
        TCGv_i64 arg4, TCGArg arg5, TCGArg arg6);

    void tcg_add_param_i32(TCGv_i32 val);

    void tcg_add_param_i64(TCGv_i64 val);
    struct QEMUDisasContextImpl;
    std::unique_ptr<QEMUDisasContextImpl> m_impl;
};
}
#endif /* QEMUDISASCONTEXT_H */
