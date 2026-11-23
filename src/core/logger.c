#include "logger.h"
#include "memory/memory.h"
#include "platform/platform.h"
#include "string/vstring.h"
#include <stdarg.h>

#if defined(LOGGER_FATAL_BUFFER_SIZE) && LOGGER_FATAL_BUFFER_SIZE > 256
#else
#define LOGGER_FATAL_BUFFER_SIZE 1024
#endif
// backup buffer if allocation fails
char fatal_buffer[LOGGER_FATAL_BUFFER_SIZE];

void _logger_emit_output(LOG_LEVEL level, const char *fmt, ...) {
    static const char *const level_prefixes[6] = {
        "[FATAL]: ", "[ERROR]: ", "[WARN]: ",
        "[INFO]: ",  "[DEBUG]: ", "[TRACE]: ",
    };
    // select level prefix
    const char *level_prefix = level_prefixes[level];
    va_list args_size;
    va_start(args_size, fmt);

    // query the needed size for each string operation
    i32 prefix_len = string_append_string(nullptr, (char *)level_prefix);

    i32 msg_len =
        string_append_as_formatedv_string(nullptr, (char *)fmt, args_size);

    va_end(args_size);

    u64 total_len = prefix_len + msg_len;

    char *buffer = nullptr;
    int is_fatal = (level == LOG_LEVEL_FATAL);

    if (is_fatal) {
        // use emergency buffer if FATAL
        if (total_len >= sizeof(fatal_buffer)) {
            total_len = sizeof(fatal_buffer) - 1;
        }
        buffer = fatal_buffer;
    } else {
        buffer = vallocate(total_len + 1);
        if (!buffer) {
            return;
        }
    }
    // operate on the string buffer
    buffer[0] = '\0';

    string_append_string(buffer, (char *)level_prefix);

    va_list args_write;
    va_start(args_write, fmt);
    string_append_as_formatedv_string(buffer, (char *)fmt, args_write);
    va_end(args_write);

    i32 const sink = (is_fatal || level == LOG_LEVEL_ERROR) ? CONSOLE_SINK_ERR
                                                            : CONSOLE_SINK_OUT;

    platform_write_console(sink, buffer);

    if (!is_fatal) {
        vdeallocate(buffer);
    }
}
