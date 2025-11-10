/*
 * Integration Test: Full Compilation Pipeline
 *
 * Tests the complete compiler pipeline:
 * - Source code → Lexer → Parser → Semantic → Coroutine → Lowering → Codegen → C11 code
 * - Verifies generated C code compiles with gcc -std=c11 -ffreestanding
 * - Tests end-to-end compilation with real implementations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
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

// Full compilation pipeline
static int compile_tick_to_c(const char* source, const char* output_file,
                             Arena* arena, ErrorList* errors) {
    // Phase 1: Lexing
    Lexer lexer;
    lexer_init(&lexer, source, strlen(source), "test.tick");

    // Phase 2: Parsing
    Parser parser;
    parser_init(&parser, &lexer, arena);
    AstNode* ast = parser_parse(&parser);
    if (!ast || error_list_has_errors(errors)) {
        fprintf(stderr, "Parse phase failed\n");
        return 0;
    }

    // Phase 3: Symbol Resolution
    Scope* scope = scope_create(arena);
    resolver_analyze(ast, scope, arena, errors);
    if (error_list_has_errors(errors)) {
        fprintf(stderr, "Symbol resolution phase failed\n");
        return 0;
    }

    // Phase 4: Type Checking
    typeck_analyze(ast, scope, arena, errors);
    if (error_list_has_errors(errors)) {
        fprintf(stderr, "Type checking phase failed\n");
        return 0;
    }

    // Phase 5: Coroutine Analysis (if needed)
    CoroMetadata* coro_meta = NULL;
    // TODO: Check if function is async and run coroutine analysis

    // Phase 6: IR Lowering
    IrNode* ir = lower_ast_to_ir(ast, coro_meta, arena, errors);
    if (!ir || error_list_has_errors(errors)) {
        fprintf(stderr, "IR lowering phase failed\n");
        return 0;
    }

    // Phase 7: Code Generation
    FILE* out = fopen(output_file, "w");
    if (!out) {
        fprintf(stderr, "Could not open output file: %s\n", output_file);
        return 0;
    }

    CodegenContext ctx;
    codegen_init(&ctx, arena, errors, "test_module");
    ctx.source_out = out;
    ctx.emit_line_directives = true;

    // Emit runtime header
    codegen_emit_runtime_header(out);

    // Emit module
    codegen_emit_module(ir, &ctx);

    fclose(out);

    if (error_list_has_errors(errors)) {
        fprintf(stderr, "Code generation phase failed\n");
        return 0;
    }

    return 1;
}

// Helper to compile C code with gcc
static int compile_c_code(const char* c_file, const char* output_file) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "gcc -Wall -Wextra -Werror -std=c11 -ffreestanding -c %s -o %s 2>&1",
             c_file, output_file);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        return 0;
    }

    char buffer[256];
    int has_output = 0;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        fprintf(stderr, "%s", buffer);
        has_output = 1;
    }

    int status = pclose(pipe);
    return (status == 0 && !has_output);
}

// Test 1: Simple function full compile
static int test_simple_full_compile(void) {
    TEST("Simple function full compile");

    const char* source =
        "fn add(a: i32, b: i32) i32 {\n"
        "    return a + b;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 32768);

    ErrorList errors;
    error_list_init(&errors, &arena);

    // Compile to C
    int success = compile_tick_to_c(source, "test_simple_full.c", &arena, &errors);

    if (!success) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed to compile to C");
    }

    // Compile C code
    if (!compile_c_code("test_simple_full.c", "test_simple_full.o")) {
        arena_destroy(&arena);
        FAIL("Generated C code does not compile");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 2: Multiple functions
static int test_multiple_functions_compile(void) {
    TEST("Multiple functions full compile");

    const char* source =
        "fn helper() i32 {\n"
        "    return 42;\n"
        "}\n"
        "\n"
        "fn main_func() i32 {\n"
        "    let x = helper();\n"
        "    return x + 10;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 32768);

    ErrorList errors;
    error_list_init(&errors, &arena);

    int success = compile_tick_to_c(source, "test_multi_full.c", &arena, &errors);

    if (!success) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed to compile to C");
    }

    if (!compile_c_code("test_multi_full.c", "test_multi_full.o")) {
        arena_destroy(&arena);
        FAIL("Generated C code does not compile");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 3: Control flow
static int test_control_flow_compile(void) {
    TEST("Control flow full compile");

    const char* source =
        "fn conditional(x: i32) i32 {\n"
        "    if (x > 10) {\n"
        "        return x * 2;\n"
        "    } else {\n"
        "        return x + 5;\n"
        "    }\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 32768);

    ErrorList errors;
    error_list_init(&errors, &arena);

    int success = compile_tick_to_c(source, "test_control_full.c", &arena, &errors);

    if (!success) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed to compile to C");
    }

    if (!compile_c_code("test_control_full.c", "test_control_full.o")) {
        arena_destroy(&arena);
        FAIL("Generated C code does not compile");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 4: Struct types
static int test_struct_compile(void) {
    TEST("Struct types full compile");

    const char* source =
        "struct Point {\n"
        "    x: i32,\n"
        "    y: i32,\n"
        "}\n"
        "\n"
        "fn make_point(x: i32, y: i32) Point {\n"
        "    let p: Point = Point { x: x, y: y };\n"
        "    return p;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 32768);

    ErrorList errors;
    error_list_init(&errors, &arena);

    int success = compile_tick_to_c(source, "test_struct_full.c", &arena, &errors);

    if (!success) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed to compile to C");
    }

    if (!compile_c_code("test_struct_full.c", "test_struct_full.o")) {
        arena_destroy(&arena);
        FAIL("Generated C code does not compile");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 5: Loops
static int test_loop_compile(void) {
    TEST("Loop full compile");

    const char* source =
        "fn sum_range(n: i32) i32 {\n"
        "    let sum: i32 = 0;\n"
        "    for (let i: i32 = 0; i < n; i = i + 1) {\n"
        "        sum = sum + i;\n"
        "    }\n"
        "    return sum;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 32768);

    ErrorList errors;
    error_list_init(&errors, &arena);

    int success = compile_tick_to_c(source, "test_loop_full.c", &arena, &errors);

    if (!success) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed to compile to C");
    }

    if (!compile_c_code("test_loop_full.c", "test_loop_full.o")) {
        arena_destroy(&arena);
        FAIL("Generated C code does not compile");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 6: Verify compilation flags
static int test_compilation_flags(void) {
    TEST("Verify strict compilation flags");

    const char* source =
        "fn test() i32 {\n"
        "    return 42;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 32768);

    ErrorList errors;
    error_list_init(&errors, &arena);

    int success = compile_tick_to_c(source, "test_flags_full.c", &arena, &errors);

    if (!success) {
        arena_destroy(&arena);
        FAIL("Failed to compile to C");
    }

    // Try with very strict flags
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "gcc -Wall -Wextra -Werror -Wpedantic -std=c11 -ffreestanding "
             "-Wno-unused -c test_flags_full.c -o test_flags_full.o 2>&1");

    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        arena_destroy(&arena);
        FAIL("Could not run gcc");
    }

    char buffer[256];
    int has_errors = 0;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        if (strstr(buffer, "error:")) {
            fprintf(stderr, "%s", buffer);
            has_errors = 1;
        }
    }

    int status = pclose(pipe);
    if (status != 0 || has_errors) {
        arena_destroy(&arena);
        FAIL("Code does not compile with strict flags");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

int main(void) {
    printf("\n========================================\n");
    printf("Full Compilation Pipeline Tests\n");
    printf("========================================\n\n");

    // Run tests
    test_simple_full_compile();
    test_multiple_functions_compile();
    test_control_flow_compile();
    test_struct_compile();
    test_loop_compile();
    test_compilation_flags();

    // Print summary
    printf("\n========================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
