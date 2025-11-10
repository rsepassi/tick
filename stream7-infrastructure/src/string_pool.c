#include "string_pool.h"
#include "arena.h"
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Initial capacity for strings array
#define INITIAL_STRING_CAPACITY 256

// Simple FNV-1a hash for strings (unused for now, but useful for future trie/hash table)
static uint32_t hash_string(const char* str, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint8_t)str[i];
        hash *= 16777619u;
    }
    return hash;
}

void string_pool_init(StringPool* pool, Arena* arena) {
    assert(pool != NULL);
    assert(arena != NULL);

    pool->arena = arena;

    // Allocate initial strings array from arena
    pool->strings = (const char**)arena_alloc(arena,
                                              INITIAL_STRING_CAPACITY * sizeof(const char*),
                                              sizeof(void*));
    pool->string_count = 0;
    pool->string_capacity = INITIAL_STRING_CAPACITY;
}

const char* string_pool_lookup(StringPool* pool, const char* str, size_t len) {
    assert(pool != NULL);
    assert(str != NULL);

    // Linear search for now - simple but O(n)
    // TODO: Later convert to trie or hash table for O(1) lookup
    for (size_t i = 0; i < pool->string_count; i++) {
        const char* pooled = pool->strings[i];
        // Check length first (cheap), then content
        if (strlen(pooled) == len && memcmp(pooled, str, len) == 0) {
            return pooled;
        }
    }

    return NULL;
}

const char* string_pool_intern(StringPool* pool, const char* str, size_t len) {
    assert(pool != NULL);
    assert(str != NULL);
    assert(pool->arena != NULL);

    // Check if already interned
    const char* existing = string_pool_lookup(pool, str, len);
    if (existing != NULL) {
        return existing;
    }

    // Allocate string from arena
    char* new_str = (char*)arena_alloc(pool->arena, len + 1, 1);
    memcpy(new_str, str, len);
    new_str[len] = '\0';

    // Check if we need to grow the strings array
    if (pool->string_count >= pool->string_capacity) {
        // Allocate a new, larger array from arena
        size_t new_capacity = pool->string_capacity * 2;
        const char** new_strings = (const char**)arena_alloc(pool->arena,
                                                              new_capacity * sizeof(const char*),
                                                              sizeof(void*));

        // Copy old pointers to new array
        memcpy(new_strings, pool->strings, pool->string_count * sizeof(const char*));

        pool->strings = new_strings;
        pool->string_capacity = new_capacity;
    }

    // Add to strings array
    pool->strings[pool->string_count++] = new_str;

    return new_str;
}
