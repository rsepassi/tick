#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_MAX 2048

#define UNUSED(x) (void)(x)

// Alignment macros
#define ALIGNF(x, align) (((x) + (align) - 1) & ~((align) - 1))  // Align forward
#define ALIGNB(x, align) ((x) & ~((align) - 1))                   // Align back
#define ALIGNED(x, align) (((x) & ((align) - 1)) == 0)            // Test alignment

#define CHECK_OK(expr) do { \
  if ((expr) != TICK_OK) { \
    fprintf(stderr, "CHECK_OK failed: %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    abort(); \
  } \
} while (0)

#define TICK_USER_LOG(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#define TICK_USER_LOGE(fmt, ...) fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__)

#ifdef TICK_DEBUG
#define DLOG(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define DLOG(fmt, ...) do {} while (0)
#endif

#define TICK_HELP "Usage: tick emitc <input.tick> -o <output>\n"
#define TICK_READ_FILE_ERROR "Failed to read input file"

typedef uint8_t u8;
typedef size_t usz;
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
  u8 flags;  // TICK_ALLOCATOR_*
  u8 alignment2;  // 0=regular alignment, >0 power of 2 alignment
} tick_allocator_config_t;

typedef struct {
  // buf->sz=0 -> malloc
  // buf->sz>0 -> realloc
  // newsz=0 -> free
  // config=NULL -> defaults
  tick_err_t (*realloc)(void* ctx, tick_buf_t* buf, usz newsz, tick_allocator_config_t* config);
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
  TICK_TOK_ASYNC,
  TICK_TOK_BREAK,
  TICK_TOK_CASE,
  TICK_TOK_CATCH,
  TICK_TOK_CONTINUE,
  TICK_TOK_DEFAULT,
  TICK_TOK_DEFER,
  TICK_TOK_ELSE,
  TICK_TOK_EMBED_FILE,
  TICK_TOK_ENUM,
  TICK_TOK_ERRDEFER,
  TICK_TOK_FN,
  TICK_TOK_FOR,
  TICK_TOK_IF,
  TICK_TOK_IMPORT,
  TICK_TOK_LET,
  TICK_TOK_ALIGN,
  TICK_TOK_OR,
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
  TICK_TOK_LPAREN,      // (
  TICK_TOK_RPAREN,      // )
  TICK_TOK_LBRACE,      // {
  TICK_TOK_RBRACE,      // }
  TICK_TOK_LBRACKET,    // [
  TICK_TOK_RBRACKET,    // ]
  TICK_TOK_COMMA,       // ,
  TICK_TOK_SEMICOLON,   // ;
  TICK_TOK_COLON,       // :
  TICK_TOK_DOT,         // .
  TICK_TOK_QUESTION,    // ?
  TICK_TOK_UNDERSCORE,  // _
  TICK_TOK_AT,          // @

  // Operators
  TICK_TOK_PLUS,        // +
  TICK_TOK_MINUS,       // -
  TICK_TOK_STAR,        // *
  TICK_TOK_SLASH,       // /
  TICK_TOK_PERCENT,     // %
  TICK_TOK_AMPERSAND,   // &
  TICK_TOK_PIPE,        // |
  TICK_TOK_CARET,       // ^
  TICK_TOK_TILDE,       // ~
  TICK_TOK_BANG,        // !
  TICK_TOK_EQ,          // =
  TICK_TOK_LT,          // <
  TICK_TOK_GT,          // >
  TICK_TOK_PLUS_PIPE,   // +| (saturating add)
  TICK_TOK_MINUS_PIPE,  // -| (saturating sub)
  TICK_TOK_STAR_PIPE,   // *| (saturating mul)
  TICK_TOK_SLASH_PIPE,  // /| (saturating div)
  TICK_TOK_BANG_EQ,     // !=
  TICK_TOK_EQ_EQ,       // ==
  TICK_TOK_LT_EQ,       // <=
  TICK_TOK_GT_EQ,       // >=
  TICK_TOK_LSHIFT,      // <<
  TICK_TOK_RSHIFT,      // >>

  // Comments
  TICK_TOK_COMMENT,     // #
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

typedef struct {
  tick_buf_t input;
  tick_alloc_t alloc;
  tick_buf_t errbuf;
  usz pos;
  usz line;
  usz col;
} tick_lex_t;

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
  TICK_AST_IF_STMT,
  TICK_AST_FOR_STMT,
  TICK_AST_SWITCH_STMT,
  TICK_AST_BREAK_STMT,
  TICK_AST_CONTINUE_STMT,
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
  TICK_AST_FIELD_ACCESS_EXPR,
  TICK_AST_INDEX_EXPR,
  TICK_AST_STRUCT_INIT_EXPR,
  TICK_AST_ARRAY_INIT_EXPR,
  TICK_AST_CAST_EXPR,
  TICK_AST_IDENTIFIER_EXPR,
  TICK_AST_ASYNC_EXPR,
  TICK_AST_EMBED_FILE_EXPR,
  TICK_AST_TYPE_NAMED,
  TICK_AST_TYPE_ARRAY,
  TICK_AST_TYPE_SLICE,
  TICK_AST_TYPE_POINTER,
  TICK_AST_TYPE_OPTIONAL,
  TICK_AST_TYPE_ERROR_UNION,
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

// AST node structure
struct tick_ast_node_s {
  tick_ast_node_kind_t kind;
  tick_ast_loc_t loc;
  tick_ast_node_t* next;  // For building linked lists during parsing
  union {
    struct {
      uint64_t value;
    } literal;
    struct {
      tick_buf_t name;
      tick_ast_node_t* decls;
    } module;
    struct {
      tick_buf_t path;
      bool is_pub;
      bool is_volatile;
    } import;
    struct {
      tick_buf_t name;
      tick_buf_t path;
      bool is_pub;
    } import_decl;
    struct {
      tick_buf_t name;
      tick_ast_node_t* type;
      tick_ast_node_t* init;
      tick_qualifier_flags_t quals;
    } decl;
    struct {
      tick_buf_t name;
      tick_ast_node_t* params;
      tick_ast_node_t* return_type;
      tick_ast_node_t* body;
      bool is_pub;
    } function_decl;
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
      tick_ast_node_t* tag_type;  // NULL for automatic tag
      tick_ast_node_t* fields;  // Linked list of TICK_AST_FIELD nodes
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
    } binary_expr;
    struct {
      tick_unop_t op;
      tick_ast_node_t* operand;
    } unary_expr;
    struct {
      tick_ast_node_t* callee;
      tick_ast_node_t* args;
    } call_expr;
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
      tick_ast_node_t* fields;  // Linked list of TICK_AST_STRUCT_INIT_FIELD nodes
    } struct_init_expr;
    struct {
      tick_ast_node_t* elements;
    } array_init_expr;
    struct {
      tick_ast_node_t* type;
      tick_ast_node_t* expr;
    } cast_expr;
    struct {
      tick_buf_t name;
    } identifier_expr;
    struct {
      tick_ast_node_t* call;  // Function call expression
      tick_ast_node_t* frame;  // Frame expression
    } async_expr;
    struct {
      tick_buf_t path;
    } embed_file_expr;
    struct {
      tick_ast_node_t* stmt;
    } defer_stmt;
    struct {
      tick_ast_node_t* stmt;
    } errdefer_stmt;
    struct {
      tick_ast_node_t* handle;
    } resume_stmt;
    struct {
      tick_ast_node_t* call;  // Function call to try
      tick_buf_t capture_name;  // |name| capture
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
tick_cli_result_t tick_driver_parse_args(tick_driver_args_t* args, int argc, char** argv);
tick_err_t tick_driver_read_file(tick_alloc_t* alloc, tick_buf_t* output, tick_buf_t path);

// Allocator functions
tick_alloc_t tick_allocator_libc(void);
tick_alloc_t tick_allocator_seglist(tick_alloc_seglist_t* seglist, tick_alloc_t backing);

// Lexer functions
void tick_lex_init(tick_lex_t* lex, tick_buf_t input, tick_alloc_t alloc, tick_buf_t errbuf);
void tick_lex_next(tick_lex_t* lex, tick_tok_t* tok);
const char* tick_tok_format(tick_tok_t* tok, char* buf, usz buf_sz);

// Parser functions
void tick_parse_init(tick_parse_t* parse, tick_alloc_t alloc, tick_buf_t errbuf);
tick_err_t tick_parse_tok(tick_parse_t* parse, tick_tok_t* tok);

// AST functions
const char* tick_ast_kind_str(tick_ast_node_kind_t kind);
tick_err_t tick_ast_analyze(tick_ast_t* ast, tick_buf_t errbuf);
tick_err_t tick_ast_lower(tick_ast_t* ast, tick_buf_t errbuf);

// Output functions
tick_err_t tick_output_format_name(tick_buf_t output_path, tick_path_t* output_h, tick_path_t* output_c);
tick_writer_t tick_file_writer(FILE* f);

// Codegen functions
tick_err_t tick_codegen(tick_ast_t* ast, tick_writer_t writer_h, tick_writer_t writer_c);
