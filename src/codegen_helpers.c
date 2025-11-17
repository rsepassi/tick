// C Code Generation - Helper Functions
//
// Type checking, cast logic, and operator mapping utilities for code
// generation.

#include "codegen_helpers.h"

#include "tick.h"

// ============================================================================
// Type Helpers
// ============================================================================

// Check if builtin type is numeric (supports checked operations)
bool tick_type_is_numeric_builtin(tick_builtin_type_t type) {
  return (type >= TICK_TYPE_I8 && type <= TICK_TYPE_USZ);
}

// Check if a cast from src to dst is a widening conversion (always safe)
bool tick_codegen_is_widening_cast(tick_builtin_type_t src,
                                   tick_builtin_type_t dst) {
  if (src == dst) return true;

  // Signed to signed widening
  if (src == TICK_TYPE_I8 && (dst == TICK_TYPE_I16 || dst == TICK_TYPE_I32 ||
                              dst == TICK_TYPE_I64 || dst == TICK_TYPE_ISZ))
    return true;
  if (src == TICK_TYPE_I16 &&
      (dst == TICK_TYPE_I32 || dst == TICK_TYPE_I64 || dst == TICK_TYPE_ISZ))
    return true;
  if (src == TICK_TYPE_I32 && (dst == TICK_TYPE_I64 || dst == TICK_TYPE_ISZ))
    return true;

  // Unsigned to unsigned widening
  if (src == TICK_TYPE_U8 && (dst == TICK_TYPE_U16 || dst == TICK_TYPE_U32 ||
                              dst == TICK_TYPE_U64 || dst == TICK_TYPE_USZ))
    return true;
  if (src == TICK_TYPE_U16 &&
      (dst == TICK_TYPE_U32 || dst == TICK_TYPE_U64 || dst == TICK_TYPE_USZ))
    return true;
  if (src == TICK_TYPE_U32 && (dst == TICK_TYPE_U64 || dst == TICK_TYPE_USZ))
    return true;

  // Unsigned to signed widening (safe if destination is larger)
  if (src == TICK_TYPE_U8 && (dst == TICK_TYPE_I16 || dst == TICK_TYPE_I32 ||
                              dst == TICK_TYPE_I64 || dst == TICK_TYPE_ISZ))
    return true;
  if (src == TICK_TYPE_U16 &&
      (dst == TICK_TYPE_I32 || dst == TICK_TYPE_I64 || dst == TICK_TYPE_ISZ))
    return true;
  if (src == TICK_TYPE_U32 && (dst == TICK_TYPE_I64 || dst == TICK_TYPE_ISZ))
    return true;

  return false;
}

// Compute the cast strategy for a given source and destination type
tick_cast_strategy_t tick_codegen_compute_cast_strategy(
    tick_builtin_type_t src, tick_builtin_type_t dst) {
  // Non-numeric types use bare C cast
  bool src_numeric = tick_type_is_numeric_builtin(src);
  bool dst_numeric = tick_type_is_numeric_builtin(dst);
  if (!src_numeric || !dst_numeric) {
    return CAST_STRATEGY_BARE;
  }

  // Widening casts are safe - use bare C cast
  if (tick_codegen_is_widening_cast(src, dst)) {
    return CAST_STRATEGY_BARE;
  }

  // Narrowing or sign-changing casts need checked cast
  return CAST_STRATEGY_CHECKED;
}

// ============================================================================
// Operator Mapping
// ============================================================================

const char* tick_codegen_binop_to_c(tick_binop_t op) {
  // Map operators to C operators
  // Note: Arithmetic/shift operations have runtime_func set and won't reach
  // here
  switch (op) {
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
    case BINOP_LOGICAL_AND:
      return "&&";
    case BINOP_LOGICAL_OR:
      return "||";
    case BINOP_AND:
      return "&";
    case BINOP_OR:
      return "|";
    case BINOP_XOR:
      return "^";
    default:
      return "?";
  }
}

const char* tick_codegen_unop_to_c(tick_unop_t op) {
  // Map operators to C operators
  // Note: Signed negation has runtime_func set and won't reach here
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
      return "?";
  }
}

// ============================================================================
// Builtin Helpers
// ============================================================================

// Check if a builtin needs its first argument (format string) cast to const
// char*
bool tick_codegen_builtin_needs_format_cast(tick_at_builtin_t builtin) {
  switch (builtin) {
    case TICK_AT_BUILTIN_DBG:
    case TICK_AT_BUILTIN_PANIC:
      return true;
    default:
      return false;
  }
}
