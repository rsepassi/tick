# Async Runtime - Epoll Integration

This directory contains a complete, production-quality epoll-based async runtime for the tick compiler's platform abstraction layer.

## Overview

The async runtime provides a platform-independent interface for asynchronous I/O operations. This implementation uses Linux's epoll for high-performance event notification.

## Architecture

### Core Components

1. **async_runtime.h** - Platform-independent interface
   - Defines the `async_t` base structure used by all operations
   - Operation-specific structures (read, write, accept, connect, timer, close)
   - Runtime API functions

2. **epoll_runtime.c** - Linux epoll implementation
   - Event loop based on `epoll_wait()`
   - Non-blocking I/O operations
   - Timer management with nanosecond precision
   - FD to operation mapping

### Operation Types

The runtime supports the following async operations:

- **ASYNC_OP_READ** - Asynchronous read from file descriptor
- **ASYNC_OP_WRITE** - Asynchronous write to file descriptor
- **ASYNC_OP_ACCEPT** - Accept incoming TCP connections
- **ASYNC_OP_CONNECT** - Establish TCP connection
- **ASYNC_OP_CLOSE** - Close file descriptor
- **ASYNC_OP_TIMER** - Timer with nanosecond precision

### Event Loop Model

The runtime uses a completion-based model:

1. Operations are submitted via `async_submit()`
2. Runtime registers FDs with epoll
3. Event loop (`async_run_once()`) waits for events
4. Completed operations are added to completion queue
5. Callbacks are invoked for completed operations

## API Reference

### Runtime Management

```c
// Create runtime instance
async_runtime* async_runtime_create(void);

// Destroy runtime and free resources
void async_runtime_destroy(async_runtime* rt);

// Run event loop for one iteration
// Returns: number of operations completed
int async_run_once(async_runtime* rt, int timeout_ms);

// Run until all operations complete
int async_run_until_complete(async_runtime* rt);

// Check if there are pending operations
bool async_has_pending(async_runtime* rt);
```

### Operation Submission

```c
// Submit operation to runtime
void async_submit(async_runtime* rt, async_t* op);

// Cancel pending operation
void async_cancel(async_runtime* rt, async_t* op);
```

### Time Functions

```c
// Get current monotonic time in nanoseconds
uint64_t async_time_now_ns(void);
```

### Operation Initialization

Each operation type has an init helper:

```c
void async_read_init(async_read_t* op, int fd, void* buffer, size_t count,
                     async_callback_t callback, void* ctx);

void async_write_init(async_write_t* op, int fd, const void* buffer, size_t count,
                      async_callback_t callback, void* ctx);

void async_accept_init(async_accept_t* op, int listen_fd,
                       async_callback_t callback, void* ctx);

void async_connect_init(async_connect_t* op, int fd, uint32_t addr, uint16_t port,
                        async_callback_t callback, void* ctx);

void async_timer_init(async_timer_t* op, uint64_t timeout_ns,
                      async_callback_t callback, void* ctx);

void async_close_init(async_close_t* op, int fd,
                      async_callback_t callback, void* ctx);
```

## Usage Examples

### Simple Timer

```c
void on_timer(async_t* op) {
    printf("Timer expired!\n");
}

async_runtime* rt = async_runtime_create();
async_timer_t timer;

async_timer_init(&timer, 1000000000ULL, on_timer, NULL);  // 1 second
async_submit(rt, &timer.base);

async_run_until_complete(rt);
async_runtime_destroy(rt);
```

### Async Read

```c
void on_read(async_t* op) {
    if (op->result > 0) {
        printf("Read %d bytes\n", op->result);
    } else {
        printf("Read error: %d\n", op->result);
    }
}

char buffer[1024];
async_read_t read_op;

async_read_init(&read_op, fd, buffer, sizeof(buffer), on_read, NULL);
async_submit(rt, &read_op.base);
```

### TCP Server

```c
void on_accept(async_t* op) {
    async_accept_t* accept_op = (async_accept_t*)op;
    if (op->result == 0) {
        int client_fd = accept_op->client_fd;
        // Handle client connection
    }
}

async_accept_t accept_op;
async_accept_init(&accept_op, listen_fd, on_accept, NULL);
async_submit(rt, &accept_op.base);
```

## Implementation Details

### Non-Blocking I/O

All file descriptors are automatically set to non-blocking mode by the runtime. Operations that would block return `-EAGAIN` or `-EWOULDBLOCK`.

### Edge-Triggered Epoll

The runtime uses edge-triggered mode (`EPOLLET`) with one-shot semantics (`EPOLLONESHOT`) to ensure each operation completes exactly once.

### Timer Management

Timers are maintained in a sorted linked list ordered by absolute deadline. The event loop timeout is adjusted based on the next expiring timer.

### Error Handling

- Operation results are stored in the `result` field of `async_t`
- Positive values indicate success (bytes transferred, etc.)
- Negative values are `-errno` error codes
- Zero for read operations indicates EOF

### Memory Management

- The runtime does not allocate memory for operations
- Users must ensure operations remain valid until completion
- Operations can be stack-allocated for synchronous patterns
- For async patterns, operations should be heap-allocated or embedded in context structures

## Performance Characteristics

- **Scalability**: O(1) operation submission and completion
- **Latency**: Timers accurate to ~1ms on typical Linux systems
- **Throughput**: Limited by epoll performance (excellent for high connection counts)
- **Memory**: O(n) where n is number of active operations

## Platform Requirements

- Linux kernel 2.6.27+ (for epoll_create1)
- POSIX.1-2001 for clock_gettime
- C11 compiler

## Building

```bash
make          # Build all tests
make test     # Build and run all tests
make clean    # Clean build artifacts
```

## Test Programs

### test_timer.c

Tests timer functionality with multiple concurrent timers of different durations.

**Expected output:**
```
Timer 1 expired (timeout was 50000000 ns)
Timer 2 expired (timeout was 100000000 ns)
Timer 3 expired (timeout was 200000000 ns)
```

### test_pipe_io.c

Tests async read/write operations using Unix pipes.

**Expected output:**
```
Write completed: 19 bytes
Read completed: 19 bytes
Read data: 'Hello, async world!'
```

### test_tcp_echo.c

Tests TCP accept/read/write with a simple echo server and client.

**Expected output:**
```
Accepted connection: client_fd=5, addr=127.0.0.1
Read 17 bytes: 'Hello from client'
Wrote 17 bytes
```

## Integration with Tick Compiler

The tick compiler generates code that uses the platform abstraction interface defined in `async_runtime.h`. The generated code:

1. Embeds `async_t` structures in coroutine frames
2. Initializes operations with appropriate opcodes
3. Submits operations to the runtime
4. Suspends coroutines until operations complete
5. Resumes coroutines via callbacks

### Generated Code Pattern

```c
// Coroutine frame
typedef struct {
    enum { STATE_0, STATE_1, STATE_2 } tag;
    union {
        struct { /* state 0 vars */ } state_0;
        struct {
            async_read_t read_op;  // Operation embedded in frame
            char buffer[1024];
        } state_1;
    } data;
} coro_frame_t;

// State machine
void coro_resume(coro_frame_t* frame, async_runtime* rt) {
    switch (frame->tag) {
        case STATE_0:
            // Initialize read operation
            async_read_init(&frame->data.state_1.read_op, ...);
            async_submit(rt, &frame->data.state_1.read_op.base);
            frame->tag = STATE_1;
            return;  // Suspend

        case STATE_1:
            // Operation completed, continue execution
            int bytes = frame->data.state_1.read_op.base.result;
            // ...
    }
}
```

## Future Enhancements

Possible improvements for production use:

1. **io_uring support** - Higher performance on modern kernels
2. **kqueue support** - macOS/BSD compatibility
3. **IOCP support** - Windows compatibility
4. **Priority queues** - Support operation priorities
5. **Batch submission** - Submit multiple operations at once
6. **Connection pooling** - Reuse connections efficiently
7. **Rate limiting** - Throttle operations per time period
8. **Statistics** - Operation latency and throughput metrics

## License

This runtime implementation is part of the tick compiler project and follows the same license.
