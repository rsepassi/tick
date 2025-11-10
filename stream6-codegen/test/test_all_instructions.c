#include "../include/codegen.h"
#include "ir.h"
#include "type.h"
#include "arena.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// External mock functions from test_codegen.c
extern void arena_init(Arena* arena, size_t block_size);
extern void arena_free(Arena* arena);
extern void* arena_alloc(Arena* arena, size_t size, size_t alignment);
extern void error_list_init(ErrorList* list, Arena* arena);
extern void error_list_add(ErrorList* list, ErrorKind kind, SourceLocation loc, const char* fmt, ...);
extern IrNode* ir_alloc_node(IrNodeKind kind, Arena* arena);
extern IrValue* ir_alloc_value(IrValueKind kind, Type* type, Arena* arena);
extern IrInstruction* ir_alloc_instruction(IrNodeKind kind, Arena* arena);
extern IrBasicBlock* ir_alloc_block(uint32_t id, const char* label, Arena* arena);
extern void ir_block_add_instruction(IrBasicBlock* block, IrInstruction* instr, Arena* arena);
extern IrFunction* ir_function_create(const char* name, Type* return_type, IrParam* params, size_t param_count, Arena* arena);
extern void ir_function_add_block(IrFunction* func, IrBasicBlock* block, Arena* arena);
extern Type* type_new_primitive(TypeKind kind, Arena* arena);

// Test all supported IR instruction types
void test_all_instructions() {
    printf("Test: All IR instruction types... ");

    Arena arena;
    ErrorList errors;
    arena_init(&arena, 4096);
    error_list_init(&errors, &arena);

    Type* i32_type = type_new_primitive(TYPE_I32, &arena);
    Type* void_type = type_new_primitive(TYPE_VOID, &arena);

    // Create function
    IrFunction* func = ir_function_create("test_all", void_type, NULL, 0, &arena);
    func->next_temp_id = 10; // Allocate some temps

    // Create blocks
    IrBasicBlock* entry = ir_alloc_block(0, "entry", &arena);
    IrBasicBlock* true_block = ir_alloc_block(1, "true_block", &arena);
    IrBasicBlock* false_block = ir_alloc_block(2, "false_block", &arena);
    IrBasicBlock* merge = ir_alloc_block(3, "merge", &arena);

    // Add predecessors to blocks that will be jumped to
    entry->predecessor_count = 0;
    true_block->predecessor_count = 1;
    false_block->predecessor_count = 1;
    merge->predecessor_count = 2;

    // Create some values
    IrValue* temp1 = ir_alloc_value(IR_VALUE_TEMP, i32_type, &arena);
    temp1->data.temp.id = 1;
    temp1->data.temp.name = "temp1";

    IrValue* temp2 = ir_alloc_value(IR_VALUE_TEMP, i32_type, &arena);
    temp2->data.temp.id = 2;
    temp2->data.temp.name = "temp2";

    IrValue* const_5 = ir_alloc_value(IR_VALUE_CONSTANT, i32_type, &arena);
    const_5->data.constant.data.int_val = 5;

    IrValue* const_10 = ir_alloc_value(IR_VALUE_CONSTANT, i32_type, &arena);
    const_10->data.constant.data.int_val = 10;

    // Test IR_ASSIGN
    IrInstruction* assign = ir_alloc_instruction(IR_ASSIGN, &arena);
    assign->data.assign.dest = temp1;
    assign->data.assign.src = const_5;
    ir_block_add_instruction(entry, assign, &arena);

    // Test IR_BINARY_OP
    IrInstruction* binop = ir_alloc_instruction(IR_BINARY_OP, &arena);
    binop->data.binary_op.dest = temp2;
    binop->data.binary_op.op = IR_OP_ADD;
    binop->data.binary_op.left = temp1;
    binop->data.binary_op.right = const_10;
    ir_block_add_instruction(entry, binop, &arena);

    // Test IR_UNARY_OP
    IrValue* temp3 = ir_alloc_value(IR_VALUE_TEMP, i32_type, &arena);
    temp3->data.temp.id = 3;
    IrInstruction* unary = ir_alloc_instruction(IR_UNARY_OP, &arena);
    unary->data.unary_op.dest = temp3;
    unary->data.unary_op.op = IR_OP_NEG;
    unary->data.unary_op.operand = temp2;
    ir_block_add_instruction(entry, unary, &arena);

    // Test IR_LOAD
    IrValue* ptr = ir_alloc_value(IR_VALUE_TEMP, i32_type, &arena);
    ptr->data.temp.id = 4;
    IrValue* loaded = ir_alloc_value(IR_VALUE_TEMP, i32_type, &arena);
    loaded->data.temp.id = 5;
    IrInstruction* load = ir_alloc_instruction(IR_LOAD, &arena);
    load->data.load.dest = loaded;
    load->data.load.addr = ptr;
    ir_block_add_instruction(entry, load, &arena);

    // Test IR_STORE
    IrInstruction* store = ir_alloc_instruction(IR_STORE, &arena);
    store->data.store.addr = ptr;
    store->data.store.value = temp1;
    ir_block_add_instruction(entry, store, &arena);

    // Test IR_CAST
    IrValue* casted = ir_alloc_value(IR_VALUE_TEMP, i32_type, &arena);
    casted->data.temp.id = 6;
    IrInstruction* cast = ir_alloc_instruction(IR_CAST, &arena);
    cast->data.cast.dest = casted;
    cast->data.cast.target_type = i32_type;
    cast->data.cast.value = temp1;
    ir_block_add_instruction(entry, cast, &arena);

    // Test IR_COND_JUMP
    IrValue* cond = ir_alloc_value(IR_VALUE_TEMP, i32_type, &arena);
    cond->data.temp.id = 7;
    IrInstruction* cond_jump = ir_alloc_instruction(IR_COND_JUMP, &arena);
    cond_jump->data.cond_jump.cond = cond;
    cond_jump->data.cond_jump.true_target = true_block;
    cond_jump->data.cond_jump.false_target = false_block;
    ir_block_add_instruction(entry, cond_jump, &arena);

    // Test IR_JUMP
    IrInstruction* jump1 = ir_alloc_instruction(IR_JUMP, &arena);
    jump1->data.jump.target = merge;
    ir_block_add_instruction(true_block, jump1, &arena);

    IrInstruction* jump2 = ir_alloc_instruction(IR_JUMP, &arena);
    jump2->data.jump.target = merge;
    ir_block_add_instruction(false_block, jump2, &arena);

    // Test IR_RETURN
    IrInstruction* ret = ir_alloc_instruction(IR_RETURN, &arena);
    ret->data.ret.value = NULL; // void return
    ir_block_add_instruction(merge, ret, &arena);

    // Add blocks to function
    ir_function_add_block(func, entry, &arena);
    ir_function_add_block(func, true_block, &arena);
    ir_function_add_block(func, false_block, &arena);
    ir_function_add_block(func, merge, &arena);

    // Create module
    IrModule* mod = (IrModule*)arena_alloc(&arena, sizeof(IrModule), _Alignof(IrModule));
    mod->name = "test_all";
    mod->function_count = 1;
    mod->functions = (IrFunction**)arena_alloc(&arena, sizeof(IrFunction*), _Alignof(IrFunction*));
    mod->functions[0] = func;

    IrNode* module_node = ir_alloc_node(IR_MODULE, &arena);
    module_node->data.module = mod;

    // Generate code
    CodegenContext ctx;
    codegen_init(&ctx, &arena, &errors, "test_all");

    FILE* header = fopen("/tmp/test_all.h", "w");
    FILE* source = fopen("/tmp/test_all.c", "w");
    ctx.header_out = header;
    ctx.source_out = source;

    codegen_emit_module(module_node, &ctx);

    fclose(header);
    fclose(source);

    // Try to compile
    int result = system("gcc -Wall -Wextra -std=c11 -ffreestanding -I/tmp -c /tmp/test_all.c -o /tmp/test_all.o 2>&1");

    arena_free(&arena);

    if (result == 0) {
        printf("PASSED\n");
    } else {
        printf("FAILED (compilation errors)\n");
        system("cat /tmp/test_all.c");
    }
}

int main(void) {
    printf("=== Testing All Instruction Types ===\n\n");
    test_all_instructions();
    printf("\nTest completed!\n");
    return 0;
}
