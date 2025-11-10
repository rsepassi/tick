/*
 * Integration Test: Lexer + Parser
 *
 * Tests the boundary between lexer and parser:
 * - Lexer produces tokens
 * - Parser consumes tokens and builds AST
 * - Verifies data flows correctly between components
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

// Helper function to run lexer + parser
static AstNode* parse_source(const char* source, Arena* arena, ErrorList* errors) {
    Lexer lexer;
    lexer_init(&lexer, source, strlen(source), "test.tick");

    Parser parser;
    parser_init(&parser, &lexer, arena);

    AstNode* ast = parser_parse(&parser);

    return ast;
}

// Test 1: Simple function parsing
static int test_simple_function(void) {
    TEST("Simple function parsing");

    const char* source =
        "fn add(a: i32, b: i32) i32 {\n"
        "    return a + b;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    AstNode* ast = parse_source(source, &arena, &errors);

    if (!ast) {
        arena_destroy(&arena);
        FAIL("Failed to parse");
    }

    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
        arena_destroy(&arena);
        FAIL("Parse errors occurred");
    }

    // Verify AST structure
    assert(ast->kind == AST_MODULE);

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 2: Async function with suspend
static int test_async_function(void) {
    TEST("Async function parsing");

    const char* source =
        "pub async fn fetch_data(url: []u8) ![]u8 {\n"
        "    let handle = suspend http_request(url);\n"
        "    return resume handle;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    AstNode* ast = parse_source(source, &arena, &errors);

    if (!ast) {
        arena_destroy(&arena);
        FAIL("Failed to parse");
    }

    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
        arena_destroy(&arena);
        FAIL("Parse errors occurred");
    }

    assert(ast->kind == AST_MODULE);

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 3: Try/catch error handling
static int test_try_catch(void) {
    TEST("Try/catch parsing");

    const char* source =
        "fn process() !void {\n"
        "    try {\n"
        "        let result = risky_operation();\n"
        "    } catch (err) {\n"
        "        return err;\n"
        "    }\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    AstNode* ast = parse_source(source, &arena, &errors);

    if (!ast) {
        arena_destroy(&arena);
        FAIL("Failed to parse");
    }

    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
        arena_destroy(&arena);
        FAIL("Parse errors occurred");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 4: Defer/errdefer
static int test_defer(void) {
    TEST("Defer/errdefer parsing");

    const char* source =
        "fn allocate_resource() !void {\n"
        "    let resource = acquire();\n"
        "    defer release(resource);\n"
        "    errdefer cleanup(resource);\n"
        "    \n"
        "    let result = use_resource(resource);\n"
        "    return result;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    AstNode* ast = parse_source(source, &arena, &errors);

    if (!ast) {
        arena_destroy(&arena);
        FAIL("Failed to parse");
    }

    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
        arena_destroy(&arena);
        FAIL("Parse errors occurred");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 5: Struct definition
static int test_struct_definition(void) {
    TEST("Struct definition parsing");

    const char* source =
        "struct Point {\n"
        "    x: i32,\n"
        "    y: i32,\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    AstNode* ast = parse_source(source, &arena, &errors);

    if (!ast) {
        arena_destroy(&arena);
        FAIL("Failed to parse");
    }

    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
        arena_destroy(&arena);
        FAIL("Parse errors occurred");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 6: Syntax error handling
static int test_syntax_error(void) {
    TEST("Syntax error handling");

    const char* source =
        "fn broken(\n"  // Missing closing paren and body
        "}\n";

    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    AstNode* ast = parse_source(source, &arena, &errors);

    // Should fail to parse
    if (ast != NULL && !error_list_has_errors(&errors)) {
        arena_destroy(&arena);
        FAIL("Should have detected syntax error");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 7: Multiple functions
static int test_multiple_functions(void) {
    TEST("Multiple functions parsing");

    const char* source =
        "fn foo() void {}\n"
        "\n"
        "fn bar() i32 {\n"
        "    return 42;\n"
        "}\n"
        "\n"
        "pub fn main() void {\n"
        "    foo();\n"
        "    let x = bar();\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    AstNode* ast = parse_source(source, &arena, &errors);

    if (!ast) {
        arena_destroy(&arena);
        FAIL("Failed to parse");
    }

    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
        arena_destroy(&arena);
        FAIL("Parse errors occurred");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

int main(void) {
    printf("\n========================================\n");
    printf("Lexer + Parser Integration Tests\n");
    printf("========================================\n\n");

    // Run tests
    test_simple_function();
    test_async_function();
    test_try_catch();
    test_defer();
    test_struct_definition();
    test_syntax_error();
    test_multiple_functions();

    // Print summary
    printf("\n========================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
