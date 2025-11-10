# Parallel Implementation Plan

## Current Status (2025-11-10)

**✅ Iteration 1 COMPLETE**: All 7 streams developed independently and reconciled their interfaces
**✅ Interface Reconciliation COMPLETE**: All changes merged into `interfaces2/`
**✅ Interface Standardization COMPLETE**: All streams now use `interfaces2/` as single source of truth
**🔄 Iteration 2 IN PROGRESS**: Integration phase with stable interfaces

### Major Achievements:
- All 7 streams have working implementations
- Interfaces reconciled and documented in `interfaces2/RECONCILIATION.md`
- 34 duplicate interface files removed from stream directories
- Original `interfaces/` directory deleted to eliminate ambiguity
- All Makefiles updated to reference `interfaces2/`
- Makefiles created for Streams 4 & 5
- **interfaces2/ is now the stable linkage point for parallel work**

### Compilation Status:
- ✅ Stream 1 (Lexer): 33/33 tests passing
- ❌ Stream 2 (Parser): Grammar bug blocks compilation
- ❌ Stream 3 (Semantic): Test files need AST field updates
- ✅ Stream 4 (Coroutine): 8/8 tests passing (using temporary stubs)
- ✅ Stream 5 (Lowering): Compiles (test has data issue)
- ✅ Stream 6 (Codegen): 6/6 tests passing
- ✅ Stream 7 (Infrastructure): 20/20 tests passing

---

## Overview

This plan defines an iterative approach to parallel compiler development. Initial interfaces provide starting points, not fixed contracts. Parallel work streams develop independently, modifying interfaces as needed. After the first iteration, teams reconcile interface changes collaboratively before a second iteration with agreed-upon contracts.

**Note:** Iteration 1 and reconciliation are now complete. The reconciled interfaces are in `interfaces2/` and serve as the authoritative source for all streams.

## Core Interfaces (Now in interfaces2/)

**Status:** Reconciled and stable. All interfaces are now in `interfaces2/` directory.

These interfaces were developed during Iteration 1, modified by each stream as needed, then reconciled collaboratively. They now serve as the stable contracts for Iteration 2 integration work.

**Key Principles**:
- Prefer caller-provided memory (init, not create)
- No struct hiding - all fields visible
- Structured allocation patterns (arenas, segmented slabs)
- String pool for all identifiers and names
- Hierarchical scopes (blocks, functions, modules)
- No forward declarations needed
- Analysis starts from exported functions downward

### 1.1 Lexer Interface (`interfaces2/lexer.h`)

**Purpose**: Streaming tokenization for parser consumption

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
    TOKEN_IN, TOKEN_WHILE,

    // Operators
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
    TOKEN_AMPERSAND, TOKEN_PIPE, TOKEN_CARET, TOKEN_TILDE,
    TOKEN_LSHIFT, TOKEN_RSHIFT,
    TOKEN_AMP_AMP, TOKEN_PIPE_PIPE, TOKEN_BANG,
    TOKEN_LT, TOKEN_GT, TOKEN_LT_EQ, TOKEN_GT_EQ, TOKEN_EQ_EQ, TOKEN_BANG_EQ,
    TOKEN_EQ, TOKEN_PLUS_EQ, TOKEN_MINUS_EQ, TOKEN_STAR_EQ, TOKEN_SLASH_EQ,

    // Delimiters
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE,
    TOKEN_LBRACKET, TOKEN_RBRACKET,
    TOKEN_DOT, TOKEN_ARROW, TOKEN_COLON, TOKEN_SEMICOLON,
    TOKEN_COMMA, TOKEN_DOT_DOT,

    TOKEN_EOF,
    TOKEN_ERROR
} TokenKind;

typedef struct Token {
    TokenKind kind;
    const char* start;      // Pointer into source text
    size_t length;
    uint32_t line;
    uint32_t column;

    // For literals
    union {
        uint64_t int_value;
        struct {
            const char* str_value;  // Pointer into string pool
            size_t str_length;
        };
    } literal;
} Token;

typedef struct Lexer {
    const char* source;
    size_t source_len;
    const char* filename;

    const char* current;
    uint32_t line;
    uint32_t column;

    Token current_token;
    bool has_error;
} Lexer;
```

**Interface Functions**:
```c
// Initialize lexer with caller-provided memory
void lexer_init(Lexer* lexer, const char* source, size_t len, const char* filename);

// Get next token (parser calls this repeatedly)
Token lexer_next_token(Lexer* lexer);

// Peek at current token without advancing
Token lexer_peek(Lexer* lexer);
```

---

### 1.2 Parser Interface (`interfaces2/parser.h`)

**Purpose**: Lemon-based LALR(1) parser that constructs AST via callbacks

**Parser Implementation**: Uses SQLite's Lemon parser generator (https://sqlite.org/src/doc/trunk/doc/lemon.html)

**Interface Functions**:
```c
typedef struct Parser {
    Lexer* lexer;
    void* lemon_parser;  // Opaque Lemon parser state
    AstNode* root;       // Completed AST root
    Arena* ast_arena;    // Arena for AST allocation
    bool has_error;
} Parser;

// Initialize parser with caller-provided memory
void parser_init(Parser* parser, Lexer* lexer, Arena* ast_arena);

// Parse and return root AST node
AstNode* parser_parse(Parser* parser);

// Called by Lemon on token match
void parser_on_match(Parser* parser, /* Lemon callback args */);
```

---

### 1.3 AST Interface (`interfaces2/ast.h`)

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
// Utilities for semantic analysis
void ast_print_debug(AstNode* node, FILE* out);
void ast_walk(AstNode* node, void (*visitor)(AstNode*, void*), void* ctx);
```

**Note**: AST nodes allocated in parser's arena, no individual free needed.

---

### 1.4 String Pool Interface (`interfaces2/string_pool.h`)

**Purpose**: Centralized string interning for all identifiers and names

**Key Data Structures**:
```c
typedef struct StringPool {
    char* buffer;        // Segmented slab allocation
    size_t used;
    size_t capacity;

    // TODO: Later convert to trie for faster lookups
    const char** strings;
    size_t string_count;
} StringPool;
```

**Interface Functions**:
```c
// Initialize with caller-provided memory
void string_pool_init(StringPool* pool, Arena* arena);

// Intern a string (returns pointer to pooled copy)
const char* string_pool_intern(StringPool* pool, const char* str, size_t len);

// Lookup existing string (returns NULL if not found)
const char* string_pool_lookup(StringPool* pool, const char* str, size_t len);
```

---

### 1.5 Type System Interface (`interfaces2/type.h`)

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
// Type builders (allocate from arena)
void type_init_primitive(Type* t, TypeKind kind, Arena* arena);
void type_init_array(Type* t, Type* elem, size_t length, Arena* arena);
void type_init_slice(Type* t, Type* elem, Arena* arena);

// Type utilities
bool type_equals(Type* a, Type* b);
size_t type_sizeof(Type* t);
size_t type_alignof(Type* t);
```

---

### 1.6 Symbol Table Interface (`interfaces2/symbol.h`)

**Purpose**: Hierarchical name resolution and scope management

**Scope Hierarchy**: Module → Function → Block (nested)

**No Forward Declarations**: Single-pass resolution is sufficient.

**Key Data Structures**:
```c
typedef enum {
    SCOPE_MODULE,
    SCOPE_FUNCTION,
    SCOPE_BLOCK,
} ScopeKind;

typedef enum {
    SYMBOL_FUNCTION,
    SYMBOL_TYPE,
    SYMBOL_VARIABLE,
    SYMBOL_CONSTANT,
    SYMBOL_MODULE,
} SymbolKind;

typedef struct Symbol {
    SymbolKind kind;
    const char* name;        // Pointer into string pool
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

typedef struct Scope {
    ScopeKind kind;
    struct Scope* parent;  // NULL for module scope
    Symbol** symbols;
    size_t symbol_count;
    size_t symbol_capacity;
} Scope;
```

**Interface Functions**:
```c
// Initialize scope with caller-provided memory
void scope_init(Scope* scope, ScopeKind kind, Scope* parent, Arena* arena);

// Symbol operations
void scope_insert(Scope* scope, Symbol* sym, Arena* arena);
Symbol* scope_lookup(Scope* scope, const char* name);          // Search up hierarchy
Symbol* scope_lookup_local(Scope* scope, const char* name);    // Current scope only
```

---

### 1.7 IR Interface (`interfaces2/ir.h`)

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
// Lowering allocates IR from arena
IrNode* ir_alloc_node(IrNodeKind kind, Arena* arena);

// Codegen consumes
void codegen_emit(IrNode* ir, FILE* out);
```

---

### 1.8 Coroutine Metadata Interface (`interfaces2/coro_metadata.h`)

**Purpose**: State machine transformation information

**Analysis Strategy**: Start from exported (`pub`) functions and analyze call chains downward. This enables full-program coroutine analysis without requiring forward declarations.

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
// Coro analyzer produces (allocated from arena)
void coro_metadata_init(CoroMetadata* meta, AstNode* function, Arena* arena);
void coro_analyze_function(AstNode* function, Scope* scope, CoroMetadata* meta, Arena* arena);
```

---

### 1.9 Error Reporting Interface (`interfaces2/error.h`)

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
// Initialize with caller-provided memory
void error_list_init(ErrorList* list, Arena* arena);

// Add errors (messages stored in arena)
void error_list_add(ErrorList* list, ErrorKind kind, SourceLocation loc,
                    const char* fmt, ...);

// Reporting
void error_list_print(ErrorList* list, FILE* out);
bool error_list_has_errors(ErrorList* list);
```

---

### 1.10 Arena Allocator Interface (`interfaces2/arena.h`)

**Purpose**: Fast, structured allocation for compiler phases

**Key Data Structures**:
```c
typedef struct ArenaBlock {
    struct ArenaBlock* next;
    size_t used;
    size_t capacity;
    char data[];
} ArenaBlock;

typedef struct Arena {
    ArenaBlock* current;
    ArenaBlock* first;
    size_t block_size;
} Arena;
```

**Interface Functions**:
```c
// Initialize arena with initial block size
void arena_init(Arena* arena, size_t block_size);

// Allocate from arena (never fails, grows automatically)
void* arena_alloc(Arena* arena, size_t size, size_t alignment);

// Free entire arena (all blocks at once)
void arena_free(Arena* arena);
```

---

## Parallel Work Streams

These modules can be developed in parallel, with each stream owning and modifying their dependent interfaces during iteration 1.

### Stream 1: Lexer
**Owns**: `lexer.h`, `token.h` (Token definition)
**Depends On**: `string_pool.h`, `error.h`

**Implementation**:
- `src/lexer.c` - UTF-8 tokenization with streaming interface
- Support all token types, literals, operators
- Integrate with string pool for identifiers

**Testing**:
- Unit tests for all token types
- Edge cases: underscores in numbers, escape sequences, Unicode
- Error recovery

---

### Stream 2: Parser
**Owns**: `parser.h`, `ast.h`
**Depends On**: `lexer.h`, `arena.h`, `error.h`

**Implementation**:
- `grammar.y` - Lemon grammar specification
- `src/parser.c` - Lemon integration and AST construction callbacks
- Build complete AST with source locations

**Testing**:
- Unit tests for all AST node types
- Parse valid programs
- Error recovery and helpful messages
- Mock lexer for parser-only tests

---

### Stream 3: Semantic Analysis
**Owns**: `symbol.h`, `type.h`
**Depends On**: `ast.h`, `string_pool.h`, `arena.h`, `error.h`

**Modules**:
- `src/resolver.c` - Build scopes, resolve names, handle imports
- `src/typeck.c` - Type checking and inference

**Implementation**:
- Build hierarchical scopes (module → function → block)
- Type inference for literals and local variables
- Result type checking
- All names interned in string pool

**Testing**:
- Unit tests for scope hierarchy
- Type checking tests (valid and error cases)
- Mock ASTs for isolated testing

---

### Stream 4: Coroutine Analysis
**Owns**: `coro_metadata.h`
**Depends On**: `ast.h`, `symbol.h`, `type.h`, `arena.h`, `error.h`

**Implementation**:
- `src/coro_analyze.c` - CFG construction, liveness analysis, frame layout
- Start from exported functions, analyze call chains downward
- Compute suspend points
- Liveness analysis across suspend points
- Tagged union frame layout (one struct per state with live vars)
- Frame size calculation

**Testing**:
- Unit tests for CFG construction
- Liveness analysis tests with known results
- Frame size verification tests
- Mock typed ASTs for testing

---

### Stream 5: IR Lowering
**Owns**: `ir.h`
**Depends On**: `ast.h`, `type.h`, `symbol.h`, `coro_metadata.h`, `arena.h`

**Implementation**:
- `src/lower.c` - AST → IR transformation
  - Coroutine state machine transformation
  - Error handling (try/catch → if/goto)
  - Cleanup (defer/errdefer)
  - Computed goto patterns

**Testing**:
- Unit tests for each transformation
- Verify IR structure
- Mock ASTs and coroutine metadata

---

### Stream 6: Code Generation
**Owns**: None (consumer only)
**Depends On**: `ir.h`, `type.h`, `coro_metadata.h`

**Implementation**:
- `src/codegen.c` - IR → C11 code generation
  - Type translation (stdint.h mapping)
  - Function generation
  - State machine generation (computed goto)
  - `#line` directive insertion
  - Identifier prefixing (`__u_`)

**Testing**:
- Unit tests for C generation
- Verify C code compiles: `-Wall -Werror -Wextra -std=c11 -ffreestanding`
- Round-trip tests (compile + run)
- Mock IR for testing

---

### Stream 7: Core Infrastructure
**Owns**: `arena.h`, `string_pool.h`, `error.h`
**Depends On**: None

**Implementation**:
- `src/arena.c` - Arena allocator with segmented blocks
- `src/string_pool.c` - String interning (later: trie)
- `src/error.c` - Error reporting and formatting
- `test/test_harness.c` - Testing utilities
- `test/mock_platform.c` - Mock async runtime

**Testing**:
- Arena allocation tests
- String pool interning tests
- Mock implementations for other streams

---

## Iterative Development Strategy

### Iteration 1: Independent Development

**Goal**: Each stream implements their module(s) and modifies interfaces as needed.

**Approach**:
1. Start with initial interfaces as defined above
2. Each stream "owns" their interfaces and can modify freely
3. Use mock implementations of dependencies
4. Focus on correctness within module
5. Track interface changes in version control

**Deliverables**:
- Working implementation of each module
- Unit tests passing
- List of interface changes made
- Mock implementations for dependencies

---

### Interface Reconciliation: Collaborative Convergence

**Goal**: Merge interface changes from all streams into agreed-upon contracts.

**Process**:
1. Each stream presents their interface modifications
2. Identify conflicts (multiple streams modified same interface differently)
3. Discuss trade-offs and design choices
4. Agree on unified interface for each header
5. Document rationale for major decisions

**Key Questions**:
- Which changes improve usability?
- Which changes reduce complexity?
- Are there incompatible changes that need resolution?
- Can we simplify further?

**Deliverables**:
- Agreed-upon interface headers (committed to main)
- Design decisions document
- Migration guide for iteration 2

---

### Iteration 2: Integration

**Goal**: Update implementations to work with reconciled interfaces and integrate into pipeline.

**Approach**:
1. Each stream updates their implementation for new interfaces
2. Remove mocks, use real implementations
3. End-to-end integration testing begins
4. Fix integration bugs
5. Optimize and refactor

**Testing**:
- Full pipeline: Source → Tokens → AST → Typed AST → IR → C → Binary
- All language features
- Coroutine transformations
- Multiple architectures (x86_64, ARM, RISC-V)
- Compliance (freestanding C11)
- Memory leak checks

**Deliverables**:
- Integrated compiler pipeline
- Comprehensive test suite
- Documentation
- Example programs

---

## Pipeline Dependencies

While streams work independently, there's a natural data flow:

```
Stream 7 (Infrastructure)
    ├── Arena, StringPool, Errors
    │
    ├─> Stream 1 (Lexer) ─> Tokens
    │       │
    │       └─> Stream 2 (Parser) ─> AST
    │               │
    │               └─> Stream 3 (Semantic) ─> Typed AST + Symbols
    │                       │
    │                       ├─> Stream 4 (Coro Analysis) ─> Metadata
    │                       │       │
    │                       └───────┴─> Stream 5 (Lowering) ─> IR
    │                                       │
    │                                       └─> Stream 6 (Codegen) ─> C code
```

During iteration 1, each stream uses mocks for dependencies.
During iteration 2, streams integrate using real implementations.

---

## Development Guidelines

### During Iteration 1

1. **Interface Ownership**: Modify your owned interfaces freely to suit your needs
2. **Mock Dependencies**: Create simple mocks for testing (don't wait for other streams)
3. **Document Changes**: Track all interface modifications with rationale
4. **Test Thoroughly**: Unit tests should achieve >80% coverage
5. **Stay Simple**: Prefer simple solutions over complex abstractions

### During Reconciliation

1. **Open Communication**: Explain why you made each change
2. **Be Flexible**: Others may have better solutions
3. **Find Common Ground**: Look for designs that satisfy all needs
4. **Document Decisions**: Record why final design was chosen

### During Iteration 2

1. **Adapt Quickly**: Update your implementation for new interfaces
2. **Help Others**: Integration issues affect everyone
3. **Test Integration**: Don't just test your module, test the pipeline
4. **Fix Bugs Fast**: Integration bugs block everyone

---

## Success Criteria

The parallel implementation succeeds when:

1. **Correctness**: All test programs compile and run correctly
2. **Compliance**: Generated C compiles with `-Wall -Werror -Wextra -std=c11 -ffreestanding`
3. **Portability**: Works on x86_64, ARM, and RISC-V
4. **Performance**: Coroutine frames contain only live variables
5. **Maintainability**: Clean interfaces, documented code, >80% test coverage
6. **Completeness**: All language features implemented
