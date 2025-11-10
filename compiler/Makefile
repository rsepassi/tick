CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -g -O0
INCLUDES = -I../interfaces2 -I../stream6-codegen/include -I../stream2-parser

# Ensure parser is generated first
PARSER_GENERATED = ../stream2-parser/build/grammar.c

# Source files from various streams
LEXER_SRC = ../stream1-lexer/src/lexer.c
PARSER_SRC = ../stream2-parser/src/parser.c $(PARSER_GENERATED)
SEMANTIC_SRC = ../stream3-semantic/src/resolver.c ../stream3-semantic/src/typeck.c
LOWERING_SRC = ../stream5-lowering/src/lower.c
CODEGEN_SRC = ../stream6-codegen/src/codegen.c
INFRA_SRC = ../stream1-lexer/src/arena.c \
            ../stream1-lexer/src/error.c \
            ../stream1-lexer/src/string_pool.c

MAIN_SRC = main.c

ALL_SRC = $(MAIN_SRC) $(LEXER_SRC) $(PARSER_SRC) $(SEMANTIC_SRC) \
          $(LOWERING_SRC) $(CODEGEN_SRC) $(INFRA_SRC)

# Parser dependencies
PARSER_DEP = ../stream2-parser/src/parser.c ../stream2-parser/src/ticklang.h

OBJS = $(ALL_SRC:.c=.o)

TARGET = tickc

.PHONY: all clean test parser

all: parser $(TARGET)

parser:
	@cd ../stream2-parser && make

$(TARGET): $(ALL_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(ALL_SRC)

clean:
	rm -f $(TARGET) *.o

test: $(TARGET)
	@echo "Testing compiler with examples/01_hello.tick..."
	./$(TARGET) -v -emit-c ../examples/01_hello.tick -o test_output
