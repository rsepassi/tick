# Parallel Implementation Plan

## Overview

This plan defines key interfaces first, enabling parallel development of the compiler modules. The strategy is to lock down the data structure contracts early, then allow teams to work independently on each compilation phase.

## Phase 1: Core Interface Definitions (Week 1)

These interfaces must be defined and agreed upon **before** parallel implementation begins. All are specified in shared header files that multiple modules depend on.

### 1.1 Token Interface (`src/token.h`)

**Purpose**: Contract between lexer and parser

**Key Data Structures**:
```c
typedef enum {
    // Literals
    TOKEN_INT_LITERAL,
    TOKEN_STRING_LITERAL,
    TOKEN_BOOL_LITERAL,

    // Identifiers
    TOKEN_IDENTIFIER,

    // Keywords
    TOKEN_ASYNC, TOKEN_SUSPEND, TOKEN_RESUME,
    TOKEN_TRY, TOKEN_CATCH, TOKEN_DEFER, TOKEN_ERRDEFER,
    TOKEN_PUB, TOKEN_IMPORT, TOKEN_LET, TOKEN_VAR,
    TOKEN_FN, TOKEN_RETURN, TOKEN_IF, TOKEN_ELSE,
    TOKEN_FOR, TOKEN_SWITCH, TOKEN_CASE, TOKEN_DEFAULT,
    TOKEN_BREAK, TOKEN_CONTINUE, TOKEN_PACKED, TOKEN_VOLATILE,
    TOKEN_EMBED_FILE, TOKEN_STRUCT, TOKEN_ENUM, TOKEN_UNION,

    // Operators
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, /* ... */

    // Delimiters
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE, /* ... */

    TOKEN_EOF,
    TOKEN_ERROR
} TokenKind;

typedef struct {
    TokenKind kind;
    const char* start;      // Pointer to source text
    size_t length;
    uint32_t line;
    uint32_t column;

    // For literals
    union {
        uint64_t int_value;
        struct {
            const char* str_value;
            size_t str_length;
        };
    } literal;
} Token;

typedef struct {
    Token* tokens;
    size_t count;
    size_t capacity;
} TokenStream;
```

**Interface Functions**:
```c
// Lexer produces
TokenStream* lex_source(const char* source, size_t length, const char* filename);

// Parser consumes
void token_stream_free(TokenStream* stream);
```

---

### 1.2 AST Interface (`src/ast.h`)

**Purpose**: Contract between parser and semantic analysis phases

**Key Node Types**:
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
    AST_IF_STMT,
    AST_FOR_STMT,
    AST_SWITCH_STMT,
    AST_DEFER_STMT,
    AST_ERRDEFER_STMT,
    AST_SUSPEND_STMT,
    AST_RESUME_STMT,
    AST_BLOCK_STMT,
    AST_EXPR_STMT,

    // Expressions
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_CALL_EXPR,
    AST_ASYNC_CALL_EXPR,
    AST_FIELD_ACCESS_EXPR,
    AST_INDEX_EXPR,
    AST_LITERAL_EXPR,
    AST_IDENTIFIER_EXPR,
    AST_CAST_EXPR,

    // Types
    AST_TYPE_PRIMITIVE,
    AST_TYPE_ARRAY,
    AST_TYPE_SLICE,
    AST_TYPE_POINTER,
    AST_TYPE_FUNCTION,
    AST_TYPE_RESULT,
    AST_TYPE_NAMED,
} AstNodeKind;

typedef struct SourceLocation {
    uint32_t line;
    uint32_t column;
    const char* filename;
} SourceLocation;

typedef struct AstNode {
    AstNodeKind kind;
    SourceLocation loc;

    // Type annotation (filled by typeck phase)
    struct Type* type;

    // Union of all node-specific data
    union {
        struct { /* module fields */ } module;
        struct { /* function fields */ } function;
        struct { /* binary expr fields */ } binary_expr;
        // ... etc
    } data;
} AstNode;
```

**Interface Functions**:
```c
// Parser produces
AstNode* parse(TokenStream* tokens);

// Semantic analysis consumes
void ast_free(AstNode* node);

// Utilities
void ast_print_debug(AstNode* node);
```

---

### 1.3 Type System Interface (`src/type.h`)

**Purpose**: Shared type representation across all semantic phases

**Key Data Structures**:
```c
typedef enum {
    TYPE_I8, TYPE_I16, TYPE_I32, TYPE_I64, TYPE_ISIZE,
    TYPE_U8, TYPE_U16, TYPE_U32, TYPE_U64, TYPE_USIZE,
    TYPE_BOOL,
    TYPE_VOID,
    TYPE_ARRAY,
    TYPE_SLICE,
    TYPE_POINTER,
    TYPE_STRUCT,
    TYPE_ENUM,
    TYPE_UNION,
    TYPE_FUNCTION,
    TYPE_RESULT,
    TYPE_OPTION,
} TypeKind;

typedef struct Type {
    TypeKind kind;
    size_t size;        // Size in bytes (0 if not yet computed)
    size_t alignment;

    union {
        struct {
            struct Type* elem_type;
            size_t length;
        } array;

        struct {
            struct Type* elem_type;
        } slice;

        struct {
            struct Type* pointee_type;
        } pointer;

        struct {
            const char* name;
            struct StructField* fields;
            size_t field_count;
            bool is_packed;
        } struct_type;

        struct {
            const char* name;
            struct Type* underlying_type;
            struct EnumVariant* variants;
            size_t variant_count;
        } enum_type;

        struct {
            struct Type* value_type;
            struct Type* error_type;
        } result;

        // ... etc
    } data;
} Type;

typedef struct SymbolTable SymbolTable;
```

**Interface Functions**:
```c
Type* type_create_primitive(TypeKind kind);
Type* type_create_array(Type* elem, size_t length);
Type* type_create_slice(Type* elem);
bool type_equals(Type* a, Type* b);
size_t type_sizeof(Type* t);
size_t type_alignof(Type* t);
```

---

### 1.4 Symbol Table Interface (`src/symbol.h`)

**Purpose**: Name resolution and scope management

**Key Data Structures**:
```c
typedef enum {
    SYMBOL_FUNCTION,
    SYMBOL_TYPE,
    SYMBOL_VARIABLE,
    SYMBOL_CONSTANT,
    SYMBOL_MODULE,
} SymbolKind;

typedef struct Symbol {
    SymbolKind kind;
    const char* name;
    Type* type;
    bool is_public;
    SourceLocation defined_at;

    union {
        struct {
            AstNode* function_node;
            bool is_coroutine;  // Filled by coro_analyze
        } function;

        struct {
            AstNode* var_node;
            bool is_volatile;
        } variable;

        // ... etc
    } data;
} Symbol;

typedef struct SymbolTable {
    struct SymbolTable* parent;  // For nested scopes
    Symbol** symbols;
    size_t count;
    size_t capacity;
} SymbolTable;
```

**Interface Functions**:
```c
SymbolTable* symbol_table_create(SymbolTable* parent);
void symbol_table_insert(SymbolTable* table, Symbol* sym);
Symbol* symbol_table_lookup(SymbolTable* table, const char* name);
Symbol* symbol_table_lookup_local(SymbolTable* table, const char* name);
```

---

### 1.5 IR Interface (`src/ir.h`)

**Purpose**: Contract between lowering and code generation

**Key Data Structures**:
```c
typedef enum {
    IR_FUNCTION,
    IR_BASIC_BLOCK,
    IR_ASSIGN,
    IR_BINARY_OP,
    IR_CALL,
    IR_RETURN,
    IR_IF,
    IR_SWITCH,
    IR_FOR_LOOP,
    IR_JUMP,
    IR_LABEL,
    IR_STATE_MACHINE,  // For coroutines
    IR_SUSPEND,
    IR_RESUME,
    IR_DEFER_REGION,
} IrNodeKind;

typedef struct IrNode {
    IrNodeKind kind;
    Type* type;
    SourceLocation loc;  // For #line directives

    union {
        struct {
            const char* name;
            Type* return_type;
            struct IrParam* params;
            size_t param_count;
            struct IrNode* body;
            bool is_state_machine;
            struct CoroMetadata* coro_meta;  // If state machine
        } function;

        struct {
            uint32_t state_id;
            struct IrNode* instructions;
            size_t instruction_count;
        } basic_block;

        // ... etc
    } data;
} IrNode;
```

**Interface Functions**:
```c
// Lower produces
IrNode* lower_ast(AstNode* ast, SymbolTable* symbols);

// Codegen consumes
void codegen_emit(IrNode* ir, FILE* out);
```

---

### 1.6 Coroutine Metadata Interface (`src/coro_metadata.h`)

**Purpose**: State machine transformation information

**Key Data Structures**:
```c
typedef struct SuspendPoint {
    uint32_t state_id;
    SourceLocation loc;
    struct VarLiveness* live_vars;
    size_t live_var_count;
} SuspendPoint;

typedef struct VarLiveness {
    const char* var_name;
    Type* var_type;
    bool is_live;
} VarLiveness;

typedef struct StateStruct {
    uint32_t state_id;
    struct StateField* fields;  // Live vars at this state
    size_t field_count;
    size_t size;
    size_t alignment;
} StateStruct;

typedef struct CoroMetadata {
    const char* function_name;
    SuspendPoint* suspend_points;
    size_t suspend_count;

    StateStruct* state_structs;
    size_t state_count;

    size_t total_frame_size;  // Size of tagged union
    size_t frame_alignment;
} CoroMetadata;
```

**Interface Functions**:
```c
// Coro analyzer produces
CoroMetadata* analyze_coroutine(AstNode* function, SymbolTable* symbols);

// Lower consumes
void coro_metadata_free(CoroMetadata* meta);
```

---

### 1.7 Error Reporting Interface (`src/error.h`)

**Purpose**: Consistent error handling across all phases

**Key Data Structures**:
```c
typedef enum {
    ERROR_LEXICAL,
    ERROR_SYNTAX,
    ERROR_TYPE,
    ERROR_RESOLUTION,
    ERROR_COROUTINE,
} ErrorKind;

typedef struct CompileError {
    ErrorKind kind;
    SourceLocation loc;
    char* message;
    char* suggestion;  // Optional fix hint
} CompileError;

typedef struct ErrorList {
    CompileError* errors;
    size_t count;
    size_t capacity;
} ErrorList;
```

**Interface Functions**:
```c
ErrorList* error_list_create();
void error_list_add(ErrorList* list, ErrorKind kind, SourceLocation loc,
                    const char* fmt, ...);
void error_list_print(ErrorList* list, FILE* out);
bool error_list_has_errors(ErrorList* list);
```

---

## Phase 2: Parallel Module Implementation (Weeks 2-6)

Once interfaces are locked, these modules can be developed **in parallel** by different teams/individuals:

### Track A: Frontend (Lexer + Parser)
**Dependencies**: `token.h`, `ast.h`, `error.h`

**Modules**:
- `src/lexer.c` - Tokenization
- `src/parser.c` - LALR(1) parsing

**Deliverables**:
- Unit tests for lexer (all token types, edge cases)
- Unit tests for parser (all AST node types)
- Example source files that parse successfully

**Team Size**: 1-2 people

---

### Track B: Semantic Analysis (Resolver + Type Checker)
**Dependencies**: `ast.h`, `symbol.h`, `type.h`, `error.h`

**Modules**:
- `src/resolver.c` - Module resolution, import handling, symbol tables
- `src/typeck.c` - Type checking, type inference

**Deliverables**:
- Unit tests for symbol resolution
- Unit tests for type checking (all types, error cases)
- Integration tests with parser output

**Team Size**: 2 people (1 per module)

---

### Track C: Coroutine Analysis
**Dependencies**: `ast.h`, `symbol.h`, `type.h`, `coro_metadata.h`, `error.h`

**Modules**:
- `src/coro_analyze.c` - CFG building, liveness analysis, frame layout

**Deliverables**:
- Unit tests for suspend point detection
- Unit tests for liveness analysis
- Unit tests for frame size calculation
- Test cases with known coroutine sizes

**Team Size**: 1-2 people (complex module)

---

### Track D: IR Lowering
**Dependencies**: `ast.h`, `ir.h`, `coro_metadata.h`, `symbol.h`, `type.h`

**Modules**:
- `src/lower.c` - AST → IR transformation
  - Coroutine state machine transformation
  - Error handling lowering (try/catch)
  - Cleanup lowering (defer/errdefer)
  - Computed goto pattern lowering

**Deliverables**:
- Unit tests for each lowering transformation
- Integration tests with typeck output
- Verify IR structure matches spec

**Team Size**: 2 people

---

### Track E: Code Generation
**Dependencies**: `ir.h`, `type.h`, `coro_metadata.h`

**Modules**:
- `src/codegen.c` - IR → C11 code generation
  - Type translation (stdint.h mapping)
  - Function generation
  - State machine generation (computed goto)
  - `#line` directive insertion
  - Identifier prefixing (`__u_`)

**Deliverables**:
- Unit tests for C generation
- Verify C code compiles with `-Wall -Werror -Wextra -std=c11`
- Round-trip tests (compile generated C, run tests)

**Team Size**: 2 people

---

### Track F: Core Utilities & Testing Infrastructure
**Dependencies**: All interface headers

**Modules**:
- `src/arena.c` - Arena allocator for compiler
- `src/util.c` - String handling, file I/O
- `test/test_harness.c` - Testing framework
- `test/mock_platform.c` - Mock async runtime for testing

**Deliverables**:
- Testing utilities usable by all tracks
- Memory leak detection
- Mock platform for async testing

**Team Size**: 1 person

---

## Phase 3: Integration & Testing (Week 7)

All tracks merge. Integration testing begins:

### 3.1 End-to-End Pipeline Tests
- Source → Tokens → AST → Typed AST → IR → C → Compiled binary
- Test with all language features
- Verify coroutine transformations are correct

### 3.2 Compliance Testing
- Verify C output compiles with strict flags
- Test on multiple architectures (x86_64, ARM, RISC-V)
- Freestanding mode verification

### 3.3 Performance & Size Testing
- Measure coroutine frame sizes
- Verify against hand-computed sizes
- Benchmark compilation speed

---

## Critical Path & Ordering

```
Phase 1 (Week 1): Define ALL interfaces
    ↓
Phase 2 (Weeks 2-6): Parallel implementation
    Track A (Frontend) → Track B (Semantic) → Track C (Coro) → Track D (Lower) → Track E (Codegen)
    Track F (Utilities) - Supports all tracks
    ↓
Phase 3 (Week 7): Integration & testing
```

**Parallelization Dependencies**:
- Track A must complete before Track B can test end-to-end
- Track B must complete before Track C can test
- Track C must complete before Track D can test
- Track D must complete before Track E can test
- BUT: All can be developed simultaneously using mock inputs

---

## Development Guidelines

### 1. Interface Stability
- Once Phase 1 interfaces are committed, **no breaking changes** without team agreement
- Use semantic versioning for interface headers
- Any additions must be backward compatible

### 2. Mock-Driven Development
Each track creates **mock implementations** of dependencies:
- Track B creates mock tokens to test resolver without lexer
- Track C creates mock typed ASTs to test analysis
- Track E creates mock IR to test codegen

### 3. Continuous Integration
- Every commit must pass:
  - Unit tests for that module
  - Integration tests if dependencies are ready
  - C compilation of generated code (for codegen)
  - Memory leak checks (valgrind)

### 4. Documentation
Each module must have:
- Header documentation (Doxygen style)
- Implementation notes
- Test coverage report

---

## Risk Mitigation

### Risk 1: Interface Design Flaws
**Mitigation**: Week 1 includes review period. All teams must sign off on interfaces before Phase 2 starts.

### Risk 2: Integration Issues
**Mitigation**: Weekly integration testing starting Week 3 (even with partial implementations).

### Risk 3: Coroutine Analysis Complexity
**Mitigation**: Track C gets 2 people. Start with simple cases (no nested calls, no complex control flow).

### Risk 4: Code Generation Bugs
**Mitigation**: Extensive round-trip testing. Compile generated C, run tests, compare results.

---

## Milestone Checklist

### Week 1: Interface Definition
- [ ] `token.h` reviewed and committed
- [ ] `ast.h` reviewed and committed
- [ ] `type.h` reviewed and committed
- [ ] `symbol.h` reviewed and committed
- [ ] `ir.h` reviewed and committed
- [ ] `coro_metadata.h` reviewed and committed
- [ ] `error.h` reviewed and committed
- [ ] All teams sign off on interfaces

### Week 2-6: Parallel Implementation
- [ ] Track A: Lexer complete with tests
- [ ] Track A: Parser complete with tests
- [ ] Track B: Resolver complete with tests
- [ ] Track B: Type checker complete with tests
- [ ] Track C: Coroutine analyzer complete with tests
- [ ] Track D: IR lowering complete with tests
- [ ] Track E: Code generator complete with tests
- [ ] Track F: Testing infrastructure complete

### Week 7: Integration
- [ ] Full pipeline compiles simple programs
- [ ] Full pipeline compiles programs with coroutines
- [ ] Generated C compiles with strict flags
- [ ] Memory leak free
- [ ] Documentation complete

---

## Success Criteria

The parallel implementation is successful when:

1. **Correctness**: All test programs compile and run correctly
2. **Compliance**: Generated C compiles with `-Wall -Werror -Wextra -std=c11 -ffreestanding`
3. **Portability**: Code works on x86_64, ARM, and RISC-V
4. **Performance**: Coroutine frames use only live variables (verified)
5. **Maintainability**: Clean interfaces, documented code, >80% test coverage
