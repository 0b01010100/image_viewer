#pragma once
#include "../defines.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct vec_base {
    u64 capacity;
    u64 length;
    u64 stride;
} vec_base;

#define vec_create(type, capacity) (type *)_vec_create(sizeof(type), capacity)

void *_vec_create(u64 stride, u64 capacity);

void *_vec_push(void *array, void *data);

#define vec_push(array, data) ((array) = _vec_push(array, data))

void vec_pop(void *array, void *popped);

u64 vec_length(void *array);

u64 vec_capacity(void *array);

u64 vec_stride(void *array);

void vec_destroy(void *array);
