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
            struct IrNode* instructions;
            size_t instruction_count;
        } basic_block;

        // ... etc
    } data;
} IrNode;

// Forward declare Arena
typedef struct Arena Arena;

// Lowering allocates IR from arena
IrNode* ir_alloc_node(IrNodeKind kind, Arena* arena);

// Codegen consumes
void codegen_emit(IrNode* ir, FILE* out);

#endif // IR_H
