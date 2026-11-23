#include "../containers/vec.h"
#include "../memory/memory.h"
#include "../platform/platform.h"

void *_vec_create(u64 stride, u64 capacity) {
    vec_base *new_array = vallocate(sizeof(vec_base) + (capacity * stride));
    if (!new_array)
        return nullptr;

    new_array->stride = stride;
    new_array->capacity = capacity;
    new_array->length = 0;
    return (char *)new_array + sizeof(vec_base);
}

void *_vec_push(void *array, void *data) {
    vec_base *base = (vec_base *)((char *)array - sizeof(vec_base));

    if (base->length >= base->capacity) {
        u64 new_capacity = base->capacity == 0 ? 1 : base->capacity * 2;

        vec_base *new_base =
            vreallocate(base, sizeof(vec_base) + (new_capacity * base->stride));
        if (!new_base)
            return nullptr;

        new_base->capacity = new_capacity;
        base = new_base;
    }

    char *data_start = (char *)base + sizeof(vec_base);
    platform_copy_memory(data_start + (base->length * base->stride), data,
                         base->stride);

    base->length++;
    return data_start;
}

void vec_pop(void *array, void *popped) {
    assert(array);

    vec_base *base = (vec_base *)((char *)array - sizeof(vec_base));
    if (base->length == 0) {
        return;
    }

    char *data_start = (char *)base + sizeof(vec_base);
    u64 offset = (base->length - 1) * base->stride; // FIX: length - 1

    platform_copy_memory(popped, data_start + offset, base->stride);
    platform_set_memory(data_start + offset, 0, base->stride);

    base->length -= 1;
}

u64 vec_length(void *array) {
    return ((vec_base *)((char *)array - sizeof(vec_base)))->length;
}

u64 vec_capacity(void *array) {
    return ((vec_base *)((char *)array - sizeof(vec_base)))->capacity;
}

u64 vec_stride(void *array) {
    return ((vec_base *)((char *)array - sizeof(vec_base)))->stride;
}

void vec_destroy(void *array) {
    if (!array) {
        return;
    }
    vec_base *base = (vec_base *)((char *)array - sizeof(vec_base));
    vdeallocate(base);
}
