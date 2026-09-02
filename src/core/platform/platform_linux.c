
#if defined(PLATFORM_LINUX)
#include "platform.h"
#include "../array/darray.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <xcb/randr.h>
// #include <xcb/shm.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

typedef struct linux_handle_info {
  xcb_connection_t *connection;
  xcb_screen_t *screen;
} linux_handle_info;

typedef struct platform_state {
  linux_handle_info handle;
  xcb_intern_atom_reply_t *window_close_reply;
  vwindow **windows;

  platform_window_resize_callback window_resize_callback;
  platform_window_render_callback window_render_callback;
  platform_window_close_callback window_close_callback;
} platform_state;

static platform_state *state_ptr;

typedef struct vwindow_platform_state {
  xcb_window_t window;
  // this is wired for 2 reasons. I don't think win32 or apple encourange context sharing for windows
  // but xcb dose, but at the same time allocating a context wit hte window mean more allocation and fewer ids since xcb limits id to 32 byte int
  // my goal at first was to usethese 2 togeher to make use of the 4 byte padding
  // also life times is different for win32 and xcb
  // xcb_gcontext_t window_context;
  xcb_connection_t *connection;
  u8 depth;
} vwindow_platform_state;

b8 platform_initalize() {
  if (state_ptr) return false; // already initalized
  state_ptr = malloc(sizeof(platform_state));
  if (!state_ptr) {
    return false;
  }

  int screen_num;
  state_ptr->handle.connection = xcb_connect(PNULL, &screen_num);
  if (xcb_connection_has_error(state_ptr->handle.connection)) {
    free(state_ptr);
    return false;
  }

  const xcb_setup_t *setup = xcb_get_setup(state_ptr->handle.connection);
  const xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
  state_ptr->handle.screen = iter.data;

  state_ptr->window_close_reply = XCB_NONE;
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
                xcb_destroy_window(
                    state_ptr->handle.connection,
                    state_ptr->windows[i]->platform_state->window
                );

                state_ptr->windows[i]->platform_state->window = 0;

                free(state_ptr->windows[i]->platform_state);
                state_ptr->windows[i]->platform_state = PNULL;

            }
        }
        state_ptr->window_render_callback = PNULL;
        state_ptr->window_resize_callback = PNULL;
        state_ptr->window_close_callback = PNULL;
        xcb_flush(state_ptr->handle.connection);

        darray_destroy(state_ptr->windows);
        state_ptr->windows = PNULL;
        free(state_ptr->window_close_reply);

        xcb_disconnect(state_ptr->handle.connection);
        free(state_ptr);
        state_ptr = PNULL;
    }
}

b8 platform_window_create(vwindow *out_window, char const *name, u32 const w,
                          u32 const h, u32 const x, u32 const y) {
  if (!state_ptr || !state_ptr->handle.connection) {
    return false;
  }

  const xcb_setup_t *setup = xcb_get_setup(state_ptr->handle.connection);
  xcb_screen_iterator_t const iter = xcb_setup_roots_iterator(setup);

  const xcb_screen_t *screen = iter.data;

  vwindow_platform_state *state = malloc(sizeof(vwindow_platform_state));

  if (!state) {
    return false;
  }

  state->connection = state_ptr->handle.connection;
  state->depth = screen->root_depth;
  state->window = xcb_generate_id(state_ptr->handle.connection);

  u32 const value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  u32 const value_list[] = {screen->black_pixel,
                            XCB_EVENT_MASK_EXPOSURE |
                                XCB_EVENT_MASK_STRUCTURE_NOTIFY};

  xcb_create_window(state_ptr->handle.connection, XCB_COPY_FROM_PARENT,
                    state->window, screen->root, x, y, w, h, 0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
                    value_mask, value_list);

  u32 const name_len = strlen(name);
  xcb_change_property(state->connection, XCB_PROP_MODE_REPLACE, state->window,
                      XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, name_len, name);
  xcb_map_window(state_ptr->handle.connection, state->window);
  xcb_flush(state_ptr->handle.connection);

  if (state_ptr->window_close_reply == XCB_NONE) {
    xcb_intern_atom_cookie_t c_protocols =
        xcb_intern_atom(state_ptr->handle.connection, 1, 12, "WM_PROTOCOLS");
    xcb_intern_atom_cookie_t c_delete = xcb_intern_atom(
        state_ptr->handle.connection, 0, 16, "WM_DELETE_WINDOW");

    xcb_intern_atom_reply_t *reply1 =
        xcb_intern_atom_reply(state_ptr->handle.connection, c_protocols, 0);

    xcb_intern_atom_reply_t *reply2 =
        xcb_intern_atom_reply(state_ptr->handle.connection, c_delete, 0);

    xcb_change_property(state_ptr->handle.connection, XCB_PROP_MODE_REPLACE,
                        state->window, reply1->atom, 4, 32, 1, &reply2->atom);

    state_ptr->window_close_reply = reply2;
  }
  out_window->platform_state = state;
  out_window->width = w;
  out_window->height = h;
  darray_push(state_ptr->windows, out_window);
  return true;
}

void platform_window_destroy(vwindow *window)
{
    if (!window || !state_ptr) {
        return;
    }

    u32 len = darray_length(state_ptr->windows);

    for (u32 i = 0; i < len; ++i) {
        if (state_ptr->windows[i] == window) {
            xcb_destroy_window(
                state_ptr->handle.connection,
                window->platform_state->window
            );

            window->platform_state->window = XCB_NONE;

            free(window->platform_state);
            window->platform_state = PNULL;

            state_ptr->windows[i] = PNULL;

            xcb_flush(state_ptr->handle.connection);
            return;
        }
    }
}

static vwindow *vwindow_from_xcb_window_t(xcb_window_t handle, u64 *out_index) {
  for (u64 i = 0; i < darray_length(state_ptr->windows); i++) {
    if (state_ptr->windows[i]->platform_state->window == handle) {
      if (out_index) {
        *out_index = i;
      }
      return state_ptr->windows[i];
    }
  }
  return 0;
}

typedef struct vwindow_context_state {
  xcb_gcontext_t gc;
  vwindow *window;
} vwindow_context_state;

b8 platform_graphics_context_create(vwindow_context *out_context,
                                    vwindow *window) {
  vwindow_platform_state* window_state = window->platform_state;

  vwindow_context_state *conext_state =
      malloc(sizeof(vwindow_context_state));

  if (!conext_state) {
    return false;
  }

  conext_state->window = window;
  conext_state->gc = xcb_generate_id(window->platform_state->connection);

  xcb_void_cookie_t cookie = xcb_create_gc(window->platform_state->connection,
  conext_state->gc,
                                           window->platform_state->window, 0, PNULL);

  xcb_generic_error_t *error =
      xcb_request_check(window->platform_state->connection, cookie);

  if (error) {
    free(error);
    free(conext_state);
    return false;
  }

  out_context->platform_state = conext_state;

  return true;
}

void platform_graphics_context_put_image(vwindow_context *context,
                                         bitmap bm, u32 x, u32 y) {
  vwindow_context_state *graphics_state =
      (vwindow_context_state *)context->platform_state;

  vwindow_platform_state *window_state = graphics_state->window->platform_state;

  xcb_void_cookie_t cookie = xcb_put_image(
      window_state->connection, XCB_IMAGE_FORMAT_Z_PIXMAP,
      window_state->window, graphics_state->gc, bm.width, bm.height, x, y, 0,
      window_state->depth, bm.stride * bm.height, bm.pixels);

  xcb_generic_error_t *error =
      xcb_request_check(window_state->connection, cookie);

  if (error) {
    printf("xcb_put_image failed: error_code=%d\n", error->error_code);
    free(error);
    return;
  }

  xcb_flush(window_state->connection);
}

b8 platform_pump_message() {
  xcb_generic_event_t *generic_event;

  while ((generic_event = xcb_poll_for_event(state_ptr->handle.connection))) {
    switch (generic_event->response_type & ~0x80) {
    case XCB_EXPOSE: {
      xcb_expose_event_t *event = (xcb_expose_event_t *)generic_event;
      vwindow *window = vwindow_from_xcb_window_t(event->window, PNULL);
      if (darray_length(state_ptr->windows) <= 0) {
        break;
      }
      state_ptr->window_render_callback(window);
    } break;
    case XCB_CONFIGURE_NOTIFY: {
      xcb_configure_notify_event_t *event =
          (xcb_configure_notify_event_t *)generic_event;

      vwindow *window = vwindow_from_xcb_window_t(event->window, PNULL);

      if (event->width != window->width || event->height != window->height) {

        window->width = event->width;
        window->height = event->height;

        if (state_ptr->window_resize_callback) {
          state_ptr->window_resize_callback(window, window->width,
                                            window->height);
        }
      }
    } break;
    case XCB_CLIENT_MESSAGE: {
      xcb_client_message_event_t *event =
          (xcb_client_message_event_t *)generic_event;
      if (event->data.data32[0] == state_ptr->window_close_reply->atom) {
        vwindow *window = vwindow_from_xcb_window_t(event->window, PNULL);
        if (state_ptr->window_close_callback) {
          state_ptr->window_close_callback(window);
        }
      }
    } break;
  }
    free(generic_event);
  }
  return 1;
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