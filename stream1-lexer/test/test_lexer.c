#include "../include/lexer.h"
#include "../include/string_pool.h"
#include "../include/error.h"
#include "../include/arena.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

// Test helper macros
#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("Running test_%s...\n", #name); \
    test_##name(); \
    printf("  PASSED\n"); \
} while(0)

#define ASSERT_EQ(a, b) assert((a) == (b))
#define ASSERT_STR_EQ(a, b) assert(strcmp((a), (b)) == 0)
#define ASSERT_TRUE(x) assert(x)
#define ASSERT_FALSE(x) assert(!(x))

// Test setup helpers
typedef struct {
    Arena arena;
    StringPool string_pool;
    ErrorList error_list;
    Lexer lexer;
} TestContext;

static void setup_test_context(TestContext* ctx, const char* source) {
    arena_init(&ctx->arena, 4096);
    string_pool_init(&ctx->string_pool, &ctx->arena);
    error_list_init(&ctx->error_list, &ctx->arena);
    lexer_init(&ctx->lexer, source, strlen(source), "test.txt",
               &ctx->string_pool, &ctx->error_list);
}

static void teardown_test_context(TestContext* ctx) {
    arena_free(&ctx->arena);
}

// Token assertion helper
static void assert_token(Token* token, TokenKind expected_kind, const char* expected_text) {
    ASSERT_EQ(token->kind, expected_kind);
    if (expected_text) {
        size_t expected_len = strlen(expected_text);
        ASSERT_EQ(token->length, expected_len);
        ASSERT_TRUE(memcmp(token->start, expected_text, expected_len) == 0);
    }
}

// ========== Basic Token Tests ==========

TEST(single_char_tokens) {
    TestContext ctx;
    setup_test_context(&ctx, "( ) { } [ ] , ; : ~");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_LPAREN);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_RPAREN);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_LBRACE);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_RBRACE);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_LBRACKET);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_RBRACKET);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_COMMA);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_SEMICOLON);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_COLON);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_TILDE);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_EOF);

    teardown_test_context(&ctx);
}

TEST(operators) {
    TestContext ctx;
    setup_test_context(&ctx, "+ - * / % & | ^ << >> && || !");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_PLUS);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_MINUS);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_STAR);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_SLASH);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_PERCENT);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_AMPERSAND);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_PIPE);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_CARET);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_LSHIFT);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_RSHIFT);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_AMP_AMP);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_PIPE_PIPE);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_BANG);

    teardown_test_context(&ctx);
}

TEST(comparison_operators) {
    TestContext ctx;
    setup_test_context(&ctx, "< > <= >= == !=");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_LT);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_GT);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_LT_EQ);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_GT_EQ);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_EQ_EQ);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_BANG_EQ);

    teardown_test_context(&ctx);
}

TEST(assignment_operators) {
    TestContext ctx;
    setup_test_context(&ctx, "= += -= *= /=");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_EQ);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_PLUS_EQ);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_MINUS_EQ);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_STAR_EQ);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_SLASH_EQ);

    teardown_test_context(&ctx);
}

TEST(arrow_and_dots) {
    TestContext ctx;
    setup_test_context(&ctx, "-> . ..");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_ARROW);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_DOT);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_DOT_DOT);

    teardown_test_context(&ctx);
}

// ========== Keyword Tests ==========

TEST(keywords_async_flow) {
    TestContext ctx;
    setup_test_context(&ctx, "async suspend resume");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_ASYNC);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_SUSPEND);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_RESUME);

    teardown_test_context(&ctx);
}

TEST(keywords_error_handling) {
    TestContext ctx;
    setup_test_context(&ctx, "try catch defer errdefer");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_TRY);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_CATCH);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_DEFER);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_ERRDEFER);

    teardown_test_context(&ctx);
}

TEST(keywords_declarations) {
    TestContext ctx;
    setup_test_context(&ctx, "pub import let var fn struct enum union");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_PUB);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_IMPORT);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_LET);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_VAR);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_FN);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_STRUCT);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_ENUM);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_UNION);

    teardown_test_context(&ctx);
}

TEST(keywords_control_flow) {
    TestContext ctx;
    setup_test_context(&ctx, "if else for while switch case default break continue return");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_IF);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_ELSE);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_FOR);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_WHILE);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_SWITCH);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_CASE);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_DEFAULT);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_BREAK);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_CONTINUE);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_RETURN);

    teardown_test_context(&ctx);
}

TEST(keywords_other) {
    TestContext ctx;
    setup_test_context(&ctx, "packed volatile in embed_file");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_PACKED);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_VOLATILE);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_IN);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_EMBED_FILE);

    teardown_test_context(&ctx);
}

TEST(bool_literals) {
    TestContext ctx;
    setup_test_context(&ctx, "true false");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_BOOL_LITERAL);
    ASSERT_EQ(t.literal.int_value, 1);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_BOOL_LITERAL);
    ASSERT_EQ(t.literal.int_value, 0);

    teardown_test_context(&ctx);
}

// ========== Identifier Tests ==========

TEST(identifiers) {
    TestContext ctx;
    setup_test_context(&ctx, "foo bar_baz _underscore CamelCase");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    assert_token(&t, TOKEN_IDENTIFIER, "foo");
    t = lexer_next_token(&ctx.lexer);
    assert_token(&t, TOKEN_IDENTIFIER, "bar_baz");
    t = lexer_next_token(&ctx.lexer);
    assert_token(&t, TOKEN_IDENTIFIER, "_underscore");
    t = lexer_next_token(&ctx.lexer);
    assert_token(&t, TOKEN_IDENTIFIER, "CamelCase");

    teardown_test_context(&ctx);
}

TEST(identifier_vs_keyword) {
    TestContext ctx;
    setup_test_context(&ctx, "fn function fnord");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_FN);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_IDENTIFIER);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_IDENTIFIER);

    teardown_test_context(&ctx);
}

// ========== Number Literal Tests ==========

TEST(decimal_numbers) {
    TestContext ctx;
    setup_test_context(&ctx, "0 42 1234567890");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 0);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 42);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 1234567890);

    teardown_test_context(&ctx);
}

TEST(numbers_with_underscores) {
    TestContext ctx;
    setup_test_context(&ctx, "1_000_000 12_34_56");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 1000000);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 123456);

    teardown_test_context(&ctx);
}

TEST(hexadecimal_numbers) {
    TestContext ctx;
    setup_test_context(&ctx, "0x0 0xFF 0xDEADBEEF 0x1A2B");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 0x0);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 0xFF);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 0xDEADBEEF);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 0x1A2B);

    teardown_test_context(&ctx);
}

TEST(hex_with_underscores) {
    TestContext ctx;
    setup_test_context(&ctx, "0xDE_AD_BE_EF");

    Token t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 0xDEADBEEF);

    teardown_test_context(&ctx);
}

TEST(octal_numbers) {
    TestContext ctx;
    setup_test_context(&ctx, "0o0 0o7 0o777 0o123");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 00);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 07);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 0777);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 0123);

    teardown_test_context(&ctx);
}

TEST(binary_numbers) {
    TestContext ctx;
    setup_test_context(&ctx, "0b0 0b1 0b1010 0b11111111");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 0);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 1);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 10);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 255);

    teardown_test_context(&ctx);
}

TEST(binary_with_underscores) {
    TestContext ctx;
    setup_test_context(&ctx, "0b1111_0000");

    Token t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 0b11110000);

    teardown_test_context(&ctx);
}

// ========== String Literal Tests ==========

TEST(simple_strings) {
    TestContext ctx;
    setup_test_context(&ctx, "\"hello\" \"world\"");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_STRING_LITERAL);
    ASSERT_STR_EQ(t.literal.str_value, "hello");
    ASSERT_EQ(t.literal.str_length, 5);

    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_STRING_LITERAL);
    ASSERT_STR_EQ(t.literal.str_value, "world");
    ASSERT_EQ(t.literal.str_length, 5);

    teardown_test_context(&ctx);
}

TEST(empty_string) {
    TestContext ctx;
    setup_test_context(&ctx, "\"\"");

    Token t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_STRING_LITERAL);
    ASSERT_EQ(t.literal.str_length, 0);

    teardown_test_context(&ctx);
}

TEST(strings_with_escapes) {
    TestContext ctx;
    setup_test_context(&ctx, "\"hello\\nworld\" \"tab\\there\" \"quote\\\"\"");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_STRING_LITERAL);
    ASSERT_STR_EQ(t.literal.str_value, "hello\nworld");

    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_STRING_LITERAL);
    ASSERT_STR_EQ(t.literal.str_value, "tab\there");

    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_STRING_LITERAL);
    ASSERT_STR_EQ(t.literal.str_value, "quote\"");

    teardown_test_context(&ctx);
}

TEST(string_with_backslash) {
    TestContext ctx;
    setup_test_context(&ctx, "\"path\\\\to\\\\file\"");

    Token t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_STRING_LITERAL);
    ASSERT_STR_EQ(t.literal.str_value, "path\\to\\file");

    teardown_test_context(&ctx);
}

// ========== Comment Tests ==========

TEST(line_comments) {
    TestContext ctx;
    setup_test_context(&ctx, "// This is a comment\n42");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_LINE_COMMENT);

    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 42);

    teardown_test_context(&ctx);
}

TEST(block_comments) {
    TestContext ctx;
    setup_test_context(&ctx, "/* comment */ 42");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_BLOCK_COMMENT);

    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);

    teardown_test_context(&ctx);
}

TEST(nested_block_comments) {
    TestContext ctx;
    setup_test_context(&ctx, "/* outer /* inner */ still comment */ 42");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_BLOCK_COMMENT);

    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_INT_LITERAL);
    ASSERT_EQ(t.literal.int_value, 42);

    teardown_test_context(&ctx);
}

// ========== Error Tests ==========

TEST(unterminated_string) {
    TestContext ctx;
    setup_test_context(&ctx, "\"unterminated");

    Token t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_ERROR);
    ASSERT_TRUE(ctx.lexer.has_error);
    ASSERT_TRUE(error_list_has_errors(&ctx.error_list));

    teardown_test_context(&ctx);
}

TEST(unterminated_block_comment) {
    TestContext ctx;
    setup_test_context(&ctx, "/* unterminated comment");

    Token t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_ERROR);
    ASSERT_TRUE(ctx.lexer.has_error);

    teardown_test_context(&ctx);
}

TEST(invalid_hex_number) {
    TestContext ctx;
    setup_test_context(&ctx, "0x");

    Token t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_ERROR);

    teardown_test_context(&ctx);
}

// ========== Line and Column Tracking ==========

TEST(line_and_column_tracking) {
    TestContext ctx;
    setup_test_context(&ctx, "foo\nbar\nbaz");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.line, 1);
    ASSERT_EQ(t.column, 1);

    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.line, 2);
    ASSERT_EQ(t.column, 1);

    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.line, 3);
    ASSERT_EQ(t.column, 1);

    teardown_test_context(&ctx);
}

// ========== Integration Tests ==========

TEST(simple_function) {
    TestContext ctx;
    setup_test_context(&ctx, "fn add(a: i32, b: i32) -> i32 { return a + b; }");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_FN);
    t = lexer_next_token(&ctx.lexer);
    assert_token(&t, TOKEN_IDENTIFIER, "add");
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_LPAREN);
    t = lexer_next_token(&ctx.lexer);
    assert_token(&t, TOKEN_IDENTIFIER, "a");
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_COLON);

    teardown_test_context(&ctx);
}

TEST(async_function) {
    TestContext ctx;
    setup_test_context(&ctx, "async fn process() { suspend; resume other; }");

    Token t;
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_ASYNC);
    t = lexer_next_token(&ctx.lexer);
    ASSERT_EQ(t.kind, TOKEN_FN);
    t = lexer_next_token(&ctx.lexer);
    assert_token(&t, TOKEN_IDENTIFIER, "process");
    // ... we can see it's working

    teardown_test_context(&ctx);
}

// ========== Main Test Runner ==========

int main(void) {
    printf("Running Lexer Tests\n");
    printf("===================\n\n");

    // Basic tokens
    RUN_TEST(single_char_tokens);
    RUN_TEST(operators);
    RUN_TEST(comparison_operators);
    RUN_TEST(assignment_operators);
    RUN_TEST(arrow_and_dots);

    // Keywords
    RUN_TEST(keywords_async_flow);
    RUN_TEST(keywords_error_handling);
    RUN_TEST(keywords_declarations);
    RUN_TEST(keywords_control_flow);
    RUN_TEST(keywords_other);
    RUN_TEST(bool_literals);

    // Identifiers
    RUN_TEST(identifiers);
    RUN_TEST(identifier_vs_keyword);

    // Numbers
    RUN_TEST(decimal_numbers);
    RUN_TEST(numbers_with_underscores);
    RUN_TEST(hexadecimal_numbers);
    RUN_TEST(hex_with_underscores);
    RUN_TEST(octal_numbers);
    RUN_TEST(binary_numbers);
    RUN_TEST(binary_with_underscores);

    // Strings
    RUN_TEST(simple_strings);
    RUN_TEST(empty_string);
    RUN_TEST(strings_with_escapes);
    RUN_TEST(string_with_backslash);

    // Comments
    RUN_TEST(line_comments);
    RUN_TEST(block_comments);
    RUN_TEST(nested_block_comments);

    // Errors
    RUN_TEST(unterminated_string);
    RUN_TEST(unterminated_block_comment);
    RUN_TEST(invalid_hex_number);

    // Tracking
    RUN_TEST(line_and_column_tracking);

    // Integration
    RUN_TEST(simple_function);
    RUN_TEST(async_function);

    printf("\n===================\n");
    printf("All tests passed!\n");
    return 0;
}
