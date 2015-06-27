#ifndef TGTYPE_H
#define TGTYPE_H
#include "types.h"
#include "compiler.h"

typedef struct TCGv_i32__* TCGv_i32;
typedef struct TCGv_i64__* TCGv_i64;
typedef struct TCGv_ptr__* TCGv_ptr;
#define tcg_temp_free tcg_temp_free_i32
#define tcg_temp_new() (TCGv) tcg_temp_new_i32();
#define TCGV_UNUSED_I32(x) x = MAKE_TCGV_I32(-1)
#define TCGV_UNUSED_I64(x) x = MAKE_TCGV_I64(-1)
#define TCGV_UNUSED_PTR(x) x = MAKE_TCGV_PTR(-1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define TCGv TCGv_i32
#define TARGET_FMT_lx "%08x"
#define TCG_TARGET_REG_BITS 32
#define TARGET_LONG_BITS 32
#ifdef __x86_64__
#define TARGET_X86_64
#endif
#ifdef TARGET_X86_64
#define TARGET_LONG_BITS 64
#else
#define TARGET_LONG_BITS 32
#endif

#define TCG_CALL_DUMMY_ARG      ((TCGArg)(-1))
typedef enum {
    /* non-signed */
    TCG_COND_NEVER = 0 | 0 | 0 | 0,
    TCG_COND_ALWAYS = 0 | 0 | 0 | 1,
    TCG_COND_EQ = 8 | 0 | 0 | 0,
    TCG_COND_NE = 8 | 0 | 0 | 1,
    /* signed */
    TCG_COND_LT = 0 | 0 | 2 | 0,
    TCG_COND_GE = 0 | 0 | 2 | 1,
    TCG_COND_LE = 8 | 0 | 2 | 0,
    TCG_COND_GT = 8 | 0 | 2 | 1,
    /* unsigned */
    TCG_COND_LTU = 0 | 4 | 0 | 0,
    TCG_COND_GEU = 0 | 4 | 0 | 1,
    TCG_COND_LEU = 8 | 4 | 0 | 0,
    TCG_COND_GTU = 8 | 4 | 0 | 1,
} TCGCond;

typedef enum TCGMemOp {
    MO_8 = 0,
    MO_16 = 1,
    MO_32 = 2,
    MO_64 = 3,
    MO_SIZE = 3, /* Mask for the above.  */

    MO_SIGN = 4, /* Sign-extended, otherwise zero-extended.  */

    MO_BSWAP = 8, /* Host reverse endian.  */
#ifdef HOST_WORDS_BIGENDIAN
    MO_LE = MO_BSWAP,
    MO_BE = 0,
#else
    MO_LE = 0,
    MO_BE = MO_BSWAP,
#endif
#ifdef TARGET_WORDS_BIGENDIAN
    MO_TE = MO_BE,
#else
    MO_TE = MO_LE,
#endif

    /* Combinations of the above, for ease of use.  */
    MO_UB = MO_8,
    MO_UW = MO_16,
    MO_UL = MO_32,
    MO_SB = MO_SIGN | MO_8,
    MO_SW = MO_SIGN | MO_16,
    MO_SL = MO_SIGN | MO_32,
    MO_Q = MO_64,

    MO_LEUW = MO_LE | MO_UW,
    MO_LEUL = MO_LE | MO_UL,
    MO_LESW = MO_LE | MO_SW,
    MO_LESL = MO_LE | MO_SL,
    MO_LEQ = MO_LE | MO_Q,

    MO_BEUW = MO_BE | MO_UW,
    MO_BEUL = MO_BE | MO_UL,
    MO_BESW = MO_BE | MO_SW,
    MO_BESL = MO_BE | MO_SL,
    MO_BEQ = MO_BE | MO_Q,

    MO_TEUW = MO_TE | MO_UW,
    MO_TEUL = MO_TE | MO_UL,
    MO_TESW = MO_TE | MO_SW,
    MO_TESL = MO_TE | MO_SL,
    MO_TEQ = MO_TE | MO_Q,

    MO_SSIZE = MO_SIZE | MO_SIGN,
} TCGMemOp;

typedef target_long tcg_target_long;
typedef target_ulong tcg_target_ulong;
typedef intptr_t TCGArg;

#if TCG_TARGET_REG_BITS == 64
# define TCG_AREG0 TCG_REG_R14
#else
# define TCG_AREG0 TCG_REG_EBP
#endif
typedef enum {
    TCG_REG_EAX = 0,
    TCG_REG_ECX,
    TCG_REG_EDX,
    TCG_REG_EBX,
    TCG_REG_ESP,
    TCG_REG_EBP,
    TCG_REG_ESI,
    TCG_REG_EDI,

    /* 64-bit registers; always define the symbols to avoid
       too much if-deffing.  */
    TCG_REG_R8,
    TCG_REG_R9,
    TCG_REG_R10,
    TCG_REG_R11,
    TCG_REG_R12,
    TCG_REG_R13,
    TCG_REG_R14,
    TCG_REG_R15,
    TCG_REG_RAX = TCG_REG_EAX,
    TCG_REG_RCX = TCG_REG_ECX,
    TCG_REG_RDX = TCG_REG_EDX,
    TCG_REG_RBX = TCG_REG_EBX,
    TCG_REG_RSP = TCG_REG_ESP,
    TCG_REG_RBP = TCG_REG_EBP,
    TCG_REG_RSI = TCG_REG_ESI,
    TCG_REG_RDI = TCG_REG_EDI,
} TCGReg;

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

#endif /* TGTYPE_H */
