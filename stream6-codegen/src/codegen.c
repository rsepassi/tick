#include "../include/codegen.h"
#include "ir.h"
#include "type.h"
#include "coro_metadata.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// Helper macros for indentation
#define INDENT(ctx) for(int i = 0; i < (ctx)->indent_level; i++) fprintf((ctx)->source_out, "    ")
#define INDENT_H(ctx) for(int i = 0; i < (ctx)->indent_level; i++) fprintf((ctx)->header_out, "    ")

// Forward declarations
static void emit_function(IrFunction* func, CodegenContext* ctx);
static void emit_basic_block(IrBasicBlock* block, CodegenContext* ctx);
static void emit_instruction(IrInstruction* instr, CodegenContext* ctx);
static void emit_value(IrValue* value, CodegenContext* ctx);
static void emit_state_machine(IrFunction* func, CodegenContext* ctx);
static void emit_line_directive(SourceLocation loc, CodegenContext* ctx);
static const char* binary_op_to_c(IrBinaryOp op);
static const char* unary_op_to_c(IrUnaryOp op);

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

// Binary operator to C string
static const char* binary_op_to_c(IrBinaryOp op) {
    switch (op) {
        case IR_OP_ADD: return "+";
        case IR_OP_SUB: return "-";
        case IR_OP_MUL: return "*";
        case IR_OP_DIV: return "/";
        case IR_OP_MOD: return "%";
        case IR_OP_AND: return "&";
        case IR_OP_OR:  return "|";
        case IR_OP_XOR: return "^";
        case IR_OP_SHL: return "<<";
        case IR_OP_SHR: return ">>";
        case IR_OP_EQ:  return "==";
        case IR_OP_NE:  return "!=";
        case IR_OP_LT:  return "<";
        case IR_OP_LE:  return "<=";
        case IR_OP_GT:  return ">";
        case IR_OP_GE:  return ">=";
        case IR_OP_LOGICAL_AND: return "&&";
        case IR_OP_LOGICAL_OR:  return "||";
        default: return "?";
    }
}

// Unary operator to C string
static const char* unary_op_to_c(IrUnaryOp op) {
    switch (op) {
        case IR_OP_NEG: return "-";
        case IR_OP_NOT: return "~";
        case IR_OP_DEREF: return "*";
        case IR_OP_ADDR_OF: return "&";
        case IR_OP_LOGICAL_NOT: return "!";
        default: return "?";
    }
}

// Emit a value (temporary, constant, parameter, etc.)
static void emit_value(IrValue* value, CodegenContext* ctx) {
    if (!value || !ctx->source_out) return;

    switch (value->kind) {
        case IR_VALUE_TEMP:
            if (value->data.temp.name) {
                fprintf(ctx->source_out, "%s",
                       codegen_prefix_identifier(value->data.temp.name, ctx->arena));
            } else {
                fprintf(ctx->source_out, "t%u", value->data.temp.id);
            }
            break;

        case IR_VALUE_CONSTANT:
            // Emit constant based on type
            if (value->type && value->type->kind == TYPE_BOOL) {
                fprintf(ctx->source_out, "%s", value->data.constant.data.bool_val ? "true" : "false");
            } else if (value->type && (value->type->kind >= TYPE_I8 && value->type->kind <= TYPE_ISIZE)) {
                fprintf(ctx->source_out, "%lld", (long long)value->data.constant.data.int_val);
            } else if (value->type && (value->type->kind >= TYPE_U8 && value->type->kind <= TYPE_USIZE)) {
                fprintf(ctx->source_out, "%llu", (unsigned long long)value->data.constant.data.uint_val);
            } else {
                fprintf(ctx->source_out, "%s",
                       value->data.constant.data.str_val ? value->data.constant.data.str_val : "0");
            }
            break;

        case IR_VALUE_PARAM:
            fprintf(ctx->source_out, "%s",
                   codegen_prefix_identifier(value->data.param.name, ctx->arena));
            break;

        case IR_VALUE_GLOBAL:
            fprintf(ctx->source_out, "%s",
                   codegen_prefix_identifier(value->data.global.name, ctx->arena));
            break;

        case IR_VALUE_NULL:
            fprintf(ctx->source_out, "NULL");
            break;

        default:
            fprintf(ctx->source_out, "/* unknown value */");
            break;
    }
}

// Emit an instruction
static void emit_instruction(IrInstruction* instr, CodegenContext* ctx) {
    if (!instr || !ctx->source_out) return;

    emit_line_directive(instr->loc, ctx);

    switch (instr->kind) {
        case IR_ASSIGN:
            INDENT(ctx);
            emit_value(instr->data.assign.dest, ctx);
            fprintf(ctx->source_out, " = ");
            emit_value(instr->data.assign.src, ctx);
            fprintf(ctx->source_out, ";\n");
            break;

        case IR_BINARY_OP:
            INDENT(ctx);
            emit_value(instr->data.binary_op.dest, ctx);
            fprintf(ctx->source_out, " = ");
            emit_value(instr->data.binary_op.left, ctx);
            fprintf(ctx->source_out, " %s ", binary_op_to_c(instr->data.binary_op.op));
            emit_value(instr->data.binary_op.right, ctx);
            fprintf(ctx->source_out, ";\n");
            break;

        case IR_UNARY_OP:
            INDENT(ctx);
            emit_value(instr->data.unary_op.dest, ctx);
            fprintf(ctx->source_out, " = %s", unary_op_to_c(instr->data.unary_op.op));
            emit_value(instr->data.unary_op.operand, ctx);
            fprintf(ctx->source_out, ";\n");
            break;

        case IR_CALL:
        case IR_ASYNC_CALL:
            INDENT(ctx);
            if (instr->data.call.dest) {
                emit_value(instr->data.call.dest, ctx);
                fprintf(ctx->source_out, " = ");
            }
            emit_value(instr->data.call.func, ctx);
            fprintf(ctx->source_out, "(");
            for (size_t i = 0; i < instr->data.call.arg_count; i++) {
                if (i > 0) fprintf(ctx->source_out, ", ");
                emit_value(instr->data.call.args[i], ctx);
            }
            fprintf(ctx->source_out, ");\n");
            break;

        case IR_RETURN:
            INDENT(ctx);
            fprintf(ctx->source_out, "return");
            if (instr->data.ret.value) {
                fprintf(ctx->source_out, " ");
                emit_value(instr->data.ret.value, ctx);
            }
            fprintf(ctx->source_out, ";\n");
            break;

        case IR_JUMP:
            INDENT(ctx);
            fprintf(ctx->source_out, "goto %s;\n",
                   codegen_prefix_identifier(instr->data.jump.target->label, ctx->arena));
            break;

        case IR_COND_JUMP:
            INDENT(ctx);
            fprintf(ctx->source_out, "if (");
            emit_value(instr->data.cond_jump.cond, ctx);
            fprintf(ctx->source_out, ") goto %s; else goto %s;\n",
                   codegen_prefix_identifier(instr->data.cond_jump.true_target->label, ctx->arena),
                   codegen_prefix_identifier(instr->data.cond_jump.false_target->label, ctx->arena));
            break;

        case IR_LOAD:
            INDENT(ctx);
            emit_value(instr->data.load.dest, ctx);
            fprintf(ctx->source_out, " = *");
            emit_value(instr->data.load.addr, ctx);
            fprintf(ctx->source_out, ";\n");
            break;

        case IR_STORE:
            INDENT(ctx);
            fprintf(ctx->source_out, "*");
            emit_value(instr->data.store.addr, ctx);
            fprintf(ctx->source_out, " = ");
            emit_value(instr->data.store.value, ctx);
            fprintf(ctx->source_out, ";\n");
            break;

        case IR_ALLOCA:
            INDENT(ctx);
            emit_value(instr->data.alloca.dest, ctx);
            fprintf(ctx->source_out, " = alloca(sizeof(%s)",
                   codegen_type_to_c(instr->data.alloca.alloc_type, ctx->arena));
            if (instr->data.alloca.count > 1) {
                fprintf(ctx->source_out, " * %zu", instr->data.alloca.count);
            }
            fprintf(ctx->source_out, ");\n");
            break;

        case IR_GET_FIELD:
            INDENT(ctx);
            emit_value(instr->data.get_field.dest, ctx);
            fprintf(ctx->source_out, " = ");
            emit_value(instr->data.get_field.base, ctx);
            fprintf(ctx->source_out, ".%s;\n",
                   codegen_prefix_identifier(instr->data.get_field.field_name, ctx->arena));
            break;

        case IR_GET_INDEX:
            INDENT(ctx);
            emit_value(instr->data.get_index.dest, ctx);
            fprintf(ctx->source_out, " = ");
            emit_value(instr->data.get_index.base, ctx);
            fprintf(ctx->source_out, "[");
            emit_value(instr->data.get_index.index, ctx);
            fprintf(ctx->source_out, "];\n");
            break;

        case IR_CAST:
            INDENT(ctx);
            emit_value(instr->data.cast.dest, ctx);
            fprintf(ctx->source_out, " = (%s)",
                   codegen_type_to_c(instr->data.cast.target_type, ctx->arena));
            emit_value(instr->data.cast.value, ctx);
            fprintf(ctx->source_out, ";\n");
            break;

        default:
            INDENT(ctx);
            fprintf(ctx->source_out, "/* unsupported instruction kind: %d */\n", instr->kind);
            break;
    }
}

// Emit a basic block
static void emit_basic_block(IrBasicBlock* block, CodegenContext* ctx) {
    if (!block || !ctx->source_out) return;

    // Emit label if present and block has predecessors (needs to be jumped to)
    if (block->label && block->predecessor_count > 0) {
        fprintf(ctx->source_out, "%s:\n",
               codegen_prefix_identifier(block->label, ctx->arena));
    }

    // Emit all instructions in the block
    for (size_t i = 0; i < block->instruction_count; i++) {
        emit_instruction(block->instructions[i], ctx);
    }
}

// Emit state machine using computed goto
static void emit_state_machine(IrFunction* func, CodegenContext* ctx) {
    if (!func->coro_meta) return;

    CoroMetadata* meta = func->coro_meta;

    // Generate state struct type definition
    INDENT(ctx);
    fprintf(ctx->source_out, "struct %s_state {\n",
           codegen_prefix_identifier(func->name, ctx->arena));
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
           codegen_prefix_identifier(func->name, ctx->arena));

    // Computed goto dispatcher
    INDENT(ctx);
    fprintf(ctx->source_out, "goto *machine.state;\n\n");

    // Generate state labels and code
    for (size_t i = 0; i < meta->suspend_count; i++) {
        SuspendPoint* sp = &meta->suspend_points[i];
        const char* label = codegen_prefix_identifier(func->name, ctx->arena);
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

// Emit a function
static void emit_function(IrFunction* func, CodegenContext* ctx) {
    if (!func) return;

    const char* func_name = codegen_prefix_identifier(func->name, ctx->arena);
    const char* return_type = codegen_type_to_c(func->return_type, ctx->arena);

    // Function declaration in header
    if (ctx->header_out) {
        fprintf(ctx->header_out, "%s %s(", return_type, func_name);
        for (size_t i = 0; i < func->param_count; i++) {
            if (i > 0) fprintf(ctx->header_out, ", ");
            IrParam* param = &func->params[i];
            const char* param_type = codegen_type_to_c(param->type, ctx->arena);
            const char* param_name = codegen_prefix_identifier(param->name, ctx->arena);
            fprintf(ctx->header_out, "%s %s", param_type, param_name);
        }
        if (func->param_count == 0) {
            fprintf(ctx->header_out, "void");
        }
        fprintf(ctx->header_out, ");\n\n");
    }

    // Function definition in source
    if (ctx->source_out) {
        fprintf(ctx->source_out, "%s %s(", return_type, func_name);
        for (size_t i = 0; i < func->param_count; i++) {
            if (i > 0) fprintf(ctx->source_out, ", ");
            IrParam* param = &func->params[i];
            const char* param_type = codegen_type_to_c(param->type, ctx->arena);
            const char* param_name = codegen_prefix_identifier(param->name, ctx->arena);
            fprintf(ctx->source_out, "%s %s", param_type, param_name);
        }
        if (func->param_count == 0) {
            fprintf(ctx->source_out, "void");
        }
        fprintf(ctx->source_out, ") {\n");

        ctx->indent_level++;

        // Check if this is a state machine function
        if (func->is_state_machine && func->coro_meta) {
            emit_state_machine(func, ctx);
        } else {
            // Emit all basic blocks
            for (size_t i = 0; i < func->block_count; i++) {
                emit_basic_block(func->blocks[i], ctx);
            }
        }

        ctx->indent_level--;
        fprintf(ctx->source_out, "}\n\n");
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

    IrModule* mod = module->data.module;
    if (!mod) return;

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
    for (size_t i = 0; i < mod->function_count; i++) {
        emit_function(mod->functions[i], ctx);
    }

    // Close header guard
    if (ctx->header_out) {
        fprintf(ctx->header_out, "#endif /* %s_H */\n",
               codegen_prefix_identifier(ctx->module_name, ctx->arena));
    }
}
