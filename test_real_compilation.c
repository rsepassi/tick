/*
 * Real compilation test - attempt to compile 01_hello.tick
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "resolver.h"
#include "typeck.h"
#include "coro_analyze.h"
#include "lower.h"
#include "codegen.h"
#include "arena.h"
#include "error.h"
#include "string_pool.h"

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = 0;
    fclose(f);

    return buf;
}

int main(void) {
    printf("Attempting to compile examples/01_hello.tick...\n\n");

    // Read source file
    char* source = read_file("examples/01_hello.tick");
    if (!source) {
        fprintf(stderr, "ERROR: Could not read examples/01_hello.tick\n");
        return 1;
    }

    printf("Source file loaded (%zu bytes)\n", strlen(source));

    // Initialize infrastructure
    Arena arena;
    arena_init(&arena, 1024 * 1024);  // 1MB arena

    ErrorList errors;
    error_list_init(&errors, &arena);

    StringPool string_pool;
    string_pool_init(&string_pool, &arena);

    printf("Infrastructure initialized\n");

    // Phase 1: Lexer
    printf("\n[1/7] Running lexer...\n");
    Lexer lexer;
    lexer_init(&lexer, source, strlen(source), "01_hello.tick", &string_pool, &errors);

    // Phase 2: Parser
    printf("[2/7] Running parser...\n");
    Parser parser;
    parser_init(&parser, &lexer, &arena, &errors);
    AstNode* ast = parser_parse(&parser);

    if (!ast) {
        fprintf(stderr, "ERROR: Parser failed\n");
        if (error_list_has_errors(&errors)) {
            error_list_print(&errors, stderr);
        }
        free(source);
        arena_free(&arena);
        return 1;
    }
    printf("  AST generated successfully\n");

    // Phase 3: Semantic analysis (resolver)
    printf("[3/7] Running resolver...\n");
    SymbolTable* symtab = resolve_symbols(ast, &arena, &errors);
    if (!symtab || error_list_has_errors(&errors)) {
        fprintf(stderr, "ERROR: Symbol resolution failed\n");
        error_list_print(&errors, stderr);
        free(source);
        arena_free(&arena);
        return 1;
    }
    printf("  Symbols resolved\n");

    // Phase 4: Type checking
    printf("[4/7] Running type checker...\n");
    if (!typecheck(ast, symtab, &errors) || error_list_has_errors(&errors)) {
        fprintf(stderr, "ERROR: Type checking failed\n");
        error_list_print(&errors, stderr);
        free(source);
        arena_free(&arena);
        return 1;
    }
    printf("  Type checking passed\n");

    // Phase 5: Coroutine analysis
    printf("[5/7] Running coroutine analysis...\n");
    CoroMetadata* coro_meta = analyze_coroutines(ast, &arena, &errors);
    if (!coro_meta || error_list_has_errors(&errors)) {
        fprintf(stderr, "ERROR: Coroutine analysis failed\n");
        error_list_print(&errors, stderr);
        free(source);
        arena_free(&arena);
        return 1;
    }
    printf("  Coroutine analysis complete\n");

    // Phase 6: Lowering to IR
    printf("[6/7] Running IR lowering...\n");
    IrModule* ir = lower_to_ir(ast, coro_meta, &arena, &errors);
    if (!ir || error_list_has_errors(&errors)) {
        fprintf(stderr, "ERROR: IR lowering failed\n");
        error_list_print(&errors, stderr);
        free(source);
        arena_free(&arena);
        return 1;
    }
    printf("  IR generated\n");

    // Phase 7: Code generation
    printf("[7/7] Running code generation...\n");
    FILE* out_h = fopen("generated_01_hello.h", "w");
    FILE* out_c = fopen("generated_01_hello.c", "w");
    if (!out_h || !out_c) {
        fprintf(stderr, "ERROR: Could not create output files\n");
        free(source);
        arena_free(&arena);
        return 1;
    }

    if (!codegen(ir, out_h, out_c, &errors) || error_list_has_errors(&errors)) {
        fprintf(stderr, "ERROR: Code generation failed\n");
        error_list_print(&errors, stderr);
        fclose(out_h);
        fclose(out_c);
        free(source);
        arena_free(&arena);
        return 1;
    }

    fclose(out_h);
    fclose(out_c);
    printf("  Code generated: generated_01_hello.h, generated_01_hello.c\n");

    printf("\n✓ Compilation successful!\n");
    printf("\nAttempting to compile generated C code...\n");

    int result = system("gcc -std=c11 -Wall -Wextra -c generated_01_hello.c -o generated_01_hello.o 2>&1");
    if (result == 0) {
        printf("✓ Generated C code compiles successfully!\n");
    } else {
        printf("✗ Generated C code failed to compile\n");
    }

    free(source);
    arena_free(&arena);

    return result == 0 ? 0 : 1;
}
