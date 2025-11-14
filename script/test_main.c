// C wrapper for tests to avoid main signature issues
#include <stdint.h>

// Forward declaration of the test function from Tick
int32_t test(void);

int main(int argc, char** argv) {
    (void)argc;  // Unused
    (void)argv;  // Unused
    return test();
}