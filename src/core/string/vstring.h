#include "../defines.h"
#include <stdarg.h>
u32 string_lenght(char *data);

int string_append_whole_number(u64 dest_addr, u64 src);

int string_append_string(char *dest, char *src);

int string_as_formated_string(char *format, ...);

int string_append_as_formatedv_string(char *dest, char *format,
                                      va_list arg_vec);

int string_append_as_formated_string(char *dest, char *format, ...);
