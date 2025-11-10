#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "lexer.h"
#include "parser.h"
#include "arena.h"
#include "error.h"
#include "string_pool.h"
#include "symbol.h"
#include "ir.h"
#include "codegen.h"

void resolver_resolve(AstNode* module_node, SymbolTable* symbol_table,
                     StringPool* string_pool, ErrorList* errors, Arena* arena);
void typeck_check_module(AstNode* module_node, SymbolTable* symbol_table,
                        ErrorList* errors, Arena* arena);

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { printf("  Test: %s ... ", name); tests_run++; } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 0; } while(0)

// Helper: Generate C code to memory
static char* generate_c_code(const char* source, Arena* arena, ErrorList* errors) {
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

    IrModule* ir = ir_lower_ast(ast, arena);
    if (!ir) {
        return NULL;
    }

    // Generate C code to temporary file
    FILE* tmp = tmpfile();
    if (!tmp) {
        return NULL;
    }

    CodegenContext ctx;
    codegen_init(&ctx, arena, errors, "test");
    ctx.header_out = tmpfile();  // Discard header
    ctx.source_out = tmp;

    IrNode* ir_node = arena_alloc(arena, sizeof(IrNode), 8);
    ir_node->kind = IR_MODULE;
    ir_node->data.module = ir;

    codegen_emit_module(ir_node, &ctx);

    if (errors->count > 0) {
        fclose(tmp);
        if (ctx.header_out) fclose(ctx.header_out);
        return NULL;
    }

    // Read generated code into buffer
    fseek(tmp, 0, SEEK_END);
    long size = ftell(tmp);
    fseek(tmp, 0, SEEK_SET);

    char* buffer = (char*)malloc(size + 1);
    fread(buffer, 1, size, tmp);
    buffer[size] = '\0';

    fclose(tmp);
    if (ctx.header_out) fclose(ctx.header_out);

    return buffer;
}

// Test 1: Simple function codegen
static int test1(void) {
    TEST("Simple function codegen");

    Arena arena;
    arena_init(&arena, 64 * 1024);
    ErrorList errors = {0};

    const char* source = "let add = fn(x: i32, y: i32) -> i32 { return x + y; };";

    char* c_code = generate_c_code(source, &arena, &errors);
    if (!c_code) {
        FAIL("Failed to generate C code");
    }

    // Check that generated code contains function signature
    if (!strstr(c_code, "int32_t")) {
        free(c_code);
        FAIL("Generated code missing type declarations");
    }

    if (!strstr(c_code, "__u_add")) {
        free(c_code);
        FAIL("Generated code missing function name");
    }

    free(c_code);
    arena_free(&arena);
    PASS();
    return 1;
}

// Test 2: Multiple functions codegen
static int test2(void) {
    TEST("Multiple functions codegen");

    Arena arena;
    arena_init(&arena, 64 * 1024);
    ErrorList errors = {0};

    const char* source =
        "let add = fn(x: i32, y: i32) -> i32 { return x + y; };\n"
        "let sub = fn(x: i32, y: i32) -> i32 { return x - y; };\n"
        "let mul = fn(x: i32, y: i32) -> i32 { return x * y; };";

    char* c_code = generate_c_code(source, &arena, &errors);
    if (!c_code) {
        FAIL("Failed to generate C code");
    }

    // Check all functions are present
    if (!strstr(c_code, "__u_add") || !strstr(c_code, "__u_sub") || !strstr(c_code, "__u_mul")) {
        free(c_code);
        FAIL("Generated code missing functions");
    }

    free(c_code);
    arena_free(&arena);
    PASS();
    return 1;
}

// Test 3: Type translation
static int test3(void) {
    TEST("Type translation codegen");

    Arena arena;
    arena_init(&arena, 64 * 1024);
    ErrorList errors = {0};

    const char* source =
        "let f1 = fn(x: i8) -> i8 { return x; };\n"
        "let f2 = fn(x: u32) -> u32 { return x; };\n"
        "let f3 = fn(x: i64) -> i64 { return x; };\n"
        "let f4 = fn(x: bool) -> bool { return x; };";

    char* c_code = generate_c_code(source, &arena, &errors);
    if (!c_code) {
        FAIL("Failed to generate C code");
    }

    // Check type translations
    if (!strstr(c_code, "int8_t") || !strstr(c_code, "uint32_t") ||
        !strstr(c_code, "int64_t") || !strstr(c_code, "bool")) {
        free(c_code);
        FAIL("Type translations incorrect");
    }

    free(c_code);
    arena_free(&arena);
    PASS();
    return 1;
}

// Test 4: Control flow codegen
static int test4(void) {
    TEST("Control flow codegen");

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

    char* c_code = generate_c_code(source, &arena, &errors);
    if (!c_code) {
        FAIL("Failed to generate C code");
    }

    // Check for control flow structures (goto or labels)
    if (!strstr(c_code, "goto") && !strstr(c_code, "if")) {
        free(c_code);
        FAIL("Missing control flow in generated code");
    }

    free(c_code);
    arena_free(&arena);
    PASS();
    return 1;
}

// Test 5: C code compilation validation
static int test5(void) {
    TEST("C code validation");

    Arena arena;
    arena_init(&arena, 64 * 1024);
    ErrorList errors = {0};

    const char* source =
        "let compute = fn(a: i32, b: i32, c: i32) -> i32 {\n"
        "    let temp1 = a + b;\n"
        "    let temp2 = temp1 * c;\n"
        "    return temp2;\n"
        "};";

    char* c_code = generate_c_code(source, &arena, &errors);
    if (!c_code) {
        FAIL("Failed to generate C code");
    }

    // Write to file and compile with GCC
    FILE* f = fopen("/tmp/test_codegen.c", "w");
    if (!f) {
        free(c_code);
        FAIL("Failed to write test file");
    }

    // Add necessary headers
    fprintf(f, "#include <stdint.h>\n");
    fprintf(f, "#include <stdbool.h>\n");
    fputs(c_code, f);
    fclose(f);

    // Try to compile
    int ret = system("gcc -std=c11 -ffreestanding -Wall -Wextra -c /tmp/test_codegen.c -o /tmp/test_codegen.o 2>&1");

    free(c_code);
    arena_free(&arena);

    if (WEXITSTATUS(ret) != 0) {
        FAIL("Generated C code does not compile");
    }

    unlink("/tmp/test_codegen.c");
    unlink("/tmp/test_codegen.o");

    PASS();
    return 1;
}

int main(void) {
    printf("\n========================================\n");
    printf("IR + Code Generation Integration Tests\n");
    printf("========================================\n\n");
    
    test1(); test2(); test3(); test4(); test5();
    
    printf("\n========================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
