# Makefile for Lemon Parser Generator
# Source: https://github.com/sqlite/sqlite

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
TARGET = lemon

.PHONY: all clean

all: $(TARGET)

$(TARGET): lemon.c
	$(CC) $(CFLAGS) lemon.c -o $(TARGET)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	@echo "Lemon built successfully"
	@echo "Usage: ./lemon grammar.y"
	@echo "Template file lempar.c must be in the same directory"

test: $(TARGET)
	@echo "Lemon version:"
	@./$(TARGET) -x 2>&1 || true
