#ifndef TCG_FUNCTIONS_H
#define TCG_FUNCTIONS_H
#include "tgtypes.h"
#ifdef __cplusplus
extern "C" {
#endif

TCGv_i64 tcg_global_mem_new_i64(int reg, intptr_t offset, const char* name);
#ifdef __cplusplus
}
#endif
#endif /* TCG_FUNCTIONS_H */
