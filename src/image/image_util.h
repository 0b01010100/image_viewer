#pragma once
#include "../core/defines.h"

typedef struct {
    uint32_t *pixels;
    i32 width;
    i32 height;
} Image;


void* swizzle_rgba_to_bgra_horizontal(Image* src);

void image_resize_nearest(const Image *src, Image dst);

void* clear_color(Image* bitmap, i32 c);

void blit(Image dst,
                const  Image src,
                 i32 dst_x, i32 dst_y);