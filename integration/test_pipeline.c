/*
 * Integration Test: Complete Pipeline End-to-End Tests
 *
 * Comprehensive end-to-end tests of the full Tick compiler pipeline:
 * - Simple functions (no async)
 * - Async functions with suspend points
 * - Error handling (try/catch)
 * - Defer/errdefer
 * - Type checking
 * - Symbol resolution
 * - State machine generation
 * - C11 code generation and compilation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "lexer.h"
#include "parser.h"
#include "arena.h"
#include "error.h"
#include "ast.h"
#include "symbol.h"
#include "type.h"
#include "coro_metadata.h"
#include "ir.h"
#include "codegen.h"

// Test result tracking
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        printf("  Test: %s ...\n", name); \
        tests_run++; \
    } while(0)

#define PASS() \
    do { \
        printf("    ✓ PASS\n"); \
        tests_passed++; \
    } while(0)

#define FAIL(msg) \
    do { \
        printf("    ✗ FAIL: %s\n", msg); \
        return 0; \
    } while(0)

// Forward declarations
typedef struct Scope Scope;
void resolver_analyze(AstNode* ast, Scope* scope, Arena* arena, ErrorList* errors);
void typeck_analyze(AstNode* ast, Scope* scope, Arena* arena, ErrorList* errors);
Scope* scope_create(Arena* arena);
IrNode* lower_ast_to_ir(AstNode* ast, CoroMetadata* coro_meta, Arena* arena, ErrorList* errors);

// Full compilation pipeline
static int compile_and_verify(const char* test_name, const char* source) {
    Arena arena;
    arena_init(&arena, 65536);

    ErrorList errors;
    error_list_init(&errors, &arena);

    // Phase 1: Lexing
    printf("    [1/7] Lexing...\n");
    Lexer lexer;
    lexer_init(&lexer, source, strlen(source), test_name);

    // Phase 2: Parsing
    printf("    [2/7] Parsing...\n");
    Parser parser;
    parser_init(&parser, &lexer, &arena);
    AstNode* ast = parser_parse(&parser);
    if (!ast || error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
        arena_destroy(&arena);
        return 0;
    }

    // Phase 3: Symbol Resolution
    printf("    [3/7] Symbol resolution...\n");
    Scope* scope = scope_create(&arena);
    resolver_analyze(ast, scope, &arena, &errors);
    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
        arena_destroy(&arena);
        return 0;
    }

    // Phase 4: Type Checking
    printf("    [4/7] Type checking...\n");
    typeck_analyze(ast, scope, &arena, &errors);
    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
        arena_destroy(&arena);
        return 0;
    }

    // Phase 5: Coroutine Analysis (if needed)
    printf("    [5/7] Coroutine analysis...\n");
    CoroMetadata* coro_meta = NULL;
    // TODO: Detect async functions and run analysis

    // Phase 6: IR Lowering
    printf("    [6/7] IR lowering...\n");
    IrNode* ir = lower_ast_to_ir(ast, coro_meta, &arena, &errors);
    if (!ir || error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
        arena_destroy(&arena);
        return 0;
    }

    // Phase 7: Code Generation
    printf("    [7/7] Code generation...\n");
    char output_file[256];
    snprintf(output_file, sizeof(output_file), "pipeline_%s.c", test_name);

    FILE* out = fopen(output_file, "w");
    if (!out) {
        arena_destroy(&arena);
        return 0;
    }

    CodegenContext ctx;
    codegen_init(&ctx, &arena, &errors, test_name);
    ctx.source_out = out;
    ctx.emit_line_directives = true;

    codegen_emit_runtime_header(out);
    codegen_emit_module(ir, &ctx);

    fclose(out);

    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
        arena_destroy(&arena);
        return 0;
    }

    // Verify C code compiles
    printf("    [*] Compiling generated C code...\n");
    char compile_cmd[512];
    snprintf(compile_cmd, sizeof(compile_cmd),
             "gcc -Wall -Wextra -Werror -std=c11 -ffreestanding -c %s -o pipeline_%s.o 2>&1",
             output_file, test_name);

    FILE* pipe = popen(compile_cmd, "r");
    if (!pipe) {
        arena_destroy(&arena);
        return 0;
    }

    char buffer[256];
    int has_errors = 0;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        fprintf(stderr, "    GCC: %s", buffer);
        has_errors = 1;
    }

    int status = pclose(pipe);
    arena_destroy(&arena);

    return (status == 0 && !has_errors);
}

// Test 1: Simple function (no async)
static int test_simple_function(void) {
    TEST("Simple function (no async)");

    const char* source =
        "fn add(a: i32, b: i32) i32 {\n"
        "    return a + b;\n"
        "}\n"
        "\n"
        "fn multiply(x: i32, y: i32) i32 {\n"
        "    return x * y;\n"
        "}\n"
        "\n"
        "pub fn compute(n: i32) i32 {\n"
        "    let sum = add(n, 10);\n"
        "    let product = multiply(sum, 2);\n"
        "    return product;\n"
        "}\n";

    if (!compile_and_verify("simple_function", source)) {
        FAIL("Pipeline failed");
    }

    PASS();
    return 1;
}

// Test 2: Async function with suspend points
static int test_async_function(void) {
    TEST("Async function with suspend points");

    const char* source =
        "pub async fn async_task(x: i32) i32 {\n"
        "    let y = x * 2;\n"
        "    suspend;\n"
        "    let z = y + 10;\n"
        "    suspend;\n"
        "    return z;\n"
        "}\n";

    if (!compile_and_verify("async_function", source)) {
        FAIL("Pipeline failed");
    }

    PASS();
    return 1;
}

// Test 3: Error handling (try/catch)
static int test_error_handling(void) {
    TEST("Error handling (try/catch)");

    const char* source =
        "fn risky_operation(x: i32) !i32 {\n"
        "    if (x < 0) {\n"
        "        return error.InvalidInput;\n"
        "    }\n"
        "    return x * 2;\n"
        "}\n"
        "\n"
        "pub fn safe_call(n: i32) !i32 {\n"
        "    try {\n"
        "        let result = risky_operation(n);\n"
        "        return result;\n"
        "    } catch (err) {\n"
        "        return err;\n"
        "    }\n"
        "}\n";

    if (!compile_and_verify("error_handling", source)) {
        FAIL("Pipeline failed");
    }

    PASS();
    return 1;
}

// Test 4: Defer/errdefer
static int test_defer_errdefer(void) {
    TEST("Defer/errdefer resource management");

    const char* source =
        "fn acquire_resource() *void {\n"
        "    return null;\n"
        "}\n"
        "\n"
        "fn release_resource(r: *void) void {\n"
        "}\n"
        "\n"
        "fn cleanup_resource(r: *void) void {\n"
        "}\n"
        "\n"
        "pub fn manage_resource() !void {\n"
        "    let resource = acquire_resource();\n"
        "    defer release_resource(resource);\n"
        "    errdefer cleanup_resource(resource);\n"
        "    \n"
        "    // Use resource\n"
        "    return;\n"
        "}\n";

    if (!compile_and_verify("defer", source)) {
        FAIL("Pipeline failed");
    }

    PASS();
    return 1;
}

// Test 5: Type checking complex expressions
static int test_type_checking(void) {
    TEST("Type checking complex expressions");

    const char* source =
        "struct Point {\n"
        "    x: i32,\n"
        "    y: i32,\n"
        "}\n"
        "\n"
        "fn distance_squared(p1: Point, p2: Point) i32 {\n"
        "    let dx = p1.x - p2.x;\n"
        "    let dy = p1.y - p2.y;\n"
        "    return dx * dx + dy * dy;\n"
        "}\n"
        "\n"
        "pub fn test_points() i32 {\n"
        "    let p1: Point = Point { x: 0, y: 0 };\n"
        "    let p2: Point = Point { x: 3, y: 4 };\n"
        "    return distance_squared(p1, p2);\n"
        "}\n";

    if (!compile_and_verify("type_checking", source)) {
        FAIL("Pipeline failed");
    }

    PASS();
    return 1;
}

// Test 6: Symbol resolution across modules
static int test_symbol_resolution(void) {
    TEST("Symbol resolution");

    const char* source =
        "fn helper_one() i32 {\n"
        "    return 42;\n"
        "}\n"
        "\n"
        "fn helper_two() i32 {\n"
        "    return helper_one() + 8;\n"
        "}\n"
        "\n"
        "fn helper_three() i32 {\n"
        "    let a = helper_one();\n"
        "    let b = helper_two();\n"
        "    return a + b;\n"
        "}\n"
        "\n"
        "pub fn main_func() i32 {\n"
        "    return helper_three();\n"
        "}\n";

    if (!compile_and_verify("symbol_resolution", source)) {
        FAIL("Pipeline failed");
    }

    PASS();
    return 1;
}

// Test 7: State machine generation for coroutines
static int test_state_machine(void) {
    TEST("State machine generation");

    const char* source =
        "pub async fn http_request(url: []u8) ![]u8 {\n"
        "    // State 0: Initial\n"
        "    let socket = connect(url);\n"
        "    defer close(socket);\n"
        "    \n"
        "    // State 1: Sending request\n"
        "    suspend send_request(socket);\n"
        "    \n"
        "    // State 2: Waiting for response\n"
        "    let response = suspend receive_response(socket);\n"
        "    \n"
        "    // State 3: Done\n"
        "    return response;\n"
        "}\n";

    if (!compile_and_verify("state_machine", source)) {
        FAIL("Pipeline failed");
    }

    PASS();
    return 1;
}

// Test 8: Nested async calls
static int test_nested_async(void) {
    TEST("Nested async function calls");

    const char* source =
        "async fn inner_task(x: i32) i32 {\n"
        "    suspend;\n"
        "    return x * 2;\n"
        "}\n"
        "\n"
        "pub async fn outer_task(n: i32) i32 {\n"
        "    let a = suspend inner_task(n);\n"
        "    let b = suspend inner_task(a);\n"
        "    return b;\n"
        "}\n";

    if (!compile_and_verify("nested_async", source)) {
        FAIL("Pipeline failed");
    }

    PASS();
    return 1;
}

// Test 9: Complex control flow
static int test_complex_control_flow(void) {
    TEST("Complex control flow");

    const char* source =
        "fn fibonacci(n: i32) i32 {\n"
        "    if (n <= 1) {\n"
        "        return n;\n"
        "    }\n"
        "    \n"
        "    let a: i32 = 0;\n"
        "    let b: i32 = 1;\n"
        "    \n"
        "    for (let i: i32 = 2; i <= n; i = i + 1) {\n"
        "        let temp = a + b;\n"
        "        a = b;\n"
        "        b = temp;\n"
        "    }\n"
        "    \n"
        "    return b;\n"
        "}\n"
        "\n"
        "pub fn test_fib() i32 {\n"
        "    return fibonacci(10);\n"
        "}\n";

    if (!compile_and_verify("control_flow", source)) {
        FAIL("Pipeline failed");
    }

    PASS();
    return 1;
}

// Test 10: Mixed async and sync
static int test_mixed_async_sync(void) {
    TEST("Mixed async and sync functions");

    const char* source =
        "fn sync_helper(x: i32) i32 {\n"
        "    return x + 10;\n"
        "}\n"
        "\n"
        "async fn async_worker(n: i32) i32 {\n"
        "    let temp = sync_helper(n);\n"
        "    suspend;\n"
        "    return temp * 2;\n"
        "}\n"
        "\n"
        "pub async fn coordinator(value: i32) i32 {\n"
        "    let a = sync_helper(value);\n"
        "    let b = suspend async_worker(a);\n"
        "    let c = sync_helper(b);\n"
        "    return c;\n"
        "}\n";

    if (!compile_and_verify("mixed", source)) {
        FAIL("Pipeline failed");
    }

    PASS();
    return 1;
}

int main(void) {
    printf("\n========================================\n");
    printf("Complete Pipeline End-to-End Tests\n");
    printf("========================================\n\n");

    // Run comprehensive tests
    test_simple_function();
    test_async_function();
    test_error_handling();
    test_defer_errdefer();
    test_type_checking();
    test_symbol_resolution();
    test_state_machine();
    test_nested_async();
    test_complex_control_flow();
    test_mixed_async_sync();

    // Print summary
    printf("\n========================================\n");
    printf("Pipeline Tests Complete\n");
    printf("========================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);

    if (tests_passed == tests_run) {
        printf("\n🎉 All pipeline tests passed!\n");
    } else {
        printf("\n❌ Some tests failed\n");
    }

    printf("========================================\n\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
