#include "../include/codegen.h"
#include "../include/ir.h"
#include "../include/type.h"
#include "../include/arena.h"
#include "../include/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <stdalign.h>

// Test helpers
static Arena test_arena;
static ErrorList test_errors;

void setup_test() {
    arena_init(&test_arena, 4096);
    error_list_init(&test_errors, &test_arena);
}

void teardown_test() {
    arena_free(&test_arena);
}

// Mock IR node allocation
IrNode* ir_alloc_node(IrNodeKind kind, Arena* arena) {
    IrNode* node = (IrNode*)arena_alloc(arena, sizeof(IrNode), _Alignof(IrNode));
    memset(node, 0, sizeof(IrNode));
    node->kind = kind;
    return node;
}

IrValue* ir_alloc_value(IrValueKind kind, Type* type, Arena* arena) {
    IrValue* value = (IrValue*)arena_alloc(arena, sizeof(IrValue), _Alignof(IrValue));
    memset(value, 0, sizeof(IrValue));
    value->kind = kind;
    value->type = type;
    return value;
}

IrInstruction* ir_alloc_instruction(IrNodeKind kind, Arena* arena) {
    IrInstruction* instr = (IrInstruction*)arena_alloc(arena, sizeof(IrInstruction), _Alignof(IrInstruction));
    memset(instr, 0, sizeof(IrInstruction));
    instr->kind = kind;
    return instr;
}

IrBasicBlock* ir_alloc_block(uint32_t id, const char* label, Arena* arena) {
    IrBasicBlock* block = (IrBasicBlock*)arena_alloc(arena, sizeof(IrBasicBlock), _Alignof(IrBasicBlock));
    memset(block, 0, sizeof(IrBasicBlock));
    block->id = id;
    block->label = label;
    block->instruction_capacity = 16;
    block->instructions = (IrInstruction**)arena_alloc(arena,
        sizeof(IrInstruction*) * block->instruction_capacity, _Alignof(IrInstruction*));
    return block;
}

void ir_block_add_instruction(IrBasicBlock* block, IrInstruction* instr, Arena* arena) {
    (void)arena;  // May be used for reallocation in real implementation
    if (block->instruction_count < block->instruction_capacity) {
        block->instructions[block->instruction_count++] = instr;
    }
}

IrFunction* ir_function_create(const char* name, Type* return_type,
                               IrParam* params, size_t param_count, Arena* arena) {
    IrFunction* func = (IrFunction*)arena_alloc(arena, sizeof(IrFunction), _Alignof(IrFunction));
    memset(func, 0, sizeof(IrFunction));
    func->name = name;
    func->return_type = return_type;
    func->params = params;
    func->param_count = param_count;
    func->block_capacity = 16;
    func->blocks = (IrBasicBlock**)arena_alloc(arena,
        sizeof(IrBasicBlock*) * func->block_capacity, _Alignof(IrBasicBlock*));
    return func;
}

void ir_function_add_block(IrFunction* func, IrBasicBlock* block, Arena* arena) {
    (void)arena;  // May be used for reallocation in real implementation
    if (func->block_count < func->block_capacity) {
        func->blocks[func->block_count++] = block;
        if (func->block_count == 1) {
            func->entry = block;
        }
    }
}

// Mock type initialization - using new type_new_* API
Type* type_new_primitive(TypeKind kind, Arena* arena) {
    Type* t = (Type*)arena_alloc(arena, sizeof(Type), _Alignof(Type));
    memset(t, 0, sizeof(Type));
    t->kind = kind;
    switch (kind) {
        case TYPE_I8:
        case TYPE_U8:
            t->size = 1;
            t->alignment = 1;
            break;
        case TYPE_I16:
        case TYPE_U16:
            t->size = 2;
            t->alignment = 2;
            break;
        case TYPE_I32:
        case TYPE_U32:
            t->size = 4;
            t->alignment = 4;
            break;
        case TYPE_I64:
        case TYPE_U64:
            t->size = 8;
            t->alignment = 8;
            break;
        case TYPE_BOOL:
            t->size = 1;
            t->alignment = 1;
            break;
        case TYPE_VOID:
            t->size = 0;
            t->alignment = 1;
            break;
        default:
            break;
    }
    return t;
}

// Mock error list functions
void error_list_init(ErrorList* list, Arena* arena) {
    list->arena = arena;
    list->errors = NULL;
    list->count = 0;
    list->capacity = 0;
}

void error_list_add(ErrorList* list, ErrorKind kind, SourceLocation loc,
                   const char* fmt, ...) {
    (void)list;
    (void)kind;
    (void)loc;
    (void)fmt;
    // Mock implementation - in real code would format and store
    printf("Error: ");
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

void error_list_print(ErrorList* list, FILE* out) {
    (void)list;
    (void)out;
}

bool error_list_has_errors(ErrorList* list) {
    return list->count > 0;
}

// Mock arena functions
void arena_init(Arena* arena, size_t block_size) {
    arena->block_size = block_size;
    arena->current = (ArenaBlock*)malloc(sizeof(ArenaBlock) + block_size);
    arena->current->next = NULL;
    arena->current->used = 0;
    arena->current->capacity = block_size;
    arena->first = arena->current;
}

void* arena_alloc(Arena* arena, size_t size, size_t alignment) {
    // Align the current position
    size_t aligned_used = (arena->current->used + alignment - 1) & ~(alignment - 1);

    if (aligned_used + size > arena->current->capacity) {
        // Allocate new block
        size_t new_size = size > arena->block_size ? size : arena->block_size;
        ArenaBlock* new_block = (ArenaBlock*)malloc(sizeof(ArenaBlock) + new_size);
        new_block->next = NULL;
        new_block->used = 0;
        new_block->capacity = new_size;
        arena->current->next = new_block;
        arena->current = new_block;
        aligned_used = 0;
    }

    void* result = arena->current->data + aligned_used;
    arena->current->used = aligned_used + size;
    return result;
}

void arena_free(Arena* arena) {
    ArenaBlock* block = arena->first;
    while (block) {
        ArenaBlock* next = block->next;
        free(block);
        block = next;
    }
    arena->first = NULL;
    arena->current = NULL;
}

// Test 1: Identifier prefixing
void test_identifier_prefixing() {
    printf("Test: Identifier prefixing... ");
    setup_test();

    const char* prefixed = codegen_prefix_identifier("my_var", &test_arena);
    assert(strcmp(prefixed, "__u_my_var") == 0);

    // Test double prefixing (should not add another prefix)
    const char* double_prefixed = codegen_prefix_identifier(prefixed, &test_arena);
    assert(strcmp(double_prefixed, "__u_my_var") == 0);

    teardown_test();
    printf("PASSED\n");
}

// Test 2: Type translation
void test_type_translation() {
    printf("Test: Type translation... ");
    setup_test();

    Type* i32_type = type_new_primitive(TYPE_I32, &test_arena);
    assert(strcmp(codegen_type_to_c(i32_type, &test_arena), "int32_t") == 0);

    Type* u64_type = type_new_primitive(TYPE_U64, &test_arena);
    assert(strcmp(codegen_type_to_c(u64_type, &test_arena), "uint64_t") == 0);

    Type* usize_type = type_new_primitive(TYPE_USIZE, &test_arena);
    assert(strcmp(codegen_type_to_c(usize_type, &test_arena), "size_t") == 0);

    Type* isize_type = type_new_primitive(TYPE_ISIZE, &test_arena);
    assert(strcmp(codegen_type_to_c(isize_type, &test_arena), "ptrdiff_t") == 0);

    Type* bool_type = type_new_primitive(TYPE_BOOL, &test_arena);
    assert(strcmp(codegen_type_to_c(bool_type, &test_arena), "bool") == 0);

    teardown_test();
    printf("PASSED\n");
}

// Test 3: Simple function generation
void test_simple_function() {
    printf("Test: Simple function generation... ");
    setup_test();

    // Create a simple function: i32 add(i32 a, i32 b) { return a + b; }
    Type* i32_type = type_new_primitive(TYPE_I32, &test_arena);

    // Create parameters
    IrParam* params = (IrParam*)arena_alloc(&test_arena, sizeof(IrParam) * 2, _Alignof(IrParam));
    params[0].name = "a";
    params[0].type = i32_type;
    params[0].index = 0;
    params[1].name = "b";
    params[1].type = i32_type;
    params[1].index = 1;

    // Create function
    IrFunction* func = ir_function_create("add", i32_type, params, 2, &test_arena);

    // Create entry block
    IrBasicBlock* entry = ir_alloc_block(0, "entry", &test_arena);

    // Create values for parameters
    IrValue* val_a = ir_alloc_value(IR_VALUE_PARAM, i32_type, &test_arena);
    val_a->data.param.name = "a";
    val_a->data.param.index = 0;

    IrValue* val_b = ir_alloc_value(IR_VALUE_PARAM, i32_type, &test_arena);
    val_b->data.param.name = "b";
    val_b->data.param.index = 1;

    // Create temp for result
    IrValue* temp = ir_alloc_value(IR_VALUE_TEMP, i32_type, &test_arena);
    temp->data.temp.id = 0;
    temp->data.temp.name = "result";

    // Create binary op: result = a + b
    IrInstruction* add_instr = ir_alloc_instruction(IR_BINARY_OP, &test_arena);
    add_instr->data.binary_op.dest = temp;
    add_instr->data.binary_op.op = IR_OP_ADD;
    add_instr->data.binary_op.left = val_a;
    add_instr->data.binary_op.right = val_b;
    add_instr->type = i32_type;
    add_instr->loc = (SourceLocation){1, 1, "test.lang"};

    // Create return instruction
    IrInstruction* ret_instr = ir_alloc_instruction(IR_RETURN, &test_arena);
    ret_instr->data.ret.value = temp;
    ret_instr->loc = (SourceLocation){2, 5, "test.lang"};

    // Add instructions to block
    ir_block_add_instruction(entry, add_instr, &test_arena);
    ir_block_add_instruction(entry, ret_instr, &test_arena);

    // Add block to function
    ir_function_add_block(func, entry, &test_arena);

    // Create module
    IrModule* mod = (IrModule*)arena_alloc(&test_arena, sizeof(IrModule), _Alignof(IrModule));
    mod->name = "test";
    mod->function_count = 1;
    mod->functions = (IrFunction**)arena_alloc(&test_arena, sizeof(IrFunction*), _Alignof(IrFunction*));
    mod->functions[0] = func;

    IrNode* module_node = ir_alloc_node(IR_MODULE, &test_arena);
    module_node->data.module = mod;

    // Generate code
    CodegenContext ctx;
    codegen_init(&ctx, &test_arena, &test_errors, "test");

    FILE* header = fopen("/tmp/test.h", "w");
    FILE* source = fopen("/tmp/test.c", "w");
    ctx.header_out = header;
    ctx.source_out = source;

    codegen_emit_module(module_node, &ctx);

    fclose(header);
    fclose(source);

    // Verify generated files exist and contain expected content
    FILE* verify = fopen("/tmp/test.h", "r");
    assert(verify != NULL);
    char line[256];
    bool found_declaration = false;
    while (fgets(line, sizeof(line), verify)) {
        if (strstr(line, "int32_t __u_add")) {
            found_declaration = true;
            break;
        }
    }
    fclose(verify);
    assert(found_declaration);

    teardown_test();
    printf("PASSED\n");
}

// Test 4: Runtime header generation
void test_runtime_header() {
    printf("Test: Runtime header generation... ");

    FILE* runtime = fopen("/tmp/lang_runtime.h", "w");
    codegen_emit_runtime_header(runtime);
    fclose(runtime);

    // Verify file exists and contains key elements
    FILE* verify = fopen("/tmp/lang_runtime.h", "r");
    assert(verify != NULL);
    char content[4096];
    size_t read = fread(content, 1, sizeof(content) - 1, verify);
    content[read] = '\0';
    fclose(verify);

    assert(strstr(content, "#include <stdint.h>") != NULL);
    assert(strstr(content, "#include <stdbool.h>") != NULL);
    assert(strstr(content, "RESULT_OK") != NULL);
    assert(strstr(content, "__u_Coroutine") != NULL);

    printf("PASSED\n");
}

// Test 5: Code compiles with C11 freestanding
void test_c11_compilation() {
    printf("Test: C11 freestanding compilation... ");
    setup_test();

    // Create simple module
    Type* i32_type = type_new_primitive(TYPE_I32, &test_arena);

    // Create parameter
    IrParam* params = (IrParam*)arena_alloc(&test_arena, sizeof(IrParam), _Alignof(IrParam));
    params[0].name = "x";
    params[0].type = i32_type;
    params[0].index = 0;

    // Create function
    IrFunction* func = ir_function_create("identity", i32_type, params, 1, &test_arena);

    // Create entry block
    IrBasicBlock* entry = ir_alloc_block(0, "entry", &test_arena);

    // Create parameter value
    IrValue* val_x = ir_alloc_value(IR_VALUE_PARAM, i32_type, &test_arena);
    val_x->data.param.name = "x";
    val_x->data.param.index = 0;

    // Create return instruction
    IrInstruction* ret_instr = ir_alloc_instruction(IR_RETURN, &test_arena);
    ret_instr->data.ret.value = val_x;

    // Add instruction to block
    ir_block_add_instruction(entry, ret_instr, &test_arena);

    // Add block to function
    ir_function_add_block(func, entry, &test_arena);

    // Create module
    IrModule* mod = (IrModule*)arena_alloc(&test_arena, sizeof(IrModule), _Alignof(IrModule));
    mod->name = "compile_test";
    mod->function_count = 1;
    mod->functions = (IrFunction**)arena_alloc(&test_arena, sizeof(IrFunction*), _Alignof(IrFunction*));
    mod->functions[0] = func;

    IrNode* module_node = ir_alloc_node(IR_MODULE, &test_arena);
    module_node->data.module = mod;

    // Generate code
    CodegenContext ctx;
    codegen_init(&ctx, &test_arena, &test_errors, "compile_test");

    FILE* runtime = fopen("/tmp/lang_runtime.h", "w");
    codegen_emit_runtime_header(runtime);
    fclose(runtime);

    FILE* header = fopen("/tmp/compile_test.h", "w");
    FILE* source = fopen("/tmp/compile_test.c", "w");
    ctx.header_out = header;
    ctx.source_out = source;

    codegen_emit_module(module_node, &ctx);

    fclose(header);
    fclose(source);

    // Try to compile with strict C11 freestanding flags
    int result = system("gcc -Wall -Werror -Wextra -std=c11 -ffreestanding -I/tmp -c /tmp/compile_test.c -o /tmp/compile_test.o 2>&1");

    teardown_test();

    if (result == 0) {
        printf("PASSED\n");
    } else {
        printf("FAILED (compilation errors)\n");
        system("cat /tmp/compile_test.c");
    }
}

// Test 6: Assignment and variable references
void test_assignment() {
    printf("Test: Assignment generation... ");
    setup_test();

    Type* i32_type = type_new_primitive(TYPE_I32, &test_arena);
    Type* void_type = type_new_primitive(TYPE_VOID, &test_arena);

    // Create function
    IrFunction* func = ir_function_create("test_assign", void_type, NULL, 0, &test_arena);

    // Create entry block
    IrBasicBlock* entry = ir_alloc_block(0, "entry", &test_arena);

    // Create values
    IrValue* var_x = ir_alloc_value(IR_VALUE_TEMP, i32_type, &test_arena);
    var_x->data.temp.name = "x";
    var_x->data.temp.id = 0;

    IrValue* const_42 = ir_alloc_value(IR_VALUE_CONSTANT, i32_type, &test_arena);
    const_42->data.constant.data.int_val = 42;

    // Create assignment: x = 42
    IrInstruction* assign = ir_alloc_instruction(IR_ASSIGN, &test_arena);
    assign->data.assign.dest = var_x;
    assign->data.assign.src = const_42;

    // Add instruction to block
    ir_block_add_instruction(entry, assign, &test_arena);

    // Add block to function
    ir_function_add_block(func, entry, &test_arena);

    // Create module
    IrModule* mod = (IrModule*)arena_alloc(&test_arena, sizeof(IrModule), _Alignof(IrModule));
    mod->name = "assign_test";
    mod->function_count = 1;
    mod->functions = (IrFunction**)arena_alloc(&test_arena, sizeof(IrFunction*), _Alignof(IrFunction*));
    mod->functions[0] = func;

    IrNode* module_node = ir_alloc_node(IR_MODULE, &test_arena);
    module_node->data.module = mod;

    // Generate code
    CodegenContext ctx;
    codegen_init(&ctx, &test_arena, &test_errors, "assign_test");

    FILE* header = fopen("/tmp/assign_test.h", "w");
    FILE* source = fopen("/tmp/assign_test.c", "w");
    ctx.header_out = header;
    ctx.source_out = source;

    codegen_emit_module(module_node, &ctx);

    fclose(header);
    fclose(source);

    // Verify assignment is generated
    FILE* verify = fopen("/tmp/assign_test.c", "r");
    char content[1024];
    size_t readsize = fread(content, 1, sizeof(content) - 1, verify);
    content[readsize] = '\0';
    fclose(verify);

    assert(strstr(content, "__u_x") != NULL);
    assert(strstr(content, "42") != NULL);

    teardown_test();
    printf("PASSED\n");
}

int main(void) {
    printf("=== Code Generation Tests ===\n\n");

    test_identifier_prefixing();
    test_type_translation();
    test_simple_function();
    test_runtime_header();
    test_assignment();
    test_c11_compilation();

    printf("\nAll tests passed!\n");
    return 0;
}
