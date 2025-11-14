#include "tick.h"

#include <string.h>

// ============================================================================
// Driver functions
// ============================================================================

tick_cli_result_t tick_driver_parse_args(tick_driver_args_t* args, int argc,
                                         char** argv) {
  // Need at least: tick emitc <input> -o <output>
  if (argc < 2) {
    return TICK_CLI_ERR;
  }

  // Parse command
  if (strcmp(argv[1], "emitc") != 0) {
    return TICK_CLI_ERR;
  }
  args->cmd = TICK_CMD_EMITC;

  // Initialize to NULL to detect missing arguments
  char* input_arg = NULL;
  char* output_arg = NULL;

  // Parse remaining arguments
  for (int i = 2; i < argc; i++) {
    // Check for help flag
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      return TICK_CLI_HELP;
    }

    // Check for -o flag
    if (strcmp(argv[i], "-o") == 0) {
      i++;  // Move to next argument for the value
      if (i >= argc) {
        return TICK_CLI_ERR;  // -o without a value
      }
      output_arg = argv[i];
      continue;
    }

    // Check for unrecognized flag
    if (argv[i][0] == '-') {
      return TICK_CLI_ERR;  // Unrecognized flag
    }

    // Non-flag argument is the input file
    if (input_arg == NULL) {
      input_arg = argv[i];
    } else {
      return TICK_CLI_ERR;  // Multiple input files not supported
    }
  }

  // Validate required arguments
  if (input_arg == NULL || output_arg == NULL) {
    return TICK_CLI_ERR;
  }

  // Set parsed values
  args->emitc.input.buf = (u8*)input_arg;
  args->emitc.input.sz = strlen(input_arg);
  args->emitc.output.buf = (u8*)output_arg;
  args->emitc.output.sz = strlen(output_arg);

  return TICK_CLI_OK;
}

tick_err_t tick_driver_read_file(tick_alloc_t* alloc, tick_buf_t* output,
                                 tick_buf_t path) {
  // Check if path is already null-terminated
  const char* path_str;
  char path_buf[PATH_MAX];
  if (path.buf[path.sz] == '\0') {
    path_str = (const char*)path.buf;
  } else {
    if (path.sz >= PATH_MAX) {
      return TICK_ERR;
    }
    memcpy(path_buf, path.buf, path.sz);
    path_buf[path.sz] = '\0';
    path_str = path_buf;
  }

  FILE* f = fopen(path_str, "rb");
  if (f == NULL) {
    return TICK_ERR;
  }

  fseek(f, 0, SEEK_END);
  long file_size = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (file_size < 0) {
    fclose(f);
    return TICK_ERR;
  }

  tick_buf_t buf = {NULL, 0};
  if (alloc->realloc(alloc->ctx, &buf, (usz)file_size, 0) != TICK_OK) {
    fclose(f);
    return TICK_ERR;
  }

  usz bytes_read = fread(buf.buf, 1, (usz)file_size, f);
  fclose(f);
  if (bytes_read != (usz)file_size) {
    CHECK_OK(alloc->realloc(alloc->ctx, &buf, 0, 0));
    return TICK_ERR;
  }

  output->buf = buf.buf;
  output->sz = (usz)file_size;
  return TICK_OK;
}

// ============================================================================
// Allocator functions
// ============================================================================

static tick_err_t tick_allocator_libc_realloc(void* ctx, tick_buf_t* buf,
                                              usz newsz,
                                              tick_allocator_config_t* config) {
  UNUSED(ctx);

  // Use default config if NULL/0
  tick_allocator_config_t default_config = {.flags = 0, .alignment2 = 0};
  if (config == NULL || (usz)config == 0) {
    config = &default_config;
  }

  // Handle alignment
  usz alignment = 1;
  if (config->alignment2 > 0) {
    alignment = (usz)1 << config->alignment2;
  }

  void* ptr = NULL;
  if (newsz > 0) {
    // Use realloc directly if no special alignment and existing buffer
    // This allows potential in-place growth without copying
    if (alignment <= 1 && buf->buf != NULL) {
      ptr = realloc(buf->buf, newsz);
      if (ptr == NULL) {
        return TICK_ERR;
      }

      // Zero memory if requested (only new bytes)
      if (config->flags & TICK_ALLOCATOR_ZEROMEM) {
        if (buf->sz < newsz) {
          memset((u8*)ptr + buf->sz, 0, newsz - buf->sz);
        }
      }
    } else {
      // Use aligned_alloc if alignment is needed, or malloc for new allocation
      if (alignment > 1) {
        // aligned_alloc requires size to be multiple of alignment
        usz aligned_size = ALIGNF(newsz, alignment);
        ptr = aligned_alloc(alignment, aligned_size);
      } else {
        ptr = malloc(newsz);
      }

      if (ptr == NULL) {
        return TICK_ERR;
      }

      // Copy old data if resizing
      if (buf->buf != NULL && buf->sz > 0) {
        usz copy_size = buf->sz < newsz ? buf->sz : newsz;
        memcpy(ptr, buf->buf, copy_size);
        free(buf->buf);
      }

      // Zero memory if requested
      if (config->flags & TICK_ALLOCATOR_ZEROMEM) {
        if (buf->sz < newsz) {
          memset((u8*)ptr + buf->sz, 0, newsz - buf->sz);
        }
      }
    }
  } else {
    // Free memory
    if (buf->buf != NULL) {
      free(buf->buf);
    }
  }

  buf->buf = (u8*)ptr;
  buf->sz = newsz;
  return TICK_OK;
}

tick_alloc_t tick_allocator_libc(void) {
  tick_alloc_t alloc = {
      .realloc = tick_allocator_libc_realloc,
      .ctx = NULL,
  };
  return alloc;
}

typedef struct tick_alloc_seglist_segment_t {
  struct tick_alloc_seglist_segment_t* next;
  usz size;
  usz used;
  u8 data[];
} tick_alloc_seglist_segment_t;

#define SEGLIST_SEGMENT_SIZE (64 * 1024)  // 64KB segments

static tick_err_t tick_allocator_seglist_realloc(
    void* ctx, tick_buf_t* buf, usz newsz, tick_allocator_config_t* config) {
  tick_alloc_seglist_t* seglist = (tick_alloc_seglist_t*)ctx;

  // Only support allocation, not reallocation
  if (buf->buf != NULL) {
    return TICK_ERR;
  }

  // free is a no-op
  if (newsz == 0) {
    return TICK_OK;
  }

  // Use default config if NULL/0
  tick_allocator_config_t default_config = {.flags = 0, .alignment2 = 0};
  if (config == NULL || (usz)config == 0) {
    config = &default_config;
  }

  // Get current segment
  tick_alloc_seglist_segment_t* seg =
      (tick_alloc_seglist_segment_t*)seglist->segments;

  // Handle alignment - default to max_align_t for proper platform alignment
  usz alignment = _Alignof(max_align_t);
  if (config->alignment2 > 0) {
    usz requested_alignment = (usz)1 << config->alignment2;
    // Use the larger of the two alignments
    if (requested_alignment > alignment) {
      alignment = requested_alignment;
    }
  }

  // Try to allocate from current segment
  if (seg != NULL) {
    usz aligned_used = ALIGNF(seg->used, alignment);
    if (aligned_used + newsz <= seg->size) {
      buf->buf = seg->data + aligned_used;
      buf->sz = newsz;
      seg->used = aligned_used + newsz;

      if (config->flags & TICK_ALLOCATOR_ZEROMEM) {
        memset(buf->buf, 0, newsz);
      }

      seglist->total_allocated += newsz;
      return TICK_OK;
    }
  }

  // Need a new segment
  usz seg_size = SEGLIST_SEGMENT_SIZE;
  if (newsz > seg_size) {
    seg_size = newsz;
  }

  tick_buf_t seg_buf = {NULL, 0};
  tick_allocator_config_t seg_config = {.flags = 0, .alignment2 = 0};
  usz total_size = sizeof(tick_alloc_seglist_segment_t) + seg_size;
  if (seglist->backing.realloc(seglist->backing.ctx, &seg_buf, total_size,
                               &seg_config) != TICK_OK) {
    return TICK_ERR;
  }

  tick_alloc_seglist_segment_t* new_seg =
      (tick_alloc_seglist_segment_t*)seg_buf.buf;
  new_seg->next = seg;
  new_seg->size = seg_size;
  new_seg->used = newsz;
  seglist->segments = new_seg;

  buf->buf = new_seg->data;
  buf->sz = newsz;

  if (config->flags & TICK_ALLOCATOR_ZEROMEM) {
    memset(buf->buf, 0, newsz);
  }

  seglist->total_allocated += newsz;
  return TICK_OK;
}

tick_alloc_t tick_allocator_seglist(tick_alloc_seglist_t* seglist,
                                    tick_alloc_t backing) {
  seglist->backing = backing;
  seglist->segments = NULL;
  seglist->total_allocated = 0;

  tick_alloc_t alloc = {
      .realloc = tick_allocator_seglist_realloc,
      .ctx = seglist,
  };
  return alloc;
}

// ============================================================================
// Parser functions
// ============================================================================

// Forward declarations for Lemon-generated parser
void* ParseAlloc(void* (*mallocProc)(size_t));
void Parse(void* yyp, int yymajor, tick_tok_t yyminor, tick_parse_t* parse);
void ParseFree(void* p, void (*freeProc)(void*));
void ParseTrace(FILE* stream, char* zPrefix);

void tick_parse_init(tick_parse_t* parse, tick_alloc_t alloc,
                     tick_buf_t errbuf) {
  parse->alloc = alloc;
  parse->errbuf = errbuf;
  parse->root.root = NULL;
  parse->has_error = false;

  // Allocate Lemon parser
  parse->lemon_parser = ParseAlloc(malloc);

#ifdef TICK_DEBUG_PARSE
  // Enable parser tracing in debug mode
  ParseTrace(stderr, "[parser] ");
#endif

  // Null-terminate error buffer
  if (errbuf.sz > 0) {
    errbuf.buf[0] = '\0';
  }
}

tick_err_t tick_parse_tok(tick_parse_t* parse, tick_tok_t* tok) {
  if (tok->type == TICK_TOK_COMMENT) {
    return TICK_OK;
  }

  // If we hit EOF, finalize the parse
  if (tok->type == TICK_TOK_EOF) {
    tick_tok_t eof_tok = *tok;  // Copy token
    Parse(parse->lemon_parser, 0, eof_tok, parse);
    // Free the parser
    ParseFree(parse->lemon_parser, free);
    parse->lemon_parser = NULL;
  } else {
    // Feed token to Lemon parser (pass by value, not pointer)
    Parse(parse->lemon_parser, tok->type, *tok, parse);
  }
  return TICK_OK;
}

// ============================================================================
// AST functions
// ============================================================================

const char* tick_ast_kind_str(tick_ast_node_kind_t kind) {
  switch (kind) {
    case TICK_AST_LITERAL:
      return "LITERAL";
    case TICK_AST_ERROR:
      return "ERROR";
    case TICK_AST_MODULE:
      return "MODULE";
    case TICK_AST_IMPORT_DECL:
      return "IMPORT_DECL";
    case TICK_AST_DECL:
      return "DECL";
    case TICK_AST_FUNCTION_DECL:
      return "FUNCTION_DECL";
    case TICK_AST_RETURN_STMT:
      return "RETURN_STMT";
    case TICK_AST_BLOCK_STMT:
      return "BLOCK_STMT";
    case TICK_AST_EXPR_STMT:
      return "EXPR_STMT";
    case TICK_AST_BINARY_EXPR:
      return "BINARY_EXPR";
    case TICK_AST_IDENTIFIER_EXPR:
      return "IDENTIFIER_EXPR";
    case TICK_AST_TYPE_NAMED:
      return "TYPE_NAMED";
    default:
      return "UNKNOWN";
  }
}

// Initialize a single-node list
void tick_ast_list_init(tick_ast_node_t* node) {
  node->next = NULL;
  node->prev = NULL;
  node->tail = node;  // Single node is its own tail
}

// Append a node to a list, maintaining doubly-linked structure
// Returns the head of the list (unchanged)
tick_ast_node_t* tick_ast_list_append(tick_ast_node_t* head,
                                      tick_ast_node_t* node) {
  if (head == NULL) {
    // Empty list: initialize the node as a single-element list
    tick_ast_list_init(node);
    return node;
  }

  // Initialize the new node
  tick_ast_list_init(node);

  // Append to the tail of the existing list
  tick_ast_node_t* tail = head->tail;
  tail->next = node;
  node->prev = tail;
  head->tail = node;  // Update head's tail pointer

  return head;
}

// ============================================================================
// Output functions
// ============================================================================

tick_err_t tick_output_format_name(tick_buf_t output_path,
                                   tick_path_t* output_h,
                                   tick_path_t* output_c) {
  // Check if output path fits (need room for ".h" or ".c")
  if (output_path.sz + 2 >= PATH_MAX) {
    return TICK_ERR;
  }

  // Format .h file path
  memcpy(output_h->buf, output_path.buf, output_path.sz);
  output_h->buf[output_path.sz] = '.';
  output_h->buf[output_path.sz + 1] = 'h';
  output_h->buf[output_path.sz + 2] = '\0';
  output_h->sz = output_path.sz + 2;

  // Format .c file path
  memcpy(output_c->buf, output_path.buf, output_path.sz);
  output_c->buf[output_path.sz] = '.';
  output_c->buf[output_path.sz + 1] = 'c';
  output_c->buf[output_path.sz + 2] = '\0';
  output_c->sz = output_path.sz + 2;

  return TICK_OK;
}

static tick_err_t tick_file_writer_write(void* ctx, tick_buf_t* buf) {
  FILE* f = (FILE*)ctx;
  usz written = fwrite(buf->buf, 1, buf->sz, f);
  if (written != buf->sz) {
    return TICK_ERR;
  }
  return TICK_OK;
}

tick_writer_t tick_file_writer(FILE* f) {
  tick_writer_t writer = {
      .write = tick_file_writer_write,
      .ctx = f,
  };
  return writer;
}

// ============================================================================
// AST Helper Functions (Canonical Query API)
// ============================================================================

// Check if a node was compiler-generated (not from source)
bool tick_node_is_synthetic(tick_ast_node_t* node) {
  return node && (node->node_flags & TICK_NODE_FLAG_SYNTHETIC);
}

// Check if a node has completed analysis
bool tick_node_is_analyzed(tick_ast_node_t* node) {
  return node && (node->node_flags & TICK_NODE_FLAG_ANALYZED);
}

// Check if a node has completed lowering
bool tick_node_is_lowered(tick_ast_node_t* node) {
  return node && (node->node_flags & TICK_NODE_FLAG_LOWERED);
}

// Check if a node is a compiler temporary variable
bool tick_node_is_temporary(tick_ast_node_t* node) {
  if (!node) return false;
  // Check explicit flag first, fall back to tmpid check for compatibility
  if (node->node_flags & TICK_NODE_FLAG_TEMPORARY) return true;
  if (node->kind == TICK_AST_DECL && node->decl.tmpid > 0) return true;
  return false;
}

// Check if a type is unresolved (still TICK_TYPE_UNKNOWN)
bool tick_type_is_unresolved(tick_ast_node_t* type) {
  return type && type->kind == TICK_AST_TYPE_NAMED &&
         type->type_named.builtin_type == TICK_TYPE_UNKNOWN;
}

// Check if a type is fully resolved
bool tick_type_is_resolved(tick_ast_node_t* type) {
  if (!type) return false;
  if (type->kind == TICK_AST_TYPE_NAMED) {
    return type->type_named.builtin_type != TICK_TYPE_UNKNOWN;
  }
  return true;
}

// Check if a type is a named type
bool tick_type_is_named(tick_ast_node_t* type) {
  return type && type->kind == TICK_AST_TYPE_NAMED;
}

// Check if a type is a pointer type
bool tick_type_is_pointer(tick_ast_node_t* type) {
  return type && type->kind == TICK_AST_TYPE_POINTER;
}

// Check if a type is a user-defined type (struct/enum/union)
bool tick_type_is_user_defined(tick_ast_node_t* type) {
  return type && type->kind == TICK_AST_TYPE_NAMED &&
         type->type_named.builtin_type == TICK_TYPE_USER_DEFINED;
}

// Check if a type is a builtin type
bool tick_type_is_builtin(tick_ast_node_t* type) {
  return type && type->kind == TICK_AST_TYPE_NAMED &&
         type->type_named.builtin_type != TICK_TYPE_UNKNOWN &&
         type->type_named.builtin_type != TICK_TYPE_USER_DEFINED;
}

// Check if a type is an integer type (signed or unsigned)
bool tick_type_is_integer(tick_ast_node_t* type) {
  if (!tick_type_is_named(type)) return false;
  tick_builtin_type_t bt = type->type_named.builtin_type;
  return (bt >= TICK_TYPE_I8 && bt <= TICK_TYPE_ISZ) ||
         (bt >= TICK_TYPE_U8 && bt <= TICK_TYPE_USZ);
}

// Check if a type is a signed integer type
bool tick_type_is_signed(tick_ast_node_t* type) {
  if (!tick_type_is_named(type)) return false;
  tick_builtin_type_t bt = type->type_named.builtin_type;
  return bt >= TICK_TYPE_I8 && bt <= TICK_TYPE_ISZ;
}

// Check if a type is an unsigned integer type
bool tick_type_is_unsigned(tick_ast_node_t* type) {
  if (!tick_type_is_named(type)) return false;
  tick_builtin_type_t bt = type->type_named.builtin_type;
  return bt >= TICK_TYPE_U8 && bt <= TICK_TYPE_USZ;
}

// Check if a type is numeric (integer type - floats not yet supported)
bool tick_type_is_numeric(tick_ast_node_t* type) {
  return tick_type_is_integer(type);
}

// Get the builtin type enum from a type node
tick_builtin_type_t tick_type_get_builtin(tick_ast_node_t* type) {
  if (!type || type->kind != TICK_AST_TYPE_NAMED) {
    return TICK_TYPE_UNKNOWN;
  }
  return type->type_named.builtin_type;
}

// Get the pointee type from a pointer type
tick_ast_node_t* tick_type_get_pointee(tick_ast_node_t* type) {
  if (!type || type->kind != TICK_AST_TYPE_POINTER) return NULL;
  return type->type_pointer.pointee_type;
}

// Get the element type from an array type
tick_ast_node_t* tick_type_get_element(tick_ast_node_t* type) {
  if (!type || type->kind != TICK_AST_TYPE_ARRAY) return NULL;
  return type->type_array.element_type;
}

// Check if two types are structurally equal
bool tick_types_equal(tick_ast_node_t* t1, tick_ast_node_t* t2) {
  if (!t1 || !t2) return t1 == t2;
  if (t1->kind != t2->kind) return false;

  switch (t1->kind) {
    case TICK_AST_TYPE_NAMED:
      return t1->type_named.builtin_type == t2->type_named.builtin_type &&
             t1->type_named.type_entry == t2->type_named.type_entry;

    case TICK_AST_TYPE_POINTER:
      return tick_types_equal(t1->type_pointer.pointee_type,
                              t2->type_pointer.pointee_type);

    case TICK_AST_TYPE_ARRAY:
      return tick_types_equal(t1->type_array.element_type,
                              t2->type_array.element_type);

    default:
      return t1 == t2;
  }
}
