#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include "core/image/image_resize.h"
#include "core/image/stbi_image.h"
#include "core/platform/platform.h"

bitmap bm; 
bitmap clear_bm;
graphics_context_handle gch;
b8 is_running = true;
void window_resize_hook(vwindow *const state, int width, int height) {
  state->width = width;
  state->height = height;
}

void window_render_hook(vwindow *const state) {
  u32 const center_x =
      (state->width > bm.width) ? (state->width - bm.width) / 2 : 0;
  u32 const center_y =
      (state->height > bm.height) ? (state->height - bm.height) / 2 : 0;

  platform_graphics_context_put_image(&gch, clear_bm, center_x, center_y);

  platform_graphics_context_put_image(&gch, bm, center_x, center_y);
  //platform_window_present_frame(state, bm.pixels, bm.width, bm.height, bm.stride, center_x, center_y, true);

}

void window_close_hook(vwindow *const state, b8 is_last_window) {
  is_running = false;
}

static void swizzle_rgba_to_bgra(bitmap *bm) {
  u8 *pixels = (u8 *)bm->pixels;
  u32 const pixel_count = bm->width * bm->height;

  for (u32 i = 0; i < pixel_count; i++) {
    u8 *p = pixels + i * 4;
    u8 const r = p[0];
    p[0] = p[2];
    p[2] = r;
  }
}

int main(void) {
  if (!platform_initalize()) {
    printf("Platform initialization failed\n");
    return 1;
  }
  platform_window_set_resize_callback(&window_resize_hook);
  platform_window_set_render_callback(&window_render_hook);
  platform_window_set_close_callback(&window_close_hook);

  bm = (bitmap){.stride = sizeof(int) * 4, .format = IMAGE_FORMAT_B8G8R8A8};
  int channels;
  bm.pixels = stbi_load("E.jpg", (int*)&bm.width, (int*)&bm.height, &channels, 4);
  if (!bm.pixels) {
    printf("%s\n", stbi_failure_reason());
    return 1;
  }
  swizzle_rgba_to_bgra(&bm);
  printf("Image loaded\n");
  vwindow wh;
  u32 const window_w = 800;
  u32 const window_h = 600;
  if (!platform_window_create(&wh, "Texture View", window_w, window_h, 600,
                              250)) {
    printf("Window Not Created\n");
    stbi_image_free(bm.pixels);
    return 1;
  }
  printf("Window Created\n");

  platform_graphics_context_create(&gch, &wh);
  Image src = {
      .width = bm.width, .height = bm.height, .pixels = (u32 *)bm.pixels};

  u32 const new_width = 800;
  u32 const new_height = 600;
  Image dst = {.width = new_width,
               .height = new_height,
               .pixels = malloc(new_width * new_height * sizeof(*dst.pixels))};
  if (!dst.pixels) {
    printf("Failed to allocate resized image\n");
    stbi_image_free(bm.pixels);
    return 1;
  }
  image_resize_nearest(&src, dst);
  bm = (bitmap){.width = dst.width,
                .height = dst.height,
                .stride = dst.width * 4,
                .pixels = dst.pixels,
                .format = IMAGE_FORMAT_B8G8R8A8};

clear_bm = (bitmap){
    .width  = dst.width,
    .height = dst.height,
    .stride = dst.width * 4,
    .pixels = malloc((size_t)dst.width * dst.height * 4),
    .format = IMAGE_FORMAT_B8G8R8A8
};

u32* pixels = (u32*)clear_bm.pixels;

for (u32 i = 0; i < dst.width * dst.height; ++i) {
    pixels[i] = 0xFF000000;
}
  printf("image resized now drawing\n");
  // WM_CREATE  is slightly different than XCB_EXPOSE
  // WIN32 doesn't seem to call WM_PAINT at first
  // so I am left with two options call WM_PAINT from WM_CREATE, but it may
  // not render the image becasue WM_CREATE happens inside CreateWindowEx just
  // ingore the problem and manlly call what ever rendering funciton after
  // window creation. and change the XCB_EXPOSE to return out early if
  // rendering context if not created.
  u32 const center_x = (window_w > bm.width) ? (window_w - bm.width) / 2 : 0;
  u32 const center_y = (window_h > bm.height) ? (window_h - bm.height) / 2 : 0;
  platform_graphics_context_put_image(&gch, bm, center_x, center_y);
  //platform_window_present_frame(&wh, bm.pixels, bm.width, bm.height, bm.stride, center_x, center_y, true);
  while (is_running) {
    platform_pump_message();
  }
  stbi_image_free(bm.pixels);
  return 0;
}
