#ifndef STRING_POOL_H
#define STRING_POOL_H

#include <stddef.h>

// String Pool Interface
// Purpose: Centralized string interning for all identifiers and names

// Forward declare Arena
typedef struct Arena Arena;

typedef struct StringPool {
    Arena* arena;        // Arena for allocations

    // TODO: Later convert to trie for faster lookups
    const char** strings;
    size_t string_count;
    size_t string_capacity;
} StringPool;

// Initialize with caller-provided memory
void string_pool_init(StringPool* pool, Arena* arena);

// Intern a string (returns pointer to pooled copy)
const char* string_pool_intern(StringPool* pool, const char* str, size_t len);

// Lookup existing string (returns NULL if not found)
const char* string_pool_lookup(StringPool* pool, const char* str, size_t len);

#endif // STRING_POOL_H
