#pragma once
#include "../defines.h"

typedef struct platform_state platform_state;
typedef struct vwindow_platform_state vwindow_platform_state;
typedef struct vwindow_renderer_state vwindow_renderer_state;

b8 platform_initialize(u64 *memory_requirement, void *memory,
                       struct platform_state *state);

void platform_uninitialize(void);

typedef struct command_line_args {
    int argc;
    char **argv;
} command_line_args;

#ifdef PLATFORM_NO_ENTRY_POINT
#warning "main entry point is not defined in platform_<name>.c"
#warning "command_line_args platform_get_command_line_args(void) is not defined"
#else
// main entry point is defined in platform_<name>.c
// platform_get_command_line_args is defined
#define PLATFORM_ENTRY_POINT_DEFINED
#endif

command_line_args platform_get_command_line_args(void);

typedef struct vwindow {
    vwindow_platform_state *platform_state;
    vwindow_renderer_state *renderer_state;

    i32 width;
    i32 height;
} vwindow;

typedef struct vwindow_config {
    char *title;
    i32 width;
    i32 height;
    i32 xposition;
    i32 yposition;
} vwindow_config;

typedef enum CONSOLE_SINK { CONSOLE_SINK_OUT, CONSOLE_SINK_ERR } CONSOLE_SINK;

b8 platform_window_create(const vwindow_config *config, vwindow *window);
void platform_window_destroy(vwindow *window);

void platform_window_present_frame(vwindow *window, u8 *pixel_map);

void platform_process_events(void);

typedef void (*platform_window_resize_callback)(vwindow *window);
typedef void (*platform_window_close_callback)(vwindow *window);

void platform_set_window_resize_callback(
    platform_window_resize_callback callback);

void platform_set_window_close_callback(
    platform_window_close_callback callback);

void platform_write_console(CONSOLE_SINK sink, const char *message);

void *platform_heap_allocate(u64 to_alloc);
void platform_heap_deallocate(void *memory);

typedef struct vvirtual_memory {
    u64 committed;
    u64 reserved;
    void *base;
} vvirtual_memory;

b8 platform_virtual_reserve(vvirtual_memory *virtual, u64 to_reserve);
void *platform_virtual_commit(vvirtual_memory *virtual, u64 to_commit);
// void platform_virtual_decommit(vvirtual_memory* virtual, u64 amount);
void platform_virtual_unreserve(vvirtual_memory *virtual);

void *platform_zero_memory(void *memory, u64 memory_size);
void *platform_set_memory(void *memory, i32 value, u64 memory_size);
void *platform_copy_memory(void *dest, const void *src, u64 memory_size);
