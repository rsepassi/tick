#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

// Test Harness for Compiler Infrastructure
// Purpose: Simple testing utilities for all streams

// Color codes for test output
#define TEST_COLOR_RESET   "\033[0m"
#define TEST_COLOR_GREEN   "\033[32m"
#define TEST_COLOR_RED     "\033[31m"
#define TEST_COLOR_YELLOW  "\033[33m"
#define TEST_COLOR_BOLD    "\033[1m"

// Test context
typedef struct TestContext {
    int tests_run;
    int tests_passed;
    int tests_failed;
    const char* current_test;
} TestContext;

// Global test context
extern TestContext g_test_ctx;

// Test macros
#define TEST_INIT() \
    do { \
        g_test_ctx.tests_run = 0; \
        g_test_ctx.tests_passed = 0; \
        g_test_ctx.tests_failed = 0; \
        g_test_ctx.current_test = NULL; \
    } while (0)

#define TEST_BEGIN(name) \
    do { \
        g_test_ctx.current_test = name; \
        printf("%s[TEST]%s %s\n", TEST_COLOR_BOLD, TEST_COLOR_RESET, name); \
        g_test_ctx.tests_run++; \
    } while (0)

#define TEST_END() \
    do { \
        g_test_ctx.tests_passed++; \
        printf("%s[PASS]%s %s\n\n", \
               TEST_COLOR_GREEN, TEST_COLOR_RESET, g_test_ctx.current_test); \
        g_test_ctx.current_test = NULL; \
    } while (0)

#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("%s[FAIL]%s %s:%d: Assertion failed: %s\n", \
                   TEST_COLOR_RED, TEST_COLOR_RESET, \
                   __FILE__, __LINE__, #condition); \
            g_test_ctx.tests_failed++; \
            g_test_ctx.tests_passed--; \
            return; \
        } \
    } while (0)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            printf("%s[FAIL]%s %s:%d: Expected %s == %s\n", \
                   TEST_COLOR_RED, TEST_COLOR_RESET, \
                   __FILE__, __LINE__, #a, #b); \
            g_test_ctx.tests_failed++; \
            g_test_ctx.tests_passed--; \
            return; \
        } \
    } while (0)

#define ASSERT_NE(a, b) \
    do { \
        if ((a) == (b)) { \
            printf("%s[FAIL]%s %s:%d: Expected %s != %s\n", \
                   TEST_COLOR_RED, TEST_COLOR_RESET, \
                   __FILE__, __LINE__, #a, #b); \
            g_test_ctx.tests_failed++; \
            g_test_ctx.tests_passed--; \
            return; \
        } \
    } while (0)

#define ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            printf("%s[FAIL]%s %s:%d: Expected %s to be NULL\n", \
                   TEST_COLOR_RED, TEST_COLOR_RESET, \
                   __FILE__, __LINE__, #ptr); \
            g_test_ctx.tests_failed++; \
            g_test_ctx.tests_passed--; \
            return; \
        } \
    } while (0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            printf("%s[FAIL]%s %s:%d: Expected %s to not be NULL\n", \
                   TEST_COLOR_RED, TEST_COLOR_RESET, \
                   __FILE__, __LINE__, #ptr); \
            g_test_ctx.tests_failed++; \
            g_test_ctx.tests_passed--; \
            return; \
        } \
    } while (0)

#define ASSERT_STR_EQ(a, b) \
    do { \
        if (strcmp((a), (b)) != 0) { \
            printf("%s[FAIL]%s %s:%d: Expected \"%s\" == \"%s\"\n", \
                   TEST_COLOR_RED, TEST_COLOR_RESET, \
                   __FILE__, __LINE__, (a), (b)); \
            g_test_ctx.tests_failed++; \
            g_test_ctx.tests_passed--; \
            return; \
        } \
    } while (0)

#define TEST_SUMMARY() \
    do { \
        printf("========================================\n"); \
        printf("Tests run: %d\n", g_test_ctx.tests_run); \
        printf("%sPassed: %d%s\n", \
               TEST_COLOR_GREEN, g_test_ctx.tests_passed, TEST_COLOR_RESET); \
        if (g_test_ctx.tests_failed > 0) { \
            printf("%sFailed: %d%s\n", \
                   TEST_COLOR_RED, g_test_ctx.tests_failed, TEST_COLOR_RESET); \
        } else { \
            printf("Failed: 0\n"); \
        } \
        printf("========================================\n"); \
    } while (0)

#define TEST_EXIT() \
    (g_test_ctx.tests_failed > 0 ? 1 : 0)

#endif // TEST_HARNESS_H
