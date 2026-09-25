#include "../core/platform/platform.h"
#include "image_util.h"
void* swizzle_rgba_to_bgra_horizontal(Image* src)
{
    if (!src || !src->pixels)
        return PNULL;

    u8* pixels = (u8*)src->pixels;

    u64 size = (u64)src->width * src->height;

    for (u64 i = 0; i < size; ++i) {
        u8 red = pixels[i * 4 + 0];
        pixels[i * 4 + 0] = pixels[i * 4 + 2];
        pixels[i * 4 + 2] = red;
    }

    return src->pixels;
}

// https://medium.com/@epcm18/image-resampling-in-image-processing-f7b597ee78a8
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


void* clear_color(Image* bitmap, i32 c) {
    if(!bitmap){
        return PNULL;
    }
    platform_set_memory(bitmap->pixels, c, bitmap->width * bitmap->height * sizeof(u32));
    return bitmap->pixels;
}

void blit(Image dst,
          const Image src,
          i32 dst_x, i32 dst_y)
{
    u32 copy_w = src.width;
    u32 copy_h = src.height;

    u8* pixels = (u8*)src.pixels;

    if (dst_x < 0) {
        pixels += (u64)(-dst_x) * 4;
        copy_w -= (u32)(-dst_x);
        dst_x = 0;
    }

    if (dst_y < 0) {
        pixels += (u64)(-dst_y) * src.width * 4;
        copy_h -= (u32)(-dst_y);
        dst_y = 0;
    }

    if ((u32)dst_x >= dst.width || (u32)dst_y >= dst.height)
        return;

    if (dst_x + copy_w > dst.width){
        copy_w = dst.width - dst_x;
    }

    if (dst_y + copy_h > dst.height){
        copy_h = dst.height - dst_y;
    }

    for (u32 y = 0; y < copy_h; ++y) {
        platform_copy_memory(
            (u8*)dst.pixels +
                ((u64)(dst_y + y) * dst.width + dst_x) * 4,

            pixels + (u64)y * src.width * 4,

            (u64)copy_w * 4
        );
    }
}