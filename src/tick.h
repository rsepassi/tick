#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_MAX 2048

#define UNUSED(x) (void)(x)

// Alignment macros
#define ALIGNF(x, align) \
  (((x) + (align) - 1) & ~((align) - 1))                // Align forward
#define ALIGNB(x, align) ((x) & ~((align) - 1))         // Align back
#define ALIGNED(x, align) (((x) & ((align) - 1)) == 0)  // Test alignment

#define CHECK_OK(expr)                                                    \
  do {                                                                    \
    if ((expr) != TICK_OK) {                                              \
      fprintf(stderr, "CHECK_OK failed: %s:%d: %s\n", __FILE__, __LINE__, \
              #expr);                                                     \
      abort();                                                            \
    }                                                                     \
  } while (0)

#define CHECK(cond, fmt, ...)                                               \
  do {                                                                      \
    if (!(cond)) {                                                          \
      fprintf(stderr, "CHECK failed: %s:%d: " fmt "\n", __FILE__, __LINE__, \
              ##__VA_ARGS__);                                               \
      abort();                                                              \
    }                                                                       \
  } while (0)

#define TICK_USER_LOG(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#define TICK_USER_LOGE(fmt, ...) \
  fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__)

#ifdef TICK_DEBUG
#define DLOG(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define DLOG(fmt, ...) \
  do {                 \
  } while (0)
#endif

#define TICK_HELP "Usage: tick emitc <input.tick> -o <output>\n"
#define TICK_READ_FILE_ERROR "Failed to read input file"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t usz;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef ssize_t isz;

typedef enum {
  TICK_OK = 0,
  TICK_ERR = 1,
} tick_err_t;

typedef struct {
  u8* buf;
  usz sz;
} tick_buf_t;

#define TICK_ALLOCATOR_ZEROMEM (1 << 0)

typedef struct {
  u8 flags;       // TICK_ALLOCATOR_*
  u8 alignment2;  // 0=regular alignment, >0 power of 2 alignment
} tick_allocator_config_t;

typedef struct {
  // buf->sz=0 -> malloc
  // buf->sz>0 -> realloc
  // newsz=0 -> free
  // config=NULL -> defaults
  tick_err_t (*realloc)(void* ctx, tick_buf_t* buf, usz newsz,
                        tick_allocator_config_t* config);
  void* ctx;
} tick_alloc_t;

typedef struct tick_alloc_seglist_s {
  tick_alloc_t backing;
  void* segments;
  usz total_allocated;
} tick_alloc_seglist_t;

typedef struct {
  u8 buf[PATH_MAX];
  usz sz;
} tick_path_t;

typedef struct {
  tick_err_t (*write)(void* ctx, tick_buf_t* buf);
  void* ctx;
} tick_writer_t;

typedef enum {
  TICK_CMD_EMITC,
} tick_cmd_t;

typedef enum {
  TICK_CLI_OK,
  TICK_CLI_ERR,
  TICK_CLI_HELP,
} tick_cli_result_t;

typedef enum {
  TICK_TOK_UNKNOWN,
  TICK_TOK_EOF,
  TICK_TOK_ERR,

  // Identifier
  TICK_TOK_IDENT,
  TICK_TOK_AT_BUILTIN,

  // Literals
  TICK_TOK_UINT_LITERAL,
  TICK_TOK_INT_LITERAL,
  TICK_TOK_STRING_LITERAL,
  TICK_TOK_BOOL_LITERAL,
  TICK_TOK_NULL,

  // Type keywords
  TICK_TOK_BOOL,
  TICK_TOK_I8,
  TICK_TOK_I16,
  TICK_TOK_I32,
  TICK_TOK_I64,
  TICK_TOK_ISZ,
  TICK_TOK_U8,
  TICK_TOK_U16,
  TICK_TOK_U32,
  TICK_TOK_U64,
  TICK_TOK_USZ,
  TICK_TOK_VOID,

  // Keywords
  TICK_TOK_AND,
  TICK_TOK_AS,
  TICK_TOK_ASYNC,
  TICK_TOK_BREAK,
  TICK_TOK_CASE,
  TICK_TOK_CATCH,
  TICK_TOK_CONTINUE,
  TICK_TOK_DEFAULT,
  TICK_TOK_DEFER,
  TICK_TOK_ELSE,
  TICK_TOK_ENUM,
  TICK_TOK_ERRDEFER,
  TICK_TOK_EXTERN,
  TICK_TOK_FN,
  TICK_TOK_FOR,
  TICK_TOK_IF,
  TICK_TOK_IMPORT,
  TICK_TOK_LET,
  TICK_TOK_ALIGN,
  TICK_TOK_OR,
  TICK_TOK_ORELSE,
  TICK_TOK_PACKED,
  TICK_TOK_PUB,
  TICK_TOK_RESUME,
  TICK_TOK_RETURN,
  TICK_TOK_STRUCT,
  TICK_TOK_SUSPEND,
  TICK_TOK_SWITCH,
  TICK_TOK_TRY,
  TICK_TOK_UNDEFINED,
  TICK_TOK_UNION,
  TICK_TOK_VAR,
  TICK_TOK_VOLATILE,

  // Punctuation
  TICK_TOK_LPAREN,        // (
  TICK_TOK_RPAREN,        // )
  TICK_TOK_LBRACE,        // {
  TICK_TOK_RBRACE,        // }
  TICK_TOK_LBRACKET,      // [
  TICK_TOK_RBRACKET,      // ]
  TICK_TOK_COMMA,         // ,
  TICK_TOK_SEMICOLON,     // ;
  TICK_TOK_COLON,         // :
  TICK_TOK_DOT,           // .
  TICK_TOK_QUESTION,      // ?
  TICK_TOK_DOT_QUESTION,  // .?
  TICK_TOK_UNDERSCORE,    // _
  TICK_TOK_AT,            // @

  // Operators
  TICK_TOK_PLUS,           // +
  TICK_TOK_MINUS,          // -
  TICK_TOK_STAR,           // *
  TICK_TOK_SLASH,          // /
  TICK_TOK_PERCENT,        // %
  TICK_TOK_AMPERSAND,      // &
  TICK_TOK_PIPE,           // |
  TICK_TOK_CARET,          // ^
  TICK_TOK_TILDE,          // ~
  TICK_TOK_BANG,           // !
  TICK_TOK_EQ,             // =
  TICK_TOK_LT,             // <
  TICK_TOK_GT,             // >
  TICK_TOK_PLUS_PIPE,      // +| (saturating add)
  TICK_TOK_MINUS_PIPE,     // -| (saturating sub)
  TICK_TOK_STAR_PIPE,      // *| (saturating mul)
  TICK_TOK_SLASH_PIPE,     // /| (saturating div)
  TICK_TOK_PLUS_PERCENT,   // +% (wrapping add)
  TICK_TOK_MINUS_PERCENT,  // -% (wrapping sub)
  TICK_TOK_STAR_PERCENT,   // *% (wrapping mul)
  TICK_TOK_SLASH_PERCENT,  // /% (wrapping div)
  TICK_TOK_BANG_EQ,        // !=
  TICK_TOK_EQ_EQ,          // ==
  TICK_TOK_LT_EQ,          // <=
  TICK_TOK_GT_EQ,          // >=
  TICK_TOK_LSHIFT,         // <<
  TICK_TOK_RSHIFT,         // >>

  // Comments
  TICK_TOK_COMMENT,  // #
} tick_tok_type_t;

typedef union {
  uint64_t u64;
  int64_t i64;
} tick_tok_literal_t;

typedef struct {
  tick_tok_type_t type;
  tick_buf_t text;
  tick_tok_literal_t literal;
  usz line;
  usz col;
} tick_tok_t;

// Literal data (for parser non-terminal to immediately copy from ephemeral
// token)
typedef struct {
  tick_tok_literal_t value;
  usz line;
  usz col;
} tick_literal_t;

// Identifier data (for parser non-terminal to immediately copy from ephemeral
// token)
typedef struct {
  tick_buf_t text;
  usz line;
  usz col;
} tick_ident_t;

typedef struct {
  tick_buf_t input;
  tick_alloc_t alloc;
  tick_buf_t errbuf;
  usz pos;
  usz line;
  usz col;
} tick_lex_t;

// ============================================================================
// AST Pipeline and Lowering Target
// ============================================================================
//
// The Tick compiler processes the AST through multiple passes before codegen:
//
//   Parser → Analysis → Lowering → Codegen → C code
//
// Each pass establishes invariants that later passes can rely on.
//
// ----------------------------------------------------------------------------
// PARSER OUTPUT
// ----------------------------------------------------------------------------
// The parser produces an AST that directly reflects the source code:
// - BINOP_* nodes preserve operator variants (+, +%, +|)
// - UNOP_* nodes preserve unary operators (-, !, ~, &, *)
// - Type names are stored as strings (TICK_TYPE_UNKNOWN)
// - resolved_type fields are NULL
//
// ----------------------------------------------------------------------------
// ANALYSIS PASS (tick_ast_analyze)
// ----------------------------------------------------------------------------
// The analysis pass performs type resolution and inference.
//
// MUST ESTABLISH:
// 1. All TYPE_NAMED nodes have builtin_type resolved:
//    - "i32" → TICK_TYPE_I32
//    - "u64" → TICK_TYPE_U64
//    - Custom types → TICK_TYPE_USER_DEFINED
//
// 2. All operation nodes have resolved_type populated:
//    - binary_expr.resolved_type → result type of the operation
//    - unary_expr.resolved_type → result type of the operation
//    - cast_expr.resolved_type → destination type
//
// 3. Type checking performed (TODO):
//    - Operand types are compatible
//    - No type errors
//
// MAY PRESERVE:
// - Original AST structure (operations stay as BINOP_*/UNOP_*)
// - High-level types (slices, optionals, error unions)
//
// ----------------------------------------------------------------------------
// LOWERING PASS (tick_ast_lower)
// ----------------------------------------------------------------------------
// The lowering pass transforms high-level constructs into simpler forms.
//
// CURRENT DESIGN: Arithmetic operations are NOT lowered. Codegen handles them.
//
// MUST TRANSFORM:
// 1. Declaration ordering and forward declarations:
//    - Reorder module.decls into correct emission order
//    - Insert forward declaration nodes for struct/union types
//    - Forward decls are duplicate DECL nodes with is_forward_decl=true
//    - Result: module.decls can be emitted in order by single-pass codegen
//    - Example: [Point, main] → [Point(fwd), Point(full), main]
//
// 2. High-level types (when implemented):
//    - TICK_AST_TYPE_SLICE → struct { T* ptr; usz len; }
//    - TICK_AST_TYPE_OPTIONAL → tagged union or nullable pointer
//    - TICK_AST_TYPE_ERROR_UNION → tagged union
//
// 3. High-level statements (when implemented):
//    - TICK_AST_DEFER_STMT → goto/label cleanup pattern
//    - TICK_AST_ERRDEFER_STMT → conditional goto/label cleanup pattern
//    - TICK_AST_TRY_CATCH_STMT → error checking with goto/label
//    - TICK_AST_ASYNC_EXPR → state machine transformation
//    - TICK_AST_SUSPEND_STMT → state machine yield
//    - TICK_AST_RESUME_STMT → state machine resume
//    - TICK_AST_FOR_SWITCH_STMT → loop/switch combination
//    - TICK_AST_CONTINUE_SWITCH_STMT → switch continuation
//
// 4. Control flow (when implemented):
//    - All for loop variants → while (true) with explicit breaks:
//      * for {}               → while (1) { body }
//      * for cond {}          → while (1) { if (!cond) break; body }
//      * for init; cond; step → init; while (1) { if (!cond) break; body; step
//      }
//    - Complex expressions decomposed into temporary variables:
//      * tmpid field in TICK_AST_DECL nodes tracks temporaries
//      * tmpid == 0: user-named variable
//      * tmpid > 0: compiler temporary (e.g., tmpid=1 → __tmp1)
//    - If conditions lowered to single identifier expressions
//    - Switch case values reduced to numeric literals or identifiers
//
// LOWERING-ONLY CONSTRUCTS:
// The lowering pass may introduce TICK_AST_GOTO_STMT and TICK_AST_LABEL_STMT
// nodes for control flow. These nodes are NEVER generated by the parser and
// should ONLY appear in the AST after lowering. They are used to implement
// defer, errdefer, error handling, and other control flow features.
//
// MAY PRESERVE:
// - Binary/unary operation nodes (codegen will map these)
// - Cast nodes (codegen will handle)
//
// CAN ASSUME (from analysis):
// - All types are resolved
// - All operation nodes have resolved_type set
// - Type checking has passed
//
// ----------------------------------------------------------------------------
// CODEGEN INPUT (what tick_codegen expects)
// ----------------------------------------------------------------------------
// Codegen performs ZERO semantic analysis. It is a pure structural translation.
//
// PERFORMANCE: Codegen MUST be a single-pass walk over module.decls. It emits
// each declaration in order without lookahead, backtracking, or reordering.
// Lowering is responsible for inserting forward declarations and ordering
// decls.
//
// REQUIRES:
// 1. All TYPE_NAMED.builtin_type fields are resolved (not TICK_TYPE_UNKNOWN)
//
// 2. All operation nodes have resolved_type:
//    - binary_expr.resolved_type must be non-NULL for arithmetic/shift ops
//    - unary_expr.resolved_type must be non-NULL for negation
//    - cast_expr.resolved_type must be non-NULL
//
// 3. High-level types MUST be lowered:
//    - No TICK_AST_TYPE_SLICE nodes
//    - No TICK_AST_TYPE_OPTIONAL nodes
//    - No TICK_AST_TYPE_ERROR_UNION nodes
//
// 4. High-level statements MUST be lowered:
//    - No TICK_AST_DEFER_STMT nodes
//    - No TICK_AST_ERRDEFER_STMT nodes
//    - No TICK_AST_TRY_CATCH_STMT nodes
//    - No TICK_AST_ASYNC_EXPR nodes
//    - No TICK_AST_SUSPEND_STMT nodes
//    - No TICK_AST_RESUME_STMT nodes
//    - No TICK_AST_FOR_SWITCH_STMT nodes
//    - No TICK_AST_CONTINUE_SWITCH_STMT nodes
//
// 5. Control flow simplified (when implemented):
//    - For loops: Expects simple conditions or NULL (infinite loop)
//    - Switch cases: Expects numeric literals or simple identifiers
//
// 6. Struct/Array initialization expressions MUST be decomposed:
//    - All field values in STRUCT_INIT_EXPR must be LITERAL or IDENTIFIER_EXPR
//    - All elements in ARRAY_INIT_EXPR must be LITERAL or IDENTIFIER_EXPR
//    - Complex expressions (calls, binops, casts, etc.) must be extracted to
//    temporaries by lowering
//    - Example transformation by lowering:
//      * Input:  Point { x: foo() + 1, y: 2 }
//      * Lowering creates: let __tmp1: i32 = foo() + 1;
//                          Point { x: __tmp1, y: 2 }
//      * Codegen receives: DECL(__tmp1) followed by STRUCT_INIT with
//      IDENTIFIER_EXPR(__tmp1)
//    - Rationale: Keeps codegen simple and avoids re-evaluating complex
//    expressions
//
// ALLOWED LOWERING-ONLY NODES:
// Codegen DOES accept TICK_AST_GOTO_STMT and TICK_AST_LABEL_STMT nodes,
// which are introduced by the lowering pass for control flow.
//
// CODEGEN BEHAVIOR:
// - Arithmetic ops: Maps (operation, resolved_type) → runtime function
//   * BINOP_ADD with i32 → tick_checked_add_i32
//   * BINOP_WRAP_ADD with i32 → tick_wrap_add_i32
//   * BINOP_SAT_ADD with u64 → tick_sat_add_u64
//   * etc.
//
// - Comparisons/logical/bitwise: Emits C operator directly (&&, ||, ==, &, |,
// ^)
//
// - Casts: Maps (source_type, dest_type) → runtime function or C cast
//   * Widening casts (i8→i32, u16→u64): Direct C cast (always safe)
//   * Same type casts: No-op or direct C cast
//   * Narrowing numeric casts: tick_checked_cast_{src}_{dst}
//     - i32→i8 → tick_checked_cast_i32_i8 (panics if out of range)
//     - u64→u32 → tick_checked_cast_u64_u32 (panics if overflow)
//   * Signed ↔ Unsigned conversions: tick_checked_cast_{src}_{dst}
//     - i32→u32 → tick_checked_cast_i32_u32 (panics if negative)
//     - u32→i32 → tick_checked_cast_u32_i32 (panics if > i32 max)
//   * Non-numeric types (pointers, bool, user-defined): Direct C cast (no
//   runtime)
//
// - Negation: Maps to tick_checked_neg_{type} for signed types
//
// - Variable names:
//   * User variables (tmpid=0): Emit as __u_{name}
//   * Compiler temporaries (tmpid>0): Emit as __tmp{N}
//
// - For loops: All variants emit as while (1) with explicit breaks
//
// - Switch: Emits as C switch with fall-through semantics, default when
// values=NULL
//
// - Type Declarations:
//   * Structs: Emit typedef struct with fields, handle packed/alignment
//   attributes
//   * Enums: Emit typedef enum with underlying type, emit all values as
//   literals
//   * Unions: Emit tagged union (tag enum + data union wrapper struct)
//   * Forward declarations emitted for all struct/union types before
//   definitions
//   * Emission to header if pub, always to C file
//
// If codegen encounters:
// - TICK_TYPE_UNKNOWN → Fatal error (analysis didn't run)
// - NULL resolved_type for arithmetic op → Fatal error (analysis didn't run)
// - Unsupported high-level type → Fatal error (lowering didn't run)
// - NULL enum value → Fatal error (analysis didn't calculate auto-increment)
// - Non-literal alignment → Fatal error (analysis didn't evaluate constant)
// - Union with tag_type=NULL → Fatal error (lowering didn't generate tag)
//
// ----------------------------------------------------------------------------
// TYPE DECLARATION REQUIREMENTS
// ----------------------------------------------------------------------------
// For struct/enum/union declarations to work correctly, analysis and lowering
// must establish these invariants:
//
// ANALYSIS MUST ESTABLISH:
// 1. User-defined type symbol table:
//    - All struct/enum/union declarations registered by name
//    - Duplicate type names detected and reported as errors
//
// 2. Type reference resolution:
//    - All TYPE_NAMED nodes have type_decl pointer set for user types
//    - type_decl points to the STRUCT_DECL/ENUM_DECL/UNION_DECL node
//    - type_decl is NULL only for builtin types
//
// 3. Enum value calculation:
//    - All ENUM_VALUE nodes have value field set to LITERAL node (no NULL)
//    - Auto-increment values calculated: Red (NULL→0), Green=5, Blue (NULL→6)
//    - Values evaluated to integer literals
//
// 4. Alignment expression evaluation:
//    - All alignment fields reduced to LITERAL nodes or NULL
//    - Validated: alignment values are powers of 2
//    - Validated: alignment values are within target limits
//
// 5. Circular dependency detection:
//    - Circular value-embedding detected and reported as error:
//      * struct A { b: B }; struct B { a: A };  → ERROR
//    - Circular pointer references allowed (forward decl resolves):
//      * struct A { b: *B }; struct B { a: *A };  → OK
//
// LOWERING MUST TRANSFORM:
// 1. Auto-tagged unions:
//    - union { x: i32, y: u64 } with tag_type=NULL
//    - Must be transformed into explicit tagged union:
//      * Generate enum for tag: enum { x_tag, y_tag }
//      * Generate wrapper struct: struct { tag_enum tag; union { ... } data; }
//    - After lowering: all UNION_DECL nodes have non-NULL tag_type
//
// CODEGEN CAN ASSUME:
// 1. All enum values are LITERAL nodes (no NULL values)
// 2. All alignment expressions are LITERAL nodes or NULL
// 3. Types can be emitted in AST order with forward declarations
// 4. All unions have explicit tag types (no tag_type=NULL)
// 5. Type dependencies are valid (no circular value-embedding)
//
// ============================================================================

// Literal types
typedef enum {
  TICK_LIT_INT,
  TICK_LIT_UINT,
  TICK_LIT_STRING,
  TICK_LIT_BOOL,
  TICK_LIT_NULL,
  TICK_LIT_UNDEFINED,
} tick_literal_kind_t;

// AST node types
typedef enum {
  TICK_AST_LITERAL,
  TICK_AST_ERROR,
  TICK_AST_MODULE,
  TICK_AST_IMPORT,
  TICK_AST_IMPORT_DECL,
  TICK_AST_DECL,
  TICK_AST_FUNCTION_DECL,
  TICK_AST_FUNCTION,
  TICK_AST_PARAM,
  TICK_AST_STRUCT_DECL,
  TICK_AST_ENUM_DECL,
  TICK_AST_UNION_DECL,
  TICK_AST_RETURN_STMT,
  TICK_AST_BLOCK_STMT,
  TICK_AST_EXPR_STMT,
  TICK_AST_LET_STMT,
  TICK_AST_VAR_STMT,
  TICK_AST_ASSIGN_STMT,
  TICK_AST_UNUSED_STMT,
  TICK_AST_IF_STMT,
  TICK_AST_FOR_STMT,
  TICK_AST_SWITCH_STMT,
  TICK_AST_BREAK_STMT,
  TICK_AST_CONTINUE_STMT,
  TICK_AST_GOTO_STMT,
  TICK_AST_LABEL_STMT,
  TICK_AST_DEFER_STMT,
  TICK_AST_ERRDEFER_STMT,
  TICK_AST_SUSPEND_STMT,
  TICK_AST_RESUME_STMT,
  TICK_AST_TRY_CATCH_STMT,
  TICK_AST_FOR_SWITCH_STMT,
  TICK_AST_CONTINUE_SWITCH_STMT,
  TICK_AST_BINARY_EXPR,
  TICK_AST_UNARY_EXPR,
  TICK_AST_CALL_EXPR,
  TICK_AST_BUILTIN_CALL,
  TICK_AST_FIELD_ACCESS_EXPR,
  TICK_AST_INDEX_EXPR,
  TICK_AST_UNWRAP_PANIC_EXPR,
  TICK_AST_STRUCT_INIT_EXPR,
  TICK_AST_ARRAY_INIT_EXPR,
  TICK_AST_CAST_EXPR,
  TICK_AST_IDENTIFIER_EXPR,
  TICK_AST_ASYNC_EXPR,
  TICK_AST_TYPE_NAMED,
  TICK_AST_TYPE_ARRAY,
  TICK_AST_TYPE_SLICE,
  TICK_AST_TYPE_POINTER,
  TICK_AST_TYPE_OPTIONAL,
  TICK_AST_TYPE_ERROR_UNION,
  TICK_AST_TYPE_FUNCTION,
  TICK_AST_FIELD,
  TICK_AST_ENUM_VALUE,
  TICK_AST_SWITCH_CASE,
  TICK_AST_STRUCT_INIT_FIELD,
  TICK_AST_EXPORT_STMT,
} tick_ast_node_kind_t;

// AST node location
typedef struct {
  usz line;
  usz col;
} tick_ast_loc_t;

// Forward declare for recursive types
typedef struct tick_ast_node_s tick_ast_node_t;

// Qualifiers for declarations
typedef struct {
  bool is_var;  // false = let, true = var
  bool is_pub;
  bool is_volatile;
  bool is_extern;        // true = extern declaration (no definition)
  bool is_forward_decl;  // true = forward decl only (set by lowering)
} tick_qualifier_flags_t;

// Struct qualifiers
typedef struct {
  bool is_packed;
  tick_ast_node_t* alignment;  // NULL = default, expr = explicit alignment
} tick_struct_quals_t;

// Binary operators
typedef enum {
  BINOP_ADD,
  BINOP_SUB,
  BINOP_MUL,
  BINOP_DIV,
  BINOP_MOD,
  BINOP_EQ,
  BINOP_NE,
  BINOP_LT,
  BINOP_GT,
  BINOP_LE,
  BINOP_GE,
  BINOP_AND,
  BINOP_OR,
  BINOP_XOR,
  BINOP_LSHIFT,
  BINOP_RSHIFT,
  BINOP_LOGICAL_AND,
  BINOP_LOGICAL_OR,
  BINOP_SAT_ADD,
  BINOP_SAT_SUB,
  BINOP_SAT_MUL,
  BINOP_SAT_DIV,
  BINOP_WRAP_ADD,
  BINOP_WRAP_SUB,
  BINOP_WRAP_MUL,
  BINOP_WRAP_DIV,
  BINOP_ORELSE,
} tick_binop_t;

// Unary operators
typedef enum {
  UNOP_NEG,
  UNOP_NOT,
  UNOP_BIT_NOT,
  UNOP_ADDR,
  UNOP_DEREF,
} tick_unop_t;

// Assignment operators
typedef enum {
  ASSIGN_EQ,
} tick_assign_op_t;

// Builtin functions (for lowered operations)
typedef enum {
  TICK_BUILTIN_SAT_ADD,
  TICK_BUILTIN_SAT_SUB,
  TICK_BUILTIN_SAT_MUL,
  TICK_BUILTIN_SAT_DIV,
  TICK_BUILTIN_WRAP_ADD,
  TICK_BUILTIN_WRAP_SUB,
  TICK_BUILTIN_WRAP_MUL,
  TICK_BUILTIN_WRAP_DIV,
  TICK_BUILTIN_CHECKED_ADD,
  TICK_BUILTIN_CHECKED_SUB,
  TICK_BUILTIN_CHECKED_MUL,
  TICK_BUILTIN_CHECKED_DIV,
  TICK_BUILTIN_CHECKED_MOD,
  TICK_BUILTIN_CHECKED_SHL,
  TICK_BUILTIN_CHECKED_SHR,
  TICK_BUILTIN_CHECKED_NEG,
  TICK_BUILTIN_CHECKED_CAST,
} tick_builtin_t;

// Builtin types (resolved during analysis)
typedef enum {
  TICK_TYPE_UNKNOWN,  // Not yet resolved
  TICK_TYPE_I8,
  TICK_TYPE_I16,
  TICK_TYPE_I32,
  TICK_TYPE_I64,
  TICK_TYPE_ISZ,
  TICK_TYPE_U8,
  TICK_TYPE_U16,
  TICK_TYPE_U32,
  TICK_TYPE_U64,
  TICK_TYPE_USZ,
  TICK_TYPE_BOOL,
  TICK_TYPE_VOID,
  TICK_TYPE_USER_DEFINED,  // Struct, enum, union, or other user-defined type
} tick_builtin_type_t;

// AT builtins (@ prefix functions)
typedef enum {
  TICK_AT_BUILTIN_UNKNOWN,  // Not yet resolved or invalid
  TICK_AT_BUILTIN_DBG,      // @dbg - debug log
} tick_at_builtin_t;

// AST node structure
struct tick_ast_node_s {
  tick_ast_node_kind_t kind;
  tick_ast_loc_t loc;
  tick_ast_node_t* next;  // Next node in list
  tick_ast_node_t* prev;  // Previous node in list
  tick_ast_node_t*
      tail;  // For list heads: pointer to last node (used during parsing)
  union {
    struct {
      tick_literal_kind_t kind;
      union {
        uint64_t uint_value;
        int64_t int_value;
        bool bool_value;
        tick_buf_t string_value;  // For string literals
      } data;
    } literal;
    struct {
      tick_buf_t name;
      tick_ast_node_t* decls;
    } module;
    struct {
      tick_buf_t path;
    } import;
    struct {
      tick_buf_t name;
      tick_ast_node_t* type;
      tick_ast_node_t* init;
      tick_qualifier_flags_t quals;
      u32 tmpid;  // 0=user-named, >0=unnamed temporary
    } decl;
    struct {
      tick_ast_node_t* params;
      tick_ast_node_t* return_type;
      tick_ast_node_t* body;
      tick_qualifier_flags_t quals;
    } function;
    struct {
      tick_buf_t name;
      tick_ast_node_t* type;
    } param;
    struct {
      tick_ast_node_t* fields;  // Linked list of TICK_AST_FIELD nodes
      bool is_packed;
      tick_ast_node_t* alignment;  // NULL = default, expr = explicit alignment
    } struct_decl;
    struct {
      tick_ast_node_t* underlying_type;
      tick_ast_node_t* values;  // Linked list of TICK_AST_ENUM_VALUE nodes
    } enum_decl;
    struct {
      tick_ast_node_t* tag_type;   // NULL for automatic tag
      tick_ast_node_t* fields;     // Linked list of TICK_AST_FIELD nodes
      tick_ast_node_t* alignment;  // NULL = default, expr = explicit alignment
    } union_decl;
    struct {
      tick_ast_node_t* value;
    } return_stmt;
    struct {
      tick_ast_node_t* stmts;
    } block_stmt;
    struct {
      tick_ast_node_t* expr;
    } expr_stmt;
    struct {
      tick_ast_node_t* lhs;
      tick_assign_op_t op;
      tick_ast_node_t* rhs;
    } assign_stmt;
    struct {
      tick_ast_node_t* expr;
    } unused_stmt;
    struct {
      tick_ast_node_t* condition;
      tick_ast_node_t* then_block;
      tick_ast_node_t* else_block;  // Can be another if_stmt for else-if
    } if_stmt;
    struct {
      tick_ast_node_t* init_stmt;  // Can be NULL
      tick_ast_node_t* condition;  // Can be NULL (infinite loop)
      tick_ast_node_t* step_stmt;  // Can be NULL
      tick_ast_node_t* body;
    } for_stmt;
    struct {
      tick_ast_node_t* value;
      tick_ast_node_t* cases;  // Linked list of TICK_AST_SWITCH_CASE nodes
    } switch_stmt;
    struct {
      tick_binop_t op;
      tick_ast_node_t* left;
      tick_ast_node_t* right;
      tick_ast_node_t* resolved_type;  // Filled by analysis pass
    } binary_expr;
    struct {
      tick_unop_t op;
      tick_ast_node_t* operand;
      tick_ast_node_t* resolved_type;  // Filled by analysis pass
    } unary_expr;
    struct {
      tick_ast_node_t* callee;
      tick_ast_node_t* args;
    } call_expr;
    struct {
      tick_builtin_t builtin;
      tick_ast_node_t*
          type;  // Type for type-specific builtins (e.g., i32, u64)
      tick_ast_node_t* args;  // Linked list of argument expressions
    } builtin_call;
    struct {
      tick_ast_node_t* object;
      tick_buf_t field_name;
      bool is_arrow;
    } field_access_expr;
    struct {
      tick_ast_node_t* array;
      tick_ast_node_t* index;
    } index_expr;
    struct {
      tick_ast_node_t* type;
      tick_ast_node_t*
          fields;  // Linked list of TICK_AST_STRUCT_INIT_FIELD nodes
    } struct_init_expr;
    struct {
      tick_ast_node_t* elements;
    } array_init_expr;
    struct {
      tick_ast_node_t* type;
      tick_ast_node_t* expr;
      tick_ast_node_t* resolved_type;  // Filled by analysis pass (result type)
    } cast_expr;
    struct {
      tick_ast_node_t* operand;
      tick_ast_node_t* resolved_type;  // Filled by analysis pass (inner type)
    } unwrap_panic_expr;
    struct {
      tick_buf_t name;
      tick_at_builtin_t
          at_builtin;  // Resolved AT builtin (UNKNOWN if not a builtin)
    } identifier_expr;
    struct {
      tick_ast_node_t* call;   // Function call expression
      tick_ast_node_t* frame;  // Frame expression
    } async_expr;
    struct {
      tick_ast_node_t* stmt;
    } defer_stmt;
    struct {
      tick_ast_node_t* stmt;
    } errdefer_stmt;
    struct {
      tick_buf_t label;
    } goto_stmt;
    struct {
      tick_buf_t label;
    } label_stmt;
    struct {
      tick_ast_node_t* handle;
    } resume_stmt;
    struct {
      tick_ast_node_t* call;        // Function call to try
      tick_buf_t capture;           // |name| capture
      tick_ast_node_t* catch_stmt;  // Statement to execute on error
    } try_catch_stmt;
    struct {
      tick_ast_node_t* value;
      tick_ast_node_t* body;
    } for_switch_stmt;
    struct {
      tick_ast_node_t* value;
    } continue_switch_stmt;
    struct {
      tick_buf_t name;
      tick_builtin_type_t builtin_type;  // Resolved during analysis
      tick_ast_node_t*
          type_decl;  // For user-defined types, pointer to declaration
    } type_named;
    struct {
      tick_ast_node_t* element_type;
      tick_ast_node_t* size;  // NULL for slice type
    } type_array;
    struct {
      tick_ast_node_t* pointee_type;
    } type_pointer;
    struct {
      tick_ast_node_t* inner_type;
    } type_optional;
    struct {
      tick_ast_node_t* error_type;  // NULL for !T shorthand
      tick_ast_node_t* value_type;
    } type_error_union;
    struct {
      tick_ast_node_t* params;  // Linked list of params (may have NULL names)
      tick_ast_node_t* return_type;  // NULL = void
    } type_function;
    struct {
      tick_buf_t name;
      tick_ast_node_t* type;
      tick_ast_node_t* alignment;  // NULL = default, expr = explicit alignment
    } field;
    struct {
      tick_buf_t name;
      tick_ast_node_t* value;  // NULL for auto-increment
    } enum_value;
    struct {
      tick_ast_node_t* values;  // NULL for default case
      tick_ast_node_t* stmts;
    } switch_case;
    struct {
      tick_buf_t field_name;
      tick_ast_node_t* value;
    } struct_init_field;
    struct {
      tick_buf_t name;
    } export_stmt;
  } data;
};

typedef struct {
  tick_ast_node_t* root;
} tick_ast_t;

typedef struct {
  tick_alloc_t alloc;
  tick_buf_t errbuf;
  tick_ast_t root;
  void* lemon_parser;  // Lemon parser instance
  bool has_error;      // Set to true when parse error occurs
} tick_parse_t;

typedef struct {
  tick_cmd_t cmd;
  union {
    struct {
      tick_buf_t input;
      tick_buf_t output;
    } emitc;
  };
} tick_driver_args_t;

// Driver functions
tick_cli_result_t tick_driver_parse_args(tick_driver_args_t* args, int argc,
                                         char** argv);
tick_err_t tick_driver_read_file(tick_alloc_t* alloc, tick_buf_t* output,
                                 tick_buf_t path);

// Allocator functions
tick_alloc_t tick_allocator_libc(void);
tick_alloc_t tick_allocator_seglist(tick_alloc_seglist_t* seglist,
                                    tick_alloc_t backing);

// Lexer functions
void tick_lex_init(tick_lex_t* lex, tick_buf_t input, tick_alloc_t alloc,
                   tick_buf_t errbuf);
void tick_lex_next(tick_lex_t* lex, tick_tok_t* tok);
const char* tick_tok_format(tick_tok_t* tok, char* buf, usz buf_sz);

// Parser functions
void tick_parse_init(tick_parse_t* parse, tick_alloc_t alloc,
                     tick_buf_t errbuf);
tick_err_t tick_parse_tok(tick_parse_t* parse, tick_tok_t* tok);

// AST functions
const char* tick_ast_kind_str(tick_ast_node_kind_t kind);
tick_err_t tick_ast_analyze(tick_ast_t* ast, tick_alloc_t alloc,
                            tick_buf_t errbuf);
tick_err_t tick_ast_lower(tick_ast_t* ast, tick_alloc_t alloc,
                          tick_buf_t errbuf);

// AST list helpers
void tick_ast_list_init(tick_ast_node_t* node);
tick_ast_node_t* tick_ast_list_append(tick_ast_node_t* head,
                                      tick_ast_node_t* node);

// Output functions
tick_err_t tick_output_format_name(tick_buf_t output_path,
                                   tick_path_t* output_h,
                                   tick_path_t* output_c);
tick_writer_t tick_file_writer(FILE* f);

// Codegen functions
tick_err_t tick_codegen(tick_ast_t* ast, const char* source_filename,
                        const char* header_filename, tick_writer_t writer_h,
                        tick_writer_t writer_c);
