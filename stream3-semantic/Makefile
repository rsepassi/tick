# Makefile for Stream 3: Semantic Analysis

# Compiler and flags
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -I include
DEBUG_FLAGS = -g -O0
RELEASE_FLAGS = -O2 -DNDEBUG

# Directories
SRC_DIR = src
TEST_DIR = test
INCLUDE_DIR = include
BUILD_DIR = build

# Source files
RESOLVER_SRC = $(SRC_DIR)/resolver.c
TYPECK_SRC = $(SRC_DIR)/typeck.c
ALL_SRC = $(RESOLVER_SRC) $(TYPECK_SRC)

# Test files
TEST_RESOLVER_SRC = $(TEST_DIR)/test_resolver.c
TEST_TYPECK_SRC = $(TEST_DIR)/test_typeck.c

# Test executables
TEST_RESOLVER = test_resolver
TEST_TYPECK = test_typeck

# Default target
.PHONY: all
all: tests

# Build all tests
.PHONY: tests
tests: $(TEST_RESOLVER) $(TEST_TYPECK)

# Resolver test executable
$(TEST_RESOLVER): $(TEST_RESOLVER_SRC) $(RESOLVER_SRC)
	@echo "Building resolver tests..."
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o $@ $^

# Type checker test executable (depends on both resolver and typeck)
$(TEST_TYPECK): $(TEST_TYPECK_SRC) $(RESOLVER_SRC) $(TYPECK_SRC)
	@echo "Building type checker tests..."
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o $@ $^

# Run all tests
.PHONY: run-tests
run-tests: tests
	@echo "==================================="
	@echo "Running Resolver Tests"
	@echo "==================================="
	./$(TEST_RESOLVER)
	@echo ""
	@echo "==================================="
	@echo "Running Type Checker Tests"
	@echo "==================================="
	./$(TEST_TYPECK)
	@echo ""
	@echo "==================================="
	@echo "All tests completed!"
	@echo "==================================="

# Run resolver tests only
.PHONY: test-resolver
test-resolver: $(TEST_RESOLVER)
	./$(TEST_RESOLVER)

# Run type checker tests only
.PHONY: test-typeck
test-typeck: $(TEST_TYPECK)
	./$(TEST_TYPECK)

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(TEST_RESOLVER) $(TEST_TYPECK)
	rm -rf $(BUILD_DIR)
	@echo "Clean complete."

# Help target
.PHONY: help
help:
	@echo "Stream 3: Semantic Analysis - Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  all          - Build all tests (default)"
	@echo "  tests        - Build test executables"
	@echo "  run-tests    - Build and run all tests"
	@echo "  test-resolver - Run resolver tests only"
	@echo "  test-typeck  - Run type checker tests only"
	@echo "  clean        - Remove build artifacts"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Example usage:"
	@echo "  make run-tests   # Build and run all tests"
	@echo "  make clean       # Clean up"

# Phony targets
.PHONY: all tests run-tests test-resolver test-typeck clean help
