# Epoll Integration Guide

This guide explains how the tick compiler's async/await system integrates with the epoll-based runtime.

## Overview

The tick compiler transforms async/await code into state machines that interact with a platform-specific runtime. This implementation uses Linux epoll for high-performance async I/O.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                 Tick Source Code                        │
│  (async/await, suspend, resume, coroutines)             │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│             Tick Compiler Pipeline                      │
│  Lexer → Parser → Semantic → Coroutine → Lower → Codegen│
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│              Generated C Code                           │
│  - State machines with tagged union frames              │
│  - Platform abstraction calls (async_submit, etc.)      │
│  - Callbacks for operation completion                   │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│          Platform Abstraction Layer                     │
│              (async_runtime.h)                          │
│  - async_t base structure                               │
│  - Operation types (read, write, accept, etc.)          │
│  - Runtime API (submit, run, cancel)                    │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│         Epoll Runtime Implementation                    │
│            (epoll_runtime.c)                            │
│  - Event loop with epoll_wait()                         │
│  - Non-blocking I/O                                     │
│  - Timer management                                     │
│  - Completion callbacks                                 │
└─────────────────────────────────────────────────────────┘
```

## Compilation Flow

### 1. Tick Source Code

Example coroutine with async I/O:

```tick
let read_file = fn(fd: i32) -> IOError!usize {
    var buffer: [4096]u8;

    # Async read operation
    let bytes_read = async_read(fd, &buffer[0], 4096);

    # Suspend until I/O completes
    suspend;

    # Resume here when read completes
    return bytes_read;
};
```

### 2. Coroutine Analysis

The compiler identifies:
- **Suspend points**: Where coroutine can pause
- **Live variables**: Variables needed after each suspend
- **State layout**: Tagged union with one struct per state

### 3. State Machine Generation

Generated C code structure:

```c
// Coroutine frame (tagged union)
typedef struct {
    enum { STATE_0, STATE_1 } tag;
    union {
        struct {
            int32_t __u_fd;
            uint8_t __u_buffer[4096];
        } state_0;
        struct {
            async_read_t __async_read_op;
            uint8_t __u_buffer[4096];
        } state_1;
    } data;
} __coro_frame_read_file;

// Completion callback
void __coro_read_file_callback(async_t* op) {
    __coro_frame_read_file* frame = ((__coro_frame_read_file*)op->ctx);
    __coro_resume_read_file(frame);
}

// State machine resume function
void __coro_resume_read_file(__coro_frame_read_file* frame) {
    switch (frame->tag) {
        case STATE_0: {
            // Initialize async read
            async_read_init(
                &frame->data.state_1.__async_read_op,
                frame->data.state_0.__u_fd,
                frame->data.state_0.__u_buffer,
                4096,
                __coro_read_file_callback,
                frame
            );

            // Submit to runtime
            async_submit(__runtime, &frame->data.state_1.__async_read_op.base);

            // Save state and suspend
            frame->tag = STATE_1;
            return;
        }

        case STATE_1: {
            // Read completed, get result
            int32_t result = frame->data.state_1.__async_read_op.base.result;

            if (result < 0) {
                // Error handling
                return;
            }

            // Continue execution with result
            size_t bytes_read = (size_t)result;
            // ...
        }
    }
}
```

## Key Integration Points

### 1. async_t Embedding

Each async operation is embedded in the coroutine frame's state struct:

```c
struct {
    async_read_t read_op;  // Operation structure
    uint8_t buffer[4096];  // User data
} state_1;
```

This ensures the operation structure remains valid while the I/O is pending.

### 2. Callback Registration

When submitting an operation, the compiler generates a callback that resumes the coroutine:

```c
void __coro_callback(async_t* op) {
    __coro_frame* frame = (__coro_frame*)op->ctx;
    __coro_resume(frame);
}
```

The callback:
1. Retrieves the coroutine frame from the operation's `ctx` pointer
2. Calls the coroutine's resume function
3. The resume function continues from the next state

### 3. State Transitions

State transitions follow this pattern:

```
STATE_0 (initial)
    ↓
    Initialize operation
    Submit to runtime
    Save state → STATE_1
    Return (suspend)

    ... (runtime processes I/O) ...

    Callback invoked
    ↓
STATE_1 (resumed)
    ↓
    Read operation result
    Continue execution
```

### 4. Runtime Context

The generated code maintains a global or thread-local runtime pointer:

```c
static async_runtime* __runtime;

void __init_runtime(void) {
    __runtime = async_runtime_create();
}

void __shutdown_runtime(void) {
    async_runtime_destroy(__runtime);
}
```

## Example: Complete Integration

### Tick Source

```tick
let echo_server = fn(listen_fd: i32) {
    for {
        # Accept connection
        let client_fd = async_accept(listen_fd);
        suspend;

        # Read from client
        var buffer: [1024]u8;
        let n = async_read(client_fd, &buffer[0], 1024);
        suspend;

        # Echo back
        async_write(client_fd, &buffer[0], n);
        suspend;

        # Close connection
        async_close(client_fd);
        suspend;
    }
};
```

### Generated C (simplified)

```c
typedef struct {
    enum { STATE_ACCEPT, STATE_READ, STATE_WRITE, STATE_CLOSE } tag;
    union {
        struct { async_accept_t accept_op; } state_accept;
        struct { async_read_t read_op; uint8_t buffer[1024]; } state_read;
        struct { async_write_t write_op; uint8_t buffer[1024]; size_t n; } state_write;
        struct { async_close_t close_op; } state_close;
    } data;
} echo_server_frame;

void echo_server_callback(async_t* op) {
    echo_server_frame* frame = (echo_server_frame*)op->ctx;
    echo_server_resume(frame);
}

void echo_server_resume(echo_server_frame* frame) {
    switch (frame->tag) {
        case STATE_ACCEPT:
            // Get accepted client FD
            async_accept_t* accept = &frame->data.state_accept.accept_op;
            int client_fd = accept->client_fd;

            // Setup read
            async_read_init(&frame->data.state_read.read_op,
                          client_fd,
                          frame->data.state_read.buffer,
                          1024,
                          echo_server_callback,
                          frame);
            async_submit(__runtime, &frame->data.state_read.read_op.base);
            frame->tag = STATE_READ;
            return;

        case STATE_READ:
            // Get bytes read
            int n = frame->data.state_read.read_op.base.result;

            // Copy buffer to write state
            memcpy(frame->data.state_write.buffer,
                   frame->data.state_read.buffer,
                   n);
            frame->data.state_write.n = n;

            // Setup write
            async_write_init(&frame->data.state_write.write_op,
                           /* client_fd from somewhere */,
                           frame->data.state_write.buffer,
                           n,
                           echo_server_callback,
                           frame);
            async_submit(__runtime, &frame->data.state_write.write_op.base);
            frame->tag = STATE_WRITE;
            return;

        // ... STATE_WRITE and STATE_CLOSE ...
    }
}
```

### Runtime Execution

```c
int main(void) {
    async_runtime* rt = async_runtime_create();

    // Create listen socket
    int listen_fd = create_listen_socket(8080);

    // Initialize coroutine
    echo_server_frame frame = {0};
    frame.tag = STATE_ACCEPT;

    async_accept_init(&frame.data.state_accept.accept_op,
                     listen_fd,
                     echo_server_callback,
                     &frame);
    async_submit(rt, &frame.data.state_accept.accept_op.base);

    // Run event loop
    while (async_has_pending(rt)) {
        async_run_once(rt, -1);
    }

    async_runtime_destroy(rt);
    return 0;
}
```

## Memory Model

### Stack Allocation (Synchronous)

For synchronous coroutines that run to completion:

```c
echo_server_frame frame = {0};
echo_server_resume(&frame);
// Frame destroyed when function returns
```

### Heap Allocation (Asynchronous)

For truly async coroutines:

```c
echo_server_frame* frame = malloc(sizeof(echo_server_frame));
// ... initialize and submit ...
// Frame must remain valid until coroutine completes
// Cleanup in final state or completion callback
```

## Error Handling

Operations report errors via the `result` field:

```c
case STATE_READ:
    async_read_t* read_op = &frame->data.state_read.read_op;

    if (read_op->base.result < 0) {
        // Error: result is -errno
        int error = -read_op->base.result;
        // Handle error (close connection, retry, etc.)
    } else {
        // Success: result is bytes read
        size_t bytes = (size_t)read_op->base.result;
    }
    break;
```

## Concurrency Model

### Single Coroutine

One coroutine runs at a time. When it suspends, control returns to the event loop:

```
Coroutine A: run → suspend
    Event Loop: wait for I/O
Coroutine A: resume → complete
```

### Multiple Coroutines

Multiple coroutines can be active concurrently:

```
Coroutine A: run → suspend (waiting on read)
Coroutine B: run → suspend (waiting on write)
Coroutine C: run → suspend (waiting on timer)
    Event Loop: wait for any I/O
Coroutine B: resume (write completed) → suspend
Coroutine A: resume (read completed) → complete
    Event Loop: wait for remaining
Coroutine C: resume (timer expired) → complete
```

The event loop processes whichever operation completes first.

## Performance Considerations

### Frame Size

- **Optimization**: Coroutine frames only store live variables per state
- **Benefit**: Minimal memory usage, good cache locality
- **Trade-off**: More states if many variables cross suspend points

### Event Loop

- **epoll_wait**: O(1) for ready events
- **FD mapping**: O(n) linear search (could use hash table)
- **Timer queue**: O(n) insertion, O(1) expiry check

### System Calls

- **Per operation**: 1 syscall to submit (epoll_ctl), 1 to complete (read/write/accept/etc.)
- **Batching**: Event loop can process multiple completions per epoll_wait

## Debugging

### Enable Line Directives

The compiler generates `#line` directives mapping C code back to tick source:

```c
#line 42 "server.tick"
async_read_init(...);  // Maps to line 42 in server.tick
```

### Tracing Support

Each `async_t` has tracing fields:

```c
op->parent = parent_op;     // Parent operation
op->req_id = next_req_id++; // Unique request ID
```

These can be used with tracing frameworks (e.g., Perfetto) to visualize async operation chains.

### Debug Printing

The runtime can be instrumented to log events:

```c
#define DEBUG_LOG(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)

void async_submit(async_runtime* rt, async_t* op) {
    DEBUG_LOG("Submit op %p, opcode=%u, req_id=%lu", op, op->opcode, op->req_id);
    // ...
}
```

## Testing

### Unit Tests

Test individual components:

1. **Runtime tests**: `test_timer.c`, `test_pipe_io.c`, `test_tcp_echo.c`
2. **Compiler tests**: Integration tests in `integration/`

### Integration Tests

Test complete flows:

1. Compile tick code to C
2. Compile C with runtime
3. Execute and verify behavior

### Stress Tests

Test under load:

1. Many concurrent connections
2. Large data transfers
3. Long-running operations
4. Operation cancellation

## Platform Portability

### Current: Linux/epoll

Requires:
- Linux kernel 2.6.27+
- epoll API
- POSIX timers

### Future: io_uring

Higher performance on Linux 5.1+:
- Batch submission
- Zero-copy operations
- Reduced syscall overhead

### Future: kqueue

macOS/BSD support:
- Similar API to epoll
- Different event types (EV_SET, kevent)

### Future: IOCP

Windows support:
- Completion-based (like our model)
- Different I/O primitives (ReadFile, WriteFile, etc.)

## Summary

The tick compiler's epoll integration provides:

1. **Clean abstraction**: Platform-independent async operations
2. **Efficient execution**: State machines with minimal overhead
3. **Scalability**: Event-driven I/O handles many concurrent operations
4. **Portability**: Runtime can be swapped for different platforms
5. **Debuggability**: Line directives and tracing support

This architecture enables writing high-performance async systems code in tick that compiles to efficient, portable C.
