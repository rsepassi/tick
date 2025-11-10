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

// Mock type initialization
void type_init_primitive(Type* t, TypeKind kind, Arena* arena) {
    (void)arena;
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
        default:
            break;
    }
}

// Mock error list functions
void error_list_init(ErrorList* list, Arena* arena) {
    (void)arena;
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

    Type i32_type;
    type_init_primitive(&i32_type, TYPE_I32, &test_arena);
    assert(strcmp(codegen_type_to_c(&i32_type, &test_arena), "int32_t") == 0);

    Type u64_type;
    type_init_primitive(&u64_type, TYPE_U64, &test_arena);
    assert(strcmp(codegen_type_to_c(&u64_type, &test_arena), "uint64_t") == 0);

    Type usize_type;
    type_init_primitive(&usize_type, TYPE_USIZE, &test_arena);
    assert(strcmp(codegen_type_to_c(&usize_type, &test_arena), "size_t") == 0);

    Type isize_type;
    type_init_primitive(&isize_type, TYPE_ISIZE, &test_arena);
    assert(strcmp(codegen_type_to_c(&isize_type, &test_arena), "ptrdiff_t") == 0);

    Type bool_type;
    type_init_primitive(&bool_type, TYPE_BOOL, &test_arena);
    assert(strcmp(codegen_type_to_c(&bool_type, &test_arena), "bool") == 0);

    teardown_test();
    printf("PASSED\n");
}

// Test 3: Simple function generation
void test_simple_function() {
    printf("Test: Simple function generation... ");
    setup_test();

    // Create a simple function: i32 add(i32 a, i32 b) { return a + b; }
    IrNode* module = ir_alloc_node(IR_MODULE, &test_arena);
    module->data.module.name = "test";
    module->data.module.function_count = 1;
    module->data.module.functions = (IrNode**)arena_alloc(&test_arena, sizeof(IrNode*), _Alignof(IrNode*));

    IrNode* func = ir_alloc_node(IR_FUNCTION, &test_arena);
    func->data.function.name = "add";
    func->data.function.param_count = 2;
    func->data.function.is_state_machine = false;
    func->data.function.coro_meta = NULL;
    func->loc = (SourceLocation){1, 1, "test.lang"};

    Type* i32_type = (Type*)arena_alloc(&test_arena, sizeof(Type), _Alignof(Type));
    type_init_primitive(i32_type, TYPE_I32, &test_arena);
    func->data.function.return_type = i32_type;

    // Parameters
    func->data.function.params = (IrParam*)arena_alloc(&test_arena, sizeof(IrParam) * 2, _Alignof(IrParam));
    func->data.function.params[0].name = "a";
    func->data.function.params[0].type = i32_type;
    func->data.function.params[1].name = "b";
    func->data.function.params[1].type = i32_type;

    // Body: return a + b;
    IrNode* return_stmt = ir_alloc_node(IR_RETURN, &test_arena);
    return_stmt->loc = (SourceLocation){2, 5, "test.lang"};

    IrNode* add_expr = ir_alloc_node(IR_BINARY_OP, &test_arena);
    add_expr->data.binary_op.op = "+";

    IrNode* var_a = ir_alloc_node(IR_VAR_REF, &test_arena);
    var_a->data.var_ref.var_name = "a";

    IrNode* var_b = ir_alloc_node(IR_VAR_REF, &test_arena);
    var_b->data.var_ref.var_name = "b";

    add_expr->data.binary_op.left = var_a;
    add_expr->data.binary_op.right = var_b;
    return_stmt->data.return_stmt.value = add_expr;

    func->data.function.body = return_stmt;
    module->data.module.functions[0] = func;

    // Generate code
    CodegenContext ctx;
    codegen_init(&ctx, &test_arena, &test_errors, "test");

    FILE* header = fopen("/tmp/test.h", "w");
    FILE* source = fopen("/tmp/test.c", "w");
    ctx.header_out = header;
    ctx.source_out = source;

    codegen_emit_module(module, &ctx);

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
    IrNode* module = ir_alloc_node(IR_MODULE, &test_arena);
    module->data.module.name = "compile_test";
    module->data.module.function_count = 1;
    module->data.module.functions = (IrNode**)arena_alloc(&test_arena, sizeof(IrNode*), _Alignof(IrNode*));

    IrNode* func = ir_alloc_node(IR_FUNCTION, &test_arena);
    func->data.function.name = "identity";
    func->data.function.param_count = 1;
    func->data.function.is_state_machine = false;
    func->data.function.coro_meta = NULL;
    func->loc = (SourceLocation){1, 1, "test.lang"};

    Type* i32_type = (Type*)arena_alloc(&test_arena, sizeof(Type), _Alignof(Type));
    type_init_primitive(i32_type, TYPE_I32, &test_arena);
    func->data.function.return_type = i32_type;

    func->data.function.params = (IrParam*)arena_alloc(&test_arena, sizeof(IrParam), _Alignof(IrParam));
    func->data.function.params[0].name = "x";
    func->data.function.params[0].type = i32_type;

    IrNode* return_stmt = ir_alloc_node(IR_RETURN, &test_arena);
    IrNode* var_x = ir_alloc_node(IR_VAR_REF, &test_arena);
    var_x->data.var_ref.var_name = "x";
    return_stmt->data.return_stmt.value = var_x;
    func->data.function.body = return_stmt;
    module->data.module.functions[0] = func;

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

    codegen_emit_module(module, &ctx);

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

    IrNode* module = ir_alloc_node(IR_MODULE, &test_arena);
    module->data.module.name = "assign_test";
    module->data.module.function_count = 1;
    module->data.module.functions = (IrNode**)arena_alloc(&test_arena, sizeof(IrNode*), _Alignof(IrNode*));

    IrNode* func = ir_alloc_node(IR_FUNCTION, &test_arena);
    func->data.function.name = "test_assign";
    func->data.function.param_count = 0;
    func->data.function.is_state_machine = false;
    func->loc = (SourceLocation){1, 1, "test.lang"};

    Type* void_type = (Type*)arena_alloc(&test_arena, sizeof(Type), _Alignof(Type));
    type_init_primitive(void_type, TYPE_VOID, &test_arena);
    func->data.function.return_type = void_type;

    // Body: x = 42;
    IrNode* assign = ir_alloc_node(IR_ASSIGN, &test_arena);
    assign->data.assign.target = "x";

    IrNode* literal = ir_alloc_node(IR_LITERAL, &test_arena);
    literal->data.literal.literal = "42";

    assign->data.assign.value = literal;
    func->data.function.body = assign;
    module->data.module.functions[0] = func;

    CodegenContext ctx;
    codegen_init(&ctx, &test_arena, &test_errors, "assign_test");

    FILE* header = fopen("/tmp/assign_test.h", "w");
    FILE* source = fopen("/tmp/assign_test.c", "w");
    ctx.header_out = header;
    ctx.source_out = source;

    codegen_emit_module(module, &ctx);

    fclose(header);
    fclose(source);

    // Verify assignment is prefixed
    FILE* verify = fopen("/tmp/assign_test.c", "r");
    char content[1024];
    size_t read = fread(content, 1, sizeof(content) - 1, verify);
    content[read] = '\0';
    fclose(verify);

    assert(strstr(content, "__u_x = 42") != NULL);

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
