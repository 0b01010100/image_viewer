#pragma once
#include "../defines.h"

typedef struct darray_base {
  u64 stride;
  u64 length;
  u64 capacity;
  void *data;
} darray_base;

void *_darray_create(u64 stride, u64 length);

#define darray_create(type, length) _darray_create(sizeof(type), length)

u64 darray_stride(void *array);

u64 darray_length(void *array);

u64 darray_capacity(void *array);

void *_darray_push(void *array, const void *data);

void darray_destroy(void *array);

void *darray_pop_at(void *array, u64 index, void *dest);

#define darray_push(array, data)                                               \
  {                                                                            \
    typeof(data) temp_data = data;                                             \
    (array = _darray_push(array, &temp_data));                                 \
  }
