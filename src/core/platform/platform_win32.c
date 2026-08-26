#include "platform.h"

#if defined(PLATFORM_WINDOWS)
#include "../array/darray.h"
#include <stdlib.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define MY_WINDOW_CLASS L"window_WC"

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
    state_ptr = malloc(sizeof(platform_state));
    state_ptr->handle.instance = (HINSTANCE)GetModuleHandleW(PNULL);

    HBRUSH black_brush = (HBRUSH)GetStockObject(BLACK_BRUSH);
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.hInstance = state_ptr->handle.instance;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = MY_WINDOW_CLASS;
    wc.hbrBackground = black_brush;

    if (!RegisterClassExW(&wc)) {
        DWORD const last_error = GetLastError();

        if (last_error == ERROR_CLASS_ALREADY_EXISTS) {
            return true;
        }

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
        return false;
    }
    state_ptr->windows = darray_create(vwindow, 1);
    state_ptr->window_render_callback = PNULL;
    state_ptr->window_resize_callback = PNULL;
    state_ptr->window_close_callback = PNULL;
    return true;
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
    platform_state->hwnd = CreateWindowExW(dwExStyle, MY_WINDOW_CLASS, wname, dwStyle,
                                    position_x, position_y, window_w, window_h,
                                    PNULL, PNULL, state_ptr->handle.instance, PNULL);
    if (!platform_state->hwnd) {
        free(platform_state);
        return false;
    }
    out_handle->platform_state = platform_state;
    ShowWindow(platform_state->hwnd, SW_SHOW);
    darray_push(state_ptr->windows, out_handle);
    //UpdateWindow(platform_state->hwnd);
    return true;
}

void platform_window_destroy(vwindow* window) {
	if (window) {
		u32 len = darray_length(state_ptr->windows);
		for (u32 i = 0; i < len; ++i) {
			if (state_ptr->windows[i] == window) {
				DestroyWindow(window->platform_state->hwnd);
				window->platform_state->hwnd = 0;
                //[0][NULL][2][3][4]...
                //   ^--freed window
                // In platform_window_create I call darray_push(state_ptr->windows, out_handle);
				state_ptr->windows[i] = 0; // this could break because old indexes could not be resued, though is breaks from many creates and destroys.
				return;
			}
		}
		DestroyWindow(window->platform_state->hwnd);
		window->platform_state->hwnd = 0;
	}
}

typedef struct platform_graphics_context_state {
    vwindow *window;
} platform_graphics_context_state;

static platform_graphics_context_state *g_active_ctx = PNULL;

b8 platform_graphics_context_create(graphics_context_handle *out_context,
                                    vwindow *window) {
    vwindow_platform_state *window_state =
        (vwindow_platform_state *)window->platform_state;

    platform_graphics_context_state *state =
        malloc(sizeof(platform_graphics_context_state));

    if (!state_ptr) {
        return false;
    }
    state->window = window;
    out_context->internal_handle = state;
    return true;
}

void platform_graphics_context_destroy(graphics_context_handle *out_context,
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

void platform_graphics_context_put_image(graphics_context_handle *context,
                                         bitmap bm, u32 x, u32 y) {
    platform_graphics_context_state *graphics_state =
        (platform_graphics_context_state *)context->internal_handle;

    HWND hwnd = graphics_state->window->platform_state->hwnd;
    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);                                        
    HDC hdc;
    b8 release_dc = false;

    // if (graphics_state->window->renderer_state) {
    //     hdc = (HDC)graphics_state->window->renderer_state;
    // } else {
        hdc = GetDC(hwnd);
        release_dc = true;
    // }

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

    int result = SetDIBitsToDevice(hdc, (int)x, (int)y, bm.width, bm.height, 0, 0, 0,
                      bm.height, bm.pixels, &bmi, DIB_RGB_COLORS);
    printf("%i", result);
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
        return 0;
    }

    case WM_PAINT: {
        //PAINTSTRUCT ps;
        //HDC hdc = BeginPaint(hwnd, &ps);

        RECT client_rect;
        GetClientRect(hwnd, &client_rect);
        //FillRect(hdc, &client_rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
        vwindow *handle = vwindow_from_HWND(hwnd, PNULL);
        if (handle && state_ptr->window_render_callback) {
            //handle->renderer_state = (void*)hdc;
            state_ptr->window_render_callback(handle);
            handle->renderer_state = PNULL;
        }
        
        //EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE: {
        u64 index = 0;
        vwindow *handle = vwindow_from_HWND(hwnd, &index);

        if (handle) {
            u64 window_count = darray_length(state_ptr->windows);
            b8 is_last = window_count == 1;

            if (state_ptr->window_close_callback) {
                state_ptr->window_close_callback(handle, is_last);
            }

            darray_pop_at(state_ptr->windows, index, PNULL);
        }

        CloseWindow(hwnd);
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
#endif
