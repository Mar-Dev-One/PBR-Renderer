#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef signed char        int8;
typedef short              int16;
typedef int                int32;
typedef long long          int64;

typedef unsigned char      uint8;
typedef unsigned short     uint16;
typedef unsigned int       uint32;
typedef unsigned long long uint64;

typedef float    f32;
typedef double   f64;

typedef bool     b8;



#if defined(__GNUC__) || defined(__clang__)
    #define LIKELY(x)   (__builtin_expect(!!(x), 1))
    #define UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
#endif



#if defined(_MSC_VER)
    #define DEBUG_BREAK() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
    #define DEBUG_BREAK() __builtin_trap()
#else
    #include <signal.h>
    #define DEBUG_BREAK() raise(SIGTRAP)
#endif



#define STATIC_ASSERT(expr, msg) _Static_assert(expr, msg)



typedef enum log_level
{
    LOG_LEVEL_TRACE,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} log_level;



static inline void
log_message(log_level level,
            const char* file,
            int line,
            const char* fmt,
            ...)
{
    static const char* names[] =
    {
        "TRACE",
        "DEBUG",
        "INFO ",
        "WARN ",
        "ERROR",
        "FATAL"
    };

#if defined(_WIN32)
    static const char* colors[] =
    {
        "\x1b[90m",
        "\x1b[36m",
        "\x1b[32m",
        "\x1b[33m",
        "\x1b[31m",
        "\x1b[35m"
    };
#else
    static const char* colors[] =
    {
        "\033[90m",
        "\033[36m",
        "\033[32m",
        "\033[33m",
        "\033[31m",
        "\033[35m"
    };
#endif

    FILE* stream =
        (level >= LOG_LEVEL_WARN)
        ? stderr
        : stdout;

    fprintf(stream,
            "%s[%s] %s:%d: ",
            colors[level],
            names[level],
            file,
            line);

    va_list args;
    va_start(args, fmt);
    vfprintf(stream, fmt, args);
    va_end(args);

    fprintf(stream, "\033[0m");
}



#define LOG_TRACE(...) \
    log_message(LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_DEBUG(...) \
    log_message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_INFO(...) \
    log_message(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_WARN(...) \
    log_message(LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_ERROR(...) \
    log_message(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)



#define FATAL(...)                                      \
    do                                                  \
    {                                                   \
        log_message(LOG_LEVEL_FATAL,                    \
                    __FILE__,                           \
                    __LINE__,                           \
                    __VA_ARGS__);                       \
        abort();                                        \
    } while (0)



#ifndef NDEBUG

#define ASSERT(expr)                                    \
    do                                                  \
    {                                                   \
        if (UNLIKELY(!(expr)))                          \
        {                                               \
            LOG_ERROR("Assertion failed: %s", #expr);   \
            DEBUG_BREAK();                              \
            assert(expr);                               \
        }                                               \
    } while (0)

#else

#define ASSERT(expr) ((void)0)

#endif



#ifndef NDEBUG

#define VERIFY(expr) ASSERT(expr)

#else

#define VERIFY(expr) ((void)(expr))

#endif



#define UNREACHABLE()                                   \
    do                                                  \
    {                                                   \
        FATAL("Unreachable code reached.");             \
    } while (0)



#define TODO(msg)                                       \
    LOG_WARN("TODO: %s", msg)

