#pragma once

#include <stdint.h>
#include <stdio.h>

#define PNULL (void*)0

typedef _Bool b8;
#define false 0
#define true 1

typedef uint8_t u8;
typedef int8_t i8;

typedef uint16_t u16;
typedef int16_t i16;

typedef uint32_t u32;
typedef int32_t i32;

typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;


#define ALLOC malloc
#define DEALLOC free

#if defined(_WIN32)
#define PLATFORM_WINDOWS 1
#elif defined(__linux__) || defined(__gnu_linux__)
#define PLATFORM_LINUX 1
#elif defined(__APPLE__) && defined(__MACH__)
#define PLATFORM_MACOS 1
#endif

#define KB(n)  ((u64)(n) * 1000ULL)
#define MB(n)  ((u64)(n) * 1000000ULL)
#define GB(n)  ((u64)(n) * 1000000000ULL)

#define KiB(n) ((u64)(n) << 10ULL)
#define MiB(n) ((u64)(n) << 20ULL)
#define GiB(n) ((u64)(n) << 30ULL)