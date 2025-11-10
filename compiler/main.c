#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "lexer.h"
#include "parser.h"
#include "symbol.h"
#include "type.h"
#include "ir.h"
#include "codegen.h"
#include "arena.h"
#include "error.h"
#include "string_pool.h"

// External function declarations
void resolver_resolve(AstNode* module_node, SymbolTable* symbol_table,
                     StringPool* string_pool, ErrorList* errors, Arena* arena);
void typeck_check_module(AstNode* module_node, SymbolTable* symbol_table,
                        ErrorList* errors, Arena* arena);
SymbolTable* symbol_table_create(Arena* arena);

// Simple compiler driver
typedef struct {
    const char* input_file;
    const char* output_base;
    bool emit_c_only;
    bool verbose;
} CompilerOptions;

static void usage(const char* prog) {
    fprintf(stderr, "Usage: %s [options] <input.tick>\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -o <name>        Output base name (default: based on input)\n");
    fprintf(stderr, "  -emit-c          Emit C code only (no compilation)\n");
    fprintf(stderr, "  -v               Verbose output\n");
    fprintf(stderr, "  -h               Show this help\n");
}

static bool parse_args(int argc, char** argv, CompilerOptions* opts) {
    memset(opts, 0, sizeof(CompilerOptions));

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return false;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -o requires an argument\n");
                return false;
            }
            opts->output_base = argv[++i];
        } else if (strcmp(argv[i], "-emit-c") == 0) {
            opts->emit_c_only = true;
        } else if (strcmp(argv[i], "-v") == 0) {
            opts->verbose = true;
        } else if (argv[i][0] != '-') {
            if (opts->input_file) {
                fprintf(stderr, "Error: multiple input files not supported\n");
                return false;
            }
            opts->input_file = argv[i];
        } else {
            fprintf(stderr, "Error: unknown option %s\n", argv[i]);
            return false;
        }
    }

    if (!opts->input_file) {
        fprintf(stderr, "Error: no input file specified\n");
        usage(argv[0]);
        return false;
    }

    // Generate default output base if not specified
    if (!opts->output_base) {
        const char* base = strrchr(opts->input_file, '/');
        base = base ? base + 1 : opts->input_file;

        const char* ext = strrchr(base, '.');
        if (ext) {
            size_t len = ext - base;
            char* output = malloc(len + 1);
            memcpy(output, base, len);
            output[len] = '\0';
            opts->output_base = output;
        } else {
            opts->output_base = base;
        }
    }

    return true;
}

static char* read_file(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open file %s\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(content, 1, size, f);
    content[read] = '\0';
    fclose(f);

    return content;
}

int main(int argc, char** argv) {
    CompilerOptions opts;
    if (!parse_args(argc, argv, &opts)) {
        return 1;
    }

    if (opts.verbose) {
        printf("Compiling %s -> %s\n", opts.input_file, opts.output_base);
    }

    // Read input file
    char* source = read_file(opts.input_file);
    if (!source) {
        return 1;
    }

    // Create arena for compilation
    Arena arena_storage;
    arena_init(&arena_storage, 64 * 1024);
    Arena* arena = &arena_storage;

    ErrorList errors = {0};
    StringPool string_pool_storage;
    string_pool_init(&string_pool_storage, arena);
    StringPool* strings = &string_pool_storage;

    // Phase 1-2: Lexing and Parsing
    if (opts.verbose) printf("Phase 1-2: Lexing and parsing...\n");

    Lexer lexer;
    lexer_init(&lexer, source, strlen(source), opts.input_file, strings, &errors);

    Parser parser;
    parser_init(&parser, &lexer, arena, &errors);

    AstNode* ast = parser_parse(&parser);
    parser_cleanup(&parser);

    if (!ast) {
        fprintf(stderr, "Parsing failed: returned NULL AST");
        if (errors.count > 0) {
            fprintf(stderr, " with %zu errors:\n", errors.count);
            for (size_t i = 0; i < errors.count; i++) {
                fprintf(stderr, "%s:%u:%u: %s\n",
                    errors.errors[i].loc.filename,
                    errors.errors[i].loc.line,
                    errors.errors[i].loc.column,
                    errors.errors[i].message);
            }
        } else {
            fprintf(stderr, " but no errors recorded.\n");
        }
        free(source);
        arena_free(arena);
        return 1;
    }

    if (errors.count > 0) {
        fprintf(stderr, "Parsing completed with %zu warnings/errors:\n", errors.count);
        for (size_t i = 0; i < errors.count; i++) {
            fprintf(stderr, "%s:%u:%u: %s\n",
                errors.errors[i].loc.filename,
                errors.errors[i].loc.line,
                errors.errors[i].loc.column,
                errors.errors[i].message);
        }
    }

    // Phase 3: Semantic analysis
    if (opts.verbose) printf("Phase 3: Name resolution...\n");
    SymbolTable symbols;
    symbol_table_init(&symbols, arena);

    resolver_resolve(ast, &symbols, strings, &errors, arena);
    if (errors.count > 0) {
        fprintf(stderr, "Name resolution failed with %zu errors:\n", errors.count);
        for (size_t i = 0; i < errors.count; i++) {
            fprintf(stderr, "%s:%u:%u: %s\n",
                errors.errors[i].loc.filename,
                errors.errors[i].loc.line,
                errors.errors[i].loc.column,
                errors.errors[i].message);
        }
        free(source);
        arena_free(arena);
        return 1;
    }

    if (opts.verbose) printf("Phase 3b: Type checking...\n");
    typeck_check_module(ast, &symbols, &errors, arena);
    if (errors.count > 0) {
        fprintf(stderr, "Type checking failed with %zu errors:\n", errors.count);
        for (size_t i = 0; i < errors.count; i++) {
            fprintf(stderr, "%s:%u:%u: %s\n",
                errors.errors[i].loc.filename,
                errors.errors[i].loc.line,
                errors.errors[i].loc.column,
                errors.errors[i].message);
        }
        free(source);
        arena_free(arena);
        return 1;
    }

    // Phase 4: IR lowering
    if (opts.verbose) printf("Phase 4: IR lowering...\n");
    IrModule* ir = ir_lower_ast(ast, arena);
    if (!ir) {
        fprintf(stderr, "IR lowering failed\n");
        free(source);
        arena_free(arena);
        return 1;
    }

    // DEBUG: Print IR info
    if (opts.verbose) {
        printf("IR lowering complete. Module has %zu functions\n", ir->function_count);
        for (size_t i = 0; i < ir->function_count; i++) {
            IrFunction* func = ir->functions[i];
            printf("  Function %zu: %s\n", i, func->name);
            printf("    Blocks: %zu\n", func->block_count);
            for (size_t j = 0; j < func->block_count; j++) {
                printf("      Block %zu: %u instructions\n", j, (unsigned)func->blocks[j]->instruction_count);
            }
        }
    }

    // Phase 5: Code generation
    if (opts.verbose) printf("Phase 5: Code generation...\n");

    // Open output files
    char header_path[256];
    char source_path[256];
    snprintf(header_path, sizeof(header_path), "%s.h", opts.output_base);
    snprintf(source_path, sizeof(source_path), "%s.c", opts.output_base);

    FILE* header_file = fopen(header_path, "w");
    FILE* source_file = fopen(source_path, "w");
    if (!header_file || !source_file) {
        fprintf(stderr, "Error: cannot create output files\n");
        if (header_file) fclose(header_file);
        if (source_file) fclose(source_file);
        free(source);
        arena_free(arena);
        return 1;
    }

    CodegenContext codegen_ctx;
    codegen_init(&codegen_ctx, arena, &errors, opts.output_base);
    codegen_ctx.header_out = header_file;
    codegen_ctx.source_out = source_file;
    codegen_ctx.emit_line_directives = false;  // Disable line directives for cleaner output

    IrNode* ir_node = arena_alloc(arena, sizeof(IrNode), 8);
    ir_node->kind = IR_MODULE;
    ir_node->data.module = ir;

    // Also generate lang_runtime.h if needed
    FILE* runtime_h = fopen("lang_runtime.h", "w");
    if (runtime_h) {
        codegen_emit_runtime_header(runtime_h);
        fclose(runtime_h);
    }

    codegen_emit_module(ir_node, &codegen_ctx);

    fclose(header_file);
    fclose(source_file);

    if (errors.count > 0) {
        fprintf(stderr, "Code generation failed with %zu errors:\n", errors.count);
        for (size_t i = 0; i < errors.count; i++) {
            fprintf(stderr, "%s\n", errors.errors[i].message);
        }
        free(source);
        arena_free(arena);
        return 1;
    }

    printf("Generated %s and %s\n", header_path, source_path);

    // Clean up
    free(source);
    arena_free(arena);

    return 0;
}
