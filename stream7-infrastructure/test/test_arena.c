#include "test_harness.h"
#include "arena.h"
#include <string.h>

// Test basic allocation
void test_arena_basic_alloc() {
    TEST_BEGIN("arena_basic_alloc");

    Arena arena;
    arena_init(&arena, 1024);

    // Allocate some memory
    int* ptr1 = (int*)arena_alloc(&arena, sizeof(int), sizeof(int));
    ASSERT_NOT_NULL(ptr1);
    *ptr1 = 42;

    int* ptr2 = (int*)arena_alloc(&arena, sizeof(int), sizeof(int));
    ASSERT_NOT_NULL(ptr2);
    *ptr2 = 100;

    // Check values
    ASSERT_EQ(*ptr1, 42);
    ASSERT_EQ(*ptr2, 100);

    arena_free(&arena);
    TEST_END();
}

// Test alignment
void test_arena_alignment() {
    TEST_BEGIN("arena_alignment");

    Arena arena;
    arena_init(&arena, 1024);

    // Allocate with different alignments
    char* ptr1 = (char*)arena_alloc(&arena, 1, 1);
    ASSERT_NOT_NULL(ptr1);
    ASSERT_EQ((size_t)ptr1 % 1, 0);

    int* ptr2 = (int*)arena_alloc(&arena, sizeof(int), sizeof(int));
    ASSERT_NOT_NULL(ptr2);
    ASSERT_EQ((size_t)ptr2 % sizeof(int), 0);

    double* ptr3 = (double*)arena_alloc(&arena, sizeof(double), sizeof(double));
    ASSERT_NOT_NULL(ptr3);
    ASSERT_EQ((size_t)ptr3 % sizeof(double), 0);

    arena_free(&arena);
    TEST_END();
}

// Test block growth
void test_arena_block_growth() {
    TEST_BEGIN("arena_block_growth");

    Arena arena;
    arena_init(&arena, 128);  // Small initial block

    // Allocate enough to force block growth
    void* ptrs[20];
    for (int i = 0; i < 20; i++) {
        ptrs[i] = arena_alloc(&arena, 32, sizeof(void*));
        ASSERT_NOT_NULL(ptrs[i]);
        memset(ptrs[i], i, 32);
    }

    // Verify all allocations are still valid
    for (int i = 0; i < 20; i++) {
        unsigned char* ptr = (unsigned char*)ptrs[i];
        for (int j = 0; j < 32; j++) {
            ASSERT_EQ(ptr[j], (unsigned char)i);
        }
    }

    arena_free(&arena);
    TEST_END();
}

// Test large allocation
void test_arena_large_alloc() {
    TEST_BEGIN("arena_large_alloc");

    Arena arena;
    arena_init(&arena, 1024);

    // Allocate larger than block size
    size_t large_size = 2048;
    void* large_ptr = arena_alloc(&arena, large_size, sizeof(void*));
    ASSERT_NOT_NULL(large_ptr);
    memset(large_ptr, 0xFF, large_size);

    // Verify
    unsigned char* bytes = (unsigned char*)large_ptr;
    for (size_t i = 0; i < large_size; i++) {
        ASSERT_EQ(bytes[i], 0xFF);
    }

    arena_free(&arena);
    TEST_END();
}

// Test multiple small allocations
void test_arena_many_small() {
    TEST_BEGIN("arena_many_small");

    Arena arena;
    arena_init(&arena, 4096);

    // Allocate many small objects
    int* values[1000];
    for (int i = 0; i < 1000; i++) {
        values[i] = (int*)arena_alloc(&arena, sizeof(int), sizeof(int));
        ASSERT_NOT_NULL(values[i]);
        *values[i] = i;
    }

    // Verify all values
    for (int i = 0; i < 1000; i++) {
        ASSERT_EQ(*values[i], i);
    }

    arena_free(&arena);
    TEST_END();
}

// Test default block size
void test_arena_default_size() {
    TEST_BEGIN("arena_default_size");

    Arena arena;
    arena_init(&arena, 0);  // Should use default

    void* ptr = arena_alloc(&arena, 16, sizeof(void*));
    ASSERT_NOT_NULL(ptr);

    arena_free(&arena);
    TEST_END();
}

int main() {
    TEST_INIT();

    test_arena_basic_alloc();
    test_arena_alignment();
    test_arena_block_growth();
    test_arena_large_alloc();
    test_arena_many_small();
    test_arena_default_size();

    TEST_SUMMARY();
    return TEST_EXIT();
}
