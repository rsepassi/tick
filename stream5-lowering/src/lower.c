#include "ir.h"
#include "ast.h"
#include "type.h"
#include "symbol.h"
#include "coro_metadata.h"
#include "arena.h"
#include "error.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>

// Variable mapping entry
typedef struct VarMapEntry {
    const char* name;
    IrValue* value;
} VarMapEntry;

// Internal context for lowering
typedef struct LowerContext {
    Arena* arena;
    IrFunction* current_function;
    IrBasicBlock* current_block;

    // Control flow context
    IrBasicBlock* break_target;
    IrBasicBlock* continue_target;

    // Error handling context
    IrBasicBlock* error_cleanup_block;

    // Defer stack
    IrDeferCleanup** defer_stack;
    size_t defer_count;
    size_t defer_capacity;

    // Variable tracking (maps identifier names to IR values)
    VarMapEntry* var_map;
    size_t var_map_count;
    size_t var_map_capacity;
} LowerContext;

// Forward declarations
static IrValue* lower_expr(LowerContext* ctx, AstNode* expr);
static void lower_stmt(LowerContext* ctx, AstNode* stmt);
static IrBasicBlock* lower_block_stmt(LowerContext* ctx, AstNode* block);
static void lower_break_stmt(LowerContext* ctx, AstNode* stmt);
static void lower_continue_stmt(LowerContext* ctx, AstNode* stmt);
static void lower_continue_switch_stmt(LowerContext* ctx, AstNode* stmt);
static void lower_resume_stmt(LowerContext* ctx, AstNode* stmt);
static void lower_try_catch_stmt(LowerContext* ctx, AstNode* stmt);

//=============================================================================
// Variable Map Helpers
//=============================================================================

static void var_map_init(LowerContext* ctx) {
    ctx->var_map_capacity = 32;
    ctx->var_map_count = 0;
    ctx->var_map = arena_alloc(ctx->arena,
        sizeof(VarMapEntry) * ctx->var_map_capacity, 8);
}

static void var_map_register(LowerContext* ctx, const char* name, IrValue* value) {
    // Expand capacity if needed
    if (ctx->var_map_count >= ctx->var_map_capacity) {
        ctx->var_map_capacity *= 2;
        VarMapEntry* new_map = arena_alloc(ctx->arena,
            sizeof(VarMapEntry) * ctx->var_map_capacity, 8);
        memcpy(new_map, ctx->var_map,
            sizeof(VarMapEntry) * ctx->var_map_count);
        ctx->var_map = new_map;
    }

    // Add new entry
    ctx->var_map[ctx->var_map_count].name = name;
    ctx->var_map[ctx->var_map_count].value = value;
    ctx->var_map_count++;
}

static IrValue* var_map_lookup(LowerContext* ctx, const char* name) {
    // Linear search (could be optimized with hash table if needed)
    for (size_t i = 0; i < ctx->var_map_count; i++) {
        if (strcmp(ctx->var_map[i].name, name) == 0) {
            return ctx->var_map[i].value;
        }
    }
    return NULL;
}

//=============================================================================
// IR Construction Helpers
//=============================================================================

IrNode* ir_alloc_node(IrNodeKind kind, Arena* arena) {
    IrNode* node = arena_alloc(arena, sizeof(IrNode), 8);
    memset(node, 0, sizeof(IrNode));
    node->kind = kind;
    return node;
}

IrValue* ir_alloc_value(IrValueKind kind, Type* type, Arena* arena) {
    IrValue* value = arena_alloc(arena, sizeof(IrValue), 8);
    memset(value, 0, sizeof(IrValue));
    value->kind = kind;
    value->type = type;
    return value;
}

IrInstruction* ir_alloc_instruction(IrNodeKind kind, Arena* arena) {
    IrInstruction* instr = arena_alloc(arena, sizeof(IrInstruction), 8);
    memset(instr, 0, sizeof(IrInstruction));
    instr->kind = kind;
    return instr;
}

IrBasicBlock* ir_alloc_block(uint32_t id, const char* label, Arena* arena) {
    IrBasicBlock* block = arena_alloc(arena, sizeof(IrBasicBlock), 8);
    memset(block, 0, sizeof(IrBasicBlock));
    block->id = id;
    block->label = label;
    block->instruction_capacity = 16;
    block->instructions = arena_alloc(arena,
        sizeof(IrInstruction*) * block->instruction_capacity, 8);
    return block;
}

//=============================================================================
// IR Function Builder
//=============================================================================

IrFunction* ir_function_create(const char* name, Type* return_type,
                               IrParam* params, size_t param_count, Arena* arena) {
    IrFunction* func = arena_alloc(arena, sizeof(IrFunction), 8);
    memset(func, 0, sizeof(IrFunction));
    func->name = name;
    func->return_type = return_type;
    func->params = params;
    func->param_count = param_count;
    func->next_temp_id = 0;
    func->next_block_id = 0;
    func->block_capacity = 16;
    func->blocks = arena_alloc(arena, sizeof(IrBasicBlock*) * func->block_capacity, 8);

    // Create entry block
    func->entry = ir_alloc_block(func->next_block_id++, "entry", arena);
    func->blocks[func->block_count++] = func->entry;

    return func;
}

IrValue* ir_function_new_temp(IrFunction* func, Type* type, Arena* arena) {
    IrValue* temp = ir_alloc_value(IR_VALUE_TEMP, type, arena);
    temp->data.temp.id = func->next_temp_id++;
    return temp;
}

IrBasicBlock* ir_function_new_block(IrFunction* func, const char* label, Arena* arena) {
    IrBasicBlock* block = ir_alloc_block(func->next_block_id++, label, arena);
    return block;
}

void ir_function_add_block(IrFunction* func, IrBasicBlock* block, Arena* arena) {
    if (func->block_count >= func->block_capacity) {
        func->block_capacity *= 2;
        IrBasicBlock** new_blocks = arena_alloc(arena,
            sizeof(IrBasicBlock*) * func->block_capacity, 8);
        memcpy(new_blocks, func->blocks, sizeof(IrBasicBlock*) * func->block_count);
        func->blocks = new_blocks;
    }
    func->blocks[func->block_count++] = block;
}

//=============================================================================
// Basic Block Builder
//=============================================================================

void ir_block_add_instruction(IrBasicBlock* block, IrInstruction* instr, Arena* arena) {
    if (block->instruction_count >= block->instruction_capacity) {
        block->instruction_capacity *= 2;
        IrInstruction** new_instrs = arena_alloc(arena,
            sizeof(IrInstruction*) * block->instruction_capacity, 8);
        memcpy(new_instrs, block->instructions,
            sizeof(IrInstruction*) * block->instruction_count);
        block->instructions = new_instrs;
    }
    block->instructions[block->instruction_count++] = instr;
}

void ir_block_add_predecessor(IrBasicBlock* block, IrBasicBlock* pred, Arena* arena) {
    // Allocate or resize predecessors array
    IrBasicBlock** new_preds = arena_alloc(arena,
        sizeof(IrBasicBlock*) * (block->predecessor_count + 1), 8);
    if (block->predecessors) {
        memcpy(new_preds, block->predecessors,
            sizeof(IrBasicBlock*) * block->predecessor_count);
    }
    new_preds[block->predecessor_count++] = pred;
    block->predecessors = new_preds;
}

void ir_block_add_successor(IrBasicBlock* block, IrBasicBlock* succ, Arena* arena) {
    // Allocate or resize successors array
    IrBasicBlock** new_succs = arena_alloc(arena,
        sizeof(IrBasicBlock*) * (block->successor_count + 1), 8);
    if (block->successors) {
        memcpy(new_succs, block->successors,
            sizeof(IrBasicBlock*) * block->successor_count);
    }
    new_succs[block->successor_count++] = succ;
    block->successors = new_succs;
}

//=============================================================================
// Expression Lowering
//=============================================================================

static IrValue* lower_literal_expr(LowerContext* ctx, AstNode* expr) {
    assert(expr->kind == AST_LITERAL_EXPR);
    IrValue* value = ir_alloc_value(IR_VALUE_CONSTANT, expr->type, ctx->arena);

    // Extract literal value from AST
    switch (expr->data.literal_expr.literal_kind) {
        case LITERAL_INT:
            value->data.constant.data.int_val = expr->data.literal_expr.value.int_value;
            break;
        case LITERAL_STRING:
            value->data.constant.data.str_val = expr->data.literal_expr.value.string.str_value;
            break;
        case LITERAL_BOOL:
            value->data.constant.data.bool_val = expr->data.literal_expr.value.bool_value;
            break;
    }

    return value;
}

static IrValue* lower_identifier_expr(LowerContext* ctx, AstNode* expr) {
    assert(expr->kind == AST_IDENTIFIER_EXPR);

    // Look up variable name from AST
    const char* name = expr->data.identifier_expr.name;

    // First, check if it's a parameter
    for (size_t i = 0; i < ctx->current_function->param_count; i++) {
        if (strcmp(ctx->current_function->params[i].name, name) == 0) {
            // Found parameter - create a value for it
            IrValue* param_value = ir_alloc_value(IR_VALUE_PARAM, expr->type, ctx->arena);
            param_value->data.param.index = i;
            param_value->data.param.name = name;
            return param_value;
        }
    }

    // Look up the identifier in the variable map (for local variables)
    IrValue* var_addr = var_map_lookup(ctx, name);

    if (var_addr) {
        // Found existing variable - it's a pointer from alloca
        // We need to load the value from the address
        IrValue* loaded_value = ir_function_new_temp(ctx->current_function, expr->type, ctx->arena);

        IrInstruction* load_instr = ir_alloc_instruction(IR_LOAD, ctx->arena);
        load_instr->type = expr->type;
        load_instr->loc = expr->loc;
        load_instr->data.load.dest = loaded_value;
        load_instr->data.load.addr = var_addr;

        ir_block_add_instruction(ctx->current_block, load_instr, ctx->arena);

        return loaded_value;
    }

    // If not found, this could be a global or undefined variable
    // For now, emit an error message and create a placeholder
    fprintf(stderr, "Warning: Undefined variable '%s' (could be global or undefined)\n", name);

    // Create a placeholder temp (this maintains compatibility with old behavior)
    IrValue* value = ir_function_new_temp(ctx->current_function, expr->type, ctx->arena);
    value->data.temp.name = name;
    return value;
}

static IrBinaryOp map_binary_op(BinaryOp ast_op) {
    switch (ast_op) {
        case BINOP_ADD: return IR_OP_ADD;
        case BINOP_SUB: return IR_OP_SUB;
        case BINOP_MUL: return IR_OP_MUL;
        case BINOP_DIV: return IR_OP_DIV;
        case BINOP_MOD: return IR_OP_MOD;
        case BINOP_AND: return IR_OP_AND;
        case BINOP_OR: return IR_OP_OR;
        case BINOP_XOR: return IR_OP_XOR;
        case BINOP_LSHIFT: return IR_OP_SHL;
        case BINOP_RSHIFT: return IR_OP_SHR;
        case BINOP_LOGICAL_AND: return IR_OP_LOGICAL_AND;
        case BINOP_LOGICAL_OR: return IR_OP_LOGICAL_OR;
        case BINOP_LT: return IR_OP_LT;
        case BINOP_GT: return IR_OP_GT;
        case BINOP_LT_EQ: return IR_OP_LE;
        case BINOP_GT_EQ: return IR_OP_GE;
        case BINOP_EQ_EQ: return IR_OP_EQ;
        case BINOP_BANG_EQ: return IR_OP_NE;
        default: return IR_OP_ADD;
    }
}

static IrValue* lower_binary_expr(LowerContext* ctx, AstNode* expr) {
    assert(expr->kind == AST_BINARY_EXPR);

    // Lower operands
    IrValue* left = lower_expr(ctx, expr->data.binary_expr.left);
    IrValue* right = lower_expr(ctx, expr->data.binary_expr.right);

    // Create destination temp
    IrValue* dest = ir_function_new_temp(ctx->current_function, expr->type, ctx->arena);

    // Create binary op instruction
    IrInstruction* instr = ir_alloc_instruction(IR_BINARY_OP, ctx->arena);
    instr->type = expr->type;
    instr->loc = expr->loc;
    instr->data.binary_op.dest = dest;
    instr->data.binary_op.left = left;
    instr->data.binary_op.right = right;
    instr->data.binary_op.op = map_binary_op(expr->data.binary_expr.op);

    ir_block_add_instruction(ctx->current_block, instr, ctx->arena);

    return dest;
}

static IrUnaryOp map_unary_op(UnaryOp ast_op) {
    switch (ast_op) {
        case UNOP_NEG: return IR_OP_NEG;
        case UNOP_NOT: return IR_OP_LOGICAL_NOT;
        case UNOP_BIT_NOT: return IR_OP_NOT;
        case UNOP_DEREF: return IR_OP_DEREF;
        case UNOP_ADDR: return IR_OP_ADDR_OF;
        default: return IR_OP_NEG;
    }
}

static IrValue* lower_unary_expr(LowerContext* ctx, AstNode* expr) {
    assert(expr->kind == AST_UNARY_EXPR);

    // Lower operand
    IrValue* operand = lower_expr(ctx, expr->data.unary_expr.operand);

    // Create destination temp
    IrValue* dest = ir_function_new_temp(ctx->current_function, expr->type, ctx->arena);

    // Create unary op instruction
    IrInstruction* instr = ir_alloc_instruction(IR_UNARY_OP, ctx->arena);
    instr->type = expr->type;
    instr->loc = expr->loc;
    instr->data.unary_op.dest = dest;
    instr->data.unary_op.operand = operand;
    instr->data.unary_op.op = map_unary_op(expr->data.unary_expr.op);

    ir_block_add_instruction(ctx->current_block, instr, ctx->arena);

    return dest;
}

static IrValue* lower_call_expr(LowerContext* ctx, AstNode* expr) {
    assert(expr->kind == AST_CALL_EXPR);

    // Lower function expression
    IrValue* func = lower_expr(ctx, expr->data.call_expr.callee);

    // Lower arguments
    size_t arg_count = expr->data.call_expr.arg_count;
    IrValue** args = NULL;
    if (arg_count > 0) {
        args = arena_alloc(ctx->arena, sizeof(IrValue*) * arg_count, 8);
        for (size_t i = 0; i < arg_count; i++) {
            args[i] = lower_expr(ctx, expr->data.call_expr.args[i]);
        }
    }

    // Create destination temp (NULL for void)
    IrValue* dest = NULL;
    if (expr->type && expr->type->kind != TYPE_VOID) {
        dest = ir_function_new_temp(ctx->current_function, expr->type, ctx->arena);
    }

    // Create call instruction
    IrInstruction* instr = ir_alloc_instruction(IR_CALL, ctx->arena);
    instr->type = expr->type;
    instr->loc = expr->loc;
    instr->data.call.dest = dest;
    instr->data.call.func = func;
    instr->data.call.args = args;
    instr->data.call.arg_count = arg_count;

    ir_block_add_instruction(ctx->current_block, instr, ctx->arena);

    return dest;
}

static IrValue* lower_async_call_expr(LowerContext* ctx, AstNode* expr) {
    assert(expr->kind == AST_ASYNC_CALL_EXPR);

    // Lower function expression
    IrValue* func = lower_expr(ctx, expr->data.async_call_expr.callee);

    // Lower arguments
    size_t arg_count = expr->data.async_call_expr.arg_count;
    IrValue** args = NULL;
    if (arg_count > 0) {
        args = arena_alloc(ctx->arena, sizeof(IrValue*) * arg_count, 8);
        for (size_t i = 0; i < arg_count; i++) {
            args[i] = lower_expr(ctx, expr->data.async_call_expr.args[i]);
        }
    }

    // Create destination temp
    IrValue* dest = NULL;
    if (expr->type->kind != TYPE_VOID) {
        dest = ir_function_new_temp(ctx->current_function, expr->type, ctx->arena);
    }

    // Create async call instruction
    IrInstruction* instr = ir_alloc_instruction(IR_ASYNC_CALL, ctx->arena);
    instr->type = expr->type;
    instr->loc = expr->loc;
    instr->data.async_call.dest = dest;
    instr->data.async_call.func = func;
    instr->data.async_call.args = args;
    instr->data.async_call.arg_count = arg_count;

    ir_block_add_instruction(ctx->current_block, instr, ctx->arena);

    return dest;
}

static IrValue* lower_field_access_expr(LowerContext* ctx, AstNode* expr) {
    assert(expr->kind == AST_FIELD_ACCESS_EXPR);

    // Lower base expression
    IrValue* base = lower_expr(ctx, expr->data.field_access_expr.object);

    // Create destination temp
    IrValue* dest = ir_function_new_temp(ctx->current_function, expr->type, ctx->arena);

    // Create get_field instruction
    IrInstruction* instr = ir_alloc_instruction(IR_GET_FIELD, ctx->arena);
    instr->type = expr->type;
    instr->loc = expr->loc;
    instr->data.get_field.dest = dest;
    instr->data.get_field.base = base;
    instr->data.get_field.field_index = 0; // TODO: Look up field index from type
    instr->data.get_field.field_name = expr->data.field_access_expr.field_name;

    ir_block_add_instruction(ctx->current_block, instr, ctx->arena);

    return dest;
}

static IrValue* lower_index_expr(LowerContext* ctx, AstNode* expr) {
    assert(expr->kind == AST_INDEX_EXPR);

    // Lower base and index
    IrValue* base = lower_expr(ctx, expr->data.index_expr.array);
    IrValue* index = lower_expr(ctx, expr->data.index_expr.index);

    // Create destination temp
    IrValue* dest = ir_function_new_temp(ctx->current_function, expr->type, ctx->arena);

    // Create get_index instruction
    IrInstruction* instr = ir_alloc_instruction(IR_GET_INDEX, ctx->arena);
    instr->type = expr->type;
    instr->loc = expr->loc;
    instr->data.get_index.dest = dest;
    instr->data.get_index.base = base;
    instr->data.get_index.index = index;

    ir_block_add_instruction(ctx->current_block, instr, ctx->arena);

    return dest;
}

static IrValue* lower_cast_expr(LowerContext* ctx, AstNode* expr) {
    assert(expr->kind == AST_CAST_EXPR);

    // Lower value to cast
    IrValue* value = lower_expr(ctx, expr->data.cast_expr.expr);

    // Create destination temp
    IrValue* dest = ir_function_new_temp(ctx->current_function, expr->type, ctx->arena);

    // Create cast instruction
    IrInstruction* instr = ir_alloc_instruction(IR_CAST, ctx->arena);
    instr->type = expr->type;
    instr->loc = expr->loc;
    instr->data.cast.dest = dest;
    instr->data.cast.target_type = expr->type;
    instr->data.cast.value = value;

    ir_block_add_instruction(ctx->current_block, instr, ctx->arena);

    return dest;
}

static IrValue* lower_struct_init_expr(LowerContext* ctx, AstNode* expr) {
    assert(expr->kind == AST_STRUCT_INIT_EXPR);

    // Allocate memory for the struct
    IrValue* dest = ir_function_new_temp(ctx->current_function, expr->type, ctx->arena);

    IrInstruction* alloca_instr = ir_alloc_instruction(IR_ALLOCA, ctx->arena);
    alloca_instr->type = expr->type;
    alloca_instr->loc = expr->loc;
    alloca_instr->data.alloca.dest = dest;
    alloca_instr->data.alloca.alloc_type = expr->type;
    alloca_instr->data.alloca.count = 1;

    ir_block_add_instruction(ctx->current_block, alloca_instr, ctx->arena);

    // Initialize each field
    for (size_t i = 0; i < expr->data.struct_init_expr.field_count; i++) {
        AstStructInit* field_init = &expr->data.struct_init_expr.fields[i];

        // Lower the field value
        IrValue* field_value = lower_expr(ctx, field_init->value);

        // Get field address
        IrValue* field_addr = ir_function_new_temp(ctx->current_function,
            field_value->type, ctx->arena);

        IrInstruction* get_field = ir_alloc_instruction(IR_GET_FIELD, ctx->arena);
        get_field->type = field_value->type;
        get_field->loc = expr->loc;
        get_field->data.get_field.dest = field_addr;
        get_field->data.get_field.base = dest;
        get_field->data.get_field.field_index = i;
        get_field->data.get_field.field_name = field_init->field_name;

        ir_block_add_instruction(ctx->current_block, get_field, ctx->arena);

        // Store the value into the field
        IrInstruction* store = ir_alloc_instruction(IR_STORE, ctx->arena);
        store->loc = expr->loc;
        store->data.store.addr = field_addr;
        store->data.store.value = field_value;

        ir_block_add_instruction(ctx->current_block, store, ctx->arena);
    }

    return dest;
}

static IrValue* lower_array_init_expr(LowerContext* ctx, AstNode* expr) {
    assert(expr->kind == AST_ARRAY_INIT_EXPR);

    // Allocate memory for the array
    IrValue* dest = ir_function_new_temp(ctx->current_function, expr->type, ctx->arena);

    IrInstruction* alloca_instr = ir_alloc_instruction(IR_ALLOCA, ctx->arena);
    alloca_instr->type = expr->type;
    alloca_instr->loc = expr->loc;
    alloca_instr->data.alloca.dest = dest;
    alloca_instr->data.alloca.alloc_type = expr->type;
    alloca_instr->data.alloca.count = expr->data.array_init_expr.element_count;

    ir_block_add_instruction(ctx->current_block, alloca_instr, ctx->arena);

    // Initialize each element
    for (size_t i = 0; i < expr->data.array_init_expr.element_count; i++) {
        // Lower the element value
        IrValue* element_value = lower_expr(ctx, expr->data.array_init_expr.elements[i]);

        // Create constant for index
        IrValue* index_const = ir_alloc_value(IR_VALUE_CONSTANT, NULL, ctx->arena);
        index_const->data.constant.data.int_val = i;

        // Get element address
        IrValue* element_addr = ir_function_new_temp(ctx->current_function,
            element_value->type, ctx->arena);

        IrInstruction* get_index = ir_alloc_instruction(IR_GET_INDEX, ctx->arena);
        get_index->type = element_value->type;
        get_index->loc = expr->loc;
        get_index->data.get_index.dest = element_addr;
        get_index->data.get_index.base = dest;
        get_index->data.get_index.index = index_const;

        ir_block_add_instruction(ctx->current_block, get_index, ctx->arena);

        // Store the value into the element
        IrInstruction* store = ir_alloc_instruction(IR_STORE, ctx->arena);
        store->loc = expr->loc;
        store->data.store.addr = element_addr;
        store->data.store.value = element_value;

        ir_block_add_instruction(ctx->current_block, store, ctx->arena);
    }

    return dest;
}

static IrValue* lower_range_expr(LowerContext* ctx, AstNode* expr) {
    assert(expr->kind == AST_RANGE_EXPR);

    // Lower start and end expressions
    IrValue* start = lower_expr(ctx, expr->data.range_expr.start);
    IrValue* end = lower_expr(ctx, expr->data.range_expr.end);

    // Create a temporary for the range (typically this would be a struct with start/end)
    // For now, we'll create a simple representation
    IrValue* dest = ir_function_new_temp(ctx->current_function, expr->type, ctx->arena);

    // Allocate memory for range struct
    IrInstruction* alloca_instr = ir_alloc_instruction(IR_ALLOCA, ctx->arena);
    alloca_instr->type = expr->type;
    alloca_instr->loc = expr->loc;
    alloca_instr->data.alloca.dest = dest;
    alloca_instr->data.alloca.alloc_type = expr->type;
    alloca_instr->data.alloca.count = 1;

    ir_block_add_instruction(ctx->current_block, alloca_instr, ctx->arena);

    // Store start value (field 0)
    IrValue* start_field = ir_function_new_temp(ctx->current_function, start->type, ctx->arena);
    IrInstruction* get_start_field = ir_alloc_instruction(IR_GET_FIELD, ctx->arena);
    get_start_field->type = start->type;
    get_start_field->loc = expr->loc;
    get_start_field->data.get_field.dest = start_field;
    get_start_field->data.get_field.base = dest;
    get_start_field->data.get_field.field_index = 0;
    get_start_field->data.get_field.field_name = "start";

    ir_block_add_instruction(ctx->current_block, get_start_field, ctx->arena);

    IrInstruction* store_start = ir_alloc_instruction(IR_STORE, ctx->arena);
    store_start->loc = expr->loc;
    store_start->data.store.addr = start_field;
    store_start->data.store.value = start;

    ir_block_add_instruction(ctx->current_block, store_start, ctx->arena);

    // Store end value (field 1)
    IrValue* end_field = ir_function_new_temp(ctx->current_function, end->type, ctx->arena);
    IrInstruction* get_end_field = ir_alloc_instruction(IR_GET_FIELD, ctx->arena);
    get_end_field->type = end->type;
    get_end_field->loc = expr->loc;
    get_end_field->data.get_field.dest = end_field;
    get_end_field->data.get_field.base = dest;
    get_end_field->data.get_field.field_index = 1;
    get_end_field->data.get_field.field_name = "end";

    ir_block_add_instruction(ctx->current_block, get_end_field, ctx->arena);

    IrInstruction* store_end = ir_alloc_instruction(IR_STORE, ctx->arena);
    store_end->loc = expr->loc;
    store_end->data.store.addr = end_field;
    store_end->data.store.value = end;

    ir_block_add_instruction(ctx->current_block, store_end, ctx->arena);

    return dest;
}

static IrValue* lower_try_expr(LowerContext* ctx, AstNode* expr) {
    assert(expr->kind == AST_TRY_EXPR);

    // Lower the expression that might produce an error
    IrValue* result = lower_expr(ctx, expr->data.try_expr.expr);

    // Create error check blocks
    IrBasicBlock* error_block = ir_function_new_block(ctx->current_function,
        "try.error", ctx->arena);
    IrBasicBlock* success_block = ir_function_new_block(ctx->current_function,
        "try.success", ctx->arena);

    // Create temporary for error value
    IrValue* error_value = ir_function_new_temp(ctx->current_function, expr->type, ctx->arena);

    // Create error check instruction
    IrInstruction* error_check = ir_alloc_instruction(IR_ERROR_CHECK, ctx->arena);
    error_check->type = expr->type;
    error_check->loc = expr->loc;
    error_check->data.error_check.value = result;
    error_check->data.error_check.error_label = error_block;
    error_check->data.error_check.error_dest = error_value;

    ir_block_add_instruction(ctx->current_block, error_check, ctx->arena);

    // Add CFG edges for error path
    ir_block_add_successor(ctx->current_block, error_block, ctx->arena);
    ir_block_add_predecessor(error_block, ctx->current_block, ctx->arena);
    ir_block_add_successor(ctx->current_block, success_block, ctx->arena);
    ir_block_add_predecessor(success_block, ctx->current_block, ctx->arena);

    // Handle error block - propagate the error
    ir_function_add_block(ctx->current_function, error_block, ctx->arena);
    ctx->current_block = error_block;

    // Create error propagate instruction
    IrInstruction* error_propagate = ir_alloc_instruction(IR_ERROR_PROPAGATE, ctx->arena);
    error_propagate->loc = expr->loc;
    error_propagate->data.error_propagate.error_value = error_value;

    ir_block_add_instruction(ctx->current_block, error_propagate, ctx->arena);

    // Continue in success block
    ir_function_add_block(ctx->current_function, success_block, ctx->arena);
    ctx->current_block = success_block;

    return result;
}

static IrValue* lower_expr(LowerContext* ctx, AstNode* expr) {
    if (!expr) return NULL;

    switch (expr->kind) {
        case AST_LITERAL_EXPR:
            return lower_literal_expr(ctx, expr);
        case AST_IDENTIFIER_EXPR:
            return lower_identifier_expr(ctx, expr);
        case AST_BINARY_EXPR:
            return lower_binary_expr(ctx, expr);
        case AST_UNARY_EXPR:
            return lower_unary_expr(ctx, expr);
        case AST_CALL_EXPR:
            return lower_call_expr(ctx, expr);
        case AST_ASYNC_CALL_EXPR:
            return lower_async_call_expr(ctx, expr);
        case AST_FIELD_ACCESS_EXPR:
            return lower_field_access_expr(ctx, expr);
        case AST_INDEX_EXPR:
            return lower_index_expr(ctx, expr);
        case AST_CAST_EXPR:
            return lower_cast_expr(ctx, expr);
        case AST_STRUCT_INIT_EXPR:
            return lower_struct_init_expr(ctx, expr);
        case AST_ARRAY_INIT_EXPR:
            return lower_array_init_expr(ctx, expr);
        case AST_RANGE_EXPR:
            return lower_range_expr(ctx, expr);
        case AST_TRY_EXPR:
            return lower_try_expr(ctx, expr);
        default:
            assert(0 && "Unhandled expression kind");
            return NULL;
    }
}

//=============================================================================
// Statement Lowering
//=============================================================================

static void lower_let_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_LET_STMT);

    // Lower initializer expression
    IrValue* init_value = lower_expr(ctx, stmt->data.let_decl.init);

    // Allocate local variable
    IrInstruction* alloca_instr = ir_alloc_instruction(IR_ALLOCA, ctx->arena);
    alloca_instr->loc = stmt->loc;

    // Get the variable type from the type annotation or initializer
    Type* var_type = NULL;
    if (stmt->data.let_decl.type && stmt->data.let_decl.type->type) {
        // Explicit type annotation
        var_type = stmt->data.let_decl.type->type;
    } else if (init_value && init_value->type) {
        // Infer from initializer
        var_type = init_value->type;
    }

    // If still NULL, use a default type (shouldn't happen after proper type checking)
    if (!var_type) {
        fprintf(stderr, "Warning: Cannot determine type for let statement, defaulting to i32\n");
        // Create a default i32 type
        var_type = arena_alloc(ctx->arena, sizeof(Type), 8);
        var_type->kind = TYPE_I32;
        var_type->size = 4;
        var_type->alignment = 4;
    }

    // Create pointer type for the destination (alloca returns a pointer to the allocated type)
    Type* ptr_type = type_new_pointer(var_type, ctx->arena);

    // Create temp with the variable name for better codegen
    IrValue* dest = ir_function_new_temp(ctx->current_function, ptr_type, ctx->arena);
    dest->data.temp.name = stmt->data.let_decl.name;

    alloca_instr->data.alloca.dest = dest;
    alloca_instr->data.alloca.alloc_type = var_type;  // Allocate space for var_type
    alloca_instr->data.alloca.count = 1;

    ir_block_add_instruction(ctx->current_block, alloca_instr, ctx->arena);

    // Register the variable in the variable map
    const char* var_name = stmt->data.let_decl.name;
    if (var_name) {
        var_map_register(ctx, var_name, alloca_instr->data.alloca.dest);
    }

    // Store initial value
    if (init_value) {
        IrInstruction* store_instr = ir_alloc_instruction(IR_STORE, ctx->arena);
        store_instr->loc = stmt->loc;
        store_instr->data.store.addr = alloca_instr->data.alloca.dest;
        store_instr->data.store.value = init_value;

        ir_block_add_instruction(ctx->current_block, store_instr, ctx->arena);
    }
}

static void lower_var_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_VAR_STMT);

    // Lower initializer expression if present
    IrValue* init_value = NULL;
    if (stmt->data.var_decl.init) {
        init_value = lower_expr(ctx, stmt->data.var_decl.init);
    }

    // Allocate local variable
    IrInstruction* alloca_instr = ir_alloc_instruction(IR_ALLOCA, ctx->arena);
    alloca_instr->loc = stmt->loc;

    // Get the variable type from the type annotation or initializer
    Type* var_type = NULL;
    if (stmt->data.var_decl.type && stmt->data.var_decl.type->type) {
        // Explicit type annotation
        var_type = stmt->data.var_decl.type->type;
    } else if (init_value && init_value->type) {
        // Infer from initializer
        var_type = init_value->type;
    }

    // If still NULL, use a default type (shouldn't happen after proper type checking)
    if (!var_type) {
        fprintf(stderr, "Warning: Cannot determine type for var statement, defaulting to i32\n");
        // Create a default i32 type
        var_type = arena_alloc(ctx->arena, sizeof(Type), 8);
        var_type->kind = TYPE_I32;
        var_type->size = 4;
        var_type->alignment = 4;
    }

    // Create pointer type for the destination (alloca returns a pointer to the allocated type)
    Type* ptr_type = type_new_pointer(var_type, ctx->arena);

    // Create temp with the variable name for better codegen
    IrValue* dest = ir_function_new_temp(ctx->current_function, ptr_type, ctx->arena);
    dest->data.temp.name = stmt->data.var_decl.name;

    alloca_instr->data.alloca.dest = dest;
    alloca_instr->data.alloca.alloc_type = var_type;  // Allocate space for var_type
    alloca_instr->data.alloca.count = 1;

    ir_block_add_instruction(ctx->current_block, alloca_instr, ctx->arena);

    // Register the variable in the variable map
    const char* var_name = stmt->data.var_decl.name;
    if (var_name) {
        var_map_register(ctx, var_name, alloca_instr->data.alloca.dest);
    }

    // Store initial value if present
    if (init_value) {
        IrInstruction* store_instr = ir_alloc_instruction(IR_STORE, ctx->arena);
        store_instr->loc = stmt->loc;
        store_instr->data.store.addr = alloca_instr->data.alloca.dest;
        store_instr->data.store.value = init_value;

        ir_block_add_instruction(ctx->current_block, store_instr, ctx->arena);
    }
}

static void lower_assign_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_ASSIGN_STMT);

    // Lower RHS expression
    IrValue* rhs_value = lower_expr(ctx, stmt->data.assign_stmt.rhs);

    // For compound assignments (+=, -=, etc.), we need to:
    // 1. Load the current value from LHS
    // 2. Perform the operation
    // 3. Store the result back

    if (stmt->data.assign_stmt.op != ASSIGN_SIMPLE) {
        // Load current value
        IrValue* lhs_value = lower_expr(ctx, stmt->data.assign_stmt.lhs);

        // Map compound assignment to binary operation
        IrBinaryOp bin_op;
        switch (stmt->data.assign_stmt.op) {
            case ASSIGN_ADD: bin_op = IR_OP_ADD; break;
            case ASSIGN_SUB: bin_op = IR_OP_SUB; break;
            case ASSIGN_MUL: bin_op = IR_OP_MUL; break;
            case ASSIGN_DIV: bin_op = IR_OP_DIV; break;
            case ASSIGN_MOD: bin_op = IR_OP_MOD; break;
            case ASSIGN_AND: bin_op = IR_OP_AND; break;
            case ASSIGN_OR: bin_op = IR_OP_OR; break;
            case ASSIGN_XOR: bin_op = IR_OP_XOR; break;
            case ASSIGN_LSHIFT: bin_op = IR_OP_SHL; break;
            case ASSIGN_RSHIFT: bin_op = IR_OP_SHR; break;
            default: bin_op = IR_OP_ADD; break;
        }

        // Create result temp
        IrValue* result = ir_function_new_temp(ctx->current_function,
            stmt->data.assign_stmt.lhs->type, ctx->arena);

        // Create binary op instruction
        IrInstruction* bin_instr = ir_alloc_instruction(IR_BINARY_OP, ctx->arena);
        bin_instr->type = stmt->data.assign_stmt.lhs->type;
        bin_instr->loc = stmt->loc;
        bin_instr->data.binary_op.dest = result;
        bin_instr->data.binary_op.left = lhs_value;
        bin_instr->data.binary_op.right = rhs_value;
        bin_instr->data.binary_op.op = bin_op;

        ir_block_add_instruction(ctx->current_block, bin_instr, ctx->arena);

        rhs_value = result;
    }

    // For simple assignment, we need to get the address of the LHS
    // and store the RHS value to it
    // For now, treat LHS as a temp and create an assign instruction
    IrValue* lhs_value = lower_expr(ctx, stmt->data.assign_stmt.lhs);

    IrInstruction* assign_instr = ir_alloc_instruction(IR_ASSIGN, ctx->arena);
    assign_instr->type = stmt->data.assign_stmt.lhs->type;
    assign_instr->loc = stmt->loc;
    assign_instr->data.assign.dest = lhs_value;
    assign_instr->data.assign.src = rhs_value;

    ir_block_add_instruction(ctx->current_block, assign_instr, ctx->arena);
}

static void lower_return_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_RETURN_STMT);

    // Execute defers in reverse order
    for (int i = ctx->defer_count - 1; i >= 0; i--) {
        IrDeferCleanup* cleanup = ctx->defer_stack[i];
        for (size_t j = 0; j < cleanup->instruction_count; j++) {
            ir_block_add_instruction(ctx->current_block,
                cleanup->instructions[j], ctx->arena);
        }
    }

    // Lower return value
    IrValue* ret_value = lower_expr(ctx, stmt->data.return_stmt.value);

    // Create return instruction
    IrInstruction* ret_instr = ir_alloc_instruction(IR_RETURN, ctx->arena);
    ret_instr->loc = stmt->loc;
    ret_instr->data.ret.value = ret_value;

    ir_block_add_instruction(ctx->current_block, ret_instr, ctx->arena);
}

static void lower_if_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_IF_STMT);

    // Lower condition
    IrValue* cond = lower_expr(ctx, stmt->data.if_stmt.condition);

    // Create blocks
    IrBasicBlock* then_block = ir_function_new_block(ctx->current_function,
        "if.then", ctx->arena);
    IrBasicBlock* else_block = stmt->data.if_stmt.else_block ?
        ir_function_new_block(ctx->current_function, "if.else", ctx->arena) : NULL;
    IrBasicBlock* merge_block = ir_function_new_block(ctx->current_function,
        "if.merge", ctx->arena);

    // Create conditional jump
    IrInstruction* cond_jump = ir_alloc_instruction(IR_COND_JUMP, ctx->arena);
    cond_jump->loc = stmt->loc;
    cond_jump->data.cond_jump.cond = cond;
    cond_jump->data.cond_jump.true_target = then_block;
    cond_jump->data.cond_jump.false_target = else_block ? else_block : merge_block;

    ir_block_add_instruction(ctx->current_block, cond_jump, ctx->arena);

    // Add CFG edges
    ir_block_add_successor(ctx->current_block, then_block, ctx->arena);
    ir_block_add_predecessor(then_block, ctx->current_block, ctx->arena);

    // Add then block to function
    ir_function_add_block(ctx->current_function, then_block, ctx->arena);

    // Lower then block
    IrBasicBlock* saved_block __attribute__((unused)) = ctx->current_block;
    ctx->current_block = then_block;
    lower_stmt(ctx, stmt->data.if_stmt.then_block);

    // Jump to merge
    IrInstruction* then_jump = ir_alloc_instruction(IR_JUMP, ctx->arena);
    then_jump->data.jump.target = merge_block;
    ir_block_add_instruction(ctx->current_block, then_jump, ctx->arena);
    ir_block_add_successor(ctx->current_block, merge_block, ctx->arena);
    ir_block_add_predecessor(merge_block, ctx->current_block, ctx->arena);

    // Lower else block if exists
    if (else_block) {
        ctx->current_block = else_block;
        ir_function_add_block(ctx->current_function, else_block, ctx->arena);
        lower_stmt(ctx, stmt->data.if_stmt.else_block);

        // Jump to merge
        IrInstruction* else_jump = ir_alloc_instruction(IR_JUMP, ctx->arena);
        else_jump->data.jump.target = merge_block;
        ir_block_add_instruction(ctx->current_block, else_jump, ctx->arena);
        ir_block_add_successor(ctx->current_block, merge_block, ctx->arena);
        ir_block_add_predecessor(merge_block, ctx->current_block, ctx->arena);
    }

    // Continue in merge block
    ir_function_add_block(ctx->current_function, merge_block, ctx->arena);
    ctx->current_block = merge_block;
}

static void lower_for_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_FOR_STMT);

    // Create loop blocks
    IrBasicBlock* header = ir_function_new_block(ctx->current_function,
        "loop.header", ctx->arena);
    IrBasicBlock* body = ir_function_new_block(ctx->current_function,
        "loop.body", ctx->arena);
    IrBasicBlock* exit = ir_function_new_block(ctx->current_function,
        "loop.exit", ctx->arena);

    // TODO: Lower init statement

    // Jump to header
    IrInstruction* jump_header = ir_alloc_instruction(IR_JUMP, ctx->arena);
    jump_header->data.jump.target = header;
    ir_block_add_instruction(ctx->current_block, jump_header, ctx->arena);
    ir_block_add_successor(ctx->current_block, header, ctx->arena);
    ir_block_add_predecessor(header, ctx->current_block, ctx->arena);

    // Lower header (condition check)
    ir_function_add_block(ctx->current_function, header, ctx->arena);
    ctx->current_block = header;

    IrValue* cond = lower_expr(ctx, NULL); // TODO: Get condition from stmt
    IrInstruction* cond_jump = ir_alloc_instruction(IR_COND_JUMP, ctx->arena);
    cond_jump->data.cond_jump.cond = cond;
    cond_jump->data.cond_jump.true_target = body;
    cond_jump->data.cond_jump.false_target = exit;
    ir_block_add_instruction(ctx->current_block, cond_jump, ctx->arena);

    ir_block_add_successor(header, body, ctx->arena);
    ir_block_add_predecessor(body, header, ctx->arena);
    ir_block_add_successor(header, exit, ctx->arena);
    ir_block_add_predecessor(exit, header, ctx->arena);

    // Lower body
    ir_function_add_block(ctx->current_function, body, ctx->arena);
    ctx->current_block = body;

    // Save break/continue targets
    IrBasicBlock* saved_break = ctx->break_target;
    IrBasicBlock* saved_continue = ctx->continue_target;
    ctx->break_target = exit;
    ctx->continue_target = header; // TODO: Should be increment block if exists

    // TODO: Lower body statements

    // Restore targets
    ctx->break_target = saved_break;
    ctx->continue_target = saved_continue;

    // TODO: Lower increment statement

    // Jump back to header
    IrInstruction* jump_back = ir_alloc_instruction(IR_JUMP, ctx->arena);
    jump_back->data.jump.target = header;
    ir_block_add_instruction(ctx->current_block, jump_back, ctx->arena);
    ir_block_add_successor(ctx->current_block, header, ctx->arena);
    ir_block_add_predecessor(header, ctx->current_block, ctx->arena);

    // Continue in exit block
    ir_function_add_block(ctx->current_function, exit, ctx->arena);
    ctx->current_block = exit;
}

static void lower_switch_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_SWITCH_STMT);

    // Lower switch value
    IrValue* value __attribute__((unused)) = lower_expr(ctx, NULL); // TODO: Get value from stmt

    // Create merge block
    IrBasicBlock* merge = ir_function_new_block(ctx->current_function,
        "switch.merge", ctx->arena);

    // TODO: Create case blocks and lower each case

    // Continue in merge block
    ir_function_add_block(ctx->current_function, merge, ctx->arena);
    ctx->current_block = merge;
}

static void lower_defer_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_DEFER_STMT);

    // Create defer cleanup
    IrDeferCleanup* cleanup = arena_alloc(ctx->arena, sizeof(IrDeferCleanup), 8);
    cleanup->is_errdefer = false;

    // Lower defer body and store instructions
    // TODO: Create a temporary block to collect defer instructions
    cleanup->instructions = NULL;
    cleanup->instruction_count = 0;

    // Push onto defer stack
    if (!ctx->defer_stack) {
        ctx->defer_capacity = 8;
        ctx->defer_stack = arena_alloc(ctx->arena,
            sizeof(IrDeferCleanup*) * ctx->defer_capacity, 8);
    }
    if (ctx->defer_count >= ctx->defer_capacity) {
        ctx->defer_capacity *= 2;
        IrDeferCleanup** new_stack = arena_alloc(ctx->arena,
            sizeof(IrDeferCleanup*) * ctx->defer_capacity, 8);
        memcpy(new_stack, ctx->defer_stack,
            sizeof(IrDeferCleanup*) * ctx->defer_count);
        ctx->defer_stack = new_stack;
    }
    ctx->defer_stack[ctx->defer_count++] = cleanup;
}

static void lower_errdefer_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_ERRDEFER_STMT);

    // Similar to defer but only executed on error paths
    IrDeferCleanup* cleanup = arena_alloc(ctx->arena, sizeof(IrDeferCleanup), 8);
    cleanup->is_errdefer = true;

    // TODO: Lower errdefer body
    cleanup->instructions = NULL;
    cleanup->instruction_count = 0;

    // Push onto defer stack
    if (!ctx->defer_stack) {
        ctx->defer_capacity = 8;
        ctx->defer_stack = arena_alloc(ctx->arena,
            sizeof(IrDeferCleanup*) * ctx->defer_capacity, 8);
    }
    if (ctx->defer_count >= ctx->defer_capacity) {
        ctx->defer_capacity *= 2;
        IrDeferCleanup** new_stack = arena_alloc(ctx->arena,
            sizeof(IrDeferCleanup*) * ctx->defer_capacity, 8);
        memcpy(new_stack, ctx->defer_stack,
            sizeof(IrDeferCleanup*) * ctx->defer_count);
        ctx->defer_stack = new_stack;
    }
    ctx->defer_stack[ctx->defer_count++] = cleanup;
}

static void lower_suspend_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_SUSPEND_STMT);

    // In async functions, suspend creates a coroutine suspend point
    // This requires:
    // 1. Saving live variables to coroutine frame
    // 2. Updating state field in frame
    // 3. Returning control to caller
    // 4. On resume, restoring live variables and continuing

    // The state_id will be filled in by the state machine transformation
    // based on the coroutine metadata analysis

    // Create suspend instruction
    IrInstruction* suspend = ir_alloc_instruction(IR_SUSPEND, ctx->arena);
    suspend->loc = stmt->loc;
    // State ID and live vars will be populated during state machine transformation
    suspend->data.suspend.state_id = 0;
    suspend->data.suspend.live_vars = NULL;
    suspend->data.suspend.live_var_count = 0;

    ir_block_add_instruction(ctx->current_block, suspend, ctx->arena);

    // Create a new block for the continuation after resume
    IrBasicBlock* resume_block = ir_function_new_block(ctx->current_function,
        "suspend_resume", ctx->arena);
    ir_function_add_block(ctx->current_function, resume_block, ctx->arena);

    // Add CFG edge from suspend to resume
    ir_block_add_successor(ctx->current_block, resume_block, ctx->arena);
    ir_block_add_predecessor(resume_block, ctx->current_block, ctx->arena);

    // Continue lowering in the resume block
    ctx->current_block = resume_block;
}

static void lower_expr_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_EXPR_STMT);

    // Just lower the expression (side effects matter)
    lower_expr(ctx, stmt->data.expr_stmt.expr);
}

static void lower_break_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_BREAK_STMT);

    // Break jumps to the loop exit block
    if (ctx->break_target) {
        IrInstruction* jmp = ir_alloc_instruction(IR_JUMP, ctx->arena);
        jmp->loc = stmt->loc;
        jmp->data.jump.target = ctx->break_target;
        ir_block_add_instruction(ctx->current_block, jmp, ctx->arena);
        ir_block_add_successor(ctx->current_block, ctx->break_target, ctx->arena);
        ir_block_add_predecessor(ctx->break_target, ctx->current_block, ctx->arena);
    } else {
        // Error: break outside of loop - should be caught in semantic analysis
        fprintf(stderr, "Error: break statement outside of loop\n");
    }
}

static void lower_continue_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_CONTINUE_STMT);

    // Continue jumps to the loop header/increment block
    if (ctx->continue_target) {
        IrInstruction* jmp = ir_alloc_instruction(IR_JUMP, ctx->arena);
        jmp->loc = stmt->loc;
        jmp->data.jump.target = ctx->continue_target;
        ir_block_add_instruction(ctx->current_block, jmp, ctx->arena);
        ir_block_add_successor(ctx->current_block, ctx->continue_target, ctx->arena);
        ir_block_add_predecessor(ctx->continue_target, ctx->current_block, ctx->arena);
    } else {
        // Error: continue outside of loop - should be caught in semantic analysis
        fprintf(stderr, "Error: continue statement outside of loop\n");
    }
}

static void lower_continue_switch_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_CONTINUE_SWITCH_STMT);

    // Lower the new value expression
    IrValue* value = lower_expr(ctx, stmt->data.continue_switch_stmt.value);

    // Store the new value to the switch variable
    // Then jump back to the switch header
    // TODO: This needs proper implementation with switch context tracking
    // For now, just emit an assign and jump to continue target

    if (ctx->continue_target) {
        // Create an assignment to update the switch variable
        // (The switch variable tracking would need to be added to context)

        // Jump back to loop header
        IrInstruction* jmp = ir_alloc_instruction(IR_JUMP, ctx->arena);
        jmp->loc = stmt->loc;
        jmp->data.jump.target = ctx->continue_target;
        ir_block_add_instruction(ctx->current_block, jmp, ctx->arena);
        ir_block_add_successor(ctx->current_block, ctx->continue_target, ctx->arena);
        ir_block_add_predecessor(ctx->continue_target, ctx->current_block, ctx->arena);
    }
}

static void lower_resume_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_RESUME_STMT);

    // Lower the coroutine expression
    IrValue* coro = lower_expr(ctx, stmt->data.resume_stmt.coro);

    // Create resume instruction
    IrInstruction* resume = ir_alloc_instruction(IR_RESUME, ctx->arena);
    resume->loc = stmt->loc;
    resume->data.resume.coro_handle = coro;
    resume->data.resume.state_id = 0;  // Will be filled during state machine transformation
    ir_block_add_instruction(ctx->current_block, resume, ctx->arena);
}

static void lower_try_catch_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_TRY_CATCH_STMT);

    // Create blocks for try, catch, and continuation
    IrBasicBlock* try_block = ir_function_new_block(ctx->current_function, "try", ctx->arena);
    IrBasicBlock* catch_block = NULL;
    IrBasicBlock* cont_block = ir_function_new_block(ctx->current_function, "try_cont", ctx->arena);

    if (stmt->data.try_catch_stmt.catch_block) {
        catch_block = ir_function_new_block(ctx->current_function, "catch", ctx->arena);
    }

    // Jump to try block
    IrInstruction* jmp_try = ir_alloc_instruction(IR_JUMP, ctx->arena);
    jmp_try->data.jump.target = try_block;
    ir_block_add_instruction(ctx->current_block, jmp_try, ctx->arena);
    ir_block_add_successor(ctx->current_block, try_block, ctx->arena);

    // Lower try block
    ctx->current_block = try_block;
    ir_function_add_block(ctx->current_function, try_block, ctx->arena);
    lower_block_stmt(ctx, stmt->data.try_catch_stmt.try_block);

    // Jump to continuation from try block
    IrInstruction* jmp_cont = ir_alloc_instruction(IR_JUMP, ctx->arena);
    jmp_cont->data.jump.target = cont_block;
    ir_block_add_instruction(ctx->current_block, jmp_cont, ctx->arena);
    ir_block_add_successor(ctx->current_block, cont_block, ctx->arena);

    // Lower catch block if present
    if (catch_block) {
        ctx->current_block = catch_block;
        ir_function_add_block(ctx->current_function, catch_block, ctx->arena);

        // TODO: Bind error variable
        // const char* error_var = stmt->data.try_catch_stmt.error_var;

        lower_block_stmt(ctx, stmt->data.try_catch_stmt.catch_block);

        // Jump to continuation from catch block
        IrInstruction* jmp_cont_catch = ir_alloc_instruction(IR_JUMP, ctx->arena);
        jmp_cont_catch->data.jump.target = cont_block;
        ir_block_add_instruction(ctx->current_block, jmp_cont_catch, ctx->arena);
        ir_block_add_successor(ctx->current_block, cont_block, ctx->arena);
    }

    // Continue with continuation block
    ctx->current_block = cont_block;
    ir_function_add_block(ctx->current_function, cont_block, ctx->arena);
}

static void lower_stmt(LowerContext* ctx, AstNode* stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case AST_LET_STMT:
            lower_let_stmt(ctx, stmt);
            break;
        case AST_VAR_STMT:
            lower_var_stmt(ctx, stmt);
            break;
        case AST_ASSIGN_STMT:
            lower_assign_stmt(ctx, stmt);
            break;
        case AST_RETURN_STMT:
            lower_return_stmt(ctx, stmt);
            break;
        case AST_IF_STMT:
            lower_if_stmt(ctx, stmt);
            break;
        case AST_FOR_STMT:
            lower_for_stmt(ctx, stmt);
            break;
        case AST_SWITCH_STMT:
            lower_switch_stmt(ctx, stmt);
            break;
        case AST_BREAK_STMT:
            lower_break_stmt(ctx, stmt);
            break;
        case AST_CONTINUE_STMT:
            lower_continue_stmt(ctx, stmt);
            break;
        case AST_CONTINUE_SWITCH_STMT:
            lower_continue_switch_stmt(ctx, stmt);
            break;
        case AST_DEFER_STMT:
            lower_defer_stmt(ctx, stmt);
            break;
        case AST_ERRDEFER_STMT:
            lower_errdefer_stmt(ctx, stmt);
            break;
        case AST_SUSPEND_STMT:
            lower_suspend_stmt(ctx, stmt);
            break;
        case AST_RESUME_STMT:
            lower_resume_stmt(ctx, stmt);
            break;
        case AST_TRY_CATCH_STMT:
            lower_try_catch_stmt(ctx, stmt);
            break;
        case AST_EXPR_STMT:
            lower_expr_stmt(ctx, stmt);
            break;
        case AST_BLOCK_STMT:
            lower_block_stmt(ctx, stmt);
            break;
        default:
            assert(0 && "Unhandled statement kind");
    }
}

static IrBasicBlock* lower_block_stmt(LowerContext* ctx, AstNode* block) {
    assert(block->kind == AST_BLOCK_STMT);

    // Iterate through statements in block and lower each
    for (size_t i = 0; i < block->data.block_stmt.stmt_count; i++) {
        lower_stmt(ctx, block->data.block_stmt.stmts[i]);
    }

    return ctx->current_block;
}

//=============================================================================
// Function Lowering
//=============================================================================

IrFunction* ir_lower_function(AstNode* func_node, CoroMetadata* coro_meta, Arena* arena) {
    assert(func_node->kind == AST_FUNCTION_DECL);

    // Extract function info from AST
    const char* name = func_node->data.function_decl.name;

    // Get return type - either from return_type node or void
    Type* return_type = NULL;
    if (func_node->data.function_decl.return_type) {
        return_type = func_node->data.function_decl.return_type->type;
    } else {
        // No return type specified means void - create a void type
        return_type = arena_alloc(arena, sizeof(Type), 8);
        return_type->kind = TYPE_VOID;
        return_type->size = 0;
        return_type->alignment = 1;
    }

    // Convert AST params to IR params
    size_t param_count = func_node->data.function_decl.param_count;
    IrParam* params = NULL;
    if (param_count > 0) {
        params = arena_alloc(arena, sizeof(IrParam) * param_count, 8);
        for (size_t i = 0; i < param_count; i++) {
            params[i].name = func_node->data.function_decl.params[i].name;
            params[i].type = func_node->data.function_decl.params[i].type->type;
            params[i].index = i;
        }
    }

    // Create IR function
    IrFunction* func = ir_function_create(name, return_type, params, param_count, arena);

    // Set coroutine metadata if provided
    if (coro_meta) {
        func->is_state_machine = true;
        func->coro_meta = coro_meta;
    }

    // Create lowering context
    LowerContext ctx = {
        .arena = arena,
        .current_function = func,
        .current_block = func->entry,
        .break_target = NULL,
        .continue_target = NULL,
        .error_cleanup_block = NULL,
        .defer_stack = NULL,
        .defer_count = 0,
        .defer_capacity = 0,
        .var_map = NULL,
        .var_map_count = 0,
        .var_map_capacity = 0,
    };

    // Initialize variable map
    var_map_init(&ctx);

    // Register function parameters in the variable map
    for (size_t i = 0; i < param_count; i++) {
        // Create IR parameter value
        IrValue* param_value = ir_alloc_value(IR_VALUE_PARAM, params[i].type, arena);
        param_value->data.param.index = i;
        param_value->data.param.name = params[i].name;

        // Register parameter in variable map
        var_map_register(&ctx, params[i].name, param_value);
    }

    // Lower function body
    if (func_node->data.function_decl.body) {
        lower_stmt(&ctx, func_node->data.function_decl.body);
    }

    // If this is a coroutine, transform to state machine
    if (func->is_state_machine) {
        func->state_machine = ir_transform_to_state_machine(func, coro_meta, arena);
    }

    return func;
}

//=============================================================================
// Module Lowering
//=============================================================================

IrModule* ir_lower_ast(AstNode* ast, Arena* arena) {
    assert(ast->kind == AST_MODULE);

    IrModule* module = arena_alloc(arena, sizeof(IrModule), 8);
    memset(module, 0, sizeof(IrModule));

    // Extract module name from AST
    module->name = ast->data.module.name;

    // Count functions
    size_t func_count = 0;
    for (size_t i = 0; i < ast->data.module.decl_count; i++) {
        if (ast->data.module.decls[i]->kind == AST_FUNCTION_DECL) {
            func_count++;
        }
    }

    // Allocate function array
    if (func_count > 0) {
        module->functions = arena_alloc(arena, sizeof(IrFunction*) * func_count, 8);
        module->function_count = 0;

        // Lower all functions
        for (size_t i = 0; i < ast->data.module.decl_count; i++) {
            if (ast->data.module.decls[i]->kind == AST_FUNCTION_DECL) {
                module->functions[module->function_count++] =
                    ir_lower_function(ast->data.module.decls[i], NULL, arena);
            }
        }
    }

    return module;
}

//=============================================================================
// State Machine Transformation
//=============================================================================

// Helper to create a label name for a state
__attribute__((unused))
static const char* state_label_name(uint32_t state_id, Arena* arena) {
    char* label = arena_alloc(arena, 32, 1);
    snprintf(label, 32, "state_%u", state_id);
    return label;
}

// Helper to create a resume label name for a state
static const char* resume_label_name(uint32_t state_id, Arena* arena) {
    char* label = arena_alloc(arena, 32, 1);
    snprintf(label, 32, "resume_%u", state_id);
    return label;
}

// Helper to map variable names to IrValues for state save/restore
static IrValue** map_live_vars_to_values(IrFunction* func, VarLiveness* live_vars,
                                          size_t var_count, Arena* arena) {
    if (var_count == 0) return NULL;

    IrValue** values = arena_alloc(arena, sizeof(IrValue*) * var_count, 8);
    for (size_t i = 0; i < var_count; i++) {
        // Create or lookup the temp for this variable
        IrValue* val = ir_function_new_temp(func, live_vars[i].var_type, arena);
        val->data.temp.name = live_vars[i].var_name;
        values[i] = val;
    }
    return values;
}

// Generate state dispatch logic (resume point)
static void generate_state_dispatch(IrFunction* func, IrStateMachine* sm,
                                     IrBasicBlock* dispatch_block, Arena* arena) {
    // Load the current state from the coroutine frame
    // state_var = frame_ptr->state
    IrInstruction* load_state = ir_alloc_instruction(IR_LOAD, arena);
    load_state->type = sm->state_var->type;
    load_state->data.load.dest = sm->state_var;
    load_state->data.load.addr = sm->frame_ptr;
    ir_block_add_instruction(dispatch_block, load_state, arena);

    // For computed goto approach (fast path):
    // Create a jump table with labels for each state
    // goto *resume_targets[state_var]

    // For switch-based approach (portable):
    // switch (state_var) {
    //   case 0: goto state_0;
    //   case 1: goto state_1;
    //   ...
    // }

    // We'll generate computed goto for performance
    // First, create the initial state block (state 0)
    IrBasicBlock* initial_block = ir_function_new_block(func, "state_0", arena);
    ir_function_add_block(func, initial_block, arena);

    // Add conditional jumps for each state
    IrBasicBlock* prev_check_block = dispatch_block;
    for (size_t i = 0; i < sm->state_count; i++) {
        IrStateMachineState* state = &sm->states[i];

        // Create constant for state ID
        IrValue* state_id_const = ir_alloc_value(IR_VALUE_CONSTANT, sm->state_var->type, arena);
        state_id_const->data.constant.data.int_val = state->state_id;

        // Create comparison: state_var == state_id
        IrValue* cmp_result = ir_function_new_temp(func, NULL, arena); // bool type
        IrInstruction* cmp = ir_alloc_instruction(IR_BINARY_OP, arena);
        cmp->data.binary_op.dest = cmp_result;
        cmp->data.binary_op.op = IR_OP_EQ;
        cmp->data.binary_op.left = sm->state_var;
        cmp->data.binary_op.right = state_id_const;
        ir_block_add_instruction(prev_check_block, cmp, arena);

        // Create next check block for fallthrough
        IrBasicBlock* next_check_block = NULL;
        if (i < sm->state_count - 1) {
            next_check_block = ir_function_new_block(func, "state_check", arena);
            ir_function_add_block(func, next_check_block, arena);
        }

        // Conditional jump: if (cmp_result) goto state_block else goto next_check
        IrInstruction* cond_jump = ir_alloc_instruction(IR_COND_JUMP, arena);
        cond_jump->data.cond_jump.cond = cmp_result;
        cond_jump->data.cond_jump.true_target = state->block;
        cond_jump->data.cond_jump.false_target = next_check_block ? next_check_block : initial_block;
        ir_block_add_instruction(prev_check_block, cond_jump, arena);

        // Add CFG edges
        ir_block_add_successor(prev_check_block, state->block, arena);
        ir_block_add_predecessor(state->block, prev_check_block, arena);

        if (next_check_block) {
            ir_block_add_successor(prev_check_block, next_check_block, arena);
            ir_block_add_predecessor(next_check_block, prev_check_block, arena);
            prev_check_block = next_check_block;
        }
    }
}

// Transform function body to extract code for each state
static void split_function_at_suspend_points(IrFunction* func, CoroMetadata* meta,
                                              IrStateMachine* sm, Arena* arena) {
    // This function splits the original IR blocks at suspend points
    // For each suspend point, we need to:
    // 1. Save live variables to the coroutine frame
    // 2. Update the state field in the frame
    // 3. Return control to caller
    // 4. On resume, restore live variables and continue

    // Walk through the function blocks and find suspend instructions
    for (size_t i = 0; i < func->block_count; i++) {
        IrBasicBlock* block = func->blocks[i];

        // Find suspend instructions in this block
        for (size_t j = 0; j < block->instruction_count; j++) {
            IrInstruction* instr = block->instructions[j];

            if (instr->kind == IR_SUSPEND) {
                uint32_t state_id = instr->data.suspend.state_id;

                // Find the corresponding suspend point in metadata
                SuspendPoint* sp = NULL;
                for (size_t k = 0; k < meta->suspend_count; k++) {
                    if (meta->suspend_points[k].state_id == state_id) {
                        sp = &meta->suspend_points[k];
                        break;
                    }
                }

                if (!sp) continue;

                // Generate state save instruction before suspend
                IrInstruction* save = ir_alloc_instruction(IR_STATE_SAVE, arena);
                save->data.state_save.frame = sm->frame_ptr;
                save->data.state_save.state_id = state_id;
                save->data.state_save.vars = map_live_vars_to_values(func, sp->live_vars,
                                                                      sp->live_var_count, arena);
                save->data.state_save.var_count = sp->live_var_count;

                // Insert save instruction before suspend
                // (In a full implementation, we'd insert it before the suspend in the instruction array)

                // Update suspend instruction with live vars
                instr->data.suspend.live_vars = save->data.state_save.vars;
                instr->data.suspend.live_var_count = save->data.state_save.var_count;
            }
        }
    }
}

IrStateMachine* ir_transform_to_state_machine(IrFunction* func, CoroMetadata* meta, Arena* arena) {
    assert(func->is_state_machine);
    assert(meta);

    IrStateMachine* sm = arena_alloc(arena, sizeof(IrStateMachine), 8);
    memset(sm, 0, sizeof(IrStateMachine));

    sm->metadata = meta;
    sm->initial_state = 0;
    sm->state_count = meta->state_count;

    // Create frame pointer type (pointer to coroutine frame struct)
    Type* frame_type = arena_alloc(arena, sizeof(Type), 8);
    frame_type->kind = TYPE_POINTER;
    frame_type->size = sizeof(void*);
    frame_type->alignment = sizeof(void*);

    // Create state variable type (uint32_t for state discriminator)
    Type* state_type = arena_alloc(arena, sizeof(Type), 8);
    state_type->kind = TYPE_U32;
    state_type->size = sizeof(uint32_t);
    state_type->alignment = sizeof(uint32_t);

    // Allocate coroutine frame pointer and state variable
    sm->frame_ptr = ir_function_new_temp(func, frame_type, arena);
    sm->frame_ptr->data.temp.name = "__coro_frame";

    sm->state_var = ir_function_new_temp(func, state_type, arena);
    sm->state_var->data.temp.name = "__coro_state";

    // Allocate state machine states array
    sm->states = arena_alloc(arena, sizeof(IrStateMachineState) * (sm->state_count + 1), 8);

    // Create initial state (state 0) - function entry point
    IrStateMachineState* initial_state = &sm->states[0];
    initial_state->state_id = 0;
    initial_state->state_struct = NULL; // No saved state for initial entry
    initial_state->block = ir_function_new_block(func, "state_0_entry", arena);
    ir_function_add_block(func, initial_state->block, arena);

    // Transform each suspend point into a resume state
    for (size_t i = 0; i < meta->suspend_count; i++) {
        SuspendPoint* sp = &meta->suspend_points[i];

        // State IDs start at 1 for the first suspend point
        uint32_t state_idx = sp->state_id + 1;
        IrStateMachineState* state = &sm->states[state_idx];
        state->state_id = sp->state_id;
        state->state_struct = &meta->state_structs[i];

        // Create resume block for this state
        state->block = ir_function_new_block(func, resume_label_name(sp->state_id, arena), arena);
        ir_function_add_block(func, state->block, arena);

        // Add state restore instructions at the beginning of resume block
        IrInstruction* restore = ir_alloc_instruction(IR_STATE_RESTORE, arena);
        restore->data.state_restore.frame = sm->frame_ptr;
        restore->data.state_restore.state_id = sp->state_id;
        restore->data.state_restore.vars = map_live_vars_to_values(func, sp->live_vars,
                                                                    sp->live_var_count, arena);
        restore->data.state_restore.var_count = sp->live_var_count;

        ir_block_add_instruction(state->block, restore, arena);
    }

    // Create a new entry block that does state dispatch
    IrBasicBlock* dispatch_block = ir_function_new_block(func, "dispatch", arena);

    // Generate the state dispatch logic
    generate_state_dispatch(func, sm, dispatch_block, arena);

    // Replace the function's entry block with dispatch block
    // (The old entry block becomes state_0)
    func->entry = dispatch_block;

    // Insert dispatch block at the beginning
    ir_function_add_block(func, dispatch_block, arena);

    // Split the function body at suspend points and generate save instructions
    split_function_at_suspend_points(func, meta, sm, arena);

    return sm;
}

//=============================================================================
// Cleanup Transformation
//=============================================================================

void ir_insert_defer_cleanup(IrFunction* func, IrBasicBlock* block, Arena* arena) {
    // Insert defer cleanup instructions at function exit points
    // For async functions, this includes:
    // - Normal returns
    // - Error propagation
    // - Coroutine destruction (when suspended coroutine is dropped)

    for (size_t i = 0; i < func->defer_stack_count; i++) {
        IrDeferCleanup* cleanup = func->defer_stack[i];
        if (!cleanup->is_errdefer) {
            // Insert cleanup instructions
            for (size_t j = 0; j < cleanup->instruction_count; j++) {
                ir_block_add_instruction(block, cleanup->instructions[j], arena);
            }
        }
    }
}

void ir_insert_errdefer_cleanup(IrFunction* func, IrBasicBlock* block, Arena* arena) {
    // Insert errdefer cleanup instructions at error exit points
    // Errdefer only executes on error paths, not on normal returns
    // In async functions, this includes:
    // - try expression error branches
    // - Error propagation from awaited calls

    for (size_t i = 0; i < func->defer_stack_count; i++) {
        IrDeferCleanup* cleanup = func->defer_stack[i];
        if (cleanup->is_errdefer) {
            // Insert cleanup instructions
            for (size_t j = 0; j < cleanup->instruction_count; j++) {
                ir_block_add_instruction(block, cleanup->instructions[j], arena);
            }
        }
    }
}

// Insert defer cleanup at suspend points (for async functions)
// Defer blocks don't execute at suspend points, only at actual function exit
// This is important for resource management in async functions
void ir_insert_async_defer_tracking(IrFunction* func, Arena* arena) {
    if (!func->is_state_machine) return;

    // For each defer in the stack, we need to track which defers are active
    // when we suspend. This information is stored in the coroutine frame
    // so that if the coroutine is destroyed while suspended, the appropriate
    // cleanup can be performed.

    // In the coroutine frame, we can add a bitfield tracking which defers
    // are active at each suspend point. This allows the runtime to execute
    // the correct cleanup if the coroutine is dropped.

    (void)func;
    (void)arena;
    // TODO: Implement defer tracking in coroutine frames
}

//=============================================================================
// Debug Output
//=============================================================================

void ir_print_debug(IrNode* ir, FILE* out) {
    if (!ir || !out) return;

    switch (ir->kind) {
        case IR_MODULE:
            fprintf(out, "Module: %s\n", ir->data.module->name);
            for (size_t i = 0; i < ir->data.module->function_count; i++) {
                fprintf(out, "  Function %zu\n", i);
            }
            break;
        case IR_FUNCTION:
            fprintf(out, "Function: %s\n", ir->data.function->name);
            fprintf(out, "  Blocks: %zu\n", ir->data.function->block_count);
            break;
        default:
            fprintf(out, "IR node kind: %d\n", ir->kind);
    }
}
