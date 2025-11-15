#include "tick.h"

// tick compiler
// ./tick emitc input.tick -o output
int main(int argc, char** argv) {
  // cli parse
  tick_driver_args_t args;
  switch (tick_driver_parse_args(&args, argc, argv)) {
    case TICK_CLI_HELP:
    case TICK_CLI_ERR:
      TICK_USER_LOG(TICK_HELP);
      exit(1);
    case TICK_CLI_OK:
      break;
  }

  // load input file into memory
  tick_alloc_t libc_alloc = tick_allocator_libc();
  tick_buf_t input;
  if (tick_driver_read_file(&libc_alloc, &input, args.emitc.input) != TICK_OK) {
    TICK_USER_LOGE(TICK_READ_FILE_ERROR);
    exit(1);
  }

  // setup output early to fail early
  tick_path_t output_h;
  tick_path_t output_c;
  if (tick_output_format_name(args.emitc.output, &output_h, &output_c) !=
      TICK_OK) {
    TICK_USER_LOGE("output file path too long");
    exit(1);
  }
  FILE* f_h = fopen((const char*)output_h.buf, "wb");
  if (f_h == NULL) {
    TICK_USER_LOGE("could not open output file %s", (const char*)output_h.buf);
    exit(1);
  }
  FILE* f_c = fopen((const char*)output_c.buf, "wb");
  if (f_c == NULL) {
    TICK_USER_LOGE("could not open output file %s", (const char*)output_c.buf);
    fclose(f_h);
    exit(1);
  }

  // setup segmented list allocators
  tick_alloc_seglist_t str_pool_alloc_;
  tick_alloc_seglist_t ast_pool_alloc_;
  tick_alloc_t str_pool_alloc =
      tick_allocator_seglist(&str_pool_alloc_, libc_alloc);
  tick_alloc_t ast_pool_alloc =
      tick_allocator_seglist(&ast_pool_alloc_, libc_alloc);

  // setup lexer + parser
  u8 errbuf_storage[4096];
  tick_buf_t errbuf = {errbuf_storage, sizeof(errbuf_storage)};
  tick_lex_t lex;
  tick_lex_init(&lex, input, str_pool_alloc, errbuf);
  tick_parse_t parse;
  tick_parse_init(&parse, ast_pool_alloc, errbuf);

#define COMPILE_ERR()                 \
  do {                                \
    TICK_USER_LOGE("%s", errbuf.buf); \
    exit(1);                          \
  } while (0)

  // tokenize + parse
  tick_tok_t tok = {0};
  do {
    tick_lex_next(&lex, &tok);

    char tok_buf[256];
    UNUSED(tok_buf);
    PLOG("[lex] %s", tick_tok_format(&tok, tok_buf, sizeof(tok_buf)));

    if (tok.type == TICK_TOK_ERR) COMPILE_ERR();

    if (tick_parse_tok(&parse, &tok) != TICK_OK) COMPILE_ERR();
    if (parse.has_error) COMPILE_ERR();
  } while (tok.type != TICK_TOK_EOF);
  tick_ast_t root = parse.root;
  if (root.root == NULL) {
    snprintf((char*)errbuf.buf, errbuf.sz, "Module failed to parse");
    COMPILE_ERR();
  }

  // analyze, typecheck
  if (tick_ast_analyze(&root, parse.alloc, errbuf) != TICK_OK) COMPILE_ERR();

#undef COMPILE_ERR

  // codegen
  tick_writer_t out_writer_h = tick_file_writer(f_h);
  tick_writer_t out_writer_c = tick_file_writer(f_c);

  // Extract basename from header path for #include directive
  const char* header_basename = (const char*)output_h.buf;
  for (const char* p = (const char*)output_h.buf; *p; p++) {
    if (*p == '/' || *p == '\\') {
      header_basename = p + 1;
    }
  }

  if (tick_codegen(&root, (const char*)args.emitc.input.buf, header_basename,
                   out_writer_h, out_writer_c) != TICK_OK) {
    TICK_USER_LOGE("failed to write to output files");
    exit(1);
  }
  fclose(f_h);
  fclose(f_c);

  exit(0);
}
