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
  tick_err_t (*realloc)(void* ctx, tick_buf_t* buf, usz newsz, tick_allocator_config_t* config);
  void* ctx;
} tick_alloc_t;

typedef struct tick_alloc_seglist_t {
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
  TICK_TOK_EOF,
  TICK_TOK_ERR,

  // Identifier
  TICK_TOK_IDENT,

  // Literals
  TICK_TOK_UINT_LITERAL,
  TICK_TOK_INT_LITERAL,
  TICK_TOK_STRING_LITERAL,
  TICK_TOK_BOOL_LITERAL,

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
  TICK_TOK_IN,
  TICK_TOK_LET,
  TICK_TOK_OR,
  TICK_TOK_PACKED,
  TICK_TOK_PUB,
  TICK_TOK_RESUME,
  TICK_TOK_RETURN,
  TICK_TOK_STRUCT,
  TICK_TOK_SUSPEND,
  TICK_TOK_SWITCH,
  TICK_TOK_TRY,
  TICK_TOK_UNION,
  TICK_TOK_VAR,
  TICK_TOK_VOLATILE,
  TICK_TOK_WHILE,

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
  TICK_TOK_DOT_DOT,     // ..
  TICK_TOK_QUESTION,    // ?

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
  TICK_TOK_PLUS_EQ,     // +=
  TICK_TOK_MINUS_EQ,    // -=
  TICK_TOK_STAR_EQ,     // *=
  TICK_TOK_SLASH_EQ,    // /=
  TICK_TOK_PLUS_PIPE,   // +| (saturating add)
  TICK_TOK_MINUS_PIPE,  // -| (saturating sub)
  TICK_TOK_STAR_PIPE,   // *| (saturating mul)
  TICK_TOK_SLASH_PIPE,  // /| (saturating div)
  TICK_TOK_PLUS_PERCENT,   // +% (modulo add)
  TICK_TOK_MINUS_PERCENT,  // -% (modulo sub)
  TICK_TOK_STAR_PERCENT,   // *% (modulo mul)
  TICK_TOK_SLASH_PERCENT,  // /% (modulo div)
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

typedef struct tick_ast_node_t tick_ast_node_t;
typedef struct {
  tick_ast_node_t* root;
} tick_ast_t;

typedef struct {
  tick_alloc_t alloc;
  tick_buf_t errbuf;
  tick_ast_t root;
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
tick_err_t tick_ast_analyze(tick_ast_t* ast, tick_buf_t errbuf);
tick_err_t tick_ast_lower(tick_ast_t* ast, tick_buf_t errbuf);

// Output functions
tick_err_t tick_output_format_name(tick_buf_t output_path, tick_path_t* output_h, tick_path_t* output_c);
tick_writer_t tick_file_writer(FILE* f);

// Codegen functions
tick_err_t tick_codegen(tick_ast_t* ast, tick_writer_t writer_h, tick_writer_t writer_c);
