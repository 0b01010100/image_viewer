#pragma once
#include <stdint.h>
#include <stdio.h>

#define PNULL (void*)0

typedef _Bool b8;
#define false 0
#define true 1

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

#if defined(_WIN32) || defined(WIN32)
#define PLATFORM_WINDOWS 1
#elif defined(__linux__) || defined(__gnu_linux__)
#define PLATFORM_LINUX 1
#elif defined(__APPLE__)
#define PLATFORM_MACOS 1
#else
#error platform not supported
#endif
