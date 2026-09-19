#pragma once
#include "../defines.h"
typedef struct platform_state platform_state;
typedef struct vwindow_platform_state vwindow_platform_state;
typedef struct vwindow_renderer_state vwindow_renderer_state;

// abstract the UTF conversion away for all platforms
// UTF8 to Wide/Window's UTF16
// Does nothing but returns the lenght if already UTF8
typedef void *platform_string;
u32 platform_string_to_utf8(platform_string plf_str, char* utf8_str, u32 utf8_str_len);
u32 utf8_to_platform_string(char* utf8_str, platform_string plf_str, u32 plf_str_len);

// #if defined(PLATFORM_WINDOWS)
// u32 wutf16_to_utf8(wchar_t* wutf16_str, char* utf8_str, u32 utf8_str_len);
// u32 utf8_to_wutf16(char* utf8_str, wchar_t* wutf16_str, u32 utf16_str_len);
// typedef wchar_t platform_char;
// typedef wchar_t* platform_string;
// #else
// // No Proper definition 
// #define utf8_to_wutf16(utf8_str, wutf16_str, utf16_str_len) utf16_str_len
// #define wutf16_to_utf8(wutf16_str, utf8_str, utf8_str_len) utf8_str_len
// typedef char platform_char;
// typedef char* platform_string;
// #endif

b8 platform_initialize();
void platform_uninitalize();

typedef struct vwindow {
    vwindow_platform_state* platform_state;
    vwindow_renderer_state* renderer_state;

    u32 width;
    u32 height;
}vwindow;

typedef enum CONSOLE_SINK {
    CONSOLE_SINK_OUT,
    CONSOLE_SINK_ERR
}CONSOLE_SINK;

b8 platform_window_create(platform_string title, u32 width, u32 height, u32 x, u32 y, vwindow* window);
void platform_window_destroy(vwindow* window);

// BGRA
// stride = sizeof(u8)*4
// API native renderering.
// pretty much the only function needed for CPU rendering
void platform_window_present_frame(vwindow* window, u8* pixel_map, u32 width, u32 height, u32 x, u32 y);

void platform_pump_messages();

typedef void (*platform_window_resize_callback)(vwindow *const window);
typedef void (*platform_window_close_callback)(vwindow *const window);

void platform_set_window_resize_callback(platform_window_resize_callback callback);
void platform_set_window_close_callback(platform_window_close_callback callback);

void platform_write_console(CONSOLE_SINK sink, platform_string message);


void* platform_heap_allocate(u64 to_alloc);
void platform_heap_deallocate(void* memory);

typedef struct vvirtual_memory {
    u64 committed;
    u64 reserved;
    void* base;
}vvirtual_memory;

b8 platform_virtual_reserve(vvirtual_memory* virtual, u64 to_reserve);
void* platform_virtual_commit(vvirtual_memory* virtual, u64 to_commit);
//void platform_virtual_decommit(vvirtual_memory* virtual, u64 amount);
void platform_virtual_unreserve(vvirtual_memory* virtual);

void* platform_zero_memory(void* memory, u64 memory_size);
void* platform_set_memory(void* memory, i32 value, u64 memory_size);
void* platform_copy_memory(void* dest, void* src, u64 memory_size);