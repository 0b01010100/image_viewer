#include "../defines.h"

typedef struct arena {
    void* memory;
    u64 memory_total;
    u64 offset;
}arena;

b8 arena_create(arena* arena, void* memory, u64 memory_total);

void* arena_allocate(arena* arena, u64 memory_to_allocate);

void arena_free_all(arena* arena);

void arena_destroy(arena* arena);