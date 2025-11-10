#include "test_harness.h"
#include "string_pool.h"
#include "arena.h"
#include <string.h>

// Test basic string interning
void test_string_pool_basic() {
    TEST_BEGIN("string_pool_basic");

    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    // Intern a string
    const char* str1 = string_pool_intern(&pool, "hello", 5);
    ASSERT_NOT_NULL(str1);
    ASSERT_STR_EQ(str1, "hello");

    // Intern the same string - should return the same pointer
    const char* str2 = string_pool_intern(&pool, "hello", 5);
    ASSERT_NOT_NULL(str2);
    ASSERT_EQ((void*)str1, (void*)str2);

    arena_free(&arena);
    TEST_END();
}

// Test multiple different strings
void test_string_pool_multiple() {
    TEST_BEGIN("string_pool_multiple");

    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    // Intern different strings
    const char* str1 = string_pool_intern(&pool, "foo", 3);
    const char* str2 = string_pool_intern(&pool, "bar", 3);
    const char* str3 = string_pool_intern(&pool, "baz", 3);

    ASSERT_NOT_NULL(str1);
    ASSERT_NOT_NULL(str2);
    ASSERT_NOT_NULL(str3);

    ASSERT_STR_EQ(str1, "foo");
    ASSERT_STR_EQ(str2, "bar");
    ASSERT_STR_EQ(str3, "baz");

    // Check they are different pointers
    ASSERT_NE((void*)str1, (void*)str2);
    ASSERT_NE((void*)str2, (void*)str3);
    ASSERT_NE((void*)str1, (void*)str3);

    arena_free(&arena);
    TEST_END();
}

// Test lookup
void test_string_pool_lookup() {
    TEST_BEGIN("string_pool_lookup");

    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    // Lookup non-existent string
    const char* str1 = string_pool_lookup(&pool, "test", 4);
    ASSERT_NULL(str1);

    // Intern string
    const char* str2 = string_pool_intern(&pool, "test", 4);
    ASSERT_NOT_NULL(str2);

    // Lookup should now succeed
    const char* str3 = string_pool_lookup(&pool, "test", 4);
    ASSERT_NOT_NULL(str3);
    ASSERT_EQ((void*)str2, (void*)str3);

    arena_free(&arena);
    TEST_END();
}

// Test partial matches
void test_string_pool_partial() {
    TEST_BEGIN("string_pool_partial");

    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    // Intern "hello"
    const char* str1 = string_pool_intern(&pool, "hello", 5);
    ASSERT_NOT_NULL(str1);

    // Lookup "hel" should not match
    const char* str2 = string_pool_lookup(&pool, "hel", 3);
    ASSERT_NULL(str2);

    // Lookup "hello!" should not match
    const char* str3 = string_pool_lookup(&pool, "hello!", 6);
    ASSERT_NULL(str3);

    arena_free(&arena);
    TEST_END();
}

// Test many strings (stress test)
void test_string_pool_many() {
    TEST_BEGIN("string_pool_many");

    Arena arena;
    arena_init(&arena, 65536);

    StringPool pool;
    string_pool_init(&pool, &arena);

    // Intern many strings
    char buffer[32];
    const char* strings[500];

    for (int i = 0; i < 500; i++) {
        snprintf(buffer, sizeof(buffer), "string_%d", i);
        strings[i] = string_pool_intern(&pool, buffer, strlen(buffer));
        ASSERT_NOT_NULL(strings[i]);
    }

    // Verify all strings
    for (int i = 0; i < 500; i++) {
        snprintf(buffer, sizeof(buffer), "string_%d", i);
        ASSERT_STR_EQ(strings[i], buffer);
    }

    // Verify re-interning gives same pointers
    for (int i = 0; i < 500; i++) {
        snprintf(buffer, sizeof(buffer), "string_%d", i);
        const char* str = string_pool_intern(&pool, buffer, strlen(buffer));
        ASSERT_EQ((void*)str, (void*)strings[i]);
    }

    arena_free(&arena);
    TEST_END();
}

// Test empty string
void test_string_pool_empty() {
    TEST_BEGIN("string_pool_empty");

    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    // Intern empty string
    const char* str1 = string_pool_intern(&pool, "", 0);
    ASSERT_NOT_NULL(str1);
    ASSERT_STR_EQ(str1, "");

    // Should get same pointer
    const char* str2 = string_pool_intern(&pool, "", 0);
    ASSERT_EQ((void*)str1, (void*)str2);

    arena_free(&arena);
    TEST_END();
}

// Test long string
void test_string_pool_long() {
    TEST_BEGIN("string_pool_long");

    Arena arena;
    arena_init(&arena, 8192);

    StringPool pool;
    string_pool_init(&pool, &arena);

    // Create a long string
    char long_str[1000];
    memset(long_str, 'a', sizeof(long_str) - 1);
    long_str[sizeof(long_str) - 1] = '\0';

    const char* str1 = string_pool_intern(&pool, long_str, strlen(long_str));
    ASSERT_NOT_NULL(str1);
    ASSERT_STR_EQ(str1, long_str);

    // Should get same pointer
    const char* str2 = string_pool_intern(&pool, long_str, strlen(long_str));
    ASSERT_EQ((void*)str1, (void*)str2);

    arena_free(&arena);
    TEST_END();
}

int main() {
    TEST_INIT();

    test_string_pool_basic();
    test_string_pool_multiple();
    test_string_pool_lookup();
    test_string_pool_partial();
    test_string_pool_many();
    test_string_pool_empty();
    test_string_pool_long();

    TEST_SUMMARY();
    return TEST_EXIT();
}
