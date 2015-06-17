#ifndef TYPES_H
#define TYPES_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
typedef uint8_t flag;
typedef uint64_t target_ulong;
typedef int64_t target_long;
typedef uint16_t float16;
typedef uint32_t float32;
typedef uint64_t float64;

#ifndef __cplusplus
typedef uint8_t bool;
#define true (bool)1
#define false (bool)0
#endif

#define xglue(x,y) x ## y
#define glue(x,y) xglue(x, y)
typedef int (*fprintf_function)(FILE* f, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

#endif /* TYPES_H */
