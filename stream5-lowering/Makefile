CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -I./include -I../interfaces2

SRC_DIR = src
TEST_DIR = test
BUILD_DIR = build
INFRA_DIR = ../stream7-infrastructure/src

# Source files
SOURCES = $(SRC_DIR)/lower.c
TEST_SOURCES = $(TEST_DIR)/test_lower.c

# Infrastructure dependencies
INFRA_SOURCES = $(INFRA_DIR)/arena.c \
                $(INFRA_DIR)/error.c \
                $(INFRA_DIR)/string_pool.c \
                ../stream3-semantic/src/typeck.c

# Targets
.PHONY: all clean test

all: test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build and run tests
test: $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SOURCES) $(INFRA_SOURCES) $(TEST_SOURCES) -o $(BUILD_DIR)/test_lower
	@echo "Running tests..."
	@$(BUILD_DIR)/test_lower

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
