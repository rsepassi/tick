// C Code Generation - Helper Functions
//
// Type checking, cast logic, and operator mapping utilities for code
// generation.

#ifndef TICK_CODEGEN_HELPERS_H
#define TICK_CODEGEN_HELPERS_H

#include "tick.h"

// Check if builtin type is numeric (supports checked operations)
bool tick_type_is_numeric_builtin(tick_builtin_type_t type);

// Check if a cast from src to dst is a widening conversion (always safe)
bool tick_codegen_is_widening_cast(tick_builtin_type_t src,
                                   tick_builtin_type_t dst);

// Compute the cast strategy for a given source and destination type
tick_cast_strategy_t tick_codegen_compute_cast_strategy(
    tick_builtin_type_t src, tick_builtin_type_t dst);

// Map binary operator to C operator string
const char* tick_codegen_binop_to_c(tick_binop_t op);

// Map unary operator to C operator string
const char* tick_codegen_unop_to_c(tick_unop_t op);

// Check if a builtin needs its first argument (format string) cast to const
// char*
bool tick_codegen_builtin_needs_format_cast(tick_at_builtin_t builtin);

#endif  // TICK_CODEGEN_HELPERS_H
