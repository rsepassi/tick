# Makefile for Integration Tests
# Tests the full compiler pipeline with real implementations from all streams

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -g
LDFLAGS =

# Root directory
ROOT = ..

# Include paths for all interfaces
INCLUDES = -I$(ROOT)/interfaces2 \
           -I$(ROOT)/stream6-codegen/include \
           -I$(ROOT)/stream4-coroutine

# Source files from all streams
LEXER_SRCS = $(ROOT)/stream1-lexer/src/lexer.c \
             $(ROOT)/stream1-lexer/src/arena.c \
             $(ROOT)/stream1-lexer/src/string_pool.c \
             $(ROOT)/stream1-lexer/src/error.c

PARSER_SRCS = $(ROOT)/stream2-parser/src/parser.c

SEMANTIC_SRCS = $(ROOT)/stream3-semantic/src/typeck.c \
                $(ROOT)/stream3-semantic/src/resolver.c

COROUTINE_SRCS = $(ROOT)/stream4-coroutine/src/coro_analyze.c

LOWERING_SRCS = $(ROOT)/stream5-lowering/src/lower.c

CODEGEN_SRCS = $(ROOT)/stream6-codegen/src/codegen.c

INFRA_SRCS = $(ROOT)/stream7-infrastructure/src/arena.c \
             $(ROOT)/stream7-infrastructure/src/string_pool.c \
             $(ROOT)/stream7-infrastructure/src/error.c

# All implementation sources
ALL_IMPL_SRCS = $(LEXER_SRCS) $(PARSER_SRCS) $(SEMANTIC_SRCS) \
                $(COROUTINE_SRCS) $(LOWERING_SRCS) $(CODEGEN_SRCS)

# Test source files
TEST_SRCS = test_lexer_parser.c \
            test_semantic.c \
            test_coroutine.c \
            test_lowering.c \
            test_codegen.c \
            test_full_compile.c \
            test_pipeline.c

# Test executables
TESTS = test_lexer_parser \
        test_semantic \
        test_coroutine \
        test_lowering \
        test_codegen \
        test_full_compile \
        test_pipeline

# Build directory
BUILD_DIR = build

.PHONY: all clean test help

all: $(TESTS)

help:
	@echo "Integration Test Suite for Tick Compiler"
	@echo ""
	@echo "Targets:"
	@echo "  all                 - Build all integration tests"
	@echo "  test                - Run all integration tests"
	@echo "  test_lexer_parser   - Test lexer + parser integration"
	@echo "  test_semantic       - Test parser + semantic analysis"
	@echo "  test_coroutine      - Test semantic + coroutine analysis"
	@echo "  test_lowering       - Test coroutine + IR lowering"
	@echo "  test_codegen        - Test IR + code generation"
	@echo "  test_full_compile   - Test complete pipeline"
	@echo "  test_pipeline       - Test main end-to-end scenarios"
	@echo "  clean               - Remove all build artifacts"

# Build test executables
test_lexer_parser: test_lexer_parser.c $(LEXER_SRCS) $(PARSER_SRCS) $(INFRA_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

test_semantic: test_semantic.c $(LEXER_SRCS) $(PARSER_SRCS) $(SEMANTIC_SRCS) $(INFRA_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

test_coroutine: test_coroutine.c $(LEXER_SRCS) $(PARSER_SRCS) $(SEMANTIC_SRCS) $(COROUTINE_SRCS) $(INFRA_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

test_lowering: test_lowering.c $(LEXER_SRCS) $(PARSER_SRCS) $(SEMANTIC_SRCS) $(COROUTINE_SRCS) $(LOWERING_SRCS) $(INFRA_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

test_codegen: test_codegen.c $(ALL_IMPL_SRCS) $(INFRA_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

test_full_compile: test_full_compile.c $(ALL_IMPL_SRCS) $(INFRA_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

test_pipeline: test_pipeline.c $(ALL_IMPL_SRCS) $(INFRA_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

# Run all tests
test: $(TESTS)
	@echo "=========================================="
	@echo "Integration Test Suite"
	@echo "=========================================="
	@echo ""
	@echo "Running Lexer + Parser Integration..."
	@./test_lexer_parser || exit 1
	@echo ""
	@echo "Running Parser + Semantic Integration..."
	@./test_semantic || exit 1
	@echo ""
	@echo "Running Semantic + Coroutine Integration..."
	@./test_coroutine || exit 1
	@echo ""
	@echo "Running Coroutine + Lowering Integration..."
	@./test_lowering || exit 1
	@echo ""
	@echo "Running Lowering + Codegen Integration..."
	@./test_codegen || exit 1
	@echo ""
	@echo "Running Full Compilation Integration..."
	@./test_full_compile || exit 1
	@echo ""
	@echo "Running End-to-End Pipeline Tests..."
	@./test_pipeline || exit 1
	@echo ""
	@echo "=========================================="
	@echo "All Integration Tests Passed!"
	@echo "=========================================="

clean:
	rm -f $(TESTS) *.o
	rm -rf $(BUILD_DIR)
	rm -f generated_*.c generated_*.h

.PHONY: $(TESTS)
