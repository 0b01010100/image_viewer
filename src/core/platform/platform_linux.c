// CODE FOR LINUX
#include "platform.h"

#ifdef PLATFORM_LINUX
#include <stdio.h>

#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "../containers/vec.h"
#include "../logger.h"
#include "../memory/memory.h"

// may rename to application info or something
// should have information that is know during startup time
typedef struct platform_command_line_args {
    int argc;
    char **argv;
} platform_command_line_args;

platform_command_line_args application_args;
platform_state *state_ptr;
vwindow *vwindow_from_xcb_window_t(xcb_window_t handle, u64 *out_index);

typedef struct linux_handle_info {
    xcb_connection_t *connection;
    xcb_screen_t *screen;
} linux_handle_info;

struct platform_state {
    linux_handle_info handle;
    xcb_atom_t window_close_atom;
    xcb_atom_t wm_protocols_atom;
    vwindow **windows; // vec
    platform_window_resize_callback window_resize_callback;
    platform_window_close_callback window_close_callback;
    command_line_args args;
};

struct vwindow_platform_state {
    xcb_window_t window;
    xcb_gcontext_t gcontext;
    xcb_connection_t *connection;
};

b8 platform_initialize(u64 *memory_requirement, void *memory,
                       struct platform_state *state) {
    if (state_ptr) {
        return false;
    }

    if (!state) {
        *memory_requirement = sizeof(platform_state);
        return true;
    }

    if (!memory) {
        VERROR("memory can't be null");
        return false;
    }

    state_ptr = memory;
    int screen_num;
    xcb_connection_t *connection = xcb_connect(nullptr, &screen_num);

    int const connection_error = xcb_connection_has_error(connection);

    if (connection_error > 0) {
        VERROR("Failed to start connection, error code: %i", connection_error);
        return false;
    }

    const xcb_setup_t *setup = xcb_get_setup(connection);

    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);

    for (int i = 0; i < screen_num; ++i) {
        xcb_screen_next(&iter);
    }

    state_ptr->handle.connection = connection;
    state_ptr->handle.screen = iter.data;
    state_ptr->window_resize_callback = nullptr;
    state_ptr->window_close_callback = nullptr;
    state_ptr->windows = vec_create(vwindow *, 1);
    state_ptr->args.argv = application_args.argv;
    state_ptr->args.argc = application_args.argc;
    state_ptr->window_close_atom = XCB_ATOM_NONE;
    state_ptr->wm_protocols_atom = XCB_ATOM_NONE;
    return true;
}

void platform_uninitialize(void) {
    if (!state_ptr) {
        VFATAL("Double free corruption");
        return;
    }

    u32 const len = vec_length(state_ptr->windows);
    for (u32 i = 0; i < len; ++i) {
        if (state_ptr->windows[i] != nullptr &&
            state_ptr->windows[i]->platform_state != nullptr) {
            xcb_destroy_window(state_ptr->handle.connection,
                               state_ptr->windows[i]->platform_state->window);
            vdeallocate(state_ptr->windows[i]->platform_state);
            state_ptr->windows[i]->platform_state = nullptr;
            state_ptr->windows[i] = nullptr;
            return;
        }
    }
    xcb_disconnect(state_ptr->handle.connection);
    vec_destroy(state_ptr->windows);

    state_ptr->windows = nullptr;
    state_ptr->handle.connection = XCB_NONE;
    state_ptr->handle.screen = nullptr;
    state_ptr->window_resize_callback = nullptr;
    state_ptr->window_close_callback = nullptr;
    state_ptr->args.argv = nullptr;
    state_ptr->args.argc = 0;
    state_ptr->window_close_atom = XCB_ATOM_NONE;
    state_ptr->wm_protocols_atom = XCB_ATOM_NONE;
}

command_line_args platform_get_command_line_args(void) {
    return state_ptr->args;
}

b8 platform_window_create(const vwindow_config *config, vwindow *window) {
    (void)window->width;
    (void)window->height;

    vwindow_platform_state *window_state =
        vallocate(sizeof(vwindow_platform_state));
    if (!window_state) {
        return false;
    }

    u32 const value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    u32 const value_list[] = {
        state_ptr->handle.screen->black_pixel,
        XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY,
    };

    window_state->window = xcb_generate_id(state_ptr->handle.connection);

    xcb_void_cookie_t window_cookie = xcb_create_window_checked(
        state_ptr->handle.connection, XCB_COPY_FROM_PARENT,
        window_state->window, state_ptr->handle.screen->root,
        (i16)config->xposition, (i16)config->yposition, config->width,
        config->height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
        state_ptr->handle.screen->root_visual, value_mask, value_list);

    xcb_generic_error_t *window_error =
        xcb_request_check(state_ptr->handle.connection, window_cookie);

    if (window_error) {
        VERROR("xcb_create_window failed: error_code=%u\n",
               window_error->error_code);
        free(window_error);
        vdeallocate(window_state);
        return false;
    }
    char *title = config->title;
    if (!title) {
        title = "xcb_window";
    }

    u32 const name_len = strlen(title);
    xcb_change_property(state_ptr->handle.connection, XCB_PROP_MODE_REPLACE,
                        window_state->window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING,
                        8, name_len, title);

    xcb_map_window(state_ptr->handle.connection, window_state->window);
    xcb_void_cookie_t map_cookie = xcb_map_window_checked(
        state_ptr->handle.connection, window_state->window);

    xcb_generic_error_t *error =
        xcb_request_check(state_ptr->handle.connection, map_cookie);

    if (error) {
        VERROR("xcb_map_window failed: error_code=%u\n", error->error_code);
        free(error);
        xcb_destroy_window(state_ptr->handle.connection, window_state->window);
        vdeallocate(window_state);
    }

    if (state_ptr->window_close_atom == XCB_ATOM_NONE) {
        xcb_intern_atom_cookie_t c_protocols = xcb_intern_atom(
            state_ptr->handle.connection, 1, 12, "WM_PROTOCOLS");

        xcb_intern_atom_cookie_t c_delete = xcb_intern_atom(
            state_ptr->handle.connection, 0, 16, "WM_DELETE_WINDOW");

        xcb_intern_atom_reply_t *reply1 = xcb_intern_atom_reply(
            state_ptr->handle.connection, c_protocols, nullptr);

        xcb_intern_atom_reply_t *reply2 = xcb_intern_atom_reply(
            state_ptr->handle.connection, c_delete, nullptr);

        if (reply1 && reply2) {
            xcb_change_property(state_ptr->handle.connection,
                                XCB_PROP_MODE_REPLACE, window_state->window,
                                reply1->atom, XCB_ATOM_ATOM, 32, 1,
                                &reply2->atom);

            state_ptr->window_close_atom = reply2->atom;
        }
        free(reply1);
        free(reply2);
    }

    window_state->gcontext = xcb_generate_id(state_ptr->handle.connection);
    u32 const gcontext_value_mask =
        0; // XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
    uint32_t gcontext_values[2];
    gcontext_values[0] = state_ptr->handle.screen->black_pixel;
    gcontext_values[1] = 0;
    xcb_change_gc_value_list_t gcontext_value_list = {};

    xcb_void_cookie_t gcontext_cookie = xcb_create_gc(
        state_ptr->handle.connection, window_state->gcontext,
        window_state->window, gcontext_value_mask, &gcontext_value_list);
    xcb_generic_error_t *gcontext_error =
        xcb_request_check(state_ptr->handle.connection, gcontext_cookie);

    if (gcontext_error) {
        VERROR("xcb_create_window failed: error_code=%u\n",
               gcontext_error->error_code);
        free(gcontext_error);
        xcb_destroy_window(state_ptr->handle.connection, window_state->window);
        vdeallocate(window_state);
        return false;
    }
    xcb_flush(state_ptr->handle.connection);

    window->width = config->width;
    window->height = config->height;
    window_state->connection = state_ptr->handle.connection;
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
            xcb_destroy_window(state_ptr->handle.connection,
                               window->platform_state->window);
            vdeallocate(window->platform_state);
            window->platform_state = nullptr;
            state_ptr->windows[i] = nullptr;
            return;
        }
    }
    VERROR("Destroying a window that was somehow not registered with the "
           "platform layer.\n");
    xcb_destroy_window(state_ptr->handle.connection,
                       window->platform_state->window);
    window->platform_state->window = XCB_NONE;
}

void platform_window_present_frame(vwindow *window, u8 *pixel_map) {
    vwindow_platform_state *window_state = window->platform_state;
    xcb_put_image(state_ptr->handle.connection, XCB_IMAGE_FORMAT_Z_PIXMAP,
                  window_state->window, window_state->gcontext, window->width,
                  window->height, 0, 0, 0, state_ptr->handle.screen->root_depth,
                  window->width * window->height * (i32)sizeof(u32), pixel_map);

    xcb_flush(state_ptr->handle.connection);
}
#define XCB_RESPONE_TYPE_MASK(response_type) ((response_type) & ~0x80)

void platform_process_events(void) {
    xcb_generic_event_t *generic_event;
    while ((generic_event = xcb_poll_for_event(state_ptr->handle.connection))) {
        switch (XCB_RESPONE_TYPE_MASK(generic_event->response_type)) {
        case XCB_EXPOSE: {

        } break;
        // https://stackoverflow.com/questions/79919816/how-do-i-correctly-handle-xcb-resize-request-events
        case XCB_CONFIGURE_NOTIFY: {
            xcb_configure_notify_event_t *event =
                (xcb_configure_notify_event_t *)generic_event;
            vwindow *window = vwindow_from_xcb_window_t(event->window, nullptr);
            u16 width = event->width;
            u16 height = event->height;

            if (width != window->width || height != window->height) {
                window->width = width;
                window->height = height;
                if (state_ptr->window_resize_callback) {
                    state_ptr->window_resize_callback(window);
                }
            }
        } break;

        case XCB_CLIENT_MESSAGE: {
            xcb_client_message_event_t *event =
                (xcb_client_message_event_t *)generic_event;
            vwindow *window = vwindow_from_xcb_window_t(event->window, nullptr);
            if (event->data.data32[0] == state_ptr->window_close_atom) {
                if (state_ptr->window_close_callback) {
                    state_ptr->window_close_callback(window);
                }
            }
        } break;
        }
        free(generic_event);
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
    FILE *stream = (sink == CONSOLE_SINK_OUT) ? stdout : stderr;
    fprintf(stream, "%s", message);
}
// Cache the page size to eliminate sysconf overhead
u64 platform_page_size(void) {
    static u64 page_size = 0;
    if (page_size == 0) {
        long sz = sysconf(_SC_PAGESIZE);
        page_size = (sz > 0) ? (u64)sz : 4096;
    }
    return page_size;
}

u64 platform_page_align(u64 size) {
    u64 page_size = platform_page_size();
    return (size + page_size - 1) & ~(page_size - 1);
}

b8 platform_virtual_reserve(vvirtual_memory *virtual, u64 to_reserve) {
    if (!virtual || to_reserve == 0) {
        return false;
    }
    u64 aligned_reserve = platform_page_align(to_reserve);

    void *map = mmap(nullptr, aligned_reserve, PROT_NONE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map == MAP_FAILED) {
        return false;
    }

    virtual->base = map;
    virtual->committed = 0;
    virtual->reserved = aligned_reserve;
    return true;
}

void *platform_virtual_commit(vvirtual_memory *virtual, u64 to_commit) {
    if (!virtual || !virtual->base || to_commit == 0) {
        return nullptr;
    }

    u64 aligned_commit = platform_page_align(to_commit);

    // Bounds check using the ALIGNED commit size
    if (virtual->committed + aligned_commit > virtual->reserved) {
        return nullptr;
    }
    uint8_t *commit_addr = (uint8_t *)virtual->base + virtual->committed;

    if (mprotect(commit_addr, aligned_commit, PROT_READ | PROT_WRITE) != 0) {
        return nullptr;
    }

    virtual->committed += aligned_commit;
    return commit_addr;
}

void platform_virtual_decommit(vvirtual_memory *virtual, u64 amount) {
    if (!virtual || !virtual->base || amount == 0 ||
        amount > virtual->committed) {
        return;
    }

    // Align decommit amount up to whole page boundaries
    u64 aligned_amount = platform_page_align(amount);
    if (aligned_amount > virtual->committed) {
        aligned_amount = virtual->committed;
    }

    u64 new_committed = virtual->committed - aligned_amount;
    uint8_t *decommit_addr = (uint8_t *)virtual->base + new_committed;

    // Revoke permissions on full page boundaries
    mprotect(decommit_addr, aligned_amount, PROT_NONE);

    // Return physical pages back to OS
#ifdef MADV_DONTNEED
    madvise(decommit_addr, aligned_amount, MADV_DONTNEED);
#endif

    virtual->committed = new_committed;
}

void platform_virtual_unreserve(vvirtual_memory *virtual) {
    if (!virtual || !virtual->base) {
        return;
    }
    munmap(virtual->base, virtual->reserved);

    virtual->base = nullptr;
    virtual->reserved = 0;
    virtual->committed = 0;
}

void *platform_heap_allocate(u64 to_alloc) {
    return malloc(to_alloc);
}

void platform_heap_deallocate(void *memory) {
    free(memory);
}

void *platform_zero_memory(void *memory, u64 memory_size) {
    return memset(memory, 0, memory_size);
}

void *platform_set_memory(void *memory, i32 value, u64 memory_size) {
    return memset(memory, value, memory_size);
}

void *platform_copy_memory(void *dest, const void *src, u64 memory_size) {
    return memcpy(dest, src, memory_size);
}

vwindow *vwindow_from_xcb_window_t(xcb_window_t handle, u64 *out_index) {
    for (u64 i = 0; i < vec_length(state_ptr->windows); i++) {
        if (!state_ptr->windows[i]) {
            continue;
        }
        if (state_ptr->windows[i]->platform_state->window == handle) {
            if (out_index) {
                *out_index = i;
            }
            return state_ptr->windows[i];
        }
    }
    return 0;
}

#ifdef PLATFORM_ENTRY_POINT_DEFINED
// may use a macro for this to allow name change via compiler flag
extern int application_main(void);

// int platform_main(int argc, char **argv)
int main(int argc, char **argv) {
    application_args.argc = argc;
    application_args.argv = argv;
    return application_main();
}
#endif
#endif
