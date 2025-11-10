# Stream 1: Lexer

This directory contains the lexer implementation for the async systems language compiler.

## Quick Start

Build and run tests:
```bash
make test
```

Clean build artifacts:
```bash
make clean
```

## Directory Structure

```
stream1-lexer/
├── include/          # Interface headers
│   ├── lexer.h      # Lexer interface (modified)
│   ├── error.h      # Error reporting interface
│   ├── string_pool.h # String interning interface
│   └── arena.h      # Arena allocator interface
├── src/             # Implementation
│   ├── lexer.c      # Lexer implementation (1050+ lines)
│   ├── arena.c      # Arena stub implementation
│   ├── string_pool.c # String pool stub implementation
│   └── error.c      # Error list stub implementation
├── test/            # Unit tests
│   └── test_lexer.c # Comprehensive test suite (730+ lines, 33 tests)
├── build/           # Build artifacts (generated)
├── Makefile         # Build system
├── subsystem-doc.md # Detailed subsystem documentation
├── interface-changes.md # Interface changes documentation
└── README.md        # This file
```

## Features

✅ **Complete Token Support**
- All keywords (async, suspend, resume, try, catch, defer, etc.)
- All operators (arithmetic, bitwise, logical, comparison, assignment)
- Integer literals (decimal, hex, octal, binary with underscores)
- String literals with escape sequences
- Boolean literals
- Comments (line and block, nested)

✅ **Robust Error Handling**
- Accurate line/column tracking
- Clear error messages
- Integration with error list interface
- Graceful error recovery

✅ **String Interning**
- All identifiers interned via string pool
- Fast identifier comparison (pointer equality)
- Memory efficient

✅ **Comprehensive Tests**
- 33 unit tests covering all features
- Edge case testing
- Integration tests
- Error condition tests

## Usage Example

```c
#include "include/lexer.h"
#include "include/string_pool.h"
#include "include/error.h"
#include "include/arena.h"

int main() {
    // Setup
    Arena arena;
    StringPool string_pool;
    ErrorList error_list;
    Lexer lexer;

    arena_init(&arena, 4096);
    string_pool_init(&string_pool, &arena);
    error_list_init(&error_list, &arena);

    const char* source = "fn main() { return 42; }";
    lexer_init(&lexer, source, strlen(source), "main.txt",
               &string_pool, &error_list);

    // Tokenize
    Token token;
    while ((token = lexer_next_token(&lexer)).kind != TOKEN_EOF) {
        printf("Token: kind=%d, text='%.*s'\n",
               token.kind, (int)token.length, token.start);
    }

    // Cleanup
    arena_free(&arena);
    return 0;
}
```

## Documentation

- **[subsystem-doc.md](subsystem-doc.md)** - Detailed design and implementation documentation
- **[interface-changes.md](interface-changes.md)** - Complete list of interface changes with rationale

## Test Results

All 33 tests passing:
```
Running Lexer Tests
===================

Running test_single_char_tokens... PASSED
Running test_operators... PASSED
Running test_comparison_operators... PASSED
Running test_assignment_operators... PASSED
Running test_arrow_and_dots... PASSED
Running test_keywords_async_flow... PASSED
Running test_keywords_error_handling... PASSED
Running test_keywords_declarations... PASSED
Running test_keywords_control_flow... PASSED
Running test_keywords_other... PASSED
Running test_bool_literals... PASSED
Running test_identifiers... PASSED
Running test_identifier_vs_keyword... PASSED
Running test_decimal_numbers... PASSED
Running test_numbers_with_underscores... PASSED
Running test_hexadecimal_numbers... PASSED
Running test_hex_with_underscores... PASSED
Running test_octal_numbers... PASSED
Running test_binary_numbers... PASSED
Running test_binary_with_underscores... PASSED
Running test_simple_strings... PASSED
Running test_empty_string... PASSED
Running test_strings_with_escapes... PASSED
Running test_string_with_backslash... PASSED
Running test_line_comments... PASSED
Running test_block_comments... PASSED
Running test_nested_block_comments... PASSED
Running test_unterminated_string... PASSED
Running test_unterminated_block_comment... PASSED
Running test_invalid_hex_number... PASSED
Running test_line_and_column_tracking... PASSED
Running test_simple_function... PASSED
Running test_async_function... PASSED

===================
All tests passed!
```

## Interface Changes

Key changes to `lexer.h`:
1. Added `StringPool*` and `ErrorList*` dependencies to `Lexer` struct
2. Added tracking fields: `token_start`, `token_line`, `token_column`
3. Added comment token types: `TOKEN_LINE_COMMENT`, `TOKEN_BLOCK_COMMENT`
4. Updated `lexer_init()` to accept string pool and error list parameters

See [interface-changes.md](interface-changes.md) for complete details.

## Dependencies

- **string_pool.h** - String interning for identifiers
- **error.h** - Error reporting
- **arena.h** - Memory allocation (transitive via string_pool and error)

## Status

✅ **Complete and tested**
- All required features implemented
- All tests passing
- Documentation complete
- Ready for integration with parser (Stream 2)

## Contact

Part of the async systems language compiler parallel implementation.
See `/home/user/tick/doc/parallel-impl-plan.md` for overall project plan.
