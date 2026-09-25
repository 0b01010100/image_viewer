// CODE FOR MICROSOFT WINDOWS
#include "platform.h"

#if defined(PLATFORM_WINDOWS)
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <wchar.h>

// not necessary but added anyway
#pragma comment(lib,"kernel32")
#pragma comment(lib,"user32")
#pragma comment(lib,"gdi32")

#include "../containers/vec.h"
#include "../logger.h"

#define PL_WINDOW_CLASS L"PL_WINDOW_CLASS_WC"
u32 utf8_to_wutf16(char* utf8_str, wchar_t* plf_str_len, u32 utf16_str_len);
u32 wutf16_to_utf8(wchar_t* plf_str_len, char* utf8_str, u32 utf8_str_len);
typedef wchar_t* platform_string_internal;
static struct platform_state* state_ptr;
static vwindow *vwindow_from_HWND(HWND handle);
LRESULT CALLBACK WindowProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

typedef struct win32_handle_info {
    HINSTANCE instance;
} win32_handle_info; 


struct platform_state
{
    win32_handle_info handle;
    vwindow** windows; // vec
    platform_window_resize_callback window_resize_callback;
    platform_window_close_callback window_close_callback;
};

struct vwindow_platform_state
{
    HWND hwnd;
};


b8 platform_initialize()
{
    state_ptr = ALLOC(sizeof(struct platform_state));
    if(!state_ptr){
        return false;
    }
    state_ptr->handle.instance = GetModuleHandleW(PNULL);
    
    // Basic Window Description
    HBRUSH black_brush = (HBRUSH)GetStockObject(BLACK_BRUSH);
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.hInstance = state_ptr->handle.instance;
    wc.lpfnWndProc = WindowProcW;
    wc.lpszClassName = PL_WINDOW_CLASS;
    wc.hbrBackground = black_brush;

    // Register the Sctrion for later use
    ATOM reg = RegisterClassExW(&wc);
    if(!reg){

        DWORD const last_error = GetLastError();
        LPWSTR wmessage_buf = PNULL;
        u64 size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
								  NULL, last_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&wmessage_buf, 0, NULL);
        
        if (size) {
            MessageBoxW(PNULL, wmessage_buf, L"Window registration failed",
                        MB_ICONERROR | MB_OK);
            LocalFree(wmessage_buf);
        }
        VFATAL("Failed to register window class.");
        DEALLOC(state_ptr);
        return false;
    }

    state_ptr->windows = vec_create(vwindow*, 1);
    state_ptr->window_resize_callback = PNULL;
    state_ptr->window_close_callback = PNULL;
    return true;
}

void platform_uninitalize()
{
    if(!state_ptr){
        VFATAL("Double free corruption");
        return;
    }
    u32 len = vec_length(state_ptr->windows);
    for (u32 i = 0; i < len; ++i) {
        if (state_ptr->windows[i] != PNULL) {
            DestroyWindow(state_ptr->windows[i]->platform_state->hwnd);
            state_ptr->windows[i]->platform_state->hwnd = PNULL;
            free(state_ptr->windows[i]->platform_state);
            state_ptr->windows[i]->platform_state = PNULL;
        }
    }
    vec_destroy(state_ptr->windows);
    state_ptr->windows = PNULL;
    state_ptr->window_resize_callback = PNULL;
    state_ptr->window_close_callback = PNULL;
    
    DEALLOC(state_ptr);
    UnregisterClassW(PL_WINDOW_CLASS, state_ptr->handle.instance);
    state_ptr = PNULL;
}

b8 platform_window_create(platform_string title, u32 width, u32 height, u32 x, u32 y, vwindow* window)
{
    platform_string_internal ititle = title;
    if(!state_ptr){
        return false;
    }

    (void)window->width;
    (void)window->height;

    vwindow_platform_state* window_state = ALLOC(sizeof(vwindow_platform_state));
    if(!window_state) {
        return false;
    }
    DWORD const dwStyle = WS_OVERLAPPEDWINDOW;
    DWORD const dwStyleEx = WS_EX_APPWINDOW | WS_EX_OVERLAPPEDWINDOW;

    RECT wr = {};
    AdjustWindowRectEx(&wr, dwStyle, FALSE, dwStyleEx);

    u32 const window_width = (wr.right - wr.left) + width;
    u32 const window_height = (wr.bottom - wr.top) + height;

    i32 const window_x  = x + wr.left;
    i32 const window_y  = y; // + wr.top moves the window to far up for me
    
    //wchar_t wname[256];
    //utf8_to_wutf16(name, wname, _countof(wname));
    window_state->hwnd = CreateWindowExW(dwStyleEx, 
        PL_WINDOW_CLASS, ititle, 
        dwStyle, 
        window_x, window_y, 
        window_width, window_height, 
        PNULL, PNULL, 
        state_ptr->handle.instance, 
        PNULL
    );

    ShowWindow(window_state->hwnd, SW_SHOW);
    window->width = width;
    window->height = height;
    window->platform_state = window_state;
    window->renderer_state = NULL;

    vec_push(state_ptr->windows, &window);
    return true;
}

void platform_window_destroy(vwindow *window) {
  if (window) {
    u32 len = vec_length(state_ptr->windows);
    for (u32 i = 0; i < len; ++i) {
      if (state_ptr->windows[i] == window) {
        DestroyWindow(window->platform_state->hwnd);
        //[0][NULL][2][3][4]...
        //   ^--freed window
        // In platform_window_create I call darray_push(state_ptr->windows, out_handle);
        // this could break because old indexes could not be resued, though it breaks from many creates and destroys.
        free(window->platform_state); // once fixed I may remove this, for reuse.
        window->platform_state = PNULL;
        state_ptr->windows[i] = PNULL;
        return;
      }
    }
    VERROR("Destroying a window that was somehow not registered with the platform layer.\n");
    DestroyWindow(window->platform_state->hwnd);
    window->platform_state->hwnd = NULL;
  }
}

void platform_window_present_frame(vwindow* window, u8* pixel_map)
{
    HDC hdc = GetDC(window->platform_state->hwnd);

    // https://gist.github.com/taxilian/1068352
    BITMAPINFO  bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFO);
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biSizeImage = window->width * window->height * sizeof(u32);
    bmi.bmiHeader.biWidth = window->width;
    bmi.bmiHeader.biHeight = -window->height;

    // should I use SetDIBitsToDevice or BitBlt
    int written = SetDIBitsToDevice(hdc, 
        0, 0, 
        window->width, window->height, 
        0, 0, 
        0, 
        window->height, 
        pixel_map, 
        &bmi, 
        DIB_RGB_COLORS
    );
    (void)written;

    // without this the program was lagging so bad
    ReleaseDC(window->platform_state->hwnd, hdc);
}

void platform_pump_messages()
{
    MSG Msg;
    while(PeekMessageW(&Msg, PNULL, 0, 0, PM_REMOVE)){
        TranslateMessage(&Msg);
        DispatchMessageW(&Msg);
    }
}

// void platform_wait_to_pump_messages()
// {
//     MSG Msg;
//     while(GetMessageW(&Msg, PNULL, 0, 0) < 0){
//         TranslateMessage(&Msg);
//         DispatchMessageW(&Msg);
//     }
// }

void platform_set_window_resize_callback(
    platform_window_resize_callback callback) {
  state_ptr->window_resize_callback = callback;
}

void platform_set_window_close_callback(
    platform_window_close_callback callback) {
  state_ptr->window_close_callback = callback;
}

void platform_write_console(CONSOLE_SINK sink, platform_string message){
    platform_string_internal imessage = message;
    DWORD StdHandle = (sink == CONSOLE_SINK_OUT)? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE;
    HANDLE ConsoleOutput = GetStdHandle(StdHandle);
    WriteConsoleW(ConsoleOutput, imessage, wcslen(imessage), NULL, NULL);
}

b8 platform_virtual_reserve(vvirtual_memory* virtual, u64 to_reserve)
{
    virtual->base = VirtualAlloc(NULL, to_reserve, MEM_RESERVE, PAGE_NOACCESS);
    if(virtual->base){
        virtual->reserved = to_reserve;
        virtual->committed = 0;
        return true;
    }
    return false;
}

void* platform_virtual_commit(vvirtual_memory* virtual, u64 to_commit)
{
    void* commit_target = (u8*)virtual->base + virtual->committed;
    
    void* requested = VirtualAlloc(commit_target, to_commit, MEM_COMMIT, PAGE_READWRITE);
    if(requested){
        virtual->committed += to_commit;
    }

    return requested;
}

void platform_virtual_decommit(vvirtual_memory* virtual, u64 to_decommit)
{
    void* decommit_target = (u8*)virtual->base + virtual->committed - to_decommit;
    VirtualFree(decommit_target, to_decommit, MEM_DECOMMIT);
    virtual->committed -= to_decommit;
}

void platform_virtual_unreserve(vvirtual_memory* virtual)
{
    VirtualFree(virtual->base, 0, MEM_RELEASE);
    
    virtual->base = NULL;
    virtual->reserved = 0;
    virtual->committed = 0;
}

void* platform_heap_allocate(u64 to_alloc){
    return HeapAlloc(GetProcessHeap(), 0, to_alloc);
}

void platform_heap_deallocate(void* memory){
    HeapFree(GetProcessHeap(), 0, memory);
}

void* platform_zero_memory(void* memory, u64 memory_size){
    return memset(memory, 0, memory_size); // ZeroMemory
}

void* platform_set_memory(void* memory, i32 value, u64 memory_size){
    return memset(memory, value, memory_size);
}

void* platform_copy_memory(void* dest, const void* src, u64 memory_size){
    return memcpy(dest, src, memory_size);
}

u32 platform_string_to_utf8(platform_string plf_str_len, char* utf8_str, u32 utf8_str_len)
{
    return wutf16_to_utf8(plf_str_len, utf8_str, utf8_str_len);
}

u32 utf8_to_platform_string(char* utf8_str, platform_string plf_str_len, u32 utf16_str_len)
{
    return utf8_to_wutf16(utf8_str, plf_str_len, utf16_str_len);
}

u32 utf8_to_wutf16(char* utf8_str, wchar_t* plf_str_len, u32 utf16_str_len){
    return MultiByteToWideChar(
        CP_UTF8, 
        0, 
        utf8_str, 
        -1, 
        plf_str_len, 
        utf16_str_len
    );
}

u32 wutf16_to_utf8(wchar_t* plf_str_len, char* utf8_str, u32 utf8_str_len){
    return WideCharToMultiByte(
        CP_UTF8, 
        0, 
        plf_str_len, 
        -1, 
        utf8_str, 
        utf8_str_len,
        0,
        PNULL
    );
}

static vwindow *vwindow_from_HWND(HWND handle) {
    if(!state_ptr ||!state_ptr->windows) return 0;
    for (u64 i = 0; i < vec_length(state_ptr->windows); i++) {
        if (!state_ptr->windows[i]) {
            continue;
        }
        vwindow_platform_state * platform_state = (vwindow_platform_state *)state_ptr->windows[i]->platform_state;
         if (platform_state && platform_state->hwnd == handle){
            return state_ptr->windows[i];
        }
    }
    return 0;
}

LRESULT CALLBACK  WindowProcW(
  HWND   hWnd,
  UINT   Msg,
  WPARAM wParam,
  LPARAM lParam
){
    switch(Msg){
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rect;
            GetClientRect(hWnd, &rect);
            FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
        }return 1;

        case WM_CLOSE: {
            vwindow* window = vwindow_from_HWND(hWnd);
            if(window && state_ptr->window_close_callback) {
                state_ptr->window_close_callback(window);
            }
        }return 0;
        case WM_SIZE: {
            vwindow* window = vwindow_from_HWND(hWnd);
            if(window && state_ptr->window_resize_callback) {
                if (window->width != LOWORD(lParam) || window->height  != HIWORD(lParam)){
                    window->width = LOWORD(lParam);
                    window->height = HIWORD(lParam);
                    state_ptr->window_resize_callback(window);
                }
            }
            //InvalidateRect(hWnd, NULL, TRUE);
        }return 0;
    }
    return DefWindowProcW(hWnd, Msg, wParam, lParam);
}

#endif