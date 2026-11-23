// CODE FOR MICROSOFT WINDOWS
#include "platform.h"

#ifdef PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <wchar.h>
#include <windows.h>
#ifdef _MSC_VER
#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
#endif
#include "../containers/vec.h"
#include "../logger.h"
#include "../memory/memory.h"

#if defined(PLATFORM_WIN32_WRITE_CONSOLE_BYTE_MAX)
#if PLATFORM_WIN32_WRITE_CONSOLE_BYTE_MAX < 1024
#error "PLATFORM_WIN32_WRITE_CONSOLE_BYTE_MAX must be at least 1024"
#endif
#else
#define PLATFORM_WIN32_WRITE_CONSOLE_BYTE_MAX 1024
#endif

#define PLATFORM_WIN32_WRITE_CONSOLE_WCHARS_MAX                                \
    (PLATFORM_WIN32_WRITE_CONSOLE_BYTE_MAX / sizeof(WCHAR))

/* Max decimal digits for a 32-bit unsigned value (4294967295 -> 10 digits). */
#define U32_MAX_DEC_DIGITS 10

#define PLATFORM_WIN32_TRUNCATION_EXAMPLE_STRING                               \
    L" chars truncated. set PLATFORM_WIN32_WRITE_CONSOLE_BYTE_MAX= "

#define PLATFORM_WIN32_TRUNCATION_MSG_WCHARS                                   \
    ((U32_MAX_DEC_DIGITS * 2) +                                                \
     (sizeof(PLATFORM_WIN32_TRUNCATION_EXAMPLE_STRING) / sizeof(WCHAR)) + 1)

STATIC_ASSERT(PLATFORM_WIN32_WRITE_CONSOLE_WCHARS_MAX >=
                  PLATFORM_WIN32_TRUNCATION_MSG_WCHARS,
              "PLATFORM_WIN32_WRITE_CONSOLE_WCHARS_MAX is too small for the "
              "truncation warning.");

static wchar_t *platform_win32_basic_window_class =
    L"win32_basic_window_class_WC";

i32 platform_win32_utf8_to_wutf16(const char *utf8_str, wchar_t *wutf16_buffer,
                                  i32 wutf16_buffer_capacity);

i32 platform_win32_wutf16_to_utf8(const wchar_t *wutf16_str, char *utf8_buffer,
                                  i32 utf8_buffer_capacity);

// Source - https://stackoverflow.com/a/79172197
// Posted by xKaihatsu, modified by community. See post 'Timeline' for change
// history Retrieved 2026-11-07, License - CC BY-SA 4.0
// Converts windows wide-shell args form to utf8 exec form args
i32 platform_win32_CommandLineWToUTF8v(u64 *memory_requirement, void *memory,
                                       LPCWSTR lpCmdline, char ***out_argv,
                                       int *out_argc);

struct platform_state *state_ptr;
vwindow *vwindow_from_HWND(HWND handle);
static LRESULT CALLBACK WindowProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

typedef struct win32_handle_info {
    HINSTANCE instance;
} win32_handle_info;

struct platform_state {
    win32_handle_info handle;
    vwindow **windows; // vec
    platform_window_resize_callback window_resize_callback;
    platform_window_close_callback window_close_callback;
    command_line_args args;
    char argv_storage[]; // C99
};

struct vwindow_platform_state {
    HWND hwnd;
};

b8 platform_initialize(u64 *memory_requirement, void *memory,
                       struct platform_state *state) {
    UNUSED(platform_win32_wutf16_to_utf8); // Stop warning this: could be used
                                           // by plugins that need it
    if (state_ptr) {
        return false;
    }

    if (!state) {
        u64 total_required = 0;
        if (platform_win32_CommandLineWToUTF8v(&total_required, nullptr,
                                               GetCommandLineW(), nullptr,
                                               nullptr) == -1) {
            VFATAL("failed to parse command line arguments. ");
            return false;
        }
        total_required += sizeof(platform_state);
        *memory_requirement = total_required;
        return true;
    }
    if (!memory) {
        VERROR("memory can't be null");
        return false;
    }

    state_ptr = state;
    state_ptr->handle.instance = GetModuleHandleW(nullptr);

    // Basic Window Description
    HBRUSH const black_brush = (HBRUSH)GetStockObject(BLACK_BRUSH);
    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(WNDCLASSEXW);
    window_class.hInstance = state_ptr->handle.instance;
    window_class.lpfnWndProc = WindowProcW;
    window_class.lpszClassName = platform_win32_basic_window_class;
    window_class.hbrBackground = black_brush;

    // Register window class for later use
    ATOM const reg = RegisterClassExW(&window_class);
    if (!reg) {
        DWORD const last_error = GetLastError();
        LPWSTR wmessage_buf = nullptr;
        u64 const size = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, last_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&wmessage_buf, 0, nullptr);

        if (size) {
            MessageBoxW(nullptr, wmessage_buf, L"Window registration failed",
                        MB_ICONERROR | MB_OK);
            LocalFree(wmessage_buf);
        }
        VFATAL("Failed to register window class.");
        vdeallocate(state_ptr);
        return false;
    }

    state_ptr->windows = vec_create(vwindow *, 1);
    state_ptr->window_resize_callback = nullptr;
    state_ptr->window_close_callback = nullptr;
    // Finally do the needed conversion on the allocated args storage buffer
    if (platform_win32_CommandLineWToUTF8v(
            nullptr, state_ptr->argv_storage, GetCommandLineW(),
            &state_ptr->args.argv, &state_ptr->args.argc) == -1) {
        VFATAL("Windows Wide Char args parse faild");
    };
    return true;
}

void platform_uninitialize(void) {
    if (!state_ptr) {
        VFATAL("Double free corruption");
        return;
    }
    u32 const len = vec_length(state_ptr->windows);
    for (u32 i = 0; i < len; ++i) {
        if (state_ptr->windows[i] != nullptr) {
            DestroyWindow(state_ptr->windows[i]->platform_state->hwnd);
            state_ptr->windows[i]->platform_state->hwnd = nullptr;
            vdeallocate(state_ptr->windows[i]->platform_state);
            state_ptr->windows[i]->platform_state = nullptr;
            state_ptr->windows[i] = nullptr;
        }
    }
    vec_destroy(state_ptr->windows);
    state_ptr->windows = nullptr;
    state_ptr->window_resize_callback = nullptr;
    state_ptr->window_close_callback = nullptr;
    UnregisterClassW(platform_win32_basic_window_class,
                     state_ptr->handle.instance);
    state_ptr = nullptr;
}

command_line_args platform_get_command_line_args(void) {
    return state_ptr->args;
}

b8 platform_window_create(const vwindow_config *config, vwindow *window) {
    if (!state_ptr) {
        return false;
    }

    (void)window->width;
    (void)window->height;

    vwindow_platform_state *window_state =
        vallocate(sizeof(vwindow_platform_state));
    if (!window_state) {
        return false;
    }
    DWORD const dwStyle = WS_OVERLAPPEDWINDOW;
    DWORD const dwStyleEx = WS_EX_APPWINDOW | WS_EX_OVERLAPPEDWINDOW;

    RECT wr = {};
    AdjustWindowRectEx(&wr, dwStyle, FALSE, dwStyleEx);

    i32 const window_width = (wr.right - wr.left) + config->width;
    i32 const window_height = (wr.bottom - wr.top) + config->height;

    i32 const window_x = config->xposition + wr.left;
    i32 const window_y =
        config->yposition; // + wr.top moves the window to far up for me

    i32 const to_alloc =
        platform_win32_utf8_to_wutf16(config->title, nullptr, 0);
    if (to_alloc <= 0) {
        VERROR("failed to convert string to something win32 and understand");
        return false;
    }
    void *wtitle_buffer = vallocate(to_alloc);
    platform_win32_utf8_to_wutf16(config->title, wtitle_buffer, to_alloc);

    window_state->hwnd = CreateWindowExW(
        dwStyleEx, platform_win32_basic_window_class, wtitle_buffer, dwStyle,
        window_x, window_y, window_width, window_height, nullptr, nullptr,
        state_ptr->handle.instance, nullptr);

    if (!window_state->hwnd) {
        VERROR("failed to create window");
        vdeallocate(wtitle_buffer);
        vdeallocate(window_state);
        return false;
    }
    vdeallocate(wtitle_buffer);
    ShowWindow(window_state->hwnd, SW_SHOW);
    window->width = config->width;
    window->height = config->height;
    window->platform_state = window_state;
    window->renderer_state = nullptr;

    vec_push(state_ptr->windows, &window);
    return true;
}

void platform_window_destroy(vwindow *window) {
    if (!window || !window->platform_state) {
        return;
    }
    u32 const len = vec_length(state_ptr->windows);
    for (u32 i = 0; i < len; ++i) {
        if (state_ptr->windows[i] == window) {
            DestroyWindow(window->platform_state->hwnd);
            //[0][nullptr][2][3][4]...
            //   ^--freed window
            // In platform_window_create I call
            // darray_push(state_ptr->windows, out_handle); this could break
            // because old indexes could not be resued, though it breaks
            // from many creates and destroys.
            vdeallocate(window->platform_state); // once fixed I may remove
                                                 // this, for reuse.
            window->platform_state = nullptr;
            state_ptr->windows[i] = nullptr;
            return;
        }
    }
    VERROR("Destroying a window that was somehow not registered with the "
           "platform layer.\n");
    DestroyWindow(window->platform_state->hwnd);
    window->platform_state->hwnd = nullptr;
}

void platform_window_present_frame(vwindow *window, u8 *pixel_map) {
    HDC hdc = GetDC(window->platform_state->hwnd);

    // https://gist.github.com/taxilian/1068352
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFO);
    int const biBitCount = 32;
    bmi.bmiHeader.biBitCount = biBitCount;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biSizeImage =
        window->width * window->height * (u32)sizeof(u32);
    bmi.bmiHeader.biWidth = (LONG)window->width;
    bmi.bmiHeader.biHeight = -(LONG)window->height;

    // should I use SetDIBitsToDevice or BitBlt
    SetDIBitsToDevice(hdc, 0, 0, window->width, window->height, 0, 0, 0,
                      window->height, pixel_map, &bmi, DIB_RGB_COLORS);

    // without this the program was lagging so bad
    ReleaseDC(window->platform_state->hwnd, hdc);
}

void platform_process_events(void) {
    MSG Msg;
    while (PeekMessageW(&Msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&Msg);
        DispatchMessageW(&Msg);
    }
}

void platform_set_window_resize_callback(
    platform_window_resize_callback callback) {
    state_ptr->window_resize_callback = callback;
}

void platform_set_window_close_callback(
    platform_window_close_callback callback) {
    state_ptr->window_close_callback = callback;
}

void platform_write_console(CONSOLE_SINK sink, const char *message) {
    wchar_t stackBuffer[PLATFORM_WIN32_WRITE_CONSOLE_WCHARS_MAX];
    int const wcharsNeeded =
        MultiByteToWideChar(CP_UTF8, 0, message, -1, nullptr, 0);
    // or we could use a while loop instead of truncating
    if (wcharsNeeded > PLATFORM_WIN32_WRITE_CONSOLE_WCHARS_MAX) {
        VWARN("%d chars truncated. set PLATFORM_WRITE_CONSOLE_BYTE_MAX = %d",
              wcharsNeeded - ARRAYSIZE(stackBuffer), wcharsNeeded);
        return;
    }

    MultiByteToWideChar(CP_UTF8, 0, message, -1, stackBuffer,
                        ARRAYSIZE(stackBuffer));

    DWORD const StdHandle =
        (sink == CONSOLE_SINK_OUT) ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE;
    HANDLE ConsoleOutput = GetStdHandle(StdHandle);
    DWORD wcharsWritten = 0;

    WriteConsoleW(ConsoleOutput, stackBuffer, wcharsNeeded - 1, &wcharsWritten,
                  nullptr);
}

b8 platform_virtual_reserve(vvirtual_memory *virtual, u64 to_reserve) {
    virtual->base =
        VirtualAlloc(nullptr, to_reserve, MEM_RESERVE, PAGE_NOACCESS);
    if (virtual->base) {
        virtual->reserved = to_reserve;
        virtual->committed = 0;
        return true;
    }
    return false;
}

void *platform_virtual_commit(vvirtual_memory *virtual, u64 to_commit) {
    void *commit_target = (u8 *)virtual->base + virtual->committed;

    void *requested =
        VirtualAlloc(commit_target, to_commit, MEM_COMMIT, PAGE_READWRITE);
    if (requested) {
        virtual->committed += to_commit;
    }

    return requested;
}

void platform_virtual_decommit(vvirtual_memory *virtual, u64 to_decommit) {
    void *decommit_target =
        (u8 *)virtual->base + virtual->committed - to_decommit;
    VirtualFree(decommit_target, to_decommit, MEM_DECOMMIT);
    virtual->committed -= to_decommit;
}

void platform_virtual_unreserve(vvirtual_memory *virtual) {
    VirtualFree(virtual->base, 0, MEM_RELEASE);

    virtual->base = nullptr;
    virtual->reserved = 0;
    virtual->committed = 0;
}

void *platform_heap_allocate(u64 to_alloc) {
    return HeapAlloc(GetProcessHeap(), 0, to_alloc);
}

void platform_heap_deallocate(void *memory) {
    HeapFree(GetProcessHeap(), 0, memory);
}

void *platform_zero_memory(void *memory, u64 memory_size) {
    return memset(memory, 0, memory_size); // ZeroMemory
}

void *platform_set_memory(void *memory, i32 value, u64 memory_size) {
    return memset(memory, value, memory_size);
}

void *platform_copy_memory(void *dest, const void *src, u64 memory_size) {
    return memcpy(dest, src, memory_size);
}

vwindow *vwindow_from_HWND(HWND handle) {
    if (!state_ptr || !state_ptr->windows) {
        return 0;
    }
    for (u64 i = 0; i < vec_length(state_ptr->windows); i++) {
        if (!state_ptr->windows[i]) {
            continue;
        }
        vwindow_platform_state *platform_state =
            state_ptr->windows[i]->platform_state;
        if (platform_state && platform_state->hwnd == handle) {
            return state_ptr->windows[i];
        }
    }
    return 0;
}

static LRESULT CALLBACK WindowProcW(HWND hWnd, UINT Msg, WPARAM wParam,
                                    LPARAM lParam) {
    switch (Msg) {
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rect;
        GetClientRect(hWnd, &rect);
        FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    }
        return 1;

    case WM_CLOSE: {
        vwindow *window = vwindow_from_HWND(hWnd);
        if (window && state_ptr->window_close_callback) {
            state_ptr->window_close_callback(window);
        }
    }
        return 0;
    case WM_SIZE: {
        vwindow *window = vwindow_from_HWND(hWnd);
        if (window && state_ptr->window_resize_callback) {
            if (window->width != LOWORD(lParam) ||
                window->height != HIWORD(lParam)) {
                window->width = LOWORD(lParam);
                window->height = HIWORD(lParam);
                state_ptr->window_resize_callback(window);
            }
        }
        // InvalidateRect(hWnd, nullptr, TRUE);
    } break;
    default: {
        // case default
    } break;
    }
    return DefWindowProcW(hWnd, Msg, wParam, lParam);
}

#ifndef PLATFORM_NO_ENTRY_POINT
// may use a macro for this to allow name change via compiler flag
extern int application_main(void);
// Handle Subsystem CONSOLE AND WINDOWS
#ifndef PLATFORM_WIN32_CONSOLE
// int platform_main()
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine,
                   int nCmdShow) {
    UNUSED(hInstance);
    UNUSED(hPrevInstance);
    UNUSED(lpCmdLine);
    UNUSED(nCmdShow);
    return application_main();
}

#else
// int platform_main()
int main(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);
    return application_main();
}
#endif
#endif
// WIN32 utils

i32 platform_win32_utf8_to_wutf16(const char *utf8_str, wchar_t *wutf16_buffer,
                                  i32 wutf16_buffer_capacity) {
    if (!utf8_str) {
        return 0;
    }

    i32 const result = MultiByteToWideChar(
        CP_UTF8, 0, utf8_str, -1, wutf16_buffer, wutf16_buffer_capacity);

    if (result == 0) {
        DWORD const error = GetLastError();

        switch (error) {
        case ERROR_NO_UNICODE_TRANSLATION:
            VERROR("Invalid UTF-8 input.");
            break;

        case ERROR_INSUFFICIENT_BUFFER:
            VERROR("UTF-16 output buffer is too small.");
            break;

        default:
            VERROR("MultiByteToWideChar failed: %lu", error);
            break;
        }

        return 0;
    }

    return result;
}

i32 platform_win32_wutf16_to_utf8(const wchar_t *wutf16_str, char *utf8_buffer,
                                  i32 utf8_buffer_capacity) {
    if (!wutf16_str) {
        return 0;
    }

    i32 const result = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, wutf16_str, -1, utf8_buffer,
        utf8_buffer_capacity, nullptr, nullptr);

    if (result == 0) {
        DWORD const error = GetLastError();

        switch (error) {
        case ERROR_NO_UNICODE_TRANSLATION: {
            VERROR("Invalid UTF-16 input: unpaired surrogate.");
            break;
        }
        case ERROR_INSUFFICIENT_BUFFER: {
            VERROR("UTF-8 output buffer is too small.");
            break;
        }
        default: {
            VERROR("WideCharToMultiByte failed: %lu", error);
            break;
        }
        }

        return 0;
    }

    return result;
}
// Source - https://stackoverflow.com/a/13872211
// Posted by Cheers and hth. - Alf, modified by community. See post
// 'Timeline' for change history Retrieved 2026-11-06, License - CC
// BY-SA 4.0 MIX of stack overflow and AI to create this function
/**
 * Parses or queries Windows command line arguments using engine-provided
 * memory.
 *
 * - If `memory == nullptr`: Calculates the required size and writes it to
 * `memory_requirement`.
 * - If `memory != nullptr`: Parses the command line directly into the `memory`
 * buffer, setting `out_argv` and `out_argc`.
 *
 * Returns: 0 on success, or a non-zero error code on failure.
 */
i32 platform_win32_CommandLineWToUTF8v(u64 *memory_requirement, void *memory,
                                       LPCWSTR lpCmdline, char ***out_argv,
                                       int *out_argc) {
    if (!lpCmdline || *lpCmdline == L'\0') {
        if (memory_requirement) {
            *memory_requirement = 0;
        }
        if (out_argc) {
            *out_argc = 0;
        }
        if (out_argv) {
            *out_argv = nullptr;
        }
        return ERROR_INVALID_PARAMETER;
    }

    // --- PHASE 1: Count arguments (using wide string for counting pass)
    // ---
    int argc = 1;
    LPCWSTR ws = lpCmdline;

    if (*ws == L'"') {
        ws++;
        while (*ws) {
            if (*ws++ == L'"') {
                break;
            }
        }
    } else {
        while (*ws && *ws != L' ' && *ws != L'\t') {
            ws++;
        }
    }

    while (*ws == L' ' || *ws == L'\t') {
        ws++;
    }
    if (*ws) {
        argc++;
    }
    int qcount = 0;
    int bcount = 0;
    while (*ws) {
        if ((*ws == L' ' || *ws == L'\t') && qcount == 0) {
            while (*ws == L' ' || *ws == L'\t') {
                ws++;
            }
            if (*ws) {
                argc++;
            }
            bcount = 0;
        } else if (*ws == L'\\') {
            bcount++;
            ws++;
        } else if (*ws == L'"') {
            if ((bcount & 1) == 0) {
                qcount++;
            }
            ws++;
            bcount = 0;
            while (*ws == L'"') {
                qcount++;
                ws++;
            }
            qcount = qcount % 3;
            if (qcount == 2) {
                qcount = 0;
            }
        } else {
            bcount = 0;
            ws++;
        }
    }

    // Determine exact buffer size needed for UTF-8 conversion of the
    // command line
    int const utf8_len_with_nul = WideCharToMultiByte(
        CP_UTF8, 0, lpCmdline, -1, nullptr, 0, nullptr, nullptr);
    if (utf8_len_with_nul <= 0) {
        return ERROR_INVALID_DATA;
    }

    // --- CALCULATE REQUIRED MEMORY SIZE ---
    // Layout:
    // 1. Pointer array: (argc + 1) * sizeof(char*)
    // 2. UTF-8 string buffer: utf8_len_with_nul bytes
    u64 const ptr_array_size = (u64)(argc + 1) * sizeof(char *);
    u64 total_size = ptr_array_size + (u64)utf8_len_with_nul;

    if (memory_requirement) {
        *memory_requirement = total_size;
    }

    // If memory is nullptr, this was just a size query call
    if (!memory) {
        if (out_argc) {
            *out_argc = argc;
        }
        return 0;
    }

    if (!out_argv || !out_argc) {
        return ERROR_INVALID_PARAMETER;
    }

    // --- PHASE 2: Populate memory with UTF-8 strings ---
    char **argv = (char **)memory;
    char *cmdline = (char *)memory + ptr_array_size;

    // Convert wide command line directly into our engine's UTF-8 string
    // buffer
    WideCharToMultiByte(CP_UTF8, 0, lpCmdline, -1, cmdline, utf8_len_with_nul,
                        nullptr, nullptr);

    // Parse and unescape directly on the UTF-8 buffer
    char *d = cmdline;
    char *s = cmdline;
    argv[0] = d;
    int current_argc = 1;

    // First argument (executable path)
    if (*d == '"') {
        s = d + 1;
        while (*s) {
            if (*s == '"') {
                s++;
                break;
            }
            *d++ = *s++;
        }
    } else {
        while (*d && *d != ' ' && *d != '\t') {
            d++;
        }
        s = d;
        if (*s) {
            s++;
        }
    }

    *d++ = '\0';

    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (!*s) {
        argv[current_argc] = nullptr;
        *out_argv = argv;
        *out_argc = current_argc;
        return 0;
    }

    argv[current_argc++] = d;
    qcount = bcount = 0;

    while (*s) {
        if ((*s == ' ' || *s == '\t') && qcount == 0) {
            *d++ = '\0';
            bcount = 0;

            do {
                s++;
            } while (*s == ' ' || *s == '\t');

            if (*s) {
                argv[current_argc++] = d;
            }
        } else if (*s == '\\') {
            *d++ = *s++;
            bcount++;
        } else if (*s == '"') {
            if ((bcount & 1) == 0) {
                d -= bcount / 2;
                qcount++;
            } else {
                d = d - ((bcount / 2) - 1);
                *d++ = '"';
            }
            s++;
            bcount = 0;

            while (*s == '"') {
                if (++qcount == 3) {
                    *d++ = '"';
                    qcount = 0;
                }
                s++;
            }
            if (qcount == 2) {
                qcount = 0;
            }
        } else {
            *d++ = *s++;
            bcount = 0;
        }
    }

    *d = '\0';
    argv[current_argc] = nullptr;

    *out_argv = argv;
    *out_argc = current_argc;

    return 0;
}
#endif
