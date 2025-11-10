# Makefile for stream2-parser
# Builds the parser module using Lemon parser generator

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I. -I../interfaces2
LEMON = lemon

# Directories
SRC_DIR = src
TEST_DIR = test
BUILD_DIR = build
INFRA_DIR = ../stream7-infrastructure/src
LEXER_DIR = ../stream1-lexer/src

# Source files
PARSER_SRC = $(SRC_DIR)/parser.c
GRAMMAR_SRC = grammar.y
GENERATED_PARSER = $(BUILD_DIR)/grammar.c

# Infrastructure source files
INFRA_SRCS = $(INFRA_DIR)/arena.c $(INFRA_DIR)/error.c $(INFRA_DIR)/string_pool.c
LEXER_SRC = $(LEXER_DIR)/lexer.c

# Object files
PARSER_OBJ = $(BUILD_DIR)/parser.o
GENERATED_OBJ = $(BUILD_DIR)/grammar.o
INFRA_OBJS = $(BUILD_DIR)/arena.o $(BUILD_DIR)/error.o $(BUILD_DIR)/string_pool.o
LEXER_OBJ = $(BUILD_DIR)/lexer.o

# Test executable
TEST_EXE = $(BUILD_DIR)/test_parser

.PHONY: all clean test

all: $(BUILD_DIR) $(GENERATED_PARSER)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Generate parser from Lemon grammar
$(GENERATED_PARSER): $(GRAMMAR_SRC)
	$(LEMON) -d$(BUILD_DIR) $(GRAMMAR_SRC)
	mv $(BUILD_DIR)/grammar.c $(GENERATED_PARSER)
	mv $(BUILD_DIR)/grammar.h $(BUILD_DIR)/grammar.h

# Compile parser integration
$(PARSER_OBJ): $(PARSER_SRC) $(GENERATED_PARSER)
	$(CC) $(CFLAGS) -c $(PARSER_SRC) -o $(PARSER_OBJ)

# Compile generated parser
$(GENERATED_OBJ): $(GENERATED_PARSER)
	$(CC) $(CFLAGS) -c $(GENERATED_PARSER) -o $(GENERATED_OBJ)

# Compile infrastructure files
$(BUILD_DIR)/arena.o: $(INFRA_DIR)/arena.c
	$(CC) $(CFLAGS) -c $(INFRA_DIR)/arena.c -o $(BUILD_DIR)/arena.o

$(BUILD_DIR)/error.o: $(INFRA_DIR)/error.c
	$(CC) $(CFLAGS) -c $(INFRA_DIR)/error.c -o $(BUILD_DIR)/error.o

$(BUILD_DIR)/string_pool.o: $(INFRA_DIR)/string_pool.c
	$(CC) $(CFLAGS) -c $(INFRA_DIR)/string_pool.c -o $(BUILD_DIR)/string_pool.o

# Compile lexer
$(BUILD_DIR)/lexer.o: $(LEXER_SRC)
	$(CC) $(CFLAGS) -c $(LEXER_SRC) -o $(BUILD_DIR)/lexer.o

# Build test executable
$(TEST_EXE): $(TEST_DIR)/test_parser.c $(PARSER_OBJ) $(GENERATED_OBJ) $(INFRA_OBJS) $(LEXER_OBJ)
	$(CC) $(CFLAGS) $(TEST_DIR)/test_parser.c $(PARSER_OBJ) $(GENERATED_OBJ) $(INFRA_OBJS) $(LEXER_OBJ) -o $(TEST_EXE)

# Run tests
test: $(TEST_EXE)
	$(TEST_EXE)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Help
help:
	@echo "Parser Module Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all     - Build parser (generates grammar.c from grammar.y)"
	@echo "  test    - Build and run unit tests"
	@echo "  clean   - Remove build artifacts"
	@echo "  help    - Show this help message"
	@echo ""
	@echo "Requirements:"
	@echo "  - Lemon parser generator (lemon)"
	@echo "  - GCC or compatible C compiler"
