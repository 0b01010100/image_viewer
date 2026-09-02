#include "application.h"
#include <stdlib.h>

#if defined(PLATFORM_WINDOWS)

extern u64 win32_wutf16_to_utf8(const wchar_t* wutf16_str, char* utf8_str, u64 utf8_str_size);
int wmain(int argc, wchar_t* wargv[])
{
    u64 chars = 0;
    for (int i = 0; i < argc; i++) {
        chars += win32_wutf16_to_utf8(wargv[i], PNULL, 0);
    }

    char** argv = malloc(
        sizeof(char*) * (argc + 1) + chars
    );

    char* data = (char*)(argv + argc + 1);

    for (int i = 0; i < argc; i++) {
        argv[i] = data;

        u64 bytes = win32_wutf16_to_utf8(
            wargv[i],
            data,
            chars
        );

        data += bytes;
        chars -= bytes;
    }
    argv[argc] = PNULL;
    int ret = app_main(argc, argv);
    free(argv);
    return ret;
}
#else
int main(int argc, char** argv)
{
    return app_main(argc, argv);
}
#endif