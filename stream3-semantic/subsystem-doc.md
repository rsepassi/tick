# Stream 3: Semantic Analysis - Subsystem Documentation

## Overview

This subsystem implements semantic analysis for the async systems language compiler. It consists of two main phases: **name resolution** (resolver) and **type checking** (typeck). The semantic analysis phase transforms an untyped AST from the parser into a fully typed and resolved AST ready for coroutine analysis and code generation.

## Architecture

### Phase 1: Name Resolution (resolver.c)

The resolver builds hierarchical symbol tables and resolves all name references in the AST.

**Key Responsibilities:**
- Build hierarchical scopes (Module → Function → Block)
- Create symbol table entries for all declarations
- Resolve identifier references to symbols
- Detect duplicate declarations
- Intern all names in the string pool

**Scope Hierarchy:**
```
Module Scope
  ├── Function1 Scope
  │   ├── Parameters
  │   └── Block Scope
  │       ├── Local variables
  │       └── Nested Block Scopes
  └── Function2 Scope
      └── ...
```

**Design Decisions:**
1. **Single-pass resolution**: No forward declarations needed. Declarations are resolved in order.
2. **String interning**: All identifiers are interned in a string pool for efficient comparison (pointer equality).
3. **Symbol ownership**: Each scope owns its symbols; symbols are never shared between scopes.
4. **Error recovery**: Resolution continues after errors to find as many issues as possible.

### Phase 2: Type Checking (typeck.c)

The type checker assigns types to all AST nodes and verifies type correctness.

**Key Responsibilities:**
- Resolve type nodes to concrete types
- Assign types to all expressions
- Check type compatibility in assignments and operations
- Perform type inference for literals and local variables
- Validate function return types
- Check struct/enum/union declarations

**Type System Features:**
1. **Primitive types**: i8, i16, i32, i64, isize, u8, u16, u32, u64, usize, bool, void
2. **Composite types**: Arrays, slices, pointers, structs, enums, unions
3. **Special types**: Result<T,E>, Option<T>
4. **Function types**: First-class function pointers
5. **Type inference**: Local variables can omit type annotations if initializer is present

**Design Decisions:**
1. **Two-pass checking**: Type declarations checked first, then function bodies
2. **Primitive singletons**: Primitive types use singleton instances for efficiency
3. **Arena allocation**: All types allocated from arena (no manual freeing needed)
4. **Strict typing**: No implicit conversions; explicit casts required
5. **Structural equality**: Types compared by structure, not name (except for named types)

## Symbol Table Design

### Symbol Structure

Each symbol represents a named entity:

```c
typedef struct Symbol {
    SymbolKind kind;           // FUNCTION, TYPE, VARIABLE, CONSTANT, MODULE, PARAM
    const char* name;          // Pointer into string pool
    Type* type;                // Resolved type
    bool is_public;            // Visibility
    SourceLocation defined_at; // For error messages

    union {
        struct { AstNode* ast_node; bool is_coroutine; bool is_async; } function;
        struct { AstNode* ast_node; } type_decl;
        struct { AstNode* ast_node; bool is_mutable; bool is_volatile; } variable;
        struct { AstNode* ast_node; } constant;
        struct { const char* path; AstNode* ast_node; } module;
        struct { AstNode* ast_node; size_t param_index; } param;
    } data;
} Symbol;
```

### Scope Structure

Scopes form a hierarchy with parent pointers:

```c
typedef struct Scope {
    ScopeKind kind;            // MODULE, FUNCTION, BLOCK
    Scope* parent;             // NULL for module scope
    Symbol** symbols;          // Dynamic array of symbols
    size_t symbol_count;
    size_t symbol_capacity;
    const char* name;          // For debugging
    AstNode* owner_node;       // AST node that owns this scope
} Scope;
```

### Symbol Table

The symbol table tracks all scopes in a module:

```c
typedef struct SymbolTable {
    Scope* module_scope;       // Root scope
    Scope** all_scopes;        // All scopes for cleanup
    size_t scope_count;
    size_t scope_capacity;
    Arena* arena;              // Memory allocation
} SymbolTable;
```

### Lookup Algorithm

Name lookup follows the scope chain:

1. Start at current scope
2. Search local symbols (linear search)
3. If not found, move to parent scope
4. Repeat until found or reach module scope
5. If not found, report error

**Complexity**: O(n) where n is number of symbols in scope chain. For typical programs with small scopes, this is acceptable. Future optimization: hash tables.

## Type System Design

### Type Representation

All types share a common structure:

```c
typedef struct Type {
    TypeKind kind;
    size_t size;        // In bytes
    size_t alignment;

    union {
        struct { Type* elem_type; size_t length; } array;
        struct { Type* elem_type; } slice;
        struct { Type* pointee_type; } pointer;
        struct { const char* name; StructField* fields; size_t field_count;
                 bool is_packed; size_t alignment_override; } struct_type;
        struct { const char* name; Type* underlying_type; EnumVariant* variants;
                 size_t variant_count; } enum_type;
        struct { const char* name; UnionVariant* variants; size_t variant_count;
                 Type* tag_type; } union_type;
        struct { FunctionParam* params; size_t param_count; Type* return_type;
                 bool is_variadic; } function;
        struct { Type* value_type; Type* error_type; } result;
        struct { Type* inner_type; } option;
    } data;
} Type;
```

### Type Builders

Types are constructed via builder functions:

```c
Type* type_new_primitive(TypeKind kind, Arena* arena);
Type* type_new_array(Type* elem, size_t length, Arena* arena);
Type* type_new_slice(Type* elem, Arena* arena);
Type* type_new_pointer(Type* pointee, Arena* arena);
Type* type_new_struct(const char* name, Arena* arena);
Type* type_new_enum(const char* name, Type* underlying, Arena* arena);
Type* type_new_union(const char* name, Type* tag_type, Arena* arena);
Type* type_new_function(Type* return_type, Arena* arena);
Type* type_new_result(Type* value_type, Type* error_type, Arena* arena);
Type* type_new_option(Type* inner_type, Arena* arena);
```

### Type Equality

Type equality is structural for most types:

- **Primitives**: Compare kind
- **Pointers/Arrays/Slices**: Compare element type recursively
- **Structs/Enums/Unions**: Compare by name (nominal typing)
- **Functions**: Compare parameter types and return type
- **Result/Option**: Compare inner types recursively

### Type Inference

Type inference is performed in these cases:

1. **Integer literals**: Default to `i64`
2. **Boolean literals**: `bool`
3. **String literals**: `[]u8` (slice of u8)
4. **Local variables**: Infer from initializer if no explicit type

**Example:**
```
let x = 42;        // Inferred as i64
let y: i32 = 42;   // Explicit i32
```

## Testing Strategy

### Unit Tests

Two test suites ensure correctness:

1. **test_resolver.c**: Tests name resolution
   - Module scope creation
   - Function scope nesting
   - Name resolution across scopes
   - Duplicate declaration detection
   - Undefined identifier detection
   - String interning

2. **test_typeck.c**: Tests type checking
   - Literal type inference
   - Binary expression type checking
   - Type mismatch detection
   - Local variable type inference
   - Explicit type annotation checking
   - Struct type creation
   - Return type checking

### Test Approach

**Mock ASTs**: Tests create minimal AST structures manually, avoiding parser dependency.

**Error Testing**: Many tests verify that type errors are correctly detected (negative tests).

**Coverage**: Tests cover common cases and edge cases, ensuring >80% code coverage.

### Running Tests

```bash
# Compile resolver tests
gcc -o test_resolver test/test_resolver.c src/resolver.c -I include -std=c11

# Compile typeck tests
gcc -o test_typeck test/test_typeck.c src/resolver.c src/typeck.c -I include -std=c11

# Run tests
./test_resolver
./test_typeck
```

## Interface Dependencies

### What We Depend On

- **ast.h**: AST node definitions (expanded with full structures)
- **string_pool.h**: String interning
- **arena.h**: Memory allocation
- **error.h**: Error reporting

### What We Provide

- **symbol.h**: Symbol table and scope management
- **type.h**: Type system and type checking utilities
- **resolver API**: `resolver_resolve()` function
- **typeck API**: `typeck_check_module()` function

## Memory Management

**Arena Allocation**: All symbols, scopes, and types are allocated from arenas.

**Lifetime**: Symbols and types live until the arena is freed (at end of compilation).

**No Leaks**: Arena-based allocation eliminates individual free() calls and prevents leaks.

**String Pool**: Interned strings live in arena-allocated buffers.

## Error Handling

**Error Collection**: Errors are collected in an `ErrorList`, not thrown.

**Continue on Error**: Both resolver and typeck continue after errors to find multiple issues.

**Source Locations**: All errors include source location (file, line, column).

**Error Types**:
- `ERROR_RESOLUTION`: Undefined names, duplicate declarations, import errors
- `ERROR_TYPE`: Type mismatches, invalid operations, return type errors

**Example Error Messages:**
```
test.tick:10:5: error: Undefined identifier: undefined_var
test.tick:15:12: error: Type mismatch: cannot assign i64 to bool
test.tick:20:1: error: Duplicate function declaration: main
```

## Performance Considerations

1. **String Interning**: O(n) lookup, but typical identifier count is small. Future: trie-based pool.

2. **Symbol Lookup**: O(n) in scope chain. Acceptable for typical scope sizes. Future: hash tables.

3. **Type Equality**: Structural comparison can be O(n) for nested types. Cached results could improve this.

4. **Arena Allocation**: Very fast (bump pointer allocation), no fragmentation.

5. **Single Pass**: Both resolver and typeck are single-pass over the AST.

## Future Enhancements

1. **Hash-based symbol lookup**: Replace linear search with hash table
2. **Trie-based string pool**: Faster interning for large identifier sets
3. **Type caching**: Cache type equality results
4. **Generic types**: Add support for parametric polymorphism
5. **Flow-sensitive typing**: Track type refinements in control flow
6. **Effect system**: Track side effects and async operations

## Integration with Other Phases

### Input: Parser (Stream 2)

Expects an untyped AST with:
- All syntax validated
- Source locations attached
- Basic structure correct

### Output: Coroutine Analysis (Stream 4)

Provides:
- Fully typed AST
- Symbol table with all declarations
- Type information for all expressions
- Function metadata (is_async, is_coroutine flags)

### Output: IR Lowering (Stream 5)

Provides:
- Type-checked AST
- Resolved symbols for code generation
- Type sizes and alignments for layout

## Key Invariants

1. **After resolution**: All identifier references point to valid symbols
2. **After typeck**: All expression nodes have non-NULL type fields
3. **Type soundness**: If typeck succeeds, all operations are type-safe
4. **Name uniqueness**: No duplicate symbols in any scope
5. **Scope integrity**: All scopes form a valid tree with module scope at root

## Known Limitations

1. **No cross-module resolution**: Import declarations are noted but not fully resolved (requires multi-module compiler infrastructure)
2. **Limited type inference**: Only for literals and local variables with initializers
3. **No implicit conversions**: All type conversions must be explicit casts
4. **Linear symbol lookup**: May be slow for very large scopes (unlikely in practice)
5. **No partial type checking**: Entire module must be checked together

## Example Usage

```c
// Initialize infrastructure
Arena arena;
arena_init(&arena, 1024 * 1024);

StringPool pool;
string_pool_init(&pool, &arena);

ErrorList errors;
error_list_init(&errors, &arena);

SymbolTable symtab;
symbol_table_init(&symtab, &arena);

// Parse AST (from parser)
AstNode* module = parse_module(...);

// Phase 1: Name resolution
resolver_resolve(module, &symtab, &pool, &errors, &arena);

if (error_list_has_errors(&errors)) {
    error_list_print(&errors, stderr);
    return 1;
}

// Phase 2: Type checking
typeck_check_module(module, &symtab, &errors, &arena);

if (error_list_has_errors(&errors)) {
    error_list_print(&errors, stderr);
    return 1;
}

// Now module is fully typed and ready for next phase
// ...

// Cleanup
arena_free(&arena);
```

## Conclusion

The semantic analysis subsystem provides robust name resolution and type checking for the async systems language. It uses hierarchical scopes, arena-based allocation, and strict type checking to ensure correctness. The design is simple, testable, and integrates cleanly with other compiler phases.
