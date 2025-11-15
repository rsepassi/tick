#include <string.h>

#include "tick.h"

// ============================================================================
// Analysis Debug Logging
// ============================================================================

// ============================================================================
// Error Reporting Utilities
// ============================================================================

// Analysis error macro - reports error with location context
#define ANALYSIS_ERROR(ctx, loc, fmt, ...)                                   \
  do {                                                                       \
    char __msg_buf[512];                                                     \
    snprintf(__msg_buf, sizeof(__msg_buf), fmt, ##__VA_ARGS__);              \
    tick_format_error_with_context((ctx)->errbuf, (ctx)->source, (loc).line, \
                                   (loc).col, __msg_buf, 0);                 \
  } while (0)

// Analysis error with span (underlines multiple characters)
#define ANALYSIS_ERROR_SPAN(ctx, loc, span_len, fmt, ...)                    \
  do {                                                                       \
    char __msg_buf[512];                                                     \
    snprintf(__msg_buf, sizeof(__msg_buf), fmt, ##__VA_ARGS__);              \
    tick_format_error_with_context((ctx)->errbuf, (ctx)->source, (loc).line, \
                                   (loc).col, __msg_buf, (span_len));        \
  } while (0)

#ifdef TICK_DEBUG_ANALYZE
// Indented logging helpers - these assume 'ctx' is a pointer in scope
#define ALOG(fmt, ...) DLOG("%*s" fmt, (ctx->log_depth * 2), "", ##__VA_ARGS__)
#define ALOG_ENTER(fmt, ...)                                    \
  do {                                                          \
    DLOG("%*s> " fmt, (ctx->log_depth * 2), "", ##__VA_ARGS__); \
    ctx->log_depth++;                                           \
  } while (0)
#define ALOG_EXIT(fmt, ...)                                     \
  do {                                                          \
    ctx->log_depth--;                                           \
    DLOG("%*s< " fmt, (ctx->log_depth * 2), "", ##__VA_ARGS__); \
  } while (0)
#define ALOG_SECTION(fmt, ...) \
  DLOG("\n[DEBUG]%*s=== " fmt " ===", (ctx->log_depth * 2), "", ##__VA_ARGS__)

#endif  // TICK_DEBUG_ANALYZE

// ============================================================================
// Type to String Helpers (available in all builds for error messages)
// ============================================================================

static const char* tick_builtin_type_str(tick_builtin_type_t type) {
  switch (type) {
    case TICK_TYPE_UNKNOWN:
      return "UNKNOWN";
    case TICK_TYPE_I8:
      return "i8";
    case TICK_TYPE_I16:
      return "i16";
    case TICK_TYPE_I32:
      return "i32";
    case TICK_TYPE_I64:
      return "i64";
    case TICK_TYPE_ISZ:
      return "isz";
    case TICK_TYPE_U8:
      return "u8";
    case TICK_TYPE_U16:
      return "u16";
    case TICK_TYPE_U32:
      return "u32";
    case TICK_TYPE_U64:
      return "u64";
    case TICK_TYPE_USZ:
      return "usz";
    case TICK_TYPE_BOOL:
      return "bool";
    case TICK_TYPE_VOID:
      return "void";
    case TICK_TYPE_USER_DEFINED:
      return "USER_DEFINED";
    default:
      return "<?>";
  }
}

// Convert type node to string for logging and error messages
static const char* tick_type_str(tick_ast_node_t* type) {
  if (!type) return "null";
  switch (type->kind) {
    case TICK_AST_TYPE_NAMED:
      return tick_builtin_type_str(type->type_named.builtin_type);
    case TICK_AST_TYPE_POINTER:
      return "pointer";
    case TICK_AST_TYPE_ARRAY:
      return "array";
    case TICK_AST_TYPE_FUNCTION:
      return "function";
    case TICK_AST_TYPE_OPTIONAL:
      return "optional";
    case TICK_AST_TYPE_ERROR_UNION:
      return "error_union";
    default:
      return "?";
  }
}

// Helper function to map binary operation + type to builtin enum
// Returns the builtin enum, or 0 if no runtime function needed
static tick_builtin_t map_binop_to_builtin(tick_binop_t op,
                                           tick_builtin_type_t type) {
  // Check if type is signed for wrapping operations
  bool is_signed = (type >= TICK_TYPE_I8 && type <= TICK_TYPE_ISZ);

  switch (op) {
    // Saturating operations
    case BINOP_SAT_ADD:
      return TICK_BUILTIN_SAT_ADD;
    case BINOP_SAT_SUB:
      return TICK_BUILTIN_SAT_SUB;
    case BINOP_SAT_MUL:
      return TICK_BUILTIN_SAT_MUL;
    case BINOP_SAT_DIV:
      return TICK_BUILTIN_SAT_DIV;

    // Wrapping operations - only for signed types
    case BINOP_WRAP_ADD:
      return is_signed ? TICK_BUILTIN_WRAP_ADD : (tick_builtin_t)0;
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
    case BINOP_DIV:
      return TICK_BUILTIN_CHECKED_DIV;
    case BINOP_MOD:
      return TICK_BUILTIN_CHECKED_MOD;

    // Shifts - always checked
    case BINOP_LSHIFT:
      return TICK_BUILTIN_CHECKED_SHL;
    case BINOP_RSHIFT:
      return TICK_BUILTIN_CHECKED_SHR;

    default:
      return (
          tick_builtin_t)0;  // Not an arithmetic operation - no runtime func
  }
}

// Check if identifier needs __u_ prefix
// Returns false for: AT builtins, extern decls, pub decls, temporaries
// Returns true for: regular user variables
static bool identifier_needs_user_prefix(tick_ast_node_t* identifier) {
  CHECK(identifier && identifier->kind == TICK_AST_IDENTIFIER_EXPR,
        "must be identifier");

  // AT builtins don't need prefix
  if (identifier->identifier_expr.at_builtin != TICK_AT_BUILTIN_UNKNOWN) {
    return false;
  }

  // If symbol resolved, check the declaration
  tick_symbol_t* sym = identifier->identifier_expr.symbol;
  if (sym && sym->decl) {
    tick_ast_node_t* decl = sym->decl;

    // Temporaries don't need prefix (have __tmp{N} instead)
    if (tick_node_is_temporary(decl)) {
      return false;
    }

    // Extern and pub declarations don't need prefix (link with C code)
    if (decl->kind == TICK_AST_DECL) {
      if (decl->decl.quals.is_extern || decl->decl.quals.is_pub) {
        return false;
      }
    }
  }

  // Default: needs __u_ prefix
  return true;
}

// ============================================================================
// Debug-only String Helpers
// ============================================================================

#ifdef TICK_DEBUG_ANALYZE

static const char* tick_binop_str(tick_binop_t op) {
  switch (op) {
    case BINOP_ADD:
      return "+";
    case BINOP_SUB:
      return "-";
    case BINOP_MUL:
      return "*";
    case BINOP_DIV:
      return "/";
    case BINOP_MOD:
      return "%";
    case BINOP_EQ:
      return "==";
    case BINOP_NE:
      return "!=";
    case BINOP_LT:
      return "<";
    case BINOP_GT:
      return ">";
    case BINOP_LE:
      return "<=";
    case BINOP_GE:
      return ">=";
    case BINOP_AND:
      return "&";
    case BINOP_OR:
      return "|";
    case BINOP_XOR:
      return "^";
    case BINOP_LSHIFT:
      return "<<";
    case BINOP_RSHIFT:
      return ">>";
    case BINOP_LOGICAL_AND:
      return "&&";
    case BINOP_LOGICAL_OR:
      return "||";
    case BINOP_SAT_ADD:
      return "+|";
    case BINOP_SAT_SUB:
      return "-|";
    case BINOP_SAT_MUL:
      return "*|";
    case BINOP_SAT_DIV:
      return "/|";
    case BINOP_WRAP_ADD:
      return "+%";
    case BINOP_WRAP_SUB:
      return "-%";
    case BINOP_WRAP_MUL:
      return "*%";
    case BINOP_WRAP_DIV:
      return "/%";
    case BINOP_ORELSE:
      return "orelse";
    default:
      return "<?>";
  }
}

static const char* tick_unop_str(tick_unop_t op) {
  switch (op) {
    case UNOP_NEG:
      return "-";
    case UNOP_NOT:
      return "!";
    case UNOP_BIT_NOT:
      return "~";
    case UNOP_ADDR:
      return "&";
    case UNOP_DEREF:
      return "*";
    default:
      return "<?>";
  }
}

static const char* tick_analyze_error_str(tick_analyze_error_t err) {
  switch (err) {
    case TICK_ANALYZE_OK:
      return "OK";
    case TICK_ANALYZE_ERR:
      return "ERR";
    case TICK_ANALYZE_ERR_UNKNOWN_TYPE:
      return "UNKNOWN_TYPE";
    case TICK_ANALYZE_ERR_UNKNOWN_SYMBOL:
      return "UNKNOWN_SYMBOL";
    case TICK_ANALYZE_ERR_DUPLICATE_NAME:
      return "DUPLICATE_NAME";
    case TICK_ANALYZE_ERR_TYPE_MISMATCH:
      return "TYPE_MISMATCH";
    case TICK_ANALYZE_ERR_CIRCULAR_DEPENDENCY:
      return "CIRCULAR_DEPENDENCY";
    default:
      return "<?>";
  }
}

__attribute__((unused)) static const char* tick_analysis_state_str(
    tick_analysis_state_t state) {
  switch (state) {
    case TICK_ANALYSIS_STATE_NOT_STARTED:
      return "NOT_STARTED";
    case TICK_ANALYSIS_STATE_IN_PROGRESS:
      return "IN_PROGRESS";
    case TICK_ANALYSIS_STATE_COMPLETED:
      return "COMPLETED";
    case TICK_ANALYSIS_STATE_FAILED:
      return "FAILED";
    default:
      return "<?>";
  }
}

#else
#define ALOG(fmt, ...) (void)(0)
#define ALOG_ENTER(fmt, ...) (void)(0)
#define ALOG_EXIT(fmt, ...) (void)(0)
#define ALOG_SECTION(fmt, ...) (void)(0)
#endif

// ============================================================================
// Helper Macros
// ============================================================================

// Allocate an AST node with zero-initialization
// Usage: ALLOC_AST_NODE(alloc, my_node);
#define ALLOC_AST_NODE(alloc, node_var)                             \
  do {                                                              \
    tick_allocator_config_t config = {                              \
        .flags = TICK_ALLOCATOR_ZEROMEM,                            \
        .alignment2 = 0,                                            \
    };                                                              \
    tick_buf_t buf = {0};                                           \
    if ((alloc).realloc((alloc).ctx, &buf, sizeof(tick_ast_node_t), \
                        &config) != TICK_OK)                        \
      return NULL;                                                  \
    node_var = (tick_ast_node_t*)buf.buf;                           \
  } while (0)

// Decompose a field to a simple expression if needed
// Usage: DECOMPOSE_FIELD(expr, binary_expr.left);
#define DECOMPOSE_FIELD(expr_ptr, field)                               \
  do {                                                                 \
    if (!is_simple_expr((expr_ptr)->field)) {                          \
      (expr_ptr)->field = decompose_to_simple((expr_ptr)->field, ctx); \
    }                                                                  \
  } while (0)

// Decompose a linked list element to a simple expression if needed
// Handles list pointer replacement while preserving next pointers
// Usage: DECOMPOSE_LIST_ELEM(arg_ptr);
#define DECOMPOSE_LIST_ELEM(elem_ptr_var)                           \
  do {                                                              \
    tick_ast_node_t* elem = *(elem_ptr_var);                        \
    if (!is_simple_expr(elem)) {                                    \
      tick_ast_node_t* simplified = decompose_to_simple(elem, ctx); \
      if (simplified && simplified != elem) {                       \
        simplified->next = elem->next;                              \
        *(elem_ptr_var) = simplified;                               \
        elem = simplified;                                          \
      }                                                             \
    }                                                               \
  } while (0)

// Return cached resolved type if available
// Usage: RETURN_IF_RESOLVED(expr, binary_expr);
#define RETURN_IF_RESOLVED(expr_node, field)   \
  do {                                         \
    if ((expr_node)->field.resolved_type) {    \
      return (expr_node)->field.resolved_type; \
    }                                          \
  } while (0)

// ============================================================================
// Analysis pass
// ============================================================================
//
// This module implements the analysis pass for the Tick AST.
//
// IMPORTANT: See tick.h "AST Pipeline and Lowering Target" for the complete
// specification of what these passes must establish for codegen.
//
// Summary:
// - Analysis: Resolve types, populate resolved_type fields, validate
// - Lowering: Transform high-level types to basic types
//
// See tick.h for full details on the pipeline contract.
//
// ============================================================================

// ============================================================================
// Canonical AST Helper Functions
// ============================================================================
// These functions provide explicit, robust APIs for querying and manipulating
// AST nodes, eliminating brittle assumptions like "name.sz == 0" checks.

// ============================================================================
// Type Analysis Helper Functions
// ============================================================================
// Note: The canonical query API functions (tick_node_is_*, tick_type_is_*,
// etc.) are now defined in tick.c and declared in tick.h for use across all
// files.

// Transform static string declarations from *u8 to u8[N] array type
// This normalizes the representation so codegen doesn't need special handling
static tick_ast_node_t* transform_static_string_decl(tick_ast_node_t* decl,
                                                     tick_alloc_t alloc) {
  CHECK(decl && decl->kind == TICK_AST_DECL, "must be decl");

  // Check if this is a static string literal
  if (!decl->decl.quals.is_static || !decl->decl.init ||
      decl->decl.init->kind != TICK_AST_LITERAL ||
      decl->decl.init->literal.kind != TICK_LIT_STRING) {
    return decl;
  }

  // Get string literal data
  tick_buf_t str = decl->decl.init->literal.data.string_value;
  usz array_size = str.sz + 1;  // +1 for null terminator
  tick_ast_loc_t loc = decl->decl.init->loc;

  // Create u8 element type
  tick_ast_node_t* u8_type;
  ALLOC_AST_NODE(alloc, u8_type);
  u8_type->kind = TICK_AST_TYPE_NAMED;
  u8_type->type_named.name = (tick_buf_t){(u8*)"u8", 2};
  u8_type->type_named.builtin_type = TICK_TYPE_U8;
  u8_type->type_named.type_entry = NULL;
  u8_type->loc = decl->decl.type->loc;

  // Create array size literal
  tick_ast_node_t* size_lit;
  ALLOC_AST_NODE(alloc, size_lit);
  size_lit->kind = TICK_AST_LITERAL;
  size_lit->literal.kind = TICK_LIT_UINT;
  size_lit->literal.data.uint_value = array_size;
  size_lit->loc = decl->decl.type->loc;

  // Create array type: u8[N]
  tick_ast_node_t* array_type;
  ALLOC_AST_NODE(alloc, array_type);
  array_type->kind = TICK_AST_TYPE_ARRAY;
  array_type->type_array.element_type = u8_type;
  array_type->type_array.size = size_lit;
  array_type->loc = decl->decl.type->loc;

  // Create array initializer with byte literals
  tick_ast_node_t* array_init;
  ALLOC_AST_NODE(alloc, array_init);
  array_init->kind = TICK_AST_ARRAY_INIT_EXPR;
  array_init->array_init_expr.elements = NULL;
  array_init->loc = loc;

  // Build linked list of byte literal elements (in reverse, then reverse again)
  tick_ast_node_t* first_elem = NULL;
  tick_ast_node_t* last_elem = NULL;

  // Add string bytes
  for (usz i = 0; i < str.sz; i++) {
    tick_ast_node_t* byte_lit;
    ALLOC_AST_NODE(alloc, byte_lit);
    byte_lit->kind = TICK_AST_LITERAL;
    byte_lit->literal.kind = TICK_LIT_UINT;
    byte_lit->literal.data.uint_value = str.buf[i];
    byte_lit->loc = loc;
    byte_lit->next = NULL;
    byte_lit->prev = last_elem;

    if (!first_elem) {
      first_elem = byte_lit;
    } else {
      last_elem->next = byte_lit;
    }
    last_elem = byte_lit;
  }

  // Add null terminator
  tick_ast_node_t* null_byte;
  ALLOC_AST_NODE(alloc, null_byte);
  null_byte->kind = TICK_AST_LITERAL;
  null_byte->literal.kind = TICK_LIT_UINT;
  null_byte->literal.data.uint_value = 0;
  null_byte->loc = loc;
  null_byte->next = NULL;
  null_byte->prev = last_elem;

  if (!first_elem) {
    first_elem = null_byte;
  } else {
    last_elem->next = null_byte;
  }

  array_init->array_init_expr.elements = first_elem;

  // Replace type and initializer
  decl->decl.type = array_type;
  decl->decl.init = array_init;

  return decl;
}

// Remove undefined literal initializers
// This normalizes undefined to init = NULL so codegen doesn't need special
// handling
static void remove_undefined_init(tick_ast_node_t* decl) {
  CHECK(decl && decl->kind == TICK_AST_DECL, "must be decl");

  if (decl->decl.init && decl->decl.init->kind == TICK_AST_LITERAL &&
      decl->decl.init->literal.kind == TICK_LIT_UNDEFINED) {
    decl->decl.init = NULL;
  }
}

// Helper to allocate a type node
static tick_ast_node_t* alloc_type_node(tick_alloc_t alloc,
                                        tick_builtin_type_t builtin_type) {
  tick_ast_node_t* node;
  ALLOC_AST_NODE(alloc, node);
  node->kind = TICK_AST_TYPE_NAMED;
  node->type_named.name =
      (tick_buf_t){.buf = NULL, .sz = 0};  // Empty name for synthetic nodes
  node->type_named.builtin_type = builtin_type;
  node->type_named.type_entry = NULL;
  node->node_flags = TICK_NODE_FLAG_SYNTHETIC;  // Compiler-generated
  node->loc.line = 0;                           // Synthesized node
  node->loc.col = 0;
  return node;
}

// Helper to allocate a literal node
static tick_ast_node_t* alloc_literal_node(tick_alloc_t alloc, int64_t value) {
  tick_ast_node_t* node;
  ALLOC_AST_NODE(alloc, node);
  node->kind = TICK_AST_LITERAL;
  node->literal.kind = TICK_LIT_INT;
  node->literal.data.int_value = value;
  node->node_flags = TICK_NODE_FLAG_SYNTHETIC;  // Compiler-generated
  node->loc.line = 0;
  node->loc.col = 0;
  return node;
}

// Helper to allocate a function type node
// Reuses existing param and return_type nodes (no data duplication)
static tick_ast_node_t* alloc_function_type_node(tick_alloc_t alloc,
                                                 tick_ast_node_t* params,
                                                 tick_ast_node_t* return_type) {
  tick_ast_node_t* node;
  ALLOC_AST_NODE(alloc, node);
  node->kind = TICK_AST_TYPE_FUNCTION;
  node->type_function.params = params;
  node->type_function.return_type = return_type;
  node->node_flags = TICK_NODE_FLAG_SYNTHETIC;  // Compiler-generated
  node->loc.line = 0;                           // Synthesized node
  node->loc.col = 0;
  return node;
}

// Helper to allocate a user-defined type node
// Creates a TYPE_NAMED node that will be resolved via type table lookup
static tick_ast_node_t* alloc_user_type_node(tick_alloc_t alloc,
                                             tick_buf_t type_name) {
  tick_ast_node_t* node;
  ALLOC_AST_NODE(alloc, node);
  node->kind = TICK_AST_TYPE_NAMED;
  node->type_named.name = type_name;
  node->type_named.builtin_type = TICK_TYPE_UNKNOWN;  // Will be resolved
  node->type_named.type_entry = NULL;                 // Will be resolved
  node->node_flags = TICK_NODE_FLAG_SYNTHETIC;        // Compiler-generated
  node->loc.line = 0;                                 // Synthesized node
  node->loc.col = 0;
  return node;
}

// ============================================================================
// Constant Expression Evaluation
// ============================================================================

// Evaluate a constant expression to an integer value
// Returns TICK_OK on success, TICK_ERR if the expression is not constant
static tick_err_t eval_const_expr(tick_ast_node_t* expr, int64_t* out_value) {
  if (!expr || !out_value) return TICK_ERR;

  switch (expr->kind) {
    case TICK_AST_LITERAL: {
      if (expr->literal.kind == TICK_LIT_INT) {
        *out_value = expr->literal.data.int_value;
        return TICK_OK;
      } else if (expr->literal.kind == TICK_LIT_UINT) {
        *out_value = (int64_t)expr->literal.data.uint_value;
        return TICK_OK;
      }
      return TICK_ERR;
    }

    case TICK_AST_BINARY_EXPR: {
      int64_t left, right;
      if (eval_const_expr(expr->binary_expr.left, &left) != TICK_OK)
        return TICK_ERR;
      if (eval_const_expr(expr->binary_expr.right, &right) != TICK_OK)
        return TICK_ERR;

      switch (expr->binary_expr.op) {
        case BINOP_ADD:
        case BINOP_SAT_ADD:
        case BINOP_WRAP_ADD:
          *out_value = left + right;
          return TICK_OK;

        case BINOP_SUB:
        case BINOP_SAT_SUB:
        case BINOP_WRAP_SUB:
          *out_value = left - right;
          return TICK_OK;

        case BINOP_MUL:
        case BINOP_SAT_MUL:
        case BINOP_WRAP_MUL:
          *out_value = left * right;
          return TICK_OK;

        case BINOP_DIV:
        case BINOP_SAT_DIV:
        case BINOP_WRAP_DIV:
          if (right == 0) return TICK_ERR;
          *out_value = left / right;
          return TICK_OK;

        case BINOP_MOD:
          if (right == 0) return TICK_ERR;
          *out_value = left % right;
          return TICK_OK;

        case BINOP_LSHIFT:
          *out_value = left << right;
          return TICK_OK;

        case BINOP_RSHIFT:
          *out_value = left >> right;
          return TICK_OK;

        case BINOP_AND:
          *out_value = left & right;
          return TICK_OK;

        case BINOP_OR:
          *out_value = left | right;
          return TICK_OK;

        case BINOP_XOR:
          *out_value = left ^ right;
          return TICK_OK;

        default:
          return TICK_ERR;
      }
    }

    case TICK_AST_UNARY_EXPR: {
      int64_t operand;
      if (eval_const_expr(expr->unary_expr.operand, &operand) != TICK_OK)
        return TICK_ERR;

      switch (expr->unary_expr.op) {
        case UNOP_NEG:
          *out_value = -operand;
          return TICK_OK;

        case UNOP_BIT_NOT:
          *out_value = ~operand;
          return TICK_OK;

        default:
          return TICK_ERR;
      }
    }

    default:
      return TICK_ERR;
  }
}

// Reduce a constant expression to a literal node
// Replaces *expr with a new LITERAL node containing the evaluated value
static tick_err_t reduce_to_literal(tick_ast_node_t** expr,
                                    tick_alloc_t alloc) {
  if (!expr || !*expr) return TICK_ERR;

  int64_t value;
  if (eval_const_expr(*expr, &value) != TICK_OK) return TICK_ERR;

  tick_ast_node_t* literal = alloc_literal_node(alloc, value);
  if (!literal) return TICK_ERR;

  // Preserve location information from original expression
  literal->loc = (*expr)->loc;

  *expr = literal;
  return TICK_OK;
}

// ============================================================================
// Expression Decomposition Helpers
// ============================================================================

// Check if an expression is "simple" (doesn't need decomposition)
// Simple expressions are:
// - LITERAL: integer/bool literals (NOT strings - they need static const
// variables)
// - IDENTIFIER_EXPR: variable references
// - STRUCT_INIT_EXPR: struct initializers (valid in C compound literals)
// - ARRAY_INIT_EXPR: array initializers (valid in C array initialization)
static bool is_simple_expr(tick_ast_node_t* expr) {
  if (!expr) return true;  // NULL is considered simple

  if (expr->kind == TICK_AST_LITERAL) {
    // String literals are NOT simple - they should be extracted to static const
    // variables
    return expr->literal.kind != TICK_LIT_STRING;
  }

  if (expr->kind == TICK_AST_IDENTIFIER_EXPR) {
    return true;
  }

  // Struct and array initializers are simple (valid in C)
  if (expr->kind == TICK_AST_STRUCT_INIT_EXPR ||
      expr->kind == TICK_AST_ARRAY_INIT_EXPR) {
    return true;
  }

  // Dereference of simple expression is simple - keeps lvalues intact
  // e.g., (*ptr).field or (*ptr)[index] should not decompose the *ptr part
  // Address-of simple expression is also simple - needed for lvalue
  // decomposition e.g., &x.y should not decompose when used as temp init
  if (expr->kind == TICK_AST_UNARY_EXPR &&
      (expr->unary_expr.op == UNOP_DEREF || expr->unary_expr.op == UNOP_ADDR) &&
      is_simple_expr(expr->unary_expr.operand)) {
    return true;
  }

  // Field access of simple expression is simple - keeps lvalues intact
  // e.g., container.field.subfield should not decompose intermediate accesses
  if (expr->kind == TICK_AST_FIELD_ACCESS_EXPR &&
      is_simple_expr(expr->field_access_expr.object)) {
    return true;
  }

  // Index of simple expressions is simple - keeps lvalues intact
  // e.g., arr[0] or arr[i] where arr is simple
  if (expr->kind == TICK_AST_INDEX_EXPR &&
      is_simple_expr(expr->index_expr.array) &&
      is_simple_expr(expr->index_expr.index)) {
    return true;
  }

  return false;
}

// Check if an entire initializer expression tree is simple
// Returns true if all expressions are LITERAL or IDENTIFIER_EXPR only
// Used to validate module-level initializers (must be simple)
// Note: Struct/array initializers with only literal/identifier values are
// allowed
static bool is_initializer_simple(tick_ast_node_t* expr) {
  if (!expr) return true;

  switch (expr->kind) {
    case TICK_AST_LITERAL:
    case TICK_AST_IDENTIFIER_EXPR:
      return true;

    case TICK_AST_STRUCT_INIT_EXPR:
      // Struct initializers are allowed if all field values are simple
      // (including nested struct/array initializers)
      for (tick_ast_node_t* field = expr->struct_init_expr.fields; field;
           field = field->next) {
        if (field->kind == TICK_AST_STRUCT_INIT_FIELD) {
          tick_ast_node_t* value = field->struct_init_field.value;
          if (!value) continue;
          // Recursively check if the value is simple
          if (!is_initializer_simple(value)) {
            return false;
          }
        }
      }
      return true;

    case TICK_AST_ARRAY_INIT_EXPR:
      // Array initializers are allowed if all elements are simple
      // (including nested struct/array initializers)
      for (tick_ast_node_t* elem = expr->array_init_expr.elements; elem;
           elem = elem->next) {
        // Recursively check if the element is simple
        if (!is_initializer_simple(elem)) {
          return false;
        }
      }
      return true;

    default:
      // Any other expression type is complex
      return false;
  }
}

// Create a temporary declaration node
// tmpid: temporary ID (from scope->next_tmpid++)
// type: type node for the temporary
// init: initializer expression
static tick_ast_node_t* create_temp_decl(tick_alloc_t alloc, u32 tmpid,
                                         tick_ast_node_t* type,
                                         tick_ast_node_t* init) {
  tick_ast_node_t* decl;
  ALLOC_AST_NODE(alloc, decl);
  decl->kind = TICK_AST_DECL;
  decl->decl.name = (tick_buf_t){0};  // Empty name for temporaries
  decl->decl.type = type;
  decl->decl.init = init;
  decl->decl.tmpid = tmpid;
  decl->decl.quals = (tick_qualifier_flags_t){0};  // Default qualifiers

  // String literals should be static const
  if (init && init->kind == TICK_AST_LITERAL &&
      init->literal.kind == TICK_LIT_STRING) {
    decl->decl.quals.is_static = true;
  }

  // Transform AST to normalize for codegen
  transform_static_string_decl(decl, alloc);
  remove_undefined_init(decl);

  decl->decl.analysis_state =
      TICK_ANALYSIS_STATE_COMPLETED;  // Already analyzed
  decl->node_flags = TICK_NODE_FLAG_SYNTHETIC | TICK_NODE_FLAG_TEMPORARY |
                     TICK_NODE_FLAG_ANALYZED;
  decl->loc = init ? init->loc : (tick_ast_loc_t){0};
  decl->next = NULL;
  decl->prev = NULL;
  return decl;
}

// Create an identifier expression that references a temporary
// The trick here is that we create an identifier with tmpid stored in the decl,
// but the identifier itself needs a non-empty name for codegen to work
// properly. Since the symbol points to a decl with tmpid > 0, codegen should
// emit __tmpN. However, we also set the identifier's name to empty since lookup
// isn't needed. The real magic is that we return a direct reference to the
// temp_decl itself, wrapped as an identifier, so codegen can process it via the
// symbol->decl path.
static tick_ast_node_t* create_temp_identifier(tick_alloc_t alloc,
                                               tick_ast_node_t* temp_decl) {
  // Allocate identifier node
  tick_ast_node_t* ident;
  ALLOC_AST_NODE(alloc, ident);
  ident->kind = TICK_AST_IDENTIFIER_EXPR;
  ident->identifier_expr.at_builtin = TICK_AT_BUILTIN_UNKNOWN;
  ident->node_flags = TICK_NODE_FLAG_SYNTHETIC;  // Compiler-generated reference
  ident->loc = temp_decl->loc;

  // Create name buffer to hold empty name (but non-NULL pointer)
  // This prevents issues with string operations in codegen
  ident->identifier_expr.name = (tick_buf_t){.buf = (u8*)"", .sz = 0};

  // Create and set up symbol pointing to the temporary decl
  tick_allocator_config_t sym_config = {.flags = TICK_ALLOCATOR_ZEROMEM,
                                        .alignment2 = 0};
  tick_buf_t sym_buf = {0};
  if (alloc.realloc(alloc.ctx, &sym_buf, sizeof(tick_symbol_t), &sym_config) !=
      TICK_OK)
    return NULL;

  tick_symbol_t* sym = (tick_symbol_t*)sym_buf.buf;
  sym->name = (tick_buf_t){.buf = (u8*)"", .sz = 0};
  sym->decl = temp_decl;
  sym->type = temp_decl->decl.type;

  ident->identifier_expr.symbol = sym;
  return ident;
}

// Insert a temporary immediately before the current statement
// This preserves evaluation order: temporaries execute right before they're
// used
static void insert_temp_in_block(tick_analyze_ctx_t* ctx,
                                 tick_ast_node_t* temp_decl) {
  CHECK(ctx->current_stmt, "no current stmt");

  // Insert immediately before current_stmt
  temp_decl->next = ctx->current_stmt;
  temp_decl->prev = ctx->current_stmt->prev;

  if (ctx->current_stmt->prev) {
    // Current statement has a predecessor, link it to temp
    ctx->current_stmt->prev->next = temp_decl;
  } else {
    // Current statement is first in block, update block head
    ctx->current_block->block_stmt.stmts = temp_decl;
  }
  ctx->current_stmt->prev = temp_decl;

  ALOG("  inserted __tmp%u before current statement", temp_decl->decl.tmpid);
}

// Forward declarations (needed for decompose_to_simple and mutual recursion)
static tick_ast_node_t* analyze_expr(tick_ast_node_t* expr,
                                     tick_analyze_ctx_t* ctx);
static tick_analyze_error_t analyze_type(tick_ast_node_t* type_node,
                                         tick_analyze_ctx_t* ctx);
static tick_analyze_error_t analyze_stmt(tick_ast_node_t* stmt,
                                         tick_analyze_ctx_t* ctx);

// Expression analysis helpers (dispatch targets)
// NOTE: Only helpers that have been extracted are declared here
static tick_ast_node_t* analyze_literal_expr(tick_ast_node_t* expr,
                                             tick_analyze_ctx_t* ctx);
static tick_ast_node_t* analyze_binary_expr(tick_ast_node_t* expr,
                                            tick_analyze_ctx_t* ctx);
static tick_ast_node_t* analyze_identifier_expr(tick_ast_node_t* expr,
                                                tick_analyze_ctx_t* ctx);
static tick_ast_node_t* analyze_unary_expr(tick_ast_node_t* expr,
                                           tick_analyze_ctx_t* ctx);
static tick_ast_node_t* analyze_cast_expr(tick_ast_node_t* expr,
                                          tick_analyze_ctx_t* ctx);
static tick_ast_node_t* analyze_struct_init_expr(tick_ast_node_t* expr,
                                                 tick_analyze_ctx_t* ctx);
static tick_ast_node_t* analyze_array_init_expr(tick_ast_node_t* expr,
                                                tick_analyze_ctx_t* ctx);
static tick_ast_node_t* analyze_call_expr(tick_ast_node_t* expr,
                                          tick_analyze_ctx_t* ctx);
static tick_ast_node_t* analyze_index_expr(tick_ast_node_t* expr,
                                           tick_analyze_ctx_t* ctx);
static tick_ast_node_t* analyze_unwrap_panic_expr(tick_ast_node_t* expr,
                                                  tick_analyze_ctx_t* ctx);
static tick_ast_node_t* analyze_field_access_expr(tick_ast_node_t* expr,
                                                  tick_analyze_ctx_t* ctx);

// ============================================================================
// Complex Helper Functions (require forward declarations)
// ============================================================================

// Check if an analysis error has occurred
// Returns true if the error buffer contains an error message
static inline bool analyze_has_error(tick_analyze_ctx_t* ctx) {
  return ctx->errbuf.buf[0] != '\0';
}

// Check if a function signature (TYPE_FUNCTION) is fully resolved
// Returns true if all parameter types and return type are resolved
// UNUSED: Temporarily disabled to debug dependency issues
/*
static bool is_function_signature_resolved(tick_ast_node_t* fn_type) {
  if (!fn_type || fn_type->kind != TICK_AST_TYPE_FUNCTION) {
    return false;
  }

  // Check return type
  if (fn_type->type_function.return_type &&
      tick_type_is_unresolved(fn_type->type_function.return_type)) {
    return false;
  }

  // Check parameter types
  for (tick_ast_node_t* param = fn_type->type_function.params; param;
       param = param->next) {
    if (param->param.type && tick_type_is_unresolved(param->param.type)) {
      return false;
    }
  }

  return true;
}
*/

// Resolve a named type via type table lookup
// Optionally tracks dependencies for user-defined types
static void resolve_named_type(tick_ast_node_t* type_node,
                               tick_analyze_ctx_t* ctx,
                               bool track_dependencies) {
  if (type_node->type_named.type_entry) return;

  type_node->type_named.type_entry =
      tick_types_lookup(ctx->types, type_node->type_named.name);

  if (type_node->type_named.type_entry) {
    type_node->type_named.builtin_type =
        type_node->type_named.type_entry->builtin_type;

    if (track_dependencies && tick_type_is_user_defined(type_node) &&
        type_node->type_named.type_entry->parent_decl) {
      tick_ast_node_t* type_decl =
          type_node->type_named.type_entry->parent_decl;
      if (type_decl->decl.analysis_state != TICK_ANALYSIS_STATE_COMPLETED) {
        tick_dependency_list_add(&ctx->pending_deps, type_decl);
        ALOG("! dep: %.*s", (int)type_decl->decl.name.sz,
             type_decl->decl.name.buf);
      }
    }
  } else {
    type_node->type_named.builtin_type = TICK_TYPE_UNKNOWN;
    ALOG("type: %.*s -> NOT FOUND", (int)type_node->type_named.name.sz,
         type_node->type_named.name.buf);
  }
}

// Get type from symbol's declaration, with caching
static tick_ast_node_t* get_symbol_type(tick_symbol_t* sym) {
  if (sym->type) return sym->type;

  if (!sym->decl) return NULL;

  tick_ast_node_t* result_type = NULL;
  switch (sym->decl->kind) {
    case TICK_AST_DECL:
      result_type = sym->decl->decl.type;
      break;
    case TICK_AST_PARAM:
      result_type = sym->decl->param.type;
      break;
    default:
      CHECK(0, "unexpected symbol decl kind: %d", sym->decl->kind);
  }

  if (result_type) {
    sym->type = result_type;
  }
  return result_type;
}

// Analyze field types for struct/union declarations
static tick_analyze_error_t analyze_field_types(tick_ast_node_t* fields,
                                                tick_analyze_ctx_t* ctx) {
  for (tick_ast_node_t* field = fields; field; field = field->next) {
    if (field->kind == TICK_AST_FIELD) {
      tick_analyze_error_t err = analyze_type(field->field.type, ctx);
      if (err != TICK_ANALYZE_OK) {
        return err;
      }
    }
  }
  return TICK_ANALYZE_OK;
}

// Ensure user-defined type (enum/struct/union) is set up properly
// Creates type node pointing to this declaration
static tick_analyze_error_t ensure_user_type_set(tick_ast_node_t* decl,
                                                 tick_analyze_ctx_t* ctx) {
  if (decl->decl.type == NULL) {
    decl->decl.type = alloc_user_type_node(ctx->alloc, decl->decl.name);
    // Don't call analyze_type here - it would add the declaration as a
    // dependency of itself, creating a circular dependency. The type node is
    // already set up correctly and will be resolved when other code references
    // this type.
  }
  return TICK_ANALYZE_OK;
}

// Decompose a complex expression into a temporary variable
// Returns either the original expression (if simple) or an IDENTIFIER_EXPR
// referencing a temporary
static tick_ast_node_t* decompose_to_simple(tick_ast_node_t* expr,
                                            tick_analyze_ctx_t* ctx) {
  // If already simple, return as-is
  if (is_simple_expr(expr)) {
    return expr;
  }

  // Can only decompose if we have a current block (function-level only)
  if (!ctx->current_block) {
    ALOG("  cannot decompose: no current_block (module-level not supported)");
    return expr;
  }

  // Analyze the expression to get its type (recursively decomposes
  // sub-expressions)
  tick_ast_node_t* expr_type = analyze_expr(expr, ctx);
  if (!expr_type) {
    ALOG("  cannot decompose: expression has no type");
    return expr;  // Can't decompose without a type
  }

  // Ensure the type itself is analyzed (resolve any TICK_TYPE_UNKNOWN)
  tick_analyze_error_t type_err = analyze_type(expr_type, ctx);
  if (type_err != TICK_ANALYZE_OK) {
    ALOG("  cannot decompose: type analysis failed");
    return expr;
  }

  // Don't decompose array types - arrays can't be copied in C
  // Keep them inline (e.g., container.ints should stay as field access)
  if (expr_type->kind == TICK_AST_TYPE_ARRAY) {
    ALOG("  cannot decompose: array type (arrays can't be copied in C)");
    return expr;
  }

  // Allocate a new temporary ID from the function-level scope
  // This ensures all temporaries in a function have unique IDs across all
  // blocks
  u32 tmpid = ctx->function_scope->next_tmpid++;

  // Create a temporary declaration: let __tmpN: <type> = <expr>;
  tick_ast_node_t* temp_decl =
      create_temp_decl(ctx->alloc, tmpid, expr_type, expr);
  if (!temp_decl) {
    return expr;  // Allocation failed, return original
  }

  // Insert the temporary before the current statement in the current block
  insert_temp_in_block(ctx, temp_decl);

  // Create an identifier expression that references this temporary
  tick_ast_node_t* temp_ident = create_temp_identifier(ctx->alloc, temp_decl);
  if (!temp_ident) {
    return expr;  // Allocation failed, return original
  }

  ALOG("  decomposed to __tmp%u", tmpid);
  return temp_ident;
}

// ============================================================================
// Object Initialization Decomposition (Phase 1)
// ============================================================================

// Forward declarations for mutual recursion
static void flatten_struct_init(tick_ast_node_t* base_lvalue,
                                tick_ast_node_t* init_expr,
                                tick_analyze_ctx_t* ctx);

// ============================================================================
// Lvalue Decomposition (Phase 2)
// ============================================================================

// Helper: Check if an lvalue is simple (doesn't need pointer decomposition)
static bool is_simple_lvalue(tick_ast_node_t* expr) {
  // Simple lvalues are just identifiers
  return expr && expr->kind == TICK_AST_IDENTIFIER_EXPR;
}

// Helper: Create a unary expression node (for address-of and dereference)
static tick_ast_node_t* create_unary_node(tick_alloc_t alloc, tick_unop_t op,
                                          tick_ast_node_t* operand,
                                          tick_ast_loc_t loc) {
  tick_ast_node_t* node;
  ALLOC_AST_NODE(alloc, node);
  node->kind = TICK_AST_UNARY_EXPR;
  node->unary_expr.op = op;
  node->unary_expr.operand = operand;
  node->unary_expr.resolved_type = NULL;  // Will be filled during analysis
  node->node_flags = TICK_NODE_FLAG_SYNTHETIC;
  node->loc = loc;
  return node;
}

// Helper: Create a pointer type node
static tick_ast_node_t* create_pointer_type_node(
    tick_alloc_t alloc, tick_ast_node_t* pointee_type) {
  tick_ast_node_t* node;
  ALLOC_AST_NODE(alloc, node);
  node->kind = TICK_AST_TYPE_POINTER;
  node->type_pointer.pointee_type = pointee_type;
  node->node_flags = TICK_NODE_FLAG_SYNTHETIC;
  node->loc = (tick_ast_loc_t){0};
  return node;
}

// Forward declarations for helper functions
static tick_ast_node_t* create_field_access_node(tick_alloc_t alloc,
                                                 tick_ast_node_t* object,
                                                 tick_buf_t field_name,
                                                 tick_ast_loc_t loc);

// Helper: Create address-of expression for lvalue
// Handles: &x, &obj.field, &arr[i], &*ptr (cancels to just ptr)
static tick_ast_node_t* create_address_of_lvalue(tick_ast_node_t* lvalue,
                                                 tick_alloc_t alloc) {
  // Special case: &(*ptr) → ptr (address-of and deref cancel out)
  if (lvalue->kind == TICK_AST_UNARY_EXPR &&
      lvalue->unary_expr.op == UNOP_DEREF) {
    return lvalue->unary_expr.operand;
  }

  // General case: create address-of unary expression
  return create_unary_node(alloc, UNOP_ADDR, lvalue, lvalue->loc);
}

// Helper: Create a field access node
static tick_ast_node_t* create_field_access_node(tick_alloc_t alloc,
                                                 tick_ast_node_t* object,
                                                 tick_buf_t field_name,
                                                 tick_ast_loc_t loc) {
  tick_ast_node_t* node;
  ALLOC_AST_NODE(alloc, node);
  node->kind = TICK_AST_FIELD_ACCESS_EXPR;
  node->field_access_expr.object = object;
  node->field_access_expr.field_name = field_name;
  node->field_access_expr.resolved_type = NULL;
  node->field_access_expr.object_is_pointer = false;
  node->node_flags = TICK_NODE_FLAG_SYNTHETIC;
  node->loc = loc;
  return node;
}

// Helper: Create an index expression node
static tick_ast_node_t* create_index_node(tick_alloc_t alloc,
                                          tick_ast_node_t* array,
                                          tick_ast_node_t* index,
                                          tick_ast_loc_t loc) {
  tick_ast_node_t* node;
  ALLOC_AST_NODE(alloc, node);
  node->kind = TICK_AST_INDEX_EXPR;
  node->index_expr.array = array;
  node->index_expr.index = index;
  node->node_flags = TICK_NODE_FLAG_SYNTHETIC;
  node->loc = loc;
  return node;
}

// Helper: Create an assignment statement node
static tick_ast_node_t* create_assign_stmt_node(tick_alloc_t alloc,
                                                tick_ast_node_t* lhs,
                                                tick_ast_node_t* rhs,
                                                tick_ast_loc_t loc) {
  tick_ast_node_t* node;
  ALLOC_AST_NODE(alloc, node);
  node->kind = TICK_AST_ASSIGN_STMT;
  node->assign_stmt.lhs = lhs;
  node->assign_stmt.rhs = rhs;
  node->node_flags = TICK_NODE_FLAG_SYNTHETIC;
  node->loc = loc;
  node->next = NULL;
  node->prev = NULL;
  return node;
}

// Helper: Create an empty block statement node
static tick_ast_node_t* create_empty_block_stmt(tick_alloc_t alloc,
                                                tick_ast_loc_t loc) {
  tick_ast_node_t* node;
  ALLOC_AST_NODE(alloc, node);
  node->kind = TICK_AST_BLOCK_STMT;
  node->block_stmt.stmts = NULL;  // Empty block
  node->node_flags = TICK_NODE_FLAG_SYNTHETIC;
  node->loc = loc;
  node->next = NULL;
  node->prev = NULL;
  return node;
}

// Recursively flatten array initializers into index assignments
static void flatten_array_init(tick_ast_node_t* base_lvalue,
                               tick_ast_node_t* init_expr,
                               tick_analyze_ctx_t* ctx) {
  ALOG("  flattening array init");

  tick_ast_node_t* elem = init_expr->array_init_expr.elements;
  int index = 0;
  while (elem) {
    // Create index expression: base[index]
    tick_ast_node_t* index_lit = alloc_literal_node(ctx->alloc, index);
    tick_ast_node_t* index_lvalue =
        create_index_node(ctx->alloc, base_lvalue, index_lit, elem->loc);

    // Check if element is a nested struct/array initializer
    if (elem->kind == TICK_AST_STRUCT_INIT_EXPR) {
      // Recursive: flatten nested struct
      flatten_struct_init(index_lvalue, elem, ctx);
    } else if (elem->kind == TICK_AST_ARRAY_INIT_EXPR) {
      // Recursive: flatten nested array
      flatten_array_init(index_lvalue, elem, ctx);
    } else {
      // Leaf: create assignment
      tick_ast_node_t* assign =
          create_assign_stmt_node(ctx->alloc, index_lvalue, elem, elem->loc);
      insert_temp_in_block(ctx, assign);
      // Analyze the assignment immediately so lvalue decomposition happens
      // Save and restore current_stmt so temps get inserted in the right place
      tick_ast_node_t* saved_stmt = ctx->current_stmt;
      ctx->current_stmt = assign;
      analyze_stmt(assign, ctx);
      ctx->current_stmt = saved_stmt;
    }

    elem = elem->next;
    index++;
  }
}

// Recursively flatten nested struct initializers into field assignments
static void flatten_struct_init(tick_ast_node_t* base_lvalue,
                                tick_ast_node_t* init_expr,
                                tick_analyze_ctx_t* ctx) {
  ALOG("  flattening struct init");

  tick_ast_node_t* field = init_expr->struct_init_expr.fields;
  while (field) {
    CHECK(field->kind == TICK_AST_STRUCT_INIT_FIELD,
          "expected STRUCT_INIT_FIELD");

    // Create field access: base.field_name
    tick_ast_node_t* field_lvalue = create_field_access_node(
        ctx->alloc, base_lvalue, field->struct_init_field.field_name,
        field->loc);

    tick_ast_node_t* field_value = field->struct_init_field.value;

    // Check if value is a nested struct/array initializer
    if (field_value->kind == TICK_AST_STRUCT_INIT_EXPR) {
      // Recursive: flatten nested struct
      flatten_struct_init(field_lvalue, field_value, ctx);
    } else if (field_value->kind == TICK_AST_ARRAY_INIT_EXPR) {
      // Flatten nested array init
      flatten_array_init(field_lvalue, field_value, ctx);
    } else {
      // Leaf: create assignment
      tick_ast_node_t* assign = create_assign_stmt_node(
          ctx->alloc, field_lvalue, field_value, field->loc);
      insert_temp_in_block(ctx, assign);
      // Analyze the assignment immediately so lvalue decomposition happens
      // Save and restore current_stmt so temps get inserted in the right place
      tick_ast_node_t* saved_stmt = ctx->current_stmt;
      ctx->current_stmt = assign;
      analyze_stmt(assign, ctx);
      ctx->current_stmt = saved_stmt;
    }

    field = field->next;
  }
}

// Decompose struct initializer to declaration + field assignments
static tick_ast_node_t* decompose_struct_init(tick_ast_node_t* init_expr,
                                              tick_analyze_ctx_t* ctx) {
  // Skip if module-level (must stay compile-time constant)
  if (ctx->scope_depth == 0) {
    ALOG("  keeping struct init at module level");
    return init_expr;
  }

  // Can only decompose if we have a current block
  if (!ctx->current_block) {
    ALOG("  cannot decompose: no current_block");
    return init_expr;
  }

  ALOG_ENTER("decomposing struct init");

  // Get the type of the struct being initialized
  tick_ast_node_t* struct_type = init_expr->struct_init_expr.type;
  if (!struct_type) {
    ALOG_EXIT("FAILED: no type");
    return init_expr;
  }

  // Allocate temporary ID
  u32 tmpid = ctx->function_scope->next_tmpid++;

  // Create temp declaration with undefined init
  tick_ast_node_t* temp_decl =
      create_temp_decl(ctx->alloc, tmpid, struct_type, NULL);
  if (!temp_decl) {
    ALOG_EXIT("FAILED: allocation failed");
    return init_expr;
  }

  // Insert the temporary declaration
  insert_temp_in_block(ctx, temp_decl);

  // Create identifier referencing the temp
  tick_ast_node_t* temp_ident = create_temp_identifier(ctx->alloc, temp_decl);
  if (!temp_ident) {
    ALOG_EXIT("FAILED: allocation failed");
    return init_expr;
  }

  // Generate field assignments
  flatten_struct_init(temp_ident, init_expr, ctx);

  ALOG_EXIT("decomposed to __tmp%u", tmpid);
  return temp_ident;
}

// Decompose array initializer to declaration + index assignments
static tick_ast_node_t* decompose_array_init(tick_ast_node_t* init_expr,
                                             tick_analyze_ctx_t* ctx) {
  // Skip if module-level
  if (ctx->scope_depth == 0) {
    ALOG("  keeping array init at module level");
    return init_expr;
  }

  // Can only decompose if we have a current block
  if (!ctx->current_block) {
    ALOG("  cannot decompose: no current_block");
    return init_expr;
  }

  ALOG_ENTER("decomposing array init");

  // Array init needs type from context (parent declaration)
  // For now, we don't have access to the parent type, so we can't decompose
  // This will be handled when we have full context from the declaration
  // For the initial implementation, keep array inits as-is
  ALOG_EXIT("array init decomposition needs parent type context");
  return init_expr;
}

// Multi-step lvalue decomposition: transforms complex lvalues into pointer
// temporaries Example: container.field[index].subfield
//   → let __tmp0 = &container.field;
//     let __tmp1 = &(*__tmp0)[index];
//     let __tmp2 = &(*__tmp1).subfield;
//     returns: *__tmp2
//
// Note: lvalue must have already been analyzed (resolved_type fields populated)
static tick_ast_node_t* decompose_lvalue_chain(tick_ast_node_t* lvalue,
                                               tick_ast_node_t* lvalue_type,
                                               tick_analyze_ctx_t* ctx) {
  if (!lvalue || !ctx) return NULL;

  ALOG_ENTER("decomposing lvalue chain");

  // Base case: simple identifier needs no decomposition
  if (lvalue->kind == TICK_AST_IDENTIFIER_EXPR) {
    ALOG_EXIT("simple identifier, no decomposition");
    return lvalue;
  }

  // Extract the type from the lvalue node if not provided
  // (Each level except the top will extract its own type)
  if (!lvalue_type) {
    switch (lvalue->kind) {
      case TICK_AST_FIELD_ACCESS_EXPR:
        lvalue_type = lvalue->field_access_expr.resolved_type;
        break;
      case TICK_AST_UNARY_EXPR:
        lvalue_type = lvalue->unary_expr.resolved_type;
        break;
      case TICK_AST_INDEX_EXPR:
        // For index expressions, we need to get the element type
        // This should have been set during analysis, but index_expr
        // doesn't have a resolved_type field. We'll handle this
        // by looking at the array type and extracting the element type.
        // For now, we'll skip type checking for index operations
        lvalue_type = NULL;
        break;
      default:
        lvalue_type = NULL;
        break;
    }
  }

  tick_ast_node_t* access_expr = NULL;
  tick_ast_node_t* base_for_access = NULL;

  // Recursively decompose the base expression and build the current access
  switch (lvalue->kind) {
    case TICK_AST_FIELD_ACCESS_EXPR: {
      // Decompose: base.field
      tick_ast_node_t* obj = lvalue->field_access_expr.object;
      tick_ast_node_t* decomposed_obj = decompose_lvalue_chain(obj, NULL, ctx);
      if (!decomposed_obj) return NULL;

      // If decomposed_obj is a dereference (*tmpN), extract tmpN
      // Otherwise use decomposed_obj directly
      if (decomposed_obj->kind == TICK_AST_UNARY_EXPR &&
          decomposed_obj->unary_expr.op == UNOP_DEREF) {
        base_for_access = decomposed_obj->unary_expr.operand;
      } else {
        base_for_access = decomposed_obj;
      }

      // Build: (*base_for_access).field or base_for_access.field
      bool needs_deref = (decomposed_obj->kind == TICK_AST_UNARY_EXPR &&
                          decomposed_obj->unary_expr.op == UNOP_DEREF);

      if (needs_deref) {
        // Create (*tmpN).field
        tick_ast_node_t* deref = create_unary_node(
            ctx->alloc, UNOP_DEREF, base_for_access, lvalue->loc);
        access_expr = create_field_access_node(
            ctx->alloc, deref, lvalue->field_access_expr.field_name,
            lvalue->loc);
      } else {
        // Create base.field or base->field (when base is identifier)
        access_expr = create_field_access_node(
            ctx->alloc, base_for_access, lvalue->field_access_expr.field_name,
            lvalue->loc);
      }
      break;
    }

    case TICK_AST_INDEX_EXPR: {
      // Decompose: base[index]
      tick_ast_node_t* arr = lvalue->index_expr.array;
      tick_ast_node_t* decomposed_arr = decompose_lvalue_chain(arr, NULL, ctx);
      if (!decomposed_arr) return NULL;

      // If decomposed_arr is a dereference (*tmpN), extract tmpN
      if (decomposed_arr->kind == TICK_AST_UNARY_EXPR &&
          decomposed_arr->unary_expr.op == UNOP_DEREF) {
        base_for_access = decomposed_arr->unary_expr.operand;
      } else {
        base_for_access = decomposed_arr;
      }

      // Build: (*base_for_access)[index] or base_for_access[index]
      bool needs_deref = (decomposed_arr->kind == TICK_AST_UNARY_EXPR &&
                          decomposed_arr->unary_expr.op == UNOP_DEREF);

      if (needs_deref) {
        // Create (*tmpN)[index]
        tick_ast_node_t* deref = create_unary_node(
            ctx->alloc, UNOP_DEREF, base_for_access, lvalue->loc);
        access_expr = create_index_node(ctx->alloc, deref,
                                        lvalue->index_expr.index, lvalue->loc);
      } else {
        // Create base[index] (when base is identifier)
        access_expr = create_index_node(ctx->alloc, base_for_access,
                                        lvalue->index_expr.index, lvalue->loc);
      }
      break;
    }

    case TICK_AST_UNARY_EXPR: {
      if (lvalue->unary_expr.op == UNOP_DEREF) {
        // Decompose: *ptr
        tick_ast_node_t* ptr = lvalue->unary_expr.operand;
        tick_ast_node_t* decomposed_ptr =
            decompose_lvalue_chain(ptr, NULL, ctx);
        if (!decomposed_ptr) return NULL;

        // Build: *decomposed_ptr
        // If decomposed_ptr is already (*tmpN), we have *(*tmpN) which is tmpN
        if (decomposed_ptr->kind == TICK_AST_UNARY_EXPR &&
            decomposed_ptr->unary_expr.op == UNOP_DEREF) {
          access_expr = create_unary_node(ctx->alloc, UNOP_DEREF,
                                          decomposed_ptr, lvalue->loc);
        } else {
          access_expr = create_unary_node(ctx->alloc, UNOP_DEREF,
                                          decomposed_ptr, lvalue->loc);
        }
      } else {
        ALOG_EXIT("unsupported unary operator for lvalue");
        return NULL;
      }
      break;
    }

    default:
      ALOG_EXIT("unsupported lvalue kind: %d", lvalue->kind);
      return NULL;
  }

  if (!access_expr) {
    ALOG_EXIT("failed to create access expression");
    return NULL;
  }

  // Create temporary: let __tmpN = &access_expr;
  // 1. Create address-of expression
  tick_ast_node_t* addr_expr =
      create_address_of_lvalue(access_expr, ctx->alloc);

  // 2. Create pointer type for the temporary
  tick_ast_node_t* ptr_type = create_pointer_type_node(ctx->alloc, lvalue_type);

  // 3. Allocate temporary ID from function scope
  u32 tmpid = ctx->function_scope->next_tmpid++;

  // 4. Create temp declaration: let __tmpN: *T = &access_expr;
  tick_ast_node_t* temp_decl =
      create_temp_decl(ctx->alloc, tmpid, ptr_type, addr_expr);
  if (!temp_decl) {
    ALOG_EXIT("failed to create temp decl");
    return NULL;
  }

  // Mark as not analyzed so analyze_stmt will process it
  temp_decl->decl.analysis_state = TICK_ANALYSIS_STATE_NOT_STARTED;

  // 5. Insert the temporary before the current statement
  insert_temp_in_block(ctx, temp_decl);

  // 5b. Analyze the temp decl immediately so field access gets analyzed
  // This sets object_is_pointer correctly for synthetic field access nodes
  tick_ast_node_t* saved_stmt = ctx->current_stmt;
  ctx->current_stmt = temp_decl;
  tick_analyze_error_t err = analyze_stmt(temp_decl, ctx);
  ctx->current_stmt = saved_stmt;
  if (err != TICK_ANALYZE_OK) {
    ALOG_EXIT("failed to analyze temp decl");
    return NULL;
  }

  // Mark as completed to prevent re-analysis
  temp_decl->decl.analysis_state = TICK_ANALYSIS_STATE_COMPLETED;

  // 6. Create identifier referencing the temp
  tick_ast_node_t* temp_ident = create_temp_identifier(ctx->alloc, temp_decl);
  if (!temp_ident) {
    ALOG_EXIT("failed to create temp identifier");
    return NULL;
  }

  // 7. Return: *__tmpN
  tick_ast_node_t* result =
      create_unary_node(ctx->alloc, UNOP_DEREF, temp_ident, lvalue->loc);

  // Mark as synthetic to prevent re-decomposition if analyze_stmt is called
  // again
  result->node_flags |= TICK_NODE_FLAG_SYNTHETIC;

  ALOG_EXIT("created __tmp%u", tmpid);
  return result;
}

// Analyze type node - resolve builtin type names and user-defined types
static tick_analyze_error_t analyze_type(tick_ast_node_t* type_node,
                                         tick_analyze_ctx_t* ctx) {
  if (!type_node) return TICK_ANALYZE_OK;

  switch (type_node->kind) {
    case TICK_AST_TYPE_NAMED:
      // Skip lookup for synthetic nodes that are already resolved
      if (tick_node_is_synthetic(type_node) &&
          tick_type_is_resolved(type_node)) {
        // This is a synthetic node from alloc_type_node, already resolved
        ALOG("type: %s (synthetic)",
             tick_builtin_type_str(type_node->type_named.builtin_type));
        return TICK_ANALYZE_OK;
      }

      // Resolve type via type table lookup and track dependencies
      resolve_named_type(type_node, ctx, true);
      return TICK_ANALYZE_OK;

    case TICK_AST_TYPE_POINTER: {
      // For pointer types, we only need to resolve the type name, not analyze
      // the full declaration Pointers work with forward declarations only
      tick_ast_node_t* pointee = type_node->type_pointer.pointee_type;
      if (pointee && pointee->kind == TICK_AST_TYPE_NAMED) {
        // Look up type name without adding dependency
        ALOG("pointer type: *%.*s", (int)pointee->type_named.name.sz,
             pointee->type_named.name.buf);

        // Skip lookup for synthetic nodes that are already resolved
        if (!tick_node_is_synthetic(pointee) ||
            tick_type_is_unresolved(pointee)) {
          resolve_named_type(pointee, ctx, false);
        }

        ALOG("  -> %s",
             tick_builtin_type_str(pointee->type_named.builtin_type));
      } else if (pointee) {
        // For complex pointee types (arrays, functions, etc), recurse normally
        return analyze_type(pointee, ctx);
      }
      return TICK_ANALYZE_OK;
    }

    case TICK_AST_TYPE_ARRAY:
      // Reduce array size to literal if it's a constant expression
      if (type_node->type_array.size) {
        // Try to reduce to literal, but don't fail if it's already a literal
        reduce_to_literal(&type_node->type_array.size, ctx->alloc);
      }
      return analyze_type(type_node->type_array.element_type, ctx);

    case TICK_AST_TYPE_FUNCTION: {
      // Analyze return type
      tick_analyze_error_t err =
          analyze_type(type_node->type_function.return_type, ctx);
      if (err != TICK_ANALYZE_OK) return err;

      // Analyze parameter types
      tick_ast_node_t* param = type_node->type_function.params;
      while (param) {
        err = analyze_type(param->param.type, ctx);
        if (err != TICK_ANALYZE_OK) return err;
        param = param->next;
      }
      return TICK_ANALYZE_OK;
    }

    default:
      return TICK_ANALYZE_OK;
  }
}

// Get the type of an expression (returns resolved_type or infers it)
static tick_ast_node_t* analyze_expr(tick_ast_node_t* expr,
                                     tick_analyze_ctx_t* ctx) {
  if (!expr) return NULL;

  switch (expr->kind) {
    case TICK_AST_LITERAL:
      return analyze_literal_expr(expr, ctx);

    case TICK_AST_BINARY_EXPR:
      return analyze_binary_expr(expr, ctx);

    case TICK_AST_UNARY_EXPR:
      return analyze_unary_expr(expr, ctx);

    case TICK_AST_CAST_EXPR:
      return analyze_cast_expr(expr, ctx);

    case TICK_AST_STRUCT_INIT_EXPR:
      return analyze_struct_init_expr(expr, ctx);

    case TICK_AST_ARRAY_INIT_EXPR:
      return analyze_array_init_expr(expr, ctx);

    case TICK_AST_IDENTIFIER_EXPR:
      return analyze_identifier_expr(expr, ctx);

    case TICK_AST_CALL_EXPR:
      return analyze_call_expr(expr, ctx);

    case TICK_AST_INDEX_EXPR:
      return analyze_index_expr(expr, ctx);

    case TICK_AST_UNWRAP_PANIC_EXPR:
      return analyze_unwrap_panic_expr(expr, ctx);

    case TICK_AST_FIELD_ACCESS_EXPR:
      return analyze_field_access_expr(expr, ctx);

    default:
      return NULL;
  }
}

// ============================================================================
// Expression Analysis Helper Implementations
// ============================================================================

// Helper to determine if we need a dependency on a declaration for its type
// Returns true if the type is not yet available or fully resolved
static bool needs_type_dependency(tick_ast_node_t* decl,
                                  tick_ast_node_t* type) {
  (void)decl;  // Parameter reserved for future use

  // If no type at all, need the declaration
  if (!type) return true;

  // If type is unresolved (TICK_TYPE_UNKNOWN), need the declaration
  if (tick_type_is_unresolved(type)) return true;

  // For user-defined types, ensure the type definition is available
  // Note: We don't need full analysis - type entry is sufficient for forward
  // references (pointers can use forward declarations)
  if (tick_type_is_user_defined(type) && !type->type_named.type_entry) {
    return true;
  }

  // For function types, we only need the signature, not the body
  // If decl.type exists and is resolved, we have the signature
  if (type->kind == TICK_AST_TYPE_FUNCTION) {
    // Check if all param types and return type are resolved
    if (type->type_function.return_type &&
        tick_type_is_unresolved(type->type_function.return_type)) {
      return true;
    }
    for (tick_ast_node_t* param = type->type_function.params; param;
         param = param->next) {
      if (param->kind == TICK_AST_PARAM && param->param.type &&
          tick_type_is_unresolved(param->param.type)) {
        return true;
      }
    }
    // Signature is resolved, we don't need to wait for body
    return false;
  }

  // Type is resolved and available
  return false;
}

static tick_ast_node_t* analyze_literal_expr(tick_ast_node_t* expr,
                                             tick_analyze_ctx_t* ctx) {
  // Infer type based on literal kind
  if (expr->literal.kind == TICK_LIT_STRING) {
    // String literals are *u8 (pointer to u8)
    tick_ast_node_t* u8_type = alloc_type_node(ctx->alloc, TICK_TYPE_U8);
    if (!u8_type) return NULL;

    // Create pointer type node
    tick_ast_node_t* ptr_type;
    ALLOC_AST_NODE(ctx->alloc, ptr_type);
    ptr_type->kind = TICK_AST_TYPE_POINTER;
    ptr_type->type_pointer.pointee_type = u8_type;
    return ptr_type;
  } else if (expr->literal.kind == TICK_LIT_BOOL) {
    // Transform bool literal to uint: true -> 1, false -> 0
    expr->literal.kind = TICK_LIT_UINT;
    expr->literal.data.uint_value = expr->literal.data.bool_value ? 1 : 0;
    return alloc_type_node(ctx->alloc, TICK_TYPE_BOOL);
  } else if (expr->literal.kind == TICK_LIT_INT) {
    // Signed integer literals: use smallest type that fits
    int64_t value = expr->literal.data.int_value;
    if (value >= -128 && value <= 127) {
      return alloc_type_node(ctx->alloc, TICK_TYPE_I8);
    } else if (value >= -32768 && value <= 32767) {
      return alloc_type_node(ctx->alloc, TICK_TYPE_I16);
    } else if (value >= -2147483648LL && value <= 2147483647LL) {
      return alloc_type_node(ctx->alloc, TICK_TYPE_I32);
    } else {
      return alloc_type_node(ctx->alloc, TICK_TYPE_I64);
    }
  } else if (expr->literal.kind == TICK_LIT_UINT) {
    // Unsigned integer literals: prefer signed types
    uint64_t value = expr->literal.data.uint_value;
    if (value <= 127) {
      return alloc_type_node(ctx->alloc, TICK_TYPE_I8);
    } else if (value <= 32767) {
      return alloc_type_node(ctx->alloc, TICK_TYPE_I16);
    } else if (value <= 2147483647) {
      return alloc_type_node(ctx->alloc, TICK_TYPE_I32);
    } else if (value <= 9223372036854775807ULL) {
      return alloc_type_node(ctx->alloc, TICK_TYPE_I64);
    } else {
      // Only use unsigned if value doesn't fit in i64
      return alloc_type_node(ctx->alloc, TICK_TYPE_U64);
    }
  } else {
    // Fallback for other literal types
    return alloc_type_node(ctx->alloc, TICK_TYPE_I32);
  }
}

static tick_ast_node_t* analyze_binary_expr(tick_ast_node_t* expr,
                                            tick_analyze_ctx_t* ctx) {
  RETURN_IF_RESOLVED(expr, binary_expr);

#ifdef TICK_DEBUG_ANALYZE
  ALOG_ENTER("binary: %s", tick_binop_str(expr->binary_expr.op));
#endif

  // Decompose left and right operands if complex
  DECOMPOSE_FIELD(expr, binary_expr.left);
  DECOMPOSE_FIELD(expr, binary_expr.right);

  // Analyze operands (after decomposition, they should be simple)
  tick_ast_node_t* left_type = analyze_expr(expr->binary_expr.left, ctx);
  tick_ast_node_t* right_type = analyze_expr(expr->binary_expr.right, ctx);

  tick_ast_node_t* result_type = NULL;

  // Special handling for orelse operator
  if (expr->binary_expr.op == BINOP_ORELSE) {
    if (left_type && left_type->kind == TICK_AST_TYPE_OPTIONAL) {
      result_type = left_type->type_optional.inner_type;
    } else if (right_type) {
      result_type = right_type;
    }
  } else {
    // Check if both operands have valid, resolved types
    if (!left_type || !right_type) {
      result_type = NULL;
    } else if (tick_type_is_unresolved(left_type)) {
      result_type = NULL;
    } else if (tick_type_is_unresolved(right_type)) {
      result_type = NULL;
    } else if (left_type->kind == TICK_AST_TYPE_NAMED) {
      result_type =
          alloc_type_node(ctx->alloc, left_type->type_named.builtin_type);
    }
  }

  expr->binary_expr.resolved_type = result_type;

  // Populate builtin semantic category
  expr->binary_expr.builtin = (tick_builtin_t)0;
  if (result_type && result_type->kind == TICK_AST_TYPE_NAMED) {
    tick_builtin_type_t type = result_type->type_named.builtin_type;
    tick_builtin_t builtin = map_binop_to_builtin(expr->binary_expr.op, type);
    if (builtin != 0) {
      expr->binary_expr.builtin = builtin;
    }
  }

#ifdef TICK_DEBUG_ANALYZE
  ALOG_EXIT("-> %s", tick_type_str(result_type));
#endif
  return result_type;
}

static tick_ast_node_t* analyze_identifier_expr(tick_ast_node_t* expr,
                                                tick_analyze_ctx_t* ctx) {
  // Check if this is an AT builtin (@identifier)
  if (expr->identifier_expr.name.sz > 0 &&
      expr->identifier_expr.name.buf[0] == '@') {
    // Resolve @builtin string to enum
    if (expr->identifier_expr.name.sz == 4 &&
        memcmp(expr->identifier_expr.name.buf, "@dbg", 4) == 0) {
      expr->identifier_expr.at_builtin = TICK_AT_BUILTIN_DBG;
      return NULL;
    } else if (expr->identifier_expr.name.sz == 6 &&
               memcmp(expr->identifier_expr.name.buf, "@panic", 6) == 0) {
      expr->identifier_expr.at_builtin = TICK_AT_BUILTIN_PANIC;
      return NULL;
    } else {
      ANALYSIS_ERROR(ctx, expr->loc, "unknown builtin '%.*s'",
                     (int)expr->identifier_expr.name.sz,
                     expr->identifier_expr.name.buf);
      return NULL;
    }
  }

  // Look up identifier in symbol table and cache the result
  if (!expr->identifier_expr.symbol) {
    expr->identifier_expr.symbol = tick_scope_lookup_symbol(
        ctx->current_scope, expr->identifier_expr.name);
  }

  tick_symbol_t* sym = expr->identifier_expr.symbol;
  if (!sym) {
    ANALYSIS_ERROR(ctx, expr->loc, "undefined identifier '%.*s'",
                   (int)expr->identifier_expr.name.sz,
                   expr->identifier_expr.name.buf);
    return NULL;
  }

  // Annotate with prefix requirement for codegen
  expr->identifier_expr.needs_user_prefix = identifier_needs_user_prefix(expr);

  // Get type from symbol's declaration
  tick_ast_node_t* decl = sym->decl;
  tick_ast_node_t* result_type = get_symbol_type(sym);

  // Track dependencies for module-level declarations
  if (decl && decl->kind == TICK_AST_DECL) {
    tick_symbol_t* module_sym =
        tick_scope_lookup_symbol(ctx->module_scope, expr->identifier_expr.name);
    bool is_module_level = (module_sym && module_sym->decl == decl);

    bool needs_dep = needs_type_dependency(decl, result_type);

    if (is_module_level && needs_dep &&
        decl->decl.analysis_state != TICK_ANALYSIS_STATE_COMPLETED &&
        decl->decl.analysis_state != TICK_ANALYSIS_STATE_IN_PROGRESS) {
      tick_dependency_list_add(&ctx->pending_deps, decl);
      ALOG("! dep: %.*s", (int)decl->decl.name.sz, decl->decl.name.buf);
    }
  }

  if (!result_type) {
    ALOG("id: %.*s -> type not available yet",
         (int)expr->identifier_expr.name.sz, expr->identifier_expr.name.buf);
    return NULL;
  }

  ALOG("id: %.*s -> %s", (int)expr->identifier_expr.name.sz,
       expr->identifier_expr.name.buf, tick_type_str(result_type));

  return result_type;
}

static tick_ast_node_t* analyze_unary_expr(tick_ast_node_t* expr,
                                           tick_analyze_ctx_t* ctx) {
  RETURN_IF_RESOLVED(expr, unary_expr);

  ALOG_ENTER("unary: %s", tick_unop_str(expr->unary_expr.op));

  // Decompose complex operands (nested operations, calls, etc.)
  DECOMPOSE_FIELD(expr, unary_expr.operand);

  // Analyze operand
  tick_ast_node_t* operand_type = analyze_expr(expr->unary_expr.operand, ctx);

  // Check if operand type is valid and resolved
  tick_ast_node_t* result_type = NULL;
  if (!operand_type) {
    // Operand not analyzed yet
    result_type = NULL;
  } else if (tick_type_is_unresolved(operand_type)) {
    // Operand type not resolved yet
    result_type = NULL;
  } else {
    // Determine result type based on operator
    switch (expr->unary_expr.op) {
      case UNOP_ADDR: {
        // Address-of operator: T -> *T
        tick_ast_node_t* ptr_type;
        ALLOC_AST_NODE(ctx->alloc, ptr_type);
        ptr_type->kind = TICK_AST_TYPE_POINTER;
        ptr_type->type_pointer.pointee_type = operand_type;
        result_type = ptr_type;
        break;
      }

      case UNOP_DEREF: {
        // Dereference operator: *T -> T
        if (operand_type->kind == TICK_AST_TYPE_POINTER) {
          result_type = operand_type->type_pointer.pointee_type;

          // When dereferencing, we need the full type definition (not just
          // forward decl) Analyze the pointee type to ensure it's resolved
          // and track dependencies
          if (result_type) {
            tick_analyze_error_t err = analyze_type(result_type, ctx);
            if (err != TICK_ANALYZE_OK) {
              result_type = NULL;
            }
          }
        } else {
          ANALYSIS_ERROR(ctx, expr->loc,
                         "cannot dereference non-pointer type %s",
                         tick_type_str(operand_type));
          result_type = NULL;
        }
        break;
      }

      case UNOP_NOT:
        // Logical NOT: returns bool
        result_type = alloc_type_node(ctx->alloc, TICK_TYPE_BOOL);
        break;

      case UNOP_NEG:
      case UNOP_BIT_NOT:
        // Arithmetic/bitwise operators: same type as operand
        result_type = operand_type;
        break;

      default:
        result_type = operand_type;
        break;
    }
  }

  expr->unary_expr.resolved_type = result_type;

  // Populate builtin semantic category for negation (codegen will map to
  // runtime funcs)
  expr->unary_expr.builtin = (tick_builtin_t)0;  // Default: no builtin
  if (expr->unary_expr.op == UNOP_NEG && result_type &&
      result_type->kind == TICK_AST_TYPE_NAMED) {
    tick_builtin_type_t type = result_type->type_named.builtin_type;
    bool is_signed = (type >= TICK_TYPE_I8 && type <= TICK_TYPE_ISZ);
    if (is_signed) {
      // Store semantic operation category for codegen
      expr->unary_expr.builtin = TICK_BUILTIN_CHECKED_NEG;
    }
  }

  ALOG_EXIT("-> %s", tick_type_str(result_type));
  return result_type;
}

static tick_ast_node_t* analyze_cast_expr(tick_ast_node_t* expr,
                                          tick_analyze_ctx_t* ctx) {
  RETURN_IF_RESOLVED(expr, cast_expr);

  // Decompose complex source expressions
  DECOMPOSE_FIELD(expr, cast_expr.expr);

  // Analyze source expression (for validation)
  analyze_expr(expr->cast_expr.expr, ctx);

  // Result type is the cast target type
  analyze_type(expr->cast_expr.type, ctx);
  expr->cast_expr.resolved_type = expr->cast_expr.type;

  return expr->cast_expr.type;
}

static tick_ast_node_t* analyze_struct_init_expr(tick_ast_node_t* expr,
                                                 tick_analyze_ctx_t* ctx) {
  // Analyze the struct type
  analyze_type(expr->struct_init_expr.type, ctx);

  // Analyze field initializer expressions
  for (tick_ast_node_t* field = expr->struct_init_expr.fields; field;
       field = field->next) {
    if (field->kind == TICK_AST_STRUCT_INIT_FIELD) {
      // Recursively analyze the field value
      analyze_expr(field->struct_init_field.value, ctx);
    }
  }

  // Note: Decomposition happens at the statement level (in DECL case)
  // not here, so struct inits can still contain complex expressions

  return expr->struct_init_expr.type;
}

static tick_ast_node_t* analyze_array_init_expr(tick_ast_node_t* expr,
                                                tick_analyze_ctx_t* ctx) {
  // Analyze array element expressions
  tick_ast_node_t* elem = expr->array_init_expr.elements;
  while (elem) {
    analyze_expr(elem, ctx);
    elem = elem->next;
  }

  // Note: Decomposition happens at the statement level (in DECL case)
  // Array init doesn't have a specific type - inferred from context
  return NULL;
}

static tick_ast_node_t* analyze_call_expr(tick_ast_node_t* expr,
                                          tick_analyze_ctx_t* ctx) {
  // Log function name if it's an identifier
#ifdef TICK_DEBUG_ANALYZE
  if (expr->call_expr.callee &&
      expr->call_expr.callee->kind == TICK_AST_IDENTIFIER_EXPR) {
    ALOG_ENTER("call %.*s",
               (int)expr->call_expr.callee->identifier_expr.name.sz,
               expr->call_expr.callee->identifier_expr.name.buf);
  } else {
    ALOG_ENTER("call");
  }
#endif

  // Decompose callee if it's complex (e.g., function pointer deref, field
  // access)
  DECOMPOSE_FIELD(expr, call_expr.callee);

  // Analyze callee (this will resolve @builtin identifiers and get function
  // type)
  tick_ast_node_t* callee_type = analyze_expr(expr->call_expr.callee, ctx);

  // Decompose and analyze arguments
  // All arguments must be simple (LITERAL or IDENTIFIER_EXPR)
  // Note: Arguments form a linked list, so we need to be careful with
  // decomposition
  tick_ast_node_t** arg_ptr = &expr->call_expr.args;
  while (*arg_ptr) {
    DECOMPOSE_LIST_ELEM(arg_ptr);
    analyze_expr(*arg_ptr, ctx);
    arg_ptr = &(*arg_ptr)->next;
  }

  // Get return type from function signature
  tick_ast_node_t* result_type = NULL;
  if (callee_type && callee_type->kind == TICK_AST_TYPE_FUNCTION) {
    result_type = callee_type->type_function.return_type;
    ALOG_EXIT("-> %s", tick_type_str(result_type));
  } else if (!callee_type) {
    // Callee type not available yet (function not analyzed)
    // Dependencies should have been added by analyze_expr above
    ALOG_EXIT("-> type not available yet");
    result_type = NULL;
  } else {
    // @builtins - explicitly return void
    // Check if it's a known @builtin
    if (expr->call_expr.callee &&
        expr->call_expr.callee->kind == TICK_AST_IDENTIFIER_EXPR) {
      tick_at_builtin_t builtin =
          expr->call_expr.callee->identifier_expr.at_builtin;
      if (builtin == TICK_AT_BUILTIN_DBG) {
        result_type = alloc_type_node(ctx->alloc, TICK_TYPE_VOID);
        ALOG_EXIT("-> void (@dbg)");
      } else if (builtin == TICK_AT_BUILTIN_PANIC) {
        result_type = alloc_type_node(ctx->alloc, TICK_TYPE_VOID);
        ALOG_EXIT("-> void (@panic)");
      } else {
        // Unknown callee - this should not happen
        CHECK(0, "call expression has non-function callee type");
      }
    } else {
      // Unknown callee - this should not happen
      CHECK(0, "call expression has non-function callee type");
    }
  }

  return result_type;
}

static tick_ast_node_t* analyze_index_expr(tick_ast_node_t* expr,
                                           tick_analyze_ctx_t* ctx) {
  ALOG_ENTER("index");

  // Decompose complex array and index expressions
  DECOMPOSE_FIELD(expr, index_expr.array);
  DECOMPOSE_FIELD(expr, index_expr.index);

  // Analyze array/pointer expression
  tick_ast_node_t* array_type = analyze_expr(expr->index_expr.array, ctx);

  // Analyze index expression
  tick_ast_node_t* index_type = analyze_expr(expr->index_expr.index, ctx);

  // Validate index is numeric
  if (index_type && !tick_type_is_numeric(index_type)) {
    ANALYSIS_ERROR(ctx, expr->loc, "array index must be numeric, got %s",
                   tick_type_str(index_type));
    ALOG_EXIT("FAILED");
    return NULL;
  }

  // Determine element type based on array type
  tick_ast_node_t* result_type = NULL;
  if (array_type) {
    if (array_type->kind == TICK_AST_TYPE_POINTER) {
      // Pointer indexing: *T -> T
      result_type = array_type->type_pointer.pointee_type;
    } else if (array_type->kind == TICK_AST_TYPE_ARRAY ||
               array_type->kind == TICK_AST_TYPE_SLICE) {
      // Array/Slice indexing: [N]T -> T or []T -> T
      // (SLICE uses type_array with size=NULL)
      result_type = array_type->type_array.element_type;
    } else {
      ANALYSIS_ERROR(ctx, expr->loc, "cannot index into type %s",
                     tick_type_str(array_type));
      ALOG_EXIT("FAILED");
      return NULL;
    }
  }

  ALOG_EXIT("-> %s", tick_type_str(result_type));
  return result_type;
}

static tick_ast_node_t* analyze_unwrap_panic_expr(tick_ast_node_t* expr,
                                                  tick_analyze_ctx_t* ctx) {
  RETURN_IF_RESOLVED(expr, unwrap_panic_expr);

  // Decompose complex operands
  DECOMPOSE_FIELD(expr, unwrap_panic_expr.operand);

  // Analyze operand
  tick_ast_node_t* operand_type =
      analyze_expr(expr->unwrap_panic_expr.operand, ctx);

  // For 'a.?': a is ?T, result is T
  tick_ast_node_t* result_type = NULL;
  if (operand_type && operand_type->kind == TICK_AST_TYPE_OPTIONAL) {
    // Result type is the inner type of the optional
    result_type = operand_type->type_optional.inner_type;
  }

  expr->unwrap_panic_expr.resolved_type = result_type;
  return result_type;
}

static tick_ast_node_t* analyze_field_access_expr(tick_ast_node_t* expr,
                                                  tick_analyze_ctx_t* ctx) {
  RETURN_IF_RESOLVED(expr, field_access_expr);

  ALOG_ENTER("field access: .%.*s", (int)expr->field_access_expr.field_name.sz,
             expr->field_access_expr.field_name.buf);

  // Decompose complex object expressions
  DECOMPOSE_FIELD(expr, field_access_expr.object);

  // Check if object is a type name (for enum value access like Color.Red)
  tick_ast_node_t* object_type = NULL;

  if (expr->field_access_expr.object &&
      expr->field_access_expr.object->kind == TICK_AST_IDENTIFIER_EXPR) {
    // Try to look up as a type name first
    tick_buf_t obj_name = expr->field_access_expr.object->identifier_expr.name;
    tick_type_entry_t* type_entry = tick_types_lookup(ctx->types, obj_name);

    if (type_entry) {
      // Object is a type name - create a TYPE_NAMED node for it
      object_type = alloc_user_type_node(ctx->alloc, obj_name);
      if (object_type) {
        object_type->type_named.type_entry = type_entry;
        object_type->type_named.builtin_type = type_entry->builtin_type;
        ALOG("object is type name: %.*s", (int)obj_name.sz, obj_name.buf);
      }
    }
  }

  if (!object_type) {
    // Not a type name, analyze as expression
    object_type = analyze_expr(expr->field_access_expr.object, ctx);
  }

  tick_ast_node_t* result_type = NULL;

  if (!object_type) {
    // Object type not available yet
    ALOG_EXIT("object type not available");
    expr->field_access_expr.resolved_type = NULL;
    return NULL;
  }

  // Handle automatic pointer dereferencing (single-level only)
  // In Tick, obj.field works for both T and *T (like Go)
  tick_ast_node_t* base_type = object_type;
  if (object_type->kind == TICK_AST_TYPE_POINTER) {
    base_type = object_type->type_pointer.pointee_type;
    expr->field_access_expr.object_is_pointer = true;
    ALOG("dereferencing pointer to access field");

    // Analyze the pointee type to ensure it's resolved and track
    // dependencies
    if (base_type) {
      tick_analyze_error_t err = analyze_type(base_type, ctx);
      if (err != TICK_ANALYZE_OK) {
        ALOG_EXIT("FAILED to analyze pointee type");
        expr->field_access_expr.resolved_type = NULL;
        return NULL;
      }
    }
  } else {
    expr->field_access_expr.object_is_pointer = false;
  }

  // Check if we have a valid base type
  if (!base_type) {
    ANALYSIS_ERROR(ctx, expr->loc, "cannot access field of null type");
    ALOG_EXIT("FAILED: null base type");
    expr->field_access_expr.resolved_type = NULL;
    return NULL;
  }

  // Base type must be a named user-defined type (struct/union/enum)
  if (base_type->kind != TICK_AST_TYPE_NAMED) {
    ANALYSIS_ERROR(ctx, expr->loc,
                   "cannot access field of non-aggregate type %s",
                   tick_type_str(base_type));
    ALOG_EXIT("FAILED: not a named type");
    expr->field_access_expr.resolved_type = NULL;
    return NULL;
  }

  // Check if type is resolved
  if (tick_type_is_unresolved(base_type)) {
    ALOG_EXIT("base type not resolved yet");
    expr->field_access_expr.resolved_type = NULL;
    return NULL;
  }

  // Type must be user-defined (struct/union/enum)
  if (base_type->type_named.builtin_type != TICK_TYPE_USER_DEFINED) {
    ANALYSIS_ERROR(ctx, expr->loc, "cannot access field of builtin type %s",
                   tick_builtin_type_str(base_type->type_named.builtin_type));
    ALOG_EXIT("FAILED: not a user-defined type");
    expr->field_access_expr.resolved_type = NULL;
    return NULL;
  }

  // Get the type entry from the type table
  tick_type_entry_t* type_entry = base_type->type_named.type_entry;
  if (!type_entry || !type_entry->parent_decl) {
    ANALYSIS_ERROR(ctx, expr->loc, "type entry not found in type table");
    ALOG_EXIT("FAILED: no type entry");
    expr->field_access_expr.resolved_type = NULL;
    return NULL;
  }

  // Get the declaration node
  tick_ast_node_t* type_decl = type_entry->parent_decl;
  if (!type_decl || type_decl->kind != TICK_AST_DECL || !type_decl->decl.init) {
    ANALYSIS_ERROR(ctx, expr->loc, "invalid type declaration");
    ALOG_EXIT("FAILED: invalid decl");
    expr->field_access_expr.resolved_type = NULL;
    return NULL;
  }

  tick_ast_node_t* type_def = type_decl->decl.init;
  tick_buf_t field_name = expr->field_access_expr.field_name;

  // Look up field based on type kind
  switch (type_def->kind) {
    case TICK_AST_STRUCT_DECL: {
      ALOG("looking up field in struct %.*s", (int)type_decl->decl.name.sz,
           type_decl->decl.name.buf);

      // Search for field in struct fields
      for (tick_ast_node_t* field = type_def->struct_decl.fields; field;
           field = field->next) {
        if (field->kind != TICK_AST_FIELD) continue;

        if (field->field.name.sz == field_name.sz &&
            memcmp(field->field.name.buf, field_name.buf, field_name.sz) == 0) {
          // Found the field!
          result_type = field->field.type;
          ALOG("found field: %.*s -> %s", (int)field_name.sz, field_name.buf,
               tick_type_str(result_type));

          // Track dependencies if field type is unresolved
          if (result_type) {
            tick_analyze_error_t err = analyze_type(result_type, ctx);
            if (err != TICK_ANALYZE_OK) {
              result_type = NULL;
            }
          }
          break;
        }
      }

      if (!result_type) {
        ANALYSIS_ERROR(ctx, expr->loc, "struct '%.*s' has no field '%.*s'",
                       (int)type_decl->decl.name.sz, type_decl->decl.name.buf,
                       (int)field_name.sz, field_name.buf);
        ALOG_EXIT("FAILED: field not found");
      }
      break;
    }

    case TICK_AST_UNION_DECL: {
      ALOG("looking up field in union %.*s", (int)type_decl->decl.name.sz,
           type_decl->decl.name.buf);

      // Search for field in union fields
      for (tick_ast_node_t* field = type_def->union_decl.fields; field;
           field = field->next) {
        if (field->kind != TICK_AST_FIELD) continue;

        if (field->field.name.sz == field_name.sz &&
            memcmp(field->field.name.buf, field_name.buf, field_name.sz) == 0) {
          // Found the field!
          result_type = field->field.type;
          ALOG("found field: %.*s -> %s", (int)field_name.sz, field_name.buf,
               tick_type_str(result_type));

          // Track dependencies if field type is unresolved
          if (result_type) {
            tick_analyze_error_t err = analyze_type(result_type, ctx);
            if (err != TICK_ANALYZE_OK) {
              result_type = NULL;
            }
          }
          break;
        }
      }

      if (!result_type) {
        ANALYSIS_ERROR(ctx, expr->loc, "union '%.*s' has no field '%.*s'",
                       (int)type_decl->decl.name.sz, type_decl->decl.name.buf,
                       (int)field_name.sz, field_name.buf);
        ALOG_EXIT("FAILED: field not found");
      }
      break;
    }

    case TICK_AST_ENUM_DECL: {
      ALOG("looking up value in enum %.*s", (int)type_decl->decl.name.sz,
           type_decl->decl.name.buf);

      // Search for value in enum values
      bool found = false;
      for (tick_ast_node_t* value = type_def->enum_decl.values; value;
           value = value->next) {
        if (value->kind != TICK_AST_ENUM_VALUE) continue;

        if (value->enum_value.name.sz == field_name.sz &&
            memcmp(value->enum_value.name.buf, field_name.buf, field_name.sz) ==
                0) {
          // Found the enum value!
          // Transform field access node to ENUM_VALUE node
          expr->kind = TICK_AST_ENUM_VALUE;
          expr->enum_value.parent_decl = type_decl;
          expr->enum_value.name = field_name;
          expr->enum_value.value = value->enum_value.value;

          result_type = type_decl->decl.type;
          found = true;
          ALOG("transformed enum field access %.*s.%.*s -> ENUM_VALUE",
               (int)type_decl->decl.name.sz, type_decl->decl.name.buf,
               (int)field_name.sz, field_name.buf);

          // Return early since we transformed the node
          ALOG_EXIT("-> %s", tick_type_str(result_type));
          return result_type;
        }
      }

      if (!found) {
        ANALYSIS_ERROR(ctx, expr->loc, "enum '%.*s' has no value '%.*s'",
                       (int)type_decl->decl.name.sz, type_decl->decl.name.buf,
                       (int)field_name.sz, field_name.buf);
        ALOG_EXIT("FAILED: enum value not found");
      }
      break;
    }

    default:
      ANALYSIS_ERROR(ctx, expr->loc, "cannot access fields of type %s",
                     tick_type_str(base_type));
      ALOG_EXIT("FAILED: unsupported type kind");
      break;
  }

  expr->field_access_expr.resolved_type = result_type;
  ALOG_EXIT("-> %s", tick_type_str(result_type));
  return result_type;
}

// ============================================================================
// Scope Guard for RAII-style State Management
// ============================================================================

// Initialize guard with current context (saves nothing yet)
static void scope_guard_init(tick_scope_guard_t* guard,
                             tick_analyze_ctx_t* ctx) {
  guard->ctx = ctx;
  guard->prev_function_scope = NULL;
  guard->prev_block = NULL;
  guard->prev_stmt = NULL;
  guard->did_push_scope = false;
}

// Push a new scope (increments depth, creates new scope)
static void scope_guard_push_scope(tick_scope_guard_t* guard) {
  tick_scope_push(guard->ctx);
  guard->did_push_scope = true;
}

// Save and set function_scope
static void scope_guard_set_function_scope(tick_scope_guard_t* guard,
                                           tick_scope_t* new_scope) {
  guard->prev_function_scope = guard->ctx->function_scope;
  guard->ctx->function_scope = new_scope;
}

// Save and set current_block
static void scope_guard_set_block(tick_scope_guard_t* guard,
                                  tick_ast_node_t* new_block) {
  guard->prev_block = guard->ctx->current_block;
  guard->ctx->current_block = new_block;
}

// Save and set current_stmt
static void scope_guard_set_stmt(tick_scope_guard_t* guard,
                                 tick_ast_node_t* new_stmt) {
  guard->prev_stmt = guard->ctx->current_stmt;
  guard->ctx->current_stmt = new_stmt;
}

// Restore all saved state in reverse order of setup
static void scope_guard_destroy(tick_scope_guard_t* guard) {
  // Restore current_stmt (last set, first restored)
  if (guard->prev_stmt != NULL || guard->ctx->current_stmt != NULL) {
    guard->ctx->current_stmt = guard->prev_stmt;
  }

  // Restore current_block
  if (guard->prev_block != NULL || guard->ctx->current_block != NULL) {
    guard->ctx->current_block = guard->prev_block;
  }

  // Restore function_scope
  if (guard->prev_function_scope != NULL ||
      guard->ctx->function_scope != guard->ctx->module_scope) {
    guard->ctx->function_scope = guard->prev_function_scope;
  }

  // Pop scope (last pushed, first popped - LIFO)
  if (guard->did_push_scope) {
    tick_scope_pop(guard->ctx);
  }
}

// Analyze a statement
static tick_analyze_error_t analyze_stmt(tick_ast_node_t* stmt,
                                         tick_analyze_ctx_t* ctx) {
  if (!stmt) return TICK_ANALYZE_OK;

  switch (stmt->kind) {
    case TICK_AST_BLOCK_STMT: {
      // Use scope guard for automatic state management
      tick_scope_guard_t guard;
      scope_guard_init(&guard, ctx);
      scope_guard_push_scope(&guard);
      scope_guard_set_block(&guard, stmt);

      // Iterate through statements, tracking current_stmt for temporary
      // insertion
      for (tick_ast_node_t* s = stmt->block_stmt.stmts; s; s = s->next) {
        // Set current_stmt so temporaries are inserted before this statement
        scope_guard_set_stmt(&guard, s);

        tick_analyze_error_t err = analyze_stmt(s, ctx);
        if (err != TICK_ANALYZE_OK) {
          scope_guard_destroy(&guard);  // Auto-cleanup on error
          return err;
        }
      }

      // Clear current_stmt after loop
      scope_guard_set_stmt(&guard, NULL);

      // Cleanup: restore all saved state
      scope_guard_destroy(&guard);
      return TICK_ANALYZE_OK;
    }

    case TICK_AST_RETURN_STMT: {
      // Don't decompose here - analyze_expr handles it internally
      if (stmt->return_stmt.value) {
        analyze_expr(stmt->return_stmt.value, ctx);
        if (analyze_has_error(ctx)) {
          return TICK_ANALYZE_ERR;
        }
      }
      return TICK_ANALYZE_OK;
    }

    case TICK_AST_DECL: {
      ALOG_ENTER("%.*s", (int)stmt->decl.name.sz, stmt->decl.name.buf);

      // Skip if already analyzed (e.g., compiler-generated temps)
      if (stmt->decl.analysis_state == TICK_ANALYSIS_STATE_COMPLETED) {
        ALOG_EXIT("already analyzed");
        return TICK_ANALYZE_OK;
      }

      // If type is NULL, infer from initializer
      // analyze_expr handles decomposition internally
      if (stmt->decl.type == NULL && stmt->decl.init) {
        tick_ast_node_t* inferred_type = analyze_expr(stmt->decl.init, ctx);
        if (analyze_has_error(ctx)) {
          ALOG_EXIT("FAILED");
          return TICK_ANALYZE_ERR;
        }
        stmt->decl.type = inferred_type;
        ALOG("inferred: %s", tick_type_str(inferred_type));

        // Decompose struct/array init if needed
        if (stmt->decl.init->kind == TICK_AST_STRUCT_INIT_EXPR) {
          stmt->decl.init = decompose_struct_init(stmt->decl.init, ctx);
        } else if (stmt->decl.init->kind == TICK_AST_ARRAY_INIT_EXPR) {
          stmt->decl.init = decompose_array_init(stmt->decl.init, ctx);
        }
      } else if (stmt->decl.init) {
        // Type is explicit, analyze initializer separately
        analyze_expr(stmt->decl.init, ctx);
        if (analyze_has_error(ctx)) {
          ALOG_EXIT("FAILED");
          return TICK_ANALYZE_ERR;
        }

        // Decompose struct/array init if needed
        if (stmt->decl.init->kind == TICK_AST_STRUCT_INIT_EXPR) {
          stmt->decl.init = decompose_struct_init(stmt->decl.init, ctx);
        } else if (stmt->decl.init->kind == TICK_AST_ARRAY_INIT_EXPR) {
          stmt->decl.init = decompose_array_init(stmt->decl.init, ctx);
        }
      }

      // Analyze declaration type (in case it has dependencies)
      tick_analyze_error_t err = analyze_type(stmt->decl.type, ctx);
      if (err != TICK_ANALYZE_OK) {
        ALOG_EXIT("FAILED");
        return err;
      }

      // Transform AST to normalize for codegen
      transform_static_string_decl(stmt, ctx->alloc);
      remove_undefined_init(stmt);

      // Register symbol in current scope
      tick_scope_insert_symbol(ctx->current_scope, stmt->decl.name, stmt);

      ALOG_EXIT("%.*s: %s", (int)stmt->decl.name.sz, stmt->decl.name.buf,
                tick_type_str(stmt->decl.type));
      return TICK_ANALYZE_OK;
    }

    case TICK_AST_ASSIGN_STMT: {
      // Analyze both sides first
      tick_ast_node_t* lhs_type = analyze_expr(stmt->assign_stmt.lhs, ctx);
      if (analyze_has_error(ctx)) {
        return TICK_ANALYZE_ERR;
      }
      tick_ast_node_t* rhs_type = analyze_expr(stmt->assign_stmt.rhs, ctx);
      if (analyze_has_error(ctx)) {
        return TICK_ANALYZE_ERR;
      }

      // Phase 2: Multi-step lvalue decomposition to pointer form
      // Transform complex lvalues into step-by-step pointer temporaries
      // Example: container.field[index].subfield = value
      //   → let __tmp0 = &container.field;
      //     let __tmp1 = &(*__tmp0)[index];
      //     let __tmp2 = &(*__tmp1).subfield;
      //     *__tmp2 = value;
      // Skip for synthetic assignments or already-decomposed lvalues
      if (!is_simple_lvalue(stmt->assign_stmt.lhs) &&
          !(stmt->node_flags & TICK_NODE_FLAG_SYNTHETIC) &&
          !(stmt->assign_stmt.lhs->node_flags & TICK_NODE_FLAG_SYNTHETIC)) {
        tick_ast_node_t* final_lvalue =
            decompose_lvalue_chain(stmt->assign_stmt.lhs, lhs_type, ctx);
        if (!final_lvalue) {
          return TICK_ANALYZE_ERR;
        }
        stmt->assign_stmt.lhs = final_lvalue;
      }

      // Type compatibility checking (for future enhancement)
      // TODO: Add type compatibility validation between lhs_type and rhs_type
      (void)lhs_type;
      (void)rhs_type;

      return TICK_ANALYZE_OK;
    }

    case TICK_AST_EXPR_STMT: {
      // Don't decompose here - analyze_expr handles it internally
      analyze_expr(stmt->expr_stmt.expr, ctx);
      if (analyze_has_error(ctx)) {
        return TICK_ANALYZE_ERR;
      }
      return TICK_ANALYZE_OK;
    }

    case TICK_AST_IF_STMT: {
      // Don't decompose here - analyze_expr handles it internally
      analyze_expr(stmt->if_stmt.condition, ctx);
      if (analyze_has_error(ctx)) {
        return TICK_ANALYZE_ERR;
      }

      // Normalize else block to ensure codegen assumptions:
      // 1. If else_block is NULL, create an empty BLOCK_STMT
      // 2. If else_block is an IF_STMT (else-if), wrap it in a BLOCK_STMT
      if (stmt->if_stmt.else_block == NULL) {
        // Create empty block for missing else clause
        stmt->if_stmt.else_block =
            create_empty_block_stmt(ctx->alloc, stmt->loc);
      } else if (stmt->if_stmt.else_block->kind == TICK_AST_IF_STMT) {
        // Wrap else-if in a block statement
        tick_ast_node_t* wrapper =
            create_empty_block_stmt(ctx->alloc, stmt->if_stmt.else_block->loc);
        wrapper->block_stmt.stmts = stmt->if_stmt.else_block;
        stmt->if_stmt.else_block = wrapper;
      }

      // Analyze then and else blocks (else_block now guaranteed to be non-NULL
      // BLOCK_STMT)
      tick_analyze_error_t err = analyze_stmt(stmt->if_stmt.then_block, ctx);
      if (err != TICK_ANALYZE_OK) return err;
      err = analyze_stmt(stmt->if_stmt.else_block, ctx);
      if (err != TICK_ANALYZE_OK) return err;
      return TICK_ANALYZE_OK;
    }

    case TICK_AST_UNUSED_STMT: {
      // Analyze the expression being discarded
      analyze_expr(stmt->unused_stmt.expr, ctx);
      if (analyze_has_error(ctx)) {
        return TICK_ANALYZE_ERR;
      }
      return TICK_ANALYZE_OK;
    }

    case TICK_AST_SWITCH_STMT: {
      // Analyze the switch value expression
      analyze_expr(stmt->switch_stmt.value, ctx);
      if (analyze_has_error(ctx)) {
        return TICK_ANALYZE_ERR;
      }

      // Normalize each case's statements to be wrapped in a BLOCK_STMT
      tick_ast_node_t* case_node = stmt->switch_stmt.cases;
      while (case_node) {
        // If stmts is not already a BLOCK_STMT, wrap it
        if (case_node->switch_case.stmts == NULL) {
          // Empty case - create empty block
          case_node->switch_case.stmts =
              create_empty_block_stmt(ctx->alloc, case_node->loc);
        } else if (case_node->switch_case.stmts->kind != TICK_AST_BLOCK_STMT) {
          // Wrap the statement list in a BLOCK_STMT
          tick_ast_node_t* wrapper =
              create_empty_block_stmt(ctx->alloc, case_node->loc);
          wrapper->block_stmt.stmts = case_node->switch_case.stmts;
          case_node->switch_case.stmts = wrapper;
        }

        // Analyze the case block
        tick_analyze_error_t err =
            analyze_stmt(case_node->switch_case.stmts, ctx);
        if (err != TICK_ANALYZE_OK) return err;

        case_node = case_node->next;
      }

      return TICK_ANALYZE_OK;
    }

    default:
      return TICK_ANALYZE_OK;
  }
}

// Analyze an enum declaration and populate auto-increment values
static tick_analyze_error_t analyze_enum_decl(tick_ast_node_t* decl,
                                              tick_analyze_ctx_t* ctx) {
  if (!decl || decl->kind != TICK_AST_DECL) return TICK_ANALYZE_OK;

  tick_ast_node_t* enum_node = decl->decl.init;
  if (!enum_node || enum_node->kind != TICK_AST_ENUM_DECL)
    return TICK_ANALYZE_OK;

  ALOG_ENTER("enum %.*s", (int)decl->decl.name.sz, decl->decl.name.buf);

  // Analyze underlying type
  tick_analyze_error_t err =
      analyze_type(enum_node->enum_decl.underlying_type, ctx);
  if (err != TICK_ANALYZE_OK) return err;

  // Walk the enum values and calculate auto-increment
  // The list is in source order (parser builds it with append)
  // We can process it directly to assign auto-increment values

  // First pass: reduce all explicit values to literals
  for (tick_ast_node_t* value_node = enum_node->enum_decl.values; value_node;
       value_node = value_node->next) {
    if (value_node->kind != TICK_AST_ENUM_VALUE) continue;

    if (value_node->enum_value.value != NULL) {
      // Reduce to literal if needed
      reduce_to_literal(&value_node->enum_value.value, ctx->alloc);
    }
  }

  // Second pass: assign auto-increment values in source order
  int64_t current_value = 0;
  for (tick_ast_node_t* value_node = enum_node->enum_decl.values; value_node;
       value_node = value_node->next) {
    if (value_node->kind != TICK_AST_ENUM_VALUE) continue;

    // Set parent pointer so enum values can self-describe for codegen
    value_node->enum_value.parent_decl = decl;

    if (value_node->enum_value.value == NULL) {
      // Auto-increment: create a literal node with current_value
      tick_ast_node_t* literal = alloc_literal_node(ctx->alloc, current_value);
      if (!literal) return TICK_ANALYZE_OK;  // Skip this value if alloc fails
      value_node->enum_value.value = literal;
    } else {
      // Explicit value: already reduced in first pass
      current_value = value_node->enum_value.value->literal.data.int_value;
    }

    // Increment for next value
    current_value++;
  }

  ALOG_EXIT("enum complete");
  return TICK_ANALYZE_OK;
}

// Analyze function signature only (return type + parameter types)
// This is sufficient for function calls and type checking
static tick_analyze_error_t analyze_function_signature(
    tick_ast_node_t* decl, tick_analyze_ctx_t* ctx) {
  if (!decl || decl->kind != TICK_AST_DECL) return TICK_ANALYZE_OK;

  tick_ast_node_t* func = decl->decl.init;
  if (!func || func->kind != TICK_AST_FUNCTION) return TICK_ANALYZE_OK;

  // Skip if signature already analyzed
  if (decl->decl.signature_state == TICK_ANALYSIS_STATE_COMPLETED) {
    return TICK_ANALYZE_OK;
  }

  ALOG_ENTER("fn %.*s signature", (int)decl->decl.name.sz, decl->decl.name.buf);

  // Mark signature as in progress
  decl->decl.signature_state = TICK_ANALYSIS_STATE_IN_PROGRESS;

  // Analyze return type
  tick_analyze_error_t err = analyze_type(func->function.return_type, ctx);
  if (err != TICK_ANALYZE_OK) {
    decl->decl.signature_state = TICK_ANALYSIS_STATE_FAILED;
    ALOG_EXIT("FAILED (return type)");
    return err;
  }

  // Log return type details
#ifdef TICK_DEBUG_ANALYZE
  const char* ret_type_str = "void";
  if (func->function.return_type &&
      func->function.return_type->kind == TICK_AST_TYPE_NAMED) {
    ret_type_str = tick_builtin_type_str(
        func->function.return_type->type_named.builtin_type);
  }
  ALOG("return: %s", ret_type_str);
#endif

  // Analyze parameter types (don't need to enter scope for signature analysis)
  for (tick_ast_node_t* param = func->function.params; param;
       param = param->next) {
    if (param->kind == TICK_AST_PARAM) {
      err = analyze_type(param->param.type, ctx);
      if (err != TICK_ANALYZE_OK) {
        decl->decl.signature_state = TICK_ANALYSIS_STATE_FAILED;
        ALOG_EXIT("FAILED (param type)");
        return err;
      }

      ALOG("param %.*s: %s", (int)param->param.name.sz, param->param.name.buf,
           tick_type_str(param->param.type));
    }
  }

  // Check if signature has unresolved dependencies
  if (ctx->pending_deps.head != NULL) {
    // Reset state so we retry later
    decl->decl.signature_state = TICK_ANALYSIS_STATE_NOT_STARTED;
    ALOG_EXIT("signature has dependencies, deferring");
    return TICK_ANALYZE_OK;
  }

  // Signature complete!
  decl->decl.signature_state = TICK_ANALYSIS_STATE_COMPLETED;
  ALOG_EXIT("signature complete");
  return TICK_ANALYZE_OK;
}

// Analyze function body
// Requires: signature must be completed first
static tick_analyze_error_t analyze_function_body(tick_ast_node_t* decl,
                                                  tick_analyze_ctx_t* ctx) {
  if (!decl || decl->kind != TICK_AST_DECL) return TICK_ANALYZE_OK;

  tick_ast_node_t* func = decl->decl.init;
  if (!func || func->kind != TICK_AST_FUNCTION) return TICK_ANALYZE_OK;

  // Signature must be completed first
  CHECK(decl->decl.signature_state == TICK_ANALYSIS_STATE_COMPLETED,
        "function body analysis requires completed signature");

  // Skip if body already analyzed
  if (decl->decl.body_state == TICK_ANALYSIS_STATE_COMPLETED) {
    return TICK_ANALYZE_OK;
  }

  ALOG_ENTER("fn %.*s body", (int)decl->decl.name.sz, decl->decl.name.buf);

  // Mark body as in progress
  decl->decl.body_state = TICK_ANALYSIS_STATE_IN_PROGRESS;

  // Use scope guard for automatic state management
  tick_scope_guard_t guard;
  scope_guard_init(&guard, ctx);
  scope_guard_push_scope(&guard);
  scope_guard_set_function_scope(&guard, ctx->current_scope);

  // Register parameters in function scope
  // (Signature was already analyzed, so types are resolved)
  for (tick_ast_node_t* param = func->function.params; param;
       param = param->next) {
    if (param->kind == TICK_AST_PARAM) {
      tick_scope_insert_symbol(ctx->current_scope, param->param.name, param);
    }
  }

  // Analyze function body
  tick_analyze_error_t err = analyze_stmt(func->function.body, ctx);
  if (err != TICK_ANALYZE_OK) {
    decl->decl.body_state = TICK_ANALYSIS_STATE_FAILED;
    scope_guard_destroy(&guard);  // Auto-cleanup on error
    ALOG_EXIT("FAILED (body)");
    return err;
  }

  // Cleanup: restore all saved state
  scope_guard_destroy(&guard);

  // Check if body has unresolved dependencies
  if (ctx->pending_deps.head != NULL) {
    // Reset state so we retry later
    decl->decl.body_state = TICK_ANALYSIS_STATE_NOT_STARTED;
    ALOG_EXIT("body has dependencies, deferring");
    return TICK_ANALYZE_OK;
  }

  // Body complete!
  decl->decl.body_state = TICK_ANALYSIS_STATE_COMPLETED;
  ALOG_EXIT("body complete");
  return TICK_ANALYZE_OK;
}

// Analyze a function declaration (coordinator for signature + body)
static tick_analyze_error_t analyze_function(tick_ast_node_t* decl,
                                             tick_analyze_ctx_t* ctx) {
  if (!decl || decl->kind != TICK_AST_DECL) return TICK_ANALYZE_OK;

  tick_ast_node_t* func = decl->decl.init;
  if (!func || func->kind != TICK_AST_FUNCTION) return TICK_ANALYZE_OK;

  // Analyze signature first
  tick_analyze_error_t err = analyze_function_signature(decl, ctx);
  if (err != TICK_ANALYZE_OK) {
    return err;
  }

  // If signature not completed yet (has dependencies), defer body
  if (decl->decl.signature_state != TICK_ANALYSIS_STATE_COMPLETED) {
    return TICK_ANALYZE_OK;
  }

  // Signature complete, analyze body
  return analyze_function_body(decl, ctx);
}

// Helper to create and collect forward declaration for struct/union
static void collect_forward_decl(tick_ast_node_t* decl,
                                 tick_analyze_ctx_t* ctx) {
  if (!decl || decl->kind != TICK_AST_DECL) return;

  // Manually allocate AST node (can't use ALLOC_AST_NODE macro in void
  // function)
  tick_allocator_config_t config = {
      .flags = TICK_ALLOCATOR_ZEROMEM,
      .alignment2 = 0,
  };
  tick_buf_t buf = {0};
  if (ctx->alloc.realloc(ctx->alloc.ctx, &buf, sizeof(tick_ast_node_t),
                         &config) != TICK_OK) {
    return;  // Silently fail on allocation error
  }
  tick_ast_node_t* fwd_decl = (tick_ast_node_t*)buf.buf;

  // Copy the declaration but mark as forward
  *fwd_decl = *decl;
  fwd_decl->decl.quals.is_forward_decl = true;
  fwd_decl->node_flags = TICK_NODE_FLAG_SYNTHETIC | TICK_NODE_FLAG_ANALYZED;
  fwd_decl->next = NULL;
  fwd_decl->prev = NULL;

  // Add to forward declarations list (manually, using next/prev pointers)
  if (!ctx->forward_decls.head) {
    ctx->forward_decls.head = fwd_decl;
    ctx->forward_decls.tail = fwd_decl;
  } else {
    ctx->forward_decls.tail->next = fwd_decl;
    fwd_decl->prev = ctx->forward_decls.tail;
    ctx->forward_decls.tail = fwd_decl;
  }
}

// Analyze struct declaration - resolve field types
static tick_analyze_error_t analyze_struct_decl(tick_ast_node_t* decl,
                                                tick_analyze_ctx_t* ctx) {
  if (!decl || decl->kind != TICK_AST_DECL) return TICK_ANALYZE_OK;

  tick_ast_node_t* struct_node = decl->decl.init;
  if (!struct_node || struct_node->kind != TICK_AST_STRUCT_DECL)
    return TICK_ANALYZE_OK;

  ALOG_ENTER("struct %.*s", (int)decl->decl.name.sz, decl->decl.name.buf);

  // Analyze field types
  tick_analyze_error_t err =
      analyze_field_types(struct_node->struct_decl.fields, ctx);
  if (err != TICK_ANALYZE_OK) {
    ALOG_EXIT("FAILED");
    return err;
  }

  // Collect forward declaration for this struct
  collect_forward_decl(decl, ctx);

  ALOG_EXIT("struct complete");
  return TICK_ANALYZE_OK;
}

// Helper to count union fields
static u32 count_union_fields(tick_ast_node_t* fields) {
  u32 count = 0;
  for (tick_ast_node_t* field = fields; field; field = field->next) {
    if (field->kind == TICK_AST_FIELD) {
      count++;
    }
  }
  return count;
}

// Helper to determine optimal tag type based on field count
static tick_builtin_type_t optimal_tag_type(u32 field_count) {
  if (field_count <= 255) return TICK_TYPE_U8;
  if (field_count <= 65535) return TICK_TYPE_U16;
  return TICK_TYPE_U32;
}

// Helper to create an enum value node
static tick_ast_node_t* alloc_enum_value_node(tick_alloc_t alloc,
                                              tick_buf_t name,
                                              tick_ast_node_t* value) {
  tick_ast_node_t* node;
  ALLOC_AST_NODE(alloc, node);
  node->kind = TICK_AST_ENUM_VALUE;
  node->enum_value.name = name;
  node->enum_value.value = value;
  node->loc.line = 0;  // Synthesized node
  node->loc.col = 0;
  return node;
}

// Generate tag enum for auto-tagged union
// Returns the generated enum DECL node, or NULL on error
// union_owner_decl: the DECL node containing the union (for inheriting quals)
static tick_ast_node_t* generate_union_tag_enum(
    tick_ast_node_t* union_decl, tick_ast_node_t* union_fields,
    tick_buf_t union_name, tick_ast_node_t* union_owner_decl,
    tick_analyze_ctx_t* ctx) {
  // Count fields to determine optimal tag type
  u32 field_count = count_union_fields(union_fields);
  if (field_count == 0) {
    ANALYSIS_ERROR(ctx, union_decl->loc, "union '%.*s' has no fields",
                   (int)union_name.sz, union_name.buf);
    return NULL;
  }

  tick_builtin_type_t tag_builtin = optimal_tag_type(field_count);
  ALOG("generating tag enum: %u fields -> %s", field_count,
       tick_builtin_type_str(tag_builtin));

  // Allocate enum name buffer: "UnionName_Tag"
  const char* tag_suffix = "_Tag";
  usz tag_suffix_len = strlen(tag_suffix);
  usz enum_name_len = union_name.sz + tag_suffix_len;

  tick_allocator_config_t config = {.flags = 0, .alignment2 = 0};
  tick_buf_t enum_name_buf = {0};
  if (ctx->alloc.realloc(ctx->alloc.ctx, &enum_name_buf, enum_name_len,
                         &config) != TICK_OK) {
    return NULL;
  }
  memcpy(enum_name_buf.buf, union_name.buf, union_name.sz);
  memcpy(enum_name_buf.buf + union_name.sz, tag_suffix, tag_suffix_len);
  tick_buf_t enum_name = {.buf = enum_name_buf.buf, .sz = enum_name_len};

  // Create enum values from union fields
  tick_ast_node_t* enum_values = NULL;
  tick_ast_node_t* last_value = NULL;
  int64_t value_index = 0;

  for (tick_ast_node_t* field = union_fields; field; field = field->next) {
    if (field->kind != TICK_AST_FIELD) continue;

    // Allocate enum value name buffer: "fieldname_tag"
    const char* value_suffix = "_tag";
    usz value_suffix_len = strlen(value_suffix);
    usz value_name_len = field->field.name.sz + value_suffix_len;

    tick_buf_t value_name_buf = {0};
    if (ctx->alloc.realloc(ctx->alloc.ctx, &value_name_buf, value_name_len,
                           &config) != TICK_OK) {
      return NULL;
    }
    memcpy(value_name_buf.buf, field->field.name.buf, field->field.name.sz);
    memcpy(value_name_buf.buf + field->field.name.sz, value_suffix,
           value_suffix_len);
    tick_buf_t value_name = {.buf = value_name_buf.buf, .sz = value_name_len};

    // Create literal value for enum
    tick_ast_node_t* literal = alloc_literal_node(ctx->alloc, value_index++);
    if (!literal) return NULL;

    // Create enum value node
    tick_ast_node_t* enum_value =
        alloc_enum_value_node(ctx->alloc, value_name, literal);
    if (!enum_value) return NULL;

    // Append to linked list
    if (!enum_values) {
      enum_values = enum_value;
      last_value = enum_value;
    } else {
      last_value->next = enum_value;
      enum_value->prev = last_value;
      last_value = enum_value;
    }
  }

  // Create enum declaration node
  tick_ast_node_t* enum_node;
  ALLOC_AST_NODE(ctx->alloc, enum_node);
  enum_node->kind = TICK_AST_ENUM_DECL;
  enum_node->enum_decl.underlying_type =
      alloc_type_node(ctx->alloc, tag_builtin);
  enum_node->enum_decl.values = enum_values;
  enum_node->loc = union_decl->loc;

  // Create wrapper DECL node (inherits pub status from union)
  tick_ast_node_t* enum_decl;
  ALLOC_AST_NODE(ctx->alloc, enum_decl);
  enum_decl->kind = TICK_AST_DECL;
  enum_decl->decl.name = enum_name;
  enum_decl->decl.init = enum_node;
  enum_decl->decl.type = NULL;  // Will be set by ensure_user_type_set
  enum_decl->decl.tmpid = 0;
  enum_decl->decl.quals = (tick_qualifier_flags_t){0};
  // Inherit pub qualifier from union so tag enum is emitted to header if needed
  if (union_owner_decl && union_owner_decl->decl.quals.is_pub) {
    enum_decl->decl.quals.is_pub = true;
  }
  enum_decl->decl.analysis_state = TICK_ANALYSIS_STATE_NOT_STARTED;
  enum_decl->loc = union_decl->loc;

  ALOG("created tag enum: %.*s", (int)enum_name.sz, enum_name.buf);
  return enum_decl;
}

// Validate that explicit tag type matches union fields
static tick_analyze_error_t validate_union_tag_type(
    tick_ast_node_t* union_decl, tick_ast_node_t* tag_type,
    tick_ast_node_t* union_fields, tick_buf_t union_name,
    tick_analyze_ctx_t* ctx) {
  // Tag type must be a user-defined enum
  if (tag_type->kind != TICK_AST_TYPE_NAMED) {
    ANALYSIS_ERROR(ctx, union_decl->loc,
                   "union '%.*s' tag type must be a named type",
                   (int)union_name.sz, union_name.buf);
    return TICK_ANALYZE_ERR;
  }

  if (tag_type->type_named.builtin_type != TICK_TYPE_USER_DEFINED) {
    ANALYSIS_ERROR(ctx, union_decl->loc,
                   "union '%.*s' tag type must be an enum (got %s)",
                   (int)union_name.sz, union_name.buf,
                   tick_builtin_type_str(tag_type->type_named.builtin_type));
    return TICK_ANALYZE_ERR;
  }

  // Get the enum declaration from the type entry
  tick_type_entry_t* type_entry = tag_type->type_named.type_entry;
  if (!type_entry || !type_entry->parent_decl) {
    ANALYSIS_ERROR(ctx, union_decl->loc,
                   "union '%.*s' tag type not found in type table",
                   (int)union_name.sz, union_name.buf);
    return TICK_ANALYZE_ERR;
  }

  tick_ast_node_t* tag_decl = type_entry->parent_decl;
  if (tag_decl->kind != TICK_AST_DECL || !tag_decl->decl.init ||
      tag_decl->decl.init->kind != TICK_AST_ENUM_DECL) {
    ANALYSIS_ERROR(ctx, union_decl->loc, "union '%.*s' tag type is not an enum",
                   (int)union_name.sz, union_name.buf);
    return TICK_ANALYZE_ERR;
  }

  // Check that each union field has a corresponding enum value
  tick_ast_node_t* enum_node = tag_decl->decl.init;
  for (tick_ast_node_t* field = union_fields; field; field = field->next) {
    if (field->kind != TICK_AST_FIELD) continue;

    // Look for matching enum value
    bool found = false;
    for (tick_ast_node_t* enum_val = enum_node->enum_decl.values; enum_val;
         enum_val = enum_val->next) {
      if (enum_val->kind != TICK_AST_ENUM_VALUE) continue;

      // Check if enum value name matches field name
      if (enum_val->enum_value.name.sz == field->field.name.sz &&
          memcmp(enum_val->enum_value.name.buf, field->field.name.buf,
                 field->field.name.sz) == 0) {
        found = true;
        break;
      }
    }

    if (!found) {
      ANALYSIS_ERROR(ctx, field->loc,
                     "union '%.*s' field '%.*s' has no corresponding value in "
                     "tag enum '%.*s'",
                     (int)union_name.sz, union_name.buf,
                     (int)field->field.name.sz, field->field.name.buf,
                     (int)tag_decl->decl.name.sz, tag_decl->decl.name.buf);
      return TICK_ANALYZE_ERR;
    }
  }

  return TICK_ANALYZE_OK;
}

// Analyze union declaration - resolve field types and handle tag generation
static tick_analyze_error_t analyze_union_decl(tick_ast_node_t* decl,
                                               tick_analyze_ctx_t* ctx) {
  if (!decl || decl->kind != TICK_AST_DECL) return TICK_ANALYZE_OK;

  tick_ast_node_t* union_node = decl->decl.init;
  if (!union_node || union_node->kind != TICK_AST_UNION_DECL)
    return TICK_ANALYZE_OK;

  ALOG_ENTER("union %.*s", (int)decl->decl.name.sz, decl->decl.name.buf);

  // Handle auto-tagged union: generate tag enum
  if (!union_node->union_decl.tag_type) {
    ALOG("auto-tagged union, generating tag enum");

    // Generate the tag enum declaration
    tick_ast_node_t* tag_enum_decl = generate_union_tag_enum(
        union_node, union_node->union_decl.fields, decl->decl.name, decl, ctx);
    if (!tag_enum_decl) {
      ALOG_EXIT("FAILED: tag enum generation failed");
      return TICK_ANALYZE_ERR;
    }

    // Set up the enum's user-defined type
    tick_analyze_error_t err = ensure_user_type_set(tag_enum_decl, ctx);
    if (err != TICK_ANALYZE_OK) {
      ALOG_EXIT("FAILED: ensure_user_type_set failed");
      return err;
    }

    // Register the enum in the type table
    // Tag enum inherits pub status from parent union
    tick_err_t reg_err =
        tick_types_insert(ctx->types, tag_enum_decl->decl.name,
                          tag_enum_decl->decl.init, TICK_TYPE_USER_DEFINED,
                          tag_enum_decl, decl->decl.quals.is_pub, ctx->alloc);
    if (reg_err != TICK_OK) {
      ALOG_EXIT("FAILED: type table insertion failed");
      return TICK_ANALYZE_ERR;
    }

    // Register the enum in module scope
    tick_scope_insert_symbol(ctx->module_scope, tag_enum_decl->decl.name,
                             tag_enum_decl);

    // Insert the enum declaration before the union in module.decls
    CHECK(ctx->module && ctx->module->kind == TICK_AST_MODULE,
          "ctx->module must be MODULE node");

    // Find the union declaration in the module's decl list
    for (tick_ast_node_t* d = ctx->module->module.decls; d; d = d->next) {
      if (d == decl) {
        // Found the union decl, insert enum before it
        tag_enum_decl->next = d;
        tag_enum_decl->prev = d->prev;
        if (d->prev) {
          d->prev->next = tag_enum_decl;
        } else {
          // Union is first in list, update module head
          ctx->module->module.decls = tag_enum_decl;
        }
        d->prev = tag_enum_decl;
        break;
      }
    }

    // Set the union's tag_type to reference the generated enum
    union_node->union_decl.tag_type = tag_enum_decl->decl.type;

    // Mark enum as completed (it's fully analyzed)
    tag_enum_decl->decl.analysis_state = TICK_ANALYSIS_STATE_COMPLETED;

    ALOG("tag enum inserted and registered");
  }

  // Analyze tag type (whether explicit or generated)
  if (union_node->union_decl.tag_type) {
    tick_analyze_error_t err =
        analyze_type(union_node->union_decl.tag_type, ctx);
    if (err != TICK_ANALYZE_OK) {
      ALOG_EXIT("FAILED");
      return err;
    }

    // For explicit tags, validate that fields match enum values
    // We can detect explicit tags by checking if the tag_type's name doesn't
    // end with "_Tag"
    tick_ast_node_t* tag_type = union_node->union_decl.tag_type;
    if (tag_type->kind == TICK_AST_TYPE_NAMED &&
        tag_type->type_named.type_entry) {
      tick_type_entry_t* entry = tag_type->type_named.type_entry;
      if (entry->parent_decl) {
        tick_buf_t tag_name = entry->parent_decl->decl.name;

        // Check if this is NOT a generated tag (doesn't end with "_Tag")
        bool is_generated = false;
        const char* tag_suffix = "_Tag";
        usz suffix_len = strlen(tag_suffix);
        if (tag_name.sz >= suffix_len + decl->decl.name.sz) {
          // Check if it ends with "_Tag" and starts with union name
          if (memcmp(tag_name.buf + tag_name.sz - suffix_len, tag_suffix,
                     suffix_len) == 0 &&
              memcmp(tag_name.buf, decl->decl.name.buf, decl->decl.name.sz) ==
                  0) {
            is_generated = true;
          }
        }

        if (!is_generated) {
          ALOG("explicit tag type, validating");
          err = validate_union_tag_type(union_node, tag_type,
                                        union_node->union_decl.fields,
                                        decl->decl.name, ctx);
          if (err != TICK_ANALYZE_OK) {
            ALOG_EXIT("FAILED: tag validation failed");
            return err;
          }
        }
      }
    }
  }

  // Analyze field types
  tick_analyze_error_t err =
      analyze_field_types(union_node->union_decl.fields, ctx);
  if (err != TICK_ANALYZE_OK) {
    ALOG_EXIT("FAILED");
    return err;
  }

  // Collect forward declaration for this union
  collect_forward_decl(decl, ctx);

  ALOG_EXIT("union complete");
  return TICK_ANALYZE_OK;
}

// Analyze a declaration without an initializer (must be extern)
static tick_analyze_error_t analyze_decl_without_init(tick_ast_node_t* decl,
                                                      tick_analyze_ctx_t* ctx) {
  CHECK(decl && decl->kind == TICK_AST_DECL, "must be a DECL");
  CHECK(!decl->decl.init, "must not have init");
  CHECK(decl->decl.type, "decl without init must have type, parser-enforced");

  if (!decl->decl.quals.is_extern) {
    ANALYSIS_ERROR(
        ctx, decl->loc,
        "declaration '%.*s' must either have an initializer or be extern\n",
        (int)decl->decl.name.sz, decl->decl.name.buf);
    return TICK_ANALYZE_ERR;
  }
  return analyze_type(decl->decl.type, ctx);
}

// Analyze a declaration with an initializer
//
// Function Type Rules:
// ====================
// Bare TYPE_FUNCTION (not wrapped in TYPE_POINTER) is only valid in two cases:
//
// 1. Function implementation:
//    let f = fn() i32 { ... }
//    - decl.type is inferred or explicitly TYPE_FUNCTION
//    - decl.init is FUNCTION node
//    - Emits as: int32_t __u_f(...) { ... }
//
// 2. Extern function declaration (no init, handled in
// analyze_decl_without_init):
//    extern let f: fn() i32;
//    - decl.type is TYPE_FUNCTION
//    - decl.init is NULL
//    - decl.quals.is_extern is true
//    - Emits as: extern int32_t f(...);
//
// For function pointer variables, use TYPE_POINTER(TYPE_FUNCTION):
//    let fp: *fn() i32;
//    - decl.type is TYPE_POINTER(TYPE_FUNCTION)
//    - Emits as: int32_t (*__u_fp)(...);
//
// Any other use of bare TYPE_FUNCTION is an error.
static tick_analyze_error_t analyze_decl_with_init(tick_ast_node_t* decl,
                                                   tick_analyze_ctx_t* ctx) {
  CHECK(decl && decl->kind == TICK_AST_DECL && decl->decl.init,
        "must be a DECL with init");

  tick_ast_node_t* init = decl->decl.init;
  switch (init->kind) {
    case TICK_AST_FUNCTION:
      if (decl->decl.type == NULL) {
        decl->decl.type = alloc_function_type_node(
            ctx->alloc, init->function.params, init->function.return_type);
        tick_analyze_error_t err = analyze_type(decl->decl.type, ctx);
        if (err != TICK_ANALYZE_OK) return err;
      }
      return analyze_function(decl, ctx);

    case TICK_AST_ENUM_DECL: {
      // Set decl type to the enum type
      tick_analyze_error_t err = ensure_user_type_set(decl, ctx);
      if (err != TICK_ANALYZE_OK) return err;
      return analyze_enum_decl(decl, ctx);
    }

    case TICK_AST_STRUCT_DECL: {
      // Set decl type to the struct type
      tick_analyze_error_t err = ensure_user_type_set(decl, ctx);
      if (err != TICK_ANALYZE_OK) return err;
      return analyze_struct_decl(decl, ctx);
    }

    case TICK_AST_UNION_DECL: {
      // Set decl type to the union type
      tick_analyze_error_t err = ensure_user_type_set(decl, ctx);
      if (err != TICK_ANALYZE_OK) return err;
      return analyze_union_decl(decl, ctx);
    }

    default:
      // Regular variable declaration with initializer (expression)
      ALOG_ENTER("var %.*s", (int)decl->decl.name.sz, decl->decl.name.buf);

      // Validate function type usage: bare TYPE_FUNCTION only valid for
      // function implementations (handled above) or extern declarations
      // (no init). If we're here with TYPE_FUNCTION, it's an error.
      if (decl->decl.type && decl->decl.type->kind == TICK_AST_TYPE_FUNCTION) {
        ANALYSIS_ERROR(
            ctx, decl->loc,
            "variable '%.*s' cannot have function type; use function pointer "
            "type '*fn(...)' instead",
            (int)decl->decl.name.sz, decl->decl.name.buf);
        return TICK_ANALYZE_ERR;
      }

      // Check if this is a module-level declaration with complex initializer
      if (ctx->scope_depth == 0 && init && !is_initializer_simple(init)) {
        ANALYSIS_ERROR(ctx, decl->loc,
                       "module-level variable '%.*s' cannot have complex "
                       "initializer expression",
                       (int)decl->decl.name.sz, decl->decl.name.buf);
        return TICK_ANALYZE_ERR;
      }

      // If type is NULL, infer from initializer
      if (decl->decl.type == NULL) {
        tick_ast_node_t* inferred_type = analyze_expr(init, ctx);
        // Check if analyze_expr failed
        if (analyze_has_error(ctx)) {
          ALOG_EXIT("FAILED");
          return TICK_ANALYZE_ERR;
        }
        decl->decl.type = inferred_type;
        ALOG("inferred: %s", tick_type_str(inferred_type));
      } else {
        tick_analyze_error_t err = analyze_type(decl->decl.type, ctx);
        if (err != TICK_ANALYZE_OK) {
          ALOG_EXIT("FAILED");
          return err;
        }
        analyze_expr(init, ctx);
        // Check if analyze_expr failed
        if (analyze_has_error(ctx)) {
          ALOG_EXIT("FAILED");
          return TICK_ANALYZE_ERR;
        }
      }

      // Transform AST to normalize for codegen
      transform_static_string_decl(decl, ctx->alloc);
      remove_undefined_init(decl);

      ALOG_EXIT("%.*s: %s = %s", (int)decl->decl.name.sz, decl->decl.name.buf,
                tick_type_str(decl->decl.type),
                init && init->kind == TICK_AST_LITERAL ? "literal" : "expr");
      return TICK_ANALYZE_OK;
  }
}

static tick_analyze_error_t analyze_decl_lazy(tick_ast_node_t* decl,
                                              tick_analyze_ctx_t* ctx) {
  CHECK(decl && decl->kind == TICK_AST_DECL, "must be a DECL");

  if (decl->decl.init) {
    return analyze_decl_with_init(decl, ctx);
  } else {
    return analyze_decl_without_init(decl, ctx);
  }
}

tick_err_t tick_ast_analyze(tick_ast_t* ast, tick_alloc_t alloc,
                            tick_buf_t errbuf, tick_buf_t source) {
  CHECK(ast && ast->root && ast->root->kind == TICK_AST_MODULE,
        "analyze must be given a module");

  tick_analyze_ctx_t ctx_val;
  tick_analyze_ctx_t* ctx = &ctx_val;
  tick_analyze_ctx_init(ctx, alloc, errbuf, source);

  // Set module pointer in context for access during analysis
  ctx->module = ast->root;

  ALOG_SECTION("ANALYSIS PASS");

  // PASS 1: Register module-level names and initialize analysis state
  ALOG_SECTION("PASS 1: Register Declarations");
  ctx->log_depth = 0;

  tick_ast_node_t* module = ast->root;
  for (tick_ast_node_t* decl = module->module.decls; decl; decl = decl->next) {
    CHECK(decl->kind == TICK_AST_DECL, "module decls must be DECL type");

    // Initialize analysis state
    decl->decl.analysis_state = TICK_ANALYSIS_STATE_NOT_STARTED;
    decl->decl.signature_state = TICK_ANALYSIS_STATE_NOT_STARTED;
    decl->decl.body_state = TICK_ANALYSIS_STATE_NOT_STARTED;

    // Register type declarations in type table
    if (decl->decl.init) {
      tick_ast_node_t* init = decl->decl.init;
      if (init->kind == TICK_AST_STRUCT_DECL ||
          init->kind == TICK_AST_ENUM_DECL ||
          init->kind == TICK_AST_UNION_DECL) {
        ALOG("register type: %.*s", (int)decl->decl.name.sz,
             decl->decl.name.buf);
        tick_err_t err = tick_types_insert(ctx->types, decl->decl.name, init,
                                           TICK_TYPE_USER_DEFINED, decl,
                                           decl->decl.quals.is_pub, alloc);
        if (err != TICK_OK) {
          tick_analyze_ctx_destroy(ctx);
          return TICK_ERR;
        }
      }
    }

    // Register ALL declarations in module scope (functions, variables, types)
    tick_scope_insert_symbol(ctx->current_scope, decl->decl.name, decl);

    // Enqueue pub declarations for analysis
    if (decl->decl.quals.is_pub) {
      ALOG("enqueue pub: %.*s", (int)decl->decl.name.sz, decl->decl.name.buf);
      tick_work_queue_enqueue(&ctx->work_queue, decl);
    }
  }

  // ============================================================================
  // PASS 2: Queue-driven lazy analysis
  // ============================================================================
  ALOG_SECTION("PASS 2: Lazy Analysis");
  ctx->log_depth = 0;

  tick_err_t res = TICK_OK;
  while (!tick_work_queue_empty(&ctx->work_queue)) {
    tick_ast_node_t* decl = tick_work_queue_dequeue(&ctx->work_queue);
    CHECK(decl, "NULL decl");

    if (decl->decl.analysis_state == TICK_ANALYSIS_STATE_COMPLETED ||
        decl->decl.analysis_state == TICK_ANALYSIS_STATE_FAILED) {
      continue;
    }
    if (decl->decl.analysis_state == TICK_ANALYSIS_STATE_IN_PROGRESS) {
      ANALYSIS_ERROR(ctx, decl->loc,
                     "circular dependency detected for declaration '%.*s'\n",
                     (int)decl->decl.name.sz, decl->decl.name.buf);
      goto fail;
    }

    ALOG_ENTER("decl %.*s", (int)decl->decl.name.sz, decl->decl.name.buf);
    decl->decl.analysis_state = TICK_ANALYSIS_STATE_IN_PROGRESS;
    tick_dependency_list_clear(&ctx->pending_deps);
    tick_analyze_error_t result = analyze_decl_lazy(decl, ctx);
    if (result != TICK_ANALYZE_OK) {
      decl->decl.analysis_state = TICK_ANALYSIS_STATE_FAILED;
      ALOG_EXIT("FAILED: %s", tick_analyze_error_str(result));
      goto fail;
    }

    if (ctx->pending_deps.head != NULL) {
      ALOG_EXIT("has dependencies, re-enqueueing");
      decl->decl.analysis_state = TICK_ANALYSIS_STATE_NOT_STARTED;

      if (!ctx->work_queue.head) {
        ctx->work_queue.head = ctx->pending_deps.head;
        ctx->work_queue.tail = ctx->pending_deps.tail;
      } else {
        ctx->work_queue.tail->decl.next_queued = ctx->pending_deps.head;
        ctx->work_queue.tail = ctx->pending_deps.tail;
      }

      // Clear in_pending_deps flags
      for (tick_ast_node_t* dep = ctx->pending_deps.head; dep;
           dep = dep->decl.next_queued) {
        dep->decl.in_pending_deps = false;
      }
      ctx->pending_deps.head = NULL;
      ctx->pending_deps.tail = NULL;

      tick_work_queue_enqueue(&ctx->work_queue, decl);
    } else {
      ALOG_EXIT("OK");
      decl->decl.analysis_state = TICK_ANALYSIS_STATE_COMPLETED;
    }
  }

  // PASS 3: Prepend forward declarations to module.decls
  if (ctx->forward_decls.head) {
    ALOG("Prepending %s forward declarations",
         ctx->forward_decls.tail == ctx->forward_decls.head ? "1" : "multiple");

    // Link forward_decls.tail to the current head of module.decls
    ctx->forward_decls.tail->next = module->module.decls;
    if (module->module.decls) {
      module->module.decls->prev = ctx->forward_decls.tail;
    }

    // Update module.decls to point to forward_decls.head
    module->module.decls = ctx->forward_decls.head;
  }

  goto cleanup;
fail:
  res = TICK_ERR;
cleanup:
  tick_analyze_ctx_destroy(ctx);
  ALOG_SECTION("ANALYSIS %s", res == TICK_OK ? "SUCCESS" : "FAILED");
  return res;
}
