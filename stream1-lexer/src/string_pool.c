#include "../include/string_pool.h"
#include "../include/arena.h"
#include <string.h>

void string_pool_init(StringPool* pool, Arena* arena) {
    pool->buffer = NULL;
    pool->used = 0;
    pool->capacity = 0;
    pool->strings = NULL;
    pool->string_count = 0;

    // Allocate initial buffer from arena
    pool->capacity = 4096;
    pool->buffer = (char*)arena_alloc(arena, pool->capacity, 1);

    // Allocate initial strings array
    pool->strings = (const char**)arena_alloc(arena, sizeof(char*) * 256, sizeof(char*));
}

const char* string_pool_intern(StringPool* pool, const char* str, size_t len) {
    // First check if string already exists
    const char* existing = string_pool_lookup(pool, str, len);
    if (existing != NULL) {
        return existing;
    }

    // Need to add new string
    // Make sure we have space (including null terminator)
    if (pool->used + len + 1 > pool->capacity) {
        // For simplicity, just fail - in real implementation would grow
        return NULL;
    }

    // Copy string into buffer
    char* new_str = &pool->buffer[pool->used];
    memcpy(new_str, str, len);
    new_str[len] = '\0';
    pool->used += len + 1;

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
