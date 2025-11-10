#include "test_harness.h"
#include "../error.h"
#include "../arena.h"
#include <string.h>

// Test basic error addition
void test_error_basic() {
    TEST_BEGIN("error_basic");

    Arena arena;
    arena_init(&arena, 4096);

    ErrorList list;
    error_list_init(&list, &arena);

    ASSERT_EQ(error_list_has_errors(&list), false);
    ASSERT_EQ(list.count, 0);

    // Add an error
    SourceLocation loc = {1, 5, "test.tick"};
    error_list_add(&list, ERROR_SYNTAX, loc, "Expected semicolon");

    ASSERT_EQ(error_list_has_errors(&list), true);
    ASSERT_EQ(list.count, 1);

    arena_free(&arena);
    TEST_END();
}

// Test multiple errors
void test_error_multiple() {
    TEST_BEGIN("error_multiple");

    Arena arena;
    arena_init(&arena, 4096);

    ErrorList list;
    error_list_init(&list, &arena);

    // Add multiple errors
    SourceLocation loc1 = {1, 5, "test.tick"};
    error_list_add(&list, ERROR_SYNTAX, loc1, "Expected semicolon");

    SourceLocation loc2 = {3, 10, "test.tick"};
    error_list_add(&list, ERROR_TYPE, loc2, "Type mismatch");

    SourceLocation loc3 = {5, 1, "test.tick"};
    error_list_add(&list, ERROR_LEXICAL, loc3, "Invalid character");

    ASSERT_EQ(list.count, 3);
    ASSERT_EQ(list.errors[0].kind, ERROR_SYNTAX);
    ASSERT_EQ(list.errors[1].kind, ERROR_TYPE);
    ASSERT_EQ(list.errors[2].kind, ERROR_LEXICAL);

    arena_free(&arena);
    TEST_END();
}

// Test formatted error messages
void test_error_formatted() {
    TEST_BEGIN("error_formatted");

    Arena arena;
    arena_init(&arena, 4096);

    ErrorList list;
    error_list_init(&list, &arena);

    // Add error with format string
    SourceLocation loc = {1, 5, "test.tick"};
    error_list_add(&list, ERROR_TYPE, loc, "Expected type '%s' but got '%s'", "int", "string");

    ASSERT_EQ(list.count, 1);
    ASSERT_NOT_NULL(list.errors[0].message);

    // Check message contains expected text
    ASSERT(strstr(list.errors[0].message, "int") != NULL);
    ASSERT(strstr(list.errors[0].message, "string") != NULL);

    arena_free(&arena);
    TEST_END();
}

// Test error kinds
void test_error_kinds() {
    TEST_BEGIN("error_kinds");

    Arena arena;
    arena_init(&arena, 4096);

    ErrorList list;
    error_list_init(&list, &arena);

    SourceLocation loc = {1, 1, "test.tick"};

    // Add different kinds of errors
    error_list_add(&list, ERROR_LEXICAL, loc, "Lexical error");
    error_list_add(&list, ERROR_SYNTAX, loc, "Syntax error");
    error_list_add(&list, ERROR_TYPE, loc, "Type error");
    error_list_add(&list, ERROR_RESOLUTION, loc, "Resolution error");
    error_list_add(&list, ERROR_COROUTINE, loc, "Coroutine error");

    ASSERT_EQ(list.count, 5);
    ASSERT_EQ(list.errors[0].kind, ERROR_LEXICAL);
    ASSERT_EQ(list.errors[1].kind, ERROR_SYNTAX);
    ASSERT_EQ(list.errors[2].kind, ERROR_TYPE);
    ASSERT_EQ(list.errors[3].kind, ERROR_RESOLUTION);
    ASSERT_EQ(list.errors[4].kind, ERROR_COROUTINE);

    arena_free(&arena);
    TEST_END();
}

// Test source location
void test_error_location() {
    TEST_BEGIN("error_location");

    Arena arena;
    arena_init(&arena, 4096);

    ErrorList list;
    error_list_init(&list, &arena);

    SourceLocation loc = {42, 17, "myfile.tick"};
    error_list_add(&list, ERROR_SYNTAX, loc, "Test error");

    ASSERT_EQ(list.count, 1);
    ASSERT_EQ(list.errors[0].loc.line, 42);
    ASSERT_EQ(list.errors[0].loc.column, 17);
    ASSERT_STR_EQ(list.errors[0].loc.filename, "myfile.tick");

    arena_free(&arena);
    TEST_END();
}

// Test many errors (array growth)
void test_error_many() {
    TEST_BEGIN("error_many");

    Arena arena;
    arena_init(&arena, 8192);

    ErrorList list;
    error_list_init(&list, &arena);

    // Add many errors to trigger growth
    SourceLocation loc = {1, 1, "test.tick"};
    for (int i = 0; i < 100; i++) {
        error_list_add(&list, ERROR_SYNTAX, loc, "Error %d", i);
    }

    ASSERT_EQ(list.count, 100);

    // Verify first and last
    ASSERT(strstr(list.errors[0].message, "Error 0") != NULL);
    ASSERT(strstr(list.errors[99].message, "Error 99") != NULL);

    arena_free(&arena);
    TEST_END();
}

// Test empty error list printing (should not crash)
void test_error_print_empty() {
    TEST_BEGIN("error_print_empty");

    Arena arena;
    arena_init(&arena, 4096);

    ErrorList list;
    error_list_init(&list, &arena);

    // This should not crash
    error_list_print(&list, stderr);

    arena_free(&arena);
    TEST_END();
}

int main() {
    TEST_INIT();

    test_error_basic();
    test_error_multiple();
    test_error_formatted();
    test_error_kinds();
    test_error_location();
    test_error_many();
    test_error_print_empty();

    TEST_SUMMARY();
    return TEST_EXIT();
}
