# Makefile for Stream 7: Core Infrastructure

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -I../interfaces2 -I./include
LDFLAGS =

# Source files
SRC_DIR = src
TEST_DIR = test

SRCS = $(SRC_DIR)/arena.c $(SRC_DIR)/string_pool.c $(SRC_DIR)/error.c
TEST_HARNESS = $(TEST_DIR)/test_harness.c
MOCK_PLATFORM = $(TEST_DIR)/mock_platform.c

# Test executables
TESTS = test_arena test_string_pool test_error

.PHONY: all clean test

all: $(TESTS)

test_arena: $(TEST_DIR)/test_arena.c $(SRCS) $(TEST_HARNESS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_string_pool: $(TEST_DIR)/test_string_pool.c $(SRCS) $(TEST_HARNESS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_error: $(TEST_DIR)/test_error.c $(SRCS) $(TEST_HARNESS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: $(TESTS)
	@echo "=========================================="
	@echo "Running Arena Tests"
	@echo "=========================================="
	./test_arena
	@echo ""
	@echo "=========================================="
	@echo "Running String Pool Tests"
	@echo "=========================================="
	./test_string_pool
	@echo ""
	@echo "=========================================="
	@echo "Running Error Tests"
	@echo "=========================================="
	./test_error
	@echo ""
	@echo "=========================================="
	@echo "All Tests Complete!"
	@echo "=========================================="

clean:
	rm -f $(TESTS) *.o
