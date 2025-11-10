/*
 * Integration Test: Semantic + Coroutine Analysis
 *
 * Tests the boundary between semantic analysis and coroutine transformation:
 * - Semantic analysis produces typed AST with symbol tables
 * - Coroutine analysis identifies async functions and suspend points
 * - Produces CoroMetadata with state machine information
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
#include "coro_metadata.h"

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

// Forward declarations
typedef struct Scope Scope;
void resolver_analyze(AstNode* ast, Scope* scope, Arena* arena, ErrorList* errors);
void typeck_analyze(AstNode* ast, Scope* scope, Arena* arena, ErrorList* errors);
Scope* scope_create(Arena* arena);

// Helper function to run full analysis including coroutine
static CoroMetadata* analyze_coroutine(const char* source, Arena* arena,
                                       ErrorList* errors, AstNode** out_ast) {
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

    // Find async function and analyze
    // This is simplified - real implementation would walk AST
    AstNode* async_fn = ast;  // Assume first function is async

    CoroMetadata* meta = (CoroMetadata*)arena_alloc(arena, sizeof(CoroMetadata));
    coro_metadata_init(meta, async_fn, arena);
    coro_analyze_function(async_fn, scope, meta, arena);

    if (error_list_has_errors(errors)) {
        return NULL;
    }

    if (out_ast) {
        *out_ast = ast;
    }

    return meta;
}

// Test 1: Simple async function
static int test_simple_async(void) {
    TEST("Simple async function analysis");

    const char* source =
        "pub async fn simple() void {\n"
        "    suspend;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    AstNode* ast = NULL;
    CoroMetadata* meta = analyze_coroutine(source, &arena, &errors, &ast);

    if (!meta) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed coroutine analysis");
    }

    // Verify metadata
    assert(meta->suspend_count >= 1);
    assert(meta->state_count >= 1);

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 2: Async with live variables
static int test_async_live_vars(void) {
    TEST("Async function with live variables");

    const char* source =
        "pub async fn with_state(x: i32) i32 {\n"
        "    let y = x * 2;\n"
        "    suspend;\n"
        "    return y + 10;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    AstNode* ast = NULL;
    CoroMetadata* meta = analyze_coroutine(source, &arena, &errors, &ast);

    if (!meta) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed coroutine analysis");
    }

    // Verify live variables are tracked
    assert(meta->suspend_count >= 1);
    if (meta->suspend_count > 0) {
        SuspendPoint* sp = &meta->suspend_points[0];
        // Variable 'y' should be live at suspend point
        assert(sp->live_var_count > 0);
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 3: Multiple suspend points
static int test_multiple_suspends(void) {
    TEST("Multiple suspend points");

    const char* source =
        "pub async fn multi_suspend() void {\n"
        "    suspend;  // State 0\n"
        "    let x = 42;\n"
        "    suspend;  // State 1\n"
        "    let y = x + 10;\n"
        "    suspend;  // State 2\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    AstNode* ast = NULL;
    CoroMetadata* meta = analyze_coroutine(source, &arena, &errors, &ast);

    if (!meta) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed coroutine analysis");
    }

    // Should have 3 suspend points
    assert(meta->suspend_count == 3);
    assert(meta->state_count >= 3);

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 4: Async with defer
static int test_async_defer(void) {
    TEST("Async function with defer");

    const char* source =
        "pub async fn with_defer() !void {\n"
        "    let resource = acquire();\n"
        "    defer release(resource);\n"
        "    errdefer cleanup(resource);\n"
        "    \n"
        "    suspend;\n"
        "    let result = use_resource(resource);\n"
        "    return result;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    AstNode* ast = NULL;
    CoroMetadata* meta = analyze_coroutine(source, &arena, &errors, &ast);

    if (!meta) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed coroutine analysis");
    }

    // Defer blocks must be tracked for state machine
    assert(meta->suspend_count >= 1);

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 5: State struct sizing
static int test_state_struct_sizing(void) {
    TEST("State struct sizing calculation");

    const char* source =
        "pub async fn with_data() i32 {\n"
        "    let a: i32 = 10;\n"
        "    let b: i64 = 20;\n"
        "    suspend;\n"
        "    return a + b;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    AstNode* ast = NULL;
    CoroMetadata* meta = analyze_coroutine(source, &arena, &errors, &ast);

    if (!meta) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed coroutine analysis");
    }

    // Verify frame size was calculated
    assert(meta->total_frame_size > 0);
    assert(meta->frame_alignment > 0);

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 6: Nested async calls
static int test_nested_async(void) {
    TEST("Nested async function calls");

    const char* source =
        "async fn helper() i32 {\n"
        "    suspend;\n"
        "    return 42;\n"
        "}\n"
        "\n"
        "pub async fn caller() i32 {\n"
        "    let result = suspend helper();\n"
        "    return result;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    AstNode* ast = NULL;
    CoroMetadata* meta = analyze_coroutine(source, &arena, &errors, &ast);

    if (!meta) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed coroutine analysis");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 7: Error in async context
static int test_async_error_handling(void) {
    TEST("Async function error handling");

    const char* source =
        "pub async fn may_fail() !i32 {\n"
        "    try {\n"
        "        suspend;\n"
        "        let result = risky_operation();\n"
        "        return result;\n"
        "    } catch (err) {\n"
        "        return err;\n"
        "    }\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList errors;
    error_list_init(&errors, &arena);

    AstNode* ast = NULL;
    CoroMetadata* meta = analyze_coroutine(source, &arena, &errors, &ast);

    if (!meta) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed coroutine analysis");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

int main(void) {
    printf("\n========================================\n");
    printf("Semantic + Coroutine Analysis Integration Tests\n");
    printf("========================================\n\n");

    // Run tests
    test_simple_async();
    test_async_live_vars();
    test_multiple_suspends();
    test_async_defer();
    test_state_struct_sizing();
    test_nested_async();
    test_async_error_handling();

    // Print summary
    printf("\n========================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
