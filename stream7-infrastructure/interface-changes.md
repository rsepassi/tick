# Stream 7: Core Infrastructure - Interface Changes

This document details all changes made to the interfaces during implementation.

## Overview

Stream 7 owns three interfaces:
- `arena.h` - Arena allocator
- `string_pool.h` - String interning
- `error.h` - Error reporting

## Changes Made

### 1. `string_pool.h` - Modified

**Original Structure**:
```c
typedef struct StringPool {
    char* buffer;        // Segmented slab allocation
    size_t used;
    size_t capacity;

    // TODO: Later convert to trie for faster lookups
    const char** strings;
    size_t string_count;
} StringPool;

// Forward declare Arena
typedef struct Arena Arena;
```

**Updated Structure**:
```c
// Forward declare Arena
typedef struct Arena Arena;

typedef struct StringPool {
    Arena* arena;        // Arena for allocations

    // TODO: Later convert to trie for faster lookups
    const char** strings;
    size_t string_count;
    size_t string_capacity;
} StringPool;
```

**Changes**:
1. **Added `Arena* arena` field**: Store reference to arena for allocations
   - **Rationale**: Need to allocate strings and grow arrays from the arena
   - **Impact**: StringPool can now use arena_alloc directly without managing its own buffer

2. **Removed `char* buffer`, `used`, `capacity` fields**: Simplified buffer management
   - **Rationale**: Arena handles all memory allocation, no need for custom buffer
   - **Impact**: Cleaner implementation, less state to manage

3. **Renamed field**: `string_count` capacity tracking field
   - **Added**: `string_capacity` to track array capacity
   - **Rationale**: Need separate capacity for strings array (was overloading old `capacity`)
   - **Impact**: Clearer distinction between count and capacity

4. **Moved forward declaration**: Placed before struct definition
   - **Rationale**: Arena* is used in the struct, so forward declaration must come first
   - **Impact**: Fixes compilation errors

**Why These Changes**:
- Simplifies implementation by delegating all allocation to Arena
- Eliminates duplicate buffer management code
- Makes string pool allocation consistent with rest of compiler
- Allows string pool to grow dynamically without realloc issues

### 2. `error.h` - Modified

**Original Structure**:
```c
typedef struct ErrorList {
    CompileError* errors;
    size_t count;
    size_t capacity;
} ErrorList;

// Forward declare Arena
typedef struct Arena Arena;
```

**Updated Structure**:
```c
// Forward declare Arena
typedef struct Arena Arena;

typedef enum {
    ERROR_LEXICAL,
    ERROR_SYNTAX,
    ERROR_TYPE,
    ERROR_RESOLUTION,
    ERROR_COROUTINE,
} ErrorKind;

// ... (other types)

typedef struct ErrorList {
    Arena* arena;
    CompileError* errors;
    size_t count;
    size_t capacity;
} ErrorList;
```

**Changes**:
1. **Added `Arena* arena` field**: Store reference to arena for allocations
   - **Rationale**: Need to allocate error messages and grow error array from arena
   - **Impact**: ErrorList can format and store messages without malloc

2. **Moved forward declaration**: Placed before all type definitions
   - **Rationale**: Arena* is used in ErrorList struct
   - **Impact**: Fixes compilation errors

**Why These Changes**:
- Enables error messages to be allocated from arena (consistent with compiler design)
- Allows error list to grow dynamically
- Eliminates need for manual memory management in error reporting
- All error data has same lifetime as compilation phase (freed with arena)

### 3. `arena.h` - No Changes

**Status**: No modifications needed

The arena interface was well-designed and complete as-is. No changes were required for implementation.

**Interface**:
```c
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

void arena_init(Arena* arena, size_t block_size);
void* arena_alloc(Arena* arena, size_t size, size_t alignment);
void arena_free(Arena* arena);
```

This interface provides everything needed:
- Flexible block sizing
- Alignment support
- Automatic growth
- Bulk deallocation

## New Interfaces Created

### 4. `test/test_harness.h` - New Interface

**Purpose**: Testing utilities for all streams

**Key Types**:
```c
typedef struct TestContext {
    int tests_run;
    int tests_passed;
    int tests_failed;
    const char* current_test;
} TestContext;
```

**Key Macros**:
- `TEST_INIT()` - Initialize test context
- `TEST_BEGIN(name)` - Start a test
- `TEST_END()` - End a test
- `ASSERT(cond)` - Assert condition
- `ASSERT_EQ(a, b)` - Assert equality
- `ASSERT_NE(a, b)` - Assert inequality
- `ASSERT_NULL(ptr)` - Assert NULL
- `ASSERT_NOT_NULL(ptr)` - Assert not NULL
- `ASSERT_STR_EQ(a, b)` - Assert string equality
- `TEST_SUMMARY()` - Print summary
- `TEST_EXIT()` - Return exit code

**Rationale**: Provide simple, lightweight testing for all compiler components without external dependencies.

### 5. `test/mock_platform.h` - New Interface

**Purpose**: Mock async runtime for testing

**Key Types**:
```c
typedef enum {
    MOCK_CORO_READY,
    MOCK_CORO_RUNNING,
    MOCK_CORO_SUSPENDED,
    MOCK_CORO_COMPLETED,
} MockCoroState;

typedef struct MockCoroutine {
    void* stack;
    size_t stack_size;
    void* resume_point;
    MockCoroState state;
    int result;
    void* user_data;
} MockCoroutine;

typedef struct MockAsyncEvent {
    uint64_t id;
    bool completed;
    int result;
} MockAsyncEvent;

typedef struct MockRuntime {
    MockCoroutine** coroutines;
    size_t coro_count;
    size_t coro_capacity;
    MockAsyncEvent** events;
    size_t event_count;
    size_t event_capacity;
    uint64_t next_event_id;
} MockRuntime;
```

**Key Functions**:
- `mock_runtime_init()` - Initialize runtime
- `mock_runtime_free()` - Free runtime
- `mock_coro_create()` - Create coroutine
- `mock_coro_suspend()` - Suspend coroutine
- `mock_coro_resume()` - Resume coroutine
- `mock_coro_is_done()` - Check if done
- `mock_coro_get_result()` - Get result
- `mock_event_create()` - Create event
- `mock_event_complete()` - Complete event
- `mock_event_is_ready()` - Check if ready
- `mock_event_get_result()` - Get result

**Rationale**: Enable testing of coroutine transformations and async code without real async runtime.

## Impact on Other Streams

### Streams That Use These Interfaces

All streams will use the updated interfaces. The changes are mostly internal improvements:

**Stream 1 (Lexer)**:
- Uses: Arena, StringPool, ErrorList
- Impact: Must pass Arena to string_pool_init() and error_list_init()
- Benefit: No manual memory management

**Stream 2 (Parser)**:
- Uses: Arena, StringPool, ErrorList
- Impact: Must pass Arena to string_pool_init() and error_list_init()
- Benefit: No manual memory management

**Stream 3 (Type Checker)**:
- Uses: Arena, ErrorList
- Impact: Must pass Arena to error_list_init()
- Benefit: No manual memory management

**Stream 4 (Coroutine Transform)**:
- Uses: Arena, ErrorList, MockPlatform (for tests)
- Impact: Must pass Arena to error_list_init()
- Benefit: Can use MockPlatform to test transformations

**Stream 5 (Lowering)**:
- Uses: Arena, ErrorList
- Impact: Must pass Arena to error_list_init()
- Benefit: No manual memory management

**Stream 6 (Codegen)**:
- Uses: Arena, ErrorList
- Impact: Must pass Arena to error_list_init()
- Benefit: No manual memory management

### Migration Guide

For streams using the old interfaces, update as follows:

**Old Code** (hypothetical):
```c
StringPool pool;
string_pool_init(&pool, NULL);  // No arena
```

**New Code**:
```c
Arena arena;
arena_init(&arena, 0);

StringPool pool;
string_pool_init(&pool, &arena);  // Pass arena

// ... use pool ...

arena_free(&arena);  // Cleans up everything
```

**Old Code** (hypothetical):
```c
ErrorList errors;
error_list_init(&errors, NULL);  // No arena
error_list_add(&errors, ...);
// Messages allocated with malloc?
```

**New Code**:
```c
Arena arena;
arena_init(&arena, 0);

ErrorList errors;
error_list_init(&errors, &arena);  // Pass arena
error_list_add(&errors, ...);  // Messages in arena

arena_free(&arena);  // Cleans up everything
```

## Rationale Summary

All changes serve to:

1. **Consistency**: All allocations go through arena
2. **Simplicity**: Less manual memory management
3. **Safety**: Bulk cleanup prevents leaks
4. **Performance**: Arena allocation is faster than malloc
5. **Clarity**: Explicit arena dependency makes ownership clear

The original interfaces were well-designed. These changes are refinements based on implementation experience, not fundamental redesigns.

## Testing

All changes are validated by comprehensive unit tests:
- 6 arena tests (all passing)
- 7 string pool tests (all passing)
- 7 error tests (all passing)

The test harness and mock platform are also tested indirectly by using them in the unit tests.
