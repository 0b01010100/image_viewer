#include "platform.h"

#if defined(PLATFORM_WINDOWS)
#include "../array/darray.h"
#include <stdio.h>
#include <stdlib.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define MY_WINDOW_CLASS L"window_WC"

typedef struct platform_window_state {
    HWND handle;
} platform_window_state;

typedef struct platform_state {
    HINSTANCE app_instance;
    vwindow **windows;
    platform_window_resize_callback window_resize_callback;
    platform_window_render_callback window_render_callback;
    platform_window_close_callback window_close_callback;
} platform_state;

static platform_state *state_ptr;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

b8 platform_initalize() {

    state_ptr = malloc(sizeof(platform_state));
    state_ptr->app_instance = (HINSTANCE)GetModuleHandleW(PNULL);
    WNDCLASSEXW const wc = (WNDCLASSEXW const){
        .cbSize = sizeof(WNDCLASSEXW),
        .hInstance = state_ptr->app_instance,
        .lpfnWndProc = WindowProc,
        .lpszClassName = MY_WINDOW_CLASS,
        .hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH)};

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
    state_ptr->windows = darray_create(vwindow, 2);
    state_ptr->window_render_callback = PNULL;
    state_ptr->window_resize_callback = PNULL;
    state_ptr->window_close_callback = PNULL;
    return true;
}

b8 platform_window_create(vwindow *out_handle, char const *name,
                          u32 const w, u32 const h, u32 const x, u32 const y) {

    platform_window_state *state = malloc(sizeof(platform_window_state));

    if (!state) {
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
    state->handle = CreateWindowExW(dwExStyle, MY_WINDOW_CLASS, wname, dwStyle,
                                    position_x, position_y, window_w, window_h,
                                    PNULL, PNULL, state_ptr->app_instance, PNULL);
    if (!state->handle) {
        free(state);
        return false;
    }
    out_handle->internal_handle = state;
    ShowWindow(state->handle, SW_SHOW);
    UpdateWindow(state->handle);
    darray_push(state_ptr->windows, out_handle);
    return true;
}

typedef struct platform_graphics_context_state {
    vwindow *window;
} platform_graphics_context_state;

static platform_graphics_context_state *g_active_ctx = PNULL;

b8 platform_graphics_context_create(graphics_context_handle *out_context,
                                    vwindow *window) {
    platform_window_state *window_state =
        (platform_window_state *)window->internal_handle;

    platform_graphics_context_state *state =
        malloc(sizeof(platform_graphics_context_state));

    if (!state_ptr) {
        return false;
    }
    state->window = window;
    out_context->internal_handle = state;
    return true;
}

static vwindow *vwindow_from_HWND(HWND handle, u64 *out_index) {
    for (u64 i = 0; i < darray_length(state_ptr->windows); i++) {
        platform_window_state *win_state =
            (platform_window_state *)state_ptr->windows[i]->internal_handle;
        if (win_state && win_state->handle == handle) {
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

    HWND hwnd = graphics_state->window->internal_handle->handle;

    HDC hdc;
    b8 release_dc = false;

    if (graphics_state->window->render_state) {
        hdc = (HDC)graphics_state->window->render_state;
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
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT client_rect;
        GetClientRect(hwnd, &client_rect);

        FillRect(hdc, &client_rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
        vwindow *handle = vwindow_from_HWND(hwnd, PNULL);
        if (handle && state_ptr->window_render_callback) {
            handle->render_state = hdc;
            state_ptr->window_render_callback(handle);
        }

        EndPaint(hwnd, &ps);
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
