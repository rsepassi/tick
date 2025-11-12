#ifndef TICK_RUNTIME_H_
#define TICK_RUNTIME_H_
// tick_runtime.h - Standard runtime header for all Tick programs

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>
#include <stdarg.h>
#include <limits.h>

// Type aliases matching Tick primitive types
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef ptrdiff_t isz;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t usz;

// String slice type (for lowered string slices)
typedef struct {
  u8* ptr;
  usz len;
} tick_str_t;

// ============================================================================
// Runtime Functions (platform-defined)
// ============================================================================

// Panic function - must be implemented by the platform/runtime
// Terminates program execution with an error message
void tick_panic(const char* msg, ...) __attribute__((noreturn, format(printf, 1, 2)));

// Debug logging function - prints to stderr
void tick_debug_log(const char* msg, ...) __attribute__((format(printf, 1, 2)));

// ============================================================================
// Saturating Arithmetic
// ============================================================================

// Signed 8-bit saturating arithmetic
static inline i8 tick_sat_add_i8(i8 a, i8 b) {
  i8 result;
  if (__builtin_add_overflow(a, b, &result)) {
    return (b > 0) ? INT8_MAX : INT8_MIN;
  }
  return result;
}

static inline i8 tick_sat_sub_i8(i8 a, i8 b) {
  i8 result;
  if (__builtin_sub_overflow(a, b, &result)) {
    return (b > 0) ? INT8_MIN : INT8_MAX;
  }
  return result;
}

static inline i8 tick_sat_mul_i8(i8 a, i8 b) {
  i8 result;
  if (__builtin_mul_overflow(a, b, &result)) {
    return ((a > 0) == (b > 0)) ? INT8_MAX : INT8_MIN;
  }
  return result;
}

static inline i8 tick_sat_div_i8(i8 a, i8 b) {
  if (b == 0) return 0;  // or could panic
  if (a == INT8_MIN && b == -1) return INT8_MAX;
  return a / b;
}

// Signed 16-bit saturating arithmetic
static inline i16 tick_sat_add_i16(i16 a, i16 b) {
  i16 result;
  if (__builtin_add_overflow(a, b, &result)) {
    return (b > 0) ? INT16_MAX : INT16_MIN;
  }
  return result;
}

static inline i16 tick_sat_sub_i16(i16 a, i16 b) {
  i16 result;
  if (__builtin_sub_overflow(a, b, &result)) {
    return (b > 0) ? INT16_MIN : INT16_MAX;
  }
  return result;
}

static inline i16 tick_sat_mul_i16(i16 a, i16 b) {
  i16 result;
  if (__builtin_mul_overflow(a, b, &result)) {
    return ((a > 0) == (b > 0)) ? INT16_MAX : INT16_MIN;
  }
  return result;
}

static inline i16 tick_sat_div_i16(i16 a, i16 b) {
  if (b == 0) return 0;  // or could panic
  if (a == INT16_MIN && b == -1) return INT16_MAX;
  return a / b;
}

// Signed 32-bit saturating arithmetic
static inline i32 tick_sat_add_i32(i32 a, i32 b) {
  i32 result;
  if (__builtin_add_overflow(a, b, &result)) {
    return (b > 0) ? INT32_MAX : INT32_MIN;
  }
  return result;
}

static inline i32 tick_sat_sub_i32(i32 a, i32 b) {
  i32 result;
  if (__builtin_sub_overflow(a, b, &result)) {
    return (b > 0) ? INT32_MIN : INT32_MAX;
  }
  return result;
}

static inline i32 tick_sat_mul_i32(i32 a, i32 b) {
  i32 result;
  if (__builtin_mul_overflow(a, b, &result)) {
    return ((a > 0) == (b > 0)) ? INT32_MAX : INT32_MIN;
  }
  return result;
}

static inline i32 tick_sat_div_i32(i32 a, i32 b) {
  if (b == 0) return 0;  // or could panic
  if (a == INT32_MIN && b == -1) return INT32_MAX;
  return a / b;
}

// Signed 64-bit saturating arithmetic
static inline i64 tick_sat_add_i64(i64 a, i64 b) {
  i64 result;
  if (__builtin_add_overflow(a, b, &result)) {
    return (b > 0) ? INT64_MAX : INT64_MIN;
  }
  return result;
}

static inline i64 tick_sat_sub_i64(i64 a, i64 b) {
  i64 result;
  if (__builtin_sub_overflow(a, b, &result)) {
    return (b > 0) ? INT64_MIN : INT64_MAX;
  }
  return result;
}

static inline i64 tick_sat_mul_i64(i64 a, i64 b) {
  i64 result;
  if (__builtin_mul_overflow(a, b, &result)) {
    return ((a > 0) == (b > 0)) ? INT64_MAX : INT64_MIN;
  }
  return result;
}

static inline i64 tick_sat_div_i64(i64 a, i64 b) {
  if (b == 0) return 0;  // or could panic
  if (a == INT64_MIN && b == -1) return INT64_MAX;
  return a / b;
}

// Unsigned 8-bit saturating arithmetic
static inline u8 tick_sat_add_u8(u8 a, u8 b) {
  u8 result;
  if (__builtin_add_overflow(a, b, &result)) {
    return UINT8_MAX;
  }
  return result;
}

static inline u8 tick_sat_sub_u8(u8 a, u8 b) {
  return (a > b) ? (a - b) : 0;
}

static inline u8 tick_sat_mul_u8(u8 a, u8 b) {
  u8 result;
  if (__builtin_mul_overflow(a, b, &result)) {
    return UINT8_MAX;
  }
  return result;
}

static inline u8 tick_sat_div_u8(u8 a, u8 b) {
  if (b == 0) return 0;  // or could panic
  return a / b;
}

// Unsigned 16-bit saturating arithmetic
static inline u16 tick_sat_add_u16(u16 a, u16 b) {
  u16 result;
  if (__builtin_add_overflow(a, b, &result)) {
    return UINT16_MAX;
  }
  return result;
}

static inline u16 tick_sat_sub_u16(u16 a, u16 b) {
  return (a > b) ? (a - b) : 0;
}

static inline u16 tick_sat_mul_u16(u16 a, u16 b) {
  u16 result;
  if (__builtin_mul_overflow(a, b, &result)) {
    return UINT16_MAX;
  }
  return result;
}

static inline u16 tick_sat_div_u16(u16 a, u16 b) {
  if (b == 0) return 0;  // or could panic
  return a / b;
}

// Unsigned 32-bit saturating arithmetic
static inline u32 tick_sat_add_u32(u32 a, u32 b) {
  u32 result;
  if (__builtin_add_overflow(a, b, &result)) {
    return UINT32_MAX;
  }
  return result;
}

static inline u32 tick_sat_sub_u32(u32 a, u32 b) {
  return (a > b) ? (a - b) : 0;
}

static inline u32 tick_sat_mul_u32(u32 a, u32 b) {
  u32 result;
  if (__builtin_mul_overflow(a, b, &result)) {
    return UINT32_MAX;
  }
  return result;
}

static inline u32 tick_sat_div_u32(u32 a, u32 b) {
  if (b == 0) return 0;  // or could panic
  return a / b;
}

// Unsigned 64-bit saturating arithmetic
static inline u64 tick_sat_add_u64(u64 a, u64 b) {
  u64 result;
  if (__builtin_add_overflow(a, b, &result)) {
    return UINT64_MAX;
  }
  return result;
}

static inline u64 tick_sat_sub_u64(u64 a, u64 b) {
  return (a > b) ? (a - b) : 0;
}

static inline u64 tick_sat_mul_u64(u64 a, u64 b) {
  u64 result;
  if (__builtin_mul_overflow(a, b, &result)) {
    return UINT64_MAX;
  }
  return result;
}

static inline u64 tick_sat_div_u64(u64 a, u64 b) {
  if (b == 0) return 0;  // or could panic
  return a / b;
}

// Signed size type saturating arithmetic
static inline isz tick_sat_add_isz(isz a, isz b) {
  isz result;
  if (__builtin_add_overflow(a, b, &result)) {
    return (b > 0) ? PTRDIFF_MAX : PTRDIFF_MIN;
  }
  return result;
}

static inline isz tick_sat_sub_isz(isz a, isz b) {
  isz result;
  if (__builtin_sub_overflow(a, b, &result)) {
    return (b > 0) ? PTRDIFF_MIN : PTRDIFF_MAX;
  }
  return result;
}

static inline isz tick_sat_mul_isz(isz a, isz b) {
  isz result;
  if (__builtin_mul_overflow(a, b, &result)) {
    return ((a > 0) == (b > 0)) ? PTRDIFF_MAX : PTRDIFF_MIN;
  }
  return result;
}

static inline isz tick_sat_div_isz(isz a, isz b) {
  if (b == 0) return 0;  // or could panic
  if (a == PTRDIFF_MIN && b == -1) return PTRDIFF_MAX;
  return a / b;
}

// Unsigned size type saturating arithmetic
static inline usz tick_sat_add_usz(usz a, usz b) {
  usz result;
  if (__builtin_add_overflow(a, b, &result)) {
    return SIZE_MAX;
  }
  return result;
}

static inline usz tick_sat_sub_usz(usz a, usz b) {
  return (a > b) ? (a - b) : 0;
}

static inline usz tick_sat_mul_usz(usz a, usz b) {
  usz result;
  if (__builtin_mul_overflow(a, b, &result)) {
    return SIZE_MAX;
  }
  return result;
}

static inline usz tick_sat_div_usz(usz a, usz b) {
  if (b == 0) return 0;  // or could panic
  return a / b;
}

// ============================================================================
// Wrapping Arithmetic (Two's Complement)
// ============================================================================
// These functions guarantee portable modulo 2^N wrapping behavior for signed
// integer arithmetic by performing the operation in the unsigned domain.
// Unsigned types don't need these - standard C operators already guarantee
// modulo semantics for unsigned integers per C11 6.2.5p9.

// Signed 8-bit wrapping arithmetic
static inline i8 tick_wrap_add_i8(i8 a, i8 b) {
  return (i8)((u8)a + (u8)b);
}

static inline i8 tick_wrap_sub_i8(i8 a, i8 b) {
  return (i8)((u8)a - (u8)b);
}

static inline i8 tick_wrap_mul_i8(i8 a, i8 b) {
  return (i8)((u8)a * (u8)b);
}

static inline i8 tick_wrap_div_i8(i8 a, i8 b) {
  if (b == 0) return 0;  // or could panic
  // Handle INT8_MIN / -1 overflow case
  if (a == INT8_MIN && b == -1) return INT8_MIN;  // Wraps to itself
  return a / b;
}

// Signed 16-bit wrapping arithmetic
static inline i16 tick_wrap_add_i16(i16 a, i16 b) {
  return (i16)((u16)a + (u16)b);
}

static inline i16 tick_wrap_sub_i16(i16 a, i16 b) {
  return (i16)((u16)a - (u16)b);
}

static inline i16 tick_wrap_mul_i16(i16 a, i16 b) {
  return (i16)((u16)a * (u16)b);
}

static inline i16 tick_wrap_div_i16(i16 a, i16 b) {
  if (b == 0) return 0;  // or could panic
  if (a == INT16_MIN && b == -1) return INT16_MIN;
  return a / b;
}

// Signed 32-bit wrapping arithmetic
static inline i32 tick_wrap_add_i32(i32 a, i32 b) {
  return (i32)((u32)a + (u32)b);
}

static inline i32 tick_wrap_sub_i32(i32 a, i32 b) {
  return (i32)((u32)a - (u32)b);
}

static inline i32 tick_wrap_mul_i32(i32 a, i32 b) {
  return (i32)((u32)a * (u32)b);
}

static inline i32 tick_wrap_div_i32(i32 a, i32 b) {
  if (b == 0) return 0;  // or could panic
  if (a == INT32_MIN && b == -1) return INT32_MIN;
  return a / b;
}

// Signed 64-bit wrapping arithmetic
static inline i64 tick_wrap_add_i64(i64 a, i64 b) {
  return (i64)((u64)a + (u64)b);
}

static inline i64 tick_wrap_sub_i64(i64 a, i64 b) {
  return (i64)((u64)a - (u64)b);
}

static inline i64 tick_wrap_mul_i64(i64 a, i64 b) {
  return (i64)((u64)a * (u64)b);
}

static inline i64 tick_wrap_div_i64(i64 a, i64 b) {
  if (b == 0) return 0;  // or could panic
  if (a == INT64_MIN && b == -1) return INT64_MIN;
  return a / b;
}

// Signed size type wrapping arithmetic
static inline isz tick_wrap_add_isz(isz a, isz b) {
  return (isz)((usz)a + (usz)b);
}

static inline isz tick_wrap_sub_isz(isz a, isz b) {
  return (isz)((usz)a - (usz)b);
}

static inline isz tick_wrap_mul_isz(isz a, isz b) {
  return (isz)((usz)a * (usz)b);
}

static inline isz tick_wrap_div_isz(isz a, isz b) {
  if (b == 0) return 0;  // or could panic
  if (a == PTRDIFF_MIN && b == -1) return PTRDIFF_MIN;
  return a / b;
}

// ============================================================================
// Checked Arithmetic (Panics on Undefined Behavior)
// ============================================================================
// These operations panic instead of invoking undefined behavior, making them
// suitable for generating safe, well-defined C11 code.

// ============================================================================
// Checked Addition
// ============================================================================
// Panics on: overflow

static inline i8 tick_checked_add_i8(i8 a, i8 b) {
  i8 result;
  if (__builtin_add_overflow(a, b, &result)) {
    tick_panic("addition overflow: i8(%d) + i8(%d)", (int)a, (int)b);
  }
  return result;
}

static inline i16 tick_checked_add_i16(i16 a, i16 b) {
  i16 result;
  if (__builtin_add_overflow(a, b, &result)) {
    tick_panic("addition overflow: i16(%d) + i16(%d)", (int)a, (int)b);
  }
  return result;
}

static inline i32 tick_checked_add_i32(i32 a, i32 b) {
  i32 result;
  if (__builtin_add_overflow(a, b, &result)) {
    tick_panic("addition overflow: i32(%d) + i32(%d)", a, b);
  }
  return result;
}

static inline i64 tick_checked_add_i64(i64 a, i64 b) {
  i64 result;
  if (__builtin_add_overflow(a, b, &result)) {
    tick_panic("addition overflow: i64(%lld) + i64(%lld)", (long long)a, (long long)b);
  }
  return result;
}

static inline isz tick_checked_add_isz(isz a, isz b) {
  isz result;
  if (__builtin_add_overflow(a, b, &result)) {
    tick_panic("addition overflow: isz(%td) + isz(%td)", a, b);
  }
  return result;
}

static inline u8 tick_checked_add_u8(u8 a, u8 b) {
  u8 result;
  if (__builtin_add_overflow(a, b, &result)) {
    tick_panic("addition overflow: u8(%u) + u8(%u)", (unsigned)a, (unsigned)b);
  }
  return result;
}

static inline u16 tick_checked_add_u16(u16 a, u16 b) {
  u16 result;
  if (__builtin_add_overflow(a, b, &result)) {
    tick_panic("addition overflow: u16(%u) + u16(%u)", (unsigned)a, (unsigned)b);
  }
  return result;
}

static inline u32 tick_checked_add_u32(u32 a, u32 b) {
  u32 result;
  if (__builtin_add_overflow(a, b, &result)) {
    tick_panic("addition overflow: u32(%u) + u32(%u)", a, b);
  }
  return result;
}

static inline u64 tick_checked_add_u64(u64 a, u64 b) {
  u64 result;
  if (__builtin_add_overflow(a, b, &result)) {
    tick_panic("addition overflow: u64(%llu) + u64(%llu)", (unsigned long long)a, (unsigned long long)b);
  }
  return result;
}

static inline usz tick_checked_add_usz(usz a, usz b) {
  usz result;
  if (__builtin_add_overflow(a, b, &result)) {
    tick_panic("addition overflow: usz(%zu) + usz(%zu)", a, b);
  }
  return result;
}

// ============================================================================
// Checked Subtraction
// ============================================================================
// Panics on: overflow

static inline i8 tick_checked_sub_i8(i8 a, i8 b) {
  i8 result;
  if (__builtin_sub_overflow(a, b, &result)) {
    tick_panic("subtraction overflow: i8(%d) - i8(%d)", (int)a, (int)b);
  }
  return result;
}

static inline i16 tick_checked_sub_i16(i16 a, i16 b) {
  i16 result;
  if (__builtin_sub_overflow(a, b, &result)) {
    tick_panic("subtraction overflow: i16(%d) - i16(%d)", (int)a, (int)b);
  }
  return result;
}

static inline i32 tick_checked_sub_i32(i32 a, i32 b) {
  i32 result;
  if (__builtin_sub_overflow(a, b, &result)) {
    tick_panic("subtraction overflow: i32(%d) - i32(%d)", a, b);
  }
  return result;
}

static inline i64 tick_checked_sub_i64(i64 a, i64 b) {
  i64 result;
  if (__builtin_sub_overflow(a, b, &result)) {
    tick_panic("subtraction overflow: i64(%lld) - i64(%lld)", (long long)a, (long long)b);
  }
  return result;
}

static inline isz tick_checked_sub_isz(isz a, isz b) {
  isz result;
  if (__builtin_sub_overflow(a, b, &result)) {
    tick_panic("subtraction overflow: isz(%td) - isz(%td)", a, b);
  }
  return result;
}

static inline u8 tick_checked_sub_u8(u8 a, u8 b) {
  u8 result;
  if (__builtin_sub_overflow(a, b, &result)) {
    tick_panic("subtraction overflow: u8(%u) - u8(%u)", (unsigned)a, (unsigned)b);
  }
  return result;
}

static inline u16 tick_checked_sub_u16(u16 a, u16 b) {
  u16 result;
  if (__builtin_sub_overflow(a, b, &result)) {
    tick_panic("subtraction overflow: u16(%u) - u16(%u)", (unsigned)a, (unsigned)b);
  }
  return result;
}

static inline u32 tick_checked_sub_u32(u32 a, u32 b) {
  u32 result;
  if (__builtin_sub_overflow(a, b, &result)) {
    tick_panic("subtraction overflow: u32(%u) - u32(%u)", a, b);
  }
  return result;
}

static inline u64 tick_checked_sub_u64(u64 a, u64 b) {
  u64 result;
  if (__builtin_sub_overflow(a, b, &result)) {
    tick_panic("subtraction overflow: u64(%llu) - u64(%llu)", (unsigned long long)a, (unsigned long long)b);
  }
  return result;
}

static inline usz tick_checked_sub_usz(usz a, usz b) {
  usz result;
  if (__builtin_sub_overflow(a, b, &result)) {
    tick_panic("subtraction overflow: usz(%zu) - usz(%zu)", a, b);
  }
  return result;
}

// ============================================================================
// Checked Multiplication
// ============================================================================
// Panics on: overflow

static inline i8 tick_checked_mul_i8(i8 a, i8 b) {
  i8 result;
  if (__builtin_mul_overflow(a, b, &result)) {
    tick_panic("multiplication overflow: i8(%d) * i8(%d)", (int)a, (int)b);
  }
  return result;
}

static inline i16 tick_checked_mul_i16(i16 a, i16 b) {
  i16 result;
  if (__builtin_mul_overflow(a, b, &result)) {
    tick_panic("multiplication overflow: i16(%d) * i16(%d)", (int)a, (int)b);
  }
  return result;
}

static inline i32 tick_checked_mul_i32(i32 a, i32 b) {
  i32 result;
  if (__builtin_mul_overflow(a, b, &result)) {
    tick_panic("multiplication overflow: i32(%d) * i32(%d)", a, b);
  }
  return result;
}

static inline i64 tick_checked_mul_i64(i64 a, i64 b) {
  i64 result;
  if (__builtin_mul_overflow(a, b, &result)) {
    tick_panic("multiplication overflow: i64(%lld) * i64(%lld)", (long long)a, (long long)b);
  }
  return result;
}

static inline isz tick_checked_mul_isz(isz a, isz b) {
  isz result;
  if (__builtin_mul_overflow(a, b, &result)) {
    tick_panic("multiplication overflow: isz(%td) * isz(%td)", a, b);
  }
  return result;
}

static inline u8 tick_checked_mul_u8(u8 a, u8 b) {
  u8 result;
  if (__builtin_mul_overflow(a, b, &result)) {
    tick_panic("multiplication overflow: u8(%u) * u8(%u)", (unsigned)a, (unsigned)b);
  }
  return result;
}

static inline u16 tick_checked_mul_u16(u16 a, u16 b) {
  u16 result;
  if (__builtin_mul_overflow(a, b, &result)) {
    tick_panic("multiplication overflow: u16(%u) * u16(%u)", (unsigned)a, (unsigned)b);
  }
  return result;
}

static inline u32 tick_checked_mul_u32(u32 a, u32 b) {
  u32 result;
  if (__builtin_mul_overflow(a, b, &result)) {
    tick_panic("multiplication overflow: u32(%u) * u32(%u)", a, b);
  }
  return result;
}

static inline u64 tick_checked_mul_u64(u64 a, u64 b) {
  u64 result;
  if (__builtin_mul_overflow(a, b, &result)) {
    tick_panic("multiplication overflow: u64(%llu) * u64(%llu)", (unsigned long long)a, (unsigned long long)b);
  }
  return result;
}

static inline usz tick_checked_mul_usz(usz a, usz b) {
  usz result;
  if (__builtin_mul_overflow(a, b, &result)) {
    tick_panic("multiplication overflow: usz(%zu) * usz(%zu)", a, b);
  }
  return result;
}

// ============================================================================
// Checked Division
// ============================================================================
// Panics on: division by zero, INT_MIN / -1

static inline i8 tick_checked_div_i8(i8 a, i8 b) {
  if (b == 0) tick_panic("division by zero");
  if (a == INT8_MIN && b == -1) tick_panic("division overflow: INT8_MIN / -1");
  return a / b;
}

static inline i16 tick_checked_div_i16(i16 a, i16 b) {
  if (b == 0) tick_panic("division by zero");
  if (a == INT16_MIN && b == -1) tick_panic("division overflow: INT16_MIN / -1");
  return a / b;
}

static inline i32 tick_checked_div_i32(i32 a, i32 b) {
  if (b == 0) tick_panic("division by zero");
  if (a == INT32_MIN && b == -1) tick_panic("division overflow: INT32_MIN / -1");
  return a / b;
}

static inline i64 tick_checked_div_i64(i64 a, i64 b) {
  if (b == 0) tick_panic("division by zero");
  if (a == INT64_MIN && b == -1) tick_panic("division overflow: INT64_MIN / -1");
  return a / b;
}

static inline isz tick_checked_div_isz(isz a, isz b) {
  if (b == 0) tick_panic("division by zero");
  if (a == PTRDIFF_MIN && b == -1) tick_panic("division overflow: PTRDIFF_MIN / -1");
  return a / b;
}

static inline u8 tick_checked_div_u8(u8 a, u8 b) {
  if (b == 0) tick_panic("division by zero");
  return a / b;
}

static inline u16 tick_checked_div_u16(u16 a, u16 b) {
  if (b == 0) tick_panic("division by zero");
  return a / b;
}

static inline u32 tick_checked_div_u32(u32 a, u32 b) {
  if (b == 0) tick_panic("division by zero");
  return a / b;
}

static inline u64 tick_checked_div_u64(u64 a, u64 b) {
  if (b == 0) tick_panic("division by zero");
  return a / b;
}

static inline usz tick_checked_div_usz(usz a, usz b) {
  if (b == 0) tick_panic("division by zero");
  return a / b;
}

// ============================================================================
// Checked Modulo
// ============================================================================
// Panics on: division by zero

static inline i8 tick_checked_mod_i8(i8 a, i8 b) {
  if (b == 0) tick_panic("modulo by zero");
  // Note: INT_MIN % -1 is well-defined as 0 in C, but involves division
  return a % b;
}

static inline i16 tick_checked_mod_i16(i16 a, i16 b) {
  if (b == 0) tick_panic("modulo by zero");
  return a % b;
}

static inline i32 tick_checked_mod_i32(i32 a, i32 b) {
  if (b == 0) tick_panic("modulo by zero");
  return a % b;
}

static inline i64 tick_checked_mod_i64(i64 a, i64 b) {
  if (b == 0) tick_panic("modulo by zero");
  return a % b;
}

static inline isz tick_checked_mod_isz(isz a, isz b) {
  if (b == 0) tick_panic("modulo by zero");
  return a % b;
}

static inline u8 tick_checked_mod_u8(u8 a, u8 b) {
  if (b == 0) tick_panic("modulo by zero");
  return a % b;
}

static inline u16 tick_checked_mod_u16(u16 a, u16 b) {
  if (b == 0) tick_panic("modulo by zero");
  return a % b;
}

static inline u32 tick_checked_mod_u32(u32 a, u32 b) {
  if (b == 0) tick_panic("modulo by zero");
  return a % b;
}

static inline u64 tick_checked_mod_u64(u64 a, u64 b) {
  if (b == 0) tick_panic("modulo by zero");
  return a % b;
}

static inline usz tick_checked_mod_usz(usz a, usz b) {
  if (b == 0) tick_panic("modulo by zero");
  return a % b;
}

// ============================================================================
// Checked Left Shift
// ============================================================================
// Panics on: shift < 0, shift >= bitwidth, shifting negative values, overflow

static inline i8 tick_checked_shl_i8(i8 a, i32 shift) {
  if (shift < 0) tick_panic("left shift by negative amount: %d", shift);
  if (shift >= 8) tick_panic("left shift >= bitwidth: %d >= 8", shift);
  if (a < 0) tick_panic("left shift of negative value: %d", (int)a);
  // Check for overflow: if any bits would be shifted out, it's UB
  if (shift > 0 && a > (INT8_MAX >> shift)) tick_panic("left shift overflow");
  return a << shift;
}

static inline i16 tick_checked_shl_i16(i16 a, i32 shift) {
  if (shift < 0) tick_panic("left shift by negative amount: %d", shift);
  if (shift >= 16) tick_panic("left shift >= bitwidth: %d >= 16", shift);
  if (a < 0) tick_panic("left shift of negative value: %d", (int)a);
  if (shift > 0 && a > (INT16_MAX >> shift)) tick_panic("left shift overflow");
  return a << shift;
}

static inline i32 tick_checked_shl_i32(i32 a, i32 shift) {
  if (shift < 0) tick_panic("left shift by negative amount: %d", shift);
  if (shift >= 32) tick_panic("left shift >= bitwidth: %d >= 32", shift);
  if (a < 0) tick_panic("left shift of negative value: %d", a);
  if (shift > 0 && a > (INT32_MAX >> shift)) tick_panic("left shift overflow");
  return a << shift;
}

static inline i64 tick_checked_shl_i64(i64 a, i32 shift) {
  if (shift < 0) tick_panic("left shift by negative amount: %d", shift);
  if (shift >= 64) tick_panic("left shift >= bitwidth: %d >= 64", shift);
  if (a < 0) tick_panic("left shift of negative value: %lld", (long long)a);
  if (shift > 0 && a > (INT64_MAX >> shift)) tick_panic("left shift overflow");
  return a << shift;
}

static inline isz tick_checked_shl_isz(isz a, i32 shift) {
  if (shift < 0) tick_panic("left shift by negative amount: %d", shift);
  if (shift >= (i32)(sizeof(isz) * 8)) tick_panic("left shift >= bitwidth: %d >= %d", shift, (int)(sizeof(isz) * 8));
  if (a < 0) tick_panic("left shift of negative value: %td", a);
  if (shift > 0 && a > (PTRDIFF_MAX >> shift)) tick_panic("left shift overflow");
  return a << shift;
}

static inline u8 tick_checked_shl_u8(u8 a, i32 shift) {
  if (shift < 0) tick_panic("left shift by negative amount: %d", shift);
  if (shift >= 8) tick_panic("left shift >= bitwidth: %d >= 8", shift);
  // Unsigned left shift is well-defined (wraps), but we check for "overflow" for consistency
  if (shift > 0 && a > (UINT8_MAX >> shift)) tick_panic("left shift overflow");
  return a << shift;
}

static inline u16 tick_checked_shl_u16(u16 a, i32 shift) {
  if (shift < 0) tick_panic("left shift by negative amount: %d", shift);
  if (shift >= 16) tick_panic("left shift >= bitwidth: %d >= 16", shift);
  if (shift > 0 && a > (UINT16_MAX >> shift)) tick_panic("left shift overflow");
  return a << shift;
}

static inline u32 tick_checked_shl_u32(u32 a, i32 shift) {
  if (shift < 0) tick_panic("left shift by negative amount: %d", shift);
  if (shift >= 32) tick_panic("left shift >= bitwidth: %d >= 32", shift);
  if (shift > 0 && a > (UINT32_MAX >> shift)) tick_panic("left shift overflow");
  return a << shift;
}

static inline u64 tick_checked_shl_u64(u64 a, i32 shift) {
  if (shift < 0) tick_panic("left shift by negative amount: %d", shift);
  if (shift >= 64) tick_panic("left shift >= bitwidth: %d >= 64", shift);
  if (shift > 0 && a > (UINT64_MAX >> shift)) tick_panic("left shift overflow");
  return a << shift;
}

static inline usz tick_checked_shl_usz(usz a, i32 shift) {
  if (shift < 0) tick_panic("left shift by negative amount: %d", shift);
  if (shift >= (i32)(sizeof(usz) * 8)) tick_panic("left shift >= bitwidth: %d >= %d", shift, (int)(sizeof(usz) * 8));
  if (shift > 0 && a > (SIZE_MAX >> shift)) tick_panic("left shift overflow");
  return a << shift;
}

// ============================================================================
// Checked Right Shift
// ============================================================================
// Panics on: shift < 0, shift >= bitwidth
// For signed types, also panics on negative values (implementation-defined behavior)

static inline i8 tick_checked_shr_i8(i8 a, i32 shift) {
  if (shift < 0) tick_panic("right shift by negative amount: %d", shift);
  if (shift >= 8) tick_panic("right shift >= bitwidth: %d >= 8", shift);
  if (a < 0) tick_panic("right shift of negative value (implementation-defined): %d", (int)a);
  return a >> shift;
}

static inline i16 tick_checked_shr_i16(i16 a, i32 shift) {
  if (shift < 0) tick_panic("right shift by negative amount: %d", shift);
  if (shift >= 16) tick_panic("right shift >= bitwidth: %d >= 16", shift);
  if (a < 0) tick_panic("right shift of negative value (implementation-defined): %d", (int)a);
  return a >> shift;
}

static inline i32 tick_checked_shr_i32(i32 a, i32 shift) {
  if (shift < 0) tick_panic("right shift by negative amount: %d", shift);
  if (shift >= 32) tick_panic("right shift >= bitwidth: %d >= 32", shift);
  if (a < 0) tick_panic("right shift of negative value (implementation-defined): %d", a);
  return a >> shift;
}

static inline i64 tick_checked_shr_i64(i64 a, i32 shift) {
  if (shift < 0) tick_panic("right shift by negative amount: %d", shift);
  if (shift >= 64) tick_panic("right shift >= bitwidth: %d >= 64", shift);
  if (a < 0) tick_panic("right shift of negative value (implementation-defined): %lld", (long long)a);
  return a >> shift;
}

static inline isz tick_checked_shr_isz(isz a, i32 shift) {
  if (shift < 0) tick_panic("right shift by negative amount: %d", shift);
  if (shift >= (i32)(sizeof(isz) * 8)) tick_panic("right shift >= bitwidth: %d >= %d", shift, (int)(sizeof(isz) * 8));
  if (a < 0) tick_panic("right shift of negative value (implementation-defined): %td", a);
  return a >> shift;
}

static inline u8 tick_checked_shr_u8(u8 a, i32 shift) {
  if (shift < 0) tick_panic("right shift by negative amount: %d", shift);
  if (shift >= 8) tick_panic("right shift >= bitwidth: %d >= 8", shift);
  return a >> shift;
}

static inline u16 tick_checked_shr_u16(u16 a, i32 shift) {
  if (shift < 0) tick_panic("right shift by negative amount: %d", shift);
  if (shift >= 16) tick_panic("right shift >= bitwidth: %d >= 16", shift);
  return a >> shift;
}

static inline u32 tick_checked_shr_u32(u32 a, i32 shift) {
  if (shift < 0) tick_panic("right shift by negative amount: %d", shift);
  if (shift >= 32) tick_panic("right shift >= bitwidth: %d >= 32", shift);
  return a >> shift;
}

static inline u64 tick_checked_shr_u64(u64 a, i32 shift) {
  if (shift < 0) tick_panic("right shift by negative amount: %d", shift);
  if (shift >= 64) tick_panic("right shift >= bitwidth: %d >= 64", shift);
  return a >> shift;
}

static inline usz tick_checked_shr_usz(usz a, i32 shift) {
  if (shift < 0) tick_panic("right shift by negative amount: %d", shift);
  if (shift >= (i32)(sizeof(usz) * 8)) tick_panic("right shift >= bitwidth: %d >= %d", shift, (int)(sizeof(usz) * 8));
  return a >> shift;
}

// ============================================================================
// Checked Negation
// ============================================================================
// Panics on: negating INT_MIN (overflow)

static inline i8 tick_checked_neg_i8(i8 a) {
  if (a == INT8_MIN) tick_panic("negation overflow: -INT8_MIN");
  return -a;
}

static inline i16 tick_checked_neg_i16(i16 a) {
  if (a == INT16_MIN) tick_panic("negation overflow: -INT16_MIN");
  return -a;
}

static inline i32 tick_checked_neg_i32(i32 a) {
  if (a == INT32_MIN) tick_panic("negation overflow: -INT32_MIN");
  return -a;
}

static inline i64 tick_checked_neg_i64(i64 a) {
  if (a == INT64_MIN) tick_panic("negation overflow: -INT64_MIN");
  return -a;
}

static inline isz tick_checked_neg_isz(isz a) {
  if (a == PTRDIFF_MIN) tick_panic("negation overflow: -PTRDIFF_MIN");
  return -a;
}

// ============================================================================
// Checked Casts
// ============================================================================
// Panics on: out-of-range conversions
//
// Cast naming convention: tick_checked_cast_{from}_{to}
// For example: tick_checked_cast_i32_i8 casts i32 -> i8 with range checking

// Signed to signed (narrowing)
static inline i8 tick_checked_cast_i16_i8(i16 val) {
  if (val < INT8_MIN || val > INT8_MAX) tick_panic("cast out of range: %d not in [%d, %d]", (int)val, (int)INT8_MIN, (int)INT8_MAX);
  return (i8)val;
}

static inline i8 tick_checked_cast_i32_i8(i32 val) {
  if (val < INT8_MIN || val > INT8_MAX) tick_panic("cast out of range: %d not in [%d, %d]", val, (int)INT8_MIN, (int)INT8_MAX);
  return (i8)val;
}

static inline i8 tick_checked_cast_i64_i8(i64 val) {
  if (val < INT8_MIN || val > INT8_MAX) tick_panic("cast out of range: %lld not in [%d, %d]", (long long)val, (int)INT8_MIN, (int)INT8_MAX);
  return (i8)val;
}

static inline i8 tick_checked_cast_isz_i8(isz val) {
  if (val < INT8_MIN || val > INT8_MAX) tick_panic("cast out of range: %td not in [%d, %d]", val, (int)INT8_MIN, (int)INT8_MAX);
  return (i8)val;
}

static inline i16 tick_checked_cast_i32_i16(i32 val) {
  if (val < INT16_MIN || val > INT16_MAX) tick_panic("cast out of range: %d not in [%d, %d]", val, (int)INT16_MIN, (int)INT16_MAX);
  return (i16)val;
}

static inline i16 tick_checked_cast_i64_i16(i64 val) {
  if (val < INT16_MIN || val > INT16_MAX) tick_panic("cast out of range: %lld not in [%d, %d]", (long long)val, (int)INT16_MIN, (int)INT16_MAX);
  return (i16)val;
}

static inline i16 tick_checked_cast_isz_i16(isz val) {
  if (val < INT16_MIN || val > INT16_MAX) tick_panic("cast out of range: %td not in [%d, %d]", val, (int)INT16_MIN, (int)INT16_MAX);
  return (i16)val;
}

static inline i32 tick_checked_cast_i64_i32(i64 val) {
  if (val < INT32_MIN || val > INT32_MAX) tick_panic("cast out of range: %lld not in [%d, %d]", (long long)val, INT32_MIN, INT32_MAX);
  return (i32)val;
}

static inline i32 tick_checked_cast_isz_i32(isz val) {
  if (val < INT32_MIN || val > INT32_MAX) tick_panic("cast out of range: %td not in [%d, %d]", val, INT32_MIN, INT32_MAX);
  return (i32)val;
}

// Unsigned to unsigned (narrowing)
static inline u8 tick_checked_cast_u16_u8(u16 val) {
  if (val > UINT8_MAX) tick_panic("cast out of range: %u > %u", (unsigned)val, (unsigned)UINT8_MAX);
  return (u8)val;
}

static inline u8 tick_checked_cast_u32_u8(u32 val) {
  if (val > UINT8_MAX) tick_panic("cast out of range: %u > %u", val, (unsigned)UINT8_MAX);
  return (u8)val;
}

static inline u8 tick_checked_cast_u64_u8(u64 val) {
  if (val > UINT8_MAX) tick_panic("cast out of range: %llu > %u", (unsigned long long)val, (unsigned)UINT8_MAX);
  return (u8)val;
}

static inline u8 tick_checked_cast_usz_u8(usz val) {
  if (val > UINT8_MAX) tick_panic("cast out of range: %zu > %u", val, (unsigned)UINT8_MAX);
  return (u8)val;
}

static inline u16 tick_checked_cast_u32_u16(u32 val) {
  if (val > UINT16_MAX) tick_panic("cast out of range: %u > %u", val, (unsigned)UINT16_MAX);
  return (u16)val;
}

static inline u16 tick_checked_cast_u64_u16(u64 val) {
  if (val > UINT16_MAX) tick_panic("cast out of range: %llu > %u", (unsigned long long)val, (unsigned)UINT16_MAX);
  return (u16)val;
}

static inline u16 tick_checked_cast_usz_u16(usz val) {
  if (val > UINT16_MAX) tick_panic("cast out of range: %zu > %u", val, (unsigned)UINT16_MAX);
  return (u16)val;
}

static inline u32 tick_checked_cast_u64_u32(u64 val) {
  if (val > UINT32_MAX) tick_panic("cast out of range: %llu > %u", (unsigned long long)val, UINT32_MAX);
  return (u32)val;
}

static inline u32 tick_checked_cast_usz_u32(usz val) {
  if (val > UINT32_MAX) tick_panic("cast out of range: %zu > %u", val, UINT32_MAX);
  return (u32)val;
}

// Signed to unsigned
static inline u8 tick_checked_cast_i8_u8(i8 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %d", (int)val);
  return (u8)val;
}

static inline u8 tick_checked_cast_i16_u8(i16 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %d", (int)val);
  if (val > UINT8_MAX) tick_panic("cast out of range: %d > %u", (int)val, (unsigned)UINT8_MAX);
  return (u8)val;
}

static inline u8 tick_checked_cast_i32_u8(i32 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %d", val);
  if (val > UINT8_MAX) tick_panic("cast out of range: %d > %u", val, (unsigned)UINT8_MAX);
  return (u8)val;
}

static inline u8 tick_checked_cast_i64_u8(i64 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %lld", (long long)val);
  if (val > UINT8_MAX) tick_panic("cast out of range: %lld > %u", (long long)val, (unsigned)UINT8_MAX);
  return (u8)val;
}

static inline u8 tick_checked_cast_isz_u8(isz val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %td", val);
  if (val > UINT8_MAX) tick_panic("cast out of range: %td > %u", val, (unsigned)UINT8_MAX);
  return (u8)val;
}

static inline u16 tick_checked_cast_i8_u16(i8 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %d", (int)val);
  return (u16)val;
}

static inline u16 tick_checked_cast_i16_u16(i16 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %d", (int)val);
  return (u16)val;
}

static inline u16 tick_checked_cast_i32_u16(i32 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %d", val);
  if (val > UINT16_MAX) tick_panic("cast out of range: %d > %u", val, (unsigned)UINT16_MAX);
  return (u16)val;
}

static inline u16 tick_checked_cast_i64_u16(i64 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %lld", (long long)val);
  if (val > UINT16_MAX) tick_panic("cast out of range: %lld > %u", (long long)val, (unsigned)UINT16_MAX);
  return (u16)val;
}

static inline u16 tick_checked_cast_isz_u16(isz val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %td", val);
  if (val > UINT16_MAX) tick_panic("cast out of range: %td > %u", val, (unsigned)UINT16_MAX);
  return (u16)val;
}

static inline u32 tick_checked_cast_i8_u32(i8 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %d", (int)val);
  return (u32)val;
}

static inline u32 tick_checked_cast_i16_u32(i16 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %d", (int)val);
  return (u32)val;
}

static inline u32 tick_checked_cast_i32_u32(i32 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %d", val);
  return (u32)val;
}

static inline u32 tick_checked_cast_i64_u32(i64 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %lld", (long long)val);
  if (val > UINT32_MAX) tick_panic("cast out of range: %lld > %u", (long long)val, UINT32_MAX);
  return (u32)val;
}

static inline u32 tick_checked_cast_isz_u32(isz val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %td", val);
  if (val > UINT32_MAX) tick_panic("cast out of range: %td > %u", val, UINT32_MAX);
  return (u32)val;
}

static inline u64 tick_checked_cast_i8_u64(i8 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %d", (int)val);
  return (u64)val;
}

static inline u64 tick_checked_cast_i16_u64(i16 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %d", (int)val);
  return (u64)val;
}

static inline u64 tick_checked_cast_i32_u64(i32 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %d", val);
  return (u64)val;
}

static inline u64 tick_checked_cast_i64_u64(i64 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %lld", (long long)val);
  return (u64)val;
}

static inline u64 tick_checked_cast_isz_u64(isz val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %td", val);
  return (u64)val;
}

static inline usz tick_checked_cast_i8_usz(i8 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %d", (int)val);
  return (usz)val;
}

static inline usz tick_checked_cast_i16_usz(i16 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %d", (int)val);
  return (usz)val;
}

static inline usz tick_checked_cast_i32_usz(i32 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %d", val);
  return (usz)val;
}

static inline usz tick_checked_cast_i64_usz(i64 val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %lld", (long long)val);
  return (usz)val;
}

static inline usz tick_checked_cast_isz_usz(isz val) {
  if (val < 0) tick_panic("cast of negative value to unsigned: %td", val);
  return (usz)val;
}

// Unsigned to signed
static inline i8 tick_checked_cast_u8_i8(u8 val) {
  if (val > INT8_MAX) tick_panic("cast out of range: %u > %d", (unsigned)val, (int)INT8_MAX);
  return (i8)val;
}

static inline i8 tick_checked_cast_u16_i8(u16 val) {
  if (val > INT8_MAX) tick_panic("cast out of range: %u > %d", (unsigned)val, (int)INT8_MAX);
  return (i8)val;
}

static inline i8 tick_checked_cast_u32_i8(u32 val) {
  if (val > INT8_MAX) tick_panic("cast out of range: %u > %d", val, (int)INT8_MAX);
  return (i8)val;
}

static inline i8 tick_checked_cast_u64_i8(u64 val) {
  if (val > INT8_MAX) tick_panic("cast out of range: %llu > %d", (unsigned long long)val, (int)INT8_MAX);
  return (i8)val;
}

static inline i8 tick_checked_cast_usz_i8(usz val) {
  if (val > (usz)INT8_MAX) tick_panic("cast out of range: %zu > %d", val, (int)INT8_MAX);
  return (i8)val;
}

static inline i16 tick_checked_cast_u8_i16(u8 val) {
  // Always safe: u8 max (255) < i16 max (32767)
  return (i16)val;
}

static inline i16 tick_checked_cast_u16_i16(u16 val) {
  if (val > INT16_MAX) tick_panic("cast out of range: %u > %d", (unsigned)val, (int)INT16_MAX);
  return (i16)val;
}

static inline i16 tick_checked_cast_u32_i16(u32 val) {
  if (val > INT16_MAX) tick_panic("cast out of range: %u > %d", val, (int)INT16_MAX);
  return (i16)val;
}

static inline i16 tick_checked_cast_u64_i16(u64 val) {
  if (val > INT16_MAX) tick_panic("cast out of range: %llu > %d", (unsigned long long)val, (int)INT16_MAX);
  return (i16)val;
}

static inline i16 tick_checked_cast_usz_i16(usz val) {
  if (val > (usz)INT16_MAX) tick_panic("cast out of range: %zu > %d", val, (int)INT16_MAX);
  return (i16)val;
}

static inline i32 tick_checked_cast_u8_i32(u8 val) {
  // Always safe
  return (i32)val;
}

static inline i32 tick_checked_cast_u16_i32(u16 val) {
  // Always safe
  return (i32)val;
}

static inline i32 tick_checked_cast_u32_i32(u32 val) {
  if (val > INT32_MAX) tick_panic("cast out of range: %u > %d", val, INT32_MAX);
  return (i32)val;
}

static inline i32 tick_checked_cast_u64_i32(u64 val) {
  if (val > INT32_MAX) tick_panic("cast out of range: %llu > %d", (unsigned long long)val, INT32_MAX);
  return (i32)val;
}

static inline i32 tick_checked_cast_usz_i32(usz val) {
  if (val > (usz)INT32_MAX) tick_panic("cast out of range: %zu > %d", val, INT32_MAX);
  return (i32)val;
}

static inline i64 tick_checked_cast_u8_i64(u8 val) {
  // Always safe
  return (i64)val;
}

static inline i64 tick_checked_cast_u16_i64(u16 val) {
  // Always safe
  return (i64)val;
}

static inline i64 tick_checked_cast_u32_i64(u32 val) {
  // Always safe
  return (i64)val;
}

static inline i64 tick_checked_cast_u64_i64(u64 val) {
  if (val > INT64_MAX) tick_panic("cast out of range: %llu > %lld", (unsigned long long)val, (long long)INT64_MAX);
  return (i64)val;
}

static inline i64 tick_checked_cast_usz_i64(usz val) {
  if (val > (usz)INT64_MAX) tick_panic("cast out of range: %zu > %lld", val, (long long)INT64_MAX);
  return (i64)val;
}

static inline isz tick_checked_cast_u8_isz(u8 val) {
  // Always safe
  return (isz)val;
}

static inline isz tick_checked_cast_u16_isz(u16 val) {
  // Always safe
  return (isz)val;
}

static inline isz tick_checked_cast_u32_isz(u32 val) {
  #if UINT32_MAX > PTRDIFF_MAX
  if (val > PTRDIFF_MAX) tick_panic("cast out of range: %u > %td", val, PTRDIFF_MAX);
  #endif
  return (isz)val;
}

static inline isz tick_checked_cast_u64_isz(u64 val) {
  if (val > (u64)PTRDIFF_MAX) tick_panic("cast out of range: %llu > %td", (unsigned long long)val, PTRDIFF_MAX);
  return (isz)val;
}

static inline isz tick_checked_cast_usz_isz(usz val) {
  if (val > (usz)PTRDIFF_MAX) tick_panic("cast out of range: %zu > %td", val, PTRDIFF_MAX);
  return (isz)val;
}

#endif  // TICK_RUNTIME_H_
