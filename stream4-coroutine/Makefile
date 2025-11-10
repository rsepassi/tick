CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -I../interfaces2
LDFLAGS =

SRC_DIR = src
TEST_DIR = test
BUILD_DIR = build

# Stream 7 infrastructure source files
INFRA_DIR = ../stream7-infrastructure/src
INFRA_SRCS = $(INFRA_DIR)/arena.c \
             $(INFRA_DIR)/error.c \
             $(INFRA_DIR)/string_pool.c

# Source files
SOURCES = $(SRC_DIR)/coro_analyze.c \
          $(SRC_DIR)/stubs.c
TEST_SOURCES = $(TEST_DIR)/test_coro_analyze.c

# Targets
.PHONY: all clean test

all: test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build and run tests
test: $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SOURCES) $(TEST_SOURCES) $(INFRA_SRCS) -o $(BUILD_DIR)/test_coro_analyze
	@echo "Running tests..."
	@$(BUILD_DIR)/test_coro_analyze

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
