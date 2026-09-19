#include "logger.h"
#include <string.h>
#include <stdarg.h>
#include "platform/platform.h"

typedef struct logger_state{
    u64 buffer_size;
    char* buffer;
}logger_state;

logger_state state;

void logger_initialize(u64 buffer_size, char buffer[static buffer_size])
{
    state.buffer_size = buffer_size;
    state.buffer = buffer;
}

void logger_uninitialize()
{
    if(state.buffer){
        memset(state.buffer, 0, state.buffer_size);
    }
    state.buffer = PNULL;
    state.buffer_size = 0;
}

void _logger_emit_output(LOG_LEVEL level, char* message, ...)
{
    static  char* const level_strs[6] = {"[FATAL]: ", "[ERROR]: ", "[WARN]: ", "[INFO]: ", "[DEBUG]: ", "[TRACE]: "};
    u32 const level_str_len = strlen(level_strs[level]);
    platform_copy_memory(state.buffer, level_strs[level], level_str_len);

    // [FORMAT_PART | TRANSLATE_PART]
    va_list args;
    va_start(args, message);
        vsnprintf(
            state.buffer + level_str_len, (state.buffer_size - level_str_len) / 2,message, args
        );
    va_end(args);

    platform_string pmessage =
        (platform_string)state.buffer + (state.buffer_size - level_str_len) / 2;

    utf8_to_platform_string(state.buffer,pmessage,(state.buffer_size - level_str_len) / 2);
    
    platform_write_console(
        (level == LOG_LEVEL_FATAL || level == LOG_LEVEL_ERROR)
            ? CONSOLE_SINK_ERR
            : CONSOLE_SINK_OUT,
        pmessage
    );
}