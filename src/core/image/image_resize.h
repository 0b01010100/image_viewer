#include "../defines.h"
#include <stddef.h>

typedef struct {
    uint32_t *pixels;
    size_t width;
    size_t height;
} Image;

void image_resize_nearest(const Image *src, Image dst) {
    for (size_t y = 0; y < dst.height; ++y) {
        size_t src_y = y * src->height / dst.height;
        for (size_t x = 0; x < dst.width; ++x) {
            size_t src_x = x * src->width / dst.width;
            dst.pixels[y * dst.width + x] =
                src->pixels[src_y * src->width + src_x];
        }
    }
}

void resize_nearest(const Image *src, Image *dst) {
    for (size_t y = 0; y < dst->height; ++y) {
        size_t src_y = y * src->height / dst->height;

        for (size_t x = 0; x < dst->width; ++x) {
            size_t src_x = x * src->width / dst->width;

            dst->pixels[y * dst->width + x] =
                src->pixels[src_y * src->width + src_x];
        }
    }
}
