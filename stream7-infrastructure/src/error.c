#include "../error.h"
#include "../arena.h"
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

// Initial capacity for error array
#define INITIAL_ERROR_CAPACITY 16

// Color codes for terminal output
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

// Error kind names
static const char* error_kind_names[] = {
    [ERROR_LEXICAL] = "Lexical Error",
    [ERROR_SYNTAX] = "Syntax Error",
    [ERROR_TYPE] = "Type Error",
    [ERROR_RESOLUTION] = "Resolution Error",
    [ERROR_COROUTINE] = "Coroutine Error",
};

void error_list_init(ErrorList* list, Arena* arena) {
    assert(list != NULL);
    assert(arena != NULL);

    list->arena = arena;
    list->errors = (CompileError*)arena_alloc(arena,
                                              INITIAL_ERROR_CAPACITY * sizeof(CompileError),
                                              sizeof(void*));
    list->count = 0;
    list->capacity = INITIAL_ERROR_CAPACITY;
}

void error_list_add(ErrorList* list, ErrorKind kind, SourceLocation loc,
                    const char* fmt, ...) {
    assert(list != NULL);
    assert(list->arena != NULL);
    assert(fmt != NULL);

    // Check if we need to grow the error array
    if (list->count >= list->capacity) {
        // Allocate a new, larger array from arena
        size_t new_capacity = list->capacity * 2;
        CompileError* new_errors = (CompileError*)arena_alloc(list->arena,
                                                               new_capacity * sizeof(CompileError),
                                                               sizeof(void*));

        // Copy old errors to new array
        memcpy(new_errors, list->errors, list->count * sizeof(CompileError));

        list->errors = new_errors;
        list->capacity = new_capacity;
    }

    // Format the error message
    va_list args;
    va_start(args, fmt);

    // First, determine the size needed
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (size < 0) {
        va_end(args);
        return;  // Error formatting
    }

    // Allocate message from arena
    char* message = (char*)arena_alloc(list->arena, size + 1, 1);
    vsnprintf(message, size + 1, fmt, args);
    va_end(args);

    // Add the error
    CompileError* error = &list->errors[list->count++];
    error->kind = kind;
    error->loc = loc;
    error->message = message;
    error->suggestion = NULL;  // Can be set separately if needed
}

void error_list_print(ErrorList* list, FILE* out) {
    assert(list != NULL);
    assert(out != NULL);

    if (list->count == 0) {
        return;
    }

    // Check if output is a TTY for color support
    // For simplicity, we'll just use colors - in production we'd check isatty()
    bool use_color = true;

    for (size_t i = 0; i < list->count; i++) {
        CompileError* error = &list->errors[i];

        // Print error header with kind and location
        if (use_color) {
            fprintf(out, "%s%s%s: ", COLOR_BOLD COLOR_RED,
                    error_kind_names[error->kind], COLOR_RESET);
        } else {
            fprintf(out, "%s: ", error_kind_names[error->kind]);
        }

        // Print location
        if (error->loc.filename) {
            if (use_color) {
                fprintf(out, "%s%s:%u:%u%s\n",
                        COLOR_CYAN,
                        error->loc.filename,
                        error->loc.line,
                        error->loc.column,
                        COLOR_RESET);
            } else {
                fprintf(out, "%s:%u:%u\n",
                        error->loc.filename,
                        error->loc.line,
                        error->loc.column);
            }
        }

        // Print message
        fprintf(out, "  %s\n", error->message);

        // Print suggestion if available
        if (error->suggestion) {
            if (use_color) {
                fprintf(out, "  %sHelp:%s %s\n",
                        COLOR_BOLD COLOR_YELLOW,
                        COLOR_RESET,
                        error->suggestion);
            } else {
                fprintf(out, "  Help: %s\n", error->suggestion);
            }
        }

        fprintf(out, "\n");
    }

    // Print summary
    if (use_color) {
        fprintf(out, "%s%zu error(s) generated.%s\n",
                COLOR_BOLD COLOR_RED, list->count, COLOR_RESET);
    } else {
        fprintf(out, "%zu error(s) generated.\n", list->count);
    }
}

bool error_list_has_errors(ErrorList* list) {
    assert(list != NULL);
    return list->count > 0;
}
