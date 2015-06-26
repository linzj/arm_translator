#include <stdlib.h>
#include <string.h>
#include <cpuid.h>
#include "compatglib.h"
#include "QEMUDisasContext.h"
#include "log.h"

namespace qemu {

typedef struct TCGHelperInfo {
    void* func;
    const char* name;
    unsigned flags;
    unsigned sizemask;
} TCGHelperInfo;

#include "helper-proto.h"

static const TCGHelperInfo all_helpers[] = {
#include "helper-tcg.h"
};

int gen_new_label(TCGContext* s)
{
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

struct QEMUDisasContext::QEMUDisasContextImpl {
    ExecutableMemoryAllocator* m_allocator;
    TCGContext m_tcgCtx;
};

static inline TCGMemOp tcg_canonicalize_memop(TCGMemOp op, bool is64, bool st)
{
    unsigned opInt = (unsigned)op;
    switch (opInt & MO_SIZE) {
    case MO_8:
        opInt &= ~MO_BSWAP;
        break;
    case MO_16:
        break;
    case MO_32:
        if (!is64) {
            opInt &= ~MO_SIGN;
        }
        break;
    case MO_64:
        if (!is64) {
            tcg_abort();
        }
        break;
    }
    if (st) {
        opInt &= ~MO_SIGN;
    }

    return (TCGMemOp)opInt;
}

void* tcg_malloc_internal(TCGContext* s, int size)
{
    TCGPool* p;
    int pool_size;

    if (size > TCG_POOL_CHUNK_SIZE) {
        /* big malloc: insert a new pool (XXX: could optimize) */
        p = static_cast<TCGPool*>(malloc(sizeof(TCGPool) + size));
        p->size = size;
        p->next = s->pool_first_large;
        s->pool_first_large = p;
        return p->data;
    }
    else {
        p = s->pool_current;
        if (!p) {
            p = s->pool_first;
            if (!p)
                goto new_pool;
        }
        else {
            if (!p->next) {
            new_pool:
                pool_size = TCG_POOL_CHUNK_SIZE;
                p = static_cast<TCGPool*>(malloc(sizeof(TCGPool) + pool_size));
                p->size = pool_size;
                p->next = NULL;
                if (s->pool_current)
                    s->pool_current->next = p;
                else
                    s->pool_first = p;
            }
            else {
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

void QEMUDisasContext::gen_op0(TCGOpcode opc)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
}

void QEMUDisasContext::gen_op1_i32(TCGOpcode opc, TCGv_i32 arg1)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
}

void QEMUDisasContext::gen_op1_i64(TCGOpcode opc, TCGv_i64 arg1)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
}

void QEMUDisasContext::gen_op1i(TCGOpcode opc, TCGArg arg1)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = arg1;
}

void QEMUDisasContext::gen_op2_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
}

void QEMUDisasContext::gen_op2_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
}

void QEMUDisasContext::gen_op2i_i32(TCGOpcode opc, TCGv_i32 arg1, TCGArg arg2)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = arg2;
}

void QEMUDisasContext::gen_op2i_i64(TCGOpcode opc, TCGv_i64 arg1, TCGArg arg2)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = arg2;
}

void QEMUDisasContext::gen_op2ii(TCGOpcode opc, TCGArg arg1, TCGArg arg2)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = arg1;
    *tcg_ctx.gen_opparam_ptr++ = arg2;
}

void QEMUDisasContext::gen_op3_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
    TCGv_i32 arg3)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg3);
}

void QEMUDisasContext::gen_op3_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
    TCGv_i64 arg3)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg3);
}

void QEMUDisasContext::gen_op3i_i32(TCGOpcode opc, TCGv_i32 arg1,
    TCGv_i32 arg2, TCGArg arg3)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = arg3;
}

void QEMUDisasContext::gen_op3i_i64(TCGOpcode opc, TCGv_i64 arg1,
    TCGv_i64 arg2, TCGArg arg3)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = arg3;
}

void QEMUDisasContext::gen_ldst_op_i32(TCGOpcode opc, TCGv_i32 val,
    TCGv_ptr base, TCGArg offset)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(val);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_PTR(base);
    *tcg_ctx.gen_opparam_ptr++ = offset;
}

void QEMUDisasContext::gen_ldst_op_i64(TCGOpcode opc, TCGv_i64 val,
    TCGv_ptr base, TCGArg offset)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(val);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_PTR(base);
    *tcg_ctx.gen_opparam_ptr++ = offset;
}

void QEMUDisasContext::gen_op4_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
    TCGv_i32 arg3, TCGv_i32 arg4)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg4);
}

void QEMUDisasContext::gen_op4_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
    TCGv_i64 arg3, TCGv_i64 arg4)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg4);
}

void QEMUDisasContext::gen_op4i_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
    TCGv_i32 arg3, TCGArg arg4)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg3);
    *tcg_ctx.gen_opparam_ptr++ = arg4;
}

void QEMUDisasContext::gen_op4i_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
    TCGv_i64 arg3, TCGArg arg4)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg3);
    *tcg_ctx.gen_opparam_ptr++ = arg4;
}

void QEMUDisasContext::gen_op4ii_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
    TCGArg arg3, TCGArg arg4)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = arg3;
    *tcg_ctx.gen_opparam_ptr++ = arg4;
}

void QEMUDisasContext::gen_op4ii_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
    TCGArg arg3, TCGArg arg4)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = arg3;
    *tcg_ctx.gen_opparam_ptr++ = arg4;
}

void QEMUDisasContext::gen_op5_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
    TCGv_i32 arg3, TCGv_i32 arg4, TCGv_i32 arg5)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg4);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg5);
}

void QEMUDisasContext::gen_op5_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
    TCGv_i64 arg3, TCGv_i64 arg4, TCGv_i64 arg5)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg4);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg5);
}

void QEMUDisasContext::gen_op5i_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
    TCGv_i32 arg3, TCGv_i32 arg4, TCGArg arg5)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg4);
    *tcg_ctx.gen_opparam_ptr++ = arg5;
}

void QEMUDisasContext::gen_op5i_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
    TCGv_i64 arg3, TCGv_i64 arg4, TCGArg arg5)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg4);
    *tcg_ctx.gen_opparam_ptr++ = arg5;
}

void QEMUDisasContext::gen_op5ii_i32(TCGOpcode opc, TCGv_i32 arg1,
    TCGv_i32 arg2, TCGv_i32 arg3,
    TCGArg arg4, TCGArg arg5)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg3);
    *tcg_ctx.gen_opparam_ptr++ = arg4;
    *tcg_ctx.gen_opparam_ptr++ = arg5;
}

void QEMUDisasContext::gen_op5ii_i64(TCGOpcode opc, TCGv_i64 arg1,
    TCGv_i64 arg2, TCGv_i64 arg3,
    TCGArg arg4, TCGArg arg5)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg3);
    *tcg_ctx.gen_opparam_ptr++ = arg4;
    *tcg_ctx.gen_opparam_ptr++ = arg5;
}

void QEMUDisasContext::gen_op6_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
    TCGv_i32 arg3, TCGv_i32 arg4, TCGv_i32 arg5,
    TCGv_i32 arg6)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg4);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg5);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg6);
}

void QEMUDisasContext::gen_op6_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
    TCGv_i64 arg3, TCGv_i64 arg4, TCGv_i64 arg5,
    TCGv_i64 arg6)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg4);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg5);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg6);
}

void QEMUDisasContext::gen_op6i_i32(TCGOpcode opc, TCGv_i32 arg1, TCGv_i32 arg2,
    TCGv_i32 arg3, TCGv_i32 arg4,
    TCGv_i32 arg5, TCGArg arg6)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg4);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg5);
    *tcg_ctx.gen_opparam_ptr++ = arg6;
}

void QEMUDisasContext::gen_op6i_i64(TCGOpcode opc, TCGv_i64 arg1, TCGv_i64 arg2,
    TCGv_i64 arg3, TCGv_i64 arg4,
    TCGv_i64 arg5, TCGArg arg6)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg4);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(arg5);
    *tcg_ctx.gen_opparam_ptr++ = arg6;
}

void QEMUDisasContext::gen_op6ii_i32(TCGOpcode opc, TCGv_i32 arg1,
    TCGv_i32 arg2, TCGv_i32 arg3,
    TCGv_i32 arg4, TCGArg arg5, TCGArg arg6)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opc_ptr++ = opc;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg1);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg2);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg3);
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(arg4);
    *tcg_ctx.gen_opparam_ptr++ = arg5;
    *tcg_ctx.gen_opparam_ptr++ = arg6;
}

void QEMUDisasContext::gen_op6ii_i64(TCGOpcode opc, TCGv_i64 arg1,
    TCGv_i64 arg2, TCGv_i64 arg3,
    TCGv_i64 arg4, TCGArg arg5, TCGArg arg6)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
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
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(val);
}

void QEMUDisasContext::tcg_add_param_i64(TCGv_i64 val)
{
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;
#if TCG_TARGET_REG_BITS == 32
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(TCGV_LOW(val));
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I32(TCGV_HIGH(val));
#else
    *tcg_ctx.gen_opparam_ptr++ = GET_TCGV_I64(val);
#endif
}

/**
 * find_first_bit - find the first set bit in a memory region
 * @addr: The address to start the search at
 * @size: The maximum size to search
 *
 * Returns the bit number of the first set bit.
 */
static inline unsigned long find_first_bit(const unsigned long* addr,
    unsigned long size)
{
    unsigned long result, tmp;

    for (result = 0; result < size; result += BITS_PER_LONG) {
        tmp = *addr++;
        if (tmp) {
            result += ctzl(tmp);
            return result < size ? result : size;
        }
    }
    /* Not found */
    return size;
}
static inline void tcg_temp_alloc(TCGContext* s, int n)
{
    if (n > TCG_MAX_TEMPS)
        tcg_abort();
}

int QEMUDisasContext::global_reg_new_internal(TCGType type, int reg,
    const char* name)
{
    TCGContext* s = &m_impl->m_tcgCtx;
    TCGTemp* ts;
    int idx;

#if TCG_TARGET_REG_BITS == 32
    if (type != TCG_TYPE_I32)
        tcg_abort();
#endif
    if (tcg_regset_test_reg(s->reserved_regs, reg))
        tcg_abort();
    idx = s->nb_globals;
    tcg_temp_alloc(s, s->nb_globals + 1);
    ts = &s->temps[s->nb_globals];
    ts->base_type = type;
    ts->type = type;
    ts->fixed_reg = 1;
    ts->reg = reg;
    ts->name = name;
    s->nb_globals++;
    tcg_regset_set_reg(s->reserved_regs, reg);
    return idx;
}

int QEMUDisasContext::tcg_temp_new_internal(TCGType type, int temp_local)
{
    TCGContext* s = &m_impl->m_tcgCtx;
    TCGTemp* ts;
    int idx, k;

    k = type + (temp_local ? TCG_TYPE_COUNT : 0);
    idx = find_first_bit(s->free_temps[k].l, TCG_MAX_TEMPS);
    if (idx < TCG_MAX_TEMPS) {
        /* There is already an available temp with the right type.  */
        clear_bit(idx, s->free_temps[k].l);

        ts = &s->temps[idx];
        ts->temp_allocated = 1;
        EMASSERT(ts->base_type == type);
        EMASSERT(ts->temp_local == temp_local);
    }
    else {
        idx = s->nb_temps;
#if TCG_TARGET_REG_BITS == 32
        if (type == TCG_TYPE_I64) {
            tcg_temp_alloc(s, s->nb_temps + 2);
            ts = &s->temps[s->nb_temps];
            ts->base_type = type;
            ts->type = TCG_TYPE_I32;
            ts->temp_allocated = 1;
            ts->temp_local = temp_local;
            ts->name = NULL;
            ts++;
            ts->base_type = type;
            ts->type = TCG_TYPE_I32;
            ts->temp_allocated = 1;
            ts->temp_local = temp_local;
            ts->name = NULL;
            s->nb_temps += 2;
        }
        else
#endif
        {
            tcg_temp_alloc(s, s->nb_temps + 1);
            ts = &s->temps[s->nb_temps];
            ts->base_type = type;
            ts->type = type;
            ts->temp_allocated = 1;
            ts->temp_local = temp_local;
            ts->name = NULL;
            s->nb_temps++;
        }
    }

    return idx;
}

#include "tcg-target.cpp"

static void tcg_context_init(TCGContext* s)
{
    int op, total_args, n, i;
    TCGOpDef* def;
    TCGArgConstraint* args_ct;
    int* sorted_args;
    GHashTable* helper_table;

    memset(s, 0, sizeof(*s));
    s->nb_globals = 0;

    /* Count total number of arguments and allocate the corresponding
       space */
    total_args = 0;
    for (op = 0; op < NB_OPS; op++) {
        def = &tcg_op_defs[op];
        n = def->nb_iargs + def->nb_oargs;
        total_args += n;
    }

    args_ct = (TCGArgConstraint*)malloc(sizeof(TCGArgConstraint) * total_args);
    sorted_args = (int*)malloc(sizeof(int) * total_args);

    for (op = 0; op < NB_OPS; op++) {
        def = &tcg_op_defs[op];
        def->args_ct = args_ct;
        def->sorted_args = sorted_args;
        n = def->nb_iargs + def->nb_oargs;
        sorted_args += n;
        args_ct += n;
    }

    /* Register helpers.  */
    /* Use g_direct_hash/equal for direct pointer comparisons on func.  */
    s->helpers = helper_table = g_hash_table_new(NULL, NULL);

    for (i = 0; i < ARRAY_SIZE(all_helpers); ++i) {
        g_hash_table_insert(helper_table, (gpointer)all_helpers[i].func,
            (gpointer)&all_helpers[i]);
    }

    tcg_target_init(s);
}

QEMUDisasContext::QEMUDisasContext(ExecutableMemoryAllocator* allocator)
    : m_impl(new QEMUDisasContextImpl({ allocator }))
{
}

int QEMUDisasContext::gen_new_label()
{
    TCGContext* s = &m_impl->m_tcgCtx;
    qemu::gen_new_label(s);
}

void QEMUDisasContext::gen_set_label(int n)
{
    gen_op1i(INDEX_op_set_label, n);
}

int QEMUDisasContext::global_mem_new_internal(TCGType type, int reg,
    intptr_t offset,
    const char* name)
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
        strncpy(buf, name, sizeof(buf));
        strncat(buf, "_0", sizeof(buf));
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
#define pstrcpy(a, b, c) strncpy(a, c, b)
#define pstrcat(a, b, c) strncat(a, c, b)
        pstrcpy(buf, sizeof(buf), name);
        pstrcat(buf, sizeof(buf), "_1");
        ts->name = strdup(buf);

        s->nb_globals += 2;
    }
    else
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
    int idx = global_mem_new_internal(TCG_TYPE_I64, reg, offset, name);
    return MAKE_TCGV_I64(idx);
}

TCGv_i32 QEMUDisasContext::global_mem_new_i32(int reg, intptr_t offset, const char* name)
{
    int idx = global_mem_new_internal(TCG_TYPE_I32, reg, offset, name);
    return MAKE_TCGV_I32(idx);
}

TCGv_i32 QEMUDisasContext::global_reg_new_i32(int reg, const char* name)
{
    int idx;

    idx = global_reg_new_internal(TCG_TYPE_I32, reg, name);
    return MAKE_TCGV_I32(idx);
}

TCGv_ptr QEMUDisasContext::global_reg_new_ptr(int reg, const char* name)
{
#if UINTPTR_MAX == UINT32_MAX
    return TCGV_NAT_TO_PTR(global_reg_new_i32(reg, name));
#else
    return TCGV_NAT_TO_PTR(global_reg_new_i64(reg, name));
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
        gen_op6_i32(INDEX_op_add2_i32, rl, rh, al, ah, bl, bh);
        /* Allow the optimizer room to replace add2 with two moves.  */
        gen_op0(INDEX_op_nop);
    }
    else {
        EMUNREACHABLE();
    }
}

void QEMUDisasContext::gen_add_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    gen_op3_i32(INDEX_op_add_i32, ret, arg1, arg2);
}

void QEMUDisasContext::gen_add_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
    gen_op6_i32(INDEX_op_add2_i32, TCGV_LOW(ret), TCGV_HIGH(ret),
        TCGV_LOW(arg1), TCGV_HIGH(arg1), TCGV_LOW(arg2),
        TCGV_HIGH(arg2));
    /* Allow the optimizer room to replace add2 with two moves.  */
    gen_op0(INDEX_op_nop);
}

void QEMUDisasContext::gen_addi_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    /* some cases can be optimized here */
    if (arg2 == 0) {
        gen_mov_i32(ret, arg1);
    }
    else {
        TCGv_i32 t0 = const_i32(arg2);
        gen_add_i32(ret, arg1, t0);
        temp_free_i32(t0);
    }
}

void QEMUDisasContext::gen_addi_ptr(TCGv_ptr ret, TCGv_ptr arg1, int32_t arg2)
{
#if UINTPTR_MAX == UINT32_MAX
    gen_addi_i32(TCGV_PTR_TO_NAT(ret), TCGV_PTR_TO_NAT(arg1), (arg2));
#else
    gen_addi_i64(TCGV_PTR_TO_NAT(ret), TCGV_PTR_TO_NAT(arg1), (arg2));
#endif
}
void QEMUDisasContext::gen_addi_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
    /* some cases can be optimized here */
    if (arg2 == 0) {
        gen_mov_i64(ret, arg1);
    }
    else {
        TCGv_i64 t0 = const_i64(arg2);
        gen_add_i64(ret, arg1, t0);
        temp_free_i64(t0);
    }
}

void QEMUDisasContext::gen_andc_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    if (TCG_TARGET_HAS_andc_i32) {
        gen_op3_i32(INDEX_op_andc_i32, ret, arg1, arg2);
    }
    else {
        TCGv_i32 t0 = temp_new_i32();
        gen_not_i32(t0, arg2);
        gen_and_i32(ret, arg1, t0);
        temp_free_i32(t0);
    }
}

void QEMUDisasContext::gen_and_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    if (TCGV_EQUAL_I32(arg1, arg2)) {
        gen_mov_i32(ret, arg1);
    }
    else {
        gen_op3_i32(INDEX_op_and_i32, ret, arg1, arg2);
    }
}

void QEMUDisasContext::gen_and_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
#if TCG_TARGET_REG_BITS == 32
    gen_and_i32(TCGV_LOW(ret), TCGV_LOW(arg1), TCGV_LOW(arg2));
    gen_and_i32(TCGV_HIGH(ret), TCGV_HIGH(arg1), TCGV_HIGH(arg2));
#else
    if (TCGV_EQUAL_I64(arg1, arg2)) {
        gen_mov_i64(ret, arg1);
    }
    else {
        gen_op3_i64(INDEX_op_and_i64, ret, arg1, arg2);
    }
#endif
}

void QEMUDisasContext::gen_andi_i32(TCGv_i32 ret, TCGv_i32 arg1, uint32_t arg2)
{
    TCGv_i32 t0;
    /* Some cases can be optimized here.  */
    switch (arg2) {
    case 0:
        gen_movi_i32(ret, 0);
        return;
    case 0xffffffffu:
        gen_mov_i32(ret, arg1);
        return;
    case 0xffu:
        /* Don't recurse with gen_ext8u_i32.  */
        if (TCG_TARGET_HAS_ext8u_i32) {
            gen_op2_i32(INDEX_op_ext8u_i32, ret, arg1);
            return;
        }
        break;
    case 0xffffu:
        if (TCG_TARGET_HAS_ext16u_i32) {
            gen_op2_i32(INDEX_op_ext16u_i32, ret, arg1);
            return;
        }
        break;
    }
    t0 = const_i32(arg2);
    gen_and_i32(ret, arg1, t0);
    temp_free_i32(t0);
}

void QEMUDisasContext::gen_andi_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
#if TCG_TARGET_REG_BITS == 32
    gen_andi_i32(TCGV_LOW(ret), TCGV_LOW(arg1), arg2);
    gen_andi_i32(TCGV_HIGH(ret), TCGV_HIGH(arg1), arg2 >> 32);

#else

    TCGv_i64 t0;
    /* Some cases can be optimized here.  */
    switch (arg2) {
    case 0:
        gen_movi_i64(ret, 0);
        return;
    case 0xffffffffffffffffull:
        gen_mov_i64(ret, arg1);
        return;
    case 0xffull:
        /* Don't recurse with gen_ext8u_i32.  */
        if (TCG_TARGET_HAS_ext8u_i64) {
            gen_op2_i64(INDEX_op_ext8u_i64, ret, arg1);
            return;
        }
        break;
    case 0xffffu:
        if (TCG_TARGET_HAS_ext16u_i64) {
            gen_op2_i64(INDEX_op_ext16u_i64, ret, arg1);
            return;
        }
        break;
    case 0xffffffffull:
        if (TCG_TARGET_HAS_ext32u_i64) {
            gen_op2_i64(INDEX_op_ext32u_i64, ret, arg1);
            return;
        }
        break;
    }
    t0 = const_i64(arg2);
    gen_and_i64(ret, arg1, t0);
    temp_free_i64(t0);
#endif
}

void QEMUDisasContext::gen_br(int label)
{
    gen_op1i(INDEX_op_br, label);
}

void QEMUDisasContext::gen_brcond_i32(TCGCond cond, TCGv_i32 arg1,
    TCGv_i32 arg2, int label_index)
{
    if (cond == TCG_COND_ALWAYS) {
        gen_br(label_index);
    }
    else if (cond != TCG_COND_NEVER) {
        gen_op4ii_i32(INDEX_op_brcond_i32, arg1, arg2, cond, label_index);
    }
}

void QEMUDisasContext::gen_brcondi_i32(TCGCond cond, TCGv_i32 arg1,
    int32_t arg2, int label_index)

{
    if (cond == TCG_COND_ALWAYS) {
        gen_br(label_index);
    }
    else if (cond != TCG_COND_NEVER) {
        TCGv_i32 t0 = const_i32(arg2);
        gen_brcond_i32(cond, arg1, t0, label_index);
        temp_free_i32(t0);
    }
}
void QEMUDisasContext::gen_bswap16_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    if (TCG_TARGET_HAS_bswap16_i32) {
        gen_op2_i32(INDEX_op_bswap16_i32, ret, arg);
    }
    else {
        EMUNREACHABLE();
    }
}

void QEMUDisasContext::gen_bswap32_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    if (TCG_TARGET_HAS_bswap32_i32) {
        gen_op2_i32(INDEX_op_bswap32_i32, ret, arg);
    }
    else {
        EMUNREACHABLE();
    }
}
void QEMUDisasContext::gen_concat_i32_i64(TCGv_i64 dest, TCGv_i32 low,
    TCGv_i32 high)

{
#if TCG_TARGET_REG_BITS == 32
    gen_mov_i32(TCGV_LOW(dest), low);
    gen_mov_i32(TCGV_HIGH(dest), high);
#else
    TCGv_i64 tmp = temp_new_i64();
    /* These extensions are only needed for type correctness.
       We may be able to do better given target specific information.  */
    gen_extu_i32_i64(tmp, high);
    gen_extu_i32_i64(dest, low);
    /* If deposit is available, use it.  Otherwise use the extra
       knowledge that we have of the zero-extensions above.  */
    if (TCG_TARGET_HAS_deposit_i64 && TCG_TARGET_deposit_i64_valid(32, 32)) {
        gen_deposit_i64(dest, dest, tmp, 32, 32);
    }
    else {
        gen_shli_i64(tmp, tmp, 32);
        gen_or_i64(dest, dest, tmp);
    }
    temp_free_i64(tmp);
#endif
}

void QEMUDisasContext::gen_deposit_i32(TCGv_i32 ret, TCGv_i32 arg1,
    TCGv_i32 arg2, unsigned int ofs,
    unsigned int len)
{
    uint32_t mask;
    TCGv_i32 t1;

    EMASSERT(ofs < 32);
    EMASSERT(len <= 32);
    EMASSERT(ofs + len <= 32);

    if (ofs == 0 && len == 32) {
        gen_mov_i32(ret, arg2);
        return;
    }
    if (TCG_TARGET_HAS_deposit_i32 && TCG_TARGET_deposit_i32_valid(ofs, len)) {
        gen_op5ii_i32(INDEX_op_deposit_i32, ret, arg1, arg2, ofs, len);
        return;
    }

    mask = (1u << len) - 1;
    t1 = temp_new_i32();

    if (ofs + len < 32) {
        gen_andi_i32(t1, arg2, mask);
        gen_shli_i32(t1, t1, ofs);
    }
    else {
        gen_shli_i32(t1, arg2, ofs);
    }
    gen_andi_i32(ret, arg1, ~(mask << ofs));
    gen_or_i32(ret, ret, t1);

    temp_free_i32(t1);
}

void QEMUDisasContext::gen_mov_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    if (!TCGV_EQUAL_I32(ret, arg))
        gen_op2_i32(INDEX_op_mov_i32, ret, arg);
}

void QEMUDisasContext::gen_exit_tb(int direct)
{
    gen_op1i(INDEX_op_exit_tb, direct);
}

void QEMUDisasContext::gen_ext16s_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    if (TCG_TARGET_HAS_ext16s_i32) {
        gen_op2_i32(INDEX_op_ext16s_i32, ret, arg);
    }
    else {
        gen_shli_i32(ret, arg, 16);
        gen_sari_i32(ret, ret, 16);
    }
}

void QEMUDisasContext::gen_ext16u_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    if (TCG_TARGET_HAS_ext16u_i32) {
        gen_op2_i32(INDEX_op_ext16u_i32, ret, arg);
    }
    else {
        gen_andi_i32(ret, arg, 0xffffu);
    }
}

void QEMUDisasContext::gen_ext32u_i64(TCGv_i64 ret, TCGv_i64 arg)
{
#if TCG_TARGET_REG_BITS == 32
    gen_mov_i32(TCGV_LOW(ret), TCGV_LOW(arg));
    gen_movi_i32(TCGV_HIGH(ret), 0);
#else
    if (TCG_TARGET_HAS_ext32u_i64) {
        gen_op2_i64(INDEX_op_ext32u_i64, ret, arg);
    }
    else {
        gen_andi_i64(ret, arg, 0xffffffffu);
    }
#endif
}

void QEMUDisasContext::gen_ext8s_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    if (TCG_TARGET_HAS_ext8s_i32) {
        gen_op2_i32(INDEX_op_ext8s_i32, ret, arg);
    }
    else {
        gen_shli_i32(ret, arg, 24);
        gen_sari_i32(ret, ret, 24);
    }
}

void QEMUDisasContext::gen_ext8u_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    if (TCG_TARGET_HAS_ext8u_i32) {
        gen_op2_i32(INDEX_op_ext8u_i32, ret, arg);
    }
    else {
        gen_andi_i32(ret, arg, 0xffu);
    }
}

void QEMUDisasContext::gen_ext_i32_i64(TCGv_i64 ret, TCGv_i32 arg)
{
#if TCG_TARGET_REG_BITS == 32
    gen_mov_i32(TCGV_LOW(ret), arg);
    gen_sari_i32(TCGV_HIGH(ret), TCGV_LOW(ret), 31);
#else
#error not supported yet
#endif
}

void QEMUDisasContext::gen_extu_i32_i64(TCGv_i64 ret, TCGv_i32 arg)
{
#if TCG_TARGET_REG_BITS == 32
    gen_mov_i32(TCGV_LOW(ret), arg);
    gen_movi_i32(TCGV_HIGH(ret), 0);
#else
    gen_ext32u_i64(ret, MAKE_TCGV_I64(GET_TCGV_I32(arg)));
#endif
}

void QEMUDisasContext::gen_ld_i32(TCGv_i32 ret, TCGv_ptr arg2, tcg_target_long offset)
{
    gen_ldst_op_i32(INDEX_op_ld_i32, ret, arg2, offset);
}

void QEMUDisasContext::gen_ld_i64(TCGv_i64 ret, TCGv_ptr arg2,
    target_long offset)
{
    gen_ld_i32(TCGV_LOW(ret), arg2, offset);
    gen_ld_i32(TCGV_HIGH(ret), arg2, offset + 4);
}

void QEMUDisasContext::gen_movcond_i32(TCGCond cond, TCGv_i32 ret,
    TCGv_i32 c1, TCGv_i32 c2,
    TCGv_i32 v1, TCGv_i32 v2)
{
    if (TCG_TARGET_HAS_movcond_i32) {
        gen_op6i_i32(INDEX_op_movcond_i32, ret, c1, c2, v1, v2, cond);
    }
    else {
        EMUNREACHABLE();
    }
}

void QEMUDisasContext::gen_movcond_i64(TCGCond cond, TCGv_i64 ret,
    TCGv_i64 c1, TCGv_i64 c2,
    TCGv_i64 v1, TCGv_i64 v2)
{
#if TCG_TARGET_REG_BITS == 32
    TCGv_i32 t0 = temp_new_i32();
    TCGv_i32 t1 = temp_new_i32();
    gen_op6i_i32(INDEX_op_setcond2_i32, t0,
        TCGV_LOW(c1), TCGV_HIGH(c1),
        TCGV_LOW(c2), TCGV_HIGH(c2), cond);

    if (TCG_TARGET_HAS_movcond_i32) {
        gen_movi_i32(t1, 0);
        gen_movcond_i32(TCG_COND_NE, TCGV_LOW(ret), t0, t1,
            TCGV_LOW(v1), TCGV_LOW(v2));
        gen_movcond_i32(TCG_COND_NE, TCGV_HIGH(ret), t0, t1,
            TCGV_HIGH(v1), TCGV_HIGH(v2));
    }
    else {
        gen_neg_i32(t0, t0);

        gen_and_i32(t1, TCGV_LOW(v1), t0);
        gen_andc_i32(TCGV_LOW(ret), TCGV_LOW(v2), t0);
        gen_or_i32(TCGV_LOW(ret), TCGV_LOW(ret), t1);

        gen_and_i32(t1, TCGV_HIGH(v1), t0);
        gen_andc_i32(TCGV_HIGH(ret), TCGV_HIGH(v2), t0);
        gen_or_i32(TCGV_HIGH(ret), TCGV_HIGH(ret), t1);
    }
    temp_free_i32(t0);
    temp_free_i32(t1);
#else
    if (TCG_TARGET_HAS_movcond_i64) {
        gen_op6i_i64(INDEX_op_movcond_i64, ret, c1, c2, v1, v2, cond);
    }
    else {
        EMUNREACHABLE();
    }
#endif
}

void QEMUDisasContext::gen_mov_i64(TCGv_i64 ret, TCGv_i64 arg)
{
#if TCG_TARGET_REG_BITS == 32
    if (!TCGV_EQUAL_I64(ret, arg)) {
        gen_mov_i32(TCGV_LOW(ret), TCGV_LOW(arg));
        gen_mov_i32(TCGV_HIGH(ret), TCGV_HIGH(arg));
    }
#else
    if (!TCGV_EQUAL_I64(ret, arg))
        gen_op2_i64(INDEX_op_mov_i64, ret, arg);
#endif
}

void QEMUDisasContext::gen_movi_i32(TCGv_i32 ret, int32_t arg)
{
    gen_op2i_i32(INDEX_op_movi_i32, ret, arg);
}

void QEMUDisasContext::gen_movi_i64(TCGv_i64 ret, int64_t arg)
{
#if TCG_TARGET_REG_BITS == 32
    gen_movi_i32(TCGV_LOW(ret), arg);
    gen_movi_i32(TCGV_HIGH(ret), arg >> 32);
#else
    gen_op2i_i64(INDEX_op_movi_i64, ret, arg);
#endif
}

void QEMUDisasContext::gen_mul_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    gen_op3_i32(INDEX_op_mul_i32, ret, arg1, arg2);
}

void QEMUDisasContext::gen_muls2_i32(TCGv_i32 rl, TCGv_i32 rh,
    TCGv_i32 arg1, TCGv_i32 arg2)

{
    if (TCG_TARGET_HAS_muls2_i32) {
        gen_op4_i32(INDEX_op_muls2_i32, rl, rh, arg1, arg2);
        /* Allow the optimizer room to replace muls2 with two moves.  */
        gen_op0(INDEX_op_nop);
    }
    else {
        EMUNREACHABLE();
    }
}

void QEMUDisasContext::gen_mulu2_i32(TCGv_i32 rl, TCGv_i32 rh,
    TCGv_i32 arg1, TCGv_i32 arg2)
{
    if (TCG_TARGET_HAS_mulu2_i32) {
        gen_op4_i32(INDEX_op_mulu2_i32, rl, rh, arg1, arg2);
        /* Allow the optimizer room to replace mulu2 with two moves.  */
        gen_op0(INDEX_op_nop);
    }
    else {
        EMUNREACHABLE();
    }
}

void QEMUDisasContext::gen_neg_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    if (TCG_TARGET_HAS_neg_i32) {
        gen_op2_i32(INDEX_op_neg_i32, ret, arg);
    }
    else {
        TCGv_i32 t0 = const_i32(0);
        gen_sub_i32(ret, t0, arg);
        temp_free_i32(t0);
    }
}

void QEMUDisasContext::gen_neg_i64(TCGv_i64 ret, TCGv_i64 arg)
{
    if (TCG_TARGET_HAS_neg_i64) {
        gen_op2_i64(INDEX_op_neg_i64, ret, arg);
    }
    else {
        TCGv_i64 t0 = const_i64(0);
        gen_sub_i64(ret, t0, arg);
        temp_free_i64(t0);
    }
}

void QEMUDisasContext::gen_not_i32(TCGv_i32 ret, TCGv_i32 arg)
{
    if (TCG_TARGET_HAS_not_i32) {
        gen_op2_i32(INDEX_op_not_i32, ret, arg);
    }
    else {
        gen_xori_i32(ret, arg, -1);
    }
}

void QEMUDisasContext::gen_orc_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    if (TCG_TARGET_HAS_orc_i32) {
        gen_op3_i32(INDEX_op_orc_i32, ret, arg1, arg2);
    }
    else {
        TCGv_i32 t0 = temp_new_i32();
        gen_not_i32(t0, arg2);
        gen_or_i32(ret, arg1, t0);
        temp_free_i32(t0);
    }
}

void QEMUDisasContext::gen_or_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    if (TCGV_EQUAL_I32(arg1, arg2)) {
        gen_mov_i32(ret, arg1);
    }
    else {
        gen_op3_i32(INDEX_op_or_i32, ret, arg1, arg2);
    }
}

void QEMUDisasContext::gen_or_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
#if TCG_TARGET_REG_BITS == 32
    gen_or_i32(TCGV_LOW(ret), TCGV_LOW(arg1), TCGV_LOW(arg2));
    gen_or_i32(TCGV_HIGH(ret), TCGV_HIGH(arg1), TCGV_HIGH(arg2));
#else
    if (TCGV_EQUAL_I64(arg1, arg2)) {
        gen_mov_i64(ret, arg1);
    }
    else {
        gen_op3_i64(INDEX_op_or_i64, ret, arg1, arg2);
    }
#endif
}

void QEMUDisasContext::gen_ori_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    /* Some cases can be optimized here.  */
    if (arg2 == -1) {
        gen_movi_i32(ret, -1);
    }
    else if (arg2 == 0) {
        gen_mov_i32(ret, arg1);
    }
    else {
        TCGv_i32 t0 = const_i32(arg2);
        gen_or_i32(ret, arg1, t0);
        temp_free_i32(t0);
    }
}

void QEMUDisasContext::gen_qemu_ld_i32(TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
    memop = tcg_canonicalize_memop(memop, 0, 0);

    TCGContext& tcg_ctx = m_impl->m_tcgCtx;

    *tcg_ctx.gen_opc_ptr++ = INDEX_op_qemu_ld_i32;
    tcg_add_param_i32(val);
    tcg_add_param_tl(addr);
    *tcg_ctx.gen_opparam_ptr++ = memop;
    *tcg_ctx.gen_opparam_ptr++ = idx;
}
void QEMUDisasContext::gen_qemu_ld_i64(TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
    memop = tcg_canonicalize_memop(memop, 1, 0);

    TCGContext& tcg_ctx = m_impl->m_tcgCtx;

#if TCG_TARGET_REG_BITS == 32
    if ((memop & MO_SIZE) < MO_64) {
        gen_qemu_ld_i32(TCGV_LOW(val), addr, idx, memop);
        if (memop & MO_SIGN) {
            gen_sari_i32(TCGV_HIGH(val), TCGV_LOW(val), 31);
        }
        else {
            gen_movi_i32(TCGV_HIGH(val), 0);
        }
        return;
    }
#endif

    *tcg_ctx.gen_opc_ptr++ = INDEX_op_qemu_ld_i64;
    tcg_add_param_i64(val);
    tcg_add_param_tl(addr);
    *tcg_ctx.gen_opparam_ptr++ = memop;
    *tcg_ctx.gen_opparam_ptr++ = idx;
}

void QEMUDisasContext::gen_qemu_st_i32(TCGv_i32 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
    memop = tcg_canonicalize_memop(memop, 0, 1);
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;

    *tcg_ctx.gen_opc_ptr++ = INDEX_op_qemu_st_i32;
    tcg_add_param_i32(val);
    tcg_add_param_tl(addr);
    *tcg_ctx.gen_opparam_ptr++ = memop;
    *tcg_ctx.gen_opparam_ptr++ = idx;
}

void QEMUDisasContext::gen_qemu_st_i64(TCGv_i64 val, TCGv addr, TCGArg idx, TCGMemOp memop)
{
    memop = tcg_canonicalize_memop(memop, 1, 1);
    TCGContext& tcg_ctx = m_impl->m_tcgCtx;

#if TCG_TARGET_REG_BITS == 32
    if ((memop & MO_SIZE) < MO_64) {
        gen_qemu_st_i32(TCGV_LOW(val), addr, idx, memop);
        return;
    }
#endif

    *tcg_ctx.gen_opc_ptr++ = INDEX_op_qemu_st_i64;
    tcg_add_param_i64(val);
    tcg_add_param_tl(addr);
    *tcg_ctx.gen_opparam_ptr++ = memop;
    *tcg_ctx.gen_opparam_ptr++ = idx;
}

void QEMUDisasContext::gen_rotr_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    if (TCG_TARGET_HAS_rot_i32) {
        gen_op3_i32(INDEX_op_rotr_i32, ret, arg1, arg2);
    }
    else {
        EMUNREACHABLE();
    }
}

void QEMUDisasContext::gen_rotri_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    /* some cases can be optimized here */
    if (arg2 == 0) {
        gen_mov_i32(ret, arg1);
    }
    else {
        if (arg2 == 0) {
            gen_mov_i32(ret, arg1);
        }
        else if (TCG_TARGET_HAS_rot_i32) {
            TCGv_i32 t0 = tcg_const_i32(arg2);
            gen_op3_i32(INDEX_op_rotl_i32, ret, arg1, (TCGv_i32)arg2);
            tcg_temp_free_i32(t0);
        }
        else {
            EMUNREACHABLE();
        }
    }
}

void QEMUDisasContext::gen_sar_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    gen_op3_i32(INDEX_op_sar_i32, ret, arg1, arg2);
}

void QEMUDisasContext::gen_sari_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    if (arg2 == 0) {
        gen_mov_i32(ret, arg1);
    }
    else {
        TCGv_i32 t0 = tcg_const_i32(arg2);
        gen_sar_i32(ret, arg1, t0);
        tcg_temp_free_i32(t0);
    }
}

void QEMUDisasContext::gen_setcond_i32(TCGCond cond, TCGv_i32 ret,
    TCGv_i32 arg1, TCGv_i32 arg2)
{
    if (cond == TCG_COND_ALWAYS) {
        gen_movi_i32(ret, 1);
    }
    else if (cond == TCG_COND_NEVER) {
        gen_movi_i32(ret, 0);
    }
    else {
        gen_op4i_i32(INDEX_op_setcond_i32, ret, arg1, arg2, cond);
    }
}

void QEMUDisasContext::gen_shl_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    gen_op3_i32(INDEX_op_shl_i32, ret, arg1, arg2);
}

void QEMUDisasContext::gen_shli_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    if (arg2 == 0) {
        gen_mov_i32(ret, arg1);
    }
    else {
        TCGv_i32 t0 = const_i32(arg2);
        gen_shl_i32(ret, arg1, t0);
        temp_free_i32(t0);
    }
}

#if TCG_TARGET_REG_BITS == 32
void QEMUDisasContext::gen_shifti_i64(TCGv_i64 ret, TCGv_i64 arg1,
    int c, int right, int arith)
{
    if (c == 0) {
        gen_mov_i32(TCGV_LOW(ret), TCGV_LOW(arg1));
        gen_mov_i32(TCGV_HIGH(ret), TCGV_HIGH(arg1));
    }
    else if (c >= 32) {
        c -= 32;
        if (right) {
            if (arith) {
                gen_sari_i32(TCGV_LOW(ret), TCGV_HIGH(arg1), c);
                gen_sari_i32(TCGV_HIGH(ret), TCGV_HIGH(arg1), 31);
            }
            else {
                gen_shri_i32(TCGV_LOW(ret), TCGV_HIGH(arg1), c);
                gen_movi_i32(TCGV_HIGH(ret), 0);
            }
        }
        else {
            gen_shli_i32(TCGV_HIGH(ret), TCGV_LOW(arg1), c);
            gen_movi_i32(TCGV_LOW(ret), 0);
        }
    }
    else {
        TCGv_i32 t0, t1;

        t0 = temp_new_i32();
        t1 = temp_new_i32();
        if (right) {
            gen_shli_i32(t0, TCGV_HIGH(arg1), 32 - c);
            if (arith)
                gen_sari_i32(t1, TCGV_HIGH(arg1), c);
            else
                gen_shri_i32(t1, TCGV_HIGH(arg1), c);
            gen_shri_i32(TCGV_LOW(ret), TCGV_LOW(arg1), c);
            gen_or_i32(TCGV_LOW(ret), TCGV_LOW(ret), t0);
            gen_mov_i32(TCGV_HIGH(ret), t1);
        }
        else {
            gen_shri_i32(t0, TCGV_LOW(arg1), 32 - c);
            /* Note: ret can be the same as arg1, so we use t1 */
            gen_shli_i32(t1, TCGV_LOW(arg1), c);
            gen_shli_i32(TCGV_HIGH(ret), TCGV_HIGH(arg1), c);
            gen_or_i32(TCGV_HIGH(ret), TCGV_HIGH(ret), t0);
            gen_mov_i32(TCGV_LOW(ret), t1);
        }
        temp_free_i32(t0);
        temp_free_i32(t1);
    }
}
#endif

void QEMUDisasContext::gen_shli_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
#if TCG_TARGET_REG_BITS == 32
    gen_shifti_i64(ret, arg1, arg2, 0, 0);
#else
#error unsupported yet
#endif
}

void QEMUDisasContext::gen_shr_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    gen_op3_i32(INDEX_op_shr_i32, ret, arg1, arg2);
}

void QEMUDisasContext::gen_shri_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    if (arg2 == 0) {
        gen_mov_i32(ret, arg1);
    }
    else {
        TCGv_i32 t0 = const_i32(arg2);
        gen_shr_i32(ret, arg1, t0);
        temp_free_i32(t0);
    }
}

void QEMUDisasContext::gen_shri_i64(TCGv_i64 ret, TCGv_i64 arg1, int64_t arg2)
{
#if TCG_TARGET_REG_BITS == 32
    gen_shifti_i64(ret, arg1, arg2, 1, 0);
#else
#error unsupported yet
#endif
}

void QEMUDisasContext::gen_st_i32(TCGv_i32 arg1, TCGv_ptr arg2, tcg_target_long offset)
{
    gen_ldst_op_i32(INDEX_op_st_i32, arg1, arg2, offset);
}

void QEMUDisasContext::gen_st_i64(TCGv_i64 arg1, TCGv_ptr arg2,
    target_long offset)
{
#if TCG_TARGET_REG_BITS == 32
    gen_st_i32(TCGV_LOW(arg1), arg2, offset);
    gen_st_i32(TCGV_HIGH(arg1), arg2, offset + 4);
#else
    gen_ldst_op_i64(INDEX_op_st_i64, arg1, arg2, offset);
#endif
}

void QEMUDisasContext::gen_sub_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    gen_op3_i32(INDEX_op_sub_i32, ret, arg1, arg2);
}

void QEMUDisasContext::gen_sub_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
#if TCG_TARGET_REG_BITS == 32
    gen_op6_i32(INDEX_op_sub2_i32, TCGV_LOW(ret), TCGV_HIGH(ret),
        TCGV_LOW(arg1), TCGV_HIGH(arg1), TCGV_LOW(arg2),
        TCGV_HIGH(arg2));
    /* Allow the optimizer room to replace sub2 with two moves.  */
    gen_op0(INDEX_op_nop);
#else
    gen_op3_i64(INDEX_op_sub_i64, ret, arg1, arg2);
#endif
}
void QEMUDisasContext::gen_subi_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    /* some cases can be optimized here */
    if (arg2 == 0) {
        gen_mov_i32(ret, arg1);
    }
    else {
        TCGv_i32 t0 = const_i32(arg2);
        gen_sub_i32(ret, arg1, t0);
        temp_free_i32(t0);
    }
}

void QEMUDisasContext::gen_trunc_shr_i64_i32(TCGv_i32 ret, TCGv_i64 arg,
    unsigned int count)
{
    EMASSERT(count < 64);
    if (count >= 32) {
        gen_shri_i32(ret, TCGV_HIGH(arg), count - 32);
    }
    else if (count == 0) {
        gen_mov_i32(ret, TCGV_LOW(arg));
    }
    else {
        TCGv_i64 t = tcg_temp_new_i64();
        gen_shri_i64(t, arg, count);
        gen_mov_i32(ret, TCGV_LOW(t));
        tcg_temp_free_i64(t);
    }
}

void QEMUDisasContext::gen_trunc_i64_i32(TCGv_i32 ret, TCGv_i64 arg)
{
    gen_trunc_shr_i64_i32(ret, arg, 0);
}

void QEMUDisasContext::gen_xor_i32(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    if (TCGV_EQUAL_I32(arg1, arg2)) {
        gen_movi_i32(ret, 0);
    }
    else {
        gen_op3_i32(INDEX_op_xor_i32, ret, arg1, arg2);
    }
}

void QEMUDisasContext::gen_xor_i64(TCGv_i64 ret, TCGv_i64 arg1, TCGv_i64 arg2)
{
#if TCG_TARGET_REG_BITS == 32
    gen_xor_i32(TCGV_LOW(ret), TCGV_LOW(arg1), TCGV_LOW(arg2));
    gen_xor_i32(TCGV_HIGH(ret), TCGV_HIGH(arg1), TCGV_HIGH(arg2));
#else
    if (TCGV_EQUAL_I64(arg1, arg2)) {
        gen_movi_i64(ret, 0);
    }
    else {
        gen_op3_i64(INDEX_op_xor_i64, ret, arg1, arg2);
    }
#endif
}

void QEMUDisasContext::gen_xori_i32(TCGv_i32 ret, TCGv_i32 arg1, int32_t arg2)
{
    /* Some cases can be optimized here.  */
    if (arg2 == 0) {
        gen_mov_i32(ret, arg1);
    }
    else if (arg2 == -1 && TCG_TARGET_HAS_not_i32) {
        /* Don't recurse with gen_not_i32.  */
        gen_op2_i32(INDEX_op_not_i32, ret, arg1);
    }
    else {
        TCGv_i32 t0 = const_i32(arg2);
        gen_xor_i32(ret, arg1, t0);
        temp_free_i32(t0);
    }
}

TCGv_i32 QEMUDisasContext::tcg_temp_new_internal_i32(int temp_local)
{
    int idx;

    idx = tcg_temp_new_internal(TCG_TYPE_I32, temp_local);
    return MAKE_TCGV_I32(idx);
}

TCGv_i64 QEMUDisasContext::tcg_temp_new_internal_i64(int temp_local)
{
    int idx;

    idx = tcg_temp_new_internal(TCG_TYPE_I64, temp_local);
    return MAKE_TCGV_I64(idx);
}

TCGv_i32 QEMUDisasContext::temp_local_new_i32()
{
    return tcg_temp_new_internal_i32(1);
}

TCGv_i32 QEMUDisasContext::temp_new_i32()
{
    return tcg_temp_new_internal_i32(0);
}

TCGv_ptr QEMUDisasContext::temp_new_ptr()
{
#if UINTPTR_MAX == UINT32_MAX
    return TCGV_NAT_TO_PTR(temp_new_i32());
#else
    return TCGV_NAT_TO_PTR(temp_new_i64());
#endif
}

TCGv_i64 QEMUDisasContext::temp_new_i64()
{
    return tcg_temp_new_internal_i64(0);
}

void QEMUDisasContext::gen_callN(void* func, TCGArg ret,
    int nargs, TCGArg* args)
{
    int i, real_args, nb_rets;
    unsigned sizemask, flags;
    TCGArg* nparam;
    TCGHelperInfo* info;

    TCGContext* s = &m_impl->m_tcgCtx;
    info = static_cast<TCGHelperInfo*>(g_hash_table_lookup(s->helpers, (gpointer)func));
    flags = info->flags;
    sizemask = info->sizemask;

    *s->gen_opc_ptr++ = INDEX_op_call;
    nparam = s->gen_opparam_ptr++;
    if (ret != TCG_CALL_DUMMY_ARG) {
        if (TCG_TARGET_REG_BITS < 64 && (sizemask & 1)) {
#ifdef HOST_WORDS_BIGENDIAN
            *s->gen_opparam_ptr++ = ret + 1;
            *s->gen_opparam_ptr++ = ret;
#else
            *s->gen_opparam_ptr++ = ret;
            *s->gen_opparam_ptr++ = ret + 1;
#endif
            nb_rets = 2;
        }
        else {
            *s->gen_opparam_ptr++ = ret;
            nb_rets = 1;
        }
    }
    else {
        nb_rets = 0;
    }
    real_args = 0;
    for (i = 0; i < nargs; i++) {
        int is_64bit = sizemask & (1 << (i + 1) * 2);
        if (TCG_TARGET_REG_BITS < 64 && is_64bit) {
#ifdef TCG_TARGET_CALL_ALIGN_ARGS
            /* some targets want aligned 64 bit args */
            if (real_args & 1) {
                *s->gen_opparam_ptr++ = TCG_CALL_DUMMY_ARG;
                real_args++;
            }
#endif
/* If stack grows up, then we will be placing successive
	       arguments at lower addresses, which means we need to
	       reverse the order compared to how we would normally
	       treat either big or little-endian.  For those arguments
	       that will wind up in registers, this still works for
	       HPPA (the only current STACK_GROWSUP target) since the
	       argument registers are *also* allocated in decreasing
	       order.  If another such target is added, this logic may
	       have to get more complicated to differentiate between
	       stack arguments and register arguments.  */
#if defined(HOST_WORDS_BIGENDIAN) != defined(TCG_TARGET_STACK_GROWSUP)
            *s->gen_opparam_ptr++ = args[i] + 1;
            *s->gen_opparam_ptr++ = args[i];
#else
            *s->gen_opparam_ptr++ = args[i];
            *s->gen_opparam_ptr++ = args[i] + 1;
#endif
            real_args += 2;
            continue;
        }

        *s->gen_opparam_ptr++ = args[i];
        real_args++;
    }
    *s->gen_opparam_ptr++ = (uintptr_t)func;
    *s->gen_opparam_ptr++ = flags;

    *nparam = (nb_rets << 16) | real_args;

    /* total parameters, needed to go backward in the instruction stream */
    *s->gen_opparam_ptr++ = 1 + nb_rets + real_args + 3;
}

void QEMUDisasContext::temp_free_internal(int idx)
{
    TCGContext* s = &m_impl->m_tcgCtx;
    TCGTemp* ts;
    int k;

#if defined(CONFIG_DEBUG_TCG)
    s->temps_in_use--;
    if (s->temps_in_use < 0) {
        fprintf(stderr, "More temporaries freed than allocated!\n");
    }
#endif

    EMASSERT(idx >= s->nb_globals && idx < s->nb_temps);
    ts = &s->temps[idx];
    EMASSERT(ts->temp_allocated != 0);
    ts->temp_allocated = 0;

    k = ts->base_type + (ts->temp_local ? TCG_TYPE_COUNT : 0);
    set_bit(idx, s->free_temps[k].l);
}

void QEMUDisasContext::temp_free_i32(TCGv_i32 a)
{
    temp_free_internal(GET_TCGV_I32(a));
}

void QEMUDisasContext::temp_free_i64(TCGv_i64 a)
{
    temp_free_internal(GET_TCGV_I64(a));
}

void QEMUDisasContext::temp_free_ptr(TCGv_ptr a)
{
#if TCG_TARGET_REG_BITS == 32
    temp_free_i32(TCGV_PTR_TO_NAT(a));
#else
    temp_free_i64(TCGV_PTR_TO_NAT(a));
#endif
}
}
