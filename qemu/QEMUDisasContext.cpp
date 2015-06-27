#include <stdlib.h>
#include <vector>
#include <string.h>
#include <cpuid.h>
#include <vector>
#include "compatglib.h"
#include "QEMUDisasContext.h"
#include "ExecutableMemoryAllocator.h"
#include "log.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

typedef struct TCGHelperInfo {
    void* func;
    const char* name;
    unsigned flags;
    unsigned sizemask;
} TCGHelperInfo;

extern "C" {
#include "helper-proto.h"

static const TCGHelperInfo all_helpers[] = {
#include "helper-tcg.h"
};
#include "helper-gen.h"
}

namespace qemu {
qemu::TCGOpDef tcg_op_defs[] = {
#define DEF(s, oargs, iargs, cargs, flags)                    \
    {                                                         \
        #s, oargs, iargs, cargs, iargs + oargs + cargs, flags \
    }                                                         \
    ,
#include "tcg-opc.h"
#undef DEF
};
const size_t tcg_op_defs_max = ARRAY_SIZE(tcg_op_defs);

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
    jit::ExecutableMemoryAllocator* m_allocator;
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

static void tcg_func_start(TCGContext* s)
{
    s->nb_temps = s->nb_globals;

    /* No temps have been previously allocated for size or locality.  */
    memset(s->free_temps, 0, sizeof(s->free_temps));

    s->labels = (TCGLabel*)tcg_malloc(s, sizeof(TCGLabel) * TCG_MAX_LABELS);
    s->nb_labels = 0;
    s->current_frame_offset = s->frame_start;

#ifdef CONFIG_DEBUG_TCG
    s->goto_tb_issue_mask = 0;
#endif

    s->gen_opc_ptr = s->gen_opc_buf;
    s->gen_opparam_ptr = s->gen_opparam_buf;
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

static pthread_once_t initQEMUOnce = PTHREAD_ONCE_INIT;

static GHashTable* helper_table;
static void tcg_init_common(void)
{
    int op, total_args, n, i;
    TCGOpDef* def;
    TCGArgConstraint* args_ct;
    int* sorted_args;

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

    /* Count total number of arguments and allocate the corresponding
       space */
    total_args = 0;
    for (op = 0; op < NB_OPS; op++) {
        def = &tcg_op_defs[op];
        n = def->nb_iargs + def->nb_oargs;
        total_args += n;
    }

    /* Register helpers.  */
    /* Use g_direct_hash/equal for direct pointer comparisons on func.  */
    helper_table = g_hash_table_new(NULL, NULL);

    for (int i = 0; i < ARRAY_SIZE(all_helpers); ++i) {
        g_hash_table_insert(helper_table, (gpointer)all_helpers[i].func,
            (gpointer)&all_helpers[i]);
    }
    tcg_target_init();
}

static void tcg_set_frame(TCGContext* s, int reg, intptr_t start, intptr_t size)
{
    s->frame_start = start;
    s->frame_end = start + size;
    s->frame_reg = reg;
}

static void tcg_context_init(TCGContext* s)
{
    memset(s, 0, sizeof(*s));
    s->nb_globals = 0;

    pthread_once(&initQEMUOnce, tcg_init_common);
    s->helpers = helper_table;
    tcg_set_frame(s, TCG_REG_CALL_STACK,
        -CPU_TEMP_BUF_NLONGS * sizeof(long),
        CPU_TEMP_BUF_NLONGS * sizeof(long));
    tcg_regset_clear(s->reserved_regs);
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_CALL_STACK);
}

QEMUDisasContext::QEMUDisasContext(jit::ExecutableMemoryAllocator* allocator, void* dispDirect, void* dispIndirect, void* dispHot)
    : m_impl(new QEMUDisasContextImpl({ allocator }))
{
    memset(&m_impl->m_tcgCtx, sizeof(TCGContext), 0);
    tcg_context_init(&m_impl->m_tcgCtx);
    m_impl->m_tcgCtx.dispDirect = dispDirect;
    m_impl->m_tcgCtx.dispIndirect = dispIndirect;
    m_impl->m_tcgCtx.dispHot = dispHot;
}

QEMUDisasContext::~QEMUDisasContext()
{
    tcg_pool_reset(&m_impl->m_tcgCtx);
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
            TCGv_i32 t0 = const_i32(arg2);
            gen_op3_i32(INDEX_op_rotl_i32, ret, arg1, (TCGv_i32)arg2);
            temp_free_i32(t0);
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
        TCGv_i32 t0 = const_i32(arg2);
        gen_sar_i32(ret, arg1, t0);
        temp_free_i32(t0);
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
        TCGv_i64 t = temp_new_i64();
        gen_shri_i64(t, arg, count);
        gen_mov_i32(ret, TCGV_LOW(t));
        temp_free_i64(t);
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

void QEMUDisasContext::func_start()
{
    tcg_func_start(&m_impl->m_tcgCtx);
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
// compile start

static void temp_allocate_frame(TCGContext* s, int temp)
{
    TCGTemp* ts;
    ts = &s->temps[temp];
#if !(defined(__sparc__) && TCG_TARGET_REG_BITS == 64)
    /* Sparc64 stack is accessed with offset of 2047 */
    s->current_frame_offset = (s->current_frame_offset + (tcg_target_long)sizeof(tcg_target_long) - 1) & ~(sizeof(tcg_target_long) - 1);
#endif
    if (s->current_frame_offset + (tcg_target_long)sizeof(tcg_target_long) > s->frame_end) {
        tcg_abort();
    }
    ts->mem_offset = s->current_frame_offset;
    ts->mem_reg = s->frame_reg;
    ts->mem_allocated = 1;
    s->current_frame_offset += sizeof(tcg_target_long);
}

/* sync register 'reg' by saving it to the corresponding temporary */
static inline void tcg_reg_sync(TCGContext* s, int reg)
{
    TCGTemp* ts;
    int temp;

    temp = s->reg_to_temp[reg];
    ts = &s->temps[temp];
    EMASSERT(ts->val_type == TEMP_VAL_REG);
    if (!ts->mem_coherent && !ts->fixed_reg) {
        if (!ts->mem_allocated) {
            temp_allocate_frame(s, temp);
        }
        tcg_out_st(s, ts->type, reg, ts->mem_reg, ts->mem_offset);
    }
    ts->mem_coherent = 1;
}

/* free register 'reg' by spilling the corresponding temporary if necessary */
static void tcg_reg_free(TCGContext* s, int reg)
{
    int temp;

    temp = s->reg_to_temp[reg];
    if (temp != -1) {
        tcg_reg_sync(s, reg);
        s->temps[temp].val_type = TEMP_VAL_MEM;
        s->reg_to_temp[reg] = -1;
    }
}

/* Allocate a register belonging to reg1 & ~reg2 */
static int tcg_reg_alloc(TCGContext* s, TCGRegSet reg1, TCGRegSet reg2)
{
    int i, reg;
    TCGRegSet reg_ct;

    tcg_regset_andnot(reg_ct, reg1, reg2);

    /* first try free registers */
    for (i = 0; i < ARRAY_SIZE(tcg_target_reg_alloc_order); i++) {
        reg = tcg_target_reg_alloc_order[i];
        if (tcg_regset_test_reg(reg_ct, reg) && s->reg_to_temp[reg] == -1)
            return reg;
    }

    /* XXX: do better spill choice */
    for (i = 0; i < ARRAY_SIZE(tcg_target_reg_alloc_order); i++) {
        reg = tcg_target_reg_alloc_order[i];
        if (tcg_regset_test_reg(reg_ct, reg)) {
            tcg_reg_free(s, reg);
            return reg;
        }
    }

    tcg_abort();
}

/* mark a temporary as dead. */
static inline void temp_dead(TCGContext* s, int temp)
{
    TCGTemp* ts;

    ts = &s->temps[temp];
    if (!ts->fixed_reg) {
        if (ts->val_type == TEMP_VAL_REG) {
            s->reg_to_temp[ts->reg] = -1;
        }
        if (temp < s->nb_globals || ts->temp_local) {
            ts->val_type = TEMP_VAL_MEM;
        }
        else {
            ts->val_type = TEMP_VAL_DEAD;
        }
    }
}

/* sync a temporary to memory. 'allocated_regs' is used in case a
   temporary registers needs to be allocated to store a constant. */
static inline void temp_sync(TCGContext* s, int temp, TCGRegSet allocated_regs)
{
    TCGTemp* ts;

    ts = &s->temps[temp];
    if (!ts->fixed_reg) {
        switch (ts->val_type) {
        case TEMP_VAL_CONST:
            ts->reg = tcg_reg_alloc(s, tcg_target_available_regs[ts->type],
                allocated_regs);
            ts->val_type = TEMP_VAL_REG;
            s->reg_to_temp[ts->reg] = temp;
            ts->mem_coherent = 0;
            tcg_out_movi(s, ts->type, ts->reg, ts->val);
        /* fallthrough*/
        case TEMP_VAL_REG:
            tcg_reg_sync(s, ts->reg);
            break;
        case TEMP_VAL_DEAD:
        case TEMP_VAL_MEM:
            break;
        default:
            tcg_abort();
        }
    }
}

/* save a temporary to memory. 'allocated_regs' is used in case a
   temporary registers needs to be allocated to store a constant. */
static inline void temp_save(TCGContext* s, int temp, TCGRegSet allocated_regs)
{
#ifdef USE_LIVENESS_ANALYSIS
    /* The liveness analysis already ensures that globals are back
       in memory. Keep an EMASSERT for safety. */
    EMASSERT(s->temps[temp].val_type == TEMP_VAL_MEM || s->temps[temp].fixed_reg);
#else
    temp_sync(s, temp, allocated_regs);
    temp_dead(s, temp);
#endif
}

/* save globals to their canonical location and assume they can be
   modified be the following code. 'allocated_regs' is used in case a
   temporary registers needs to be allocated to store a constant. */
static void save_globals(TCGContext* s, TCGRegSet allocated_regs)
{
    int i;

    for (i = 0; i < s->nb_globals; i++) {
        temp_save(s, i, allocated_regs);
    }
}

/* sync globals to their canonical location and assume they can be
   read by the following code. 'allocated_regs' is used in case a
   temporary registers needs to be allocated to store a constant. */
static void sync_globals(TCGContext* s, TCGRegSet allocated_regs)
{
    int i;

    for (i = 0; i < s->nb_globals; i++) {
#ifdef USE_LIVENESS_ANALYSIS
        EMASSERT(s->temps[i].val_type != TEMP_VAL_REG || s->temps[i].fixed_reg || s->temps[i].mem_coherent);
#else
        temp_sync(s, i, allocated_regs);
#endif
    }
}

/* at the end of a basic block, we assume all temporaries are dead and
   all globals are stored at their canonical location. */
static void tcg_reg_alloc_bb_end(TCGContext* s, TCGRegSet allocated_regs)
{
    TCGTemp* ts;
    int i;

    for (i = s->nb_globals; i < s->nb_temps; i++) {
        ts = &s->temps[i];
        if (ts->temp_local) {
            temp_save(s, i, allocated_regs);
        }
        else {
#ifdef USE_LIVENESS_ANALYSIS
            /* The liveness analysis already ensures that temps are dead.
               Keep an EMASSERT for safety. */
            EMASSERT(ts->val_type == TEMP_VAL_DEAD);
#else
            temp_dead(s, i);
#endif
        }
    }

    save_globals(s, allocated_regs);
}

#define IS_DEAD_ARG(n) ((dead_args >> (n)) & 1)
#define NEED_SYNC_ARG(n) ((sync_args >> (n)) & 1)

static void tcg_reg_alloc_movi(TCGContext* s, const TCGArg* args,
    uint16_t dead_args, uint8_t sync_args)
{
    TCGTemp* ots;
    tcg_target_ulong val;

    ots = &s->temps[args[0]];
    val = args[1];

    if (ots->fixed_reg) {
        /* for fixed registers, we do not do any constant
           propagation */
        tcg_out_movi(s, ots->type, ots->reg, val);
    }
    else {
        /* The movi is not explicitly generated here */
        if (ots->val_type == TEMP_VAL_REG)
            s->reg_to_temp[ots->reg] = -1;
        ots->val_type = TEMP_VAL_CONST;
        ots->val = val;
    }
    if (NEED_SYNC_ARG(0)) {
        temp_sync(s, args[0], s->reserved_regs);
    }
    if (IS_DEAD_ARG(0)) {
        temp_dead(s, args[0]);
    }
}

static void tcg_reg_alloc_mov(TCGContext* s, const TCGOpDef* def,
    const TCGArg* args, uint16_t dead_args,
    uint8_t sync_args)
{
    TCGRegSet allocated_regs;
    TCGTemp *ts, *ots;
    TCGType otype, itype;

    tcg_regset_set(allocated_regs, s->reserved_regs);
    ots = &s->temps[args[0]];
    ts = &s->temps[args[1]];

    /* Note that otype != itype for no-op truncation.  */
    otype = ots->type;
    itype = ts->type;

    /* If the source value is not in a register, and we're going to be
       forced to have it in a register in order to perform the copy,
       then copy the SOURCE value into its own register first.  That way
       we don't have to reload SOURCE the next time it is used. */
    if (((NEED_SYNC_ARG(0) || ots->fixed_reg) && ts->val_type != TEMP_VAL_REG)
        || ts->val_type == TEMP_VAL_MEM) {
        ts->reg = tcg_reg_alloc(s, tcg_target_available_regs[itype],
            allocated_regs);
        if (ts->val_type == TEMP_VAL_MEM) {
            tcg_out_ld(s, itype, ts->reg, ts->mem_reg, ts->mem_offset);
            ts->mem_coherent = 1;
        }
        else if (ts->val_type == TEMP_VAL_CONST) {
            tcg_out_movi(s, itype, ts->reg, ts->val);
        }
        s->reg_to_temp[ts->reg] = args[1];
        ts->val_type = TEMP_VAL_REG;
    }

    if (IS_DEAD_ARG(0) && !ots->fixed_reg) {
        /* mov to a non-saved dead register makes no sense (even with
           liveness analysis disabled). */
        EMASSERT(NEED_SYNC_ARG(0));
        /* The code above should have moved the temp to a register. */
        EMASSERT(ts->val_type == TEMP_VAL_REG);
        if (!ots->mem_allocated) {
            temp_allocate_frame(s, args[0]);
        }
        tcg_out_st(s, otype, ts->reg, ots->mem_reg, ots->mem_offset);
        if (IS_DEAD_ARG(1)) {
            temp_dead(s, args[1]);
        }
        temp_dead(s, args[0]);
    }
    else if (ts->val_type == TEMP_VAL_CONST) {
        /* propagate constant */
        if (ots->val_type == TEMP_VAL_REG) {
            s->reg_to_temp[ots->reg] = -1;
        }
        ots->val_type = TEMP_VAL_CONST;
        ots->val = ts->val;
    }
    else {
        /* The code in the first if block should have moved the
           temp to a register. */
        EMASSERT(ts->val_type == TEMP_VAL_REG);
        if (IS_DEAD_ARG(1) && !ts->fixed_reg && !ots->fixed_reg) {
            /* the mov can be suppressed */
            if (ots->val_type == TEMP_VAL_REG) {
                s->reg_to_temp[ots->reg] = -1;
            }
            ots->reg = ts->reg;
            temp_dead(s, args[1]);
        }
        else {
            if (ots->val_type != TEMP_VAL_REG) {
                /* When allocating a new register, make sure to not spill the
                   input one. */
                tcg_regset_set_reg(allocated_regs, ts->reg);
                ots->reg = tcg_reg_alloc(s, tcg_target_available_regs[otype],
                    allocated_regs);
            }
            tcg_out_mov(s, otype, ots->reg, ts->reg);
        }
        ots->val_type = TEMP_VAL_REG;
        ots->mem_coherent = 0;
        s->reg_to_temp[ots->reg] = args[0];
        if (NEED_SYNC_ARG(0)) {
            tcg_reg_sync(s, ots->reg);
        }
    }
}

static void tcg_reg_alloc_op(TCGContext* s,
    const TCGOpDef* def, TCGOpcode opc,
    const TCGArg* args, uint16_t dead_args,
    uint8_t sync_args)
{
    TCGRegSet allocated_regs;
    int i, k, nb_iargs, nb_oargs, reg;
    TCGArg arg;
    const TCGArgConstraint* arg_ct;
    TCGTemp* ts;
    TCGArg new_args[TCG_MAX_OP_ARGS];
    int const_args[TCG_MAX_OP_ARGS];

    nb_oargs = def->nb_oargs;
    nb_iargs = def->nb_iargs;

    /* copy constants */
    memcpy(new_args + nb_oargs + nb_iargs,
        args + nb_oargs + nb_iargs,
        sizeof(TCGArg) * def->nb_cargs);

    /* satisfy input constraints */
    tcg_regset_set(allocated_regs, s->reserved_regs);
    for (k = 0; k < nb_iargs; k++) {
        i = def->sorted_args[nb_oargs + k];
        arg = args[i];
        arg_ct = &def->args_ct[i];
        ts = &s->temps[arg];
        if (ts->val_type == TEMP_VAL_MEM) {
            reg = tcg_reg_alloc(s, arg_ct->u.regs, allocated_regs);
            tcg_out_ld(s, ts->type, reg, ts->mem_reg, ts->mem_offset);
            ts->val_type = TEMP_VAL_REG;
            ts->reg = reg;
            ts->mem_coherent = 1;
            s->reg_to_temp[reg] = arg;
        }
        else if (ts->val_type == TEMP_VAL_CONST) {
            if (tcg_target_const_match(ts->val, ts->type, arg_ct)) {
                /* constant is OK for instruction */
                const_args[i] = 1;
                new_args[i] = ts->val;
                goto iarg_end;
            }
            else {
                /* need to move to a register */
                reg = tcg_reg_alloc(s, arg_ct->u.regs, allocated_regs);
                tcg_out_movi(s, ts->type, reg, ts->val);
                ts->val_type = TEMP_VAL_REG;
                ts->reg = reg;
                ts->mem_coherent = 0;
                s->reg_to_temp[reg] = arg;
            }
        }
        EMASSERT(ts->val_type == TEMP_VAL_REG);
        if (arg_ct->ct & TCG_CT_IALIAS) {
            if (ts->fixed_reg) {
                /* if fixed register, we must allocate a new register
                   if the alias is not the same register */
                if (arg != args[arg_ct->alias_index])
                    goto allocate_in_reg;
            }
            else {
                /* if the input is aliased to an output and if it is
                   not dead after the instruction, we must allocate
                   a new register and move it */
                if (!IS_DEAD_ARG(i)) {
                    goto allocate_in_reg;
                }
            }
        }
        reg = ts->reg;
        if (tcg_regset_test_reg(arg_ct->u.regs, reg)) {
            /* nothing to do : the constraint is satisfied */
        }
        else {
        allocate_in_reg:
            /* allocate a new register matching the constraint 
               and move the temporary register into it */
            reg = tcg_reg_alloc(s, arg_ct->u.regs, allocated_regs);
            tcg_out_mov(s, ts->type, reg, ts->reg);
        }
        new_args[i] = reg;
        const_args[i] = 0;
        tcg_regset_set_reg(allocated_regs, reg);
    iarg_end:;
    }

    /* mark dead temporaries and free the associated registers */
    for (i = nb_oargs; i < nb_oargs + nb_iargs; i++) {
        if (IS_DEAD_ARG(i)) {
            temp_dead(s, args[i]);
        }
    }

    if (def->flags & TCG_OPF_BB_END) {
        tcg_reg_alloc_bb_end(s, allocated_regs);
    }
    else {
        if (def->flags & TCG_OPF_CALL_CLOBBER) {
            /* XXX: permit generic clobber register list ? */
            for (reg = 0; reg < TCG_TARGET_NB_REGS; reg++) {
                if (tcg_regset_test_reg(tcg_target_call_clobber_regs, reg)) {
                    tcg_reg_free(s, reg);
                }
            }
        }
        if (def->flags & TCG_OPF_SIDE_EFFECTS) {
            /* sync globals if the op has side effects and might trigger
               an exception. */
            sync_globals(s, allocated_regs);
        }

        /* satisfy the output constraints */
        tcg_regset_set(allocated_regs, s->reserved_regs);
        for (k = 0; k < nb_oargs; k++) {
            i = def->sorted_args[k];
            arg = args[i];
            arg_ct = &def->args_ct[i];
            ts = &s->temps[arg];
            if (arg_ct->ct & TCG_CT_ALIAS) {
                reg = new_args[arg_ct->alias_index];
            }
            else {
                /* if fixed register, we try to use it */
                reg = ts->reg;
                if (ts->fixed_reg && tcg_regset_test_reg(arg_ct->u.regs, reg)) {
                    goto oarg_end;
                }
                reg = tcg_reg_alloc(s, arg_ct->u.regs, allocated_regs);
            }
            tcg_regset_set_reg(allocated_regs, reg);
            /* if a fixed register is used, then a move will be done afterwards */
            if (!ts->fixed_reg) {
                if (ts->val_type == TEMP_VAL_REG) {
                    s->reg_to_temp[ts->reg] = -1;
                }
                ts->val_type = TEMP_VAL_REG;
                ts->reg = reg;
                /* temp value is modified, so the value kept in memory is
                   potentially not the same */
                ts->mem_coherent = 0;
                s->reg_to_temp[reg] = arg;
            }
        oarg_end:
            new_args[i] = reg;
        }
    }

    /* emit instruction */
    tcg_out_op(s, opc, new_args, const_args);

    /* move the outputs in the correct register if needed */
    for (i = 0; i < nb_oargs; i++) {
        ts = &s->temps[args[i]];
        reg = new_args[i];
        if (ts->fixed_reg && ts->reg != reg) {
            tcg_out_mov(s, ts->type, ts->reg, reg);
        }
        if (NEED_SYNC_ARG(i)) {
            tcg_reg_sync(s, reg);
        }
        if (IS_DEAD_ARG(i)) {
            temp_dead(s, args[i]);
        }
    }
}

#ifdef TCG_TARGET_STACK_GROWSUP
#define STACK_DIR(x) (-(x))
#else
#define STACK_DIR(x) (x)
#endif

static int tcg_reg_alloc_call(TCGContext* s, const TCGOpDef* def,
    TCGOpcode opc, const TCGArg* args,
    uint16_t dead_args, uint8_t sync_args)
{
    int nb_iargs, nb_oargs, flags, nb_regs, i, reg, nb_params;
    TCGArg arg;
    TCGTemp* ts;
    intptr_t stack_offset;
    size_t call_stack_size;
    tcg_insn_unit* func_addr;
    int allocate_args;
    TCGRegSet allocated_regs;

    arg = *args++;

    nb_oargs = arg >> 16;
    nb_iargs = arg & 0xffff;
    nb_params = nb_iargs;

    func_addr = (tcg_insn_unit*)(intptr_t)args[nb_oargs + nb_iargs];
    flags = args[nb_oargs + nb_iargs + 1];

    nb_regs = ARRAY_SIZE(tcg_target_call_iarg_regs);
    if (nb_regs > nb_params) {
        nb_regs = nb_params;
    }

    /* assign stack slots first */
    call_stack_size = (nb_params - nb_regs) * sizeof(tcg_target_long);
    call_stack_size = (call_stack_size + TCG_TARGET_STACK_ALIGN - 1) & ~(TCG_TARGET_STACK_ALIGN - 1);
    allocate_args = (call_stack_size > TCG_STATIC_CALL_ARGS_SIZE);
    if (allocate_args) {
        /* XXX: if more than TCG_STATIC_CALL_ARGS_SIZE is needed,
           preallocate call stack */
        tcg_abort();
    }

    stack_offset = TCG_TARGET_CALL_STACK_OFFSET;
    for (i = nb_regs; i < nb_params; i++) {
        arg = args[nb_oargs + i];
#ifdef TCG_TARGET_STACK_GROWSUP
        stack_offset -= sizeof(tcg_target_long);
#endif
        if (arg != TCG_CALL_DUMMY_ARG) {
            ts = &s->temps[arg];
            if (ts->val_type == TEMP_VAL_REG) {
                tcg_out_st(s, ts->type, ts->reg, TCG_REG_CALL_STACK, stack_offset);
            }
            else if (ts->val_type == TEMP_VAL_MEM) {
                reg = tcg_reg_alloc(s, tcg_target_available_regs[ts->type],
                    s->reserved_regs);
                /* XXX: not correct if reading values from the stack */
                tcg_out_ld(s, ts->type, reg, ts->mem_reg, ts->mem_offset);
                tcg_out_st(s, ts->type, reg, TCG_REG_CALL_STACK, stack_offset);
            }
            else if (ts->val_type == TEMP_VAL_CONST) {
                reg = tcg_reg_alloc(s, tcg_target_available_regs[ts->type],
                    s->reserved_regs);
                /* XXX: sign extend may be needed on some targets */
                tcg_out_movi(s, ts->type, reg, ts->val);
                tcg_out_st(s, ts->type, reg, TCG_REG_CALL_STACK, stack_offset);
            }
            else {
                tcg_abort();
            }
        }
#ifndef TCG_TARGET_STACK_GROWSUP
        stack_offset += sizeof(tcg_target_long);
#endif
    }

    /* assign input registers */
    tcg_regset_set(allocated_regs, s->reserved_regs);
    for (i = 0; i < nb_regs; i++) {
        arg = args[nb_oargs + i];
        if (arg != TCG_CALL_DUMMY_ARG) {
            ts = &s->temps[arg];
            reg = tcg_target_call_iarg_regs[i];
            tcg_reg_free(s, reg);
            if (ts->val_type == TEMP_VAL_REG) {
                if (ts->reg != reg) {
                    tcg_out_mov(s, ts->type, reg, ts->reg);
                }
            }
            else if (ts->val_type == TEMP_VAL_MEM) {
                tcg_out_ld(s, ts->type, reg, ts->mem_reg, ts->mem_offset);
            }
            else if (ts->val_type == TEMP_VAL_CONST) {
                /* XXX: sign extend ? */
                tcg_out_movi(s, ts->type, reg, ts->val);
            }
            else {
                tcg_abort();
            }
            tcg_regset_set_reg(allocated_regs, reg);
        }
    }

    /* mark dead temporaries and free the associated registers */
    for (i = nb_oargs; i < nb_iargs + nb_oargs; i++) {
        if (IS_DEAD_ARG(i)) {
            temp_dead(s, args[i]);
        }
    }

    /* clobber call registers */
    for (reg = 0; reg < TCG_TARGET_NB_REGS; reg++) {
        if (tcg_regset_test_reg(tcg_target_call_clobber_regs, reg)) {
            tcg_reg_free(s, reg);
        }
    }

    /* Save globals if they might be written by the helper, sync them if
       they might be read. */
    if (flags & TCG_CALL_NO_READ_GLOBALS) {
        /* Nothing to do */
    }
    else if (flags & TCG_CALL_NO_WRITE_GLOBALS) {
        sync_globals(s, allocated_regs);
    }
    else {
        save_globals(s, allocated_regs);
    }

    tcg_out_call(s, func_addr);

    /* assign output registers and emit moves if needed */
    for (i = 0; i < nb_oargs; i++) {
        arg = args[i];
        ts = &s->temps[arg];
        reg = tcg_target_call_oarg_regs[i];
        EMASSERT(s->reg_to_temp[reg] == -1);

        if (ts->fixed_reg) {
            if (ts->reg != reg) {
                tcg_out_mov(s, ts->type, ts->reg, reg);
            }
        }
        else {
            if (ts->val_type == TEMP_VAL_REG) {
                s->reg_to_temp[ts->reg] = -1;
            }
            ts->val_type = TEMP_VAL_REG;
            ts->reg = reg;
            ts->mem_coherent = 0;
            s->reg_to_temp[reg] = arg;
            if (NEED_SYNC_ARG(i)) {
                tcg_reg_sync(s, reg);
            }
            if (IS_DEAD_ARG(i)) {
                temp_dead(s, args[i]);
            }
        }
    }

    return nb_iargs + nb_oargs + def->nb_cargs + 1;
}

static void tcg_reg_alloc_start(TCGContext* s)
{
    int i;
    TCGTemp* ts;
    for (i = 0; i < s->nb_globals; i++) {
        ts = &s->temps[i];
        if (ts->fixed_reg) {
            ts->val_type = TEMP_VAL_REG;
        }
        else {
            ts->val_type = TEMP_VAL_MEM;
        }
    }
    for (i = s->nb_globals; i < s->nb_temps; i++) {
        ts = &s->temps[i];
        if (ts->temp_local) {
            ts->val_type = TEMP_VAL_MEM;
        }
        else {
            ts->val_type = TEMP_VAL_DEAD;
        }
        ts->mem_allocated = 0;
        ts->fixed_reg = 0;
    }
    for (i = 0; i < TCG_TARGET_NB_REGS; i++) {
        s->reg_to_temp[i] = -1;
    }
}

#ifdef USE_LIVENESS_ANALYSIS
/* set a nop for an operation using 'nb_args' */
static inline void tcg_set_nop(TCGContext* s, uint16_t* opc_ptr,
    TCGArg* args, int nb_args)
{
    if (nb_args == 0) {
        *opc_ptr = INDEX_op_nop;
    }
    else {
        *opc_ptr = INDEX_op_nopn;
        args[0] = nb_args;
        args[nb_args - 1] = nb_args;
    }
}

/* liveness analysis: end of function: all temps are dead, and globals
   should be in memory. */
static inline void tcg_la_func_end(TCGContext* s, uint8_t* dead_temps,
    uint8_t* mem_temps)
{
    memset(dead_temps, 1, s->nb_temps);
    memset(mem_temps, 1, s->nb_globals);
    memset(mem_temps + s->nb_globals, 0, s->nb_temps - s->nb_globals);
}

/* liveness analysis: end of basic block: all temps are dead, globals
   and local temps should be in memory. */
static inline void tcg_la_bb_end(TCGContext* s, uint8_t* dead_temps,
    uint8_t* mem_temps)
{
    int i;

    memset(dead_temps, 1, s->nb_temps);
    memset(mem_temps, 1, s->nb_globals);
    for (i = s->nb_globals; i < s->nb_temps; i++) {
        mem_temps[i] = s->temps[i].temp_local;
    }
}

/* Liveness analysis : update the opc_dead_args array to tell if a
   given input arguments is dead. Instructions updating dead
   temporaries are removed. */
static void tcg_liveness_analysis(TCGContext* s)
{
    int i, op_index, nb_args, nb_iargs, nb_oargs, nb_ops;
    TCGOpcode op, op_new, op_new2;
    TCGArg *args, arg;
    const TCGOpDef* def;
    uint8_t *dead_temps, *mem_temps;
    uint16_t dead_args;
    uint8_t sync_args;
    bool have_op_new2;

    s->gen_opc_ptr++; /* skip end */

    nb_ops = s->gen_opc_ptr - s->gen_opc_buf;

    s->op_dead_args = static_cast<uint16_t*>(tcg_malloc(s, nb_ops * sizeof(uint16_t)));
    s->op_sync_args = static_cast<uint8_t*>(tcg_malloc(s, nb_ops * sizeof(uint8_t)));

    dead_temps = static_cast<uint8_t*>(tcg_malloc(s, s->nb_temps));
    mem_temps = static_cast<uint8_t*>(tcg_malloc(s, s->nb_temps));
    tcg_la_func_end(s, dead_temps, mem_temps);

    args = s->gen_opparam_ptr;
    op_index = nb_ops - 1;
    while (op_index >= 0) {
        op = static_cast<TCGOpcode>(s->gen_opc_buf[op_index]);
        def = &tcg_op_defs[op];
        switch (op) {
        case INDEX_op_call: {
            int call_flags;

            nb_args = args[-1];
            args -= nb_args;
            arg = *args++;
            nb_iargs = arg & 0xffff;
            nb_oargs = arg >> 16;
            call_flags = args[nb_oargs + nb_iargs + 1];

            /* pure functions can be removed if their result is not
                   used */
            if (call_flags & TCG_CALL_NO_SIDE_EFFECTS) {
                for (i = 0; i < nb_oargs; i++) {
                    arg = args[i];
                    if (!dead_temps[arg] || mem_temps[arg]) {
                        goto do_not_remove_call;
                    }
                }
                tcg_set_nop(s, s->gen_opc_buf + op_index,
                    args - 1, nb_args);
            }
            else {
            do_not_remove_call:

                /* output args are dead */
                dead_args = 0;
                sync_args = 0;
                for (i = 0; i < nb_oargs; i++) {
                    arg = args[i];
                    if (dead_temps[arg]) {
                        dead_args |= (1 << i);
                    }
                    if (mem_temps[arg]) {
                        sync_args |= (1 << i);
                    }
                    dead_temps[arg] = 1;
                    mem_temps[arg] = 0;
                }

                if (!(call_flags & TCG_CALL_NO_READ_GLOBALS)) {
                    /* globals should be synced to memory */
                    memset(mem_temps, 1, s->nb_globals);
                }
                if (!(call_flags & (TCG_CALL_NO_WRITE_GLOBALS | TCG_CALL_NO_READ_GLOBALS))) {
                    /* globals should go back to memory */
                    memset(dead_temps, 1, s->nb_globals);
                }

                /* input args are live */
                for (i = nb_oargs; i < nb_iargs + nb_oargs; i++) {
                    arg = args[i];
                    if (arg != TCG_CALL_DUMMY_ARG) {
                        if (dead_temps[arg]) {
                            dead_args |= (1 << i);
                        }
                        dead_temps[arg] = 0;
                    }
                }
                s->op_dead_args[op_index] = dead_args;
                s->op_sync_args[op_index] = sync_args;
            }
            args--;
        } break;
        case INDEX_op_debug_insn_start:
            args -= def->nb_args;
            break;
        case INDEX_op_nopn:
            nb_args = args[-1];
            args -= nb_args;
            break;
        case INDEX_op_discard:
            args--;
            /* mark the temporary as dead */
            dead_temps[args[0]] = 1;
            mem_temps[args[0]] = 0;
            break;
        case INDEX_op_end:
            break;

        case INDEX_op_add2_i32:
            op_new = INDEX_op_add_i32;
            goto do_addsub2;
        case INDEX_op_sub2_i32:
            op_new = INDEX_op_sub_i32;
            goto do_addsub2;
        case INDEX_op_add2_i64:
            op_new = INDEX_op_add_i64;
            goto do_addsub2;
        case INDEX_op_sub2_i64:
            op_new = INDEX_op_sub_i64;
        do_addsub2:
            args -= 6;
            nb_iargs = 4;
            nb_oargs = 2;
            /* Test if the high part of the operation is dead, but not
               the low part.  The result can be optimized to a simple
               add or sub.  This happens often for x86_64 guest when the
               cpu mode is set to 32 bit.  */
            if (dead_temps[args[1]] && !mem_temps[args[1]]) {
                if (dead_temps[args[0]] && !mem_temps[args[0]]) {
                    goto do_remove;
                }
                /* Create the single operation plus nop.  */
                s->gen_opc_buf[op_index] = op = op_new;
                args[1] = args[2];
                args[2] = args[4];
                EMASSERT(s->gen_opc_buf[op_index + 1] == INDEX_op_nop);
                tcg_set_nop(s, s->gen_opc_buf + op_index + 1, args + 3, 3);
                /* Fall through and mark the single-word operation live.  */
                nb_iargs = 2;
                nb_oargs = 1;
            }
            goto do_not_remove;

        case INDEX_op_mulu2_i32:
            op_new = INDEX_op_mul_i32;
            op_new2 = INDEX_op_muluh_i32;
            have_op_new2 = TCG_TARGET_HAS_muluh_i32;
            goto do_mul2;
        case INDEX_op_muls2_i32:
            op_new = INDEX_op_mul_i32;
            op_new2 = INDEX_op_mulsh_i32;
            have_op_new2 = TCG_TARGET_HAS_mulsh_i32;
            goto do_mul2;
        case INDEX_op_mulu2_i64:
            op_new = INDEX_op_mul_i64;
            op_new2 = INDEX_op_muluh_i64;
            have_op_new2 = TCG_TARGET_HAS_muluh_i64;
            goto do_mul2;
        case INDEX_op_muls2_i64:
            op_new = INDEX_op_mul_i64;
            op_new2 = INDEX_op_mulsh_i64;
            have_op_new2 = TCG_TARGET_HAS_mulsh_i64;
            goto do_mul2;
        do_mul2:
            args -= 4;
            nb_iargs = 2;
            nb_oargs = 2;
            if (dead_temps[args[1]] && !mem_temps[args[1]]) {
                if (dead_temps[args[0]] && !mem_temps[args[0]]) {
                    /* Both parts of the operation are dead.  */
                    goto do_remove;
                }
                /* The high part of the operation is dead; generate the low. */
                s->gen_opc_buf[op_index] = op = op_new;
                args[1] = args[2];
                args[2] = args[3];
            }
            else if (have_op_new2 && dead_temps[args[0]]
                && !mem_temps[args[0]]) {
                /* The low part of the operation is dead; generate the high.  */
                s->gen_opc_buf[op_index] = op = op_new2;
                args[0] = args[1];
                args[1] = args[2];
                args[2] = args[3];
            }
            else {
                goto do_not_remove;
            }
            EMASSERT(s->gen_opc_buf[op_index + 1] == INDEX_op_nop);
            tcg_set_nop(s, s->gen_opc_buf + op_index + 1, args + 3, 1);
            /* Mark the single-word operation live.  */
            nb_oargs = 1;
            goto do_not_remove;

        default:
            /* XXX: optimize by hardcoding common cases (e.g. triadic ops) */
            args -= def->nb_args;
            nb_iargs = def->nb_iargs;
            nb_oargs = def->nb_oargs;

            /* Test if the operation can be removed because all
               its outputs are dead. We assume that nb_oargs == 0
               implies side effects */
            if (!(def->flags & TCG_OPF_SIDE_EFFECTS) && nb_oargs != 0) {
                for (i = 0; i < nb_oargs; i++) {
                    arg = args[i];
                    if (!dead_temps[arg] || mem_temps[arg]) {
                        goto do_not_remove;
                    }
                }
            do_remove:
                tcg_set_nop(s, s->gen_opc_buf + op_index, args, def->nb_args);
#ifdef CONFIG_PROFILER
                s->del_op_count++;
#endif
            }
            else {
            do_not_remove:

                /* output args are dead */
                dead_args = 0;
                sync_args = 0;
                for (i = 0; i < nb_oargs; i++) {
                    arg = args[i];
                    if (dead_temps[arg]) {
                        dead_args |= (1 << i);
                    }
                    if (mem_temps[arg]) {
                        sync_args |= (1 << i);
                    }
                    dead_temps[arg] = 1;
                    mem_temps[arg] = 0;
                }

                /* if end of basic block, update */
                if (def->flags & TCG_OPF_BB_END) {
                    tcg_la_bb_end(s, dead_temps, mem_temps);
                }
                else if (def->flags & TCG_OPF_SIDE_EFFECTS) {
                    /* globals should be synced to memory */
                    memset(mem_temps, 1, s->nb_globals);
                }

                /* input args are live */
                for (i = nb_oargs; i < nb_oargs + nb_iargs; i++) {
                    arg = args[i];
                    if (dead_temps[arg]) {
                        dead_args |= (1 << i);
                    }
                    dead_temps[arg] = 0;
                }
                s->op_dead_args[op_index] = dead_args;
                s->op_sync_args[op_index] = sync_args;
            }
            break;
        }
        op_index--;
    }

    if (args != s->gen_opparam_buf) {
        tcg_abort();
    }
}
#else
/* dummy liveness analysis */
static void tcg_liveness_analysis(TCGContext* s)
{
    int nb_ops;
    nb_ops = s->gen_opc_ptr - s->gen_opc_buf;

    s->op_dead_args = tcg_malloc(nb_ops * sizeof(uint16_t));
    memset(s->op_dead_args, 0, nb_ops * sizeof(uint16_t));
    s->op_sync_args = tcg_malloc(nb_ops * sizeof(uint8_t));
    memset(s->op_sync_args, 0, nb_ops * sizeof(uint8_t));
}
#endif

static void tcg_generate_prologue_check(TCGContext* s)
{
#ifndef __i386__
#error unsupported arch
#endif
    // call +5
    tcg_out_opc(s, OPC_CALL_Jz, 0, 0, 0);
    tcg_out32(s, 0);
    tcg_out_pop(s, TCG_REG_ESI);
    int offset = 0x18, offset2 = 0x10, offset3 = 0x4;
    static const int threshold = 1000;

    tcg_out_modrm_sib_offset(s, OPC_LEA, TCG_REG_ESI, TCG_REG_ESI, -1, 0,
        offset);
    tcg_out_modrm_offset(s, OPC_ARITH_EvIz, ARITH_CMP, TCG_REG_ESI, 0);
    tcg_out32(s, threshold);

    tcg_out8(s, OPC_JCC_short + JCC_JL);
    tcg_out8(s, offset2);
    // call the function here
    tcg_out_st(s, TCG_TYPE_I32, TCG_REG_ESI, TCG_REG_CALL_STACK, 0);
    tcg_out_call(s, reinterpret_cast<tcg_insn_unit*>(s->dispHot));
    tcg_out8(s, OPC_JMP_short);
    tcg_out8(s, offset3);
    tcg_out32(s, 0);
    // here the start
    tcg_out_modrm_offset(s, OPC_ARITH_EvIb, EXT5_INC_Ev, TCG_REG_ESI, 0);
    tcg_out8(s, 1);
}

static inline int tcg_gen_code_common(TCGContext* s,
    tcg_insn_unit* gen_code_buf,
    long search_pc)
{
    TCGOpcode opc;
    int op_index;
    const TCGOpDef* def;
    const TCGArg* args;

#ifdef DEBUG_DISAS
    if (unlikely(qemu_loglevel_mask(CPU_LOG_TB_OP))) {
        qemu_log("OP:\n");
        tcg_dump_ops(s);
        qemu_log("\n");
    }
#endif

#ifdef CONFIG_PROFILER
    s->opt_time -= profile_getclock();
#endif

#ifdef USE_TCG_OPTIMIZATIONS
    s->gen_opparam_ptr = tcg_optimize(s, s->gen_opc_ptr, s->gen_opparam_buf, tcg_op_defs);
#endif

#ifdef CONFIG_PROFILER
    s->opt_time += profile_getclock();
    s->la_time -= profile_getclock();
#endif

    tcg_liveness_analysis(s);

#ifdef CONFIG_PROFILER
    s->la_time += profile_getclock();
#endif

#ifdef DEBUG_DISAS
    if (unlikely(qemu_loglevel_mask(CPU_LOG_TB_OP_OPT))) {
        qemu_log("OP after optimization and liveness analysis:\n");
        tcg_dump_ops(s);
        qemu_log("\n");
    }
#endif

    tcg_reg_alloc_start(s);

    s->code_buf = gen_code_buf;
    s->code_ptr = gen_code_buf;

#define tcg_out_tb_init(a)
    tcg_out_tb_init(s);

    args = s->gen_opparam_buf;
    op_index = 0;
    tcg_generate_prologue_check(s);
    for (;;) {
        opc = static_cast<TCGOpcode>(s->gen_opc_buf[op_index]);
#ifdef CONFIG_PROFILER
        tcg_table_op_count[opc]++;
#endif
        def = &tcg_op_defs[opc];
#if 0
        printf("%s: %d %d %d\n", def->name,
               def->nb_oargs, def->nb_iargs, def->nb_cargs);
        //        dump_regs(s);
#endif
        switch (opc) {
        case INDEX_op_mov_i32:
        case INDEX_op_mov_i64:
            tcg_reg_alloc_mov(s, def, args, s->op_dead_args[op_index],
                s->op_sync_args[op_index]);
            break;
        case INDEX_op_movi_i32:
        case INDEX_op_movi_i64:
            tcg_reg_alloc_movi(s, args, s->op_dead_args[op_index],
                s->op_sync_args[op_index]);
            break;
        case INDEX_op_debug_insn_start:
            /* debug instruction */
            break;
        case INDEX_op_nop:
        case INDEX_op_nop1:
        case INDEX_op_nop2:
        case INDEX_op_nop3:
            break;
        case INDEX_op_nopn:
            args += args[0];
            goto next;
        case INDEX_op_discard:
            temp_dead(s, args[0]);
            break;
        case INDEX_op_set_label:
            tcg_reg_alloc_bb_end(s, s->reserved_regs);
            tcg_out_label(s, args[0], s->code_ptr);
            break;
        case INDEX_op_call:
            args += tcg_reg_alloc_call(s, def, opc, args,
                s->op_dead_args[op_index],
                s->op_sync_args[op_index]);
            goto next;
        case INDEX_op_end:
            goto the_end;
        default:
            /* Sanity check that we've not introduced any unhandled opcodes. */
            if (def->flags & TCG_OPF_NOT_PRESENT) {
                tcg_abort();
            }
            /* Note: in order to speed up the code, it would be much
               faster to have specialized register allocator functions for
               some common argument patterns */
            tcg_reg_alloc_op(s, def, opc, args, s->op_dead_args[op_index],
                s->op_sync_args[op_index]);
            break;
        }
        args += def->nb_args;
    next:
        if (search_pc >= 0 && search_pc < tcg_current_code_size(s)) {
            return op_index;
        }
        op_index++;
#ifndef NDEBUG
// check_regs(s);
#endif
    }
the_end:
/* Generate TB finalization at the end of block */
#define tcg_out_tb_finalize(a)
    tcg_out_tb_finalize(s);
    return -1;
}

int tcg_gen_code(TCGContext* s, tcg_insn_unit* gen_code_buf)
{
#ifdef CONFIG_PROFILER
    {
        int n;
        n = (s->gen_opc_ptr - s->gen_opc_buf);
        s->op_count += n;
        if (n > s->op_count_max)
            s->op_count_max = n;

        s->temp_count += s->nb_temps;
        if (s->nb_temps > s->temp_count_max)
            s->temp_count_max = s->nb_temps;
    }
#endif

    tcg_gen_code_common(s, gen_code_buf, -1);

    /* flush instruction cache */
    flush_icache_range((uintptr_t)s->code_buf, (uintptr_t)s->code_ptr);

    return tcg_current_code_size(s);
}

void QEMUDisasContext::compile()
{
    static const size_t codeBufferSize = 4096;
    std::vector<tcg_insn_unit> genCodeBuffer(codeBufferSize);
    tcg_insn_unit* gen_code_buf = const_cast<tcg_insn_unit*>(genCodeBuffer.data());
    int size = tcg_gen_code(&m_impl->m_tcgCtx, gen_code_buf);
    void* dst = m_impl->m_allocator->allocate(size, 0);
    memcpy(dst, gen_code_buf, size);
}

void QEMUDisasContext::link()
{
}

void QEMUDisasContext::gen_sdiv(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    gen_helper_sdiv(this, ret, arg1, arg2);
}

void QEMUDisasContext::gen_udiv(TCGv_i32 ret, TCGv_i32 arg1, TCGv_i32 arg2)
{
    gen_helper_udiv(this, ret, arg1, arg2);
}

#define VFP_BINOP(name, size)                                                                                        \
    void QEMUDisasContext::gen_vfp_##name(TCGv_i##size ret, TCGv_i##size arg1, TCGv_i##size arg2, TCGv_ptr fpstatus) \
    {                                                                                                                \
        gen_helper_vfp_##name(this, ret, arg1, arg2, fpstatus);                                                      \
    }
VFP_BINOP(adds, 32)
VFP_BINOP(subs, 32)
VFP_BINOP(muls, 32)
VFP_BINOP(divs, 32)

VFP_BINOP(addd, 64)
VFP_BINOP(subd, 64)
VFP_BINOP(muld, 64)
VFP_BINOP(divd, 64)
#undef VFP_BINOP

#define VFP_UNARY(name, size1, size2)                                                              \
    void QEMUDisasContext::gen_vfp_##name(TCGv_i##size1 ret, TCGv_i##size2 arg, TCGv_ptr fpstatus) \
    {                                                                                              \
        gen_helper_vfp_##name(this, ret, arg, fpstatus);                                           \
    }

VFP_UNARY(touis, 32, 32)
VFP_UNARY(touizs, 32, 32)
VFP_UNARY(tosis, 32, 32)
VFP_UNARY(tosizs, 32, 32)
VFP_UNARY(touid, 32, 64)
VFP_UNARY(touizd, 32, 64)
VFP_UNARY(tosid, 32, 64)
VFP_UNARY(tosizd, 32, 64)
VFP_UNARY(sitos, 32, 32)
VFP_UNARY(uitos, 32, 32)

VFP_UNARY(uitod, 64, 32)
VFP_UNARY(sitod, 64, 32)
#undef VFP_UNARY
}
