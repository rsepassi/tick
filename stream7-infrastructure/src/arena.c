#include "arena.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

// Default block size (64KB)
#define DEFAULT_BLOCK_SIZE (64 * 1024)

// Minimum alignment for all allocations
#define MIN_ALIGNMENT sizeof(void*)

// Helper to align a size to the given alignment
static inline size_t align_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

// Allocate a new block with the given capacity
static ArenaBlock* arena_block_alloc(size_t capacity) {
    // Ensure capacity is aligned
    capacity = align_size(capacity, MIN_ALIGNMENT);

    // Allocate block header + data in one chunk
    ArenaBlock* block = (ArenaBlock*)malloc(sizeof(ArenaBlock) + capacity);
    if (!block) {
        // Out of memory - this is a fatal error in a compiler
        fprintf(stderr, "FATAL: Out of memory allocating arena block\n");
        abort();
    }

    block->next = NULL;
    block->used = 0;
    block->capacity = capacity;

    return block;
}

void arena_init(Arena* arena, size_t block_size) {
    assert(arena != NULL);

    if (block_size == 0) {
        block_size = DEFAULT_BLOCK_SIZE;
    }

    // Ensure block_size is reasonable (at least 1KB)
    if (block_size < 1024) {
        block_size = 1024;
    }

    arena->block_size = block_size;
    arena->first = arena_block_alloc(block_size);
    arena->current = arena->first;
}

void* arena_alloc(Arena* arena, size_t size, size_t alignment) {
    assert(arena != NULL);
    assert(arena->current != NULL);

    // Use default alignment if not specified
    if (alignment == 0) {
        alignment = MIN_ALIGNMENT;
    }

    // Ensure alignment is a power of 2
    assert((alignment & (alignment - 1)) == 0);

    // Align the current position
    size_t current_pos = arena->current->used;
    size_t aligned_pos = align_size(current_pos, alignment);
    (void)(aligned_pos - current_pos); // padding calculation, result unused

    // Check if we have space in current block
    if (aligned_pos + size <= arena->current->capacity) {
        void* ptr = arena->current->data + aligned_pos;
        arena->current->used = aligned_pos + size;
        return ptr;
    }

    // Need a new block
    // If the allocation is larger than our standard block size, allocate a custom-sized block
    size_t new_block_size = arena->block_size;
    if (size > new_block_size) {
        new_block_size = size + alignment;
    }

    ArenaBlock* new_block = arena_block_alloc(new_block_size);

    // Link the new block
    arena->current->next = new_block;
    arena->current = new_block;

    // Allocate from the new block (which starts at 0, so alignment is easy)
    size_t new_aligned_pos = align_size(0, alignment);
    void* ptr = new_block->data + new_aligned_pos;
    new_block->used = new_aligned_pos + size;

    return ptr;
}

void arena_free(Arena* arena) {
    assert(arena != NULL);

    ArenaBlock* block = arena->first;
    while (block != NULL) {
        ArenaBlock* next = block->next;
        free(block);
        block = next;
    }

    arena->first = NULL;
    arena->current = NULL;
}
