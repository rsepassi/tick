# Stream 7: Core Infrastructure

Foundation layer for the async systems language compiler, providing memory management, string interning, error reporting, and testing utilities.

## Quick Start

### Building and Testing

```bash
# Build all tests
make

# Run all tests
make test

# Clean
make clean
```

All 20 tests should pass:
- Arena: 6/6 tests
- String Pool: 7/7 tests
- Error: 7/7 tests

## Components

### 1. Arena Allocator
Fast bump-pointer allocation with automatic growth.

```c
#include "arena.h"

Arena arena;
arena_init(&arena, 0);  // Use default 64KB blocks

void* ptr = arena_alloc(&arena, 100, 8);  // 100 bytes, 8-byte aligned

arena_free(&arena);  // Free everything at once
```

### 2. String Pool
String interning for identifiers and literals.

```c
#include "string_pool.h"
#include "arena.h"

Arena arena;
arena_init(&arena, 4096);

StringPool pool;
string_pool_init(&pool, &arena);

const char* s1 = string_pool_intern(&pool, "hello", 5);
const char* s2 = string_pool_intern(&pool, "hello", 5);

assert(s1 == s2);  // Same pointer!

arena_free(&arena);
```

### 3. Error Reporting
Structured error collection and reporting.

```c
#include "error.h"
#include "arena.h"

Arena arena;
arena_init(&arena, 4096);

ErrorList errors;
error_list_init(&errors, &arena);

SourceLocation loc = {.line = 10, .column = 5, .filename = "test.tick"};
error_list_add(&errors, ERROR_SYNTAX, loc, "Expected semicolon");

if (error_list_has_errors(&errors)) {
    error_list_print(&errors, stderr);
}

arena_free(&arena);
```

### 4. Test Harness
Simple testing framework for unit tests.

```c
#include "test/test_harness.h"

void test_example() {
    TEST_BEGIN("example");

    int x = 42;
    ASSERT_EQ(x, 42);

    TEST_END();
}

int main() {
    TEST_INIT();
    test_example();
    TEST_SUMMARY();
    return TEST_EXIT();
}
```

### 5. Mock Platform
Async runtime simulation for testing.

```c
#include "test/mock_platform.h"

MockRuntime runtime;
mock_runtime_init(&runtime);

MockCoroutine* coro = mock_coro_create(&runtime, 4096);
mock_coro_resume(coro);

mock_runtime_free(&runtime);
```

## Files

### Headers (Interfaces)
- `arena.h` - Arena allocator interface
- `string_pool.h` - String pool interface
- `error.h` - Error reporting interface
- `test/test_harness.h` - Testing macros
- `test/mock_platform.h` - Mock async runtime

### Implementation
- `src/arena.c` - Arena implementation
- `src/string_pool.c` - String pool implementation
- `src/error.c` - Error reporting implementation
- `test/test_harness.c` - Test context
- `test/mock_platform.c` - Mock runtime implementation

### Tests
- `test/test_arena.c` - Arena tests
- `test/test_string_pool.c` - String pool tests
- `test/test_error.c` - Error tests

### Documentation
- `README.md` - This file
- `subsystem-doc.md` - Detailed component documentation
- `interface-changes.md` - Interface change log

### Build
- `Makefile` - Build configuration

## Design Principles

1. **Arena-based allocation**: All memory managed through arena for fast allocation and bulk cleanup
2. **Zero dependencies**: Core infrastructure has no external dependencies
3. **Simple and fast**: Optimized for compiler workloads (many small allocations, phase-based cleanup)
4. **Helpful errors**: Clear, colored error messages with source locations
5. **Easy testing**: Lightweight test framework with minimal boilerplate

## Usage in Other Streams

All other compiler streams depend on this infrastructure:

- **Stream 1 (Lexer)**: Arena + StringPool + ErrorList
- **Stream 2 (Parser)**: Arena + StringPool + ErrorList
- **Stream 3 (Type Checker)**: Arena + ErrorList
- **Stream 4 (Coroutine)**: Arena + ErrorList + MockPlatform
- **Stream 5 (Lowering)**: Arena + ErrorList
- **Stream 6 (Codegen)**: Arena + ErrorList

## Key Invariants

### Arena
- All allocations succeed or abort (no NULL returns)
- Alignments must be power of 2
- Arena can only be freed all at once (no individual frees)
- Block size can be 0 (uses default 64KB)

### String Pool
- Same string content always returns same pointer
- Strings are null-terminated
- String pool must be initialized with an arena
- Strings live as long as the arena

### Error List
- Error list must be initialized with an arena
- Messages are formatted using printf-style format strings
- Errors are collected, not immediately reported
- Call error_list_print() to output errors

## Performance

- **Arena allocation**: O(1) amortized, ~100-1000x faster than malloc
- **String interning**: O(n) currently (linear search), future O(log n) or O(1) with trie/hash
- **Error reporting**: O(1) per error, O(n) to print all

## Future Work

See `subsystem-doc.md` for planned enhancements:
- String pool trie/hash table
- Arena checkpointing
- Warning support in error reporting
- More realistic mock platform
- Performance benchmarks

## Testing

Run tests with:
```bash
make test
```

Expected output:
```
==========================================
Running Arena Tests
==========================================
[PASS] arena_basic_alloc
[PASS] arena_alignment
[PASS] arena_block_growth
[PASS] arena_large_alloc
[PASS] arena_many_small
[PASS] arena_default_size
Tests run: 6
Passed: 6
Failed: 0

... (similar for string_pool and error tests)

All Tests Complete!
==========================================
```

## License

Part of the async systems language compiler project.
