#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h> /* For SYS_xxx definitions */
#ifdef __ANDROID__
#include <android/log.h>
#define TAG "libhoudini"
#endif

#include <time.h>

#ifdef LLVMLOG_LEVEL
static FILE* g_log = stdout;

void __my_log(char type, const char* fmt, ...)
{
    if (!g_log)
        return;
    va_list args;
    va_start(args, fmt);
    int bytes = vsnprintf(nullptr, 0, fmt, args);
    va_end(args);
    char buf[bytes + 2];
    va_start(args, fmt);
    vsnprintf(buf, bytes + 1, fmt, args);
    va_end(args);
    if (buf[bytes - 1] != '\n') {
        buf[bytes] = '\n';
        buf[bytes + 1] = '\0';
    }
#ifndef __ANDROID__
    fprintf(g_log, "%c:%lu:%lu: ", type, (long)syscall(__NR_gettid, 0), (long)clock());
    fputs(buf, g_log);
    fflush(g_log);
#else
    int android_type;
    switch (type) {
    case 'V':
        android_type = ANDROID_LOG_VERBOSE;
        break;
    case 'P':
    case 'D':
        android_type = ANDROID_LOG_DEBUG;
        break;
    case 'E':
        android_type = ANDROID_LOG_ERROR;
        break;
    default:
        android_type = ANDROID_LOG_INFO;
        break;
    }
    __android_log_write(android_type, TAG, buf);
#endif
}

void __my_assert_fail(const char* msg, const char* file_name, int lineno)
{
#ifndef __ANDROID__
    if (!g_log) {
        __builtin_trap();
    }
    fprintf(g_log, "%lu:%lu: ASSERT FAILED:%s:%s:%d.\n", (long)syscall(__NR_gettid, 0), (long)clock(), msg, file_name, lineno);
    fflush(g_log);
#else
    __android_log_print(ANDROID_LOG_ERROR, TAG, "ASSERT FAILED:%s:%s:%d.", msg, file_name, lineno);
#endif
    __builtin_trap();
}
#endif
