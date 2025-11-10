# Stream 2: Parser - Subsystem Documentation

## Overview

The Parser module implements a complete LALR(1) parser for the async systems language using the Lemon parser generator. It consumes a stream of tokens from the Lexer and produces an Abstract Syntax Tree (AST) that represents the program structure.

## Architecture

### Components

1. **grammar.y** - Lemon grammar specification defining the language syntax
2. **src/parser.c** - Integration layer between Lemon and our compiler
3. **ast.h** - AST node definitions and data structures
4. **parser.h** - Parser interface for other compiler phases

### Parser Pipeline

```
Tokens (from Lexer) → Lemon Parser → AST Construction → Typed AST (for semantic analysis)
```

## Grammar Design

### Parser Type

- **LALR(1)** - Look-ahead LR parser with 1-token lookahead
- **Table-driven** - Lemon generates parsing tables at compile time
- **Bottom-up** - Shift-reduce parsing strategy

### Key Grammar Features

#### 1. Uniform `let` Syntax

All declarations use `let` bindings for consistency:

```
let add = fn(x: i32, y: i32) -> i32 { ... };
let Point = struct { x: i32, y: i32 };
let Status = enum(u8) { OK, ERROR };
let config = loadConfig();
```

This provides a uniform syntax across the language and simplifies parsing.

#### 2. Operator Precedence

Precedence levels (lowest to highest):

1. Assignment: `=`, `+=`, `-=`, `*=`, `/=`
2. Logical OR: `||`
3. Logical AND: `&&`
4. Bitwise OR: `|`
5. Bitwise XOR: `^`
6. Bitwise AND: `&`
7. Equality: `==`, `!=`
8. Comparison: `<`, `>`, `<=`, `>=`
9. Shift: `<<`, `>>`
10. Additive: `+`, `-`
11. Multiplicative: `*`, `/`, `%`
12. Unary: `-`, `!`, `~`, `*`, `&`
13. Postfix: `.`, `->`, `[]`, `()`

#### 3. Ambiguity Enforcement

The grammar enforces explicit parenthesization for:
- Mixed bitwise and other operators
- Mixed logical operators
- Chained comparisons

This prevents common errors and improves code clarity.

#### 4. Go-Style For Loops

For loops follow Go's syntax (no parentheses around condition, braces required):

```
for i in 0..10 { ... }              # Range iteration
for condition { ... }                # While-style
for { ... }                          # Infinite loop
for i in collection { ... }          # Collection iteration
for : tick() { ... }                 # With continue clause
```

#### 5. Switch Statements

No fallthrough by default (implicit break):

```
switch value {
    case 1, 2, 3:
        handleLow();
    case 4:
        handleMid();
    default:
        handleOther();
}
```

#### 6. Computed Goto Pattern

For VM dispatch loops:

```
while switch state {
    case INIT:
        state = PROCESS;
        continue switch state;
    case PROCESS:
        # ...
}
```

The `while switch` construct creates a loop with a switch statement, and `continue switch expr` allows updating the switch variable and jumping back to the switch.

## AST Design

### Node Structure

All AST nodes share a common header:

```c
typedef struct AstNode {
    AstNodeKind kind;        // Node type discriminator
    SourceLocation loc;       // Source location for error reporting
    Type* type;              // Type annotation (filled by typeck)
    union { ... } data;      // Node-specific data
} AstNode;
```

### Node Categories

#### Declarations
- `AST_MODULE` - Top-level module container
- `AST_IMPORT_DECL` - Import declarations
- `AST_FUNCTION_DECL` - Function declarations
- `AST_STRUCT_DECL` - Struct type declarations
- `AST_ENUM_DECL` - Enum type declarations
- `AST_UNION_DECL` - Union type declarations
- `AST_LET_DECL` - Immutable bindings
- `AST_VAR_DECL` - Mutable variables

#### Statements
- `AST_LET_STMT` - Local immutable binding
- `AST_VAR_STMT` - Local mutable variable
- `AST_ASSIGN_STMT` - Assignment with operators
- `AST_RETURN_STMT` - Return from function
- `AST_IF_STMT` - Conditional execution
- `AST_FOR_STMT` - Loop constructs
- `AST_SWITCH_STMT` - Switch/case statements
- `AST_BREAK_STMT` - Break from loop
- `AST_CONTINUE_STMT` - Continue loop iteration
- `AST_CONTINUE_SWITCH_STMT` - Computed goto pattern
- `AST_DEFER_STMT` - Deferred execution
- `AST_ERRDEFER_STMT` - Error-path deferred execution
- `AST_SUSPEND_STMT` - Coroutine suspension
- `AST_RESUME_STMT` - Coroutine resumption
- `AST_BLOCK_STMT` - Statement block
- `AST_EXPR_STMT` - Expression as statement
- `AST_TRY_CATCH_STMT` - Error handling

#### Expressions
- `AST_BINARY_EXPR` - Binary operations
- `AST_UNARY_EXPR` - Unary operations
- `AST_CALL_EXPR` - Function calls
- `AST_ASYNC_CALL_EXPR` - Async function calls
- `AST_FIELD_ACCESS_EXPR` - Struct field access (`.` and `->`)
- `AST_INDEX_EXPR` - Array indexing
- `AST_LITERAL_EXPR` - Literals (int, string, bool)
- `AST_IDENTIFIER_EXPR` - Variable references
- `AST_CAST_EXPR` - Type casts
- `AST_TRY_EXPR` - Error propagation
- `AST_STRUCT_INIT_EXPR` - Struct initialization
- `AST_ARRAY_INIT_EXPR` - Array initialization
- `AST_RANGE_EXPR` - Range expressions (`start..end`)

#### Types
- `AST_TYPE_PRIMITIVE` - Built-in types
- `AST_TYPE_ARRAY` - Fixed-size arrays
- `AST_TYPE_SLICE` - Dynamic slices
- `AST_TYPE_POINTER` - Pointers
- `AST_TYPE_FUNCTION` - Function types
- `AST_TYPE_RESULT` - Result types (Error!Value)
- `AST_TYPE_NAMED` - Named types (user-defined)

### Memory Management

All AST nodes are allocated from the parser's arena:

- Single allocation call per node
- No individual free required
- Entire AST freed at once when arena is freed
- Fast allocation (bump allocator)
- Good cache locality

## Source Location Tracking

Every AST node includes `SourceLocation`:

```c
typedef struct SourceLocation {
    uint32_t line;
    uint32_t column;
    const char* filename;
} SourceLocation;
```

This enables:
- Precise error messages
- Source mapping in generated code
- Debugging support

## Error Handling

The parser uses structured error reporting:

1. **Lexical errors** - Invalid tokens detected by lexer
2. **Syntax errors** - Parse errors detected by Lemon
3. **Semantic hints** - Suggestions for common mistakes

Errors are collected in an `ErrorList` and don't immediately terminate parsing, allowing for:
- Multiple error reporting
- Better error messages
- IDE integration

## Implementation Details

### Lemon Integration

1. **Grammar File** (`grammar.y`)
   - Token definitions
   - Precedence declarations
   - Production rules with AST construction actions

2. **Parser Integration** (`src/parser.c`)
   - Initialize Lemon parser
   - Feed tokens from lexer
   - Collect AST nodes from grammar actions
   - Handle errors

3. **AST Construction Helpers**
   - `ast_alloc()` - Allocate AST node from arena
   - `parser_error()` - Report parse error
   - List builders for collecting child nodes

### Key Design Decisions

#### Why Lemon?

- LALR(1) parsing - more powerful than recursive descent
- Table-driven - fast and predictable
- C integration - no external runtime
- Memory efficient - stack-based parsing

#### Why Arena Allocation?

- Fast allocation (single pointer bump)
- No fragmentation
- Bulk deallocation
- Predictable memory usage
- Cache-friendly layout

#### Why Explicit AST Node Types?

- Type safety at compile time
- Clear node structure
- Easy to traverse and transform
- Good IDE support
- Self-documenting code

## Testing Strategy

### Unit Tests

The test suite (`test/test_parser.c`) covers:

1. **Declaration Parsing**
   - Functions with various signatures
   - Struct declarations (packed and regular)
   - Enum declarations with explicit types
   - Union declarations (tagged and auto)
   - Import declarations

2. **Statement Parsing**
   - All statement types
   - Nested statements
   - Control flow constructs
   - Error handling constructs

3. **Expression Parsing**
   - Binary operators with precedence
   - Unary operators
   - Postfix operations
   - Complex expressions

4. **Edge Cases**
   - Empty modules
   - Nested blocks
   - Long expressions
   - Multiple declarations

### Mock Lexer

Tests use a simple mock lexer to:
- Provide controlled token streams
- Test parser in isolation
- Verify error handling

### Test Organization

```
test/
  test_parser.c      - Main test suite
  mock_lexer.c       - Mock lexer implementation (if needed)
```

## Performance Characteristics

### Time Complexity

- **Parsing**: O(n) where n is number of tokens
- **AST construction**: O(n) where n is number of nodes
- **Overall**: O(n) linear in input size

### Space Complexity

- **Parse stack**: O(d) where d is maximum nesting depth
- **AST**: O(n) where n is number of nodes
- **Arena overhead**: Minimal (~1% for typical programs)

### Expected Performance

For a 10,000 line program:
- Parse time: ~10ms
- Memory usage: ~1-2 MB
- AST nodes: ~100,000 nodes

## Integration with Other Phases

### Dependencies (Input)

1. **Lexer** (`lexer.h`)
   - Provides token stream
   - Source location information
   - Literal values

2. **Arena** (`arena.h`)
   - Memory allocation
   - Bulk deallocation

3. **Error** (`error.h`)
   - Error reporting
   - Source locations

### Dependents (Output)

1. **Resolver** (Stream 3)
   - Consumes AST
   - Builds symbol tables
   - Resolves imports

2. **Type Checker** (Stream 4)
   - Annotates AST with types
   - Validates type constraints

## Usage Example

```c
#include "parser.h"
#include "lexer.h"
#include "arena.h"
#include "error.h"

// Initialize components
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
    // AST is valid, proceed to next phase
    resolve_symbols(ast);
} else {
    // Print errors
    error_list_print(&errors, stderr);
}

// Cleanup
parser_cleanup(&parser);
arena_free(&arena);
```

## Future Enhancements

### Potential Improvements

1. **Error Recovery**
   - Continue parsing after errors
   - Synchronization points
   - Better error messages

2. **IDE Support**
   - Incremental parsing
   - Partial AST construction
   - Syntax highlighting hints

3. **Optimization**
   - Reduce memory allocations
   - Optimize common patterns
   - Cache grammar tables

4. **Tooling**
   - AST visualization
   - Grammar railroad diagrams
   - Parse tree dumper

## Conclusion

The parser module provides a robust, efficient, and maintainable foundation for the compiler. The use of Lemon for LALR(1) parsing combined with arena allocation for AST nodes results in a fast, memory-efficient parser that produces a clean AST for subsequent compilation phases.

The uniform `let` syntax, explicit precedence rules, and comprehensive AST design make the language easy to parse and analyze while maintaining clarity and type safety throughout the compilation process.
