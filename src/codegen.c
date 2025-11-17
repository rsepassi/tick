// C Code Generation
//
// This module generates C code from a type-analyzed and lowered Tick AST.
//
// IMPORTANT: See tick.h "AST Pipeline and Lowering Target" for the complete
// specification of what codegen expects from the analysis and lowering passes.
// Codegen must be fast and dumb.

#include <stdarg.h>
#include <string.h>

#include "codegen_helpers.h"
#include "codegen_runtime_tables.h"
#include "tick.h"
#include "tick_runtime_embedded.h"

// ============================================================================
// Codegen Debug Logging
// ============================================================================

#ifdef TICK_DEBUG_CODEGEN
#define CLOG DLOG
#else
#define CLOG(fmt, ...) (void)(0)
#endif

// Codegen context for tracking state during generation
typedef struct {
  tick_writer_t* writer;
  const char* source_filename;  // For #line directives
  usz last_line;  // Track last emitted line to avoid redundant #line directives
} codegen_ctx_t;

// Forward declarations
static tick_err_t codegen_type(tick_ast_node_t* type, tick_writer_t* w);

// ============================================================================
// Writer Helpers
// ============================================================================

static tick_err_t write_str(tick_writer_t* w, const char* str) {
  tick_buf_t buf = {(u8*)str, strlen(str)};
#ifdef TICK_DEBUG_CODEGEN
  fprintf(stderr, "%s", str);
#endif
  return w->write(w->ctx, &buf);
}

static tick_err_t write_fmt(tick_writer_t* w, const char* fmt, ...) {
  char buf[4096];
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  if (len < 0 || len >= (int)sizeof(buf)) {
    return TICK_ERR;
  }

#ifdef TICK_DEBUG_CODEGEN
  fprintf(stderr, "%s", buf);
#endif

  tick_buf_t tbuf = {(u8*)buf, (usz)len};
  return w->write(w->ctx, &tbuf);
}

static tick_err_t write_indent(tick_writer_t* w, int indent) {
  for (int i = 0; i < indent; i++) {
    if (write_str(w, "  ") != TICK_OK) {
      return TICK_ERR;
    }
  }
  return TICK_OK;
}

static tick_err_t write_ident(tick_writer_t* w, tick_buf_t name) {
  tick_buf_t buf = {name.buf, name.sz};
#ifdef TICK_DEBUG_CODEGEN
  fprintf(stderr, "%.*s", (int)name.sz, (char*)name.buf);
#endif
  return w->write(w->ctx, &buf);
}

// Write a variable name with appropriate prefix based on tmpid and qualifiers
// user variable: __u_{name} (unless extern or pub)
// compiler temporary: __tmp{N}
// extern: no prefix (links to external C symbol)
// pub: no prefix (exported for C code to use)
static tick_err_t write_decl_name(tick_writer_t* w, tick_ast_node_t* decl) {
  if (tick_node_is_temporary(decl)) {
    // Compiler-generated temporary
    return write_fmt(w, "__tmp%u", decl->decl.tmpid);
  } else {
    // User-named variable
    bool is_extern = decl->decl.quals.is_extern;
    bool is_pub = decl->decl.quals.is_pub;

    // Skip prefix for extern and pub declarations so they can link with C code
    if (!is_extern && !is_pub) {
      CHECK_OK(write_str(w, "__u_"));
    }
    return write_ident(w, decl->decl.name);
  }
}

// Write array declarator suffix (the [size] part after the name)
static tick_err_t write_array_suffix(tick_writer_t* w, tick_ast_node_t* type) {
  if (!type) return TICK_OK;

  if (type->kind == TICK_AST_TYPE_ARRAY) {
    // Emit array size
    if (type->type_array.size) {
      CHECK(type->type_array.size->kind == TICK_AST_LITERAL,
            "array size must be literal (no VLAs)");
      CHECK_OK(write_str(w, "["));
      CHECK_OK(write_fmt(
          w, "%llu",
          (unsigned long long)type->type_array.size->literal.data.uint_value));
      CHECK_OK(write_str(w, "]"));
    }
  }

  return TICK_OK;
}

// Write a complete C declarator (type + name + array/pointer suffixes)
// Handles the complex C declarator syntax for:
// - Normal types: int name
// - Arrays: int name[10]
// - Pointer-to-array: int (*name)[10]
static tick_err_t write_c_declarator(tick_writer_t* w, tick_ast_node_t* type,
                                     tick_ast_node_t* decl) {
  // Detect pointer-to-array: *[N]T in Tick becomes (*name)[N] in C
  // This is necessary because C declarator precedence would parse
  // `int *x[10]` as array-of-pointers, not pointer-to-array
  bool is_ptr_to_array =
      type && type->kind == TICK_AST_TYPE_POINTER &&
      type->type_pointer.pointee_type &&
      type->type_pointer.pointee_type->kind == TICK_AST_TYPE_ARRAY;

  if (is_ptr_to_array) {
    // Emit: T (*name)[N]
    tick_ast_node_t* array_type = type->type_pointer.pointee_type;
    CHECK_OK(codegen_type(array_type->type_array.element_type, w));
    CHECK_OK(write_str(w, " (*"));
    CHECK_OK(write_decl_name(w, decl));
    CHECK_OK(write_str(w, ")"));
    CHECK_OK(write_array_suffix(w, array_type));
  } else {
    // Normal case: T name or T name[N]
    CHECK_OK(codegen_type(type, w));
    CHECK_OK(write_str(w, " "));
    CHECK_OK(write_decl_name(w, decl));
    CHECK_OK(write_array_suffix(w, type));
  }

  return TICK_OK;
}

// Write #line directive for source mapping
static tick_err_t write_line_directive(codegen_ctx_t* ctx, usz line) {
  // Skip if same as last line to avoid clutter
  if (line == ctx->last_line) {
    return TICK_OK;
  }
  ctx->last_line = line;

  return write_fmt(ctx->writer, "#line %zu \"%s\"\n", line,
                   ctx->source_filename);
}

// ============================================================================
// Type Generation
// ============================================================================

static tick_err_t codegen_type(tick_ast_node_t* type, tick_writer_t* w) {
  if (!type) {
    return write_str(w, "void");
  }

  switch (type->kind) {
    case TICK_AST_TYPE_NAMED: {
      // Map builtin types to C types using enum
      // Use the runtime aliases (i8, u32, etc.) for consistency with slice
      // types
      switch (type->type_named.builtin_type) {
        case TICK_TYPE_I8:
          return write_str(w, "i8");
        case TICK_TYPE_I16:
          return write_str(w, "i16");
        case TICK_TYPE_I32:
          return write_str(w, "i32");
        case TICK_TYPE_I64:
          return write_str(w, "i64");
        case TICK_TYPE_ISZ:
          return write_str(w, "isz");
        case TICK_TYPE_U8:
          return write_str(w, "u8");
        case TICK_TYPE_U16:
          return write_str(w, "u16");
        case TICK_TYPE_U32:
          return write_str(w, "u32");
        case TICK_TYPE_U64:
          return write_str(w, "u64");
        case TICK_TYPE_USZ:
          return write_str(w, "usz");
        case TICK_TYPE_BOOL:
          return write_str(w, "bool");
        case TICK_TYPE_VOID:
          return write_str(w, "void");
        case TICK_TYPE_USER_DEFINED: {
          // User-defined types - emit with __u_ prefix only if not pub
          tick_type_entry_t* type_entry = type->type_named.type_entry;
          CHECK(type_entry,
                "type_entry not set - analysis didn't resolve type");

          if (!type_entry->is_pub) {
            CHECK_OK(write_str(w, "__u_"));
          }
          return write_ident(w, type->type_named.name);
        }
        case TICK_TYPE_UNKNOWN:
          DLOG("Type not resolved: %.*s (line %zu:%zu)",
               (int)type->type_named.name.sz, type->type_named.name.buf,
               type->loc.line, type->loc.col);
          CHECK(0, "type not resolved - analysis pass missing?");
        default:
          CHECK(0, "unknown builtin type");
      }
    }

    case TICK_AST_TYPE_POINTER: {
      tick_ast_node_t* pointee = type->type_pointer.pointee_type;

      // Function pointer special handling:
      // In C, function pointer syntax requires the * to be inside parentheses
      // with the function signature: return_type (*)(params)
      // We can't just prepend * to the pointee type because that would give
      // us return_type *(params) which is syntactically incorrect.
      //
      // So when the pointee is TYPE_FUNCTION, we delegate to its emission
      // which produces the full function pointer syntax including the (*).
      //
      // Example: Tick *fn(i32) i32
      //          → AST: TYPE_POINTER(TYPE_FUNCTION)
      //          → C: int32_t (*)(int32_t)
      if (pointee && pointee->kind == TICK_AST_TYPE_FUNCTION) {
        // TYPE_FUNCTION emission already includes the (*) syntax
        return codegen_type(pointee, w);
      }

      // Normal pointer: emit pointee type followed by *
      // Example: Tick *i32 → C: int32_t *
      CHECK_OK(codegen_type(pointee, w));
      return write_str(w, "*");
    }

    case TICK_AST_TYPE_ARRAY: {
      // For arrays, only emit the element type
      // The array brackets need to go after the name in C syntax
      return codegen_type(type->type_array.element_type, w);
    }

    case TICK_AST_TYPE_FUNCTION: {
      // TYPE_FUNCTION emission for function pointer type syntax.
      //
      // IMPORTANT: This should only be called when TYPE_FUNCTION appears as the
      // pointee of TYPE_POINTER (i.e., for function pointer variables).
      // Bare TYPE_FUNCTION on declarations represents function declarations
      // and is handled by specialized emission paths
      // (codegen_function_signature, codegen_extern_function_decl), NOT by
      // codegen_type().
      //
      // This emits: return_type (*)(params)
      // The (*) is the function pointer syntax. TYPE_POINTER will NOT add
      // another * when this is the pointee (see TYPE_POINTER case below).
      //
      // Example: *fn(i32) i32 → TYPE_POINTER(TYPE_FUNCTION)
      //          → TYPE_POINTER sees TYPE_FUNCTION pointee
      //          → Calls this, which emits: int32_t (*)(int32_t)

      CHECK_OK(codegen_type(type->type_function.return_type, w));
      CHECK_OK(write_str(w, " (*)("));

      // Emit parameter types
      tick_ast_node_t* param = type->type_function.params;
      // In C, empty parameter list should be (void) not ()
      if (!param) {
        CHECK_OK(write_str(w, "void"));
      } else {
        bool first = true;
        while (param) {
          if (!first) {
            CHECK_OK(write_str(w, ", "));
          }
          first = false;
          CHECK_OK(codegen_type(param->decl.type, w));
          param = param->next;
        }
      }

      return write_str(w, ")");
    }

    case TICK_AST_TYPE_SLICE: {
      // Emit slice as generic tick_slice_t type
      // Type safety maintained by casting ptr to correct type at use site
      CHECK_OK(write_str(w, "tick_slice_t"));
      return TICK_OK;
    }

    case TICK_AST_TYPE_OPTIONAL:
      CHECK(0, "TICK_AST_TYPE_OPTIONAL should be lowered before codegen");

    case TICK_AST_TYPE_ERROR_UNION:
      CHECK(0, "TICK_AST_TYPE_ERROR_UNION should be lowered before codegen");

    default:
      CHECK(0, "unhandled type kind");
  }
}

// ============================================================================
// Expression Generation
// ============================================================================

// Forward declarations
static tick_err_t codegen_expr(tick_ast_node_t* expr, codegen_ctx_t* ctx);

static tick_err_t codegen_literal(tick_ast_node_t* node, codegen_ctx_t* ctx) {
  switch (node->literal.kind) {
    case TICK_LIT_UINT:
      return write_fmt(ctx->writer, "%llu",
                       (unsigned long long)node->literal.data.uint_value);

    case TICK_LIT_INT:
      return write_fmt(ctx->writer, "%lld",
                       (long long)node->literal.data.int_value);

    case TICK_LIT_STRING: {
      // Emit string literal as C string literal (arrays, not slices)
      // String literals are now [N]u8 arrays, not []u8 slices
      tick_buf_t str = node->literal.data.string_value;
      usz str_len = str.sz;

      // Emit just the string (no cast needed for array initialization)
      CHECK_OK(write_str(ctx->writer, "\""));

      // Emit escaped string
      for (usz i = 0; i < str_len; i++) {
        char c = str.buf[i];
        switch (c) {
          case '\n':
            CHECK_OK(write_str(ctx->writer, "\\n"));
            break;
          case '\r':
            CHECK_OK(write_str(ctx->writer, "\\r"));
            break;
          case '\t':
            CHECK_OK(write_str(ctx->writer, "\\t"));
            break;
          case '\\':
            CHECK_OK(write_str(ctx->writer, "\\\\"));
            break;
          case '"':
            CHECK_OK(write_str(ctx->writer, "\\\""));
            break;
          case '\0':
            CHECK_OK(write_str(ctx->writer, "\\0"));
            break;
          default:
            if (c >= 32 && c <= 126) {
              CHECK_OK(write_fmt(ctx->writer, "%c", c));
            } else {
              CHECK_OK(write_fmt(ctx->writer, "\\x%02x", (unsigned char)c));
            }
            break;
        }
      }

      CHECK_OK(write_str(ctx->writer, "\""));
      return TICK_OK;
    }

    default:
      CHECK(0, "unhandled literal kind");
  }
}

static tick_err_t codegen_identifier(tick_ast_node_t* node,
                                     codegen_ctx_t* ctx) {
  // Check if this is an AT builtin
  if (node->identifier_expr.at_builtin != TICK_AT_BUILTIN_UNKNOWN) {
    switch (node->identifier_expr.at_builtin) {
      case TICK_AT_BUILTIN_DBG:
        return write_str(ctx->writer, "tick_debug_log");
      case TICK_AT_BUILTIN_PANIC:
        return write_str(ctx->writer, "tick_panic");
      case TICK_AT_BUILTIN_CHECK_DEREF:
        return write_str(ctx->writer, "tick_check_deref");
      default:
        CHECK(0, "unknown AT builtin enum value");
    }
  }

  // Check if this is a compiler-generated temporary
  if (node->identifier_expr.symbol && node->identifier_expr.symbol->decl) {
    tick_ast_node_t* decl = node->identifier_expr.symbol->decl;
    if (tick_node_is_temporary(decl)) {
      CHECK(decl->kind == TICK_AST_DECL, "temporary must be DECL");
      return write_fmt(ctx->writer, "__tmp%u", decl->decl.tmpid);
    }
  }

  // Use pre-computed prefix annotation from analysis pass
  if (node->identifier_expr.needs_user_prefix) {
    CHECK_OK(write_str(ctx->writer, "__u_"));
  }

  return write_ident(ctx->writer, node->identifier_expr.name);
}

static tick_err_t codegen_binary_expr(tick_ast_node_t* node,
                                      codegen_ctx_t* ctx) {
  // Check if this operation needs a runtime function
  const char* runtime_func = NULL;
  if (node->binary_expr.builtin != 0) {
    // Look up C runtime function from builtin category and type
    CHECK(node->binary_expr.resolved_type &&
              node->binary_expr.resolved_type->kind == TICK_AST_TYPE_NAMED,
          "binary expr must have resolved named type");
    tick_builtin_type_t type =
        node->binary_expr.resolved_type->type_named.builtin_type;
    runtime_func = RUNTIME_FUNCS[node->binary_expr.builtin][type];
  }

  if (runtime_func) {
    // Emit runtime function call
    CHECK_OK(write_str(ctx->writer, runtime_func));
    CHECK_OK(write_str(ctx->writer, "("));
    CHECK_OK(codegen_expr(node->binary_expr.left, ctx));
    CHECK_OK(write_str(ctx->writer, ", "));
    CHECK_OK(codegen_expr(node->binary_expr.right, ctx));
    CHECK_OK(write_str(ctx->writer, ")"));
    return TICK_OK;
  }

  // Direct C operator (comparisons, logical, bitwise, unsigned wrapping)
  CHECK_OK(codegen_expr(node->binary_expr.left, ctx));
  CHECK_OK(write_str(ctx->writer, " "));
  CHECK_OK(
      write_str(ctx->writer, tick_codegen_binop_to_c(node->binary_expr.op)));
  CHECK_OK(write_str(ctx->writer, " "));
  CHECK_OK(codegen_expr(node->binary_expr.right, ctx));
  return TICK_OK;
}

static tick_err_t codegen_unary_expr(tick_ast_node_t* node,
                                     codegen_ctx_t* ctx) {
  // Check if this operation needs a runtime function
  const char* runtime_func = NULL;
  if (node->unary_expr.builtin != 0) {
    // Look up C runtime function from builtin category and type
    CHECK(node->unary_expr.resolved_type &&
              node->unary_expr.resolved_type->kind == TICK_AST_TYPE_NAMED,
          "unary expr must have resolved named type");
    tick_builtin_type_t type =
        node->unary_expr.resolved_type->type_named.builtin_type;
    runtime_func = RUNTIME_FUNCS[node->unary_expr.builtin][type];
  }

  if (runtime_func) {
    // Emit runtime function call (e.g., tick_checked_neg_i32)
    CHECK_OK(write_str(ctx->writer, runtime_func));
    CHECK_OK(write_str(ctx->writer, "("));
    CHECK_OK(codegen_expr(node->unary_expr.operand, ctx));
    CHECK_OK(write_str(ctx->writer, ")"));
    return TICK_OK;
  }

  // Special handling for address-of slice/array index: emit pointer without
  // dereference
  if (node->unary_expr.op == UNOP_ADDR &&
      node->unary_expr.operand->kind == TICK_AST_INDEX_EXPR) {
    tick_ast_node_t* index_node = node->unary_expr.operand;

    if (index_node->index_expr.is_slice_index) {
      // Address of slice element: (T*)tick_slice_index_ptr(slice, index,
      // sizeof(T))
      CHECK(index_node->index_expr.resolved_type,
            "slice index must have resolved element type");

      CHECK_OK(write_str(ctx->writer, "("));
      CHECK_OK(codegen_type(index_node->index_expr.resolved_type, ctx->writer));
      CHECK_OK(write_str(ctx->writer, "*)tick_slice_index_ptr("));
      CHECK_OK(codegen_expr(index_node->index_expr.array, ctx));
      CHECK_OK(write_str(ctx->writer, ", "));
      CHECK_OK(codegen_expr(index_node->index_expr.index, ctx));
      CHECK_OK(write_str(ctx->writer, ", sizeof("));
      CHECK_OK(codegen_type(index_node->index_expr.resolved_type, ctx->writer));
      CHECK_OK(write_str(ctx->writer, "))"));
      return TICK_OK;
    } else {
      // Address of array element: &array[index]
      CHECK_OK(write_str(ctx->writer, "&"));
      CHECK_OK(codegen_expr(node->unary_expr.operand, ctx));
      return TICK_OK;
    }
  }

  // Direct C operator
  CHECK_OK(write_str(ctx->writer, tick_codegen_unop_to_c(node->unary_expr.op)));
  CHECK_OK(codegen_expr(node->unary_expr.operand, ctx));
  return TICK_OK;
}

static tick_err_t codegen_call_expr(tick_ast_node_t* node, codegen_ctx_t* ctx) {
  // Check if this is a builtin call
  tick_at_builtin_t builtin = TICK_AT_BUILTIN_UNKNOWN;
  if (node->call_expr.callee->kind == TICK_AST_IDENTIFIER_EXPR) {
    builtin = node->call_expr.callee->identifier_expr.at_builtin;
  }

  CHECK_OK(codegen_expr(node->call_expr.callee, ctx));
  CHECK_OK(write_str(ctx->writer, "("));

  tick_ast_node_t* arg = node->call_expr.args;
  bool first = true;
  int arg_index = 0;
  while (arg) {
    if (!first) {
      CHECK_OK(write_str(ctx->writer, ", "));
    }
    first = false;

    // For builtins with format strings, cast the first argument from uint8_t*
    // to const char*
    if (tick_codegen_builtin_needs_format_cast(builtin) && arg_index == 0) {
      CHECK_OK(write_str(ctx->writer, "(const char*)"));
    }

    CHECK_OK(codegen_expr(arg, ctx));
    arg = arg->next;
    arg_index++;
  }

  CHECK_OK(write_str(ctx->writer, ")"));
  return TICK_OK;
}

static tick_err_t codegen_field_access(tick_ast_node_t* node,
                                       codegen_ctx_t* ctx) {
  tick_buf_t field_name = node->field_access_expr.field_name;
  tick_ast_node_t* resolved_type = node->field_access_expr.resolved_type;

  // Special handling for slice.ptr - needs cast from void* to T*
  // We detect this by checking if the resolved type is a pointer and field is
  // "ptr"
  if (resolved_type && resolved_type->kind == TICK_AST_TYPE_POINTER &&
      field_name.sz == 3 && memcmp(field_name.buf, "ptr", 3) == 0) {
    // Emit: (T*)slice.ptr
    CHECK_OK(write_str(ctx->writer, "("));
    CHECK_OK(codegen_type(resolved_type, ctx->writer));
    CHECK_OK(write_str(ctx->writer, ")("));
    CHECK_OK(codegen_expr(node->field_access_expr.object, ctx));
    const char* op = node->field_access_expr.object_is_pointer ? "->" : ".";
    CHECK_OK(write_str(ctx->writer, op));
    CHECK_OK(write_ident(ctx->writer, field_name));
    CHECK_OK(write_str(ctx->writer, ")"));
    return TICK_OK;
  }

  // Regular field access
  CHECK_OK(write_str(ctx->writer, "("));
  CHECK_OK(codegen_expr(node->field_access_expr.object, ctx));
  CHECK_OK(write_str(ctx->writer, ")"));
  // Emit . or -> based on pre-computed pointer flag from analysis pass
  const char* op = node->field_access_expr.object_is_pointer ? "->" : ".";
  CHECK_OK(write_str(ctx->writer, op));
  CHECK_OK(write_ident(ctx->writer, node->field_access_expr.field_name));
  return TICK_OK;
}

static tick_err_t codegen_index_expr(tick_ast_node_t* node,
                                     codegen_ctx_t* ctx) {
  // Check if this is a slice index (needs bounds checking + cast)
  if (node->index_expr.is_slice_index) {
    // Emit slice indexing using tick_slice_index_ptr helper
    // Pattern: *(T*)tick_slice_index_ptr(slice, index, sizeof(T))
    CHECK(node->index_expr.resolved_type,
          "slice index must have resolved element type");

    // Dereference the returned pointer
    CHECK_OK(write_str(ctx->writer, "*("));

    // Cast to element type pointer
    CHECK_OK(codegen_type(node->index_expr.resolved_type, ctx->writer));
    CHECK_OK(write_str(ctx->writer, "*)tick_slice_index_ptr("));

    // Emit slice argument
    CHECK_OK(codegen_expr(node->index_expr.array, ctx));
    CHECK_OK(write_str(ctx->writer, ", "));

    // Emit index argument
    CHECK_OK(codegen_expr(node->index_expr.index, ctx));
    CHECK_OK(write_str(ctx->writer, ", "));

    // Emit element size
    CHECK_OK(write_str(ctx->writer, "sizeof("));
    CHECK_OK(codegen_type(node->index_expr.resolved_type, ctx->writer));
    CHECK_OK(write_str(ctx->writer, "))"));
  } else {
    // Regular array/pointer indexing
    CHECK_OK(write_str(ctx->writer, "("));
    CHECK_OK(codegen_expr(node->index_expr.array, ctx));
    CHECK_OK(write_str(ctx->writer, ")"));
    CHECK_OK(write_str(ctx->writer, "["));
    CHECK_OK(codegen_expr(node->index_expr.index, ctx));
    CHECK_OK(write_str(ctx->writer, "]"));
  }
  return TICK_OK;
}

static tick_err_t codegen_slice_expr(tick_ast_node_t* node,
                                     codegen_ctx_t* ctx) {
  // Emit slice expression using runtime helper functions
  // Replaces statement expressions with tick_slice_from_* functions

  CHECK(node->slice_expr.resolved_type, "slice expr must have resolved type");
  CHECK(node->slice_expr.resolved_type->kind == TICK_AST_TYPE_SLICE,
        "slice expr resolved_type must be SLICE");

  // Get element type for sizeof()
  tick_ast_node_t* elem_type =
      node->slice_expr.resolved_type->type_array.element_type;
  CHECK(elem_type, "slice must have element type");

  // Dispatch based on source kind (set by analysis pass)
  if (node->slice_expr.source_kind == TICK_SLICE_SOURCE_ARRAY) {
    // Slicing an array: tick_slice_from_array(arr, arr_len, start, end,
    // elem_size)
    CHECK_OK(write_str(ctx->writer, "tick_slice_from_array("));
    CHECK_OK(codegen_expr(node->slice_expr.array, ctx));
    CHECK_OK(write_str(ctx->writer, ", sizeof("));
    CHECK_OK(codegen_expr(node->slice_expr.array, ctx));
    CHECK_OK(write_str(ctx->writer, ")/sizeof(("));
    CHECK_OK(codegen_expr(node->slice_expr.array, ctx));
    CHECK_OK(write_str(ctx->writer, ")[0]), "));
  } else if (node->slice_expr.source_kind == TICK_SLICE_SOURCE_SLICE) {
    // Re-slicing: tick_slice_from_slice(src, start, end, elem_size)
    CHECK_OK(write_str(ctx->writer, "tick_slice_from_slice("));
    CHECK_OK(codegen_expr(node->slice_expr.array, ctx));
    CHECK_OK(write_str(ctx->writer, ", "));
  } else if (node->slice_expr.source_kind == TICK_SLICE_SOURCE_POINTER) {
    // Slicing a pointer: tick_slice_from_ptr(ptr, start, end, elem_size)
    CHECK_OK(write_str(ctx->writer, "tick_slice_from_ptr("));
    CHECK_OK(codegen_expr(node->slice_expr.array, ctx));
    CHECK_OK(write_str(ctx->writer, ", "));
  } else {
    CHECK(false, "slice source_kind must be set by analysis pass");
  }

  // Emit start index (0 if omitted)
  if (node->slice_expr.start) {
    CHECK_OK(codegen_expr(node->slice_expr.start, ctx));
  } else {
    CHECK_OK(write_str(ctx->writer, "0"));
  }
  CHECK_OK(write_str(ctx->writer, ", "));

  // Emit end index (length if omitted)
  if (node->slice_expr.end) {
    CHECK_OK(codegen_expr(node->slice_expr.end, ctx));
  } else {
    // For arrays, use sizeof; for slices, use .len; for pointers, must be
    // explicit
    if (node->slice_expr.source_kind == TICK_SLICE_SOURCE_ARRAY) {
      CHECK_OK(write_str(ctx->writer, "sizeof("));
      CHECK_OK(codegen_expr(node->slice_expr.array, ctx));
      CHECK_OK(write_str(ctx->writer, ")/sizeof(("));
      CHECK_OK(codegen_expr(node->slice_expr.array, ctx));
      CHECK_OK(write_str(ctx->writer, ")[0])"));
    } else if (node->slice_expr.source_kind == TICK_SLICE_SOURCE_SLICE) {
      CHECK_OK(write_str(ctx->writer, "("));
      CHECK_OK(codegen_expr(node->slice_expr.array, ctx));
      CHECK_OK(write_str(ctx->writer, ").len"));
    } else {
      CHECK(false, "pointer slicing requires explicit end index");
    }
  }
  CHECK_OK(write_str(ctx->writer, ", "));

  // Emit element size
  CHECK_OK(write_str(ctx->writer, "sizeof("));
  CHECK_OK(codegen_type(elem_type, ctx->writer));
  CHECK_OK(write_str(ctx->writer, "))"));

  return TICK_OK;
}

static tick_err_t codegen_enum_value(tick_ast_node_t* node,
                                     codegen_ctx_t* ctx) {
  // Enum value reference: emit EnumName_ValueName (pub) or
  // __u_EnumName_ValueName (private)
  CHECK(node->enum_value.parent_decl &&
            node->enum_value.parent_decl->kind == TICK_AST_DECL,
        "enum value must have parent decl");

  bool is_pub = node->enum_value.parent_decl->decl.quals.is_pub;
  if (!is_pub) {
    CHECK_OK(write_str(ctx->writer, "__u_"));
  }
  CHECK_OK(write_ident(ctx->writer, node->enum_value.parent_decl->decl.name));
  CHECK_OK(write_str(ctx->writer, "_"));
  CHECK_OK(write_ident(ctx->writer, node->enum_value.name));
  return TICK_OK;
}

static tick_err_t codegen_struct_init_expr(tick_ast_node_t* node,
                                           codegen_ctx_t* ctx) {
  // Emit: (TypeName) { .field1 = val1, .field2 = val2 }
  // Requires: All field values must be LITERAL or IDENTIFIER_EXPR (lowering
  // decomposed complex expressions)

  CHECK_OK(write_str(ctx->writer, "("));
  CHECK_OK(codegen_type(node->struct_init_expr.type, ctx->writer));
  CHECK_OK(write_str(ctx->writer, ") { "));

  tick_ast_node_t* field = node->struct_init_expr.fields;
  bool first = true;
  while (field) {
    CHECK(field->kind == TICK_AST_STRUCT_INIT_FIELD,
          "expected STRUCT_INIT_FIELD in struct initializer");

    tick_ast_node_t* value = field->struct_init_field.value;

    if (!first) {
      CHECK_OK(write_str(ctx->writer, ", "));
    }
    first = false;

    // Emit: .fieldname = value
    CHECK_OK(write_str(ctx->writer, "."));
    CHECK_OK(write_ident(ctx->writer, field->struct_init_field.field_name));
    CHECK_OK(write_str(ctx->writer, " = "));
    CHECK_OK(codegen_expr(value, ctx));

    field = field->next;
  }

  CHECK_OK(write_str(ctx->writer, " }"));
  return TICK_OK;
}

static tick_err_t codegen_array_init_expr(tick_ast_node_t* node,
                                          codegen_ctx_t* ctx) {
  // Emit: { elem1, elem2, elem3 }

  CHECK_OK(write_str(ctx->writer, "{ "));

  // Iterate elements in source order
  tick_ast_node_t* elem = node->array_init_expr.elements;
  bool first = true;
  while (elem) {
    if (!first) {
      CHECK_OK(write_str(ctx->writer, ", "));
    }
    first = false;

    CHECK_OK(codegen_expr(elem, ctx));
    elem = elem->next;
  }

  CHECK_OK(write_str(ctx->writer, " }"));
  return TICK_OK;
}

// ============================================================================
// Cast Helper Functions
// ============================================================================

static tick_err_t codegen_cast_expr(tick_ast_node_t* node, codegen_ctx_t* ctx) {
  // Compute cast strategy and runtime function from types
  // Get source and destination builtin types
  tick_builtin_type_t src_builtin = TICK_TYPE_UNKNOWN;
  tick_builtin_type_t dst_builtin = TICK_TYPE_UNKNOWN;

  // Destination type is the cast target
  if (node->cast_expr.type &&
      node->cast_expr.type->kind == TICK_AST_TYPE_NAMED) {
    dst_builtin = node->cast_expr.type->type_named.builtin_type;
  }

  // Source type comes from the expression being cast
  // For now, we need to extract it from the expression's resolved type
  // This is a simplification - in reality we'd need type checking here
  tick_ast_node_t* src_expr = node->cast_expr.expr;
  if (src_expr) {
    // Try to get resolved type from various expression kinds
    tick_ast_node_t* src_type = NULL;
    switch (src_expr->kind) {
      case TICK_AST_BINARY_EXPR:
        src_type = src_expr->binary_expr.resolved_type;
        break;
      case TICK_AST_UNARY_EXPR:
        src_type = src_expr->unary_expr.resolved_type;
        break;
      case TICK_AST_CAST_EXPR:
        src_type = src_expr->cast_expr.resolved_type;
        break;
      case TICK_AST_IDENTIFIER_EXPR:
      case TICK_AST_LITERAL:
        // For now, we'll use the destination type as a fallback
        // This is safe for bare casts
        break;
      default:
        break;
    }

    if (src_type && src_type->kind == TICK_AST_TYPE_NAMED) {
      src_builtin = src_type->type_named.builtin_type;
    }
  }

  // Compute strategy and lookup runtime function
  const char* runtime_func = NULL;
  tick_cast_strategy_t strategy = CAST_STRATEGY_BARE;

  if (src_builtin != TICK_TYPE_UNKNOWN && dst_builtin != TICK_TYPE_UNKNOWN) {
    strategy = tick_codegen_compute_cast_strategy(src_builtin, dst_builtin);
    if (strategy == CAST_STRATEGY_CHECKED) {
      runtime_func = CAST_FUNCS[src_builtin][dst_builtin];
    }
  }

  if (strategy == CAST_STRATEGY_BARE || !runtime_func) {
    // Bare C cast (widening, same type, non-numeric, or no runtime func needed)
    CHECK_OK(write_str(ctx->writer, "("));
    CHECK_OK(codegen_type(node->cast_expr.type, ctx->writer));
    CHECK_OK(write_str(ctx->writer, ")"));
    CHECK_OK(codegen_expr(node->cast_expr.expr, ctx));
    return TICK_OK;
  }

  // Checked cast - use runtime function
  CHECK_OK(write_str(ctx->writer, runtime_func));
  CHECK_OK(write_str(ctx->writer, "("));
  CHECK_OK(codegen_expr(node->cast_expr.expr, ctx));
  CHECK_OK(write_str(ctx->writer, ")"));

  return TICK_OK;
}

// ============================================================================
// Statement Generation
// ============================================================================

static tick_err_t codegen_stmt(tick_ast_node_t* stmt, codegen_ctx_t* ctx,
                               int indent);

// Block formatting styles
typedef struct {
  bool emit_braces;       // Whether to emit { and }
  bool inline_opening;    // Whether opening brace goes on same line (no indent)
  bool trailing_newline;  // Whether to emit \n after closing brace
} block_style_t;

// Standard block formatting configurations
static const block_style_t BLOCK_STD = {
    true, false, true};  // Standard block: indented braces with newline
static const block_style_t BLOCK_IF = {
    true, true, false};  // If-block: inline braces, no trailing newline
static const block_style_t BLOCK_ELSE = {
    true, true, true};  // Else-block: inline braces, with trailing newline
static const block_style_t BLOCK_FOR = {
    false, false, false};  // For-block: no braces (emit statements only)

static tick_err_t codegen_block_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx,
                                     int indent, block_style_t style) {
  // Emit opening brace if requested
  if (style.emit_braces) {
    if (!style.inline_opening) {
      CHECK_OK(write_indent(ctx->writer, indent));
    }
    CHECK_OK(write_str(ctx->writer, "{\n"));
  }

  // Emit statements
  tick_ast_node_t* stmt = node->block_stmt.stmts;
  while (stmt) {
    CHECK_OK(codegen_stmt(stmt, ctx, indent + 1));
    stmt = stmt->next;
  }

  // Emit closing brace if requested
  if (style.emit_braces) {
    CHECK_OK(write_indent(ctx->writer, indent));
    if (style.trailing_newline) {
      CHECK_OK(write_str(ctx->writer, "}\n"));
    } else {
      CHECK_OK(write_str(ctx->writer, "}"));
    }
  }

  return TICK_OK;
}

static tick_err_t codegen_return_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx,
                                      int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "return"));
  if (node->return_stmt.value) {
    CHECK_OK(write_str(ctx->writer, " "));
    CHECK_OK(codegen_expr(node->return_stmt.value, ctx));
  }
  CHECK_OK(write_str(ctx->writer, ";\n"));
  return TICK_OK;
}

static tick_err_t codegen_decl(tick_ast_node_t* node, codegen_ctx_t* ctx,
                               int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));

  // Emit static qualifier if present (for temporaries)
  if (node->decl.quals.is_static) {
    CHECK_OK(write_str(ctx->writer, "static const "));
  }

  // Emit volatile qualifier if present
  if (node->decl.quals.is_volatile) {
    CHECK_OK(write_str(ctx->writer, "volatile "));
  }

  // Emit type + name + array suffixes using centralized declarator helper
  CHECK_OK(write_c_declarator(ctx->writer, node->decl.type, node));

  // Emit initializer if present
  // Note: undefined literals are removed by analysis pass (init set to NULL)
  // Note: static string literals are transformed to ARRAY_INIT_EXPR by analysis
  if (node->decl.init) {
    CHECK_OK(write_str(ctx->writer, " = "));
    CHECK_OK(codegen_expr(node->decl.init, ctx));
  }

  CHECK_OK(write_str(ctx->writer, ";\n"));
  return TICK_OK;
}

static tick_err_t codegen_assign_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx,
                                      int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(codegen_expr(node->assign_stmt.lhs, ctx));
  CHECK_OK(write_str(ctx->writer, " = "));
  CHECK_OK(codegen_expr(node->assign_stmt.rhs, ctx));
  CHECK_OK(write_str(ctx->writer, ";\n"));
  return TICK_OK;
}

static tick_err_t codegen_unused_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx,
                                      int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "TICK_UNUSED("));
  CHECK_OK(codegen_expr(node->unused_stmt.expr, ctx));
  CHECK_OK(write_str(ctx->writer, ");\n"));
  return TICK_OK;
}

static tick_err_t codegen_expr_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx,
                                    int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(codegen_expr(node->expr_stmt.expr, ctx));
  CHECK_OK(write_str(ctx->writer, ";\n"));
  return TICK_OK;
}

static tick_err_t codegen_if_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx,
                                  int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "if ("));
  CHECK_OK(codegen_expr(node->if_stmt.condition, ctx));
  CHECK_OK(write_str(ctx->writer, ") "));
  CHECK_OK(codegen_block_stmt(node->if_stmt.then_block, ctx, indent, BLOCK_IF));
  CHECK_OK(write_str(ctx->writer, " else "));
  CHECK_OK(
      codegen_block_stmt(node->if_stmt.else_block, ctx, indent, BLOCK_ELSE));
  return TICK_OK;
}

static tick_err_t codegen_break_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx,
                                     int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "break;\n"));
  return TICK_OK;
}

static tick_err_t codegen_continue_stmt(tick_ast_node_t* node,
                                        codegen_ctx_t* ctx, int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "continue;\n"));
  return TICK_OK;
}

static tick_err_t codegen_goto_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx,
                                    int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "goto "));
  CHECK_OK(write_ident(ctx->writer, node->goto_stmt.label));
  CHECK_OK(write_str(ctx->writer, ";\n"));
  return TICK_OK;
}

static tick_err_t codegen_label_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx,
                                     int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_ident(ctx->writer, node->label_stmt.label));
  CHECK_OK(write_str(ctx->writer, ":\n"));
  return TICK_OK;
}

static tick_err_t codegen_for_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx,
                                   int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  if (node->for_stmt.init_stmt) {
    CHECK_OK(codegen_stmt(node->for_stmt.init_stmt, ctx, indent));
  }
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "while (1) {\n"));
  if (node->for_stmt.condition) {
    CHECK_OK(write_indent(ctx->writer, indent + 1));
    CHECK_OK(write_str(ctx->writer, "if (!("));
    CHECK_OK(codegen_expr(node->for_stmt.condition, ctx));
    CHECK_OK(write_str(ctx->writer, ")) break;\n"));
  }
  CHECK_OK(codegen_block_stmt(node->for_stmt.body, ctx, indent, BLOCK_FOR));
  if (node->for_stmt.step_stmt) {
    CHECK_OK(codegen_stmt(node->for_stmt.step_stmt, ctx, indent + 1));
  }
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "}\n"));
  return TICK_OK;
}

static tick_err_t codegen_switch_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx,
                                      int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "switch ("));
  CHECK_OK(codegen_expr(node->switch_stmt.value, ctx));
  CHECK_OK(write_str(ctx->writer, ") {\n"));

  tick_ast_node_t* case_node = node->switch_stmt.cases;
  while (case_node) {
    if (case_node->switch_case.values == NULL) {
      // Default case
      CHECK_OK(write_indent(ctx->writer, indent + 1));
      CHECK_OK(write_str(ctx->writer, "default:\n"));
    } else {
      // Regular case with values
      tick_ast_node_t* value = case_node->switch_case.values;
      while (value) {
        CHECK_OK(write_indent(ctx->writer, indent + 1));
        CHECK_OK(write_str(ctx->writer, "case "));
        CHECK_OK(codegen_expr(value, ctx));
        CHECK_OK(write_str(ctx->writer, ":\n"));
        value = value->next;
      }
    }

    // Emit case block (analysis ensures it's a BLOCK_STMT)
    CHECK_OK(codegen_block_stmt(case_node->switch_case.stmts, ctx, indent + 1,
                                BLOCK_STD));

    // Always emit break after the block (harmless even after
    // return/break/continue)
    CHECK_OK(write_indent(ctx->writer, indent + 1));
    CHECK_OK(write_str(ctx->writer, "break;\n"));

    case_node = case_node->next;
  }

  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "}\n"));
  return TICK_OK;
}

// ============================================================================
// Utility Functions
// ============================================================================

// ============================================================================
// Type Declaration Generation
// ============================================================================

// Helper to emit fields for struct/union declarations
static tick_err_t codegen_fields(tick_ast_node_t* first_field, tick_writer_t* w,
                                 int indent_level, bool emit_alignment_attr,
                                 const char* context_name) {
  tick_ast_node_t* field = first_field;
  while (field) {
    CHECK(field->kind == TICK_AST_FIELD, "expected FIELD node in %s",
          context_name);

    CHECK_OK(write_indent(w, indent_level));
    CHECK_OK(codegen_type(field->field.type, w));
    CHECK_OK(write_str(w, " "));
    CHECK_OK(write_ident(w, field->field.name));
    CHECK_OK(write_array_suffix(w, field->field.type));

    // Handle per-field alignment if enabled
    if (emit_alignment_attr && field->field.alignment) {
      CHECK(field->field.alignment->kind == TICK_AST_LITERAL,
            "field alignment must be literal (analysis didn't evaluate "
            "constant)");
      uint64_t align_val = field->field.alignment->literal.data.uint_value;
      CHECK_OK(write_fmt(w, " __attribute__((aligned(%llu)))",
                         (unsigned long long)align_val));
    }

    CHECK_OK(write_str(w, ";\n"));
    field = field->next;
  }
  return TICK_OK;
}

static tick_err_t codegen_struct_decl(tick_ast_node_t* decl, tick_writer_t* w,
                                      bool full_definition) {
  tick_ast_node_t* struct_node = decl->decl.init;
  CHECK(struct_node && struct_node->kind == TICK_AST_STRUCT_DECL,
        "expected STRUCT_DECL");

  bool is_pub = decl->decl.quals.is_pub;

  if (!full_definition) {
    CHECK_OK(write_str(w, "typedef struct "));
    if (!is_pub) {
      CHECK_OK(write_str(w, "__u_"));
    }
    CHECK_OK(write_ident(w, decl->decl.name));
    CHECK_OK(write_str(w, " "));
    if (!is_pub) {
      CHECK_OK(write_str(w, "__u_"));
    }
    CHECK_OK(write_ident(w, decl->decl.name));
    CHECK_OK(write_str(w, ";\n"));
    return TICK_OK;
  }

  CHECK_OK(write_str(w, "struct "));
  if (struct_node->struct_decl.is_packed) {
    CHECK_OK(write_str(w, "__attribute__((packed)) "));
  }
  if (struct_node->struct_decl.alignment) {
    CHECK(struct_node->struct_decl.alignment->kind == TICK_AST_LITERAL,
          "alignment must be literal (analysis didn't evaluate constant)");
    uint64_t align_val =
        struct_node->struct_decl.alignment->literal.data.uint_value;
    CHECK_OK(write_fmt(w, "__attribute__((aligned(%llu))) ",
                       (unsigned long long)align_val));
  }

  if (!is_pub) {
    CHECK_OK(write_str(w, "__u_"));
  }
  CHECK_OK(write_ident(w, decl->decl.name));
  CHECK_OK(write_str(w, " {\n"));
  CHECK_OK(
      codegen_fields(struct_node->struct_decl.fields, w, 1, true, "struct"));
  CHECK_OK(write_str(w, "};\n\n"));

  return TICK_OK;
}

static tick_err_t codegen_enum_decl(tick_ast_node_t* decl, tick_writer_t* w) {
  tick_ast_node_t* enum_node = decl->decl.init;
  CHECK(enum_node && enum_node->kind == TICK_AST_ENUM_DECL,
        "expected ENUM_DECL");

  bool is_pub = decl->decl.quals.is_pub;

  // Emit typedef for the enum type using its underlying type
  CHECK_OK(write_str(w, "typedef "));
  CHECK_OK(codegen_type(enum_node->enum_decl.underlying_type, w));
  CHECK_OK(write_str(w, " "));
  if (!is_pub) {
    CHECK_OK(write_str(w, "__u_"));
  }
  CHECK_OK(write_ident(w, decl->decl.name));
  CHECK_OK(write_str(w, ";\n"));

  // Emit each enum value as a static const with the correct type
  tick_ast_node_t* value = enum_node->enum_decl.values;
  while (value) {
    CHECK_OK(write_str(w, "static const "));
    if (!is_pub) {
      CHECK_OK(write_str(w, "__u_"));
    }
    CHECK_OK(write_ident(w, decl->decl.name));
    CHECK_OK(write_str(w, " "));
    if (!is_pub) {
      CHECK_OK(write_str(w, "__u_"));
    }
    CHECK_OK(write_ident(w, decl->decl.name));
    CHECK_OK(write_str(w, "_"));
    CHECK_OK(write_ident(w, value->enum_value.name));

    CHECK_OK(write_str(w, " = "));
    if (value->enum_value.value->literal.kind == TICK_LIT_INT) {
      int64_t val = value->enum_value.value->literal.data.int_value;
      CHECK_OK(write_fmt(w, "%lld", (long long)val));
    } else {
      uint64_t val = value->enum_value.value->literal.data.uint_value;
      CHECK_OK(write_fmt(w, "%llu", (unsigned long long)val));
    }
    CHECK_OK(write_str(w, ";\n"));

    value = value->next;
  }

  CHECK_OK(write_str(w, "\n"));
  return TICK_OK;
}

static tick_err_t codegen_union_decl(tick_ast_node_t* decl, tick_writer_t* w,
                                     bool full_definition) {
  tick_ast_node_t* union_node = decl->decl.init;
  CHECK(union_node && union_node->kind == TICK_AST_UNION_DECL,
        "expected UNION_DECL");
  CHECK(union_node->union_decl.tag_type != NULL,
        "union has NULL tag_type (lowering didn't generate tag enum)");

  bool is_pub = decl->decl.quals.is_pub;

  if (!full_definition) {
    CHECK_OK(write_str(w, "typedef struct "));
    if (!is_pub) {
      CHECK_OK(write_str(w, "__u_"));
    }
    CHECK_OK(write_ident(w, decl->decl.name));
    CHECK_OK(write_str(w, " "));
    if (!is_pub) {
      CHECK_OK(write_str(w, "__u_"));
    }
    CHECK_OK(write_ident(w, decl->decl.name));
    CHECK_OK(write_str(w, ";\n"));
    return TICK_OK;
  }

  CHECK_OK(write_str(w, "struct "));
  if (union_node->union_decl.alignment) {
    CHECK(union_node->union_decl.alignment->kind == TICK_AST_LITERAL,
          "alignment must be literal (analysis didn't evaluate constant)");
    uint64_t align_val =
        union_node->union_decl.alignment->literal.data.uint_value;
    CHECK_OK(write_fmt(w, "__attribute__((aligned(%llu))) ",
                       (unsigned long long)align_val));
  }

  if (!is_pub) {
    CHECK_OK(write_str(w, "__u_"));
  }
  CHECK_OK(write_ident(w, decl->decl.name));
  CHECK_OK(write_str(w, " {\n"));

  CHECK_OK(write_str(w, "  "));
  CHECK_OK(codegen_type(union_node->union_decl.tag_type, w));
  CHECK_OK(write_str(w, " tag;\n"));

  CHECK_OK(write_str(w, "  union {\n"));
  CHECK_OK(codegen_fields(union_node->union_decl.fields, w, 2, false, "union"));
  CHECK_OK(write_str(w, "  };\n"));
  CHECK_OK(write_str(w, "};\n\n"));

  return TICK_OK;
}

// ============================================================================
// Function Generation
// ============================================================================

static tick_err_t codegen_function_signature(tick_ast_node_t* decl,
                                             codegen_ctx_t* ctx,
                                             bool include_param_names) {
  tick_ast_node_t* func = decl->decl.init;

  // Return type
  CHECK_OK(codegen_type(func->function.return_type, ctx->writer));
  CHECK_OK(write_str(ctx->writer, " "));

  // Function name (with __u_ prefix)
  CHECK_OK(write_decl_name(ctx->writer, decl));

  // Parameters in source order
  CHECK_OK(write_str(ctx->writer, "("));
  tick_ast_node_t* param = func->function.params;

  // In C, empty parameter list should be (void) not ()
  if (!param) {
    CHECK_OK(write_str(ctx->writer, "void"));
  } else {
    bool first = true;
    while (param) {
      if (!first) {
        CHECK_OK(write_str(ctx->writer, ", "));
      }
      first = false;

      // Include parameter names only in implementation
      if (include_param_names) {
        // Use write_c_declarator to handle arrays and pointer-to-array
        // correctly
        CHECK_OK(write_c_declarator(ctx->writer, param->decl.type, param));
      } else {
        // Declaration without names - just emit the type
        CHECK_OK(codegen_type(param->decl.type, ctx->writer));
      }

      param = param->next;
    }
  }
  CHECK_OK(write_str(ctx->writer, ")"));

  return TICK_OK;
}

static tick_err_t codegen_function_decl_h(tick_ast_node_t* decl,
                                          codegen_ctx_t* ctx) {
  CHECK_OK(codegen_function_signature(decl, ctx,
                                      /*include_param_names=*/false));
  CHECK_OK(write_str(ctx->writer, ";\n"));
  return TICK_OK;
}

static tick_err_t codegen_function_decl_c(tick_ast_node_t* decl,
                                          codegen_ctx_t* ctx) {
  CHECK_OK(write_line_directive(ctx, decl->loc.line));
  tick_ast_node_t* func = decl->decl.init;
  CHECK_OK(codegen_function_signature(decl, ctx, /*include_param_names=*/true));
  CHECK_OK(write_str(ctx->writer, " "));
  if (func->function.body) {
    CHECK_OK(codegen_block_stmt(func->function.body, ctx, 0, BLOCK_STD));
  } else {
    CHECK_OK(write_str(ctx->writer, ";\n"));
  }
  CHECK_OK(write_str(ctx->writer, "\n"));
  return TICK_OK;
}

// ============================================================================
// Global Variable Generation
// ============================================================================

// Helper to emit extern function declaration from TYPE_FUNCTION
// Emits: extern return_type name(params);
static tick_err_t codegen_extern_function_decl(tick_ast_node_t* decl,
                                               codegen_ctx_t* ctx) {
  tick_ast_node_t* fn_type = decl->decl.type;
  CHECK(fn_type && fn_type->kind == TICK_AST_TYPE_FUNCTION,
        "expected TYPE_FUNCTION for extern function declaration");

  CHECK_OK(write_str(ctx->writer, "extern "));

  // Return type
  CHECK_OK(codegen_type(fn_type->type_function.return_type, ctx->writer));
  CHECK_OK(write_str(ctx->writer, " "));

  // Function name (with __u_ prefix handling - extern skips prefix)
  CHECK_OK(write_decl_name(ctx->writer, decl));

  // Parameters
  CHECK_OK(write_str(ctx->writer, "("));
  tick_ast_node_t* param = fn_type->type_function.params;
  if (!param) {
    CHECK_OK(write_str(ctx->writer, "void"));
  } else {
    bool first = true;
    while (param) {
      if (!first) {
        CHECK_OK(write_str(ctx->writer, ", "));
      }
      first = false;
      CHECK_OK(codegen_type(param->decl.type, ctx->writer));
      param = param->next;
    }
  }
  CHECK_OK(write_str(ctx->writer, ")"));

  CHECK_OK(write_str(ctx->writer, ";\n"));
  return TICK_OK;
}

// Helper to emit function pointer variable declaration
// Emits: [extern] [volatile] return_type (*name)(params)
static tick_err_t codegen_function_pointer_var(tick_ast_node_t* decl,
                                               codegen_ctx_t* ctx,
                                               tick_ast_node_t* fn_type,
                                               bool is_extern,
                                               bool is_volatile) {
  if (is_extern) {
    CHECK_OK(write_str(ctx->writer, "extern "));
  }
  if (is_volatile) {
    CHECK_OK(write_str(ctx->writer, "volatile "));
  }

  // Emit return type
  CHECK_OK(codegen_type(fn_type->type_function.return_type, ctx->writer));
  CHECK_OK(write_str(ctx->writer, " (*"));
  CHECK_OK(write_decl_name(ctx->writer, decl));
  CHECK_OK(write_str(ctx->writer, ")("));

  // Emit parameter types
  tick_ast_node_t* param = fn_type->type_function.params;
  if (!param) {
    CHECK_OK(write_str(ctx->writer, "void"));
  } else {
    bool first = true;
    while (param) {
      if (!first) {
        CHECK_OK(write_str(ctx->writer, ", "));
      }
      first = false;
      CHECK_OK(codegen_type(param->decl.type, ctx->writer));
      param = param->next;
    }
  }

  CHECK_OK(write_str(ctx->writer, ")"));
  return TICK_OK;
}

static tick_err_t codegen_global_var_h(tick_ast_node_t* decl,
                                       codegen_ctx_t* ctx) {
  // Function declarations (not function pointer variables) use specialized
  // emission
  if (tick_decl_is_function_declaration(decl)) {
    return codegen_extern_function_decl(decl, ctx);
  }

  CHECK_OK(write_str(ctx->writer, "extern "));
  bool is_volatile = decl->decl.quals.is_volatile;
  if (is_volatile) {
    CHECK_OK(write_str(ctx->writer, "volatile "));
  }
  CHECK_OK(write_c_declarator(ctx->writer, decl->decl.type, decl));
  CHECK_OK(write_str(ctx->writer, ";\n"));
  return TICK_OK;
}

static tick_err_t codegen_global_var_c(tick_ast_node_t* decl,
                                       codegen_ctx_t* ctx) {
  bool is_volatile = decl->decl.quals.is_volatile;
  bool is_extern = decl->decl.quals.is_extern;

  CHECK_OK(write_line_directive(ctx, decl->loc.line));

  tick_ast_node_t* type = decl->decl.type;

  // Function declarations (not function pointer variables) use specialized
  // emission
  if (tick_decl_is_function_declaration(decl)) {
    return codegen_extern_function_decl(decl, ctx);
  }

  // Function pointer variables use specialized emission
  if (type && type->kind == TICK_AST_TYPE_POINTER &&
      type->type_pointer.pointee_type &&
      type->type_pointer.pointee_type->kind == TICK_AST_TYPE_FUNCTION) {
    CHECK_OK(codegen_function_pointer_var(
        decl, ctx, type->type_pointer.pointee_type, is_extern, is_volatile));
  } else {
    // Normal variable declaration
    if (is_extern) {
      CHECK_OK(write_str(ctx->writer, "extern "));
    }
    if (is_volatile) {
      CHECK_OK(write_str(ctx->writer, "volatile "));
    }
    CHECK_OK(write_c_declarator(ctx->writer, type, decl));
  }

  // Emit initializer
  if (decl->decl.init) {
    CHECK_OK(write_str(ctx->writer, " = "));
    CHECK_OK(codegen_expr(decl->decl.init, ctx));
  }

  CHECK_OK(write_str(ctx->writer, ";\n"));
  return TICK_OK;
}

static tick_err_t codegen_expr(tick_ast_node_t* expr, codegen_ctx_t* ctx) {
  if (!expr) return TICK_OK;

  switch (expr->kind) {
    case TICK_AST_LITERAL:
      return codegen_literal(expr, ctx);
    case TICK_AST_IDENTIFIER_EXPR:
      return codegen_identifier(expr, ctx);
    case TICK_AST_BINARY_EXPR:
      return codegen_binary_expr(expr, ctx);
    case TICK_AST_UNARY_EXPR:
      return codegen_unary_expr(expr, ctx);
    case TICK_AST_CALL_EXPR:
      return codegen_call_expr(expr, ctx);
    case TICK_AST_FIELD_ACCESS_EXPR:
      return codegen_field_access(expr, ctx);
    case TICK_AST_INDEX_EXPR:
      return codegen_index_expr(expr, ctx);
    case TICK_AST_SLICE_EXPR:
      return codegen_slice_expr(expr, ctx);
    case TICK_AST_CAST_EXPR:
      return codegen_cast_expr(expr, ctx);
    case TICK_AST_ENUM_VALUE:
      return codegen_enum_value(expr, ctx);
    case TICK_AST_STRUCT_INIT_EXPR:
      return codegen_struct_init_expr(expr, ctx);
    case TICK_AST_ARRAY_INIT_EXPR:
      return codegen_array_init_expr(expr, ctx);
    default:
      CHECK(0, "unsupported expression kind");
  }
}

static tick_err_t codegen_stmt(tick_ast_node_t* stmt, codegen_ctx_t* ctx,
                               int indent) {
  if (!stmt) return TICK_OK;
  switch (stmt->kind) {
    case TICK_AST_DECL:
      return codegen_decl(stmt, ctx, indent);
    case TICK_AST_ASSIGN_STMT:
      return codegen_assign_stmt(stmt, ctx, indent);
    case TICK_AST_UNUSED_STMT:
      return codegen_unused_stmt(stmt, ctx, indent);
    case TICK_AST_EXPR_STMT:
      return codegen_expr_stmt(stmt, ctx, indent);
    case TICK_AST_BLOCK_STMT:
      return codegen_block_stmt(stmt, ctx, indent, BLOCK_STD);
    case TICK_AST_IF_STMT:
      return codegen_if_stmt(stmt, ctx, indent);
    case TICK_AST_FOR_STMT:
      return codegen_for_stmt(stmt, ctx, indent);
    case TICK_AST_SWITCH_STMT:
      return codegen_switch_stmt(stmt, ctx, indent);
    case TICK_AST_RETURN_STMT:
      return codegen_return_stmt(stmt, ctx, indent);
    case TICK_AST_BREAK_STMT:
      return codegen_break_stmt(stmt, ctx, indent);
    case TICK_AST_CONTINUE_STMT:
      return codegen_continue_stmt(stmt, ctx, indent);
    case TICK_AST_GOTO_STMT:
      return codegen_goto_stmt(stmt, ctx, indent);
    case TICK_AST_LABEL_STMT:
      return codegen_label_stmt(stmt, ctx, indent);
    default:
      CHECK(0, "unsupported statement kind");
  }
}

// ============================================================================
// Main Entry Points
// ============================================================================

tick_err_t tick_codegen(tick_ast_t* ast, const char* source_filename,
                        const char* header_filename, tick_writer_t writer_h,
                        tick_writer_t writer_c) {
  // Standard header
  CHECK_OK(write_str(&writer_h, "// Generated by tick compiler\n"));
  CHECK_OK(write_str(&writer_h, "#pragma once\n\n"));
  tick_buf_t runtime_buf = {.buf = (u8*)tick_runtime_header,
                            .sz = tick_runtime_header_len};
  CHECK_OK(writer_h.write(writer_h.ctx, &runtime_buf));
  CHECK_OK(write_str(&writer_h, "\n"));

  // Header include
  CHECK_OK(write_fmt(&writer_c, "#include \"%s\"\n\n", header_filename));

  codegen_ctx_t ctx_h = {&writer_h, source_filename, 0};
  codegen_ctx_t ctx_c = {&writer_c, source_filename, 0};
  for (tick_ast_node_t* decl = ast->root->module.decls; decl;
       decl = decl->next) {
    CHECK(decl->kind == TICK_AST_DECL, "top-level must be decls");
    bool is_pub = decl->decl.quals.is_pub;
    bool is_forward = decl->decl.quals.is_forward_decl;
    tick_ast_node_kind_t init_kind =
        decl->decl.init ? decl->decl.init->kind : TICK_AST_DECL;
    switch (init_kind) {
      case TICK_AST_ENUM_DECL:
        CHECK_OK(codegen_enum_decl(decl, is_pub ? &writer_h : &writer_c));
        break;
      case TICK_AST_STRUCT_DECL:
        CHECK_OK(codegen_struct_decl(decl, is_pub ? &writer_h : &writer_c,
                                     !is_forward));
        break;
      case TICK_AST_UNION_DECL:
        CHECK_OK(codegen_union_decl(decl, is_pub ? &writer_h : &writer_c,
                                    !is_forward));
        break;
      case TICK_AST_FUNCTION:
        if (is_pub) {
          CHECK_OK(codegen_function_decl_h(decl, &ctx_h));
        }
        CHECK_OK(codegen_function_decl_c(decl, &ctx_c));
        break;
      case TICK_AST_DECL:
        if (is_pub) {
          CHECK_OK(codegen_global_var_h(decl, &ctx_h));
        }
        CHECK_OK(codegen_global_var_c(decl, &ctx_c));
        break;
      default:
        CHECK(0, "unhandled decl type");
    }
  }
  return TICK_OK;
}
