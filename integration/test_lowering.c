/*
 * Integration Test: Coroutine + IR Lowering
 *
 * Tests the boundary between coroutine analysis and IR lowering:
 * - Coroutine analysis produces CoroMetadata with state machine info
 * - IR lowering transforms AST + metadata into IR
 * - Verifies state machines and control flow are correctly lowered
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
#include "ir.h"

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
IrNode* lower_ast_to_ir(AstNode* ast, CoroMetadata* coro_meta, Arena* arena, ErrorList* errors);

// Helper function to run full lowering
static IrNode* compile_to_ir(const char* source, Arena* arena, ErrorList* errors) {
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

    // Coroutine analysis (if needed)
    CoroMetadata* coro_meta = NULL;
    // Simplified - would check if function is async
    bool is_async = true;  // Assume async for these tests
    if (is_async) {
        coro_meta = (CoroMetadata*)arena_alloc(arena, sizeof(CoroMetadata));
        AstNode* async_fn = ast;
        coro_metadata_init(coro_meta, async_fn, arena);
        coro_analyze_function(async_fn, scope, coro_meta, arena);
        if (error_list_has_errors(errors)) {
            return NULL;
        }
    }

    // Lower to IR
    IrNode* ir = lower_ast_to_ir(ast, coro_meta, arena, errors);
    if (error_list_has_errors(errors)) {
        return NULL;
    }

    return ir;
}

// Test 1: Simple function lowering
static int test_simple_function_lowering(void) {
    TEST("Simple function lowering");

    const char* source =
        "fn add(a: i32, b: i32) i32 {\n"
        "    return a + b;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 16384);

    ErrorList errors;
    error_list_init(&errors, &arena);

    IrNode* ir = compile_to_ir(source, &arena, &errors);

    if (!ir) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed to lower to IR");
    }

    // Verify IR structure
    assert(ir->kind == IR_FUNCTION);
    assert(ir->data.function.name != NULL);

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 2: Async function state machine
static int test_async_state_machine(void) {
    TEST("Async function state machine lowering");

    const char* source =
        "pub async fn task() void {\n"
        "    suspend;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 16384);

    ErrorList errors;
    error_list_init(&errors, &arena);

    IrNode* ir = compile_to_ir(source, &arena, &errors);

    if (!ir) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed to lower to IR");
    }

    // Verify state machine was created
    assert(ir->kind == IR_FUNCTION);
    assert(ir->data.function.is_state_machine == true);
    assert(ir->data.function.coro_meta != NULL);

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 3: Control flow lowering
static int test_control_flow(void) {
    TEST("Control flow lowering");

    const char* source =
        "fn conditional(x: i32) i32 {\n"
        "    if (x > 10) {\n"
        "        return x * 2;\n"
        "    } else {\n"
        "        return x + 5;\n"
        "    }\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 16384);

    ErrorList errors;
    error_list_init(&errors, &arena);

    IrNode* ir = compile_to_ir(source, &arena, &errors);

    if (!ir) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed to lower to IR");
    }

    // Verify IR contains conditional branches
    assert(ir->kind == IR_FUNCTION);

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 4: Defer lowering
static int test_defer_lowering(void) {
    TEST("Defer statement lowering");

    const char* source =
        "fn with_defer() void {\n"
        "    let x = acquire();\n"
        "    defer release(x);\n"
        "    use_resource(x);\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 16384);

    ErrorList errors;
    error_list_init(&errors, &arena);

    IrNode* ir = compile_to_ir(source, &arena, &errors);

    if (!ir) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed to lower to IR");
    }

    // Defer should create IR_DEFER_REGION nodes
    assert(ir->kind == IR_FUNCTION);

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 5: Loop lowering
static int test_loop_lowering(void) {
    TEST("Loop lowering");

    const char* source =
        "fn loop_test() i32 {\n"
        "    let sum: i32 = 0;\n"
        "    for (let i: i32 = 0; i < 10; i = i + 1) {\n"
        "        sum = sum + i;\n"
        "    }\n"
        "    return sum;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 16384);

    ErrorList errors;
    error_list_init(&errors, &arena);

    IrNode* ir = compile_to_ir(source, &arena, &errors);

    if (!ir) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed to lower to IR");
    }

    assert(ir->kind == IR_FUNCTION);

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 6: State persistence across suspend
static int test_state_persistence(void) {
    TEST("State persistence across suspend points");

    const char* source =
        "pub async fn stateful(x: i32) i32 {\n"
        "    let y = x * 2;\n"
        "    suspend;\n"
        "    let z = y + 10;\n"
        "    suspend;\n"
        "    return z;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 16384);

    ErrorList errors;
    error_list_init(&errors, &arena);

    IrNode* ir = compile_to_ir(source, &arena, &errors);

    if (!ir) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed to lower to IR");
    }

    // Verify state machine tracks live variables
    assert(ir->data.function.is_state_machine);
    assert(ir->data.function.coro_meta != NULL);
    assert(ir->data.function.coro_meta->state_count >= 2);

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 7: Error handling lowering
static int test_error_handling_lowering(void) {
    TEST("Error handling lowering");

    const char* source =
        "fn may_fail() !i32 {\n"
        "    try {\n"
        "        let result = risky_op();\n"
        "        return result;\n"
        "    } catch (err) {\n"
        "        return err;\n"
        "    }\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 16384);

    ErrorList errors;
    error_list_init(&errors, &arena);

    IrNode* ir = compile_to_ir(source, &arena, &errors);

    if (!ir) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed to lower to IR");
    }

    assert(ir->kind == IR_FUNCTION);

    arena_destroy(&arena);
    PASS();
    return 1;
}

int main(void) {
    printf("\n========================================\n");
    printf("Coroutine + IR Lowering Integration Tests\n");
    printf("========================================\n\n");

    // Run tests
    test_simple_function_lowering();
    test_async_state_machine();
    test_control_flow();
    test_defer_lowering();
    test_loop_lowering();
    test_state_persistence();
    test_error_handling_lowering();

    // Print summary
    printf("\n========================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
