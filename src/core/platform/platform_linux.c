// CODE FOR LINUX
#include "platform.h"

#if defined(PLATFORM_LINUX)
#include <stdio.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "../containers/vec.h"
#include "../logger.h"

static platform_state* state_ptr;
typedef char* platform_string_internal;
static vwindow *vwindow_from_xcb_window_t(xcb_window_t handle, u64 *out_index);

typedef struct linux_handle_info {
  xcb_connection_t *connection;
  xcb_screen_t *screen;
} linux_handle_info; 

struct platform_state {
    linux_handle_info handle;
    xcb_atom_t window_close_atom;
    vwindow** windows; // vec
    platform_window_resize_callback window_resize_callback;
    platform_window_close_callback window_close_callback;
};

struct vwindow_platform_state {
    xcb_window_t window;
    xcb_gcontext_t gcontext;
    xcb_connection_t* connection;
};

b8 platform_initialize()
{
    state_ptr = malloc(sizeof(platform_state));
    if (!state_ptr) {
        return false;
    }

    int screen_num;
    xcb_connection_t *connection =
        xcb_connect(PNULL, &screen_num);

    int connection_error = xcb_connection_has_error(connection);

    if (connection_error > 0) {
        DEALLOC(state_ptr);

        VERROR(
            "Failed to start connection, error code: %i",
            connection_error
        );

        return false;
    }

    const xcb_setup_t *setup =
        xcb_get_setup(connection);

    xcb_screen_iterator_t iter =
        xcb_setup_roots_iterator(setup);

    for (int i = 0; i < screen_num; ++i) {
        xcb_screen_next(&iter);
    }

    state_ptr->handle.connection = connection;
    state_ptr->handle.screen = iter.data;
    state_ptr->window_resize_callback = PNULL;
    state_ptr->window_close_callback = PNULL;
    state_ptr->windows = vec_create(vwindow *, 1);

    return true;
}

void platform_uninitalize(){
    if(!state_ptr){
        VFATAL("Double free corruption");
        return;
    }

    u32 len = vec_length(state_ptr->windows);
    for (u32 i = 0; i < len; ++i) {
      if (state_ptr->windows[i] != PNULL) {
        xcb_destroy_window(state_ptr->handle.connection,
                           state_ptr->windows[i]->platform_state->window);
        free(state_ptr->windows[i]->platform_state);
        state_ptr->windows[i]->platform_state = PNULL;
        state_ptr->windows[i] = PNULL;
        return;
      }
    }

    xcb_disconnect(state_ptr->handle.connection);
    vec_destroy(state_ptr->windows);
    free(state_ptr);
}

b8 platform_window_create(platform_string title, u32 width, u32 height, u32 x, u32 y, vwindow* window){
    platform_string_internal ititle = title;

    vwindow_platform_state* window_state = ALLOC(sizeof(vwindow_platform_state));
    if(!window_state){
        return  false;
    }

    (void)window->width;
    (void)window->height;
    
    u32 const value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    u32 const value_list[] = {state_ptr->handle.screen->black_pixel,
                            XCB_EVENT_MASK_EXPOSURE |
                                XCB_EVENT_MASK_STRUCTURE_NOTIFY};

    window_state->window = xcb_generate_id(state_ptr->handle.connection);
    xcb_void_cookie_t window_cookie = xcb_create_window_checked(
        state_ptr->handle.connection,
        XCB_COPY_FROM_PARENT,
        window_state->window,
        state_ptr->handle.screen->root,
        x, y, width, height,
        0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        state_ptr->handle.screen->root_visual,
        value_mask,
        value_list
    );

    xcb_generic_error_t *window_error =
    xcb_request_check(state_ptr->handle.connection, window_cookie);

    if (window_error) {
        printf("xcb_create_window failed: error_code=%u\n",
           window_error->error_code);
        free(window_error);
        return false;
    }

    if(!ititle){
        ititle = "xcb_window";
    }
    
    u32 const name_len = strlen(ititle);
    xcb_change_property(state_ptr->handle.connection, XCB_PROP_MODE_REPLACE, window_state->window,
                      XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, name_len, ititle);
    
    xcb_map_window(state_ptr->handle.connection, window_state->window);
    xcb_void_cookie_t map_cookie = xcb_map_window_checked(state_ptr->handle.connection, window_state->window);

    xcb_generic_error_t *error =
        xcb_request_check(state_ptr->handle.connection, map_cookie);

    if (error) {
        printf("xcb_map_window failed: error_code=%u\n",
           error->error_code);
        free(error);
    }

    xcb_flush(state_ptr->handle.connection);

    if (state_ptr->window_close_atom == XCB_ATOM_NONE)
    {
        xcb_intern_atom_cookie_t c_protocols =
            xcb_intern_atom(
                state_ptr->handle.connection,
                1,
                12,
                "WM_PROTOCOLS");

        xcb_intern_atom_cookie_t c_delete =
            xcb_intern_atom(
                state_ptr->handle.connection,
                0,
                16,
                "WM_DELETE_WINDOW");

        xcb_intern_atom_reply_t *reply1 =
            xcb_intern_atom_reply(
                state_ptr->handle.connection,
                c_protocols,
                NULL);

        xcb_intern_atom_reply_t *reply2 =
            xcb_intern_atom_reply(
                state_ptr->handle.connection,
                c_delete,
                NULL);

        if (reply1 && reply2)
        {
            xcb_change_property(
                state_ptr->handle.connection,
                XCB_PROP_MODE_REPLACE,
                window_state->window,
                reply1->atom,
                XCB_ATOM_ATOM,
                32,
                1,
                &reply2->atom);

            state_ptr->window_close_atom = reply2->atom;
        }

        free(reply1);
        free(reply2);
    } 
    
    window_state->gcontext = xcb_generate_id(state_ptr->handle.connection);
    u32 gcontext_value_mask = 0;//XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
    uint32_t gcontext_values[2];
    gcontext_values[0] = state_ptr->handle.screen->black_pixel;
    gcontext_values[1] = 0;
    xcb_change_gc_value_list_t gcontext_value_list;
    xcb_void_cookie_t gcontext_cookie = xcb_create_gc(state_ptr->handle.connection, window_state->gcontext, window_state->window, gcontext_value_mask, &gcontext_value_list);
    xcb_generic_error_t *gcontext_error = xcb_request_check(state_ptr->handle.connection, window_cookie);

    if (gcontext_error) {
        printf("xcb_create_window failed: error_code=%u\n",
           gcontext_error->error_code);
        free(gcontext_error);
        return false;
    }
    xcb_flush(state_ptr->handle.connection);

    window->width = width;
    window->height = height;
    window_state->connection = state_ptr->handle.connection;
    window->platform_state = window_state;
    window->renderer_state = PNULL;
    vec_push(state_ptr->windows, &window);
    return true;
}

void platform_window_destroy(vwindow *window) {
  if (window) {
    u32 len = vec_length(state_ptr->windows);
    for (u32 i = 0; i < len; ++i) {
      if (state_ptr->windows[i] == window) {
        xcb_destroy_window(state_ptr->handle.connection,
                           window->platform_state->window);
        free(window->platform_state);
        window->platform_state = PNULL;
        state_ptr->windows[i] = PNULL;
        return;
      }
    }
    VERROR("Destroying a window that was somehow not registered with the platform layer.\n");
    xcb_destroy_window(state_ptr->handle.connection,
                       window->platform_state->window);
    window->platform_state->window = XCB_NONE;
  }
}

void platform_window_present_frame(vwindow* window, u8* pixel_map)
{
    vwindow_platform_state *window_state = window->platform_state;
    xcb_put_image(state_ptr->handle.connection, 
        XCB_IMAGE_FORMAT_Z_PIXMAP, 
        window_state->window, window_state->gcontext, 
        window->width, window->height, 0, 0, 
        0, state_ptr->handle.screen->root_depth, 
        window->width*window->height*sizeof(u32), pixel_map
    );

    xcb_flush(state_ptr->handle.connection);
}

void platform_pump_messages()
{
    xcb_generic_event_t* generic_event;
    while((generic_event = xcb_poll_for_event(state_ptr->handle.connection))){
        switch (generic_event->response_type & ~0x80) {
            case XCB_EXPOSE:{
                
            }break;
            //https://stackoverflow.com/questions/79919816/how-do-i-correctly-handle-xcb-resize-request-events
            case XCB_CONFIGURE_NOTIFY:{
                xcb_configure_notify_event_t* event = (xcb_configure_notify_event_t*)generic_event;
                vwindow* window = vwindow_from_xcb_window_t(event->window, PNULL);
                u16 width = event->width;
                u16 height = event->height;

                if(width != window->width || height != window->height){
                    window->width = width;
                    window->height = height;
                    if(state_ptr->window_resize_callback){
                        state_ptr->window_resize_callback(window);
                    }
                }
            }break;

            case XCB_CLIENT_MESSAGE: {
                xcb_client_message_event_t* event = (xcb_client_message_event_t*)generic_event;
                vwindow* window = vwindow_from_xcb_window_t(event->window, PNULL);
                if(event->data.data32[0] == state_ptr->window_close_atom){
                    if(state_ptr->window_close_callback){
                        state_ptr->window_close_callback(window);
                    }
                }
            }break;
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

void platform_write_console(CONSOLE_SINK sink, platform_string message){
    platform_string_internal imessage = message;
    FILE* stream = (sink == CONSOLE_SINK_OUT)? stdout : stderr;
    fprintf(stream, "%s", imessage);
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

b8 platform_virtual_reserve(vvirtual_memory* virtual, u64 to_reserve)
{
    if (!virtual || to_reserve == 0) return false;

    u64 aligned_reserve = platform_page_align(to_reserve);

    void* map = mmap(NULL, aligned_reserve, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map == MAP_FAILED) {
        return false;
    }

    virtual->base = map;
    virtual->committed = 0;
    virtual->reserved = aligned_reserve;
    return true;
}

void* platform_virtual_commit(vvirtual_memory* virtual, u64 to_commit)
{
    if (!virtual || !virtual->base || to_commit == 0) return PNULL;

    u64 aligned_commit = platform_page_align(to_commit);

    // Bounds check using the ALIGNED commit size
    if (virtual->committed + aligned_commit > virtual->reserved) return PNULL;

    uint8_t* commit_addr = (uint8_t*)virtual->base + virtual->committed;
    
    if (mprotect(commit_addr, aligned_commit, PROT_READ | PROT_WRITE) != 0) {
        return PNULL;
    }

    virtual->committed += aligned_commit;
    return commit_addr;
}

void platform_virtual_decommit(vvirtual_memory* virtual, u64 amount)
{
    if (!virtual || !virtual->base || amount == 0 || amount > virtual->committed) return;

    // Align decommit amount up to whole page boundaries
    u64 aligned_amount = platform_page_align(amount);
    if (aligned_amount > virtual->committed) {
        aligned_amount = virtual->committed;
    }

    u64 new_committed = virtual->committed - aligned_amount;
    uint8_t* decommit_addr = (uint8_t*)virtual->base + new_committed;

    // Revoke permissions on full page boundaries
    mprotect(decommit_addr, aligned_amount, PROT_NONE);

    // Return physical pages back to OS
#ifdef MADV_DONTNEED
    madvise(decommit_addr, aligned_amount, MADV_DONTNEED);
#endif

    virtual->committed = new_committed;
}

void platform_virtual_unreserve(vvirtual_memory* virtual)
{
    if (!virtual || !virtual->base) return;

    munmap(virtual->base, virtual->reserved);
    
    virtual->base = PNULL;
    virtual->reserved = 0;
    virtual->committed = 0;
}

void* platform_heap_allocate(u64 to_alloc){
    return malloc(to_alloc);
}

void platform_heap_deallocate(void* memory){
    free(memory);
}

void* platform_zero_memory(void* memory, u64 memory_size){
    return memset(memory, 0, memory_size);
}

void* platform_set_memory(void* memory, i32 value, u64 memory_size){
    return memset(memory, value, memory_size);
}

void* platform_copy_memory(void* dest, const void* src, u64 memory_size){
    return memcpy(dest, src, memory_size);
}

u32 platform_string_to_utf8(platform_string plf_str, char* utf8_str, u32 utf8_str_len)
{
    if(utf8_str_len){
        memcpy(utf8_str, plf_str, utf8_str_len);
        return utf8_str_len;
    }

    if(!plf_str) return 0;
    return strlen(plf_str);
}

u32 utf8_to_platform_string(char* utf8_str, platform_string plf_str, u32 plf_str_len)
{
    if(plf_str_len){
        memcpy(plf_str, utf8_str, plf_str_len);
        return plf_str_len;
    }
    if(!utf8_str) return 0; 
    return strlen(utf8_str);
}

static vwindow *vwindow_from_xcb_window_t(xcb_window_t handle, u64 *out_index) {
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

#endif