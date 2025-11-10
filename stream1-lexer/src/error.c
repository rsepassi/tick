#include "../include/error.h"
#include "../include/arena.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

void error_list_init(ErrorList* list, Arena* arena) {
    list->errors = NULL;
    list->count = 0;
    list->capacity = 16;

    list->errors = (CompileError*)arena_alloc(arena,
                                               sizeof(CompileError) * list->capacity,
                                               sizeof(CompileError));
}

void error_list_add(ErrorList* list, ErrorKind kind, SourceLocation loc,
                    const char* fmt, ...) {
    if (list->count >= list->capacity) {
        // For simplicity, just ignore - in real implementation would grow
        return;
    }

    CompileError* err = &list->errors[list->count++];
    err->kind = kind;
    err->loc = loc;
    err->suggestion = NULL;

    // Format the message
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    // For simplicity, we'll just store a pointer to a static copy
    // In a real implementation, we'd use the arena
    size_t len = strlen(buffer);
    err->message = (char*)malloc(len + 1);
    strcpy(err->message, buffer);
}

void error_list_print(ErrorList* list, FILE* out) {
    for (size_t i = 0; i < list->count; i++) {
        CompileError* err = &list->errors[i];
        const char* kind_str;

        switch (err->kind) {
            case ERROR_LEXICAL: kind_str = "Lexical"; break;
            case ERROR_SYNTAX: kind_str = "Syntax"; break;
            case ERROR_TYPE: kind_str = "Type"; break;
            case ERROR_RESOLUTION: kind_str = "Resolution"; break;
            case ERROR_COROUTINE: kind_str = "Coroutine"; break;
            default: kind_str = "Unknown"; break;
        }

        fprintf(out, "%s:%u:%u: %s error: %s\n",
                err->loc.filename, err->loc.line, err->loc.column,
                kind_str, err->message);

        if (err->suggestion) {
            fprintf(out, "  Suggestion: %s\n", err->suggestion);
        }
    }
}

bool error_list_has_errors(ErrorList* list) {
    return list->count > 0;
}
