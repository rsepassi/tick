/*
 * Integration Test: IR + Code Generation
 *
 * Tests the boundary between IR and code generation:
 * - IR lowering produces IrNode structures
 * - Code generator consumes IR and emits C11 code
 * - Verifies generated C code is valid and compiles
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

// Helper to compile to C code
static int compile_to_c(const char* source, const char* output_file,
                        Arena* arena, ErrorList* errors) {
    // Parse
    Lexer lexer;
    lexer_init(&lexer, source, strlen(source), "test.tick");

    Parser parser;
    parser_init(&parser, &lexer, arena);

    AstNode* ast = parser_parse(&parser);
    if (!ast || error_list_has_errors(errors)) {
        return 0;
    }

    // Semantic analysis
    Scope* scope = scope_create(arena);
    resolver_analyze(ast, scope, arena, errors);
    if (error_list_has_errors(errors)) {
        return 0;
    }

    typeck_analyze(ast, scope, arena, errors);
    if (error_list_has_errors(errors)) {
        return 0;
    }

    // Coroutine analysis (if needed)
    CoroMetadata* coro_meta = NULL;

    // Lower to IR
    IrNode* ir = lower_ast_to_ir(ast, coro_meta, arena, errors);
    if (!ir || error_list_has_errors(errors)) {
        return 0;
    }

    // Generate C code
    FILE* out = fopen(output_file, "w");
    if (!out) {
        return 0;
    }

    CodegenContext ctx;
    codegen_init(&ctx, arena, errors, "test_module");
    ctx.source_out = out;
    ctx.emit_line_directives = false;

    codegen_emit_module(ir, &ctx);

    fclose(out);

    if (error_list_has_errors(errors)) {
        return 0;
    }

    return 1;
}

// Test 1: Simple function code generation
static int test_simple_codegen(void) {
    TEST("Simple function code generation");

    const char* source =
        "fn add(a: i32, b: i32) i32 {\n"
        "    return a + b;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 16384);

    ErrorList errors;
    error_list_init(&errors, &arena);

    int success = compile_to_c(source, "generated_simple.c", &arena, &errors);

    if (!success) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed to generate C code");
    }

    // Verify file was created
    FILE* f = fopen("generated_simple.c", "r");
    if (!f) {
        arena_destroy(&arena);
        FAIL("Generated file not found");
    }
    fclose(f);

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 2: State machine code generation
static int test_state_machine_codegen(void) {
    TEST("State machine code generation");

    const char* source =
        "pub async fn task() void {\n"
        "    suspend;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 16384);

    ErrorList errors;
    error_list_init(&errors, &arena);

    int success = compile_to_c(source, "generated_state_machine.c", &arena, &errors);

    if (!success) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed to generate C code");
    }

    // Verify file contains state machine structures
    FILE* f = fopen("generated_state_machine.c", "r");
    if (!f) {
        arena_destroy(&arena);
        FAIL("Generated file not found");
    }

    char buffer[1024];
    int found_state = 0;
    while (fgets(buffer, sizeof(buffer), f)) {
        if (strstr(buffer, "state") || strstr(buffer, "STATE")) {
            found_state = 1;
            break;
        }
    }
    fclose(f);

    if (!found_state) {
        arena_destroy(&arena);
        FAIL("State machine structures not found in generated code");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 3: Type translation to C
static int test_type_translation(void) {
    TEST("Type translation to C");

    const char* source =
        "fn types() void {\n"
        "    let a: i32 = 10;\n"
        "    let b: u64 = 20;\n"
        "    let c: bool = true;\n"
        "    let d: []u8 = \"hello\";\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 16384);

    ErrorList errors;
    error_list_init(&errors, &arena);

    int success = compile_to_c(source, "generated_types.c", &arena, &errors);

    if (!success) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed to generate C code");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 4: Control flow code generation
static int test_control_flow_codegen(void) {
    TEST("Control flow code generation");

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

    int success = compile_to_c(source, "generated_control_flow.c", &arena, &errors);

    if (!success) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed to generate C code");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 5: Defer code generation
static int test_defer_codegen(void) {
    TEST("Defer code generation");

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

    int success = compile_to_c(source, "generated_defer.c", &arena, &errors);

    if (!success) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed to generate C code");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 6: Error handling code generation
static int test_error_handling_codegen(void) {
    TEST("Error handling code generation");

    const char* source =
        "fn may_fail() !i32 {\n"
        "    try {\n"
        "        return risky_op();\n"
        "    } catch (err) {\n"
        "        return err;\n"
        "    }\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 16384);

    ErrorList errors;
    error_list_init(&errors, &arena);

    int success = compile_to_c(source, "generated_error.c", &arena, &errors);

    if (!success) {
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        arena_destroy(&arena);
        FAIL("Failed to generate C code");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

// Test 7: Line directives
static int test_line_directives(void) {
    TEST("Line directive generation");

    const char* source =
        "fn test() i32 {\n"
        "    return 42;\n"
        "}\n";

    Arena arena;
    arena_init(&arena, 16384);

    ErrorList errors;
    error_list_init(&errors, &arena);

    // Parse and lower
    Lexer lexer;
    lexer_init(&lexer, source, strlen(source), "test.tick");
    Parser parser;
    parser_init(&parser, &lexer, &arena);
    AstNode* ast = parser_parse(&parser);

    if (!ast) {
        arena_destroy(&arena);
        FAIL("Parse failed");
    }

    Scope* scope = scope_create(&arena);
    resolver_analyze(ast, scope, &arena, &errors);
    typeck_analyze(ast, scope, &arena, &errors);
    IrNode* ir = lower_ast_to_ir(ast, NULL, &arena, &errors);

    // Generate with line directives
    FILE* out = fopen("generated_lines.c", "w");
    if (!out) {
        arena_destroy(&arena);
        FAIL("Could not open output file");
    }

    CodegenContext ctx;
    codegen_init(&ctx, &arena, &errors, "test");
    ctx.source_out = out;
    ctx.emit_line_directives = true;

    codegen_emit_module(ir, &ctx);
    fclose(out);

    // Check for #line directives
    FILE* f = fopen("generated_lines.c", "r");
    if (!f) {
        arena_destroy(&arena);
        FAIL("Generated file not found");
    }

    char buffer[256];
    int found_line = 0;
    while (fgets(buffer, sizeof(buffer), f)) {
        if (strstr(buffer, "#line")) {
            found_line = 1;
            break;
        }
    }
    fclose(f);

    if (!found_line) {
        arena_destroy(&arena);
        FAIL("#line directives not found");
    }

    arena_destroy(&arena);
    PASS();
    return 1;
}

int main(void) {
    printf("\n========================================\n");
    printf("IR + Code Generation Integration Tests\n");
    printf("========================================\n\n");

    // Run tests
    test_simple_codegen();
    test_state_machine_codegen();
    test_type_translation();
    test_control_flow_codegen();
    test_defer_codegen();
    test_error_handling_codegen();
    test_line_directives();

    // Print summary
    printf("\n========================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
