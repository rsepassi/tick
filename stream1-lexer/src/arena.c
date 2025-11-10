#include "../include/arena.h"
#include <stdlib.h>
#include <string.h>

void arena_init(Arena* arena, size_t block_size) {
    arena->block_size = block_size;
    arena->current = NULL;
    arena->first = NULL;
}

void* arena_alloc(Arena* arena, size_t size, size_t alignment) {
    // Align the size
    size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);

    // Check if current block has enough space
    if (arena->current == NULL ||
        arena->current->used + aligned_size > arena->current->capacity) {
        // Allocate new block
        size_t block_capacity = arena->block_size;
        if (aligned_size > block_capacity) {
            block_capacity = aligned_size;
        }

        ArenaBlock* new_block = (ArenaBlock*)malloc(sizeof(ArenaBlock) + block_capacity);
        new_block->next = NULL;
        new_block->used = 0;
        new_block->capacity = block_capacity;

        if (arena->current != NULL) {
            arena->current->next = new_block;
        } else {
            arena->first = new_block;
        }
        arena->current = new_block;
    }

    void* ptr = &arena->current->data[arena->current->used];
    arena->current->used += aligned_size;
    return ptr;
}

void arena_free(Arena* arena) {
    ArenaBlock* block = arena->first;
    while (block != NULL) {
        ArenaBlock* next = block->next;
        free(block);
        block = next;
    }
    arena->first = NULL;
    arena->current = NULL;
}
