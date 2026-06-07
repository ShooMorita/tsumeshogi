#include "memory_arena.h"

#include <stdlib.h>
#include <string.h>

struct ArenaChunk {
    ArenaChunk* next;
    size_t capacity;
    size_t used;
    unsigned char data[];
};

static size_t align_size(size_t size)
{
    size_t alignment = sizeof(void*);
    return (size + alignment - 1) & ~(alignment - 1);
}

static bool arena_add_chunk(MemoryArena* arena, size_t minimumCapacity)
{
    size_t capacity = arena->defaultCapacity > minimumCapacity ? arena->defaultCapacity : minimumCapacity;
    ArenaChunk* chunk = (ArenaChunk*)malloc(sizeof(*chunk) + capacity);
    if (!chunk)
        return false;

    chunk->next = arena->current ? arena->current->next : NULL;
    chunk->capacity = capacity;
    chunk->used = 0;

    if (!arena->first) {
        arena->first = chunk;
    } else {
        arena->current->next = chunk;
    }
    arena->current = chunk;
    return true;
}

bool arena_init(MemoryArena* arena, size_t defaultCapacity)
{
    arena->first = NULL;
    arena->current = NULL;
    arena->defaultCapacity = defaultCapacity;
    return arena_add_chunk(arena, defaultCapacity);
}

void arena_destroy(MemoryArena* arena)
{
    ArenaChunk* chunk = arena->first;
    while (chunk) {
        ArenaChunk* next = chunk->next;
        free(chunk);
        chunk = next;
    }
    arena->first = NULL;
    arena->current = NULL;
}

void* arena_alloc_zero(MemoryArena* arena, size_t size)
{
    size_t alignedSize = align_size(size);
    if (!arena->current || arena->current->used + alignedSize > arena->current->capacity) {
        if (arena->current && arena->current->next && alignedSize <= arena->current->next->capacity) {
            arena->current = arena->current->next;
            arena->current->used = 0;
        } else if (!arena_add_chunk(arena, alignedSize)) {
            return NULL;
        }
    }

    void* memory = arena->current->data + arena->current->used;
    arena->current->used += alignedSize;
    memset(memory, 0, alignedSize);
    return memory;
}

ArenaMark arena_mark(const MemoryArena* arena)
{
    ArenaMark mark = { arena->current, arena->current ? arena->current->used : 0 };
    return mark;
}

void arena_rewind(MemoryArena* arena, ArenaMark mark)
{
    if (!mark.chunk) {
        for (ArenaChunk* chunk = arena->first; chunk; chunk = chunk->next)
            chunk->used = 0;
        arena->current = arena->first;
        return;
    }

    bool afterMark = false;
    for (ArenaChunk* chunk = arena->first; chunk; chunk = chunk->next) {
        if (chunk == mark.chunk) {
            chunk->used = mark.used;
            afterMark = true;
        } else if (afterMark) {
            chunk->used = 0;
        }
    }
    arena->current = mark.chunk;
}
