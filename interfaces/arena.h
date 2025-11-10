#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>

// Arena Allocator Interface
// Purpose: Fast, structured allocation for compiler phases

typedef struct ArenaBlock {
    struct ArenaBlock* next;
    size_t used;
    size_t capacity;
    char data[];
} ArenaBlock;

typedef struct Arena {
    ArenaBlock* current;
    ArenaBlock* first;
    size_t block_size;
} Arena;

// Initialize arena with initial block size
void arena_init(Arena* arena, size_t block_size);

// Allocate from arena (never fails, grows automatically)
void* arena_alloc(Arena* arena, size_t size, size_t alignment);

// Free entire arena (all blocks at once)
void arena_free(Arena* arena);

#endif // ARENA_H
