#include "arena.h"
#include "../platform/platform.h"

b8 arena_create(arena* arena, void* memory, u64 memory_total)
{
    if (!arena || !memory || !memory_total){
        return false;
    }

    arena->memory = memory;
    arena->memory_total = memory_total;
    arena->offset = 0;

    platform_zero_memory(arena->memory, arena->memory_total);

    return true;
}

void* arena_allocate(arena* arena, u64 memory_to_allocate)
{
    if (!arena || !memory_to_allocate){
        return PNULL;
    }

    if(arena->memory_total - arena->offset < memory_to_allocate){
        return PNULL;
    }

    void* requested = (u8*)arena->memory + arena->offset; 
    arena->offset += memory_to_allocate;
    return requested;
}

void arena_free_all(arena* arena)
{
    platform_zero_memory(arena->memory, arena->memory_total);
    arena->offset = 0;
}

void arena_destroy(arena* arena)
{
    platform_zero_memory(arena->memory, arena->memory_total);
    arena->memory = PNULL;
    arena->memory_total = 0;
    arena->offset = 0;
}