
#if defined(PLATFORM_WINDOWS)
#include "platform.h"
#include "../array/darray.h"
#include <stdlib.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define PL_WINDOW_CLASS L"window_WC"

typedef struct win32_handle_info {
    HINSTANCE instance;
}win32_handle_info;

typedef struct vwindow_platform_state {
    HWND hwnd;
} vwindow_platform_state;

typedef struct platform_state {
    win32_handle_info handle;
    vwindow **windows;
    platform_window_resize_callback window_resize_callback;
    platform_window_render_callback window_render_callback;
    platform_window_close_callback window_close_callback;
} platform_state;

static platform_state *state_ptr;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

b8 platform_initalize() {
    if (state_ptr) return false; // already initalized

    state_ptr = malloc(sizeof(platform_state));
    if(!state_ptr) {
        return false;
    }
    state_ptr->handle.instance = (HINSTANCE)GetModuleHandleW(PNULL);
    
    HBRUSH black_brush = (HBRUSH)GetStockObject(BLACK_BRUSH);
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.hInstance = state_ptr->handle.instance;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = PL_WINDOW_CLASS;
    wc.hbrBackground = black_brush;

    if (!RegisterClassExW(&wc)) {
        DWORD const last_error = GetLastError();
        LPWSTR wmessage_buf = PNULL;
        DWORD const size = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            PNULL, last_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&wmessage_buf, 0, PNULL);

        if (size) {
            MessageBoxW(PNULL, wmessage_buf, L"Window registration failed",
                        MB_ICONERROR | MB_OK);
            LocalFree(wmessage_buf);
        }
        free(state_ptr);
        return false;
    }

    state_ptr->windows = darray_create(vwindow*, 1);
    state_ptr->window_render_callback = PNULL;
    state_ptr->window_resize_callback = PNULL;
    state_ptr->window_close_callback = PNULL;
    return true;
}

void platform_uninitalize(void)
{
    if (state_ptr) {
        u32 len = darray_length(state_ptr->windows);
        for (u32 i = 0; i < len; ++i) {
            if (state_ptr->windows[i] != PNULL) {
                DestroyWindow(state_ptr->windows[i]->platform_state->hwnd);
                state_ptr->windows[i]->platform_state->hwnd = PNULL;
                free(state_ptr->windows[i]->platform_state);
                state_ptr->windows[i]->platform_state = PNULL;
            }
        }
        UnregisterClassW(PL_WINDOW_CLASS, state_ptr->handle.instance);
        darray_destroy(state_ptr->windows);
        state_ptr->windows = PNULL;
        state_ptr->window_render_callback = PNULL;
        state_ptr->window_resize_callback = PNULL;
        state_ptr->window_close_callback = PNULL;
        free(state_ptr);
        state_ptr = PNULL;
    }
}

b8 platform_window_create(vwindow *out_handle, char const *name,
                          u32 const w, u32 const h, u32 const x, u32 const y) {

    vwindow_platform_state *platform_state = malloc(sizeof(vwindow_platform_state));
    if (!platform_state) {
        return false;
    }

    DWORD const dwExStyle = WS_EX_APPWINDOW | WS_EX_OVERLAPPEDWINDOW;
    DWORD const dwStyle = WS_OVERLAPPEDWINDOW;

    RECT wr = {0, 0, (LONG)w, (LONG)h};
    AdjustWindowRectEx(&wr, dwStyle, FALSE, dwExStyle);

    u32 const window_w = (wr.right - wr.left);
    u32 const window_h = (wr.bottom - wr.top);

    u32 const position_y = y + wr.top;
    u32 const position_x = x + wr.left;

    WCHAR wname[256];
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, 256);
    platform_state->hwnd = CreateWindowExW(dwExStyle, PL_WINDOW_CLASS, wname, dwStyle,
                                    position_x, position_y, window_w, window_h,
                                    PNULL, PNULL, state_ptr->handle.instance, PNULL);
    if (!platform_state->hwnd) {
        free(platform_state);
        return false;
    }
    out_handle->platform_state = platform_state;
    out_handle->width = w;
    out_handle->height = h;
    
    ShowWindow(platform_state->hwnd, SW_SHOW);
    darray_push(state_ptr->windows, out_handle);
    //UpdateWindow(platform_state->hwnd); may call BeginPaint, which may break
    return true;
}

void platform_window_destroy(vwindow* window) {
	if (window) {
		u32 const len = darray_length(state_ptr->windows);
		for (u32 i = 0; i < len; ++i) {
			if (state_ptr->windows[i] == window && window->platform_state->hwnd != PNULL) {
				DestroyWindow(window->platform_state->hwnd);
				window->platform_state->hwnd = 0;
                //[0][NULL][2][3][4]...
                //   ^--freed window
                // In platform_window_create I call darray_push(state_ptr->windows, out_handle);
				state_ptr->windows[i] = 0; // this could break because old indexes could not be resued, though it breaks from many creates and destroys.
				return;
			}
		}
	}
}

typedef struct vwindow_context_state {
    vwindow *window;
} vwindow_context_state;

static vwindow_context_state *g_active_ctx = PNULL;

b8 platform_graphics_context_create(vwindow_context *out_context,
                                    vwindow *window) {
    if (!state_ptr || !window) {
        return false;
    }
    vwindow_platform_state *window_state =
        (vwindow_platform_state *)window->platform_state;

    vwindow_context_state *state = malloc(sizeof(vwindow_context_state));
    state->window = window;
    out_context->platform_state = state;
    return true;
}

void platform_graphics_context_destroy(vwindow_context *out_context,
                                    vwindow *window) {
    free(out_context);
}

static vwindow *vwindow_from_HWND(HWND handle, u64 *out_index) {
    for (u64 i = 0; i < darray_length(state_ptr->windows); i++) {
        vwindow_platform_state * platform_state =
            (vwindow_platform_state *)state_ptr->windows[i]->platform_state;
        if (platform_state && platform_state->hwnd == handle) {
            if (out_index) {
                *out_index = i;
            }
            return state_ptr->windows[i];
        }
    }
    return 0;
}

void platform_graphics_context_put_image(vwindow_context *context,
                                         bitmap bm, u32 x, u32 y) {
    vwindow_context_state *graphics_state =
        (vwindow_context_state *)context->platform_state;

    HWND hwnd = graphics_state->window->platform_state->hwnd;
    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);                                        
    HDC hdc;
    b8 release_dc = false;
    
    // kind of just a work around becasue HDC has a short life time and for some reason 
    // I can't render proprly when calling GetDC in the render callback.
    // so its a mix of GetDC and Begin Paint which is weird.
    if (graphics_state->window->renderer_state) {
         hdc = (HDC)graphics_state->window->renderer_state;
    } else {
        hdc = GetDC(hwnd);
        release_dc = true;
    }

    if (!hdc) {
        return;
    }

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = (LONG)bm.width;
    bmi.bmiHeader.biHeight = -(LONG)bm.height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetDIBitsToDevice(hdc, (int)x, (int)y, bm.width, bm.height, 0, 0, 0,
                      bm.height, bm.pixels, &bmi, DIB_RGB_COLORS);
    if (release_dc) {
        ReleaseDC(hwnd, hdc);
    }

    EndPaint(hwnd, &ps);
}

b8 platform_pump_message() {
    MSG Msg;

    while (PeekMessageW(&Msg, PNULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&Msg);
        DispatchMessageW(&Msg);
    }

    return 1;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
    switch (uMsg) {
        case WM_SIZE: {
            vwindow *handle = vwindow_from_HWND(hwnd, PNULL);
            if (handle) {
                handle->width = LOWORD(lParam);
                handle->height = HIWORD(lParam);
            
                if (state_ptr->window_resize_callback) {
                    state_ptr->window_resize_callback(handle, handle->width,
                                                      handle->height);
                }
            }
            InvalidateRect(hwnd, PNULL, FALSE);
        } return 0;
    
        // may remove this completly
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
        
            RECT client_rect;
            GetClientRect(hwnd, &client_rect);
            FillRect(hdc, &client_rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
            vwindow *handle = vwindow_from_HWND(hwnd, PNULL);
            if (handle && state_ptr->window_render_callback) {
                handle->renderer_state = (void*)hdc;
                state_ptr->window_render_callback(handle);
                handle->renderer_state = PNULL;
            }
            
            EndPaint(hwnd, &ps);
        } return 0;

        case WM_CLOSE: {
            u64 index = 0;
            vwindow *handle = vwindow_from_HWND(hwnd, &index);
            if (handle) {
                if (state_ptr->window_close_callback) {
                    state_ptr->window_close_callback(handle);
                }
            }
        } break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void platform_window_set_resize_callback(
    platform_window_resize_callback callback) {
    state_ptr->window_resize_callback = callback;
}

void platform_window_set_render_callback(
    platform_window_render_callback callback) {
    state_ptr->window_render_callback = callback;
}

void platform_window_set_close_callback(
    platform_window_close_callback callback) {
    state_ptr->window_close_callback = callback;
}
// converts Windows Wide UTF16 to UTF8
u64 win32_wutf16_to_utf8(
    const wchar_t* wutf16_str,
    char* utf8_str,
    u64 utf8_str_size
)
{
    return (u64)WideCharToMultiByte(
        CP_UTF8,
        0,
        wutf16_str,
        -1,
        utf8_str,
        (int)utf8_str_size,
        NULL,
        NULL
    );
}
#endif