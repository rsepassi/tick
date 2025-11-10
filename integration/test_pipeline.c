#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "arena.h"
#include "error.h"
#include "string_pool.h"
#include "symbol.h"
#include "ir.h"
#include "codegen.h"

// External declarations
void resolver_resolve(AstNode* module_node, SymbolTable* symbol_table,
                     StringPool* string_pool, ErrorList* errors, Arena* arena);
void typeck_check_module(AstNode* module_node, SymbolTable* symbol_table,
                        ErrorList* errors, Arena* arena);

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { printf("  Test: %s ... ", name); tests_run++; } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 0; } while(0)

// Helper: Run full pipeline on source code
static int run_pipeline(const char* source, IrModule** out_ir) {
    Arena arena;
    arena_init(&arena, 64 * 1024);

    ErrorList errors = {0};
    StringPool string_pool;
    string_pool_init(&string_pool, &arena);

    // Phase 1-2: Lex and parse
    Lexer lexer;
    lexer_init(&lexer, source, strlen(source), "test.tick", &string_pool, &errors);

    Parser parser;
    parser_init(&parser, &lexer, &arena, &errors);

    AstNode* ast = parser_parse(&parser);
    parser_cleanup(&parser);

    if (!ast || errors.count > 0) {
        arena_free(&arena);
        return 0;
    }

    // Phase 3: Semantic analysis
    SymbolTable symbols;
    symbol_table_init(&symbols, &arena);

    resolver_resolve(ast, &symbols, &string_pool, &errors, &arena);
    if (errors.count > 0) {
        arena_free(&arena);
        return 0;
    }

    typeck_check_module(ast, &symbols, &errors, &arena);
    if (errors.count > 0) {
        arena_free(&arena);
        return 0;
    }

    // Phase 4: IR lowering
    IrModule* ir = ir_lower_ast(ast, &arena);
    if (!ir) {
        arena_free(&arena);
        return 0;
    }

    if (out_ir) {
        *out_ir = ir;
    }

    // Note: Not freeing arena here since IR points into it
    return 1;
}

// Test 1: Simple function pipeline
static int test1(void) {
    TEST("Simple function pipeline");

    const char* source = "let add = fn(x: i32, y: i32) -> i32 { return x + y; };";

    IrModule* ir = NULL;
    if (!run_pipeline(source, &ir)) {
        FAIL("Pipeline failed");
    }

    if (!ir || ir->function_count == 0) {
        FAIL("No functions generated");
    }

    PASS();
    return 1;
}

// Test 2: Multiple functions
static int test2(void) {
    TEST("Multiple functions pipeline");

    const char* source =
        "let add = fn(x: i32, y: i32) -> i32 { return x + y; };\n"
        "let sub = fn(x: i32, y: i32) -> i32 { return x - y; };\n"
        "let mul = fn(x: i32, y: i32) -> i32 { return x * y; };";

    IrModule* ir = NULL;
    if (!run_pipeline(source, &ir)) {
        FAIL("Pipeline failed");
    }

    if (!ir || ir->function_count != 3) {
        FAIL("Expected 3 functions");
    }

    PASS();
    return 1;
}

// Test 3: Error detection in pipeline
static int test3(void) {
    TEST("Error handling pipeline");

    // Invalid syntax - should fail at parse
    const char* source = "let broken = fn(x: i32 -> i32 {;";

    IrModule* ir = NULL;
    if (run_pipeline(source, &ir)) {
        FAIL("Pipeline should have failed");
    }

    PASS();
    return 1;
}

// Test 4: Different types
static int test4(void) {
    TEST("Type variety pipeline");

    const char* source =
        "let f1 = fn(x: i8) -> i8 { return x; };\n"
        "let f2 = fn(x: u32) -> u32 { return x; };\n"
        "let f3 = fn(x: i64) -> i64 { return x; };\n"
        "let f4 = fn(x: bool) -> bool { return x; };";

    IrModule* ir = NULL;
    if (!run_pipeline(source, &ir)) {
        FAIL("Pipeline failed");
    }

    if (!ir || ir->function_count != 4) {
        FAIL("Expected 4 functions");
    }

    PASS();
    return 1;
}

// Test 5: Complex expressions
static int test5(void) {
    TEST("Complex expressions pipeline");

    const char* source =
        "let compute = fn(a: i32, b: i32, c: i32) -> i32 {\n"
        "    let temp1 = a + b;\n"
        "    let temp2 = temp1 * c;\n"
        "    let temp3 = temp2 - a;\n"
        "    return temp3;\n"
        "};";

    IrModule* ir = NULL;
    if (!run_pipeline(source, &ir)) {
        FAIL("Pipeline failed");
    }

    if (!ir || ir->function_count == 0) {
        FAIL("No functions generated");
    }

    // Check that function has basic blocks with instructions
    IrFunction* func = ir->functions[0];
    if (func->block_count == 0) {
        FAIL("No basic blocks generated");
    }

    PASS();
    return 1;
}

int main(void) {
    printf("\n========================================\n");
    printf("End-to-End Pipeline Integration Tests\n");
    printf("========================================\n\n");
    
    test1(); test2(); test3(); test4(); test5();
    
    printf("\n========================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
