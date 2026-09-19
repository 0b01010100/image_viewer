#include "../defines.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

typedef uint64_t u64;
typedef uint8_t u8;

typedef struct vec_base {
    u64 capacity;
    u64 length;
    u64 stride;
} vec_base;

#define vec_create(type, capacity) (type*)_vec_create(sizeof(type), capacity)

void* _vec_create(u64 stride, u64 capacity){
    vec_base *new_array = malloc(sizeof(vec_base) + (capacity * stride));
    if (!new_array) return NULL;
    
    new_array->stride = stride;
    new_array->capacity = capacity;
    new_array->length = 0;
    return (char*)new_array + sizeof(vec_base);
}

void* _vec_push(void* array, void* data) { 
    vec_base *base = (vec_base *)((char *)array - sizeof(vec_base));

    if (base->length >= base->capacity) {
        u64 new_capacity = base->capacity == 0 ? 1 : base->capacity * 2;

        vec_base *new_base = realloc(base, sizeof(vec_base) + (new_capacity * base->stride));
        if (!new_base) return NULL; 

        new_base->capacity = new_capacity;
        base = new_base;
    }

    char *data_start = (char *)base + sizeof(vec_base);
    memcpy(data_start + (base->length * base->stride), data, base->stride);

    base->length++;
    return data_start;
}

#define vec_push(array, data) (array = _vec_push(array, data))

void vec_pop(void* array, void* popped) {
    assert(array);

    vec_base* base = (vec_base*)((char*)array - sizeof(vec_base));
    if(base->length == 0){
        return;
    }
    
    char* data_start = (char*)base + sizeof(vec_base);
    u64 offset = (base->length - 1) * base->stride; // FIX: length - 1
    
    memcpy(popped, data_start + offset, base->stride);
    memset(data_start + offset, 0, base->stride);
    
    base->length -= 1;
}

u64 vec_length(void* array){
    return ((vec_base*)((char*)array - sizeof(vec_base)))->length;
}

u64 vec_capacity(void* array){
    return ((vec_base*)((char*)array - sizeof(vec_base)))->capacity;
}

u64 vec_stride(void* array){
    return ((vec_base*)((char*)array - sizeof(vec_base)))->stride;
}

void vec_destroy(void* array){
    if(!array){ 
        return;
    }
    vec_base* base = (vec_base*)((char*)array - sizeof(vec_base));
    free(base);
}