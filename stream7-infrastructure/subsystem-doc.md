# Stream 7: Core Infrastructure - Subsystem Documentation

## Overview

Stream 7 provides the foundational infrastructure for the async systems language compiler. This includes memory management, string interning, error reporting, and testing utilities.

## Components

### 1. Arena Allocator (`arena.h`, `src/arena.c`)

**Purpose**: Fast, structured memory allocation for compiler phases with automatic cleanup.

**Design**:
- Segmented slab allocation with linked list of blocks
- Each block has a fixed capacity and grows automatically when exhausted
- Default block size: 64KB (configurable)
- Minimum alignment: `sizeof(void*)`
- All allocations are aligned to requested alignment (default or specified)

**Key Features**:
- **Fast allocation**: Bump pointer allocation within blocks (O(1))
- **Auto-growing**: Automatically allocates new blocks when current is full
- **Large allocations**: Handles allocations larger than block size with custom-sized blocks
- **Alignment**: Supports arbitrary power-of-2 alignments
- **Bulk deallocation**: Free entire arena at once (no individual frees)

**Memory Layout**:
```
Arena
  -> Block 1 (64KB)
       -> [allocated data...]
       -> [free space]
  -> Block 2 (64KB)
       -> [allocated data...]
       -> [free space]
  -> Block 3 (custom size for large alloc)
       -> [large allocation]
```

**Usage Pattern**:
```c
Arena arena;
arena_init(&arena, 0);  // Use default block size

void* ptr1 = arena_alloc(&arena, 100, 8);
void* ptr2 = arena_alloc(&arena, 200, 16);

arena_free(&arena);  // Free everything at once
```

**Implementation Notes**:
- Uses `malloc` for block allocation
- Aborts on out-of-memory (fatal error in compiler context)
- Each block header stores: next pointer, used size, capacity
- Data follows header in same allocation (flexible array member)

### 2. String Pool (`string_pool.h`, `src/string_pool.c`)

**Purpose**: Centralized string interning for identifiers, keywords, and string literals.

**Design**:
- Linear array of string pointers for now
- Strings allocated from provided arena
- O(n) lookup currently (simple linear search)
- Future: Convert to trie or hash table for O(1) lookup

**Key Features**:
- **Deduplication**: Same string content returns same pointer
- **Pointer equality**: Can compare strings with `==` instead of `strcmp`
- **Arena-based**: All strings allocated from caller-provided arena
- **Auto-growing**: Array grows when capacity is exceeded

**Memory Layout**:
```
StringPool
  -> Arena (for allocations)
  -> strings[] array
       -> "identifier1" (in arena)
       -> "keyword" (in arena)
       -> "another_string" (in arena)
```

**Usage Pattern**:
```c
Arena arena;
arena_init(&arena, 4096);

StringPool pool;
string_pool_init(&pool, &arena);

const char* s1 = string_pool_intern(&pool, "hello", 5);
const char* s2 = string_pool_intern(&pool, "hello", 5);

assert(s1 == s2);  // Same pointer!

arena_free(&arena);
```

**Implementation Notes**:
- Initial capacity: 256 strings
- Grows by 2x when full
- Strings stored with null terminator
- Linear search checks length first (cheap), then content
- Future optimization: Replace with trie for prefix sharing and fast lookup

### 3. Error Reporting (`error.h`, `src/error.c`)

**Purpose**: Consistent, helpful error reporting across all compiler phases.

**Design**:
- Structured error list with kind, location, message, and optional suggestion
- Errors stored in growable array (allocated from arena)
- Formatted output with colors for terminal
- Support for multiple error types (lexical, syntax, type, resolution, coroutine)

**Key Features**:
- **Source location**: Line, column, filename tracking
- **Error kinds**: Different categories for different phases
- **Formatted messages**: printf-style message formatting
- **Suggestions**: Optional fix hints for common errors
- **Colored output**: Terminal colors for better readability
- **Batch reporting**: Collect all errors, report at end

**Error Kinds**:
- `ERROR_LEXICAL`: Tokenization errors (invalid characters, etc.)
- `ERROR_SYNTAX`: Parsing errors (unexpected token, etc.)
- `ERROR_TYPE`: Type checking errors (type mismatch, etc.)
- `ERROR_RESOLUTION`: Name resolution errors (undefined identifier, etc.)
- `ERROR_COROUTINE`: Async/await transformation errors

**Output Format**:
```
Syntax Error: test.tick:10:5
  Expected semicolon after statement
  Help: Add ';' at end of line

Type Error: test.tick:15:10
  Expected type 'int' but got 'string'

2 error(s) generated.
```

**Usage Pattern**:
```c
Arena arena;
arena_init(&arena, 4096);

ErrorList errors;
error_list_init(&errors, &arena);

SourceLocation loc = {line: 10, column: 5, filename: "test.tick"};
error_list_add(&errors, ERROR_SYNTAX, loc, "Expected %s", "semicolon");

if (error_list_has_errors(&errors)) {
    error_list_print(&errors, stderr);
}

arena_free(&arena);
```

**Implementation Notes**:
- Initial capacity: 16 errors
- Grows by 2x when full
- Messages allocated from arena (printf-formatted)
- Colors: Red for errors, yellow for help, cyan for locations
- Future: Add warning support, error recovery, suggested fixes

### 4. Test Harness (`test/test_harness.h`, `test/test_harness.c`)

**Purpose**: Simple, lightweight testing framework for all compiler streams.

**Design**:
- Macro-based test framework
- Global test context for tracking results
- Colored output for pass/fail
- Minimal dependencies

**Key Macros**:
- `TEST_INIT()`: Initialize test context
- `TEST_BEGIN(name)`: Start a test
- `TEST_END()`: End a test (marks as passed)
- `ASSERT(condition)`: Assert condition is true
- `ASSERT_EQ(a, b)`: Assert equality
- `ASSERT_NE(a, b)`: Assert inequality
- `ASSERT_NULL(ptr)`: Assert pointer is NULL
- `ASSERT_NOT_NULL(ptr)`: Assert pointer is not NULL
- `ASSERT_STR_EQ(a, b)`: Assert string equality
- `TEST_SUMMARY()`: Print test results
- `TEST_EXIT()`: Return exit code (0 if all passed, 1 if any failed)

**Usage Pattern**:
```c
#include "test_harness.h"

void test_example() {
    TEST_BEGIN("example");

    int x = 42;
    ASSERT_EQ(x, 42);
    ASSERT_NE(x, 0);

    TEST_END();
}

int main() {
    TEST_INIT();

    test_example();

    TEST_SUMMARY();
    return TEST_EXIT();
}
```

**Features**:
- Automatic test counting
- Early exit on assertion failure
- File/line reporting on failures
- Color-coded output (green for pass, red for fail)
- Summary statistics

### 5. Mock Platform (`test/mock_platform.h`, `test/mock_platform.c`)

**Purpose**: Simulate async runtime for testing coroutine transformations.

**Design**:
- Mock coroutine state machine
- Mock async events
- Simple runtime for orchestrating coroutines

**Components**:
- **MockCoroutine**: Simulated coroutine with stack, state, resume point
- **MockAsyncEvent**: Simulated async event (I/O, timer, etc.)
- **MockRuntime**: Runtime for managing coroutines and events

**Coroutine States**:
- `MOCK_CORO_READY`: Ready to start
- `MOCK_CORO_RUNNING`: Currently executing
- `MOCK_CORO_SUSPENDED`: Waiting for event
- `MOCK_CORO_COMPLETED`: Finished execution

**Usage Pattern**:
```c
MockRuntime runtime;
mock_runtime_init(&runtime);

// Create a coroutine
MockCoroutine* coro = mock_coro_create(&runtime, 4096);

// Create an event
MockAsyncEvent* event = mock_event_create(&runtime);

// Simulate async operation
mock_coro_resume(coro);
mock_coro_suspend(coro);
mock_event_complete(event, 42);
mock_coro_resume(coro);

// Check result
if (mock_coro_is_done(coro)) {
    int result = mock_coro_get_result(coro);
}

mock_runtime_free(&runtime);
```

**Use Cases**:
- Testing coroutine transformation correctness
- Verifying state machine behavior
- Simulating async I/O patterns
- Testing error handling in async code

## Testing

All components have comprehensive unit tests:

### Arena Tests (`test/test_arena.c`)
- Basic allocation
- Alignment verification
- Block growth
- Large allocations
- Many small allocations
- Default block size

### String Pool Tests (`test/test_string_pool.c`)
- Basic interning
- Multiple different strings
- Lookup functionality
- Partial match rejection
- Many strings (stress test)
- Empty strings
- Long strings

### Error Tests (`test/test_error.c`)
- Basic error addition
- Multiple errors
- Formatted messages
- Different error kinds
- Source location tracking
- Array growth
- Empty list printing

### Running Tests
```bash
make test
```

All tests pass successfully:
- Arena: 6/6 tests passed
- String Pool: 7/7 tests passed
- Error: 7/7 tests passed

## Performance Characteristics

### Arena Allocator
- Allocation: O(1) amortized
- Free: O(n) where n is number of blocks (typically small)
- Memory overhead: ~24 bytes per block + alignment padding

### String Pool
- Intern: O(n) where n is number of unique strings (current)
- Lookup: O(n) where n is number of unique strings (current)
- Future: O(log n) or O(1) with trie/hash table

### Error List
- Add: O(1) amortized
- Print: O(n) where n is number of errors

## Future Enhancements

### Arena
- Add arena checkpointing (save/restore position)
- Add arena stats (total allocated, peak usage, etc.)
- Support for custom allocators

### String Pool
- Convert to trie for O(log n) lookup and prefix sharing
- Or use hash table for O(1) lookup
- Add string pool stats (unique strings, memory usage, etc.)

### Error
- Add warning support (non-fatal)
- Add error recovery hints
- Add error codes for programmatic handling
- Support for error severity levels
- Multi-line error context (show source code)

### Test Harness
- Add test fixtures (setup/teardown)
- Add test parameterization
- Add benchmark support
- Generate test reports (XML, JSON)

### Mock Platform
- Add more realistic async I/O simulation
- Add scheduler with priorities
- Add timeout simulation
- Add resource limiting

## Integration with Other Streams

Other compiler streams depend on this infrastructure:

- **Stream 1 (Lexer)**: Uses Arena, StringPool, ErrorList
- **Stream 2 (Parser)**: Uses Arena, StringPool, ErrorList
- **Stream 3 (Type Checker)**: Uses Arena, ErrorList
- **Stream 4 (Coroutine Transform)**: Uses Arena, ErrorList, MockPlatform for testing
- **Stream 5 (Lowering)**: Uses Arena, ErrorList
- **Stream 6 (Codegen)**: Uses Arena, ErrorList

All streams use the test harness for their unit tests.
