#include "defines.h"

#if VRELEASE == 1
    #define LOG_DEBUG_ENABLED 0
    #define LOG_TRACE_ENABLED 0
#else
    #define LOG_DEBUG_ENABLED 1
    #define LOG_TRACE_ENABLED 1
#endif

typedef u8 LOG_LEVEL;
#define LOG_LEVEL_FATAL (LOG_LEVEL)0
#define LOG_LEVEL_ERROR (LOG_LEVEL)1
#define LOG_LEVEL_WARN  (LOG_LEVEL)2
#define LOG_LEVEL_INFO  (LOG_LEVEL)3
#define LOG_LEVEL_DEBUG (LOG_LEVEL)4
#define LOG_LEVEL_TRACE (LOG_LEVEL)5

void logger_initialize(u64 buffer_size, char buffer[static buffer_size]);

void logger_uninitialize();

void _logger_emit_output(LOG_LEVEL level, char* message, ...);

// Logs a fatal error and should ideally be followed by application shutdown.
// Please enter 1012 charcters or less or output will be truncated
#define VFATAL(message, ...) _logger_emit_output(LOG_LEVEL_FATAL, message, ##__VA_ARGS__)

#ifndef VERROR
    // Logs a critical error that hinders proper execution.
    #define VERROR(message, ...) _logger_emit_output(LOG_LEVEL_ERROR, message, ##__VA_ARGS__)
#endif

#if LOG_WARN_ENABLED == 1
    // Logs a warning about potential issues.
    #define VWARN(message, ...) _logger_emit_output(LOG_LEVEL_WARN, message, ##__VA_ARGS__)
#else
    #define VWARN(message, ...)
#endif

#if LOG_INFO_ENABLED == 1
    // Logs standard informational messages.
    #define VINFO(message, ...) _logger_emit_output(LOG_LEVEL_INFO, message, ##__VA_ARGS__)
#else
    #define VINFO(message, ...)
#endif

#if LOG_DEBUG_ENABLED == 1
    // Logs information useful during development.
    #define VDEBUG(message, ...) _logger_emit_output(LOG_LEVEL_DEBUG, message, ##__VA_ARGS__)
#else
    #define VDEBUG(message, ...)
#endif

#if LOG_TRACE_ENABLED == 1
    // Logs extremely granular data for deep debugging.
    #define VTRACE(message, ...) _logger_emit_output(LOG_LEVEL_TRACE, message, ##__VA_ARGS__)
#else
    #define VTRACE(message, ...)
#endif