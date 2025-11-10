# Makefile for async runtime tests

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -O2
LDFLAGS =

# Source files
RUNTIME_SRC = epoll_runtime.c
RUNTIME_OBJ = epoll_runtime.o

# Test programs
TESTS = test_timer test_pipe_io test_tcp_echo

.PHONY: all clean test

all: $(TESTS)

$(RUNTIME_OBJ): $(RUNTIME_SRC) async_runtime.h
	$(CC) $(CFLAGS) -c $(RUNTIME_SRC) -o $(RUNTIME_OBJ)

test_timer: test_timer.c $(RUNTIME_OBJ)
	$(CC) $(CFLAGS) $< $(RUNTIME_OBJ) -o $@ $(LDFLAGS)

test_pipe_io: test_pipe_io.c $(RUNTIME_OBJ)
	$(CC) $(CFLAGS) $< $(RUNTIME_OBJ) -o $@ $(LDFLAGS)

test_tcp_echo: test_tcp_echo.c $(RUNTIME_OBJ)
	$(CC) $(CFLAGS) $< $(RUNTIME_OBJ) -o $@ $(LDFLAGS)

test: all
	@echo "Running timer test..."
	./test_timer
	@echo ""
	@echo "Running pipe I/O test..."
	./test_pipe_io
	@echo ""
	@echo "Running TCP echo test..."
	./test_tcp_echo
	@echo ""
	@echo "All tests passed!"

clean:
	rm -f $(RUNTIME_OBJ) $(TESTS)
