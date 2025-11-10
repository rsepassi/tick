#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include <stdbool.h>
#include "ir.h"
#include "arena.h"
#include "error.h"

// Code Generation Interface
// Purpose: Generate freestanding C11 code from lowered IR

// Output configuration
typedef struct CodegenContext {
    Arena* arena;
    ErrorList* errors;
    const char* module_name;
    bool emit_line_directives;
    FILE* header_out;   // For .h file
    FILE* source_out;   // For .c file
    int indent_level;
    IrFunction* current_function;  // Track current function for error propagation
} CodegenContext;

// Initialize context
void codegen_init(CodegenContext* ctx, Arena* arena, ErrorList* errors,
                  const char* module_name);

// Main entry points
void codegen_emit_module(IrNode* module, CodegenContext* ctx);
void codegen_emit_runtime_header(FILE* out);

// Generate prefixed identifier
const char* codegen_prefix_identifier(const char* name, Arena* arena);

// Type translation
const char* codegen_type_to_c(Type* type, Arena* arena);

#endif // CODEGEN_H
