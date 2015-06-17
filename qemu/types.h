#ifndef TYPES_H
#define TYPES_H
#include <stdint.h>
typedef uint8_t flag;
typedef uint32_t target_ulong;
typedef int32_t target_long;

#ifndef __cplusplus
typedef uint8_t bool;
#define true (bool)1
#define false (bool)0
#endif

typedef int (*fprintf_function)(FILE* f, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

#endif /* TYPES_H */
