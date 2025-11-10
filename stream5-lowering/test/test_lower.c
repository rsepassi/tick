#include "../ir.h"
#include "../ast.h"
#include "../type.h"
#include "../symbol.h"
#include "../coro_metadata.h"
#include "../arena.h"
#include "../error.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

// Test framework
#define TEST(name) void test_##name(void)
#define RUN_TEST(name) do { \
    printf("Running test: %s\n", #name); \
    test_##name(); \
    printf("  PASSED\n"); \
} while(0)

//=============================================================================
// Mock Helpers
//=============================================================================

// Create a mock AST node for testing
static AstNode* mock_ast_literal(Type* type, Arena* arena) {
    AstNode* node = arena_alloc(arena, sizeof(AstNode), 8);
    memset(node, 0, sizeof(AstNode));
    node->kind = AST_LITERAL_EXPR;
    node->type = type;
    node->loc.line = 1;
    node->loc.column = 1;
    node->loc.filename = "test.tick";
    return node;
}

static AstNode* mock_ast_identifier(const char* name, Type* type, Arena* arena) {
    AstNode* node = arena_alloc(arena, sizeof(AstNode), 8);
    memset(node, 0, sizeof(AstNode));
    node->kind = AST_IDENTIFIER_EXPR;
    node->type = type;
    node->loc.line = 1;
    node->loc.column = 1;
    node->loc.filename = "test.tick";
    return node;
}

static AstNode* mock_ast_function(const char* name, Arena* arena) {
    AstNode* node = arena_alloc(arena, sizeof(AstNode), 8);
    memset(node, 0, sizeof(AstNode));
    node->kind = AST_FUNCTION_DECL;
    node->loc.line = 1;
    node->loc.column = 1;
    node->loc.filename = "test.tick";
    return node;
}

static Type* mock_type_i32(Arena* arena) {
    Type* type = arena_alloc(arena, sizeof(Type), 8);
    memset(type, 0, sizeof(Type));
    type->kind = TYPE_I32;
    type->size = 4;
    type->alignment = 4;
    return type;
}

static Type* mock_type_void(Arena* arena) {
    Type* type = arena_alloc(arena, sizeof(Type), 8);
    memset(type, 0, sizeof(Type));
    type->kind = TYPE_VOID;
    type->size = 0;
    type->alignment = 1;
    return type;
}

//=============================================================================
// IR Construction Tests
//=============================================================================

TEST(ir_alloc_node) {
    Arena arena;
    arena_init(&arena, 4096);

    IrNode* node = ir_alloc_node(IR_FUNCTION, &arena);
    assert(node != NULL);
    assert(node->kind == IR_FUNCTION);

    arena_free(&arena);
}

TEST(ir_alloc_value) {
    Arena arena;
    arena_init(&arena, 4096);

    Type* type = mock_type_i32(&arena);
    IrValue* value = ir_alloc_value(IR_VALUE_TEMP, type, &arena);

    assert(value != NULL);
    assert(value->kind == IR_VALUE_TEMP);
    assert(value->type == type);

    arena_free(&arena);
}

TEST(ir_alloc_instruction) {
    Arena arena;
    arena_init(&arena, 4096);

    IrInstruction* instr = ir_alloc_instruction(IR_ASSIGN, &arena);
    assert(instr != NULL);
    assert(instr->kind == IR_ASSIGN);

    arena_free(&arena);
}

TEST(ir_alloc_block) {
    Arena arena;
    arena_init(&arena, 4096);

    IrBasicBlock* block = ir_alloc_block(0, "entry", &arena);
    assert(block != NULL);
    assert(block->id == 0);
    assert(strcmp(block->label, "entry") == 0);
    assert(block->instruction_count == 0);

    arena_free(&arena);
}

//=============================================================================
// IR Function Builder Tests
//=============================================================================

TEST(ir_function_create) {
    Arena arena;
    arena_init(&arena, 4096);

    Type* return_type = mock_type_i32(&arena);
    IrFunction* func = ir_function_create("test_func", return_type, NULL, 0, &arena);

    assert(func != NULL);
    assert(strcmp(func->name, "test_func") == 0);
    assert(func->return_type == return_type);
    assert(func->param_count == 0);
    assert(func->entry != NULL);
    assert(func->block_count == 1);
    assert(func->next_temp_id == 0);
    assert(func->next_block_id == 1);

    arena_free(&arena);
}

TEST(ir_function_new_temp) {
    Arena arena;
    arena_init(&arena, 4096);

    Type* return_type = mock_type_void(&arena);
    IrFunction* func = ir_function_create("test", return_type, NULL, 0, &arena);

    Type* temp_type = mock_type_i32(&arena);
    IrValue* temp1 = ir_function_new_temp(func, temp_type, &arena);
    IrValue* temp2 = ir_function_new_temp(func, temp_type, &arena);

    assert(temp1 != NULL);
    assert(temp1->kind == IR_VALUE_TEMP);
    assert(temp1->data.temp.id == 0);

    assert(temp2 != NULL);
    assert(temp2->kind == IR_VALUE_TEMP);
    assert(temp2->data.temp.id == 1);

    assert(func->next_temp_id == 2);

    arena_free(&arena);
}

TEST(ir_function_new_block) {
    Arena arena;
    arena_init(&arena, 4096);

    Type* return_type = mock_type_void(&arena);
    IrFunction* func = ir_function_create("test", return_type, NULL, 0, &arena);

    uint32_t expected_id = func->next_block_id;
    IrBasicBlock* block = ir_function_new_block(func, "test_block", &arena);

    assert(block != NULL);
    assert(block->id == expected_id);
    assert(strcmp(block->label, "test_block") == 0);

    arena_free(&arena);
}

TEST(ir_function_add_block) {
    Arena arena;
    arena_init(&arena, 4096);

    Type* return_type = mock_type_void(&arena);
    IrFunction* func = ir_function_create("test", return_type, NULL, 0, &arena);

    size_t initial_count = func->block_count;
    IrBasicBlock* block = ir_function_new_block(func, "new_block", &arena);
    ir_function_add_block(func, block, &arena);

    assert(func->block_count == initial_count + 1);
    assert(func->blocks[func->block_count - 1] == block);

    arena_free(&arena);
}

//=============================================================================
// Basic Block Builder Tests
//=============================================================================

TEST(ir_block_add_instruction) {
    Arena arena;
    arena_init(&arena, 4096);

    IrBasicBlock* block = ir_alloc_block(0, "test", &arena);
    IrInstruction* instr1 = ir_alloc_instruction(IR_ASSIGN, &arena);
    IrInstruction* instr2 = ir_alloc_instruction(IR_RETURN, &arena);

    ir_block_add_instruction(block, instr1, &arena);
    ir_block_add_instruction(block, instr2, &arena);

    assert(block->instruction_count == 2);
    assert(block->instructions[0] == instr1);
    assert(block->instructions[1] == instr2);

    arena_free(&arena);
}

TEST(ir_block_add_predecessor) {
    Arena arena;
    arena_init(&arena, 4096);

    IrBasicBlock* block = ir_alloc_block(0, "block", &arena);
    IrBasicBlock* pred1 = ir_alloc_block(1, "pred1", &arena);
    IrBasicBlock* pred2 = ir_alloc_block(2, "pred2", &arena);

    ir_block_add_predecessor(block, pred1, &arena);
    ir_block_add_predecessor(block, pred2, &arena);

    assert(block->predecessor_count == 2);
    assert(block->predecessors[0] == pred1);
    assert(block->predecessors[1] == pred2);

    arena_free(&arena);
}

TEST(ir_block_add_successor) {
    Arena arena;
    arena_init(&arena, 4096);

    IrBasicBlock* block = ir_alloc_block(0, "block", &arena);
    IrBasicBlock* succ1 = ir_alloc_block(1, "succ1", &arena);
    IrBasicBlock* succ2 = ir_alloc_block(2, "succ2", &arena);

    ir_block_add_successor(block, succ1, &arena);
    ir_block_add_successor(block, succ2, &arena);

    assert(block->successor_count == 2);
    assert(block->successors[0] == succ1);
    assert(block->successors[1] == succ2);

    arena_free(&arena);
}

//=============================================================================
// IR Value Tests
//=============================================================================

TEST(ir_value_temp) {
    Arena arena;
    arena_init(&arena, 4096);

    Type* type = mock_type_i32(&arena);
    IrValue* temp = ir_alloc_value(IR_VALUE_TEMP, type, &arena);
    temp->data.temp.id = 42;
    temp->data.temp.name = "test_temp";

    assert(temp->kind == IR_VALUE_TEMP);
    assert(temp->type == type);
    assert(temp->data.temp.id == 42);
    assert(strcmp(temp->data.temp.name, "test_temp") == 0);

    arena_free(&arena);
}

TEST(ir_value_constant) {
    Arena arena;
    arena_init(&arena, 4096);

    Type* type = mock_type_i32(&arena);
    IrValue* constant = ir_alloc_value(IR_VALUE_CONSTANT, type, &arena);
    constant->data.constant.data.int_val = 123;

    assert(constant->kind == IR_VALUE_CONSTANT);
    assert(constant->type == type);
    assert(constant->data.constant.data.int_val == 123);

    arena_free(&arena);
}

TEST(ir_value_param) {
    Arena arena;
    arena_init(&arena, 4096);

    Type* type = mock_type_i32(&arena);
    IrValue* param = ir_alloc_value(IR_VALUE_PARAM, type, &arena);
    param->data.param.index = 0;
    param->data.param.name = "arg0";

    assert(param->kind == IR_VALUE_PARAM);
    assert(param->type == type);
    assert(param->data.param.index == 0);
    assert(strcmp(param->data.param.name, "arg0") == 0);

    arena_free(&arena);
}

//=============================================================================
// IR Instruction Tests
//=============================================================================

TEST(ir_instruction_assign) {
    Arena arena;
    arena_init(&arena, 4096);

    Type* type = mock_type_i32(&arena);
    IrValue* dest = ir_alloc_value(IR_VALUE_TEMP, type, &arena);
    IrValue* src = ir_alloc_value(IR_VALUE_CONSTANT, type, &arena);

    IrInstruction* instr = ir_alloc_instruction(IR_ASSIGN, &arena);
    instr->type = type;
    instr->data.assign.dest = dest;
    instr->data.assign.src = src;

    assert(instr->kind == IR_ASSIGN);
    assert(instr->data.assign.dest == dest);
    assert(instr->data.assign.src == src);

    arena_free(&arena);
}

TEST(ir_instruction_binary_op) {
    Arena arena;
    arena_init(&arena, 4096);

    Type* type = mock_type_i32(&arena);
    IrValue* dest = ir_alloc_value(IR_VALUE_TEMP, type, &arena);
    IrValue* left = ir_alloc_value(IR_VALUE_TEMP, type, &arena);
    IrValue* right = ir_alloc_value(IR_VALUE_TEMP, type, &arena);

    IrInstruction* instr = ir_alloc_instruction(IR_BINARY_OP, &arena);
    instr->type = type;
    instr->data.binary_op.dest = dest;
    instr->data.binary_op.op = IR_OP_ADD;
    instr->data.binary_op.left = left;
    instr->data.binary_op.right = right;

    assert(instr->kind == IR_BINARY_OP);
    assert(instr->data.binary_op.dest == dest);
    assert(instr->data.binary_op.op == IR_OP_ADD);
    assert(instr->data.binary_op.left == left);
    assert(instr->data.binary_op.right == right);

    arena_free(&arena);
}

TEST(ir_instruction_call) {
    Arena arena;
    arena_init(&arena, 4096);

    Type* ret_type = mock_type_i32(&arena);
    IrValue* dest = ir_alloc_value(IR_VALUE_TEMP, ret_type, &arena);
    IrValue* func = ir_alloc_value(IR_VALUE_GLOBAL, NULL, &arena);

    IrInstruction* instr = ir_alloc_instruction(IR_CALL, &arena);
    instr->type = ret_type;
    instr->data.call.dest = dest;
    instr->data.call.func = func;
    instr->data.call.args = NULL;
    instr->data.call.arg_count = 0;

    assert(instr->kind == IR_CALL);
    assert(instr->data.call.dest == dest);
    assert(instr->data.call.func == func);
    assert(instr->data.call.arg_count == 0);

    arena_free(&arena);
}

TEST(ir_instruction_return) {
    Arena arena;
    arena_init(&arena, 4096);

    Type* type = mock_type_i32(&arena);
    IrValue* value = ir_alloc_value(IR_VALUE_TEMP, type, &arena);

    IrInstruction* instr = ir_alloc_instruction(IR_RETURN, &arena);
    instr->data.ret.value = value;

    assert(instr->kind == IR_RETURN);
    assert(instr->data.ret.value == value);

    arena_free(&arena);
}

TEST(ir_instruction_jump) {
    Arena arena;
    arena_init(&arena, 4096);

    IrBasicBlock* target = ir_alloc_block(1, "target", &arena);

    IrInstruction* instr = ir_alloc_instruction(IR_JUMP, &arena);
    instr->data.jump.target = target;

    assert(instr->kind == IR_JUMP);
    assert(instr->data.jump.target == target);

    arena_free(&arena);
}

TEST(ir_instruction_cond_jump) {
    Arena arena;
    arena_init(&arena, 4096);

    Type* bool_type = arena_alloc(&arena, sizeof(Type), 8);
    bool_type->kind = TYPE_BOOL;

    IrValue* cond = ir_alloc_value(IR_VALUE_TEMP, bool_type, &arena);
    IrBasicBlock* true_target = ir_alloc_block(1, "true", &arena);
    IrBasicBlock* false_target = ir_alloc_block(2, "false", &arena);

    IrInstruction* instr = ir_alloc_instruction(IR_COND_JUMP, &arena);
    instr->data.cond_jump.cond = cond;
    instr->data.cond_jump.true_target = true_target;
    instr->data.cond_jump.false_target = false_target;

    assert(instr->kind == IR_COND_JUMP);
    assert(instr->data.cond_jump.cond == cond);
    assert(instr->data.cond_jump.true_target == true_target);
    assert(instr->data.cond_jump.false_target == false_target);

    arena_free(&arena);
}

//=============================================================================
// Function Lowering Tests
//=============================================================================

TEST(ir_lower_function_basic) {
    Arena arena;
    arena_init(&arena, 4096);

    AstNode* func_node = mock_ast_function("test_func", &arena);
    IrFunction* ir_func = ir_lower_function(func_node, NULL, &arena);

    assert(ir_func != NULL);
    assert(ir_func->entry != NULL);
    assert(ir_func->is_state_machine == false);

    arena_free(&arena);
}

TEST(ir_lower_function_with_coro_metadata) {
    Arena arena;
    arena_init(&arena, 4096);

    // Create mock coroutine metadata
    CoroMetadata* meta = arena_alloc(&arena, sizeof(CoroMetadata), 8);
    memset(meta, 0, sizeof(CoroMetadata));
    meta->function_name = "async_func";
    meta->suspend_count = 2;
    meta->state_count = 2;

    AstNode* func_node = mock_ast_function("async_func", &arena);
    IrFunction* ir_func = ir_lower_function(func_node, meta, &arena);

    assert(ir_func != NULL);
    assert(ir_func->is_state_machine == true);
    assert(ir_func->coro_meta == meta);

    arena_free(&arena);
}

//=============================================================================
// State Machine Transformation Tests
//=============================================================================

TEST(ir_transform_to_state_machine) {
    Arena arena;
    arena_init(&arena, 4096);

    // Create function with coroutine metadata
    Type* void_type = mock_type_void(&arena);
    IrFunction* func = ir_function_create("coro", void_type, NULL, 0, &arena);
    func->is_state_machine = true;

    // Create mock coroutine metadata
    CoroMetadata* meta = arena_alloc(&arena, sizeof(CoroMetadata), 8);
    memset(meta, 0, sizeof(CoroMetadata));
    meta->function_name = "coro";
    meta->suspend_count = 2;
    meta->state_count = 2;

    // Allocate suspend points
    meta->suspend_points = arena_alloc(&arena, sizeof(SuspendPoint) * 2, 8);
    meta->suspend_points[0].state_id = 0;
    meta->suspend_points[0].live_var_count = 0;
    meta->suspend_points[1].state_id = 1;
    meta->suspend_points[1].live_var_count = 0;

    // Allocate state structs
    meta->state_structs = arena_alloc(&arena, sizeof(StateStruct) * 2, 8);
    meta->state_structs[0].state_id = 0;
    meta->state_structs[0].field_count = 0;
    meta->state_structs[1].state_id = 1;
    meta->state_structs[1].field_count = 0;

    func->coro_meta = meta;

    // Transform to state machine
    IrStateMachine* sm = ir_transform_to_state_machine(func, meta, &arena);

    assert(sm != NULL);
    assert(sm->metadata == meta);
    assert(sm->state_count == 2);
    assert(sm->initial_state == 0);
    assert(sm->frame_ptr != NULL);
    assert(sm->state_var != NULL);

    arena_free(&arena);
}

//=============================================================================
// Module Lowering Tests
//=============================================================================

TEST(ir_lower_ast_module) {
    Arena arena;
    arena_init(&arena, 4096);

    AstNode* module_node = arena_alloc(&arena, sizeof(AstNode), 8);
    memset(module_node, 0, sizeof(AstNode));
    module_node->kind = AST_MODULE;

    IrModule* module = ir_lower_ast(module_node, &arena);

    assert(module != NULL);
    assert(module->name != NULL);

    arena_free(&arena);
}

//=============================================================================
// Debug Output Tests
//=============================================================================

TEST(ir_print_debug_module) {
    Arena arena;
    arena_init(&arena, 4096);

    IrNode* node = ir_alloc_node(IR_MODULE, &arena);
    node->data.module = arena_alloc(&arena, sizeof(IrModule), 8);
    memset(node->data.module, 0, sizeof(IrModule));
    node->data.module->name = "test_module";
    node->data.module->function_count = 0;

    // Should not crash
    ir_print_debug(node, stdout);

    arena_free(&arena);
}

TEST(ir_print_debug_function) {
    Arena arena;
    arena_init(&arena, 4096);

    Type* void_type = mock_type_void(&arena);
    IrFunction* func = ir_function_create("test", void_type, NULL, 0, &arena);

    IrNode* node = ir_alloc_node(IR_FUNCTION, &arena);
    node->data.function = func;

    // Should not crash
    ir_print_debug(node, stdout);

    arena_free(&arena);
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(void) {
    printf("Running IR Lowering Tests\n");
    printf("==========================\n\n");

    // IR Construction Tests
    RUN_TEST(ir_alloc_node);
    RUN_TEST(ir_alloc_value);
    RUN_TEST(ir_alloc_instruction);
    RUN_TEST(ir_alloc_block);

    // Function Builder Tests
    RUN_TEST(ir_function_create);
    RUN_TEST(ir_function_new_temp);
    RUN_TEST(ir_function_new_block);
    RUN_TEST(ir_function_add_block);

    // Basic Block Tests
    RUN_TEST(ir_block_add_instruction);
    RUN_TEST(ir_block_add_predecessor);
    RUN_TEST(ir_block_add_successor);

    // Value Tests
    RUN_TEST(ir_value_temp);
    RUN_TEST(ir_value_constant);
    RUN_TEST(ir_value_param);

    // Instruction Tests
    RUN_TEST(ir_instruction_assign);
    RUN_TEST(ir_instruction_binary_op);
    RUN_TEST(ir_instruction_call);
    RUN_TEST(ir_instruction_return);
    RUN_TEST(ir_instruction_jump);
    RUN_TEST(ir_instruction_cond_jump);

    // Function Lowering Tests
    RUN_TEST(ir_lower_function_basic);
    RUN_TEST(ir_lower_function_with_coro_metadata);

    // State Machine Tests
    RUN_TEST(ir_transform_to_state_machine);

    // Module Tests
    RUN_TEST(ir_lower_ast_module);

    // Debug Tests
    RUN_TEST(ir_print_debug_module);
    RUN_TEST(ir_print_debug_function);

    printf("\n==========================\n");
    printf("All tests passed!\n");

    return 0;
}
