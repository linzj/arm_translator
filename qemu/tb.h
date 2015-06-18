#ifndef TB_H
#define TB_H
#include "tgtypes.h"
typedef uintptr_t tb_page_addr_t;

struct TranslationBlock {
    target_ulong pc;   /* simulated PC corresponding to this block (EIP + CS base) */
    uint64_t flags; /* flags defining in which context the code was generated */
    uint16_t size;      /* size of target code for this block (1 <=
                           size <= TARGET_PAGE_SIZE) */
#define CF_COUNT_MASK  0x7fff
#define CF_LAST_IO     0x8000 /* Last insn may be an IO access.  */
    uint32_t icount;
};
typedef struct TranslationBlock TranslationBlock;
#endif /* TB_H */
