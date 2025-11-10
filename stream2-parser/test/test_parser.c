#include "../parser.h"
#include "../ast.h"
#include "../lexer.h"
#include "../arena.h"
#include "../error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test framework macros
#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("Running test: %s...", #name); \
    test_##name(); \
    printf(" PASSED\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NEQ(a, b) ASSERT((a) != (b))
#define ASSERT_NULL(p) ASSERT((p) == NULL)
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)

// Helper to create parser from source string
static Parser* create_parser(const char* source, Arena* arena, ErrorList* errors) {
    Lexer* lexer = (Lexer*)malloc(sizeof(Lexer));
    lexer_init(lexer, source, strlen(source), "test.tick");

    Parser* parser = (Parser*)malloc(sizeof(Parser));
    parser_init(parser, lexer, arena, errors);

    return parser;
}

static void cleanup_parser(Parser* parser) {
    parser_cleanup(parser);
    free(parser->lexer);
    free(parser);
}

// Test cases

TEST(empty_module) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source = "";
    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);
    ASSERT_EQ(root->kind, AST_MODULE);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(simple_function) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let add = fn(x: i32, y: i32) -> i32 {\n"
        "    return x + y;\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);
    ASSERT_EQ(root->kind, AST_MODULE);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(struct_declaration) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let Point = struct {\n"
        "    x: i32,\n"
        "    y: i32\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);
    ASSERT_EQ(root->kind, AST_MODULE);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(enum_declaration) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let Status = enum(u8) {\n"
        "    OK = 0,\n"
        "    ERROR = 1,\n"
        "    PENDING = 2\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);
    ASSERT_EQ(root->kind, AST_MODULE);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(union_declaration) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let Result = union {\n"
        "    ok: i32,\n"
        "    error: ErrorCode\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);
    ASSERT_EQ(root->kind, AST_MODULE);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(if_statement) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    if x > 0 {\n"
        "        return 1;\n"
        "    } else {\n"
        "        return 0;\n"
        "    }\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(for_loop_range) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    for i in 0..10 {\n"
        "        process(i);\n"
        "    }\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(for_loop_condition) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    for x > 0 {\n"
        "        x = x - 1;\n"
        "    }\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(for_loop_infinite) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    for {\n"
        "        work();\n"
        "    }\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(switch_statement) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn(x: i32) {\n"
        "    switch x {\n"
        "        case 1, 2, 3:\n"
        "            handleLow();\n"
        "        case 4:\n"
        "            handleMid();\n"
        "        default:\n"
        "            handleOther();\n"
        "    }\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(while_switch_statement) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    while switch state {\n"
        "        case INIT:\n"
        "            state = PROCESS;\n"
        "            continue switch state;\n"
        "        case PROCESS:\n"
        "            break;\n"
        "    }\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(defer_statement) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    let file = openFile(path);\n"
        "    defer closeFile(file);\n"
        "    processFile(file);\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(errdefer_statement) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    let buffer = allocate(1024);\n"
        "    errdefer free(buffer);\n"
        "    process(buffer);\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(async_call) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    let future = async worker();\n"
        "    resume future;\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(suspend_statement) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let worker = fn() {\n"
        "    processStart();\n"
        "    suspend;\n"
        "    processEnd();\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(try_catch) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    try {\n"
        "        let result = divide(10, 0);\n"
        "        process(result);\n"
        "    } catch |err| {\n"
        "        handleError(err);\n"
        "    }\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(try_propagate) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    let result = try riskyOperation();\n"
        "    return result;\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(binary_expressions) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    let a = 1 + 2;\n"
        "    let b = 3 * 4;\n"
        "    let c = a - b;\n"
        "    let d = c / 2;\n"
        "    let e = d % 3;\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(comparison_expressions) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    let a = x < y;\n"
        "    let b = x > y;\n"
        "    let c = x <= y;\n"
        "    let d = x >= y;\n"
        "    let e = x == y;\n"
        "    let f = x != y;\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(logical_expressions) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    let a = x && y;\n"
        "    let b = x || y;\n"
        "    let c = !x;\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(bitwise_expressions) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    let a = x & y;\n"
        "    let b = x | y;\n"
        "    let c = x ^ y;\n"
        "    let d = x << 2;\n"
        "    let e = y >> 3;\n"
        "    let f = ~x;\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(unary_expressions) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    let a = -x;\n"
        "    let b = &value;\n"
        "    let c = *ptr;\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(field_access) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    let x = point.x;\n"
        "    let y = ptr->y;\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(array_index) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    let x = array[0];\n"
        "    let y = matrix[i][j];\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(function_call) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    let x = foo();\n"
        "    let y = bar(1, 2, 3);\n"
        "    let z = baz(x, y);\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(struct_initialization) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    let p = Point { x: 10, y: 20 };\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(array_initialization) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    let arr = [1, 2, 3, 4, 5];\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(import_declaration) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source = "let math = import \"math\";\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(public_declaration) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "pub let exported_func = fn() {\n"
        "    return 42;\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(volatile_variable) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let test = fn() {\n"
        "    volatile var reg: u32;\n"
        "    reg = 0xFF;\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

TEST(packed_struct) {
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    const char* source =
        "let Packet = struct packed {\n"
        "    header: u8,\n"
        "    length: u16,\n"
        "    data: [256]u8\n"
        "};\n";

    Parser* parser = create_parser(source, &arena, &errors);

    AstNode* root = parser_parse(parser);
    ASSERT_NOT_NULL(root);

    cleanup_parser(parser);
    arena_free(&arena);
}

// Main test runner
int main(void) {
    printf("Running parser tests...\n\n");

    RUN_TEST(empty_module);
    RUN_TEST(simple_function);
    RUN_TEST(struct_declaration);
    RUN_TEST(enum_declaration);
    RUN_TEST(union_declaration);
    RUN_TEST(if_statement);
    RUN_TEST(for_loop_range);
    RUN_TEST(for_loop_condition);
    RUN_TEST(for_loop_infinite);
    RUN_TEST(switch_statement);
    RUN_TEST(while_switch_statement);
    RUN_TEST(defer_statement);
    RUN_TEST(errdefer_statement);
    RUN_TEST(async_call);
    RUN_TEST(suspend_statement);
    RUN_TEST(try_catch);
    RUN_TEST(try_propagate);
    RUN_TEST(binary_expressions);
    RUN_TEST(comparison_expressions);
    RUN_TEST(logical_expressions);
    RUN_TEST(bitwise_expressions);
    RUN_TEST(unary_expressions);
    RUN_TEST(field_access);
    RUN_TEST(array_index);
    RUN_TEST(function_call);
    RUN_TEST(struct_initialization);
    RUN_TEST(array_initialization);
    RUN_TEST(import_declaration);
    RUN_TEST(public_declaration);
    RUN_TEST(volatile_variable);
    RUN_TEST(packed_struct);

    printf("\nAll tests passed!\n");
    return 0;
}
