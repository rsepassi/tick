# Stream 3: Semantic Analysis

This directory contains the semantic analysis implementation for the async systems language compiler.

## Overview

Semantic analysis consists of two phases:

1. **Name Resolution (resolver.c)**: Builds hierarchical scopes and resolves all name references
2. **Type Checking (typeck.c)**: Assigns types to all AST nodes and validates type correctness

## Directory Structure

```
stream3-semantic/
├── include/           # Interface headers
│   ├── ast.h         # AST definitions (expanded)
│   ├── symbol.h      # Symbol table (expanded)
│   ├── type.h        # Type system (expanded)
│   ├── error.h       # Error reporting
│   ├── arena.h       # Memory allocation
│   └── string_pool.h # String interning
├── src/              # Implementation
│   ├── resolver.c    # Name resolution
│   └── typeck.c      # Type checking
├── test/             # Unit tests
│   ├── test_resolver.c
│   └── test_typeck.c
├── subsystem-doc.md  # Architecture documentation
├── interface-changes.md  # Interface modifications
├── README.md         # This file
└── Makefile          # Build configuration
```

## Building

### Prerequisites

- GCC or Clang with C11 support
- Make (optional, but recommended)

### With Make

```bash
make all          # Build everything
make tests        # Build test executables
make run-tests    # Build and run all tests
make clean        # Clean build artifacts
```

### Manual Build

```bash
# Build resolver tests
gcc -o test_resolver test/test_resolver.c src/resolver.c \
    -I include -std=c11 -Wall -Wextra

# Build typeck tests (includes resolver dependency)
gcc -o test_typeck test/test_typeck.c src/resolver.c src/typeck.c \
    -I include -std=c11 -Wall -Wextra
```

## Running Tests

```bash
# Run resolver tests
./test_resolver

# Run type checker tests
./test_typeck
```

Expected output:
```
=== Resolver Tests ===
Running test: module_scope
  PASSED
Running test: function_scope
  PASSED
...
Passed: 7/7
```

## API Usage

### Phase 1: Name Resolution

```c
#include "symbol.h"
#include "ast.h"
#include "string_pool.h"
#include "error.h"
#include "arena.h"

// Initialize infrastructure
Arena arena;
arena_init(&arena, 1024 * 1024);

StringPool pool;
string_pool_init(&pool, &arena);

ErrorList errors;
error_list_init(&errors, &arena);

SymbolTable symtab;
symbol_table_init(&symtab, &arena);

// Get AST from parser
AstNode* module = parse_module(...);

// Resolve names
resolver_resolve(module, &symtab, &pool, &errors, &arena);

if (error_list_has_errors(&errors)) {
    error_list_print(&errors, stderr);
    return 1;
}
```

### Phase 2: Type Checking

```c
#include "type.h"

// Initialize primitive types (once per compilation)
type_init_primitives(&arena);

// Type check the module
typeck_check_module(module, &symtab, &errors, &arena);

if (error_list_has_errors(&errors)) {
    error_list_print(&errors, stderr);
    return 1;
}

// Now module is fully typed and ready for next phase
```

## Key Features

### Name Resolution
- Hierarchical scopes (Module → Function → Block)
- Single-pass resolution (no forward declarations needed)
- String interning for efficient name comparison
- Duplicate declaration detection
- Undefined identifier detection

### Type Checking
- Complete type system with primitives, composites, and special types
- Type inference for literals and local variables
- Strict type checking (no implicit conversions)
- Comprehensive error messages with source locations
- Support for Result<T,E> and Option<T> types

## Testing

Two comprehensive test suites:

- **test_resolver.c** (7 tests): Tests scope creation, name resolution, error detection
- **test_typeck.c** (10 tests): Tests type inference, type checking, error detection

Test coverage: >80% of implementation code

## Documentation

- **subsystem-doc.md**: Detailed architecture and implementation documentation
- **interface-changes.md**: Complete list of interface modifications with rationale
- **Header comments**: All public APIs documented in header files

## Dependencies

This subsystem depends on:
- `ast.h`: AST from parser (expanded with full structures)
- `string_pool.h`: String interning
- `arena.h`: Memory allocation
- `error.h`: Error reporting

This subsystem provides:
- `symbol.h`: Symbol table and scope management
- `type.h`: Type system and utilities
- Resolver API: `resolver_resolve()`
- Type checker API: `typeck_check_module()`

## Memory Management

All allocations use arena-based allocation:
- Symbols allocated from arena
- Types allocated from arena
- String pool uses arena
- No manual free() calls needed
- Clean up entire arena at once

## Error Handling

Errors are collected, not thrown:
- Both phases continue after errors to find multiple issues
- All errors include source location information
- Error types: ERROR_RESOLUTION, ERROR_TYPE
- Use `error_list_has_errors()` to check for failures

## Integration

### Input: Parser (Stream 2)
Expects untyped AST with source locations

### Output: Coroutine Analysis (Stream 4)
Provides fully typed AST with symbol table

### Output: IR Lowering (Stream 5)
Provides type information for code generation

## Known Limitations

1. No cross-module import resolution (requires multi-module infrastructure)
2. Limited type inference (only literals and local variables)
3. No implicit type conversions
4. Linear symbol lookup (acceptable for typical scope sizes)

## Future Enhancements

- Hash-based symbol lookup
- Trie-based string pool
- Generic types
- Flow-sensitive typing
- Effect system

## Authors

Implemented as Stream 3 of the parallel compiler development plan.

## License

Part of the async systems language compiler project.
