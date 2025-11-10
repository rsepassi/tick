#ifndef IR_H
#define IR_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "type.h"
#include "error.h"
#include "coro_metadata.h"

// IR Interface
// Purpose: Contract between lowering and code generation

typedef enum {
    IR_FUNCTION,
    IR_BASIC_BLOCK,
    IR_ASSIGN,
    IR_BINARY_OP,
    IR_CALL,
    IR_RETURN,
    IR_IF,
    IR_SWITCH,
    IR_FOR_LOOP,
    IR_JUMP,
    IR_LABEL,
    IR_STATE_MACHINE,  // For coroutines
    IR_SUSPEND,
    IR_RESUME,
    IR_DEFER_REGION,
    IR_LITERAL,
    IR_VAR_REF,
    IR_MODULE,  // Top-level module containing functions
} IrNodeKind;

typedef struct IrParam {
    const char* name;
    Type* type;
} IrParam;

typedef struct IrNode {
    IrNodeKind kind;
    Type* type;
    SourceLocation loc;  // For #line directives

    union {
        struct {
            const char* name;
            Type* return_type;
            struct IrParam* params;
            size_t param_count;
            struct IrNode* body;
            bool is_state_machine;
            struct CoroMetadata* coro_meta;  // If state machine
        } function;

        struct {
            uint32_t state_id;
            struct IrNode** instructions;
            size_t instruction_count;
        } basic_block;

        struct {
            const char* target;
            struct IrNode* value;
        } assign;

        struct {
            const char* op;  // "+", "-", "*", "/", etc.
            struct IrNode* left;
            struct IrNode* right;
        } binary_op;

        struct {
            const char* function_name;
            struct IrNode** args;
            size_t arg_count;
            bool is_async_call;  // Needs await/suspend
        } call;

        struct {
            struct IrNode* value;  // NULL for void return
        } return_stmt;

        struct {
            struct IrNode* condition;
            struct IrNode* then_block;
            struct IrNode* else_block;  // NULL if no else
        } if_stmt;

        struct {
            struct IrNode* value;
            struct IrNode** cases;
            size_t case_count;
            struct IrNode* default_case;  // NULL if no default
        } switch_stmt;

        struct {
            const char* label;
        } jump;

        struct {
            const char* label;
        } label;

        struct {
            uint32_t state_id;
            struct IrNode** state_blocks;
            size_t block_count;
            size_t frame_size;
            const char* state_struct_name;
        } state_machine;

        struct {
            uint32_t resume_state;
            struct IrNode* await_expr;  // Expression being awaited
        } suspend;

        struct {
            const char* continuation_name;
        } resume;

        struct {
            struct IrNode** deferred_stmts;
            size_t deferred_count;
            struct IrNode* body;
        } defer_region;

        struct {
            const char* literal;  // For literals
        } literal;

        struct {
            const char* var_name;
        } var_ref;

        struct {
            const char* name;
            struct IrNode** functions;
            size_t function_count;
        } module;
    } data;
} IrNode;

// Forward declare Arena
typedef struct Arena Arena;

// Lowering allocates IR from arena
IrNode* ir_alloc_node(IrNodeKind kind, Arena* arena);

// Codegen consumes
void codegen_emit(IrNode* ir, FILE* out);

#endif // IR_H
