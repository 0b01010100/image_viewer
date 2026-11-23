#include "memory.h"
#include "../defines.h"
#include "../platform/platform.h"

typedef struct alloc_header {
    void *raw_memory;
    u64 size;
} alloc_header;

static memory_system _memory_system;

b8 memory_system_initialize(u64 total_memory, void *memory) {
    if (!memory || !total_memory) {
        return false;
    }

    _memory_system.total_memory = total_memory;
    _memory_system.memory = memory;
    return true;
}

void memory_system_uninitialize(void) {
    _memory_system.memory = nullptr;
    _memory_system.total_memory = 0;
}

void *memory_system_allocate_aligned(u64 requested_size, u64 requested_align) {
    if (requested_align < get_up_align(sizeof(alloc_header), sizeof(void*))) {
        requested_align = get_up_align(sizeof(alloc_header), sizeof(void*));
    }

    if ((requested_align & (requested_align - 1)) != 0) {
        return nullptr;
    }

    u64 total_size = requested_size + requested_align + sizeof(alloc_header);
    void *raw_memory = platform_heap_allocate(total_size);
    if (!raw_memory) {
        return nullptr;
    }

    uintptr_t raw_address = (uintptr_t)raw_memory;
    uintptr_t aligned_address =
        (raw_address + sizeof(alloc_header) + requested_align - 1) &
        ~(requested_align - 1);

    alloc_header *header = (alloc_header *)(aligned_address - sizeof(alloc_header));
    header->raw_memory = raw_memory;
    header->size = requested_size;

    return (void *)aligned_address;
}

void *memory_system_reallocate_aligned(void *memory, u64 new_requested_size,
                                       u64 requested_align) {
    if (!memory) {
        return memory_system_allocate_aligned(new_requested_size,
                                              requested_align);
    }
    if (new_requested_size == 0) {
        memory_system_deallocate(memory);
        return nullptr;
    }

    alloc_header *old_header =
        (alloc_header *)((uintptr_t)memory - sizeof(alloc_header));

    void *new_memory =
        memory_system_allocate_aligned(new_requested_size, requested_align);
    if (!new_memory) {
        return nullptr;
    }

    u64 copy_size = old_header->size < new_requested_size
                        ? old_header->size
                        : new_requested_size;
    platform_copy_memory(new_memory, memory, copy_size);

    memory_system_deallocate(memory);
    return new_memory;
}

void *memory_system_reallocate(void *memory, u64 new_requested_size) {
    return memory_system_reallocate_aligned(memory, new_requested_size,
                                            get_up_align(sizeof(alloc_header), sizeof(void*)));
}

void memory_system_deallocate(void *memory) {
    if (!memory) {
        return;
    }

    alloc_header *header =
        (alloc_header *)((uintptr_t)memory - sizeof(alloc_header));
    void *raw_memory = header->raw_memory;

    platform_heap_deallocate(raw_memory);
}
