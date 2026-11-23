#pragma once
#include "../defines.h"

typedef struct memory_system {
    u64 total_memory;
    void *memory;
} memory_system;

b8 memory_system_initailize(u64 total_memory, void *memory);

void memory_system_uninitailize(void);

void *memory_system_allocate_aligned(u64 requested_size, u64 requested_align);

void *memory_system_reallocate_aligned(void *memory, u64 new_requested_size,
                                       u64 requested_align);

void memory_system_deallocate(void *memory);

#define vallocate_aligned(requested_size, requested_align)                     \
    memory_system_allocate_aligned(requested_size, requested_align)

#define vallocate(requested_size)                                              \
    memory_system_allocate_aligned(requested_size, sizeof(char))

#define vreallocate_aligned(memory, requested_size, requested_align)           \
    memory_system_reallocate_aligned(memory, requested_size, requested_align)

#define vreallocate(memory, requested_size)                                    \
    memory_system_reallocate_aligned(memory, requested_size, sizeof(char))

#define vdeallocate(memory) memory_system_deallocate(memory)
