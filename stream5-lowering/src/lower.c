#include "../ir.h"
#include "../ast.h"
#include "../type.h"
#include "../symbol.h"
#include "../coro_metadata.h"
#include "../arena.h"
#include "../error.h"
#include <string.h>
#include <assert.h>

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
} LowerContext;

// Forward declarations
static IrValue* lower_expr(LowerContext* ctx, AstNode* expr);
static void lower_stmt(LowerContext* ctx, AstNode* stmt);
static IrBasicBlock* lower_block_stmt(LowerContext* ctx, AstNode* block);

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

    // For now, create a temp with the identifier name
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
    if (expr->type->kind != TYPE_VOID) {
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
    alloca_instr->data.alloca.dest = ir_function_new_temp(ctx->current_function,
        stmt->type, ctx->arena);
    alloca_instr->data.alloca.alloc_type = stmt->type;
    alloca_instr->data.alloca.count = 1;

    ir_block_add_instruction(ctx->current_block, alloca_instr, ctx->arena);

    // Store initial value
    if (init_value) {
        IrInstruction* store_instr = ir_alloc_instruction(IR_STORE, ctx->arena);
        store_instr->loc = stmt->loc;
        store_instr->data.store.addr = alloca_instr->data.alloca.dest;
        store_instr->data.store.value = init_value;

        ir_block_add_instruction(ctx->current_block, store_instr, ctx->arena);
    }
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
    IrBasicBlock* saved_block = ctx->current_block;
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
    IrValue* value = lower_expr(ctx, NULL); // TODO: Get value from stmt

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

    // Create suspend instruction
    IrInstruction* suspend = ir_alloc_instruction(IR_SUSPEND, ctx->arena);
    suspend->loc = stmt->loc;
    // TODO: Get state_id and live vars from coroutine metadata
    suspend->data.suspend.state_id = 0;
    suspend->data.suspend.live_vars = NULL;
    suspend->data.suspend.live_var_count = 0;

    ir_block_add_instruction(ctx->current_block, suspend, ctx->arena);
}

static void lower_expr_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_EXPR_STMT);

    // Just lower the expression (side effects matter)
    lower_expr(ctx, stmt->data.expr_stmt.expr);
}

static void lower_stmt(LowerContext* ctx, AstNode* stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case AST_LET_STMT:
        case AST_VAR_STMT:
            lower_let_stmt(ctx, stmt);
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
        case AST_DEFER_STMT:
            lower_defer_stmt(ctx, stmt);
            break;
        case AST_ERRDEFER_STMT:
            lower_errdefer_stmt(ctx, stmt);
            break;
        case AST_SUSPEND_STMT:
            lower_suspend_stmt(ctx, stmt);
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
    Type* return_type = func_node->type; // Function type already resolved

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
    };

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

IrStateMachine* ir_transform_to_state_machine(IrFunction* func, CoroMetadata* meta, Arena* arena) {
    assert(func->is_state_machine);
    assert(meta);

    IrStateMachine* sm = arena_alloc(arena, sizeof(IrStateMachine), 8);
    memset(sm, 0, sizeof(IrStateMachine));

    sm->metadata = meta;
    sm->initial_state = 0;
    sm->state_count = meta->state_count;

    // Allocate coroutine frame
    sm->frame_ptr = ir_function_new_temp(func, NULL, arena); // TODO: Frame type
    sm->state_var = ir_function_new_temp(func, NULL, arena); // TODO: State type

    // Allocate state machine states
    sm->states = arena_alloc(arena, sizeof(IrStateMachineState) * sm->state_count, 8);

    // Transform each suspend point into a state
    for (size_t i = 0; i < meta->suspend_count; i++) {
        SuspendPoint* sp = &meta->suspend_points[i];

        IrStateMachineState* state = &sm->states[sp->state_id];
        state->state_id = sp->state_id;
        state->state_struct = &meta->state_structs[sp->state_id];

        // Create block for this state
        state->block = ir_function_new_block(func, "state", arena);

        // Add state restore instructions
        IrInstruction* restore = ir_alloc_instruction(IR_STATE_RESTORE, arena);
        restore->data.state_restore.frame = sm->frame_ptr;
        restore->data.state_restore.state_id = sp->state_id;
        // TODO: Map live vars
        restore->data.state_restore.vars = NULL;
        restore->data.state_restore.var_count = 0;

        ir_block_add_instruction(state->block, restore, arena);
        ir_function_add_block(func, state->block, arena);
    }

    return sm;
}

//=============================================================================
// Cleanup Transformation
//=============================================================================

void ir_insert_defer_cleanup(IrFunction* func, IrBasicBlock* block, Arena* arena) {
    // Insert defer cleanup instructions at function exit points
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
