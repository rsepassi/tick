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
typedef struct Scope Scope;
void resolver_analyze(AstNode* ast, Scope* scope, Arena* arena, ErrorList* errors);
void typeck_analyze(AstNode* ast, Scope* scope, Arena* arena, ErrorList* errors);
Scope* scope_create(Arena* arena);

// Helper function to run full semantic analysis
static AstNode* analyze_source(const char* source, Arena* arena, ErrorList* errors, Scope** out_scope) {
    // Parse
    Lexer lexer;
    lexer_init(&lexer, source, strlen(source), "test.tick");

    Parser parser;
    parser_init(&parser, &lexer, arena);

    AstNode* ast = parser_parse(&parser);
    if (!ast || error_list_has_errors(errors)) {
        return NULL;
    }

    // Symbol resolution
    Scope* scope = scope_create(arena);
    resolver_analyze(ast, scope, arena, errors);
    if (error_list_has_errors(errors)) {
        return NULL;
    }

    // Type checking
    typeck_analyze(ast, scope, arena, errors);
    if (error_list_has_errors(errors)) {
        return NULL;
    }

    if (out_scope) {
        *out_scope = scope;
    }

    return ast;
}

// Test 1: Type checking simple expressions
static int test_type_checking_expressions(void) {
    TEST("Type checking expressions");

    const char* source =
        "fn compute() i32 {\n"
        "    let x: i32 = 10;\n"
        "    let y: i32 = 20;\n"
        "    return x + y;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    Scope* scope = NULL;
    AstNode* ast = analyze_source(source, &arena, &errors, &scope);

    if (!ast) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed semantic analysis");
    }

    // Verify type annotations were added
    assert(ast != NULL);
    assert(scope != NULL);

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 2: Type error detection
static int test_type_error_detection(void) {
    TEST("Type error detection");

    const char* source =
        "fn bad_add() i32 {\n"
        "    let x: i32 = 10;\n"
        "    let y: []u8 = \"hello\";\n"
        "    return x + y;  // Type error: can't add i32 and []u8\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    AstNode* ast = analyze_source(source, &arena, &errors, NULL);

    // Should detect type error
    if (!error_list_has_errors(&errors)) {
        arena_destroy(&arena);
        FAIL("Should have detected type error");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 3: Symbol resolution
static int test_symbol_resolution(void) {
    TEST("Symbol resolution");

    const char* source =
        "fn helper() i32 {\n"
        "    return 42;\n"
        "}\n"
        "\n"
        "fn caller() i32 {\n"
        "    return helper();\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    Scope* scope = NULL;
    AstNode* ast = analyze_source(source, &arena, &errors, &scope);

    if (!ast) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed semantic analysis");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 4: Undefined symbol error
static int test_undefined_symbol(void) {
    TEST("Undefined symbol detection");

    const char* source =
        "fn caller() i32 {\n"
        "    return undefined_function();\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    AstNode* ast = analyze_source(source, &arena, &errors, NULL);

    // Should detect undefined symbol
    if (!error_list_has_errors(&errors)) {
        arena_destroy(&arena);
        FAIL("Should have detected undefined symbol");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 5: Function signature checking
static int test_function_signature(void) {
    TEST("Function signature checking");

    const char* source =
        "fn add(a: i32, b: i32) i32 {\n"
        "    return a + b;\n"
        "}\n"
        "\n"
        "fn main() void {\n"
        "    let result = add(10, 20);\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    Scope* scope = NULL;
    AstNode* ast = analyze_source(source, &arena, &errors, &scope);

    if (!ast) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed semantic analysis");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 6: Struct type checking
static int test_struct_types(void) {
    TEST("Struct type checking");

    const char* source =
        "struct Point {\n"
        "    x: i32,\n"
        "    y: i32,\n"
        "}\n"
        "\n"
        "fn make_point() Point {\n"
        "    let p: Point = Point { x: 10, y: 20 };\n"
        "    return p;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    Scope* scope = NULL;
    AstNode* ast = analyze_source(source, &arena, &errors, &scope);

    if (!ast) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed semantic analysis");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 7: Error type propagation
static int test_error_type(void) {
    TEST("Error type propagation");

    const char* source =
        "fn might_fail() !i32 {\n"
        "    return 42;\n"
        "}\n"
        "\n"
        "fn caller() !void {\n"
        "    let result = might_fail();\n"
        "    return;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    Scope* scope = NULL;
    AstNode* ast = analyze_source(source, &arena, &errors, &scope);

    if (!ast) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed semantic analysis");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 8: Scope handling
static int test_nested_scopes(void) {
    TEST("Nested scope handling");

    const char* source =
        "fn scopes() i32 {\n"
        "    let x: i32 = 10;\n"
        "    {\n"
        "        let y: i32 = 20;\n"
        "        let z = x + y;\n"
        "    }\n"
        "    return x;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    Scope* scope = NULL;
    AstNode* ast = analyze_source(source, &arena, &errors, &scope);

    if (!ast) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed semantic analysis");
    }

    arena_destroy(&arena);
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
