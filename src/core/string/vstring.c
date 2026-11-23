#include "vstring.h"
#include <stdio.h>
#include <string.h>
// temperary

u32 string_lenght(char *data) {
    return strlen(data);
}

int string_append_string(char *dest, char *src) {
    if (!src){
        return 0;
    }
    
    if (dest == nullptr) {
        return (int)string_lenght(src);
    }

    char *d = dest;
    while (*d != '\0') {
        d++;
    }

    int count = 0;
    while (*src != '\0') {
        *d++ = *src++;
        count++;
    }
    *d = '\0';

    return count;
}

int string_as_formated_string(char *format, ...) {
    if (!format) {
        return 0;
    }
    va_list args;
    va_start(args, format);
    int required = vsnprintf(nullptr, 0, format, args);
    va_end(args);

    return required >= 0 ? required : 0;
}

int string_append_as_formatedv_string(char *dest, char *format,
                                      va_list arg_vec) {
    if (!format) {
        return 0;
    }
    va_list args_copy;
    va_copy(args_copy, arg_vec);
    int required = vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);

    if (dest == nullptr) {
        return required >= 0 ? required : 0;
    }

    char *d = dest;
    while (*d != '\0') {
        d++;
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    int written = vsprintf(d, format, arg_vec);
#pragma clang diagnostic pop
    return written >= 0 ? written : 0;
}

int string_append_as_formated_string(char *dest, char *format, ...) {
    if (!format) {
        return 0;
    }
    va_list args;
    va_start(args, format);
    int written = string_append_as_formatedv_string(dest, format, args);
    va_end(args);

    return written;
}