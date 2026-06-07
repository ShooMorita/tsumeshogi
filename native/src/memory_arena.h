#ifndef MEMORY_ARENA_H
#define MEMORY_ARENA_H

#include <stdbool.h>
#include <stddef.h>

typedef struct ArenaChunk ArenaChunk;

typedef struct {
    ArenaChunk* first;
    ArenaChunk* current;
    size_t defaultCapacity;
} MemoryArena;

typedef struct {
    ArenaChunk* chunk;
    size_t used;
} ArenaMark;

bool arena_init(MemoryArena* arena, size_t defaultCapacity);
void arena_destroy(MemoryArena* arena);
void* arena_alloc_zero(MemoryArena* arena, size_t size);
ArenaMark arena_mark(const MemoryArena* arena);
void arena_rewind(MemoryArena* arena, ArenaMark mark);

#endif
