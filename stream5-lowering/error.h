#ifndef ERROR_H
#define ERROR_H

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

// Error Reporting Interface
// Purpose: Consistent error handling across all phases

typedef enum {
    ERROR_LEXICAL,
    ERROR_SYNTAX,
    ERROR_TYPE,
    ERROR_RESOLUTION,
    ERROR_COROUTINE,
} ErrorKind;

typedef struct SourceLocation {
    uint32_t line;
    uint32_t column;
    const char* filename;
} SourceLocation;

typedef struct CompileError {
    ErrorKind kind;
    SourceLocation loc;
    char* message;
    char* suggestion;  // Optional fix hint
} CompileError;

typedef struct ErrorList {
    CompileError* errors;
    size_t count;
    size_t capacity;
} ErrorList;

// Forward declare Arena
typedef struct Arena Arena;

// Initialize with caller-provided memory
void error_list_init(ErrorList* list, Arena* arena);

// Add errors (messages stored in arena)
void error_list_add(ErrorList* list, ErrorKind kind, SourceLocation loc,
                    const char* fmt, ...);

// Reporting
void error_list_print(ErrorList* list, FILE* out);
bool error_list_has_errors(ErrorList* list);

#endif // ERROR_H
