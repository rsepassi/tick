# Stream 2: Parser

LALR(1) parser for the async systems language compiler.

## Overview

This module implements the parser phase of the compiler pipeline. It consumes tokens from the Lexer (Stream 1) and produces an Abstract Syntax Tree (AST) for semantic analysis (Streams 3-4).

## Directory Structure

```
stream2-parser/
├── README.md                 # This file
├── Makefile                  # Build system
├── subsystem-doc.md          # Detailed implementation documentation
├── interface-changes.md      # Documentation of all interface changes
│
├── Interface Headers (owned and modified)
├── parser.h                  # Parser interface
├── ast.h                     # AST node definitions
│
├── Interface Headers (dependencies, unmodified)
├── lexer.h                   # Token stream input
├── arena.h                   # Memory allocation
├── error.h                   # Error reporting
│
├── Grammar
├── grammar.y                 # Lemon grammar specification
│
├── src/
│   └── parser.c              # Parser implementation & Lemon integration
│
└── test/
    └── test_parser.c         # Comprehensive unit tests
```

## Building

### Prerequisites

- GCC or compatible C compiler
- Lemon parser generator
- Make

### Build Commands

```bash
# Generate parser from grammar
make

# Build and run tests
make test

# Clean build artifacts
make clean

# Show help
make help
```

## Key Features

### Grammar

- **LALR(1)** parser using Lemon parser generator
- **Uniform `let` syntax** for all declarations
- **Go-style for loops** (no parens, braces required)
- **Switch statements** with no fallthrough
- **Computed goto pattern** (while switch, continue switch)
- **Operator precedence** with ambiguity enforcement

### AST Design

- **Arena allocation** - fast, bulk deallocation
- **Complete node types** - all language constructs represented
- **Source locations** - precise error reporting
- **Type annotations** - prepared for type checking phase

### Language Constructs Supported

- Module structure with imports
- Function declarations (sync and async)
- Type declarations (struct, enum, union)
- Control flow (if, for, switch, while switch)
- Error handling (try/catch, defer, errdefer)
- Async operations (async, suspend, resume)
- All expression types with proper precedence

## Usage Example

```c
#include "parser.h"
#include "lexer.h"
#include "arena.h"
#include "error.h"

// Setup
Arena arena;
arena_init(&arena, 65536);

ErrorList errors;
error_list_init(&errors, &arena);

Lexer lexer;
lexer_init(&lexer, source, strlen(source), "example.tick");

Parser parser;
parser_init(&parser, &lexer, &arena, &errors);

// Parse
AstNode* ast = parser_parse(&parser);

if (ast && !error_list_has_errors(&errors)) {
    // Success - AST ready for next phase
} else {
    error_list_print(&errors, stderr);
}

// Cleanup
parser_cleanup(&parser);
arena_free(&arena);
```

## Testing

The test suite covers:

- All declaration types
- All statement types
- All expression types with precedence
- Error handling constructs
- Async/coroutine constructs
- Edge cases and complex nesting

Run tests with: `make test`

## Documentation

- **subsystem-doc.md** - Complete implementation documentation
  - Architecture and design decisions
  - Grammar design and AST structure
  - Memory management and performance
  - Integration with other phases

- **interface-changes.md** - All interface modifications
  - Changes to ast.h and parser.h
  - Rationale for each change
  - Impact analysis and migration guide

## Interface

### Owned Interfaces

These interfaces are owned by this stream and were expanded/modified:

- `ast.h` - AST node definitions (EXPANDED from skeleton)
- `parser.h` - Parser interface (ENHANCED with error handling)

### Dependency Interfaces

These interfaces are consumed as-is:

- `lexer.h` - Token stream from Lexer
- `arena.h` - Memory allocation
- `error.h` - Error reporting

## Next Steps

This parser produces an AST for:

- **Stream 3: Resolver** - Build symbol tables, resolve imports
- **Stream 4: Type Checker** - Annotate AST with types
- **Stream 5: Lowering** - Transform AST to IR

## Performance

- **Time**: O(n) in number of tokens
- **Space**: O(n) in number of AST nodes
- **Typical**: ~10ms for 10,000 lines, ~1-2MB memory

## Implementation Notes

### Lemon Parser Generator

The grammar is written in Lemon syntax (similar to Yacc/Bison but with different features). Lemon generates a thread-safe, reentrant parser with:

- Table-driven LALR(1) parsing
- Automatic memory management for parse stack
- Integrated error recovery

### Arena Allocation

All AST nodes are allocated from an arena allocator:

- Fast allocation (single pointer bump)
- No fragmentation
- Bulk deallocation
- Good cache locality

### Error Handling

The parser collects errors in an ErrorList:

- Multiple error reporting
- Precise source locations
- Helpful error messages
- IDE-friendly format

## Contributing

When modifying the parser:

1. Update `grammar.y` for syntax changes
2. Add corresponding AST node types in `ast.h`
3. Update `parser.c` helpers if needed
4. Add tests in `test/test_parser.c`
5. Update documentation in `subsystem-doc.md`
6. Document interface changes in `interface-changes.md`

## License

Part of the async systems language compiler project.
