#pragma once

#include <stdint.h>

#if !defined(nullptr) &&                                                       \
    (!defined(__STDC_VERSION__) || __STDC_VERSION__ <= 201710)
/* -Wundef is avoided by using short circuiting in the condition */
#define nullptr ((void *)0)
#endif

#define UNUSED(var) ((void)(var))

typedef _Bool b8;
#define false 0
#define true 1

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

#define U8_MIN UINT8_MIN
#define U8_MAX UINT8_MAX
#define I8_MIN INT8_MIN
#define I8_MAX INT8_MAX

#define U16_MIN UINT16_MIN
#define U16_MAX UINT16_MAX
#define I16_MIN INT16_MIN
#define I16_MAX INT16_MAX

#define U32_MIN UINT32_MIN
#define U32_MAX UINT32_MAX
#define I32_MIN INT32_MIN
#define I32_MAX INT32_MAX

#define U64_MIN UINT64_MIN
#define U64_MAX UINT64_MAX
#define I64_MIN INT64_MIN
#define I64_MAX INT64_MAX

#ifdef _WIN32
#define PLATFORM_WINDOWS 1
#elif defined(__linux__) || defined(__gnu_linux__)
#define PLATFORM_LINUX 1
#elif defined(__APPLE__) && defined(__MACH__)
#define PLATFORM_MACOS 1
#endif

#define KB(n) ((u64)(n) * 1000ULL)
#define MB(n) ((u64)(n) * 1000000ULL)
#define GB(n) ((u64)(n) * 1000000000ULL)

#define KiB(n) ((u64)(n) << 10ULL)
#define MiB(n) ((u64)(n) << 20ULL)
#define GiB(n) ((u64)(n) << 30ULL)

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define STATIC_ASSERT _Static_assert
#elif defined(_MSC_VER)
#define STATIC_ASSERT static_assert
#else
#define STATIC_ASSERT(cond, msg)                                               \
    typedef char static_assertion_##__LINE__[(cond) ? 1 : -1]
#endif
// should check align and address for a valid number
#define get_up_align(address, align) (((address) + ((align) - 1)) & ~((align) - 1))