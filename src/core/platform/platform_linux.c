#include "platform.h"
#if defined(PLATFORM_LINUX)
#include "../array/darray.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <xcb/randr.h>
// #include <xcb/shm.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

typedef struct linux_handle_info {
	xcb_connection_t* connection;
	xcb_screen_t* screen;
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
  xcb_connection_t *connection;

  xcb_gcontext_t window_context;
  u8 depth;
} vwindow_platform_state;

b8 platform_initalize() {
  state_ptr = malloc(sizeof(platform_state));
  if (!state_ptr) {
    return false;
  }

  u32 screen_num;
  state_ptr->handle.connection = xcb_connect(PNULL, &screen_num);
  if (xcb_connection_has_error(state_ptr->handle.connection)) {
    free(state_ptr);
    return false;
  }

  const xcb_setup_t *setup = xcb_get_setup(state_ptr->handle.connection);
  const xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
  state_ptr->handle.screen = iter.data;

  state_ptr->window_close_reply = XCB_NONE;
  state_ptr->windows = darray_create(vwindow *, 1);
  state_ptr->window_render_callback = PNULL;
  state_ptr->window_resize_callback = PNULL;
  state_ptr->window_close_callback = PNULL;
  return true;
}

b8 platform_window_create(vwindow *out_window, char const *name,
                          u32 const w, u32 const h, u32 const x, u32 const y) {
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

  xcb_create_window(state_ptr->handle.connection, XCB_COPY_FROM_PARENT, state->window,
                    screen->root, x, y, w, h, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                    screen->root_visual, value_mask, value_list);

  u32 const name_len = strlen(name);
  xcb_change_property(state->connection, XCB_PROP_MODE_REPLACE, state->window,
                      XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, name_len, name);
  xcb_map_window(state_ptr->handle.connection, state->window);
  xcb_flush(state_ptr->handle.connection);

  if (state_ptr->window_close_reply == XCB_NONE) {
    xcb_intern_atom_cookie_t c_protocols =
        xcb_intern_atom(state_ptr->handle.connection, 1, 12, "WM_PROTOCOLS");
    xcb_intern_atom_cookie_t c_delete =
        xcb_intern_atom(state_ptr->handle.connection, 0, 16, "WM_DELETE_WINDOW");

    xcb_intern_atom_reply_t *reply1 =
        xcb_intern_atom_reply(state_ptr->handle.connection, c_protocols, 0);

    xcb_intern_atom_reply_t *reply2 =
        xcb_intern_atom_reply(state_ptr->handle.connection, c_delete, 0);

    xcb_change_property(state_ptr->handle.connection, XCB_PROP_MODE_REPLACE,
                        state->window, reply1->atom, 4, 32, 1, &reply2->atom);

    state_ptr->window_close_reply = reply2;
  }
  out_window->platform_state = state;
  darray_push(state_ptr->windows, out_window);
  return true;
}

void platform_window_destroy(vwindow* window){
	if (window) {
		u32 len = darray_length(state_ptr->windows);
		for (u32 i = 0; i < len; ++i) {
			if (state_ptr->windows[i] == window) {
				//string_free(window->name);
				//string_free(window->title);
				xcb_destroy_window(state_ptr->handle.connection, window->platform_state->window);
				free(window->platform_state);
				window->platform_state = PNULL;
				state_ptr->windows[i] = PNULL;
				return;
			}
		}
		//KERROR("Destroying a window that was somehow not registered with the platform layer.");
		xcb_destroy_window(state_ptr->handle.connection, window->platform_state->window);
		window->platform_state->window = XCB_NONE;
	}
}

static vwindow *vwindow_from_xcb_window_t(xcb_window_t handle,
                                                      u64 *out_index) {
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

void platform_window_present_frame(vwindow* window, void* pixels, u32 width, u32 height, u32 stride, u32 x, u32 y, b8 has_alpha){
    if (!window->platform_state->window_context){
        window->platform_state->window_context = xcb_generate_id(window->platform_state->connection);
        xcb_void_cookie_t cookie = xcb_create_gc(window->platform_state->connection, window->platform_state->window_context,
                                           window->platform_state->window, 0, PNULL);

        xcb_generic_error_t *error =
          xcb_request_check(window->platform_state->connection, cookie);
        if (error) {
            free(error);
            return;
        }
    }

    xcb_void_cookie_t cookie = xcb_put_image(
      window->platform_state->connection, XCB_IMAGE_FORMAT_Z_PIXMAP, window->platform_state->window,
      window->platform_state->window_context, width, height, x, y, 0, window->platform_state->depth,
      stride * height, pixels);

    xcb_generic_error_t *error =
      xcb_request_check(window->platform_state->connection, cookie);

    if (error) {
        printf("xcb_put_image failed: error_code=%d\n", error->error_code);
        free(error);
        return;
    }

    xcb_flush(window->platform_state->connection);
}

// typedef struct platform_graphics_context_state {
//   xcb_gcontext_t gc;
//   vwindow *window;
// } platform_graphics_context_state;

// b8 platform_graphics_context_create(graphics_context_handle *out_context,
//                                     vwindow *window) {
//    *window_state =
//       (platform_window_state *)window->internal_handle;

//   platform_graphics_context_state *state =
//       malloc(sizeof(platform_graphics_context_state));

//   if (!state) {
//     return false;
//   }

//   state->window = window;
//   state->gc = xcb_generate_id(window_state->connection);

//   xcb_void_cookie_t cookie = xcb_create_gc(window_state->connection, state->gc,
//                                            window_state->handle, 0, PNULL);

//   xcb_generic_error_t *error =
//       xcb_request_check(window_state->connection, cookie);

//   if (error) {
//     free(error);
//     free(state);
//     return false;
//   }

//   out_context->internal_handle = state;

//   return true;
// }

// void platform_graphics_context_put_image(graphics_context_handle *context,
//                                          bitmap bm, u32 x, u32 y) {
//   platform_graphics_context_state *graphics_state =
//       (platform_graphics_context_state *)context->internal_handle;

//   platform_window_state *window_state =
//       (platform_window_state *)graphics_state->window->internal_handle;

//   xcb_void_cookie_t cookie = xcb_put_image(
//       window_state->connection, XCB_IMAGE_FORMAT_Z_PIXMAP, window_state->handle,
//       graphics_state->gc, bm.width, bm.height, x, y, 0, window_state->depth,
//       bm.stride * bm.height, bm.pixels);

//   xcb_generic_error_t *error =
//       xcb_request_check(window_state->connection, cookie);

//   if (error) {
//     printf("xcb_put_image failed: error_code=%d\n", error->error_code);
//     free(error);
//     return;
//   }

//   xcb_flush(window_state->connection);
// }

b8 platform_pump_message() {
  xcb_generic_event_t *generic_event;

  while ((generic_event = xcb_poll_for_event(state_ptr->handle.connection))) {
    switch (generic_event->response_type & ~0x80) {
    case XCB_EXPOSE: {
      xcb_expose_event_t *event = (xcb_expose_event_t *)generic_event;
      vwindow *window =
          vwindow_from_xcb_window_t(event->window, PNULL);
      if (darray_length(state_ptr->windows) <= 0) {
        break;
      }
      state_ptr->window_render_callback(window);
    } break;
    case XCB_CONFIGURE_NOTIFY: {
      xcb_configure_notify_event_t *event =
          (xcb_configure_notify_event_t *)generic_event;

      vwindow *window =
          vwindow_from_xcb_window_t(event->window, PNULL);

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
        u64 index = 0;
        vwindow *window =
            vwindow_from_xcb_window_t(event->window, &index);
        b8 is_last = darray_length(state_ptr->windows);
        if (state_ptr->window_close_callback) {
          state_ptr->window_close_callback(window, is_last);
        }
        xcb_destroy_window(state_ptr->handle.connection, event->window);
        xcb_flush(state_ptr->handle.connection);
        darray_pop_at(state_ptr->windows, index, PNULL);
        if (is_last == true) {
          return 0;
        }
      }
    } break;
      free(generic_event);
    }
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
