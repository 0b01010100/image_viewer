#pragma once
#include "../core/defines.h"
#include <math.h>

void* swizzle_rgba_to_bgra_horizontal(u8* bitmap, u32 lenght){
    if(!bitmap){
        return PNULL;
    }
    for (u32 i = 0; i<lenght; i += sizeof(u32)){
        u8 red = bitmap[i+0];
        bitmap[i+0] = bitmap[i+2];
        bitmap[i+2] = red;
    }
    return bitmap;
}

// https://medium.com/@epcm18/image-resampling-in-image-processing-f7b597ee78a8
typedef struct {
    uint32_t *pixels;
    i32 width;
    i32 height;
} Image;

void image_resize_nearest(const Image *src, Image dst) {
    for (u32 y = 0; y < dst.height; ++y) {
        u32 src_y = y * src->height / dst.height;
        for (u32 x = 0; x < dst.width; ++x) {
            u32 src_x = x * src->width / dst.width;
            dst.pixels[y * dst.width + x] =
                src->pixels[src_y * src->width + src_x];
        }
    }
}