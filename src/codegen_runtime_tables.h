// C Code Generation - Runtime Function Lookup Tables
//
// These tables map semantic operations (builtin enums) and types to C runtime
// function names. This is C11 backend-specific logic - other backends would
// have different implementations.
//
// NOTE: This header should only be included after tick.h

#ifndef TICK_CODEGEN_RUNTIME_TABLES_H
#define TICK_CODEGEN_RUNTIME_TABLES_H

#define NUM_BUILTINS 17
#define NUM_TYPES 14

// Runtime function names indexed by [builtin][type]
// Returns function name string or NULL if not applicable
static const char* RUNTIME_FUNCS[NUM_BUILTINS][NUM_TYPES] = {
    // TICK_BUILTIN_SAT_ADD
    [TICK_BUILTIN_SAT_ADD] =
        {
            [TICK_TYPE_I8] = "tick_sat_add_i8",
            [TICK_TYPE_I16] = "tick_sat_add_i16",
            [TICK_TYPE_I32] = "tick_sat_add_i32",
            [TICK_TYPE_I64] = "tick_sat_add_i64",
            [TICK_TYPE_ISZ] = "tick_sat_add_isz",
            [TICK_TYPE_U8] = "tick_sat_add_u8",
            [TICK_TYPE_U16] = "tick_sat_add_u16",
            [TICK_TYPE_U32] = "tick_sat_add_u32",
            [TICK_TYPE_U64] = "tick_sat_add_u64",
            [TICK_TYPE_USZ] = "tick_sat_add_usz",
        },
    // TICK_BUILTIN_SAT_SUB
    [TICK_BUILTIN_SAT_SUB] =
        {
            [TICK_TYPE_I8] = "tick_sat_sub_i8",
            [TICK_TYPE_I16] = "tick_sat_sub_i16",
            [TICK_TYPE_I32] = "tick_sat_sub_i32",
            [TICK_TYPE_I64] = "tick_sat_sub_i64",
            [TICK_TYPE_ISZ] = "tick_sat_sub_isz",
            [TICK_TYPE_U8] = "tick_sat_sub_u8",
            [TICK_TYPE_U16] = "tick_sat_sub_u16",
            [TICK_TYPE_U32] = "tick_sat_sub_u32",
            [TICK_TYPE_U64] = "tick_sat_sub_u64",
            [TICK_TYPE_USZ] = "tick_sat_sub_usz",
        },
    // TICK_BUILTIN_SAT_MUL
    [TICK_BUILTIN_SAT_MUL] =
        {
            [TICK_TYPE_I8] = "tick_sat_mul_i8",
            [TICK_TYPE_I16] = "tick_sat_mul_i16",
            [TICK_TYPE_I32] = "tick_sat_mul_i32",
            [TICK_TYPE_I64] = "tick_sat_mul_i64",
            [TICK_TYPE_ISZ] = "tick_sat_mul_isz",
            [TICK_TYPE_U8] = "tick_sat_mul_u8",
            [TICK_TYPE_U16] = "tick_sat_mul_u16",
            [TICK_TYPE_U32] = "tick_sat_mul_u32",
            [TICK_TYPE_U64] = "tick_sat_mul_u64",
            [TICK_TYPE_USZ] = "tick_sat_mul_usz",
        },
    // TICK_BUILTIN_SAT_DIV
    [TICK_BUILTIN_SAT_DIV] =
        {
            [TICK_TYPE_I8] = "tick_sat_div_i8",
            [TICK_TYPE_I16] = "tick_sat_div_i16",
            [TICK_TYPE_I32] = "tick_sat_div_i32",
            [TICK_TYPE_I64] = "tick_sat_div_i64",
            [TICK_TYPE_ISZ] = "tick_sat_div_isz",
            [TICK_TYPE_U8] = "tick_sat_div_u8",
            [TICK_TYPE_U16] = "tick_sat_div_u16",
            [TICK_TYPE_U32] = "tick_sat_div_u32",
            [TICK_TYPE_U64] = "tick_sat_div_u64",
            [TICK_TYPE_USZ] = "tick_sat_div_usz",
        },
    // TICK_BUILTIN_WRAP_ADD
    [TICK_BUILTIN_WRAP_ADD] =
        {
            [TICK_TYPE_I8] = "tick_wrap_add_i8",
            [TICK_TYPE_I16] = "tick_wrap_add_i16",
            [TICK_TYPE_I32] = "tick_wrap_add_i32",
            [TICK_TYPE_I64] = "tick_wrap_add_i64",
            [TICK_TYPE_ISZ] = "tick_wrap_add_isz",
            // Unsigned types use native C wrapping - no runtime function needed
        },
    // TICK_BUILTIN_WRAP_SUB
    [TICK_BUILTIN_WRAP_SUB] =
        {
            [TICK_TYPE_I8] = "tick_wrap_sub_i8",
            [TICK_TYPE_I16] = "tick_wrap_sub_i16",
            [TICK_TYPE_I32] = "tick_wrap_sub_i32",
            [TICK_TYPE_I64] = "tick_wrap_sub_i64",
            [TICK_TYPE_ISZ] = "tick_wrap_sub_isz",
        },
    // TICK_BUILTIN_WRAP_MUL
    [TICK_BUILTIN_WRAP_MUL] =
        {
            [TICK_TYPE_I8] = "tick_wrap_mul_i8",
            [TICK_TYPE_I16] = "tick_wrap_mul_i16",
            [TICK_TYPE_I32] = "tick_wrap_mul_i32",
            [TICK_TYPE_I64] = "tick_wrap_mul_i64",
            [TICK_TYPE_ISZ] = "tick_wrap_mul_isz",
        },
    // TICK_BUILTIN_WRAP_DIV
    [TICK_BUILTIN_WRAP_DIV] =
        {
            [TICK_TYPE_I8] = "tick_wrap_div_i8",
            [TICK_TYPE_I16] = "tick_wrap_div_i16",
            [TICK_TYPE_I32] = "tick_wrap_div_i32",
            [TICK_TYPE_I64] = "tick_wrap_div_i64",
            [TICK_TYPE_ISZ] = "tick_wrap_div_isz",
        },
    // TICK_BUILTIN_CHECKED_ADD
    [TICK_BUILTIN_CHECKED_ADD] =
        {
            [TICK_TYPE_I8] = "tick_checked_add_i8",
            [TICK_TYPE_I16] = "tick_checked_add_i16",
            [TICK_TYPE_I32] = "tick_checked_add_i32",
            [TICK_TYPE_I64] = "tick_checked_add_i64",
            [TICK_TYPE_ISZ] = "tick_checked_add_isz",
            [TICK_TYPE_U8] = "tick_wrap_add_u8",
            [TICK_TYPE_U16] = "tick_wrap_add_u16",
            [TICK_TYPE_U32] = "tick_wrap_add_u32",
            [TICK_TYPE_U64] = "tick_wrap_add_u64",
            [TICK_TYPE_USZ] = "tick_wrap_add_usz",
        },
    // TICK_BUILTIN_CHECKED_SUB
    [TICK_BUILTIN_CHECKED_SUB] =
        {
            [TICK_TYPE_I8] = "tick_checked_sub_i8",
            [TICK_TYPE_I16] = "tick_checked_sub_i16",
            [TICK_TYPE_I32] = "tick_checked_sub_i32",
            [TICK_TYPE_I64] = "tick_checked_sub_i64",
            [TICK_TYPE_ISZ] = "tick_checked_sub_isz",
            [TICK_TYPE_U8] = "tick_wrap_sub_u8",
            [TICK_TYPE_U16] = "tick_wrap_sub_u16",
            [TICK_TYPE_U32] = "tick_wrap_sub_u32",
            [TICK_TYPE_U64] = "tick_wrap_sub_u64",
            [TICK_TYPE_USZ] = "tick_wrap_sub_usz",
        },
    // TICK_BUILTIN_CHECKED_MUL
    [TICK_BUILTIN_CHECKED_MUL] =
        {
            [TICK_TYPE_I8] = "tick_checked_mul_i8",
            [TICK_TYPE_I16] = "tick_checked_mul_i16",
            [TICK_TYPE_I32] = "tick_checked_mul_i32",
            [TICK_TYPE_I64] = "tick_checked_mul_i64",
            [TICK_TYPE_ISZ] = "tick_checked_mul_isz",
            [TICK_TYPE_U8] = "tick_wrap_mul_u8",
            [TICK_TYPE_U16] = "tick_wrap_mul_u16",
            [TICK_TYPE_U32] = "tick_wrap_mul_u32",
            [TICK_TYPE_U64] = "tick_wrap_mul_u64",
            [TICK_TYPE_USZ] = "tick_wrap_mul_usz",
        },
    // TICK_BUILTIN_CHECKED_DIV
    [TICK_BUILTIN_CHECKED_DIV] =
        {
            [TICK_TYPE_I8] = "tick_checked_div_i8",
            [TICK_TYPE_I16] = "tick_checked_div_i16",
            [TICK_TYPE_I32] = "tick_checked_div_i32",
            [TICK_TYPE_I64] = "tick_checked_div_i64",
            [TICK_TYPE_ISZ] = "tick_checked_div_isz",
            [TICK_TYPE_U8] = "tick_checked_div_u8",
            [TICK_TYPE_U16] = "tick_checked_div_u16",
            [TICK_TYPE_U32] = "tick_checked_div_u32",
            [TICK_TYPE_U64] = "tick_checked_div_u64",
            [TICK_TYPE_USZ] = "tick_checked_div_usz",
        },
    // TICK_BUILTIN_CHECKED_MOD
    [TICK_BUILTIN_CHECKED_MOD] =
        {
            [TICK_TYPE_I8] = "tick_checked_mod_i8",
            [TICK_TYPE_I16] = "tick_checked_mod_i16",
            [TICK_TYPE_I32] = "tick_checked_mod_i32",
            [TICK_TYPE_I64] = "tick_checked_mod_i64",
            [TICK_TYPE_ISZ] = "tick_checked_mod_isz",
            [TICK_TYPE_U8] = "tick_checked_mod_u8",
            [TICK_TYPE_U16] = "tick_checked_mod_u16",
            [TICK_TYPE_U32] = "tick_checked_mod_u32",
            [TICK_TYPE_U64] = "tick_checked_mod_u64",
            [TICK_TYPE_USZ] = "tick_checked_mod_usz",
        },
    // TICK_BUILTIN_CHECKED_SHL
    [TICK_BUILTIN_CHECKED_SHL] =
        {
            [TICK_TYPE_I8] = "tick_checked_shl_i8",
            [TICK_TYPE_I16] = "tick_checked_shl_i16",
            [TICK_TYPE_I32] = "tick_checked_shl_i32",
            [TICK_TYPE_I64] = "tick_checked_shl_i64",
            [TICK_TYPE_ISZ] = "tick_checked_shl_isz",
            [TICK_TYPE_U8] = "tick_checked_shl_u8",
            [TICK_TYPE_U16] = "tick_checked_shl_u16",
            [TICK_TYPE_U32] = "tick_checked_shl_u32",
            [TICK_TYPE_U64] = "tick_checked_shl_u64",
            [TICK_TYPE_USZ] = "tick_checked_shl_usz",
        },
    // TICK_BUILTIN_CHECKED_SHR
    [TICK_BUILTIN_CHECKED_SHR] =
        {
            [TICK_TYPE_I8] = "tick_checked_shr_i8",
            [TICK_TYPE_I16] = "tick_checked_shr_i16",
            [TICK_TYPE_I32] = "tick_checked_shr_i32",
            [TICK_TYPE_I64] = "tick_checked_shr_i64",
            [TICK_TYPE_ISZ] = "tick_checked_shr_isz",
            [TICK_TYPE_U8] = "tick_checked_shr_u8",
            [TICK_TYPE_U16] = "tick_checked_shr_u16",
            [TICK_TYPE_U32] = "tick_checked_shr_u32",
            [TICK_TYPE_U64] = "tick_checked_shr_u64",
            [TICK_TYPE_USZ] = "tick_checked_shr_usz",
        },
    // TICK_BUILTIN_CHECKED_NEG
    [TICK_BUILTIN_CHECKED_NEG] =
        {
            [TICK_TYPE_I8] = "tick_checked_neg_i8",
            [TICK_TYPE_I16] = "tick_checked_neg_i16",
            [TICK_TYPE_I32] = "tick_checked_neg_i32",
            [TICK_TYPE_I64] = "tick_checked_neg_i64",
            [TICK_TYPE_ISZ] = "tick_checked_neg_isz",
            // Unsigned types cannot be negated
        },
    // TICK_BUILTIN_CHECKED_CAST handled separately via CAST_FUNCS table
};

// Cast function names indexed by [src_type][dst_type]
// Returns function name string or NULL if bare cast is sufficient
static const char* CAST_FUNCS[NUM_TYPES][NUM_TYPES] = {
    // From I8
    [TICK_TYPE_I8] =
        {
            [TICK_TYPE_I8] = NULL,   // Same type - bare cast
            [TICK_TYPE_I16] = NULL,  // Widening - bare cast
            [TICK_TYPE_I32] = NULL,
            [TICK_TYPE_I64] = NULL,
            [TICK_TYPE_ISZ] = NULL,
            [TICK_TYPE_U8] = "tick_checked_cast_i8_u8",
            [TICK_TYPE_U16] = "tick_checked_cast_i8_u16",
            [TICK_TYPE_U32] = "tick_checked_cast_i8_u32",
            [TICK_TYPE_U64] = "tick_checked_cast_i8_u64",
            [TICK_TYPE_USZ] = "tick_checked_cast_i8_usz",
        },
    // From I16
    [TICK_TYPE_I16] =
        {
            [TICK_TYPE_I8] = "tick_checked_cast_i16_i8",
            [TICK_TYPE_I16] = NULL,
            [TICK_TYPE_I32] = NULL,
            [TICK_TYPE_I64] = NULL,
            [TICK_TYPE_ISZ] = NULL,
            [TICK_TYPE_U8] = "tick_checked_cast_i16_u8",
            [TICK_TYPE_U16] = "tick_checked_cast_i16_u16",
            [TICK_TYPE_U32] = "tick_checked_cast_i16_u32",
            [TICK_TYPE_U64] = "tick_checked_cast_i16_u64",
            [TICK_TYPE_USZ] = "tick_checked_cast_i16_usz",
        },
    // From I32
    [TICK_TYPE_I32] =
        {
            [TICK_TYPE_I8] = "tick_checked_cast_i32_i8",
            [TICK_TYPE_I16] = "tick_checked_cast_i32_i16",
            [TICK_TYPE_I32] = NULL,
            [TICK_TYPE_I64] = NULL,
            [TICK_TYPE_ISZ] = NULL,
            [TICK_TYPE_U8] = "tick_checked_cast_i32_u8",
            [TICK_TYPE_U16] = "tick_checked_cast_i32_u16",
            [TICK_TYPE_U32] = "tick_checked_cast_i32_u32",
            [TICK_TYPE_U64] = "tick_checked_cast_i32_u64",
            [TICK_TYPE_USZ] = "tick_checked_cast_i32_usz",
        },
    // From I64
    [TICK_TYPE_I64] =
        {
            [TICK_TYPE_I8] = "tick_checked_cast_i64_i8",
            [TICK_TYPE_I16] = "tick_checked_cast_i64_i16",
            [TICK_TYPE_I32] = "tick_checked_cast_i64_i32",
            [TICK_TYPE_I64] = NULL,
            [TICK_TYPE_ISZ] = "tick_checked_cast_i64_isz",
            [TICK_TYPE_U8] = "tick_checked_cast_i64_u8",
            [TICK_TYPE_U16] = "tick_checked_cast_i64_u16",
            [TICK_TYPE_U32] = "tick_checked_cast_i64_u32",
            [TICK_TYPE_U64] = "tick_checked_cast_i64_u64",
            [TICK_TYPE_USZ] = "tick_checked_cast_i64_usz",
        },
    // From ISZ
    [TICK_TYPE_ISZ] =
        {
            [TICK_TYPE_I8] = "tick_checked_cast_isz_i8",
            [TICK_TYPE_I16] = "tick_checked_cast_isz_i16",
            [TICK_TYPE_I32] = "tick_checked_cast_isz_i32",
            [TICK_TYPE_I64] = "tick_checked_cast_isz_i64",
            [TICK_TYPE_ISZ] = NULL,
            [TICK_TYPE_U8] = "tick_checked_cast_isz_u8",
            [TICK_TYPE_U16] = "tick_checked_cast_isz_u16",
            [TICK_TYPE_U32] = "tick_checked_cast_isz_u32",
            [TICK_TYPE_U64] = "tick_checked_cast_isz_u64",
            [TICK_TYPE_USZ] = "tick_checked_cast_isz_usz",
        },
    // From U8
    [TICK_TYPE_U8] =
        {
            [TICK_TYPE_I8] = "tick_checked_cast_u8_i8",
            [TICK_TYPE_I16] = NULL,  // Widening to larger signed - safe
            [TICK_TYPE_I32] = NULL,
            [TICK_TYPE_I64] = NULL,
            [TICK_TYPE_ISZ] = NULL,
            [TICK_TYPE_U8] = NULL,
            [TICK_TYPE_U16] = NULL,
            [TICK_TYPE_U32] = NULL,
            [TICK_TYPE_U64] = NULL,
            [TICK_TYPE_USZ] = NULL,
        },
    // From U16
    [TICK_TYPE_U16] =
        {
            [TICK_TYPE_I8] = "tick_checked_cast_u16_i8",
            [TICK_TYPE_I16] = "tick_checked_cast_u16_i16",
            [TICK_TYPE_I32] = NULL,
            [TICK_TYPE_I64] = NULL,
            [TICK_TYPE_ISZ] = NULL,
            [TICK_TYPE_U8] = "tick_checked_cast_u16_u8",
            [TICK_TYPE_U16] = NULL,
            [TICK_TYPE_U32] = NULL,
            [TICK_TYPE_U64] = NULL,
            [TICK_TYPE_USZ] = NULL,
        },
    // From U32
    [TICK_TYPE_U32] =
        {
            [TICK_TYPE_I8] = "tick_checked_cast_u32_i8",
            [TICK_TYPE_I16] = "tick_checked_cast_u32_i16",
            [TICK_TYPE_I32] = "tick_checked_cast_u32_i32",
            [TICK_TYPE_I64] = NULL,
            [TICK_TYPE_ISZ] = NULL,
            [TICK_TYPE_U8] = "tick_checked_cast_u32_u8",
            [TICK_TYPE_U16] = "tick_checked_cast_u32_u16",
            [TICK_TYPE_U32] = NULL,
            [TICK_TYPE_U64] = NULL,
            [TICK_TYPE_USZ] = NULL,
        },
    // From U64
    [TICK_TYPE_U64] =
        {
            [TICK_TYPE_I8] = "tick_checked_cast_u64_i8",
            [TICK_TYPE_I16] = "tick_checked_cast_u64_i16",
            [TICK_TYPE_I32] = "tick_checked_cast_u64_i32",
            [TICK_TYPE_I64] = "tick_checked_cast_u64_i64",
            [TICK_TYPE_ISZ] = "tick_checked_cast_u64_isz",
            [TICK_TYPE_U8] = "tick_checked_cast_u64_u8",
            [TICK_TYPE_U16] = "tick_checked_cast_u64_u16",
            [TICK_TYPE_U32] = "tick_checked_cast_u64_u32",
            [TICK_TYPE_U64] = NULL,
            [TICK_TYPE_USZ] = "tick_checked_cast_u64_usz",
        },
    // From USZ
    [TICK_TYPE_USZ] =
        {
            [TICK_TYPE_I8] = "tick_checked_cast_usz_i8",
            [TICK_TYPE_I16] = "tick_checked_cast_usz_i16",
            [TICK_TYPE_I32] = "tick_checked_cast_usz_i32",
            [TICK_TYPE_I64] = "tick_checked_cast_usz_i64",
            [TICK_TYPE_ISZ] = "tick_checked_cast_usz_isz",
            [TICK_TYPE_U8] = "tick_checked_cast_usz_u8",
            [TICK_TYPE_U16] = "tick_checked_cast_usz_u16",
            [TICK_TYPE_U32] = "tick_checked_cast_usz_u32",
            [TICK_TYPE_U64] = "tick_checked_cast_usz_u64",
            [TICK_TYPE_USZ] = NULL,
        },
};

#endif  // TICK_CODEGEN_RUNTIME_TABLES_H
