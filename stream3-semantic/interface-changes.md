# Interface Changes Documentation

## Overview

This document lists all changes made to interfaces during the implementation of Stream 3: Semantic Analysis. Each change includes the rationale and impact on other subsystems.

## Summary of Changes

1. **ast.h**: Expanded with complete AST node structures
2. **symbol.h**: Expanded with complete symbol and scope structures
3. **type.h**: Expanded with complete type system
4. **error.h**: No changes (used as-is)
5. **arena.h**: No changes (used as-is)
6. **string_pool.h**: No changes (used as-is)

---

## 1. ast.h Changes

### Added: Complete AST Node Structures

**Original**: Stub structures with comments like `/* module fields */`

**Changed To**: Fully defined structures for all AST node types

**Rationale**: The resolver and type checker need to access AST node fields to perform semantic analysis. Stub structures are insufficient for implementation.

**Specific Additions:**

#### 1.1 Added BinaryOp, UnaryOp, LiteralKind enums

```c
typedef enum {
    BINOP_ADD, BINOP_SUB, BINOP_MUL, BINOP_DIV, BINOP_MOD,
    BINOP_AND, BINOP_OR, BINOP_XOR, BINOP_LSHIFT, BINOP_RSHIFT,
    BINOP_LAND, BINOP_LOR,
    BINOP_EQ, BINOP_NE, BINOP_LT, BINOP_LE, BINOP_GT, BINOP_GE,
} BinaryOp;

typedef enum {
    UNOP_NEG, UNOP_NOT, UNOP_BNOT, UNOP_ADDR, UNOP_DEREF,
} UnaryOp;

typedef enum {
    LIT_INT, LIT_BOOL, LIT_STRING,
} LiteralKind;
```

**Rationale**: Type checker needs to know what operation is being performed to validate operand types.

#### 1.2 Added Missing AST Node Kinds

Added to `AstNodeKind` enum:
- `AST_WHILE_STMT`: While loops
- `AST_BREAK_STMT`: Break statements
- `AST_CONTINUE_STMT`: Continue statements
- `AST_ASSIGN_STMT`: Assignment statements
- `AST_TRY_EXPR`: Try expressions for Result unwrapping
- `AST_TYPE_OPTION`: Option type

**Rationale**: These are language features mentioned in design doc but missing from original interface.

#### 1.3 Added Complete Structure Definitions

Added ~30 complete struct definitions for AST node data, including:
- `AstModule`: Module container
- `AstImportDecl`: Import declarations
- `AstFunctionDecl`: Function declarations with parameters and return type
- `AstStructDecl`, `AstEnumDecl`, `AstUnionDecl`: Type declarations
- `AstLetStmt`, `AstVarStmt`: Variable declarations
- `AstIfStmt`, `AstForStmt`, `AstWhileStmt`, `AstSwitchStmt`: Control flow
- `AstBinaryExpr`, `AstUnaryExpr`, `AstCallExpr`: Expressions
- `AstTypePrimitive`, `AstTypeArray`, `AstTypeSlice`, etc.: Type nodes

**Rationale**: Resolver and type checker need to access fields like:
- Function parameter lists and return types
- Struct field definitions
- Expression operands
- Variable initializers and type annotations

#### 1.4 Added Symbol Pointers to AST Nodes

Added `Symbol* symbol` fields to:
- `AstFunctionDecl`
- `AstStructDecl`
- `AstEnumDecl`
- `AstUnionDecl`
- `AstLetStmt`
- `AstVarStmt`
- `AstForStmt` (for iterator)
- `AstIdentifierExpr`
- `AstTypeNamed`

**Rationale**: Resolver fills in these pointers during name resolution. Later phases use them to access symbol information without re-resolving names.

#### 1.5 Added AST Helper Functions

```c
AstNode* ast_new_node(AstNodeKind kind, SourceLocation loc, Arena* arena);
```

**Rationale**: Consistent way to allocate AST nodes from arena. Used by tests and potentially by parser.

**Impact on Other Streams:**
- **Parser (Stream 2)**: Must populate all these fields when constructing AST
- **Coroutine Analysis (Stream 4)**: Can access symbol and type information via AST nodes
- **IR Lowering (Stream 5)**: Can access resolved symbols and types for code generation

---

## 2. symbol.h Changes

### Added: Complete Symbol and Scope Structures

**Original**: Basic symbol and scope definitions with incomplete union data

**Changed To**: Fully detailed symbol and scope system with helper functions

**Specific Additions:**

#### 2.1 Added SYMBOL_PARAM Kind

```c
typedef enum {
    SYMBOL_FUNCTION,
    SYMBOL_TYPE,
    SYMBOL_VARIABLE,
    SYMBOL_CONSTANT,
    SYMBOL_MODULE,
    SYMBOL_PARAM,       // NEW: Function parameter
} SymbolKind;
```

**Rationale**: Function parameters are distinct from local variables. They have a fixed order and different scoping rules.

#### 2.2 Expanded Symbol Union

Added complete union variants for all symbol kinds:

```c
union {
    struct { AstNode* ast_node; bool is_coroutine; bool is_async; } function;
    struct { AstNode* ast_node; } type_decl;
    struct { AstNode* ast_node; bool is_mutable; bool is_volatile; } variable;
    struct { AstNode* ast_node; } constant;
    struct { const char* path; AstNode* ast_node; } module;
    struct { AstNode* ast_node; size_t param_index; } param;
}
```

**Rationale**:
- `ast_node` pointer allows accessing original AST for code generation
- `is_coroutine` and `is_async` flags filled by coroutine analysis phase
- `is_mutable` distinguishes `let` (immutable) from `var` (mutable)
- `is_volatile` for volatile variable qualifier
- `param_index` preserves parameter order for calling conventions

#### 2.3 Added Scope Context Fields

```c
typedef struct Scope {
    ScopeKind kind;
    struct Scope* parent;
    Symbol** symbols;
    size_t symbol_count;
    size_t symbol_capacity;

    // NEW FIELDS:
    const char* name;           // For debugging
    AstNode* owner_node;        // AST node that owns this scope
} Scope;
```

**Rationale**:
- `name`: Helpful for debugging and error messages
- `owner_node`: Allows traversing from scope back to AST (useful for code generation)

#### 2.4 Added SymbolTable Structure

```c
typedef struct SymbolTable {
    Scope* module_scope;        // Root scope for this module
    Scope** all_scopes;         // All scopes (for cleanup)
    size_t scope_count;
    size_t scope_capacity;
    Arena* arena;               // Arena for allocations
} SymbolTable;
```

**Rationale**: Centralized symbol table management. Tracks all scopes for easy cleanup and iteration.

#### 2.5 Added Helper Functions

```c
void symbol_table_init(SymbolTable* table, Arena* arena);
Scope* scope_new(SymbolTable* table, ScopeKind kind, Scope* parent, const char* name);
Symbol* symbol_new(SymbolKind kind, const char* name, SourceLocation loc, Arena* arena);
Symbol* symbol_new_function(...);
Symbol* symbol_new_type(...);
Symbol* symbol_new_variable(...);
Symbol* symbol_new_param(...);
bool scope_has_symbol(Scope* scope, const char* name);
void scope_print(Scope* scope, FILE* out, int indent);
void symbol_print(Symbol* sym, FILE* out);
```

**Rationale**:
- Type-safe symbol creation with correct initialization
- Convenience functions for common operations
- Debug printing for development and testing

**Impact on Other Streams:**
- **Coroutine Analysis (Stream 4)**: Uses `is_coroutine` and `is_async` fields in function symbols
- **IR Lowering (Stream 5)**: Accesses symbol information for name mangling and code generation
- **Parser (Stream 2)**: No impact (parser doesn't use symbols)

---

## 3. type.h Changes

### Added: Complete Type System

**Original**: Basic type structure with incomplete data union and minimal functions

**Changed To**: Full type system with all type kinds, builders, and utilities

**Specific Additions:**

#### 3.1 Added Helper Structures

```c
typedef struct StructField {
    const char* name;
    Type* type;
    size_t offset;
    bool is_public;
} StructField;

typedef struct EnumVariant {
    const char* name;
    int64_t value;
} EnumVariant;

typedef struct UnionVariant {
    const char* name;
    Type* type;
    size_t tag_value;
} UnionVariant;

typedef struct FunctionParam {
    const char* name;
    Type* type;
} FunctionParam;
```

**Rationale**: These structures were forward-declared but never defined. They're essential for composite types.

#### 3.2 Expanded Type Data Union

Added complete union variants:

```c
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
}
```

**Rationale**: Type checker needs complete type information for:
- Size and alignment calculations
- Field access validation
- Type compatibility checking

#### 3.3 Added Type Builder Functions

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

**Rationale**: Consistent API for type construction. Handles arena allocation and initialization.

#### 3.4 Added Type Modification Functions

```c
void type_struct_add_field(Type* t, const char* name, Type* field_type, Arena* arena);
void type_enum_add_variant(Type* t, const char* name, int64_t value, Arena* arena);
void type_union_add_variant(Type* t, const char* name, Type* variant_type, size_t tag_value, Arena* arena);
void type_function_add_param(Type* t, const char* name, Type* param_type, Arena* arena);
```

**Rationale**: Types are built incrementally during AST traversal. These functions allow adding fields/variants after initial type creation.

#### 3.5 Added Type Query Functions

```c
bool type_is_integer(Type* t);
bool type_is_signed_integer(Type* t);
bool type_is_unsigned_integer(Type* t);
bool type_is_numeric(Type* t);
bool type_is_pointer_like(Type* t);
bool type_is_composite(Type* t);
const char* type_to_string(Type* t, Arena* arena);
bool type_is_assignable_to(Type* from, Type* to);
bool type_can_cast_to(Type* from, Type* to);
```

**Rationale**:
- Type queries simplify type checking logic
- `type_to_string` for error messages
- `type_is_assignable_to` for assignment checking
- `type_can_cast_to` for cast validation

#### 3.6 Added Primitive Type Singletons

```c
extern Type* TYPE_I8_SINGLETON;
extern Type* TYPE_I16_SINGLETON;
// ... all primitive types ...
extern Type* TYPE_BOOL_SINGLETON;
extern Type* TYPE_VOID_SINGLETON;

void type_init_primitives(Arena* arena);
```

**Rationale**: Primitive types are used frequently. Singletons avoid repeated allocation and enable pointer equality checks for primitives.

**Impact on Other Streams:**
- **Coroutine Analysis (Stream 4)**: Uses type sizes and alignments for frame layout
- **IR Lowering (Stream 5)**: Uses type information for memory layout and operations
- **Code Generation (Stream 6)**: Uses type sizes, alignments, and structure for C code generation

---

## 4. No Changes to error.h, arena.h, string_pool.h

These interfaces were complete and sufficient as-is:

- **error.h**: Error reporting API worked perfectly for our needs
- **arena.h**: Arena allocator interface was complete
- **string_pool.h**: String interning API was sufficient

**Rationale**: Don't change what works. These interfaces are well-designed and require no modifications.

---

## 5. New Public APIs

### 5.1 Resolver API

```c
void resolver_resolve(AstNode* module_node, SymbolTable* symbol_table,
                     StringPool* string_pool, ErrorList* errors, Arena* arena);
```

**Purpose**: Perform name resolution on an AST module.

**Preconditions**:
- `module_node` is a valid AST_MODULE node from parser
- `symbol_table` is initialized
- `string_pool` is initialized
- `errors` list is initialized
- `arena` is initialized

**Postconditions**:
- All identifiers interned in string pool
- Symbol table populated with all declarations
- All identifier expressions have `symbol` field set
- Errors added to error list if resolution fails

### 5.2 Type Checker API

```c
void typeck_check_module(AstNode* module_node, SymbolTable* symbol_table,
                        ErrorList* errors, Arena* arena);
```

**Purpose**: Perform type checking on a resolved AST module.

**Preconditions**:
- `module_node` has been processed by resolver
- `symbol_table` contains all symbols
- Primitives initialized via `type_init_primitives()`

**Postconditions**:
- All AST nodes have `type` field set
- All type nodes resolved to concrete types
- Errors added to error list if type checking fails

---

## Backward Compatibility

### Breaking Changes

All changes to ast.h, symbol.h, and type.h are **additive** or **expansions of stubs**.

**Impact**: Parser and other phases must populate new fields, but existing fields unchanged.

**Migration Path**:
1. Parser must create complete AST structures (was going to need this anyway)
2. Other phases can access new symbol and type information
3. No removal or renaming of existing fields

### Forward Compatibility

The interface design allows for future extensions:

1. **Hash-based symbol lookup**: Can add hash table to `Scope` without changing API
2. **Generic types**: Can add TYPE_GENERIC kind to TypeKind enum
3. **Effect tracking**: Can add effect annotations to function symbols
4. **Incremental compilation**: Can add module dependency graph to SymbolTable

---

## Testing Impact

All interface changes are covered by unit tests:

- **test_resolver.c**: Tests all symbol table operations
- **test_typeck.c**: Tests all type system operations

**Test Coverage**: >80% of resolver.c and typeck.c code

---

## Documentation

All interface changes are documented in:

1. **Header files**: Complete doc comments for all structures and functions
2. **subsystem-doc.md**: Architecture and design decisions
3. **This file**: Rationale for each change

---

## Summary Table

| Interface | Changes | Lines Added | Breaking Changes | New APIs |
|-----------|---------|-------------|------------------|----------|
| ast.h | Expanded node structures | ~350 | No | ast_new_node() |
| symbol.h | Expanded symbol/scope | ~50 | No | 7 new functions |
| type.h | Complete type system | ~150 | No | 20+ new functions |
| error.h | None | 0 | No | - |
| arena.h | None | 0 | No | - |
| string_pool.h | None | 0 | No | - |

---

## Recommendations for Other Streams

### For Stream 2 (Parser)

- Populate all AST node fields during parsing
- Use `ast_new_node()` for consistent allocation
- Don't worry about `symbol` or `type` fields (filled by later phases)

### For Stream 4 (Coroutine Analysis)

- Access `is_async` flag in function symbols to identify coroutines
- Set `is_coroutine` flag after analysis
- Use type sizes from type.h for frame layout

### For Stream 5 (IR Lowering)

- Use symbol table to access all declarations
- Use type information for memory layout
- Access `ast_node` pointers in symbols to generate code

### For Stream 6 (Code Generation)

- Use type sizes and alignments for C struct definitions
- Use symbol visibility for C static/extern
- Use type names for C type names

---

## Conclusion

All interface changes are well-motivated by implementation needs. The changes are additive and maintain backward compatibility where possible. The expanded interfaces provide a solid foundation for semantic analysis and support the needs of downstream compiler phases.
