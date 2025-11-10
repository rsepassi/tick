# Top-level Makefile for Tick Compiler
# Builds the compiler to build/tick

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -g -O0
LEMON = $(BUILD_DIR)/lemon
LEMON_SRC = vendor/lemon/lemon.c
LEMON_TEMPLATE = vendor/lemon/lempar.c

# Include paths
INCLUDES = -I./interfaces2 \
           -I./stream6-codegen/include \
           -I./stream2-parser \
           -I./stream2-parser/build

# Build directory
BUILD_DIR = build

# Source files
MAIN_SRC = compiler/main.c

LEXER_SRCS = stream1-lexer/src/lexer.c \
             stream1-lexer/src/arena.c \
             stream1-lexer/src/error.c \
             stream1-lexer/src/string_pool.c

PARSER_SRCS = stream2-parser/src/parser.c

SEMANTIC_SRCS = stream3-semantic/src/resolver.c \
                stream3-semantic/src/typeck.c

LOWERING_SRCS = stream5-lowering/src/lower.c

CODEGEN_SRCS = stream6-codegen/src/codegen.c

# Generated parser source
GRAMMAR_Y = stream2-parser/grammar.y
GENERATED_GRAMMAR = $(BUILD_DIR)/grammar.c

# All source files
ALL_SRCS = $(MAIN_SRC) \
           $(LEXER_SRCS) \
           $(PARSER_SRCS) \
           $(GENERATED_GRAMMAR) \
           $(SEMANTIC_SRCS) \
           $(LOWERING_SRCS) \
           $(CODEGEN_SRCS)

# Object files (all in build directory)
MAIN_OBJ = $(BUILD_DIR)/main.o

LEXER_OBJS = $(BUILD_DIR)/lexer.o \
             $(BUILD_DIR)/arena.o \
             $(BUILD_DIR)/error.o \
             $(BUILD_DIR)/string_pool.o

PARSER_OBJS = $(BUILD_DIR)/parser.o \
              $(BUILD_DIR)/grammar.o

SEMANTIC_OBJS = $(BUILD_DIR)/resolver.o \
                $(BUILD_DIR)/typeck.o

LOWERING_OBJS = $(BUILD_DIR)/lower.o

CODEGEN_OBJS = $(BUILD_DIR)/codegen.o

ALL_OBJS = $(MAIN_OBJ) \
           $(LEXER_OBJS) \
           $(PARSER_OBJS) \
           $(SEMANTIC_OBJS) \
           $(LOWERING_OBJS) \
           $(CODEGEN_OBJS)

# Target executable
TARGET = $(BUILD_DIR)/tick

.PHONY: all tick clean

all: tick

tick: $(TARGET)

# Build the compiler
$(TARGET): $(BUILD_DIR) $(ALL_OBJS)
	$(CC) $(CFLAGS) $(ALL_OBJS) -o $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build lemon parser generator from source
$(LEMON): $(LEMON_SRC) $(BUILD_DIR)
	$(CC) -std=c99 -O2 $(LEMON_SRC) -o $(LEMON)

# Generate parser from Lemon grammar
$(GENERATED_GRAMMAR): $(GRAMMAR_Y) $(LEMON)
	$(LEMON) -T$(LEMON_TEMPLATE) -d$(BUILD_DIR) $(GRAMMAR_Y) || true

# Compile main
$(BUILD_DIR)/main.o: $(MAIN_SRC) $(GENERATED_GRAMMAR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $(MAIN_SRC) -o $@

# Compile lexer sources
$(BUILD_DIR)/lexer.o: stream1-lexer/src/lexer.c $(GENERATED_GRAMMAR)
	$(CC) $(CFLAGS) $(INCLUDES) -c stream1-lexer/src/lexer.c -o $@

$(BUILD_DIR)/arena.o: stream1-lexer/src/arena.c
	$(CC) $(CFLAGS) $(INCLUDES) -c stream1-lexer/src/arena.c -o $@

$(BUILD_DIR)/error.o: stream1-lexer/src/error.c
	$(CC) $(CFLAGS) $(INCLUDES) -c stream1-lexer/src/error.c -o $@

$(BUILD_DIR)/string_pool.o: stream1-lexer/src/string_pool.c
	$(CC) $(CFLAGS) $(INCLUDES) -c stream1-lexer/src/string_pool.c -o $@

# Compile parser sources
$(BUILD_DIR)/parser.o: stream2-parser/src/parser.c $(GENERATED_GRAMMAR)
	$(CC) $(CFLAGS) $(INCLUDES) -c stream2-parser/src/parser.c -o $@

$(BUILD_DIR)/grammar.o: $(GENERATED_GRAMMAR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $(GENERATED_GRAMMAR) -o $@

# Compile semantic analysis sources
$(BUILD_DIR)/resolver.o: stream3-semantic/src/resolver.c $(GENERATED_GRAMMAR)
	$(CC) $(CFLAGS) $(INCLUDES) -c stream3-semantic/src/resolver.c -o $@

$(BUILD_DIR)/typeck.o: stream3-semantic/src/typeck.c $(GENERATED_GRAMMAR)
	$(CC) $(CFLAGS) $(INCLUDES) -c stream3-semantic/src/typeck.c -o $@

# Compile lowering sources
$(BUILD_DIR)/lower.o: stream5-lowering/src/lower.c $(GENERATED_GRAMMAR)
	$(CC) $(CFLAGS) $(INCLUDES) -c stream5-lowering/src/lower.c -o $@

# Compile codegen sources
$(BUILD_DIR)/codegen.o: stream6-codegen/src/codegen.c $(GENERATED_GRAMMAR)
	$(CC) $(CFLAGS) $(INCLUDES) -c stream6-codegen/src/codegen.c -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
