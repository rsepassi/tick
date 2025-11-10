#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { printf("  Test: %s ... ", name); tests_run++; } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 0; } while(0)

// Helper: Write a string to a file
static int write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    if (!f) return 0;
    fputs(content, f);
    fclose(f);
    return 1;
}

// Helper: Check if file exists
static int file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// Helper: Run compiler
static int run_compiler(const char* input, const char* output) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "/home/user/tick/compiler/tickc -o %s %s 2>&1", output, input);
    int ret = system(cmd);
    return WEXITSTATUS(ret) == 0;
}

// Helper: Run GCC on generated C code
static int compile_c_code(const char* cfile) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "gcc -std=c11 -ffreestanding -Wall -Wextra -c %s 2>&1", cfile);
    int ret = system(cmd);
    return WEXITSTATUS(ret) == 0;
}

// Test 1: Simple function compilation
static int test1(void) {
    TEST("Full compilation simple");

    const char* source =
        "let add = fn(x: i32, y: i32) -> i32 {\n"
        "    return x + y;\n"
        "};\n";

    // Write test file
    if (!write_file("/tmp/test_simple.tick", source)) {
        FAIL("Failed to write test file");
    }

    // Compile
    if (!run_compiler("/tmp/test_simple.tick", "/tmp/test_simple")) {
        FAIL("Compilation failed");
    }

    // Check output files exist
    if (!file_exists("/tmp/test_simple.h") || !file_exists("/tmp/test_simple.c")) {
        FAIL("Output files not generated");
    }

    // Cleanup
    unlink("/tmp/test_simple.tick");
    unlink("/tmp/test_simple.h");
    unlink("/tmp/test_simple.c");

    PASS();
    return 1;
}

// Test 2: Multiple functions
static int test2(void) {
    TEST("Full compilation multiple functions");

    const char* source =
        "let add = fn(x: i32, y: i32) -> i32 { return x + y; };\n"
        "let sub = fn(x: i32, y: i32) -> i32 { return x - y; };\n"
        "let mul = fn(x: i32, y: i32) -> i32 { return x * y; };\n";

    if (!write_file("/tmp/test_multi.tick", source)) {
        FAIL("Failed to write test file");
    }

    if (!run_compiler("/tmp/test_multi.tick", "/tmp/test_multi")) {
        FAIL("Compilation failed");
    }

    if (!file_exists("/tmp/test_multi.h") || !file_exists("/tmp/test_multi.c")) {
        FAIL("Output files not generated");
    }

    unlink("/tmp/test_multi.tick");
    unlink("/tmp/test_multi.h");
    unlink("/tmp/test_multi.c");

    PASS();
    return 1;
}

// Test 3: GCC compilation of generated C code
static int test3(void) {
    TEST("GCC compilation");

    const char* source =
        "let compute = fn(a: i32, b: i32, c: i32) -> i32 {\n"
        "    let temp1 = a + b;\n"
        "    let temp2 = temp1 * c;\n"
        "    return temp2;\n"
        "};\n";

    if (!write_file("/tmp/test_gcc.tick", source)) {
        FAIL("Failed to write test file");
    }

    if (!run_compiler("/tmp/test_gcc.tick", "/tmp/test_gcc")) {
        FAIL("Compilation failed");
    }

    // Try to compile with GCC
    if (!compile_c_code("/tmp/test_gcc.c")) {
        FAIL("GCC compilation of generated C code failed");
    }

    unlink("/tmp/test_gcc.tick");
    unlink("/tmp/test_gcc.h");
    unlink("/tmp/test_gcc.c");
    unlink("/tmp/test_gcc.o");

    PASS();
    return 1;
}

// Test 4: Different types
static int test4(void) {
    TEST("Different types compilation");

    const char* source =
        "let use_i8 = fn(x: i8) -> i8 { return x; };\n"
        "let use_u32 = fn(x: u32) -> u32 { return x; };\n"
        "let use_i64 = fn(x: i64) -> i64 { return x; };\n"
        "let use_bool = fn(x: bool) -> bool { return x; };\n";

    if (!write_file("/tmp/test_types.tick", source)) {
        FAIL("Failed to write test file");
    }

    if (!run_compiler("/tmp/test_types.tick", "/tmp/test_types")) {
        FAIL("Compilation failed");
    }

    if (!file_exists("/tmp/test_types.h") || !file_exists("/tmp/test_types.c")) {
        FAIL("Output files not generated");
    }

    unlink("/tmp/test_types.tick");
    unlink("/tmp/test_types.h");
    unlink("/tmp/test_types.c");

    PASS();
    return 1;
}

// Test 5: Error handling (invalid syntax)
static int test5(void) {
    TEST("Error handling");

    const char* source =
        "let broken = fn(x: i32 -> i32 {;\n";  // Intentionally broken

    if (!write_file("/tmp/test_error.tick", source)) {
        FAIL("Failed to write test file");
    }

    // This should fail
    if (run_compiler("/tmp/test_error.tick", "/tmp/test_error")) {
        FAIL("Compiler should have failed on invalid syntax");
    }

    unlink("/tmp/test_error.tick");

    PASS();
    return 1;
}

int main(void) {
    printf("\n========================================\n");
    printf("Full Compilation Pipeline Integration Tests\n");
    printf("========================================\n\n");
    
    test1(); test2(); test3(); test4(); test5();
    
    printf("\n========================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
