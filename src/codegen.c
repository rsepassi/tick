#include "tick.h"
#include "tick_runtime_embedded.h"

#include <stdarg.h>

// ============================================================================
// Code Generation
// ============================================================================
//
// This module generates C code from a type-analyzed and lowered Tick AST.
//
// IMPORTANT: See tick.h "AST Pipeline and Lowering Target" for the complete
// specification of what codegen expects from the analysis and lowering passes.
//
// Summary:
// - Codegen performs ZERO semantic analysis (pure structural translation)
// - All types must be resolved (no TICK_TYPE_UNKNOWN)
// - All operation nodes must have resolved_type set
// - High-level types must be lowered (when implemented)
//
// Codegen maps operations to runtime functions:
//   (BINOP_ADD, i32) → tick_checked_add_i32
//   (BINOP_WRAP_ADD, i32) → tick_wrap_add_i32
//   (BINOP_SAT_ADD, u64) → tick_sat_add_u64
//   etc.
//
// See tick.h for full details on the lowering target and codegen contract.
//
// ============================================================================

// Codegen context for tracking state during generation
typedef struct {
  tick_writer_t* writer;
  const char* source_filename;  // For #line directives
  usz last_line;  // Track last emitted line to avoid redundant #line directives
} codegen_ctx_t;

// ============================================================================
// Writer Helpers
// ============================================================================

static tick_err_t write_str(tick_writer_t* w, const char* str) {
  tick_buf_t buf = {(u8*)str, strlen(str)};
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
  return w->write(w->ctx, &buf);
}

// Write a variable name with appropriate prefix based on tmpid and qualifiers
// tmpid == 0: user-named variable → __u_{name} (unless extern or pub)
// tmpid > 0: compiler temporary → __tmp{N}
// extern: no prefix (links to external C symbol)
// pub: no prefix (exported for C code to use)
static tick_err_t write_decl_name(tick_writer_t* w, tick_ast_node_t* decl) {
  if (decl->data.decl.tmpid == 0) {
    // User-named variable
    bool is_extern = decl->data.decl.quals.is_extern;
    bool is_pub = decl->data.decl.quals.is_pub;

    // Skip prefix for extern and pub declarations so they can link with C code
    if (!is_extern && !is_pub) {
      CHECK_OK(write_str(w, "__u_"));
    }
    return write_ident(w, decl->data.decl.name);
  } else {
    // Compiler-generated temporary
    return write_fmt(w, "__tmp%u", decl->data.decl.tmpid);
  }
}

// Write array declarator suffix (the [size] part after the name)
static tick_err_t write_array_suffix(tick_writer_t* w, tick_ast_node_t* type) {
  if (!type) return TICK_OK;

  if (type->kind == TICK_AST_TYPE_ARRAY) {
    // Emit array size
    if (type->data.type_array.size) {
      CHECK(type->data.type_array.size->kind == TICK_AST_LITERAL,
            "array size must be literal (no VLAs)");
      CHECK_OK(write_str(w, "["));
      CHECK_OK(write_fmt(w, "%llu",
                        (unsigned long long)type->data.type_array.size->data.literal.data.uint_value));
      CHECK_OK(write_str(w, "]"));
    }
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

  return write_fmt(ctx->writer, "#line %zu \"%s\"\n", line, ctx->source_filename);
}

// ============================================================================
// Operator Mapping
// ============================================================================

static const char* binop_to_c(tick_binop_t op) {
  switch (op) {
    // Only comparisons and logical operators can be emitted directly
    case BINOP_EQ: return "==";
    case BINOP_NE: return "!=";
    case BINOP_LT: return "<";
    case BINOP_GT: return ">";
    case BINOP_LE: return "<=";
    case BINOP_GE: return ">=";
    case BINOP_LOGICAL_AND: return "&&";
    case BINOP_LOGICAL_OR: return "||";
    // Bitwise operators - these are well-defined in C
    case BINOP_AND: return "&";
    case BINOP_OR: return "|";
    case BINOP_XOR: return "^";

    // All arithmetic and shift operations should be handled by codegen_binary_expr
    // If we reach here, it means resolved_type was not set by analysis pass
    case BINOP_ADD:
    case BINOP_SUB:
    case BINOP_MUL:
      CHECK(0, "arithmetic operations must have resolved_type set by analysis pass");
    case BINOP_DIV:
    case BINOP_MOD:
      CHECK(0, "division/modulo must have resolved_type set by analysis pass");
    case BINOP_LSHIFT:
    case BINOP_RSHIFT:
      CHECK(0, "shift operations must have resolved_type set by analysis pass");

    // Saturating and wrapping operations should be handled by codegen_binary_expr
    case BINOP_SAT_ADD:
    case BINOP_SAT_SUB:
    case BINOP_SAT_MUL:
    case BINOP_SAT_DIV:
      CHECK(0, "saturating operations must have resolved_type set by analysis pass");
    case BINOP_WRAP_ADD:
    case BINOP_WRAP_SUB:
    case BINOP_WRAP_MUL:
    case BINOP_WRAP_DIV:
      CHECK(0, "wrapping operations must have resolved_type set by analysis pass");

    default: return "?";
  }
}

static const char* unop_to_c(tick_unop_t op) {
  switch (op) {
    case UNOP_NEG:
      // Negation should be handled by codegen_unary_expr for signed types
      // If we reach here, it means resolved_type was not set or it's unsigned
      CHECK(0, "negation must have resolved_type set by analysis pass");
    case UNOP_NOT: return "!";
    case UNOP_BIT_NOT: return "~";
    case UNOP_ADDR: return "&";
    case UNOP_DEREF: return "*";
    default: return "?";
  }
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
      switch (type->data.type_named.builtin_type) {
        case TICK_TYPE_I8: return write_str(w, "int8_t");
        case TICK_TYPE_I16: return write_str(w, "int16_t");
        case TICK_TYPE_I32: return write_str(w, "int32_t");
        case TICK_TYPE_I64: return write_str(w, "int64_t");
        case TICK_TYPE_ISZ: return write_str(w, "ptrdiff_t");
        case TICK_TYPE_U8: return write_str(w, "uint8_t");
        case TICK_TYPE_U16: return write_str(w, "uint16_t");
        case TICK_TYPE_U32: return write_str(w, "uint32_t");
        case TICK_TYPE_U64: return write_str(w, "uint64_t");
        case TICK_TYPE_USZ: return write_str(w, "size_t");
        case TICK_TYPE_BOOL: return write_str(w, "bool");
        case TICK_TYPE_VOID: return write_str(w, "void");
        case TICK_TYPE_USER_DEFINED:
          // User-defined types - emit with __u_ prefix to match typedef
          CHECK_OK(write_str(w, "__u_"));
          return write_ident(w, type->data.type_named.name);
        case TICK_TYPE_UNKNOWN:
          CHECK(0, "type not resolved - analysis pass missing?");
        default:
          CHECK(0, "unknown builtin type");
      }
    }

    case TICK_AST_TYPE_POINTER: {
      // Special case: pointer to function type
      // In Tick: *fn(params) return_type
      // In C: return_type (*)(params)
      tick_ast_node_t* pointee = type->data.type_pointer.pointee_type;
      if (pointee && pointee->kind == TICK_AST_TYPE_FUNCTION) {
        // Don't add extra *, the function type handling already includes it
        return codegen_type(pointee, w);
      }
      // Normal pointer
      CHECK_OK(codegen_type(pointee, w));
      return write_str(w, "*");
    }

    case TICK_AST_TYPE_ARRAY: {
      // For arrays, only emit the element type
      // The array brackets need to go after the name in C syntax
      return codegen_type(type->data.type_array.element_type, w);
    }

    case TICK_AST_TYPE_FUNCTION: {
      // Function type cannot be directly represented in C
      // It must be converted to a function pointer by wrapping in TYPE_POINTER
      // or used in a function declaration context
      // For now, we'll treat bare function types as function pointers
      CHECK_OK(codegen_type(type->data.type_function.return_type, w));
      CHECK_OK(write_str(w, " (*)("));

      // Emit parameter types
      tick_ast_node_t* param = type->data.type_function.params;
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
          CHECK_OK(codegen_type(param->data.param.type, w));
          param = param->next;
        }
      }

      return write_str(w, ")");
    }

    case TICK_AST_TYPE_OPTIONAL:
      CHECK(0, "TYPE_OPTIONAL must be lowered before codegen");

    case TICK_AST_TYPE_ERROR_UNION:
      CHECK(0, "TYPE_ERROR_UNION must be lowered before codegen");

    case TICK_AST_TYPE_SLICE:
      CHECK(0, "TYPE_SLICE must be lowered before codegen");

    default:
      CHECK(0, "unknown type kind");
  }
}

// ============================================================================
// Expression Generation
// ============================================================================

// Forward declarations
static tick_err_t codegen_expr(tick_ast_node_t* expr, codegen_ctx_t* ctx);
static tick_builtin_t map_binop_to_builtin(tick_binop_t op, tick_builtin_type_t type);
static const char* get_builtin_function_name(tick_builtin_t builtin, tick_builtin_type_t type);

static tick_err_t codegen_literal(tick_ast_node_t* node, codegen_ctx_t* ctx) {
  switch (node->data.literal.kind) {
    case TICK_LIT_UINT:
      return write_fmt(ctx->writer, "%llu", (unsigned long long)node->data.literal.data.uint_value);

    case TICK_LIT_INT:
      return write_fmt(ctx->writer, "%lld", (long long)node->data.literal.data.int_value);

    case TICK_LIT_BOOL:
      return write_str(ctx->writer, node->data.literal.data.bool_value ? "true" : "false");

    case TICK_LIT_STRING: {
      // Emit as const char* cast of u8 array: (const char*)(uint8_t[]){byte1, byte2, ..., 0}
      tick_buf_t str = node->data.literal.data.string_value;
      CHECK_OK(write_str(ctx->writer, "(const char*)(uint8_t[]){"));

      for (usz i = 0; i < str.sz; i++) {
        if (i > 0) {
          CHECK_OK(write_str(ctx->writer, ", "));
        }
        CHECK_OK(write_fmt(ctx->writer, "%u", (unsigned int)str.buf[i]));
      }

      // Add null terminator
      if (str.sz > 0) {
        CHECK_OK(write_str(ctx->writer, ", "));
      }
      CHECK_OK(write_str(ctx->writer, "0}"));
      return TICK_OK;
    }

    case TICK_LIT_NULL:
      CHECK(0, "null must be erased by analysis pass");

    case TICK_LIT_UNDEFINED:
      CHECK(0, "undefined must be handled before codegen_literal is called");

    default:
      CHECK(0, "unknown literal kind");
  }
}

static tick_err_t codegen_identifier(tick_ast_node_t* node, codegen_ctx_t* ctx) {
  // Check if this is an AT builtin
  if (node->data.identifier_expr.at_builtin != TICK_AT_BUILTIN_UNKNOWN) {
    // Emit runtime function name based on builtin enum
    switch (node->data.identifier_expr.at_builtin) {
      case TICK_AT_BUILTIN_DBG:
        return write_str(ctx->writer, "tick_debug_log");
      default:
        CHECK(0, "unknown AT builtin enum value");
    }
  }

  // User-written identifiers get __u_ prefix
  // TODO: This should check if the identifier refers to an extern/pub declaration
  // For now, we'll prefix all identifiers except when they're known to be extern/pub
  // A proper solution would require a symbol table lookup
  CHECK_OK(write_str(ctx->writer, "__u_"));
  return write_ident(ctx->writer, node->data.identifier_expr.name);
}

static tick_err_t codegen_binary_expr(tick_ast_node_t* node, codegen_ctx_t* ctx) {
  // Check if this is an arithmetic/shift operation that needs a builtin call
  tick_ast_node_t* resolved_type = node->data.binary_expr.resolved_type;

  if (resolved_type && resolved_type->kind == TICK_AST_TYPE_NAMED) {
    tick_builtin_type_t type = resolved_type->data.type_named.builtin_type;
    tick_builtin_t builtin = map_binop_to_builtin(node->data.binary_expr.op, type);

    if (builtin != 0) {
      // Emit as builtin function call
      const char* func_name = get_builtin_function_name(builtin, type);
      CHECK(func_name, "failed to get builtin function name");

      CHECK_OK(write_str(ctx->writer, func_name));
      CHECK_OK(write_str(ctx->writer, "("));
      CHECK_OK(codegen_expr(node->data.binary_expr.left, ctx));
      CHECK_OK(write_str(ctx->writer, ", "));
      CHECK_OK(codegen_expr(node->data.binary_expr.right, ctx));
      CHECK_OK(write_str(ctx->writer, ")"));
      return TICK_OK;
    }
  }

  // For unsigned wrapping operations or non-arithmetic operations,
  // emit direct C operator (comparisons, logical, bitwise)
  CHECK_OK(codegen_expr(node->data.binary_expr.left, ctx));
  CHECK_OK(write_str(ctx->writer, " "));
  CHECK_OK(write_str(ctx->writer, binop_to_c(node->data.binary_expr.op)));
  CHECK_OK(write_str(ctx->writer, " "));
  CHECK_OK(codegen_expr(node->data.binary_expr.right, ctx));
  return TICK_OK;
}

static tick_err_t codegen_unary_expr(tick_ast_node_t* node, codegen_ctx_t* ctx) {
  // Check if this is negation with a signed type (needs checked_neg builtin)
  if (node->data.unary_expr.op == UNOP_NEG) {
    tick_ast_node_t* resolved_type = node->data.unary_expr.resolved_type;

    if (resolved_type && resolved_type->kind == TICK_AST_TYPE_NAMED) {
      tick_builtin_type_t type = resolved_type->data.type_named.builtin_type;
      bool is_signed = (type >= TICK_TYPE_I8 && type <= TICK_TYPE_ISZ);

      if (is_signed) {
        // Emit checked_neg builtin call
        const char* func_name = get_builtin_function_name(TICK_BUILTIN_CHECKED_NEG, type);
        CHECK(func_name, "failed to get checked_neg function name");

        CHECK_OK(write_str(ctx->writer, func_name));
        CHECK_OK(write_str(ctx->writer, "("));
        CHECK_OK(codegen_expr(node->data.unary_expr.operand, ctx));
        CHECK_OK(write_str(ctx->writer, ")"));
        return TICK_OK;
      }
    }
  }

  // For other unary operations, emit direct C operator
  CHECK_OK(write_str(ctx->writer, unop_to_c(node->data.unary_expr.op)));
  CHECK_OK(codegen_expr(node->data.unary_expr.operand, ctx));
  return TICK_OK;
}

static tick_err_t codegen_call_expr(tick_ast_node_t* node, codegen_ctx_t* ctx) {
  CHECK_OK(codegen_expr(node->data.call_expr.callee, ctx));
  CHECK_OK(write_str(ctx->writer, "("));

  // Arguments are already in correct order (reversed by analysis pass)
  tick_ast_node_t* arg = node->data.call_expr.args;
  bool first = true;
  while (arg) {
    if (!first) {
      CHECK_OK(write_str(ctx->writer, ", "));
    }
    first = false;
    CHECK_OK(codegen_expr(arg, ctx));
    arg = arg->next;
  }

  CHECK_OK(write_str(ctx->writer, ")"));
  return TICK_OK;
}

static tick_err_t codegen_field_access(tick_ast_node_t* node, codegen_ctx_t* ctx) {
  CHECK_OK(codegen_expr(node->data.field_access_expr.object, ctx));
  const char* op = node->data.field_access_expr.is_arrow ? "->" : ".";
  CHECK_OK(write_str(ctx->writer, op));
  CHECK_OK(write_ident(ctx->writer, node->data.field_access_expr.field_name));
  return TICK_OK;
}

static tick_err_t codegen_index_expr(tick_ast_node_t* node, codegen_ctx_t* ctx) {
  CHECK_OK(codegen_expr(node->data.index_expr.array, ctx));
  CHECK_OK(write_str(ctx->writer, "["));
  CHECK_OK(codegen_expr(node->data.index_expr.index, ctx));
  CHECK_OK(write_str(ctx->writer, "]"));
  return TICK_OK;
}

// ============================================================================
// Cast Helper Functions
// ============================================================================

// Check if a builtin type is numeric (supports checked cast operations)
static bool is_numeric_builtin_type(tick_builtin_type_t type) {
  return (type >= TICK_TYPE_I8 && type <= TICK_TYPE_USZ);
}

// Check if a cast is a widening conversion (always safe, no runtime check needed)
static bool is_widening_cast(tick_builtin_type_t src, tick_builtin_type_t dst) {
  // Widening casts are safe when destination can represent all source values

  // Same type is trivially safe
  if (src == dst) return true;

  // Signed to signed widening
  if (src == TICK_TYPE_I8 && (dst == TICK_TYPE_I16 || dst == TICK_TYPE_I32 || dst == TICK_TYPE_I64 || dst == TICK_TYPE_ISZ)) return true;
  if (src == TICK_TYPE_I16 && (dst == TICK_TYPE_I32 || dst == TICK_TYPE_I64 || dst == TICK_TYPE_ISZ)) return true;
  if (src == TICK_TYPE_I32 && (dst == TICK_TYPE_I64 || dst == TICK_TYPE_ISZ)) return true;

  // Unsigned to unsigned widening
  if (src == TICK_TYPE_U8 && (dst == TICK_TYPE_U16 || dst == TICK_TYPE_U32 || dst == TICK_TYPE_U64 || dst == TICK_TYPE_USZ)) return true;
  if (src == TICK_TYPE_U16 && (dst == TICK_TYPE_U32 || dst == TICK_TYPE_U64 || dst == TICK_TYPE_USZ)) return true;
  if (src == TICK_TYPE_U32 && (dst == TICK_TYPE_U64 || dst == TICK_TYPE_USZ)) return true;

  // Unsigned to signed widening (safe if destination is larger)
  // u8 can fit in i16, i32, i64 (but not i8)
  if (src == TICK_TYPE_U8 && (dst == TICK_TYPE_I16 || dst == TICK_TYPE_I32 || dst == TICK_TYPE_I64 || dst == TICK_TYPE_ISZ)) return true;
  if (src == TICK_TYPE_U16 && (dst == TICK_TYPE_I32 || dst == TICK_TYPE_I64 || dst == TICK_TYPE_ISZ)) return true;
  if (src == TICK_TYPE_U32 && (dst == TICK_TYPE_I64 || dst == TICK_TYPE_ISZ)) return true;

  // Note: Platform-dependent types (isz, usz) conservatively assume 64-bit
  // TODO: Could optimize for known target platforms

  return false;
}

// Get the checked cast function name for a given source and destination type
// Returns NULL if no checked cast function exists (use bare C cast instead)
static const char* get_checked_cast_function_name(tick_builtin_type_t src, tick_builtin_type_t dst) {
  // Map type enum to string suffix
  const char* src_str = NULL;
  const char* dst_str = NULL;

  switch (src) {
    case TICK_TYPE_I8: src_str = "i8"; break;
    case TICK_TYPE_I16: src_str = "i16"; break;
    case TICK_TYPE_I32: src_str = "i32"; break;
    case TICK_TYPE_I64: src_str = "i64"; break;
    case TICK_TYPE_ISZ: src_str = "isz"; break;
    case TICK_TYPE_U8: src_str = "u8"; break;
    case TICK_TYPE_U16: src_str = "u16"; break;
    case TICK_TYPE_U32: src_str = "u32"; break;
    case TICK_TYPE_U64: src_str = "u64"; break;
    case TICK_TYPE_USZ: src_str = "usz"; break;
    default: return NULL;  // Not a numeric type
  }

  switch (dst) {
    case TICK_TYPE_I8: dst_str = "i8"; break;
    case TICK_TYPE_I16: dst_str = "i16"; break;
    case TICK_TYPE_I32: dst_str = "i32"; break;
    case TICK_TYPE_I64: dst_str = "i64"; break;
    case TICK_TYPE_ISZ: dst_str = "isz"; break;
    case TICK_TYPE_U8: dst_str = "u8"; break;
    case TICK_TYPE_U16: dst_str = "u16"; break;
    case TICK_TYPE_U32: dst_str = "u32"; break;
    case TICK_TYPE_U64: dst_str = "u64"; break;
    case TICK_TYPE_USZ: dst_str = "usz"; break;
    default: return NULL;  // Not a numeric type
  }

  // Build function name: tick_checked_cast_{src}_{dst}
  static char buf[64];
  snprintf(buf, sizeof(buf), "tick_checked_cast_%s_%s", src_str, dst_str);
  return buf;
}

// Try to determine the builtin type of an expression
// Returns TICK_TYPE_UNKNOWN if type cannot be determined
static tick_builtin_type_t get_expr_builtin_type(tick_ast_node_t* expr) {
  if (!expr) return TICK_TYPE_UNKNOWN;

  switch (expr->kind) {
    case TICK_AST_BINARY_EXPR:
      if (expr->data.binary_expr.resolved_type &&
          expr->data.binary_expr.resolved_type->kind == TICK_AST_TYPE_NAMED) {
        return expr->data.binary_expr.resolved_type->data.type_named.builtin_type;
      }
      break;

    case TICK_AST_UNARY_EXPR:
      if (expr->data.unary_expr.resolved_type &&
          expr->data.unary_expr.resolved_type->kind == TICK_AST_TYPE_NAMED) {
        return expr->data.unary_expr.resolved_type->data.type_named.builtin_type;
      }
      break;

    case TICK_AST_CAST_EXPR:
      if (expr->data.cast_expr.resolved_type &&
          expr->data.cast_expr.resolved_type->kind == TICK_AST_TYPE_NAMED) {
        return expr->data.cast_expr.resolved_type->data.type_named.builtin_type;
      }
      break;

    // For other expression types (literals, identifiers, calls, etc.),
    // we don't have type information readily available without a symbol table.
    // The analysis pass would need to annotate all expressions with types.
    default:
      break;
  }

  return TICK_TYPE_UNKNOWN;
}

static tick_err_t codegen_cast_expr(tick_ast_node_t* node, codegen_ctx_t* ctx) {
  // Get destination type (must be resolved by analysis pass)
  if (!node->data.cast_expr.type || node->data.cast_expr.type->kind != TICK_AST_TYPE_NAMED) {
    // Non-builtin type (pointer, struct, etc.) - emit bare C cast
    CHECK_OK(write_str(ctx->writer, "("));
    CHECK_OK(codegen_type(node->data.cast_expr.type, ctx->writer));
    CHECK_OK(write_str(ctx->writer, ")"));
    CHECK_OK(codegen_expr(node->data.cast_expr.expr, ctx));
    return TICK_OK;
  }

  tick_builtin_type_t dst_type = node->data.cast_expr.type->data.type_named.builtin_type;

  CHECK(dst_type != TICK_TYPE_UNKNOWN, "cast destination type not resolved - analysis pass missing?");

  // Get source type from the expression being cast
  tick_builtin_type_t src_type = get_expr_builtin_type(node->data.cast_expr.expr);

  // Decision tree for cast code generation:

  // 1. If source type is unknown (literals, identifiers without type annotation),
  //    we can't determine if checked cast is needed - emit bare C cast
  if (src_type == TICK_TYPE_UNKNOWN) {
    CHECK_OK(write_str(ctx->writer, "("));
    CHECK_OK(codegen_type(node->data.cast_expr.type, ctx->writer));
    CHECK_OK(write_str(ctx->writer, ")"));
    CHECK_OK(codegen_expr(node->data.cast_expr.expr, ctx));
    return TICK_OK;
  }

  // 2. If either type is non-numeric (bool, void, user-defined),
  //    emit bare C cast (no runtime checking available)
  if (!is_numeric_builtin_type(src_type) || !is_numeric_builtin_type(dst_type)) {
    CHECK_OK(write_str(ctx->writer, "("));
    CHECK_OK(codegen_type(node->data.cast_expr.type, ctx->writer));
    CHECK_OK(write_str(ctx->writer, ")"));
    CHECK_OK(codegen_expr(node->data.cast_expr.expr, ctx));
    return TICK_OK;
  }

  // 3. If it's a widening cast (always safe), emit bare C cast
  if (is_widening_cast(src_type, dst_type)) {
    CHECK_OK(write_str(ctx->writer, "("));
    CHECK_OK(codegen_type(node->data.cast_expr.type, ctx->writer));
    CHECK_OK(write_str(ctx->writer, ")"));
    CHECK_OK(codegen_expr(node->data.cast_expr.expr, ctx));
    return TICK_OK;
  }

  // 4. Otherwise, it's a narrowing or sign-changing cast - emit checked cast
  const char* cast_func = get_checked_cast_function_name(src_type, dst_type);
  CHECK(cast_func, "failed to get checked cast function name");

  // Emit: tick_checked_cast_src_dst(expr)
  CHECK_OK(write_str(ctx->writer, cast_func));
  CHECK_OK(write_str(ctx->writer, "("));
  CHECK_OK(codegen_expr(node->data.cast_expr.expr, ctx));
  CHECK_OK(write_str(ctx->writer, ")"));

  return TICK_OK;
}

// Map binary operation + type to appropriate builtin
static tick_builtin_t map_binop_to_builtin(tick_binop_t op, tick_builtin_type_t type) {
  // Check if type is signed for wrapping operations
  bool is_signed = (type >= TICK_TYPE_I8 && type <= TICK_TYPE_ISZ);

  switch (op) {
    // Saturating operations
    case BINOP_SAT_ADD: return TICK_BUILTIN_SAT_ADD;
    case BINOP_SAT_SUB: return TICK_BUILTIN_SAT_SUB;
    case BINOP_SAT_MUL: return TICK_BUILTIN_SAT_MUL;
    case BINOP_SAT_DIV: return TICK_BUILTIN_SAT_DIV;

    // Wrapping operations - only for signed types
    case BINOP_WRAP_ADD:
      return is_signed ? TICK_BUILTIN_WRAP_ADD : (tick_builtin_t)0;  // Unsigned: no builtin needed
    case BINOP_WRAP_SUB:
      return is_signed ? TICK_BUILTIN_WRAP_SUB : (tick_builtin_t)0;
    case BINOP_WRAP_MUL:
      return is_signed ? TICK_BUILTIN_WRAP_MUL : (tick_builtin_t)0;
    case BINOP_WRAP_DIV:
      return is_signed ? TICK_BUILTIN_WRAP_DIV : (tick_builtin_t)0;

    // Default arithmetic - checked for signed, wrapping for unsigned
    case BINOP_ADD:
      return is_signed ? TICK_BUILTIN_CHECKED_ADD : TICK_BUILTIN_WRAP_ADD;
    case BINOP_SUB:
      return is_signed ? TICK_BUILTIN_CHECKED_SUB : TICK_BUILTIN_WRAP_SUB;
    case BINOP_MUL:
      return is_signed ? TICK_BUILTIN_CHECKED_MUL : TICK_BUILTIN_WRAP_MUL;

    // Division and modulo - always checked
    case BINOP_DIV: return TICK_BUILTIN_CHECKED_DIV;
    case BINOP_MOD: return TICK_BUILTIN_CHECKED_MOD;

    // Shifts - always checked
    case BINOP_LSHIFT: return TICK_BUILTIN_CHECKED_SHL;
    case BINOP_RSHIFT: return TICK_BUILTIN_CHECKED_SHR;

    default:
      return (tick_builtin_t)0;  // Not an arithmetic operation
  }
}

// Map builtin + type to runtime function name
static const char* get_builtin_function_name(tick_builtin_t builtin, tick_builtin_type_t type) {
  // Determine the operation suffix and prefix
  const char* op_suffix = NULL;
  const char* op_prefix = NULL;

  switch (builtin) {
    case TICK_BUILTIN_SAT_ADD: op_prefix = "tick_sat"; op_suffix = "add"; break;
    case TICK_BUILTIN_SAT_SUB: op_prefix = "tick_sat"; op_suffix = "sub"; break;
    case TICK_BUILTIN_SAT_MUL: op_prefix = "tick_sat"; op_suffix = "mul"; break;
    case TICK_BUILTIN_SAT_DIV: op_prefix = "tick_sat"; op_suffix = "div"; break;
    case TICK_BUILTIN_WRAP_ADD: op_prefix = "tick_wrap"; op_suffix = "add"; break;
    case TICK_BUILTIN_WRAP_SUB: op_prefix = "tick_wrap"; op_suffix = "sub"; break;
    case TICK_BUILTIN_WRAP_MUL: op_prefix = "tick_wrap"; op_suffix = "mul"; break;
    case TICK_BUILTIN_WRAP_DIV: op_prefix = "tick_wrap"; op_suffix = "div"; break;
    case TICK_BUILTIN_CHECKED_ADD: op_prefix = "tick_checked"; op_suffix = "add"; break;
    case TICK_BUILTIN_CHECKED_SUB: op_prefix = "tick_checked"; op_suffix = "sub"; break;
    case TICK_BUILTIN_CHECKED_MUL: op_prefix = "tick_checked"; op_suffix = "mul"; break;
    case TICK_BUILTIN_CHECKED_DIV: op_prefix = "tick_checked"; op_suffix = "div"; break;
    case TICK_BUILTIN_CHECKED_MOD: op_prefix = "tick_checked"; op_suffix = "mod"; break;
    case TICK_BUILTIN_CHECKED_SHL: op_prefix = "tick_checked"; op_suffix = "shl"; break;
    case TICK_BUILTIN_CHECKED_SHR: op_prefix = "tick_checked"; op_suffix = "shr"; break;
    case TICK_BUILTIN_CHECKED_NEG: op_prefix = "tick_checked"; op_suffix = "neg"; break;
    case TICK_BUILTIN_CHECKED_CAST:
      // Cast is handled specially - needs both source and dest types
      return NULL;
    default: return NULL;
  }

  // For wrapping operations, only signed types need wrapping functions
  // Unsigned types have guaranteed modulo semantics in C
  bool is_wrap = (builtin >= TICK_BUILTIN_WRAP_ADD && builtin <= TICK_BUILTIN_WRAP_DIV);

  // Map type enum to suffix
  // Format: {prefix}_{op}_{type}
  static char buf[32];
  const char* type_suffix = NULL;

  switch (type) {
    case TICK_TYPE_I8: type_suffix = "i8"; break;
    case TICK_TYPE_I16: type_suffix = "i16"; break;
    case TICK_TYPE_I32: type_suffix = "i32"; break;
    case TICK_TYPE_I64: type_suffix = "i64"; break;
    case TICK_TYPE_ISZ: type_suffix = "isz"; break;
    case TICK_TYPE_U8:
      CHECK(!is_wrap, "wrapping builtin used with unsigned type - unsigned types don't need wrapping");
      type_suffix = "u8";
      break;
    case TICK_TYPE_U16:
      CHECK(!is_wrap, "wrapping builtin used with unsigned type - unsigned types don't need wrapping");
      type_suffix = "u16";
      break;
    case TICK_TYPE_U32:
      CHECK(!is_wrap, "wrapping builtin used with unsigned type - unsigned types don't need wrapping");
      type_suffix = "u32";
      break;
    case TICK_TYPE_U64:
      CHECK(!is_wrap, "wrapping builtin used with unsigned type - unsigned types don't need wrapping");
      type_suffix = "u64";
      break;
    case TICK_TYPE_USZ:
      CHECK(!is_wrap, "wrapping builtin used with unsigned type - unsigned types don't need wrapping");
      type_suffix = "usz";
      break;
    default:
      CHECK(0, "builtin operation used with non-numeric type");
  }

  snprintf(buf, sizeof(buf), "%s_%s_%s", op_prefix, op_suffix, type_suffix);
  return buf;
}

static tick_err_t codegen_builtin_call(tick_ast_node_t* node, codegen_ctx_t* ctx) {
  // Special handling for CHECKED_CAST - needs both source and dest type
  CHECK(node->data.builtin_call.builtin != TICK_BUILTIN_CHECKED_CAST, "checked cast not yet implemented");

  // Get the builtin type
  CHECK(node->data.builtin_call.type && node->data.builtin_call.type->kind == TICK_AST_TYPE_NAMED,
        "builtin call missing or invalid type");

  tick_builtin_type_t type = node->data.builtin_call.type->data.type_named.builtin_type;
  CHECK(type != TICK_TYPE_UNKNOWN, "builtin call type not resolved - analysis pass missing?");

  const char* func_name = get_builtin_function_name(node->data.builtin_call.builtin, type);
  CHECK(func_name, "unknown builtin or type for builtin call");

  // Emit function call
  CHECK_OK(write_str(ctx->writer, func_name));
  CHECK_OK(write_str(ctx->writer, "("));

  tick_ast_node_t* arg = node->data.builtin_call.args;
  bool first = true;
  while (arg) {
    if (!first) {
      CHECK_OK(write_str(ctx->writer, ", "));
    }
    first = false;
    CHECK_OK(codegen_expr(arg, ctx));
    arg = arg->next;
  }

  CHECK_OK(write_str(ctx->writer, ")"));
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
    case TICK_AST_BUILTIN_CALL:
      return codegen_builtin_call(expr, ctx);
    case TICK_AST_FIELD_ACCESS_EXPR:
      return codegen_field_access(expr, ctx);
    case TICK_AST_INDEX_EXPR:
      return codegen_index_expr(expr, ctx);
    case TICK_AST_CAST_EXPR:
      return codegen_cast_expr(expr, ctx);

    // High-level expressions that must be lowered before codegen
    case TICK_AST_ASYNC_EXPR:
      CHECK(0, "async must be lowered before codegen (use async state machine)");

    case TICK_AST_STRUCT_INIT_EXPR: {
      // Emit: (TypeName) { .field1 = val1, .field2 = val2 }
      // Requires: All field values must be LITERAL or IDENTIFIER_EXPR (lowering decomposed complex expressions)

      CHECK_OK(write_str(ctx->writer, "("));
      CHECK_OK(codegen_type(expr->data.struct_init_expr.type, ctx->writer));
      CHECK_OK(write_str(ctx->writer, ") { "));

      tick_ast_node_t* field = expr->data.struct_init_expr.fields;
      bool first = true;
      while (field) {
        CHECK(field->kind == TICK_AST_STRUCT_INIT_FIELD, "expected STRUCT_INIT_FIELD in struct initializer");

        tick_ast_node_t* value = field->data.struct_init_field.value;

        if (!first) {
          CHECK_OK(write_str(ctx->writer, ", "));
        }
        first = false;

        // Emit: .fieldname = value
        CHECK_OK(write_str(ctx->writer, "."));
        CHECK_OK(write_ident(ctx->writer, field->data.struct_init_field.field_name));
        CHECK_OK(write_str(ctx->writer, " = "));
        CHECK_OK(codegen_expr(value, ctx));

        field = field->next;
      }

      CHECK_OK(write_str(ctx->writer, " }"));
      return TICK_OK;
    }

    case TICK_AST_ARRAY_INIT_EXPR: {
      // Emit: { elem1, elem2, elem3 }

      CHECK_OK(write_str(ctx->writer, "{ "));

      // Iterate elements in source order
      tick_ast_node_t* elem = expr->data.array_init_expr.elements;
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

    default:
      CHECK(0, "unsupported expression kind");
  }
}

// ============================================================================
// Statement Generation
// ============================================================================

static tick_err_t codegen_stmt(tick_ast_node_t* stmt, codegen_ctx_t* ctx, int indent);

static tick_err_t codegen_block_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx, int indent) {
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "{\n"));

  tick_ast_node_t* stmt = node->data.block_stmt.stmts;
  while (stmt) {
    CHECK_OK(codegen_stmt(stmt, ctx, indent + 1));
    stmt = stmt->next;
  }

  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "}\n"));
  return TICK_OK;
}

static tick_err_t codegen_return_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx, int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "return"));
  if (node->data.return_stmt.value) {
    CHECK_OK(write_str(ctx->writer, " "));
    CHECK_OK(codegen_expr(node->data.return_stmt.value, ctx));
  }
  CHECK_OK(write_str(ctx->writer, ";\n"));
  return TICK_OK;
}

static tick_err_t codegen_let_or_var_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx, int indent, bool is_var) {
  UNUSED(is_var);  // In C, both let and var are just regular variables
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));

  // Emit volatile qualifier if present
  if (node->data.decl.quals.is_volatile) {
    CHECK_OK(write_str(ctx->writer, "volatile "));
  }

  // Emit type
  CHECK_OK(codegen_type(node->data.decl.type, ctx->writer));
  CHECK_OK(write_str(ctx->writer, " "));

  // Emit name (with appropriate prefix based on tmpid)
  CHECK_OK(write_decl_name(ctx->writer, node));
  CHECK_OK(write_array_suffix(ctx->writer, node->data.decl.type));

  // Emit initializer if present and not undefined
  if (node->data.decl.init) {
    // Check if init is undefined literal
    bool is_undefined = (node->data.decl.init->kind == TICK_AST_LITERAL &&
                        node->data.decl.init->data.literal.kind == TICK_LIT_UNDEFINED);

    if (!is_undefined) {
      CHECK_OK(write_str(ctx->writer, " = "));
      CHECK_OK(codegen_expr(node->data.decl.init, ctx));
    }
  }

  CHECK_OK(write_str(ctx->writer, ";\n"));
  return TICK_OK;
}

static tick_err_t codegen_assign_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx, int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(codegen_expr(node->data.assign_stmt.lhs, ctx));
  CHECK_OK(write_str(ctx->writer, " = "));
  CHECK_OK(codegen_expr(node->data.assign_stmt.rhs, ctx));
  CHECK_OK(write_str(ctx->writer, ";\n"));
  return TICK_OK;
}

static tick_err_t codegen_unused_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx, int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "TICK_UNUSED("));
  CHECK_OK(codegen_expr(node->data.unused_stmt.expr, ctx));
  CHECK_OK(write_str(ctx->writer, ");\n"));
  return TICK_OK;
}

static tick_err_t codegen_expr_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx, int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(codegen_expr(node->data.expr_stmt.expr, ctx));
  CHECK_OK(write_str(ctx->writer, ";\n"));
  return TICK_OK;
}

static tick_err_t codegen_if_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx, int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "if ("));
  CHECK_OK(codegen_expr(node->data.if_stmt.condition, ctx));
  CHECK_OK(write_str(ctx->writer, ") "));

  // Then block (strip outer braces, we add them)
  if (node->data.if_stmt.then_block->kind == TICK_AST_BLOCK_STMT) {
    CHECK_OK(write_str(ctx->writer, "{\n"));
    tick_ast_node_t* stmt = node->data.if_stmt.then_block->data.block_stmt.stmts;
    while (stmt) {
      CHECK_OK(codegen_stmt(stmt, ctx, indent + 1));
      stmt = stmt->next;
    }
    CHECK_OK(write_indent(ctx->writer, indent));
    CHECK_OK(write_str(ctx->writer, "}"));
  } else {
    CHECK_OK(write_str(ctx->writer, "\n"));
    CHECK_OK(codegen_stmt(node->data.if_stmt.then_block, ctx, indent + 1));
    CHECK_OK(write_indent(ctx->writer, indent));
  }

  // Else block if present
  if (node->data.if_stmt.else_block) {
    CHECK_OK(write_str(ctx->writer, " else "));
    if (node->data.if_stmt.else_block->kind == TICK_AST_IF_STMT) {
      // else if - no braces
      CHECK_OK(codegen_stmt(node->data.if_stmt.else_block, ctx, 0));
    } else if (node->data.if_stmt.else_block->kind == TICK_AST_BLOCK_STMT) {
      CHECK_OK(write_str(ctx->writer, "{\n"));
      tick_ast_node_t* stmt = node->data.if_stmt.else_block->data.block_stmt.stmts;
      while (stmt) {
        CHECK_OK(codegen_stmt(stmt, ctx, indent + 1));
        stmt = stmt->next;
      }
      CHECK_OK(write_indent(ctx->writer, indent));
      CHECK_OK(write_str(ctx->writer, "}\n"));
    } else {
      CHECK_OK(write_str(ctx->writer, "\n"));
      CHECK_OK(codegen_stmt(node->data.if_stmt.else_block, ctx, indent + 1));
    }
  } else {
    CHECK_OK(write_str(ctx->writer, "\n"));
  }

  return TICK_OK;
}

static tick_err_t codegen_break_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx, int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "break;\n"));
  return TICK_OK;
}

static tick_err_t codegen_continue_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx, int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "continue;\n"));
  return TICK_OK;
}

static tick_err_t codegen_goto_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx, int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "goto "));
  CHECK_OK(write_ident(ctx->writer, node->data.goto_stmt.label));
  CHECK_OK(write_str(ctx->writer, ";\n"));
  return TICK_OK;
}

static tick_err_t codegen_label_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx, int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_ident(ctx->writer, node->data.label_stmt.label));
  CHECK_OK(write_str(ctx->writer, ":\n"));
  return TICK_OK;
}

static tick_err_t codegen_for_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx, int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));

  // Emit init statement before the loop (if present)
  if (node->data.for_stmt.init_stmt) {
    CHECK_OK(codegen_stmt(node->data.for_stmt.init_stmt, ctx, indent));
  }

  // All for loops become while (1)
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "while (1) {\n"));

  // Emit condition check as if (!condition) break; (if present)
  if (node->data.for_stmt.condition) {
    CHECK_OK(write_indent(ctx->writer, indent + 1));
    CHECK_OK(write_str(ctx->writer, "if (!("));
    CHECK_OK(codegen_expr(node->data.for_stmt.condition, ctx));
    CHECK_OK(write_str(ctx->writer, ")) break;\n"));
  }

  // Emit loop body
  if (node->data.for_stmt.body) {
    if (node->data.for_stmt.body->kind == TICK_AST_BLOCK_STMT) {
      // Emit statements directly without extra braces
      tick_ast_node_t* stmt = node->data.for_stmt.body->data.block_stmt.stmts;
      while (stmt) {
        CHECK_OK(codegen_stmt(stmt, ctx, indent + 1));
        stmt = stmt->next;
      }
    } else {
      // Single statement body
      CHECK_OK(codegen_stmt(node->data.for_stmt.body, ctx, indent + 1));
    }
  }

  // Emit step statement at end of loop (if present)
  if (node->data.for_stmt.step_stmt) {
    CHECK_OK(codegen_stmt(node->data.for_stmt.step_stmt, ctx, indent + 1));
  }

  // Close while loop
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "}\n"));

  return TICK_OK;
}

static tick_err_t codegen_switch_stmt(tick_ast_node_t* node, codegen_ctx_t* ctx, int indent) {
  CHECK_OK(write_line_directive(ctx, node->loc.line));
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "switch ("));
  CHECK_OK(codegen_expr(node->data.switch_stmt.value, ctx));
  CHECK_OK(write_str(ctx->writer, ") {\n"));

  // Iterate through cases
  tick_ast_node_t* case_node = node->data.switch_stmt.cases;
  while (case_node) {
    if (case_node->data.switch_case.values == NULL) {
      // Default case
      CHECK_OK(write_indent(ctx->writer, indent + 1));
      CHECK_OK(write_str(ctx->writer, "default:\n"));
    } else {
      // Regular case with values
      tick_ast_node_t* value = case_node->data.switch_case.values;
      while (value) {
        CHECK_OK(write_indent(ctx->writer, indent + 1));
        CHECK_OK(write_str(ctx->writer, "case "));
        CHECK_OK(codegen_expr(value, ctx));
        CHECK_OK(write_str(ctx->writer, ":\n"));
        value = value->next;
      }
    }

    // Open block for case statements (prevents variable declaration conflicts)
    CHECK_OK(write_indent(ctx->writer, indent + 1));
    CHECK_OK(write_str(ctx->writer, "{\n"));

    // Emit case statements
    tick_ast_node_t* stmt = case_node->data.switch_case.stmts;
    while (stmt) {
      CHECK_OK(codegen_stmt(stmt, ctx, indent + 2));
      stmt = stmt->next;
    }

    // Always emit break (harmless even after return/break/continue)
    CHECK_OK(write_indent(ctx->writer, indent + 2));
    CHECK_OK(write_str(ctx->writer, "break;\n"));

    // Close block
    CHECK_OK(write_indent(ctx->writer, indent + 1));
    CHECK_OK(write_str(ctx->writer, "}\n"));

    case_node = case_node->next;
  }

  // Close switch
  CHECK_OK(write_indent(ctx->writer, indent));
  CHECK_OK(write_str(ctx->writer, "}\n"));

  return TICK_OK;
}

static tick_err_t codegen_stmt(tick_ast_node_t* stmt, codegen_ctx_t* ctx, int indent) {
  if (!stmt) return TICK_OK;

  switch (stmt->kind) {
    case TICK_AST_BLOCK_STMT:
      return codegen_block_stmt(stmt, ctx, indent);
    case TICK_AST_RETURN_STMT:
      return codegen_return_stmt(stmt, ctx, indent);
    case TICK_AST_LET_STMT:
      return codegen_let_or_var_stmt(stmt, ctx, indent, false);
    case TICK_AST_VAR_STMT:
      return codegen_let_or_var_stmt(stmt, ctx, indent, true);
    case TICK_AST_ASSIGN_STMT:
      return codegen_assign_stmt(stmt, ctx, indent);
    case TICK_AST_UNUSED_STMT:
      return codegen_unused_stmt(stmt, ctx, indent);
    case TICK_AST_EXPR_STMT:
      return codegen_expr_stmt(stmt, ctx, indent);
    case TICK_AST_IF_STMT:
      return codegen_if_stmt(stmt, ctx, indent);
    case TICK_AST_FOR_STMT:
      return codegen_for_stmt(stmt, ctx, indent);
    case TICK_AST_SWITCH_STMT:
      return codegen_switch_stmt(stmt, ctx, indent);
    case TICK_AST_BREAK_STMT:
      return codegen_break_stmt(stmt, ctx, indent);
    case TICK_AST_CONTINUE_STMT:
      return codegen_continue_stmt(stmt, ctx, indent);
    case TICK_AST_GOTO_STMT:
      return codegen_goto_stmt(stmt, ctx, indent);
    case TICK_AST_LABEL_STMT:
      return codegen_label_stmt(stmt, ctx, indent);
    case TICK_AST_DECL:
      // Declaration as statement (let/var)
      return codegen_let_or_var_stmt(stmt, ctx, indent, stmt->data.decl.quals.is_var);

    // High-level statements that must be lowered before codegen
    case TICK_AST_DEFER_STMT:
      CHECK(0, "defer must be lowered before codegen (use goto/label)");
    case TICK_AST_ERRDEFER_STMT:
      CHECK(0, "errdefer must be lowered before codegen (use goto/label)");
    case TICK_AST_SUSPEND_STMT:
      CHECK(0, "suspend must be lowered before codegen (use async state machine)");
    case TICK_AST_RESUME_STMT:
      CHECK(0, "resume must be lowered before codegen (use async state machine)");
    case TICK_AST_TRY_CATCH_STMT:
      CHECK(0, "try/catch must be lowered before codegen");
    case TICK_AST_FOR_SWITCH_STMT:
      CHECK(0, "for-switch must be lowered before codegen");
    case TICK_AST_CONTINUE_SWITCH_STMT:
      CHECK(0, "continue-switch must be lowered before codegen");

    default:
      CHECK(0, "unsupported statement kind");
  }
}

// ============================================================================
// Utility Functions
// ============================================================================

// ============================================================================
// Type Declaration Generation
// ============================================================================

// Emit struct declaration to header or C file
static tick_err_t codegen_struct_decl(tick_ast_node_t* decl, tick_writer_t* w, bool full_definition) {
  tick_ast_node_t* struct_node = decl->data.decl.init;
  CHECK(struct_node && struct_node->kind == TICK_AST_STRUCT_DECL, "expected STRUCT_DECL");

  if (!full_definition) {
    // Forward declaration
    CHECK_OK(write_str(w, "typedef struct "));
    CHECK_OK(write_str(w, "__u_"));
    CHECK_OK(write_ident(w, decl->data.decl.name));
    CHECK_OK(write_str(w, " "));
    CHECK_OK(write_str(w, "__u_"));
    CHECK_OK(write_ident(w, decl->data.decl.name));
    CHECK_OK(write_str(w, ";\n"));
    return TICK_OK;
  }

  // Full definition - use "struct __u_Name { ... };" to avoid typedef redefinition
  CHECK_OK(write_str(w, "struct "));

  // Handle packed attribute
  if (struct_node->data.struct_decl.is_packed) {
    CHECK_OK(write_str(w, "__attribute__((packed)) "));
  }

  // Handle struct-level alignment
  if (struct_node->data.struct_decl.alignment) {
    CHECK(struct_node->data.struct_decl.alignment->kind == TICK_AST_LITERAL,
          "alignment must be literal (analysis didn't evaluate constant)");
    uint64_t align_val = struct_node->data.struct_decl.alignment->data.literal.data.uint_value;
    CHECK_OK(write_fmt(w, "__attribute__((aligned(%llu))) ", (unsigned long long)align_val));
  }

  CHECK_OK(write_str(w, "__u_"));
  CHECK_OK(write_ident(w, decl->data.decl.name));
  CHECK_OK(write_str(w, " {\n"));

  // Emit fields in source order
  tick_ast_node_t* field = struct_node->data.struct_decl.fields;
  while (field) {
    CHECK(field->kind == TICK_AST_FIELD, "expected FIELD node in struct");

    CHECK_OK(write_str(w, "  "));
    CHECK_OK(codegen_type(field->data.field.type, w));
    CHECK_OK(write_str(w, " "));
    CHECK_OK(write_ident(w, field->data.field.name));

    // Handle per-field alignment
    if (field->data.field.alignment) {
      CHECK(field->data.field.alignment->kind == TICK_AST_LITERAL,
            "field alignment must be literal (analysis didn't evaluate constant)");
      uint64_t align_val = field->data.field.alignment->data.literal.data.uint_value;
      CHECK_OK(write_fmt(w, " __attribute__((aligned(%llu)))", (unsigned long long)align_val));
    }

    CHECK_OK(write_str(w, ";\n"));
    field = field->next;
  }

  CHECK_OK(write_str(w, "};\n\n"));

  return TICK_OK;
}

// Emit enum declaration to header or C file
static tick_err_t codegen_enum_decl(tick_ast_node_t* decl, tick_writer_t* w) {
  tick_ast_node_t* enum_node = decl->data.decl.init;
  CHECK(enum_node && enum_node->kind == TICK_AST_ENUM_DECL, "expected ENUM_DECL");

  // Emit enum with underlying type
  CHECK_OK(write_str(w, "typedef enum {\n"));

  // Emit values (already in correct order from lowering pass)
  tick_ast_node_t* value = enum_node->data.enum_decl.values;
  bool first = true;
  while (value) {
    CHECK(value->kind == TICK_AST_ENUM_VALUE, "expected ENUM_VALUE node in enum");

    if (!first) {
      CHECK_OK(write_str(w, ",\n"));
    }
    first = false;

    CHECK_OK(write_str(w, "  "));
    CHECK_OK(write_str(w, "__u_"));
    CHECK_OK(write_ident(w, decl->data.decl.name));
    CHECK_OK(write_str(w, "_"));
    CHECK_OK(write_ident(w, value->data.enum_value.name));

    // Value must be a literal (analysis calculated auto-increment)
    if (value->data.enum_value.value) {
      CHECK(value->data.enum_value.value->kind == TICK_AST_LITERAL,
            "enum value must be literal (analysis didn't calculate auto-increment)");
      uint64_t val = value->data.enum_value.value->data.literal.data.uint_value;
      CHECK_OK(write_fmt(w, " = %llu", (unsigned long long)val));
    } else {
      CHECK(0, "enum value is NULL (analysis didn't calculate auto-increment)");
    }

    value = value->next;
  }

  CHECK_OK(write_str(w, "\n} "));

  // Cast to underlying type
  CHECK_OK(write_str(w, "__u_"));
  CHECK_OK(write_ident(w, decl->data.decl.name));
  CHECK_OK(write_str(w, ";\n\n"));

  return TICK_OK;
}

// Emit union declaration to header or C file
static tick_err_t codegen_union_decl(tick_ast_node_t* decl, tick_writer_t* w, bool full_definition) {
  tick_ast_node_t* union_node = decl->data.decl.init;
  CHECK(union_node && union_node->kind == TICK_AST_UNION_DECL, "expected UNION_DECL");

  // Unions must have explicit tag type (lowering generates it if auto)
  CHECK(union_node->data.union_decl.tag_type != NULL,
        "union has NULL tag_type (lowering didn't generate tag enum)");

  if (!full_definition) {
    // Forward declaration
    CHECK_OK(write_str(w, "typedef struct "));
    CHECK_OK(write_str(w, "__u_"));
    CHECK_OK(write_ident(w, decl->data.decl.name));
    CHECK_OK(write_str(w, " "));
    CHECK_OK(write_str(w, "__u_"));
    CHECK_OK(write_ident(w, decl->data.decl.name));
    CHECK_OK(write_str(w, ";\n"));
    return TICK_OK;
  }

  // Full definition - emit tagged union structure using "struct __u_Name { ... };"
  CHECK_OK(write_str(w, "struct "));

  // Handle alignment
  if (union_node->data.union_decl.alignment) {
    CHECK(union_node->data.union_decl.alignment->kind == TICK_AST_LITERAL,
          "alignment must be literal (analysis didn't evaluate constant)");
    uint64_t align_val = union_node->data.union_decl.alignment->data.literal.data.uint_value;
    CHECK_OK(write_fmt(w, "__attribute__((aligned(%llu))) ", (unsigned long long)align_val));
  }

  CHECK_OK(write_str(w, "__u_"));
  CHECK_OK(write_ident(w, decl->data.decl.name));
  CHECK_OK(write_str(w, " {\n"));

  // Emit tag field
  CHECK_OK(write_str(w, "  "));
  CHECK_OK(codegen_type(union_node->data.union_decl.tag_type, w));
  CHECK_OK(write_str(w, " tag;\n"));

  // Emit data union
  CHECK_OK(write_str(w, "  union {\n"));

  // Emit fields in source order
  tick_ast_node_t* field = union_node->data.union_decl.fields;
  while (field) {
    CHECK(field->kind == TICK_AST_FIELD, "expected FIELD node in union");

    CHECK_OK(write_str(w, "    "));
    CHECK_OK(codegen_type(field->data.field.type, w));
    CHECK_OK(write_str(w, " "));
    CHECK_OK(write_ident(w, field->data.field.name));
    CHECK_OK(write_str(w, ";\n"));

    field = field->next;
  }

  CHECK_OK(write_str(w, "  } data;\n"));
  CHECK_OK(write_str(w, "};\n\n"));

  return TICK_OK;
}

// ============================================================================
// Function Generation
// ============================================================================

static tick_err_t codegen_function_signature(tick_ast_node_t* decl, codegen_ctx_t* ctx, bool include_param_names) {
  tick_ast_node_t* func = decl->data.decl.init;

  // Return type
  CHECK_OK(codegen_type(func->data.function.return_type, ctx->writer));
  CHECK_OK(write_str(ctx->writer, " "));

  // Function name (with __u_ prefix)
  CHECK_OK(write_decl_name(ctx->writer, decl));

  // Parameters in source order
  CHECK_OK(write_str(ctx->writer, "("));
  tick_ast_node_t* param = func->data.function.params;

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

      CHECK_OK(codegen_type(param->data.param.type, ctx->writer));

      // Include parameter names only in implementation
      if (include_param_names) {
        CHECK_OK(write_str(ctx->writer, " "));
        // Parameter names get __u_ prefix
        CHECK_OK(write_str(ctx->writer, "__u_"));
        CHECK_OK(write_ident(ctx->writer, param->data.param.name));
      }

      param = param->next;
    }
  }
  CHECK_OK(write_str(ctx->writer, ")"));

  return TICK_OK;
}

static tick_err_t codegen_function_decl_h(tick_ast_node_t* decl, codegen_ctx_t* ctx) {
  CHECK_OK(codegen_function_signature(decl, ctx, false));  // No parameter names in header
  CHECK_OK(write_str(ctx->writer, ";\n"));
  return TICK_OK;
}

static tick_err_t codegen_function_decl_c(tick_ast_node_t* decl, codegen_ctx_t* ctx) {
  tick_ast_node_t* func = decl->data.decl.init;

  // Emit #line directive for function start
  CHECK_OK(write_line_directive(ctx, decl->loc.line));

  // Function signature
  CHECK_OK(codegen_function_signature(decl, ctx, true));  // Include parameter names in implementation
  CHECK_OK(write_str(ctx->writer, " "));

  // Function body
  if (func->data.function.body) {
    CHECK(func->data.function.body->kind == TICK_AST_BLOCK_STMT, "function body is not a block statement");
    CHECK_OK(codegen_block_stmt(func->data.function.body, ctx, 0));
  } else {
    CHECK_OK(write_str(ctx->writer, ";\n"));
  }

  CHECK_OK(write_str(ctx->writer, "\n"));
  return TICK_OK;
}

// ============================================================================
// Global Variable Generation
// ============================================================================

static tick_err_t codegen_global_var_h(tick_ast_node_t* decl, codegen_ctx_t* ctx) {
  // Emit extern declaration in header
  bool is_volatile = decl->data.decl.quals.is_volatile;
  bool is_extern = decl->data.decl.quals.is_extern;
  UNUSED(is_extern);  // Both extern and non-extern emit the same in header

  CHECK_OK(write_str(ctx->writer, "extern "));
  if (is_volatile) {
    CHECK_OK(write_str(ctx->writer, "volatile "));
  }
  CHECK_OK(codegen_type(decl->data.decl.type, ctx->writer));
  CHECK_OK(write_str(ctx->writer, " "));
  CHECK_OK(write_decl_name(ctx->writer, decl));
  CHECK_OK(write_array_suffix(ctx->writer, decl->data.decl.type));
  CHECK_OK(write_str(ctx->writer, ";\n"));

  return TICK_OK;
}

static tick_err_t codegen_global_var_c(tick_ast_node_t* decl, codegen_ctx_t* ctx) {
  bool is_volatile = decl->data.decl.quals.is_volatile;
  bool is_extern = decl->data.decl.quals.is_extern;
  tick_ast_node_t* type = decl->data.decl.type;

  CHECK_OK(write_line_directive(ctx, decl->loc.line));

  // Special case: function pointer (pointer to function type)
  // In C: return_type (*name)(params)
  if (type && type->kind == TICK_AST_TYPE_POINTER &&
      type->data.type_pointer.pointee_type &&
      type->data.type_pointer.pointee_type->kind == TICK_AST_TYPE_FUNCTION) {
    tick_ast_node_t* fn_type = type->data.type_pointer.pointee_type;

    if (is_extern) {
      CHECK_OK(write_str(ctx->writer, "extern "));
    }
    if (is_volatile) {
      CHECK_OK(write_str(ctx->writer, "volatile "));
    }

    // Emit return type
    CHECK_OK(codegen_type(fn_type->data.type_function.return_type, ctx->writer));
    CHECK_OK(write_str(ctx->writer, " (*"));
    CHECK_OK(write_decl_name(ctx->writer, decl));
    CHECK_OK(write_str(ctx->writer, ")("));

    // Emit parameter types
    tick_ast_node_t* param = fn_type->data.type_function.params;
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
        CHECK_OK(codegen_type(param->data.param.type, ctx->writer));
        param = param->next;
      }
    }

    CHECK_OK(write_str(ctx->writer, ")"));
    goto finish_decl;
  }

  // Normal variable declaration
  if (is_extern) {
    CHECK_OK(write_str(ctx->writer, "extern "));
  }

  if (is_volatile) {
    CHECK_OK(write_str(ctx->writer, "volatile "));
  }
  CHECK_OK(codegen_type(type, ctx->writer));
  CHECK_OK(write_str(ctx->writer, " "));
  CHECK_OK(write_decl_name(ctx->writer, decl));
  CHECK_OK(write_array_suffix(ctx->writer, type));

finish_decl:

  // Emit initializer only if not extern and if present and not undefined
  if (!is_extern && decl->data.decl.init) {
    // Check if init is undefined literal
    bool is_undefined = (decl->data.decl.init->kind == TICK_AST_LITERAL &&
                        decl->data.decl.init->data.literal.kind == TICK_LIT_UNDEFINED);

    if (!is_undefined) {
      CHECK_OK(write_str(ctx->writer, " = "));
      CHECK_OK(codegen_expr(decl->data.decl.init, ctx));
    }
  }

  CHECK_OK(write_str(ctx->writer, ";\n"));

  return TICK_OK;
}

// ============================================================================
// Main Entry Points
// ============================================================================

static tick_err_t codegen_header_prologue(tick_writer_t* w) {
  CHECK_OK(write_str(w, "// Generated by tick compiler\n"));
  CHECK_OK(write_str(w, "#pragma once\n\n"));

  // Write embedded runtime header as a string
  tick_buf_t runtime_buf = {
    .buf = (u8*)tick_runtime_header,
    .sz = tick_runtime_header_len
  };
  CHECK_OK(w->write(w->ctx, &runtime_buf));
  CHECK_OK(write_str(w, "\n"));

  return TICK_OK;
}

static tick_err_t codegen_c_prologue(tick_writer_t* w, const char* header_name) {
  CHECK_OK(write_str(w, "// Generated by tick compiler\n"));
  CHECK_OK(write_fmt(w, "#include \"%s\"\n\n", header_name));
  return TICK_OK;
}

tick_err_t tick_codegen(tick_ast_t* ast, const char* source_filename, const char* header_filename, tick_writer_t writer_h, tick_writer_t writer_c) {
  codegen_ctx_t ctx_h = {&writer_h, source_filename, 0};
  codegen_ctx_t ctx_c = {&writer_c, source_filename, 0};

  CHECK_OK(codegen_header_prologue(&writer_h));

  // Extract basename from header_filename for #include directive
  // Find the last '/' or '\\' to get just the filename
  const char* basename = header_filename;
  for (const char* p = header_filename; *p; p++) {
    if (*p == '/' || *p == '\\') {
      basename = p + 1;
    }
  }

  CHECK_OK(codegen_c_prologue(&writer_c, basename));

  tick_ast_node_t* module = ast->root;

  // Single pass: Emit declarations in order
  // Lowering is responsible for inserting forward declarations and ordering
  for (tick_ast_node_t* decl = module->data.module.decls; decl; decl = decl->next) {
    if (decl->kind != TICK_AST_DECL) {
      continue;
    }

    bool is_pub = decl->data.decl.quals.is_pub;
    bool is_extern = decl->data.decl.quals.is_extern;
    bool is_forward = decl->data.decl.quals.is_forward_decl;

    // Skip declarations without init unless they're extern
    if (!decl->data.decl.init && !is_extern) {
      continue;
    }

    tick_ast_node_kind_t init_kind = decl->data.decl.init ? decl->data.decl.init->kind : TICK_AST_DECL;

    switch (init_kind) {
      case TICK_AST_STRUCT_DECL:
        // Emit to header if pub
        if (is_pub) {
          CHECK_OK(codegen_struct_decl(decl, &writer_h, !is_forward));
        } else {
          // Only emit to C file if not pub
          CHECK_OK(codegen_struct_decl(decl, &writer_c, !is_forward));
        }
        break;

      case TICK_AST_ENUM_DECL:
        // Enums don't have forward declarations
        CHECK(!is_forward, "enum cannot have forward declaration");
        // Emit to header if pub
        if (is_pub) {
          CHECK_OK(codegen_enum_decl(decl, &writer_h));
        } else {
          // Only emit to C file if not pub
          CHECK_OK(codegen_enum_decl(decl, &writer_c));
        }
        break;

      case TICK_AST_UNION_DECL:
        // Emit to header if pub
        if (is_pub) {
          CHECK_OK(codegen_union_decl(decl, &writer_h, !is_forward));
        } else {
          // Only emit to C file if not pub
          CHECK_OK(codegen_union_decl(decl, &writer_c, !is_forward));
        }
        break;

      case TICK_AST_FUNCTION:
        // Functions don't have forward declarations (yet)
        CHECK(!is_forward, "function cannot have forward declaration");
        // Emit to header if pub
        if (is_pub) {
          CHECK_OK(codegen_function_decl_h(decl, &ctx_h));
        }
        // Always emit to C file
        CHECK_OK(codegen_function_decl_c(decl, &ctx_c));
        break;

      default:
        // Global variable (let/var at module level)
        CHECK(!is_forward, "global variable cannot have forward declaration");
        // Emit extern in header if pub
        if (is_pub) {
          CHECK_OK(codegen_global_var_h(decl, &ctx_h));
        }
        // Always emit definition in C file
        CHECK_OK(codegen_global_var_c(decl, &ctx_c));
        break;
    }
  }

  return TICK_OK;
}
