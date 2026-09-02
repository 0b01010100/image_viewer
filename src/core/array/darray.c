#include "darray.h" 
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *_darray_create(u64 stride, u64 length) {
  darray_base *new_array = malloc(sizeof(darray_base) + length * stride);
  new_array->stride = stride;
  new_array->capacity = length;
  new_array->length = 0;
  return new_array->data = (char *)new_array + sizeof(darray_base);
}

#define darray_create(type, length) _darray_create(sizeof(type), length)

u64 darray_stride(void *array) {
  return ((darray_base *)((char *)array - sizeof(darray_base)))->stride;
}
u64 darray_length(void *array) {
  return ((darray_base *)((char *)array - sizeof(darray_base)))->length;
}

u64 darray_capacity(void *array) {
  return ((darray_base *)((char *)array - sizeof(darray_base)))->capacity;
}

void *_darray_push(void *array, const void *data) {
  darray_base *base = (darray_base *)((char *)array - sizeof(darray_base));

  darray_base *new_base = base;

  if (base->length >= base->capacity) {
    size_t new_capacity = base->capacity * 2;

    new_base = malloc(sizeof(darray_base) + new_capacity * base->stride);

    memcpy(new_base, base, sizeof(darray_base) + base->capacity * base->stride);

    new_base->capacity = new_capacity;

    free(base);
  }

  memcpy((char *)new_base + sizeof(darray_base) +
             new_base->length * new_base->stride,
         data, new_base->stride);

  new_base->length++;
  return new_base->data = (char *)new_base + sizeof(darray_base);
}

#define darray_push(array, data)                                               \
  {                                                                            \
    typeof(data) temp_data = data;                                             \
    (array = _darray_push(array, &temp_data));                                 \
  }

void darray_destroy(void *array) { free(array); }

void *darray_pop_at(void *array, u64 index, void *dest) {
  if (!array)
    return PNULL;

  darray_base *base = (darray_base *)((char *)array - sizeof(darray_base));

  if (index >= base->length)
    return PNULL;

  uint8_t *target = (uint8_t *)array + (index * base->stride);

  if (dest) {
    memcpy(dest, target, base->stride);
  }

  u64 tail_elements = base->length - index - 1;
  if (tail_elements > 0) {
    memmove(target, target + base->stride, tail_elements * base->stride);
  }

  base->length--;

  return array;
}

/*int main() {
  int *arr_int = darray_create(int, 2);
  darray_push(arr_int, 1011);
  darray_push(arr_int, 100);
  darray_push(arr_int, 890);
  printf("%i", arr_int[2]);
}*/
