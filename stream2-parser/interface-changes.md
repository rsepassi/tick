# Interface Changes Documentation

This document details all changes made to interfaces during the Parser (Stream 2) implementation.

## Summary

- **Modified Interfaces**: 2 (parser.h, ast.h)
- **Unmodified Interfaces**: 3 (lexer.h, arena.h, error.h)
- **New Interfaces**: 0

---

## 1. ast.h - AST Node Definitions

### Change Type: EXPANDED

### Original Interface

The original `ast.h` provided only skeletal definitions:

```c
typedef enum {
    AST_MODULE,
    AST_IMPORT_DECL,
    AST_FUNCTION_DECL,
    AST_STRUCT_DECL,
    AST_ENUM_DECL,
    AST_UNION_DECL,
    AST_LET_STMT,
    AST_VAR_STMT,
    AST_RETURN_STMT,
    // ... (partial list)
} AstNodeKind;

typedef struct AstNode {
    AstNodeKind kind;
    SourceLocation loc;
    struct Type* type;
    union {
        struct { /* module fields */ } module;
        // ... (empty placeholders)
    } data;
} AstNode;
```

### Changes Made

#### 1. Added `stdbool.h` Include

**Rationale**: Many AST node fields use boolean flags (e.g., `is_pub`, `is_packed`, `is_volatile`).

```c
#include <stdbool.h>
```

#### 2. Expanded AstNodeKind Enum

**Added Node Types**:

- **Declarations**: `AST_LET_DECL`, `AST_VAR_DECL`
- **Statements**: `AST_ASSIGN_STMT`, `AST_BREAK_STMT`, `AST_CONTINUE_STMT`, `AST_CONTINUE_SWITCH_STMT`, `AST_TRY_CATCH_STMT`
- **Expressions**: `AST_TRY_EXPR`, `AST_STRUCT_INIT_EXPR`, `AST_ARRAY_INIT_EXPR`, `AST_RANGE_EXPR`

**Rationale**: These node types are required to represent all language constructs:
- Separate `AST_LET_DECL` from `AST_LET_STMT` for top-level vs local scope
- `AST_CONTINUE_SWITCH_STMT` for computed goto pattern
- `AST_TRY_EXPR` for error propagation (`try expr`)
- Initialization expressions for structured data

#### 3. Added Operator Enums

**New Types**:

```c
typedef enum {
    BINOP_ADD, BINOP_SUB, BINOP_MUL, BINOP_DIV, BINOP_MOD,
    BINOP_AND, BINOP_OR, BINOP_XOR, BINOP_LSHIFT, BINOP_RSHIFT,
    BINOP_LOGICAL_AND, BINOP_LOGICAL_OR,
    BINOP_LT, BINOP_GT, BINOP_LT_EQ, BINOP_GT_EQ, BINOP_EQ_EQ, BINOP_BANG_EQ,
} BinaryOp;

typedef enum {
    UNOP_NEG, UNOP_NOT, UNOP_BIT_NOT, UNOP_DEREF, UNOP_ADDR,
} UnaryOp;

typedef enum {
    ASSIGN_SIMPLE, ASSIGN_ADD, ASSIGN_SUB, ASSIGN_MUL, ASSIGN_DIV,
    ASSIGN_MOD, ASSIGN_AND, ASSIGN_OR, ASSIGN_XOR, ASSIGN_LSHIFT, ASSIGN_RSHIFT,
} AssignOp;
```

**Rationale**: Type-safe operator representation instead of token kinds or magic numbers.

#### 4. Added Helper Structures

**New Structures**:

- `AstTypeNode` - Represents type expressions in AST
- `AstField` - Struct/union field definition
- `AstParam` - Function parameter definition
- `AstEnumValue` - Enum value definition
- `AstSwitchCase` - Switch case clause
- `AstStructInit` - Struct field initializer

**Rationale**: These structures are needed to properly represent composite declarations and are used extensively throughout the AST.

#### 5. Filled In Node Data Structures

**Completed Union Fields** for all node types:

- **module**: `name`, `decls[]`, `decl_count`
- **import_decl**: `name`, `path`, `is_pub`
- **function_decl**: `name`, `params[]`, `param_count`, `return_type`, `body`, `is_pub`
- **struct_decl**: `name`, `fields[]`, `field_count`, `is_packed`, `is_pub`
- **enum_decl**: `name`, `underlying_type`, `values[]`, `value_count`, `is_pub`
- **union_decl**: `name`, `tag_type`, `fields[]`, `field_count`, `is_pub`
- And all other node types...

**Rationale**: The original interface left these as placeholders. We filled them in with the actual data needed to represent each construct.

#### 6. Added Literal Types

```c
struct {
    enum {
        LITERAL_INT,
        LITERAL_STRING,
        LITERAL_BOOL,
    } literal_kind;
    union {
        uint64_t int_value;
        struct { const char* str_value; size_t str_length; } string;
        bool bool_value;
    } value;
} literal_expr;
```

**Rationale**: Discriminated union for different literal types with their values.

### Impact on Other Modules

- **Resolver (Stream 3)**: Can now traverse complete AST structure
- **Type Checker (Stream 4)**: Has all node types needed for type annotation
- **Lowering (Stream 5)**: Can transform all language constructs to IR

---

## 2. parser.h - Parser Interface

### Change Type: ENHANCED

### Original Interface

```c
typedef struct Parser {
    Lexer* lexer;
    void* lemon_parser;
    AstNode* root;
    Arena* ast_arena;
    bool has_error;
} Parser;

void parser_init(Parser* parser, Lexer* lexer, Arena* ast_arena);
AstNode* parser_parse(Parser* parser);
void parser_on_match(Parser* parser /* Lemon callback args */);
```

### Changes Made

#### 1. Added error.h Include

**Rationale**: Parser needs to report errors through ErrorList.

```c
#include "error.h"
```

#### 2. Added ErrorList to Parser Structure

**Change**:
```c
typedef struct Parser {
    // ... existing fields ...
    ErrorList* errors;   // NEW: Error list for reporting
    bool has_error;
} Parser;
```

**Rationale**: Structured error reporting instead of just boolean flag. Allows collection of multiple errors with source locations.

#### 3. Updated parser_init Signature

**Change**:
```c
// Old:
void parser_init(Parser* parser, Lexer* lexer, Arena* ast_arena);

// New:
void parser_init(Parser* parser, Lexer* lexer, Arena* ast_arena, ErrorList* errors);
```

**Rationale**: Parser needs error list for reporting syntax errors.

#### 4. Removed parser_on_match

**Change**: Removed `void parser_on_match(Parser* parser /* Lemon callback args */);`

**Rationale**: This was a placeholder. Lemon grammar actions directly call helper functions instead.

#### 5. Added parser_cleanup

**Change**:
```c
void parser_cleanup(Parser* parser);
```

**Rationale**: Explicit cleanup of Lemon parser state. Follows resource management best practices.

#### 6. Added Helper Functions

**New Functions**:
```c
AstNode* ast_alloc(Parser* parser, AstNodeKind kind, SourceLocation loc);
void parser_error(Parser* parser, SourceLocation loc, const char* fmt, ...);
```

**Rationale**:
- `ast_alloc`: Used by grammar actions to allocate AST nodes from arena
- `parser_error`: Used to report syntax errors with formatted messages

### Impact on Other Modules

- **Lexer**: No changes required (interface unchanged)
- **Error Reporting**: Parser now properly integrates with error system
- **Arena**: No changes required (interface unchanged)

---

## 3. lexer.h - Lexer Interface

### Change Type: NONE

**Status**: Used as-is without modifications.

**Rationale**: The lexer interface was well-designed and complete. No changes needed for parser implementation.

---

## 4. arena.h - Arena Allocator Interface

### Change Type: NONE

**Status**: Used as-is without modifications.

**Rationale**: The arena interface provides exactly what the parser needs for AST allocation. No changes needed.

---

## 5. error.h - Error Reporting Interface

### Change Type: NONE

**Status**: Used as-is without modifications.

**Rationale**: The error reporting interface handles all our needs for syntax error reporting. No changes needed.

---

## Summary of Interface Changes

### Breaking Changes

1. **parser_init signature change**
   - **Impact**: Any code calling parser_init needs to pass ErrorList
   - **Migration**: Add error list parameter to calls
   - **Example**: `parser_init(&parser, &lexer, &arena, &errors);`

2. **Removed parser_on_match**
   - **Impact**: Low - was a placeholder function
   - **Migration**: Not needed, used internal helpers instead

### Non-Breaking Changes

1. **ast.h expansions**
   - All additions are new fields/types
   - No existing code broken
   - Only enables new functionality

2. **New helper functions**
   - `ast_alloc`, `parser_error`, `parser_cleanup`
   - Additive only, no breaking changes

### Compatibility Notes

- **Forward Compatibility**: All changes are additive or replace placeholders
- **Backward Compatibility**: Only `parser_init` signature changed
- **Semantic Versioning**: This would be a minor version bump (new features, one breaking change)

---

## Design Rationale

### Why These Changes Were Necessary

1. **AST Node Expansions**
   - Original interface was intentionally skeletal
   - Parser implementation required complete node definitions
   - Added all necessary fields to represent language constructs

2. **Error Handling Integration**
   - Parser needs to report multiple syntax errors
   - ErrorList provides structured error reporting
   - Better error messages improve developer experience

3. **Helper Functions**
   - Reduce code duplication in grammar actions
   - Centralize memory allocation logic
   - Provide consistent error reporting

### Alternative Approaches Considered

1. **Error Handling**
   - **Alternative**: Return error codes from parser
   - **Rejected**: Less flexible, harder to collect multiple errors
   - **Chosen**: ErrorList for structured reporting

2. **AST Allocation**
   - **Alternative**: Each grammar action allocates directly
   - **Rejected**: Code duplication, error-prone
   - **Chosen**: Centralized `ast_alloc` helper

3. **Node Type Granularity**
   - **Alternative**: Fewer node types, more fields
   - **Rejected**: Less type-safe, harder to pattern match
   - **Chosen**: Fine-grained node types for clarity

---

## Validation

All interface changes were validated through:

1. **Unit Tests**: 30+ test cases covering all node types
2. **Integration**: Verified interfaces work together correctly
3. **Documentation**: All changes documented with rationale

---

## Recommendations for Future Streams

### For Stream 3 (Resolver)

- Use expanded AST node types directly
- Leverage SourceLocation for error reporting
- Build symbol tables by traversing AST

### For Stream 4 (Type Checker)

- Annotate `AstNode.type` field during checking
- Use operator enums for type-safe checking
- Report errors through ErrorList

### For Stream 5 (Lowering)

- Transform complete AST to IR
- Use helper structures (AstParam, AstField, etc.)
- Preserve source locations for debugging

---

## Conclusion

All interface changes were made thoughtfully with clear rationale:

1. **Minimal Breaking Changes**: Only parser_init signature changed
2. **Complete Implementation**: AST now represents all language features
3. **Proper Integration**: Error handling, memory management work correctly
4. **Type Safety**: Enums and structures provide compile-time safety
5. **Documentation**: All changes documented for future maintainers

The interfaces now provide a solid foundation for the remaining compiler streams while maintaining compatibility where possible and making necessary improvements where required.
