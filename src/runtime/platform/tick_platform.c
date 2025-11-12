// tick_platform.c - Platform-specific runtime implementation for Tick programs

#include "tick_runtime.h"
#include "printf.h"

#include <stdio.h>
#include <stdlib.h>

// Output function wrapper for fputc
static void fputc_wrapper(char c, void* stream) {
  fputc(c, (FILE*)stream);
}

// Panic function - terminates program execution with an error message
void tick_panic(const char* msg, ...) {
  va_list args;
  va_start(args, msg);

  // Print prefix
  const char* prefix = "tick panic: ";
  while (*prefix) {
    fputc_wrapper(*prefix++, stderr);
  }

  // Print formatted message
  vfctprintf(fputc_wrapper, stderr, msg, args);

  // Print newline
  fputc_wrapper('\n', stderr);

  va_end(args);

  abort();
}

// Debug logging function - prints to stderr
void tick_debug_log(const char* msg, ...) {
  va_list args;
  va_start(args, msg);

  vfctprintf(fputc_wrapper, stderr, msg, args);

  va_end(args);
}
