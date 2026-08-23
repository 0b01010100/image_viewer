#pragma once
#include "../defines.h"
typedef struct vwindow_platform_state vwindow_platform_state;
typedef struct vwindow_renderer_state vwindow_renderer_state;

typedef struct vwindow {
  vwindow_platform_state *platform_state;
  vwindow_renderer_state *renderer_state;
  u32 width;
  u32 height;

  u32 position_x;
  u32 position_y;
} vwindow;

typedef struct platform_graphics_context_state platform_graphics_context_state;

typedef struct graphics_context_handle {
  platform_graphics_context_state *internal_handle;
} graphics_context_handle;

typedef void (*platform_window_resize_callback)(vwindow *const window,
                                                int x, int y);
typedef void (*platform_window_render_callback)(vwindow *const window);
typedef void (*platform_window_close_callback)(vwindow *const window,
                                               b8 is_last_window);

b8 platform_initalize();

b8 platform_window_create(vwindow *out_window, char const *name,
                          u32 const w, u32 const h, u32 const x, u32 const y);
void platform_window_destroy(vwindow* winodw);
b8 platform_pump_message();

b8 platform_graphics_context_create(graphics_context_handle *out_context,
                                    vwindow *window);

typedef enum image_format {
  IMAGE_FORMAT_B8G8R8A8,
  IMAGE_FORMAT_B8G8R8
} image_format;

typedef struct bitmap {
  u32 width;
  u32 height;
  u32 stride;
  image_format format;
  void *pixels;
} bitmap;

// void platform_graphics_context_put_image(graphics_context_handle *context,
//                                          bitmap bm, u32 x, u32 y);

void platform_window_present_frame(vwindow* window, void* pixels, u32 width, u32 height, u32 stride, u32 x, u32 y, b8 has_alpha);


typedef struct library_handle {
  void *internal_handle;
} library_handle;

void platform_window_set_resize_callback(platform_window_resize_callback cb);
void platform_window_set_render_callback(platform_window_render_callback cb);
void platform_window_set_close_callback(platform_window_close_callback cb);