CC = gcc
CFLAGS = -Wall -Werror -Wextra -std=c11 -I./include -I../interfaces2
FREESTANDING_FLAGS = -ffreestanding

SRC_DIR = src
TEST_DIR = test
BUILD_DIR = build

# Source files
SOURCES = $(SRC_DIR)/codegen.c
TEST_SOURCES = $(TEST_DIR)/test_codegen.c

# Targets
.PHONY: all clean test

all: test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build and run tests
test: $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SOURCES) $(TEST_SOURCES) -o $(BUILD_DIR)/test_codegen
	@echo "Running tests..."
	@$(BUILD_DIR)/test_codegen

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -f /tmp/test.h /tmp/test.c /tmp/test.o
	rm -f /tmp/lang_runtime.h
	rm -f /tmp/compile_test.h /tmp/compile_test.c /tmp/compile_test.o
	rm -f /tmp/assign_test.h /tmp/assign_test.c

# Verify generated code compiles with freestanding flags
verify-freestanding: test
	@echo "Verifying freestanding compilation..."
	@echo "This is done automatically by the test suite"
