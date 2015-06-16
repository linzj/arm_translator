#ifndef TGTYPE_H
#define TGTYPE_H
#include "types.h"

typedef struct TCGv_i32__* TCGv_i32;
typedef struct TCGv_i64__* TCGv_i64;
typedef struct TCGv_ptr__* TCGv_ptr;
typedef struct TCGv__* TCGv;
#define TARGET_FMT_lx "%08x"
#define TCG_TARGET_REG_BITS 32
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

typedef target_ulong tcg_target_long;

#endif /* TGTYPE_H */
