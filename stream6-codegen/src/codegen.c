#include "../include/codegen.h"
#include "../include/ir.h"
#include "../include/type.h"
#include "../include/coro_metadata.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// Helper macros for indentation
#define INDENT(ctx) for(int i = 0; i < (ctx)->indent_level; i++) fprintf((ctx)->source_out, "    ")
#define INDENT_H(ctx) for(int i = 0; i < (ctx)->indent_level; i++) fprintf((ctx)->header_out, "    ")

// Forward declarations
static void emit_node(IrNode* node, CodegenContext* ctx, bool is_expr);
static void emit_statement(IrNode* node, CodegenContext* ctx);
static void emit_expression(IrNode* node, CodegenContext* ctx);
static void emit_function(IrNode* node, CodegenContext* ctx);
static void emit_state_machine(IrNode* node, CodegenContext* ctx);
static void emit_line_directive(SourceLocation loc, CodegenContext* ctx);

// Initialize codegen context
void codegen_init(CodegenContext* ctx, Arena* arena, ErrorList* errors,
                  const char* module_name) {
    ctx->arena = arena;
    ctx->errors = errors;
    ctx->module_name = module_name;
    ctx->emit_line_directives = true;
    ctx->indent_level = 0;
    ctx->header_out = NULL;
    ctx->source_out = NULL;
}

// Generate prefixed identifier to avoid name collisions
const char* codegen_prefix_identifier(const char* name, Arena* arena) {
    if (!name) return NULL;

    // Check if already prefixed
    if (strncmp(name, "__u_", 4) == 0) {
        return name;
    }

    // Allocate space for "__u_" + name + null terminator
    size_t len = strlen(name) + 5;
    char* prefixed = (char*)arena_alloc(arena, len, 1);
    snprintf(prefixed, len, "__u_%s", name);
    return prefixed;
}

// Translate language types to C types
const char* codegen_type_to_c(Type* type, Arena* arena) {
    if (!type) return "void";

    switch (type->kind) {
        case TYPE_I8:    return "int8_t";
        case TYPE_I16:   return "int16_t";
        case TYPE_I32:   return "int32_t";
        case TYPE_I64:   return "int64_t";
        case TYPE_ISIZE: return "ptrdiff_t";
        case TYPE_U8:    return "uint8_t";
        case TYPE_U16:   return "uint16_t";
        case TYPE_U32:   return "uint32_t";
        case TYPE_U64:   return "uint64_t";
        case TYPE_USIZE: return "size_t";
        case TYPE_BOOL:  return "bool";
        case TYPE_VOID:  return "void";

        case TYPE_POINTER: {
            const char* pointee = codegen_type_to_c(type->data.pointer.pointee_type, arena);
            size_t len = strlen(pointee) + 3;  // +2 for "* " and +1 for null
            char* ptr_type = (char*)arena_alloc(arena, len, 1);
            snprintf(ptr_type, len, "%s*", pointee);
            return ptr_type;
        }

        case TYPE_ARRAY: {
            const char* elem = codegen_type_to_c(type->data.array.elem_type, arena);
            size_t len = strlen(elem) + 32;  // Space for brackets and size
            char* arr_type = (char*)arena_alloc(arena, len, 1);
            snprintf(arr_type, len, "%s", elem);  // Will add [size] at declaration site
            return arr_type;
        }

        case TYPE_STRUCT: {
            const char* prefixed = codegen_prefix_identifier(type->data.struct_type.name, arena);
            size_t len = strlen(prefixed) + 8;
            char* struct_type = (char*)arena_alloc(arena, len, 1);
            snprintf(struct_type, len, "struct %s", prefixed);
            return struct_type;
        }

        case TYPE_ENUM: {
            const char* prefixed = codegen_prefix_identifier(type->data.enum_type.name, arena);
            size_t len = strlen(prefixed) + 6;
            char* enum_type = (char*)arena_alloc(arena, len, 1);
            snprintf(enum_type, len, "enum %s", prefixed);
            return enum_type;
        }

        default:
            return "void /* unsupported type */";
    }
}

// Emit #line directive for source mapping
static void emit_line_directive(SourceLocation loc, CodegenContext* ctx) {
    if (!ctx->emit_line_directives || !ctx->source_out) return;
    if (!loc.filename) return;

    fprintf(ctx->source_out, "#line %u \"%s\"\n", loc.line, loc.filename);
}

// Emit a single node (statement or expression)
// Currently unused, but kept for future expansion
__attribute__((unused))
static void emit_node(IrNode* node, CodegenContext* ctx, bool is_expr) {
    if (is_expr) {
        emit_expression(node, ctx);
    } else {
        emit_statement(node, ctx);
    }
}

// Emit an expression (returns a value)
static void emit_expression(IrNode* node, CodegenContext* ctx) {
    if (!node || !ctx->source_out) return;

    switch (node->kind) {
        case IR_LITERAL:
            fprintf(ctx->source_out, "%s", node->data.literal.literal);
            break;

        case IR_VAR_REF: {
            const char* prefixed = codegen_prefix_identifier(node->data.var_ref.var_name, ctx->arena);
            fprintf(ctx->source_out, "%s", prefixed);
            break;
        }

        case IR_BINARY_OP:
            fprintf(ctx->source_out, "(");
            emit_expression(node->data.binary_op.left, ctx);
            fprintf(ctx->source_out, " %s ", node->data.binary_op.op);
            emit_expression(node->data.binary_op.right, ctx);
            fprintf(ctx->source_out, ")");
            break;

        case IR_CALL: {
            const char* prefixed = codegen_prefix_identifier(node->data.call.function_name, ctx->arena);
            fprintf(ctx->source_out, "%s(", prefixed);
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                if (i > 0) fprintf(ctx->source_out, ", ");
                emit_expression(node->data.call.args[i], ctx);
            }
            fprintf(ctx->source_out, ")");
            break;
        }

        default:
            fprintf(ctx->source_out, "/* unsupported expression */");
            break;
    }
}

// Emit a statement
static void emit_statement(IrNode* node, CodegenContext* ctx) {
    if (!node || !ctx->source_out) return;

    emit_line_directive(node->loc, ctx);

    switch (node->kind) {
        case IR_ASSIGN:
            INDENT(ctx);
            fprintf(ctx->source_out, "%s = ",
                    codegen_prefix_identifier(node->data.assign.target, ctx->arena));
            emit_expression(node->data.assign.value, ctx);
            fprintf(ctx->source_out, ";\n");
            break;

        case IR_RETURN:
            INDENT(ctx);
            fprintf(ctx->source_out, "return");
            if (node->data.return_stmt.value) {
                fprintf(ctx->source_out, " ");
                emit_expression(node->data.return_stmt.value, ctx);
            }
            fprintf(ctx->source_out, ";\n");
            break;

        case IR_IF:
            INDENT(ctx);
            fprintf(ctx->source_out, "if (");
            emit_expression(node->data.if_stmt.condition, ctx);
            fprintf(ctx->source_out, ") {\n");
            ctx->indent_level++;
            emit_statement(node->data.if_stmt.then_block, ctx);
            ctx->indent_level--;
            if (node->data.if_stmt.else_block) {
                INDENT(ctx);
                fprintf(ctx->source_out, "} else {\n");
                ctx->indent_level++;
                emit_statement(node->data.if_stmt.else_block, ctx);
                ctx->indent_level--;
            }
            INDENT(ctx);
            fprintf(ctx->source_out, "}\n");
            break;

        case IR_JUMP:
            INDENT(ctx);
            fprintf(ctx->source_out, "goto %s;\n",
                    codegen_prefix_identifier(node->data.jump.label, ctx->arena));
            break;

        case IR_LABEL:
            fprintf(ctx->source_out, "%s:\n",
                    codegen_prefix_identifier(node->data.label.label, ctx->arena));
            break;

        case IR_BASIC_BLOCK:
            for (size_t i = 0; i < node->data.basic_block.instruction_count; i++) {
                emit_statement(node->data.basic_block.instructions[i], ctx);
            }
            break;

        case IR_CALL:
            INDENT(ctx);
            emit_expression(node, ctx);
            fprintf(ctx->source_out, ";\n");
            break;

        case IR_DEFER_REGION:
            // Emit main body first
            emit_statement(node->data.defer_region.body, ctx);
            // Then emit deferred statements in reverse order
            for (int i = (int)node->data.defer_region.deferred_count - 1; i >= 0; i--) {
                emit_statement(node->data.defer_region.deferred_stmts[i], ctx);
            }
            break;

        default:
            INDENT(ctx);
            fprintf(ctx->source_out, "/* unsupported statement kind: %d */\n", node->kind);
            break;
    }
}

// Emit a regular function
static void emit_function(IrNode* node, CodegenContext* ctx) {
    if (!node || node->kind != IR_FUNCTION) return;

    const char* func_name = codegen_prefix_identifier(node->data.function.name, ctx->arena);
    const char* return_type = codegen_type_to_c(node->data.function.return_type, ctx->arena);

    // Function declaration in header
    if (ctx->header_out) {
        fprintf(ctx->header_out, "%s %s(", return_type, func_name);
        for (size_t i = 0; i < node->data.function.param_count; i++) {
            if (i > 0) fprintf(ctx->header_out, ", ");
            IrParam* param = &node->data.function.params[i];
            const char* param_type = codegen_type_to_c(param->type, ctx->arena);
            const char* param_name = codegen_prefix_identifier(param->name, ctx->arena);
            fprintf(ctx->header_out, "%s %s", param_type, param_name);
        }
        if (node->data.function.param_count == 0) {
            fprintf(ctx->header_out, "void");
        }
        fprintf(ctx->header_out, ");\n\n");
    }

    // Function definition in source
    if (ctx->source_out) {
        emit_line_directive(node->loc, ctx);
        fprintf(ctx->source_out, "%s %s(", return_type, func_name);
        for (size_t i = 0; i < node->data.function.param_count; i++) {
            if (i > 0) fprintf(ctx->source_out, ", ");
            IrParam* param = &node->data.function.params[i];
            const char* param_type = codegen_type_to_c(param->type, ctx->arena);
            const char* param_name = codegen_prefix_identifier(param->name, ctx->arena);
            fprintf(ctx->source_out, "%s %s", param_type, param_name);
        }
        if (node->data.function.param_count == 0) {
            fprintf(ctx->source_out, "void");
        }
        fprintf(ctx->source_out, ") {\n");

        ctx->indent_level++;

        // Check if this is a state machine function
        if (node->data.function.is_state_machine && node->data.function.coro_meta) {
            emit_state_machine(node, ctx);
        } else {
            // Regular function body
            emit_statement(node->data.function.body, ctx);
        }

        ctx->indent_level--;
        fprintf(ctx->source_out, "}\n\n");
    }
}

// Emit state machine using computed goto
static void emit_state_machine(IrNode* node, CodegenContext* ctx) {
    if (!node->data.function.coro_meta) return;

    CoroMetadata* meta = node->data.function.coro_meta;

    // Generate state struct type definition
    INDENT(ctx);
    fprintf(ctx->source_out, "struct %s_state {\n",
            codegen_prefix_identifier(node->data.function.name, ctx->arena));
    ctx->indent_level++;

    INDENT(ctx);
    fprintf(ctx->source_out, "void* state;  /* Current state label */\n");

    // Generate union of all state structs
    INDENT(ctx);
    fprintf(ctx->source_out, "union {\n");
    ctx->indent_level++;

    for (size_t i = 0; i < meta->state_count; i++) {
        StateStruct* state = &meta->state_structs[i];
        INDENT(ctx);
        fprintf(ctx->source_out, "struct {\n");
        ctx->indent_level++;

        for (size_t j = 0; j < state->field_count; j++) {
            INDENT(ctx);
            const char* field_type = codegen_type_to_c(state->fields[j].type, ctx->arena);
            const char* field_name = codegen_prefix_identifier(state->fields[j].name, ctx->arena);
            fprintf(ctx->source_out, "%s %s;\n", field_type, field_name);
        }

        ctx->indent_level--;
        INDENT(ctx);
        fprintf(ctx->source_out, "} state_%u;\n", state->state_id);
    }

    ctx->indent_level--;
    INDENT(ctx);
    fprintf(ctx->source_out, "} data;\n");

    ctx->indent_level--;
    INDENT(ctx);
    fprintf(ctx->source_out, "} machine;\n\n");

    // Initialize machine state
    INDENT(ctx);
    fprintf(ctx->source_out, "machine.state = &&%s_state_0;\n",
            codegen_prefix_identifier(node->data.function.name, ctx->arena));

    // Computed goto dispatcher
    INDENT(ctx);
    fprintf(ctx->source_out, "goto *machine.state;\n\n");

    // Generate state labels and code
    for (size_t i = 0; i < meta->suspend_count; i++) {
        SuspendPoint* sp = &meta->suspend_points[i];
        const char* label = codegen_prefix_identifier(node->data.function.name, ctx->arena);
        fprintf(ctx->source_out, "%s_state_%u:\n", label, sp->state_id);

        ctx->indent_level++;
        INDENT(ctx);
        fprintf(ctx->source_out, "/* State %u: %u live variables */\n",
                sp->state_id, (unsigned)sp->live_var_count);

        // Restore live variables from state struct
        for (size_t j = 0; j < sp->live_var_count; j++) {
            VarLiveness* var = &sp->live_vars[j];
            if (var->is_live) {
                INDENT(ctx);
                const char* var_name = codegen_prefix_identifier(var->var_name, ctx->arena);
                fprintf(ctx->source_out, "%s = machine.data.state_%u.%s;\n",
                        var_name, sp->state_id, var_name);
            }
        }

        INDENT(ctx);
        fprintf(ctx->source_out, "/* TODO: Resume execution */\n");
        ctx->indent_level--;
        fprintf(ctx->source_out, "\n");
    }
}

// Emit runtime header with common definitions
void codegen_emit_runtime_header(FILE* out) {
    fprintf(out, "/* Runtime support for async systems language */\n");
    fprintf(out, "#ifndef LANG_RUNTIME_H\n");
    fprintf(out, "#define LANG_RUNTIME_H\n\n");

    fprintf(out, "/* Freestanding C11 headers */\n");
    fprintf(out, "#include <stdint.h>\n");
    fprintf(out, "#include <stddef.h>\n");
    fprintf(out, "#include <stdbool.h>\n");
    fprintf(out, "#include <stdalign.h>\n\n");

    fprintf(out, "/* Result type support */\n");
    fprintf(out, "#define RESULT_OK(T) struct { bool is_ok; T value; }\n");
    fprintf(out, "#define RESULT_ERR(E) struct { bool is_ok; E error; }\n\n");

    fprintf(out, "/* Option type support */\n");
    fprintf(out, "#define OPTION(T) struct { bool has_value; T value; }\n\n");

    fprintf(out, "/* Coroutine support */\n");
    fprintf(out, "typedef struct {\n");
    fprintf(out, "    void* state;\n");
    fprintf(out, "    void* data;\n");
    fprintf(out, "} __u_Coroutine;\n\n");

    fprintf(out, "#endif /* LANG_RUNTIME_H */\n");
}

// Main entry point: emit entire module
void codegen_emit_module(IrNode* module, CodegenContext* ctx) {
    if (!module || module->kind != IR_MODULE) {
        if (ctx->errors) {
            error_list_add(ctx->errors, ERROR_TYPE,
                          (SourceLocation){0, 0, "codegen"},
                          "Expected module node, got something else");
        }
        return;
    }

    // Emit header guard and includes for header file
    if (ctx->header_out) {
        fprintf(ctx->header_out, "/* Generated code - do not edit */\n");
        fprintf(ctx->header_out, "#ifndef %s_H\n",
                codegen_prefix_identifier(ctx->module_name, ctx->arena));
        fprintf(ctx->header_out, "#define %s_H\n\n",
                codegen_prefix_identifier(ctx->module_name, ctx->arena));
        fprintf(ctx->header_out, "#include \"lang_runtime.h\"\n\n");
    }

    // Emit includes for source file
    if (ctx->source_out) {
        fprintf(ctx->source_out, "/* Generated code - do not edit */\n");
        fprintf(ctx->source_out, "#include \"%s.h\"\n\n", ctx->module_name);
    }

    // Emit all functions
    for (size_t i = 0; i < module->data.module.function_count; i++) {
        emit_function(module->data.module.functions[i], ctx);
    }

    // Close header guard
    if (ctx->header_out) {
        fprintf(ctx->header_out, "#endif /* %s_H */\n",
                codegen_prefix_identifier(ctx->module_name, ctx->arena));
    }
}
