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
  TICK_TOK_IDENT,
  TICK_TOK_NUMBER,
  // Add more token types as needed
} tick_tok_type_t;

typedef struct {
  tick_tok_type_t type;
  tick_buf_t text;
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
