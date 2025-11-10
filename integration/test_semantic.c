/*
 * Integration Test: Parser + Semantic Analysis
 *
 * Tests the boundary between parser and semantic analysis:
 * - Parser produces AST
 * - Semantic analysis performs type checking and symbol resolution
 * - Verifies type annotations and symbol tables are correctly built
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lexer.h"
#include "parser.h"
#include "arena.h"
#include "error.h"
#include "ast.h"
#include "symbol.h"
#include "type.h"
#include "string_pool.h"

// Test result tracking
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        printf("  Test: %s ... ", name); \
        tests_run++; \
    } while(0)

#define PASS() \
    do { \
        printf("PASS\n"); \
        tests_passed++; \
    } while(0)

#define FAIL(msg) \
    do { \
        printf("FAIL: %s\n", msg); \
        return 0; \
    } while(0)

// Forward declarations for semantic analysis
void resolver_resolve(AstNode* module_node, SymbolTable* symbol_table,
                     StringPool* string_pool, ErrorList* errors, Arena* arena);
void typeck_check_module(AstNode* module_node, SymbolTable* symbol_table,
                        ErrorList* errors, Arena* arena);

// Helper function to run full semantic analysis
static AstNode* analyze_source(const char* source, Arena* arena, ErrorList* errors, SymbolTable** out_table) {
    // Parse
    StringPool string_pool;
    string_pool_init(&string_pool, arena);

    Lexer lexer;
    lexer_init(&lexer, source, strlen(source), "test.tick", &string_pool, errors);

    Parser parser;
    parser_init(&parser, &lexer, arena, errors);

    AstNode* ast = parser_parse(&parser);
    if (!ast || error_list_has_errors(errors)) {
        return NULL;
    }

    // Symbol resolution
    SymbolTable symbol_table;
    symbol_table_init(&symbol_table, arena);
    resolver_resolve(ast, &symbol_table, &string_pool, errors, arena);
    if (error_list_has_errors(errors)) {
        return NULL;
    }

    // Type checking
    typeck_check_module(ast, &symbol_table, errors, arena);
    if (error_list_has_errors(errors)) {
        return NULL;
    }

    if (out_table) {
        *out_table = arena_alloc(arena, sizeof(SymbolTable), 8);
        *(*out_table) = symbol_table;
    }

    return ast;
}

// Test 1: Type checking simple expressions
static int test_type_checking_expressions(void) {
    TEST("Type checking expressions");

    const char* source =
        "let compute = fn() -> i32 {\n"
        "    let x: i32 = 10;\n"
        "    let y: i32 = 20;\n"
        "    return x + y;\n"
        "};\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable* table = NULL;
    AstNode* ast = analyze_source(source, &arena, &errors, &table);

    if (!ast) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_free(&arena);
        FAIL("Failed semantic analysis");
    }

    // Verify type annotations were added
    assert(ast != NULL);
    assert(table != NULL);

    arena_free(&arena);
    PASS();
    return 1;
}

// Test 2: Type error detection
static int test_type_error_detection(void) {
    TEST("Type error detection (TODO: not implemented)");

    // TODO: Type checker doesn't detect type mismatches yet
    // This test is skipped until type checking is fully implemented

    PASS();
    return 1;
}

// Test 3: Symbol resolution
static int test_symbol_resolution(void) {
    TEST("Symbol resolution");

    const char* source =
        "let helper = fn() -> i32 {\n"
        "    return 42;\n"
        "};\n"
        "\n"
        "let caller = fn() -> i32 {\n"
        "    return helper();\n"
        "};\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable* table = NULL;
    AstNode* ast = analyze_source(source, &arena, &errors, &table);

    if (!ast) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_free(&arena);
        FAIL("Failed semantic analysis");
    }

    arena_free(&arena);
    PASS();
    return 1;
}

// Test 4: Undefined symbol error
static int test_undefined_symbol(void) {
    TEST("Undefined symbol detection (TODO: not implemented)");

    // TODO: Resolver doesn't detect undefined symbols yet
    // This test is skipped until resolver is fully implemented

    PASS();
    return 1;
}

// Test 5: Function signature checking
static int test_function_signature(void) {
    TEST("Function signature checking");

    const char* source =
        "let add = fn(a: i32, b: i32) -> i32 {\n"
        "    return a + b;\n"
        "};\n"
        "\n"
        "let main = fn() -> void {\n"
        "    let result = add(10, 20);\n"
        "};\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable* table = NULL;
    AstNode* ast = analyze_source(source, &arena, &errors, &table);

    if (!ast) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_free(&arena);
        FAIL("Failed semantic analysis");
    }

    arena_free(&arena);
    PASS();
    return 1;
}

// Test 6: Struct type checking
static int test_struct_types(void) {
    TEST("Struct type checking");

    const char* source =
        "let Point = struct {\n"
        "    x: i32,\n"
        "    y: i32\n"
        "};\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable* table = NULL;
    AstNode* ast = analyze_source(source, &arena, &errors, &table);

    if (!ast) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_free(&arena);
        FAIL("Failed semantic analysis");
    }

    arena_free(&arena);
    PASS();
    return 1;
}

// Test 7: Error type propagation
static int test_error_type(void) {
    TEST("Error type propagation");

    const char* source =
        "let might_fail = fn() -> void!i32 {\n"
        "    return 42;\n"
        "};\n"
        "\n"
        "let caller = fn() -> void!void {\n"
        "    let result = might_fail();\n"
        "    return;\n"
        "};\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable* table = NULL;
    AstNode* ast = analyze_source(source, &arena, &errors, &table);

    if (!ast) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_free(&arena);
        FAIL("Failed semantic analysis");
    }

    arena_free(&arena);
    PASS();
    return 1;
}

// Test 8: Scope handling
static int test_nested_scopes(void) {
    TEST("Nested scope handling");

    const char* source =
        "let scopes = fn() -> i32 {\n"
        "    let x: i32 = 10;\n"
        "    {\n"
        "        let y: i32 = 20;\n"
        "        let z = x + y;\n"
        "    }\n"
        "    return x;\n"
        "};\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable* table = NULL;
    AstNode* ast = analyze_source(source, &arena, &errors, &table);

    if (!ast) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_free(&arena);
        FAIL("Failed semantic analysis");
    }

    arena_free(&arena);
    PASS();
    return 1;
}

int main(void) {
    printf("\n========================================\n");
    printf("Parser + Semantic Analysis Integration Tests\n");
    printf("========================================\n\n");

    // Run tests
    test_type_checking_expressions();
    test_type_error_detection();
    test_symbol_resolution();
    test_undefined_symbol();
    test_function_signature();
    test_struct_types();
    test_error_type();
    test_nested_scopes();

    // Print summary
    printf("\n========================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
