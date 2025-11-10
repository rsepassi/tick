CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -I./include -I../interfaces2
LDFLAGS =

# Source files
SRC_DIR = src
TEST_DIR = test
BUILD_DIR = build

# Source files
SRCS = $(SRC_DIR)/lexer.c \
       $(SRC_DIR)/arena.c \
       $(SRC_DIR)/string_pool.c \
       $(SRC_DIR)/error.c

# Test files
TEST_SRCS = $(TEST_DIR)/test_lexer.c

# Object files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/%.o)

# Test executable
TEST_EXEC = $(BUILD_DIR)/test_lexer

.PHONY: all clean test

all: test

# Build directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test files
$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link test executable
$(TEST_EXEC): $(OBJS) $(TEST_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

# Run tests
test: $(TEST_EXEC)
	./$(TEST_EXEC)

clean:
	rm -rf $(BUILD_DIR)
