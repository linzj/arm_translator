#include <stdlib.h>
#include "compatglib.h"
#include "QEMUDisasContext.h"
#include "log.h"

namespace qemu
{

void* tcg_malloc_internal(TCGContext* s, int size)
{
    TCGPool* p;
    int pool_size;

    if (size > TCG_POOL_CHUNK_SIZE) {
        /* big malloc: insert a new pool (XXX: could optimize) */
        p = malloc(sizeof(TCGPool) + size);
        p->size = size;
        p->next = s->pool_first_large;
        s->pool_first_large = p;
        return p->data;
    } else {
        p = s->pool_current;
        if (!p) {
            p = s->pool_first;
            if (!p)
                goto new_pool;
        } else {
            if (!p->next) {
new_pool:
                pool_size = TCG_POOL_CHUNK_SIZE;
                p = malloc(sizeof(TCGPool) + pool_size);
                p->size = pool_size;
                p->next = NULL;
                if (s->pool_current)
                    s->pool_current->next = p;
                else
                    s->pool_first = p;
            } else {
                p = p->next;
            }
        }
    }
    s->pool_current = p;
    s->pool_cur = p->data + size;
    s->pool_end = p->data + p->size;
    return p->data;
}

void tcg_pool_reset(TCGContext* s)
{
    TCGPool *p, *t;
    for (p = s->pool_first_large; p; p = t) {
        t = p->next;
        g_free(p);
    }
    s->pool_first_large = NULL;
    s->pool_cur = s->pool_end = NULL;
    s->pool_current = NULL;
}

#if UINTPTR_MAX == UINT32_MAX
#define HOST_LONG_BITS 32
#elif UINTPTR_MAX == UINT64_MAX
#define HOST_LONG_BITS 64
#else
#error Unknown pointer size
#endif

#if HOST_LONG_BITS == 32
#define MAX_OPC_PARAM_PER_ARG 2
#else
#define MAX_OPC_PARAM_PER_ARG 1
#endif
#define MAX_OPC_PARAM_IARGS 5
#define MAX_OPC_PARAM_OARGS 1
#define MAX_OPC_PARAM_ARGS (MAX_OPC_PARAM_IARGS + MAX_OPC_PARAM_OARGS)

/* A Call op needs up to 4 + 2N parameters on 32-bit archs,
 * and up to 4 + N parameters on 64-bit archs
 * (N = number of input arguments + output arguments).  */
#define MAX_OPC_PARAM (4 + (MAX_OPC_PARAM_PER_ARG * MAX_OPC_PARAM_ARGS))
#define OPC_BUF_SIZE 640
#define OPC_MAX_SIZE (OPC_BUF_SIZE - MAX_OP_PER_INSTR)

void QEMUDisasContext::tcg_gen_op0(TCGOpcode opc)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
}

void QEMUDisasContext::tcg_gen_op1_i32(TCGOpcode opc, TCGv_i32 arg1)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
}

void QEMUDisasContext::tcg_gen_op1_i64(TCGOpcode opc, TCGv_i64 arg1)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
}

void QEMUDisasContext::tcg_gen_op1i(TCGOpcode opc, TCGArg arg1)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = arg1;
}

void QEMUDisasContext::tcg_gen_op2_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
}

void QEMUDisasContext::tcg_gen_op2_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
}

void QEMUDisasContext::tcg_gen_op2i_i32(TCGOpcode opc, TCGv_i32 arg1, TCGArg arg2)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = arg2;
}

void QEMUDisasContext::tcg_gen_op2i_i64(TCGOpcode opc, TCGv_i64 arg1, TCGArg arg2)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = arg2;
}

void QEMUDisasContext::tcg_gen_op2ii(TCGOpcode opc, TCGArg arg1, TCGArg arg2)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = arg1;
    *tcg_ctx.gen_opparam_ptr++ = arg2;
}

void QEMUDisasContext::tcg_gen_op3_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
                                       TCGv_i32 arg3)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg3);
}

void QEMUDisasContext::tcg_gen_op3_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
                                       TCGv_i64 arg3)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg3);
}

void QEMUDisasContext::tcg_gen_op3i_i32(TCGOpcode opc, TCGv_i32 arg1,
                                        TCGv_i32 arg2, TCGArg arg3)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = arg3;
}

void QEMUDisasContext::tcg_gen_op3i_i64(TCGOpcode opc, TCGv_i64 arg1,
                                        TCGv_i64 arg2, TCGArg arg3)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = arg3;
}

void QEMUDisasContext::tcg_gen_ldst_op_i32(TCGOpcode opc, TCGv_i32 val,
        TCGv_ptr base, TCGArg offset)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(val);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_PTR(base);
    *tcg_ctx.gen_opparam_ptr++ = offset;
}

void QEMUDisasContext::tcg_gen_ldst_op_i64(TCGOpcode opc, TCGv_i64 val,
        TCGv_ptr base, TCGArg offset)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(val);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_PTR(base);
    *tcg_ctx.gen_opparam_ptr++ = offset;
}

void QEMUDisasContext::tcg_gen_op4_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
                                       TCGv_i32 arg3, TCGv_i32 arg4)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg4);
}

void QEMUDisasContext::tcg_gen_op4_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
                                       TCGv_i64 arg3, TCGv_i64 arg4)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg4);
}

void QEMUDisasContext::tcg_gen_op4i_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
                                        TCGv_i32 arg3, TCGArg arg4)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg3);
    *tcg_ctx.gen_opparam_ptr++ = arg4;
}

void QEMUDisasContext::tcg_gen_op4i_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
                                        TCGv_i64 arg3, TCGArg arg4)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg3);
    *tcg_ctx.gen_opparam_ptr++ = arg4;
}

void QEMUDisasContext::tcg_gen_op4ii_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
        TCGArg arg3, TCGArg arg4)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = arg3;
    *tcg_ctx.gen_opparam_ptr++ = arg4;
}

void QEMUDisasContext::tcg_gen_op4ii_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
        TCGArg arg3, TCGArg arg4)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = arg3;
    *tcg_ctx.gen_opparam_ptr++ = arg4;
}

void QEMUDisasContext::tcg_gen_op5_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
                                       TCGv_i32 arg3, TCGv_i32 arg4, TCGv_i32 arg5)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg4);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg5);
}

void QEMUDisasContext::tcg_gen_op5_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
                                       TCGv_i64 arg3, TCGv_i64 arg4, TCGv_i64 arg5)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg4);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg5);
}

void QEMUDisasContext::tcg_gen_op5i_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
                                        TCGv_i32 arg3, TCGv_i32 arg4, TCGArg arg5)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg4);
    *tcg_ctx.gen_opparam_ptr++ = arg5;
}

void QEMUDisasContext::tcg_gen_op5i_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
                                        TCGv_i64 arg3, TCGv_i64 arg4, TCGArg arg5)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg4);
    *tcg_ctx.gen_opparam_ptr++ = arg5;
}

void QEMUDisasContext::tcg_gen_op5ii_i32(TCGOpcode opc, TCGv_i32 arg1,
        TCGv_i32 arg2, TCGv_i32 arg3,
        TCGArg arg4, TCGArg arg5)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg3);
    *tcg_ctx.gen_opparam_ptr++ = arg4;
    *tcg_ctx.gen_opparam_ptr++ = arg5;
}

void QEMUDisasContext::tcg_gen_op5ii_i64(TCGOpcode opc, TCGv_i64 arg1,
        TCGv_i64 arg2, TCGv_i64 arg3,
        TCGArg arg4, TCGArg arg5)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg3);
    *tcg_ctx.gen_opparam_ptr++ = arg4;
    *tcg_ctx.gen_opparam_ptr++ = arg5;
}

void QEMUDisasContext::tcg_gen_op6_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
                                       TCGv_i32 arg3, TCGv_i32 arg4, TCGv_i32 arg5,
                                       TCGv_i32 arg6)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg4);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg5);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg6);
}

void QEMUDisasContext::tcg_gen_op6_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
                                       TCGv_i64 arg3, TCGv_i64 arg4, TCGv_i64 arg5,
                                       TCGv_i64 arg6)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg4);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg5);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg6);
}

void QEMUDisasContext::tcg_gen_op6i_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
                                        TCGv_i32 arg3, TCGv_i32 arg4,
                                        TCGv_i32 arg5, TCGArg arg6)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg4);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg5);
    *tcg_ctx.gen_opparam_ptr++ = arg6;
}

void QEMUDisasContext::tcg_gen_op6i_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
                                        TCGv_i64 arg3, TCGv_i64 arg4,
                                        TCGv_i64 arg5, TCGArg arg6)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg4);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg5);
    *tcg_ctx.gen_opparam_ptr++ = arg6;
}

void QEMUDisasContext::tcg_gen_op6ii_i32(TCGOpcode opc, TCGv_i32 arg1,
        TCGv_i32 arg2, TCGv_i32 arg3,
        TCGv_i32 arg4, TCGArg arg5, TCGArg arg6)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg4);
    *tcg_ctx.gen_opparam_ptr++ = arg5;
    *tcg_ctx.gen_opparam_ptr++ = arg6;
}

void QEMUDisasContext::tcg_gen_op6ii_i64(TCGOpcode opc, TCGv_i64 arg1,
        TCGv_i64 arg2, TCGv_i64 arg3,
        TCGv_i64 arg4, TCGArg arg5, TCGArg arg6)
{
    TCGContext* tcg_ctx = &m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg4);
    *tcg_ctx.gen_opparam_ptr++ = arg5;
    *tcg_ctx.gen_opparam_ptr++ = arg6;
}

void QEMUDisasContext::tcg_add_param_i32(TCGv_i32 val)
{
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(val);
}

void QEMUDisasContext::tcg_add_param_i64(TCGv_i64 val)
{
#if TCG_TARGET_REG_BITS == 32
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(TCGV_LOW(val));
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(TCGV_HIGH(val));
#else
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(val);
#endif
}

static inline TCGv_i32 QEMU_ARTIFICIAL MAKE_TCGV_I32(intptr_t i)
{
    return (TCGv_i32)i;
}

static inline TCGv_i64 QEMU_ARTIFICIAL MAKE_TCGV_I64(intptr_t i)
{
    return (TCGv_i64)i;
}

static inline TCGv_ptr QEMU_ARTIFICIAL MAKE_TCGV_PTR(intptr_t i)
{
    return (TCGv_ptr)i;
}

static inline intptr_t QEMU_ARTIFICIAL GET_TCGV_I32(TCGv_i32 t)
{
    return (intptr_t)t;
}

static inline intptr_t QEMU_ARTIFICIAL GET_TCGV_I64(TCGv_i64 t)
{
    return (intptr_t)t;
}

static inline intptr_t QEMU_ARTIFICIAL GET_TCGV_PTR(TCGv_ptr t)
{
    return (intptr_t)t;
}

struct QEMUDisasContext::QEMUDisasContextImpl {
    ExecutableMemoryAllocator* m_allocator;
    TCGContext m_tcgCtx;
};

QEMUDisasContext::QEMUDisasContext(ExecutableMemoryAllocator* allocator)
    : m_impl(new QEMUDisasContextImpl( { allocator } )
{
}

int QEMUDisasContext::gen_new_label()
{
    TCGContext* s = &m_impl->m_tcgCtx;
    int idx;
    TCGLabel* l;

    if (s->nb_labels >= TCG_MAX_LABELS)
        tcg_abort();
    idx = s->nb_labels++;
    l = &s->labels[idx];
    l->has_value = 0;
    l->u.first_reloc = NULL;
    return idx;
}

void QEMUDisasContext::gen_set_label(int n)
{
    tcg_gen_op1i(INDEX_op_set_label, n);
}

int QEMUDisasContext::tcg_global_mem_new_internal(TCGType type, int reg,
        intptr_t offset,
        const char *name)
{
    TCGContext* s = &m_impl->m_tcgCtx;
    TCGTemp* ts;
    int idx;

    idx = s->nb_globals;
#if TCG_TARGET_REG_BITS == 32
    if (type == TCG_TYPE_I64) {
        char buf[64];
        tcg_temp_alloc(s, s->nb_globals + 2);
        ts = &s->temps[s->nb_globals];
        ts->base_type = type;
        ts->type = TCG_TYPE_I32;
        ts->fixed_reg = 0;
        ts->mem_allocated = 1;
        ts->mem_reg = reg;
#ifdef HOST_WORDS_BIGENDIAN
        ts->mem_offset = offset + 4;
#else
        ts->mem_offset = offset;
#endif
        pstrcpy(buf, sizeof(buf), name);
        pstrcat(buf, sizeof(buf), "_0");
        ts->name = strdup(buf);
        ts++;

        ts->base_type = type;
        ts->type = TCG_TYPE_I32;
        ts->fixed_reg = 0;
        ts->mem_allocated = 1;
        ts->mem_reg = reg;
#ifdef HOST_WORDS_BIGENDIAN
        ts->mem_offset = offset;
#else
        ts->mem_offset = offset + 4;
#endif
        pstrcpy(buf, sizeof(buf), name);
        pstrcat(buf, sizeof(buf), "_1");
        ts->name = strdup(buf);

        s->nb_globals += 2;
    } else
#endif
    {
        tcg_temp_alloc(s, s->nb_globals + 1);
        ts = &s->temps[s->nb_globals];
        ts->base_type = type;
        ts->type = type;
        ts->fixed_reg = 0;
        ts->mem_allocated = 1;
        ts->mem_reg = reg;
        ts->mem_offset = offset;
        ts->name = name;
        s->nb_globals++;
    }
    return idx;
}

TCGv_i64 QEMUDisasContext::global_mem_new_i64(int reg, intptr_t offset, const char* name)
{
    int idx = tcg_global_mem_new_internal(TCG_TYPE_I64, reg, offset, name);
    return MAKE_TCGV_I64(idx);
}

TCGv_i32 QEMUDisasContext::global_mem_new_i32(int reg, intptr_t offset, const char* name)
{
    int idx = tcg_global_mem_new_internal(TCG_TYPE_I32, reg, offset, name);
    return MAKE_TCGV_I32(idx);
}

TCGv_ptr QEMUDisasContext::global_reg_new_ptr(int reg, const char* name)
{
#if UINTPTR_MAX == UINT32_MAX
    return TCGV_NAT_TO_PTR(tcg_global_reg_new_i32(reg, name));
#else
    return TCGV_NAT_TO_PTR(tcg_global_reg_new_i64(reg, name));
#endif
}

TCGv_i32 QEMUDisasContext::const_i32(int32_t val)
{
    TCGv_i32 t0;
    t0 = temp_new_i32();
    gen_movi_i32(t0, val);
    return t0;
}

TCGv_ptr QEMUDisasContext::const_ptr(const void* val)
{
#if UINTPTR_MAX == UINT32_MAX
    return TCGV_NAT_TO_PTR(const_i32(reinterpret_cast<int32_t>(val)));
#else
    return TCGV_NAT_TO_PTR(const_i64(reinterpret_cast<int64_t>(val)));
#endif
}

TCGv_i64 QEMUDisasContext::const_i64(int64_t val)
{
    TCGv_i64 t0;
    t0 = temp_new_i64();
    gen_movi_i64(t0, val);
    return t0;
}

void QEMUDisasContext::gen_add2_i32(TCGv_i32 rl, TCGv_i32 rh, TCGv_i32 al,
                                    TCGv_i32 ah, TCGv_i32 bl, TCGv_i32 bh)
{
    if (TCG_TARGET_HAS_add2_i32) {
        tcg_gen_op6_i32(INDEX_op_add2_i32, rl, rh, al, ah, bl, bh);
        /* Allow the optimizer room to replace add2 with two moves.  */
        tcg_gen_op0(INDEX_op_nop);
    } else {
        EMUNREACHABLE();
    }
}

void QEMUDisasContext::gen_add_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    tcg_gen_op3_i32(INDEX_op_add_i32, ret, arg1, arg2);
}

void QEMUDisasContext::gen_add_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    tcg_gen_op6_i32(INDEX_op_add2_i32, TCGV_LOW(ret), TCGV_HIGH(ret),
                    TCGV_LOW(arg1), TCGV_HIGH(arg1), TCGV_LOW(arg2),
                    TCGV_HIGH(arg2));
    /* Allow the optimizer room to replace add2 with two moves.  */
    tcg_gen_op0(INDEX_op_nop);
}

void QEMUDisasContext::gen_addi_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    /* some cases can be optimized here */
    if (arg2 == 0) {
        tcg_gen_mov_i32(ret, arg1);
    } else {
        TCGv_i32 t0 = tcg_const_i32(arg2);
        tcg_gen_add_i32(ret, arg1, t0);
        tcg_temp_free_i32(t0);
    }
}

void QEMUDisasContext::gen_addi_ptr(TCGv_ptr ret, TCGv_ptr arg1, int32_t arg2)
{
}
void QEMUDisasContext::gen_addi_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
}
void QEMUDisasContext::gen_andc_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
}
void QEMUDisasContext::gen_and_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
}
void QEMUDisasContext::gen_and_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
}
void QEMUDisasContext::gen_andi_i32(TCGv_i32 ret, TCGv_i32 arg1, uint32_t arg2)
{
}
void QEMUDisasContext::gen_andi_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
}
void QEMUDisasContext::gen_brcondi_i32(TCGCond cond, TCGv_i32 arg1,
                                       int32_t arg2, int label_index)

{
}
void QEMUDisasContext::gen_bswap16_i32(TCGv_i32 ret, TCGv_i32 arg)
{
}
void QEMUDisasContext::gen_bswap32_i32(TCGv_i32 ret, TCGv_i32 arg)
{
}
void QEMUDisasContext::gen_concat_i32_i64(TCGv_i64 dest, TCGv_i32 low,
        TCGv_i32 high)

{
}

void QEMUDisasContext::gen_deposit_i32(TCGv_i32 ret, TCGv_i32 arg1,
                                       TCGv_i32 arg2, unsigned int ofs,
                                       unsigned int len)

{
}
void QEMUDisasContext::gen_mov_i32(TCGv_i32 ret, TCGv_i32 arg)
{
}
void QEMUDisasContext::gen_exit_tb(int direct)
{
}
void QEMUDisasContext::gen_ext16s_i32(TCGv_i32 ret, TCGv_i32 arg)
{
}
void QEMUDisasContext::gen_ext16u_i32(TCGv_i32 ret, TCGv_i32 arg)
{
}
void QEMUDisasContext::gen_ext32u_i64(TCGv_i64 ret, TCGv_i64 arg)
{
}
void QEMUDisasContext::gen_ext8s_i32(TCGv_i32 ret, TCGv_i32 arg)
{
}
void QEMUDisasContext::gen_ext8u_i32(TCGv_i32 ret, TCGv_i32 arg)
{
}
void QEMUDisasContext::gen_ext_i32_i64(TCGv_i64 ret, TCGv_i32 arg)
{
}
void QEMUDisasContext::gen_extu_i32_i64(TCGv_i64 ret, TCGv_i32 arg)
{
}
void QEMUDisasContext::gen_ld_i32(TCGv_i32 ret, TCGv_ptr arg2, tcg_target_long offset)
{
}
void QEMUDisasContext::gen_ld_i64(TCGv_i64 ret, TCGv_ptr arg2,
                                  target_long offset)

{
}
void QEMUDisasContext::gen_movcond_i32(TCGCond cond, TCGv_i32 ret,
                                       TCGv_i32 c1, TCGv_i32 c2,
                                       TCGv_i32 v1, TCGv_i32 v2)

{
}
void QEMUDisasContext::gen_movcond_i64(TCGCond cond, TCGv_i64 ret,
                                       TCGv_i64 c1, TCGv_i64 c2,
                                       TCGv_i64 v1, TCGv_i64 v2)

{
}
void QEMUDisasContext::gen_mov_i64(TCGv_i64 ret, TCGv_i64 arg)
{
}
void QEMUDisasContext::gen_movi_i32(TCGv_i32 ret, int32_t arg)
{
}
void QEMUDisasContext::gen_movi_i64(TCGv_i64 ret, int64_t arg)
{
}
void QEMUDisasContext::gen_mul_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
}
void QEMUDisasContext::gen_muls2_i32(TCGv_i32 rl, TCGv_i32 rh,
                                     TCGv_i32 arg1, TCGv_i32 arg2)

{
}
void QEMUDisasContext::gen_mulu2_i32(TCGv_i32 rl, TCGv_i32 rh,
                                     TCGv_i32 arg1, TCGv_i32 arg2)

{
}
void QEMUDisasContext::gen_neg_i32(TCGv_i32 ret, TCGv_i32 arg)
{
}
void QEMUDisasContext::gen_neg_i64(TCGv_i64 ret, TCGv_i64 arg)
{
}
void QEMUDisasContext::gen_not_i32(TCGv_i32 ret, TCGv_i32 arg)
{
}
void QEMUDisasContext::gen_orc_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
}
void QEMUDisasContext::gen_or_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
}
void QEMUDisasContext::gen_or_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
}
void QEMUDisasContext::gen_ori_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
}
void QEMUDisasContext::gen_qemu_ld_i32(TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
}
void QEMUDisasContext::gen_qemu_ld_i64(TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
}
void QEMUDisasContext::gen_qemu_st_i32(TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
}
void QEMUDisasContext::gen_qemu_st_i64(TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
}
void QEMUDisasContext::gen_rotr_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
}
void QEMUDisasContext::gen_rotri_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
}
void QEMUDisasContext::gen_sar_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
}
void QEMUDisasContext::gen_sari_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
}
void QEMUDisasContext::gen_setcond_i32(TCGCond cond, TCGv_i32 ret,
                                       TCGv_i32 arg1, TCGv_i32 arg2)

{
}
void QEMUDisasContext::gen_shl_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
}
void QEMUDisasContext::gen_shli_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
}
void QEMUDisasContext::gen_shli_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
}
void QEMUDisasContext::gen_shr_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
}
void QEMUDisasContext::gen_shri_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
}
void QEMUDisasContext::gen_shri_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
}
void QEMUDisasContext::gen_st_i32(TCGv_i32 arg1, TCGv_ptr arg2, tcg_target_long offset)
{
}
void QEMUDisasContext::gen_st_i64(TCGv_i64 arg1, TCGv_ptr arg2,
                                  target_long offset)

{
}
void QEMUDisasContext::gen_sub_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
}
void QEMUDisasContext::gen_sub_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
}
void QEMUDisasContext::gen_subi_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
}
void QEMUDisasContext::gen_trunc_i64_i32(TCGv_i32 ret, TCGv_i64 arg)
{
}
void QEMUDisasContext::gen_xor_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
}
void QEMUDisasContext::gen_xor_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
}
void QEMUDisasContext::gen_xori_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
}
TCGv_i32 QEMUDisasContext::temp_local_new_i32()
{
}
TCGv_i32 QEMUDisasContext::temp_new_i32()
{
}
TCGv_ptr QEMUDisasContext::temp_new_ptr()
{
}
TCGv_i64 QEMUDisasContext::temp_new_i64()
{
}
void QEMUDisasContext::gen_callN(void* func, TCGArg ret,
                                 int nargs, TCGArg* args)
{
}
}
