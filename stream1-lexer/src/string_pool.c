#include "string_pool.h"
#include "arena.h"
#include <string.h>

void string_pool_init(StringPool* pool, Arena* arena) {
    pool->arena = arena;
    pool->strings = NULL;
    pool->string_count = 0;
    pool->string_capacity = 256;

    // Allocate initial strings array
    pool->strings = (const char**)arena_alloc(arena, sizeof(char*) * pool->string_capacity, sizeof(char*));
}

const char* string_pool_intern(StringPool* pool, const char* str, size_t len) {
    // First check if string already exists
    const char* existing = string_pool_lookup(pool, str, len);
    if (existing != NULL) {
        return existing;
    }

    // Need to add new string
    // Check if we need to grow the strings array
    if (pool->string_count >= pool->string_capacity) {
        // For simplicity, just fail - in real implementation would grow
        return NULL;
    }

    // Allocate string from arena
    char* new_str = (char*)arena_alloc(pool->arena, len + 1, 1);
    memcpy(new_str, str, len);
    new_str[len] = '\0';

    // Add to strings array
    pool->strings[pool->string_count++] = new_str;

    return new_str;
}

const char* string_pool_lookup(StringPool* pool, const char* str, size_t len) {
    for (size_t i = 0; i < pool->string_count; i++) {
        const char* pooled = pool->strings[i];
        size_t pooled_len = strlen(pooled);
        if (pooled_len == len && memcmp(pooled, str, len) == 0) {
            return pooled;
        }
    }
    return NULL;
}
