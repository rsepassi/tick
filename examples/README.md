# Examples

This directory contains example programs demonstrating all features of the async systems language compiler, from simple to complex.

## Overview

Examples are organized by complexity and feature coverage:

1. **Basic Examples** - Simple functions, types, control flow
2. **Error Handling** - Result types, try/catch, defer/errdefer
3. **Async/Coroutines** - Suspend/resume, async calls
4. **Full Programs** - Complete applications with I/O

## Example Programs

### 01_hello.tick
Basic function definition and calling.

### 02_types.tick
Demonstrates primitive types, structs, enums, and unions.

### 03_control_flow.tick
If statements, loops, switch statements.

### 04_errors.tick
Error handling with Result types, try/catch, and propagation.

### 05_resources.tick
Resource management with defer and errdefer.

### 06_async_basic.tick
Basic async/await with suspend and resume.

### 07_async_io.tick
Async I/O operations with the epoll runtime.

### 08_tcp_echo_server.tick
Complete TCP echo server using epoll-based async I/O.

## Runtime

The `runtime/` directory contains the epoll-based async runtime that provides:
- File descriptor I/O (read, write, accept, connect)
- Timer operations
- Event loop with epoll

## Building and Running

Each example can be compiled with the tick compiler:

```bash
# Compile example
./tick compile examples/01_hello.tick -o hello

# Run
gcc -std=c11 -Wall -Wextra hello.c runtime/epoll_runtime.c -o hello
./hello
```
