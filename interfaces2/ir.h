#ifndef IR_H
#define IR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "type.h"
#include "error.h"
#include "coro_metadata.h"

// IR Interface
// Purpose: Contract between lowering and code generation
// Design: Structured IR with explicit temporaries (three-address code style)

// Forward declarations
typedef struct IrNode IrNode;
typedef struct IrValue IrValue;
typedef struct IrInstruction IrInstruction;
typedef struct IrBasicBlock IrBasicBlock;

// IR node types
typedef enum {
    // Top-level
    IR_MODULE,
    IR_FUNCTION,

    // Basic blocks and control flow
    IR_BASIC_BLOCK,
    IR_IF,
    IR_SWITCH,
    IR_LOOP,
    IR_LABEL,
    IR_JUMP,
    IR_COND_JUMP,
    IR_RETURN,

    // Instructions
    IR_ASSIGN,
    IR_BINARY_OP,
    IR_UNARY_OP,
    IR_CALL,
    IR_ASYNC_CALL,
    IR_LOAD,
    IR_STORE,
    IR_ALLOCA,
    IR_GET_FIELD,
    IR_GET_INDEX,
    IR_CAST,
    IR_PHI,  // For SSA-like representation

    // Coroutine operations
    IR_STATE_MACHINE,
    IR_SUSPEND,
    IR_RESUME,
    IR_STATE_SAVE,
    IR_STATE_RESTORE,

    // Cleanup and error handling
    IR_DEFER_REGION,
    IR_ERRDEFER_REGION,
    IR_TRY_BEGIN,
    IR_TRY_END,
    IR_ERROR_CHECK,
    IR_ERROR_PROPAGATE,

    // Values
    IR_TEMP,
    IR_CONSTANT,
    IR_PARAM,
    IR_GLOBAL,
} IrNodeKind;

// Binary operators
typedef enum {
    IR_OP_ADD, IR_OP_SUB, IR_OP_MUL, IR_OP_DIV, IR_OP_MOD,
    IR_OP_AND, IR_OP_OR, IR_OP_XOR,
    IR_OP_SHL, IR_OP_SHR,
    IR_OP_EQ, IR_OP_NE, IR_OP_LT, IR_OP_LE, IR_OP_GT, IR_OP_GE,
    IR_OP_LOGICAL_AND, IR_OP_LOGICAL_OR,
} IrBinaryOp;

// Unary operators
typedef enum {
    IR_OP_NEG, IR_OP_NOT, IR_OP_DEREF, IR_OP_ADDR_OF, IR_OP_LOGICAL_NOT,
} IrUnaryOp;

// IR Value representation
typedef enum {
    IR_VALUE_TEMP,      // Temporary variable
    IR_VALUE_CONSTANT,  // Compile-time constant
    IR_VALUE_PARAM,     // Function parameter
    IR_VALUE_GLOBAL,    // Global variable
    IR_VALUE_NULL,      // Null/undefined
} IrValueKind;

struct IrValue {
    IrValueKind kind;
    Type* type;

    union {
        struct {
            uint32_t id;  // Unique temp ID
            const char* name;  // Optional debug name
        } temp;

        struct {
            union {
                int64_t int_val;
                uint64_t uint_val;
                double float_val;
                bool bool_val;
                const char* str_val;
            } data;
        } constant;

        struct {
            uint32_t index;
            const char* name;
        } param;

        struct {
            const char* name;
        } global;
    } data;
};

// Function parameter
typedef struct IrParam {
    const char* name;
    Type* type;
    uint32_t index;
} IrParam;

// Switch case
typedef struct IrSwitchCase {
    IrValue* value;  // Case value (NULL for default)
    IrBasicBlock* target;  // Target block
} IrSwitchCase;

// Defer/errdefer cleanup
typedef struct IrDeferCleanup {
    IrInstruction** instructions;
    size_t instruction_count;
    bool is_errdefer;  // true = errdefer, false = defer
} IrDeferCleanup;

// State machine state
typedef struct IrStateMachineState {
    uint32_t state_id;
    IrBasicBlock* block;
    StateStruct* state_struct;  // From coro metadata
} IrStateMachineState;

// Basic block
struct IrBasicBlock {
    uint32_t id;
    const char* label;
    IrInstruction** instructions;
    size_t instruction_count;
    size_t instruction_capacity;

    // Control flow
    IrBasicBlock** predecessors;
    size_t predecessor_count;
    IrBasicBlock** successors;
    size_t successor_count;
};

// IR Instruction
struct IrInstruction {
    IrNodeKind kind;
    Type* type;
    SourceLocation loc;

    union {
        // IR_ASSIGN: dest = src
        struct {
            IrValue* dest;
            IrValue* src;
        } assign;

        // IR_BINARY_OP: dest = left op right
        struct {
            IrValue* dest;
            IrBinaryOp op;
            IrValue* left;
            IrValue* right;
        } binary_op;

        // IR_UNARY_OP: dest = op operand
        struct {
            IrValue* dest;
            IrUnaryOp op;
            IrValue* operand;
        } unary_op;

        // IR_CALL: dest = func(args)
        struct {
            IrValue* dest;  // NULL for void
            IrValue* func;
            IrValue** args;
            size_t arg_count;
        } call;

        // IR_ASYNC_CALL: dest = await func(args)
        struct {
            IrValue* dest;
            IrValue* func;
            IrValue** args;
            size_t arg_count;
        } async_call;

        // IR_LOAD: dest = *addr
        struct {
            IrValue* dest;
            IrValue* addr;
        } load;

        // IR_STORE: *addr = value
        struct {
            IrValue* addr;
            IrValue* value;
        } store;

        // IR_ALLOCA: dest = alloca(type, count)
        struct {
            IrValue* dest;
            Type* alloc_type;
            size_t count;  // For arrays
        } alloca;

        // IR_GET_FIELD: dest = base.field
        struct {
            IrValue* dest;
            IrValue* base;
            uint32_t field_index;
            const char* field_name;
        } get_field;

        // IR_GET_INDEX: dest = base[index]
        struct {
            IrValue* dest;
            IrValue* base;
            IrValue* index;
        } get_index;

        // IR_CAST: dest = (type)value
        struct {
            IrValue* dest;
            Type* target_type;
            IrValue* value;
        } cast;

        // IR_PHI: dest = phi(values, blocks)
        struct {
            IrValue* dest;
            IrValue** values;
            IrBasicBlock** blocks;
            size_t value_count;
        } phi;

        // IR_JUMP: goto label
        struct {
            IrBasicBlock* target;
        } jump;

        // IR_COND_JUMP: if (cond) goto true_target else goto false_target
        struct {
            IrValue* cond;
            IrBasicBlock* true_target;
            IrBasicBlock* false_target;
        } cond_jump;

        // IR_RETURN: return value
        struct {
            IrValue* value;  // NULL for void
        } ret;

        // IR_SUSPEND: suspend(state_id)
        struct {
            uint32_t state_id;
            IrValue** live_vars;
            size_t live_var_count;
        } suspend;

        // IR_RESUME: resume(coro_handle, state_id)
        struct {
            IrValue* coro_handle;
            uint32_t state_id;
        } resume;

        // IR_STATE_SAVE: save live vars to coroutine frame
        struct {
            IrValue* frame;
            IrValue** vars;
            size_t var_count;
            uint32_t state_id;
        } state_save;

        // IR_STATE_RESTORE: restore live vars from coroutine frame
        struct {
            IrValue* frame;
            IrValue** vars;
            size_t var_count;
            uint32_t state_id;
        } state_restore;

        // IR_ERROR_CHECK: if (value is error) goto error_label
        struct {
            IrValue* value;
            IrBasicBlock* error_label;
            IrValue* error_dest;  // Where to store error value
        } error_check;

        // IR_ERROR_PROPAGATE: return error
        struct {
            IrValue* error_value;
        } error_propagate;
    } data;
};

// Control flow nodes
typedef struct IrIf {
    IrValue* condition;
    IrBasicBlock* then_block;
    IrBasicBlock* else_block;  // NULL if no else
    IrBasicBlock* merge_block;
} IrIf;

typedef struct IrSwitch {
    IrValue* value;
    IrSwitchCase* cases;
    size_t case_count;
    IrBasicBlock* default_block;  // NULL if no default
    IrBasicBlock* merge_block;
} IrSwitch;

typedef struct IrLoop {
    IrBasicBlock* header;
    IrBasicBlock* body;
    IrBasicBlock* exit;
} IrLoop;

typedef struct IrDeferRegion {
    IrBasicBlock* body;
    IrDeferCleanup** cleanups;
    size_t cleanup_count;
} IrDeferRegion;

typedef struct IrStateMachine {
    uint32_t initial_state;
    IrStateMachineState* states;
    size_t state_count;
    CoroMetadata* metadata;
    IrValue* frame_ptr;  // Coroutine frame
    IrValue* state_var;  // Current state variable
} IrStateMachine;

// IR Function
typedef struct IrFunction {
    const char* name;
    Type* return_type;
    IrParam* params;
    size_t param_count;

    // Function body
    IrBasicBlock** blocks;
    size_t block_count;
    size_t block_capacity;

    // Entry block
    IrBasicBlock* entry;

    // Coroutine metadata
    bool is_state_machine;
    IrStateMachine* state_machine;
    CoroMetadata* coro_meta;

    // Temporaries
    uint32_t next_temp_id;
    uint32_t next_block_id;

    // Defer stack for cleanup
    IrDeferCleanup** defer_stack;
    size_t defer_stack_count;
    size_t defer_stack_capacity;
} IrFunction;

// IR Module
typedef struct IrModule {
    const char* name;
    IrFunction** functions;
    size_t function_count;

    // Global variables
    IrValue** globals;
    size_t global_count;
} IrModule;

// Main IR node structure
struct IrNode {
    IrNodeKind kind;
    Type* type;
    SourceLocation loc;

    union {
        IrModule* module;
        IrFunction* function;
        IrBasicBlock* block;
        IrInstruction* instruction;
        IrIf* if_stmt;
        IrSwitch* switch_stmt;
        IrLoop* loop;
        IrDeferRegion* defer_region;
        IrStateMachine* state_machine;
        IrValue* value;
    } data;
};

// Forward declare Arena
typedef struct Arena Arena;
typedef struct AstNode AstNode;

// IR construction API
IrNode* ir_alloc_node(IrNodeKind kind, Arena* arena);
IrValue* ir_alloc_value(IrValueKind kind, Type* type, Arena* arena);
IrInstruction* ir_alloc_instruction(IrNodeKind kind, Arena* arena);
IrBasicBlock* ir_alloc_block(uint32_t id, const char* label, Arena* arena);

// IR function builder
IrFunction* ir_function_create(const char* name, Type* return_type,
                               IrParam* params, size_t param_count, Arena* arena);
IrValue* ir_function_new_temp(IrFunction* func, Type* type, Arena* arena);
IrBasicBlock* ir_function_new_block(IrFunction* func, const char* label, Arena* arena);
void ir_function_add_block(IrFunction* func, IrBasicBlock* block, Arena* arena);

// Basic block builder
void ir_block_add_instruction(IrBasicBlock* block, IrInstruction* instr, Arena* arena);
void ir_block_add_predecessor(IrBasicBlock* block, IrBasicBlock* pred, Arena* arena);
void ir_block_add_successor(IrBasicBlock* block, IrBasicBlock* succ, Arena* arena);

// Lowering API
IrModule* ir_lower_ast(AstNode* ast, Arena* arena);
IrFunction* ir_lower_function(AstNode* func_node, CoroMetadata* coro_meta, Arena* arena);
IrBasicBlock* ir_lower_block(AstNode* block_node, IrFunction* func, Arena* arena);
IrInstruction* ir_lower_stmt(AstNode* stmt, IrFunction* func, IrBasicBlock* block, Arena* arena);
IrValue* ir_lower_expr(AstNode* expr, IrFunction* func, IrBasicBlock* block, Arena* arena);

// State machine transformation
IrStateMachine* ir_transform_to_state_machine(IrFunction* func, CoroMetadata* meta, Arena* arena);

// Cleanup transformation
void ir_insert_defer_cleanup(IrFunction* func, IrBasicBlock* block, Arena* arena);
void ir_insert_errdefer_cleanup(IrFunction* func, IrBasicBlock* block, Arena* arena);

// Codegen consumes
void codegen_emit(IrNode* ir, FILE* out);
void ir_print_debug(IrNode* ir, FILE* out);

#endif // IR_H
