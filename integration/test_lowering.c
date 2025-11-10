#include <stdio.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "arena.h"
#include "error.h"
#include "string_pool.h"
#include "symbol.h"
#include "ir.h"

void resolver_resolve(AstNode* module_node, SymbolTable* symbol_table,
                     StringPool* string_pool, ErrorList* errors, Arena* arena);
void typeck_check_module(AstNode* module_node, SymbolTable* symbol_table,
                        ErrorList* errors, Arena* arena);

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { printf("  Test: %s ... ", name); tests_run++; } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 0; } while(0)

// Helper: Parse and lower source code to IR
static IrModule* parse_and_lower(const char* source, Arena* arena, ErrorList* errors) {
    StringPool string_pool;
    string_pool_init(&string_pool, arena);

    Lexer lexer;
    lexer_init(&lexer, source, strlen(source), "test.tick", &string_pool, errors);

    Parser parser;
    parser_init(&parser, &lexer, arena, errors);

    AstNode* ast = parser_parse(&parser);
    parser_cleanup(&parser);

    if (!ast || errors->count > 0) {
        return NULL;
    }

    SymbolTable symbols;
    symbol_table_init(&symbols, arena);

    resolver_resolve(ast, &symbols, &string_pool, errors, arena);
    if (errors->count > 0) {
        return NULL;
    }

    typeck_check_module(ast, &symbols, errors, arena);
    if (errors->count > 0) {
        return NULL;
    }

    return ir_lower_ast(ast, arena);
}

// Test 1: Simple function lowering
static int test1(void) {
    TEST("Simple function lowering");

    Arena arena;
    arena_init(&arena, 64 * 1024);
    ErrorList errors = {0};

    const char* source = "let add = fn(x: i32, y: i32) -> i32 { return x + y; };";

    IrModule* ir = parse_and_lower(source, &arena, &errors);
    if (!ir) {
        FAIL("Failed to lower AST to IR");
    }

    if (ir->function_count != 1) {
        FAIL("Expected 1 function");
    }

    IrFunction* func = ir->functions[0];
    if (func->param_count != 2) {
        FAIL("Expected 2 parameters");
    }

    if (func->block_count == 0) {
        FAIL("Expected at least one basic block");
    }

    arena_free(&arena);
    PASS();
    return 1;
}

// Test 2: Multiple operations
static int test2(void) {
    TEST("Multiple operations lowering");

    Arena arena;
    arena_init(&arena, 64 * 1024);
    ErrorList errors = {0};

    const char* source =
        "let compute = fn(a: i32, b: i32) -> i32 {\n"
        "    let t1 = a + b;\n"
        "    let t2 = t1 * 2;\n"
        "    return t2;\n"
        "};";

    IrModule* ir = parse_and_lower(source, &arena, &errors);
    if (!ir) {
        FAIL("Failed to lower AST to IR");
    }

    IrFunction* func = ir->functions[0];
    if (func->block_count == 0) {
        FAIL("Expected at least one basic block");
    }

    // Check that we have instructions
    IrBasicBlock* block = func->blocks[0];
    if (block->instruction_count == 0) {
        FAIL("Expected instructions in basic block");
    }

    arena_free(&arena);
    PASS();
    return 1;
}

// Test 3: Control flow lowering
static int test3(void) {
    TEST("Control flow lowering");

    Arena arena;
    arena_init(&arena, 64 * 1024);
    ErrorList errors = {0};

    const char* source =
        "let max = fn(a: i32, b: i32) -> i32 {\n"
        "    if (a > b) {\n"
        "        return a;\n"
        "    } else {\n"
        "        return b;\n"
        "    }\n"
        "};";

    IrModule* ir = parse_and_lower(source, &arena, &errors);
    if (!ir) {
        FAIL("Failed to lower AST to IR");
    }

    IrFunction* func = ir->functions[0];
    // If statement should create multiple basic blocks
    if (func->block_count < 2) {
        FAIL("Expected multiple basic blocks for if statement");
    }

    arena_free(&arena);
    PASS();
    return 1;
}

// Test 4: Multiple functions lowering
static int test4(void) {
    TEST("Multiple functions lowering");

    Arena arena;
    arena_init(&arena, 64 * 1024);
    ErrorList errors = {0};

    const char* source =
        "let add = fn(x: i32, y: i32) -> i32 { return x + y; };\n"
        "let sub = fn(x: i32, y: i32) -> i32 { return x - y; };\n"
        "let mul = fn(x: i32, y: i32) -> i32 { return x * y; };";

    IrModule* ir = parse_and_lower(source, &arena, &errors);
    if (!ir) {
        FAIL("Failed to lower AST to IR");
    }

    if (ir->function_count != 3) {
        FAIL("Expected 3 functions");
    }

    // Check each function has basic structure
    for (size_t i = 0; i < ir->function_count; i++) {
        IrFunction* func = ir->functions[i];
        if (func->block_count == 0) {
            FAIL("Function missing basic blocks");
        }
        if (func->param_count != 2) {
            FAIL("Function should have 2 parameters");
        }
    }

    arena_free(&arena);
    PASS();
    return 1;
}

// Test 5: Type preservation in lowering
static int test5(void) {
    TEST("Type preservation lowering");

    Arena arena;
    arena_init(&arena, 64 * 1024);
    ErrorList errors = {0};

    const char* source =
        "let f1 = fn(x: i8) -> i8 { return x; };\n"
        "let f2 = fn(x: u32) -> u32 { return x; };\n"
        "let f3 = fn(x: i64) -> i64 { return x; };\n"
        "let f4 = fn(x: bool) -> bool { return x; };";

    IrModule* ir = parse_and_lower(source, &arena, &errors);
    if (!ir) {
        FAIL("Failed to lower AST to IR");
    }

    if (ir->function_count != 4) {
        FAIL("Expected 4 functions");
    }

    // Verify each function has proper return type
    for (size_t i = 0; i < ir->function_count; i++) {
        IrFunction* func = ir->functions[i];
        if (!func->return_type) {
            FAIL("Function missing return type");
        }
    }

    arena_free(&arena);
    PASS();
    return 1;
}

int main(void) {
    printf("\n========================================\n");
    printf("Coroutine + IR Lowering Integration Tests\n");
    printf("========================================\n\n");
    
    test1(); test2(); test3(); test4(); test5();
    
    printf("\n========================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
