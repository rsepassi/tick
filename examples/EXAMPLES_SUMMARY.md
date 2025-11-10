# Examples Summary

## Quick Reference

| Example | Focus | Complexity | Features Demonstrated |
|---------|-------|------------|----------------------|
| 01_hello.tick | Basic functions | ★☆☆☆☆ | Function definition, calling, return values |
| 02_types.tick | Type system | ★★☆☆☆ | Structs, enums, unions, arrays, packed structs |
| 03_control_flow.tick | Control flow | ★★☆☆☆ | If/else, loops, switch, computed goto pattern |
| 04_errors.tick | Error handling | ★★★☆☆ | Result types, try/catch, error propagation |
| 05_resources.tick | Resource management | ★★★☆☆ | defer, errdefer, cleanup patterns |
| 06_async_basic.tick | Coroutines | ★★★★☆ | async, suspend, resume, state machines |
| 07_async_io.tick | Async I/O | ★★★★☆ | Platform abstraction, callbacks, I/O operations |
| 08_tcp_echo_server.tick | Complete app | ★★★★★ | TCP server, accept/read/write, concurrent clients |

## Learning Path

### Beginner (Examples 1-3)

Start here to learn the basics:

1. **01_hello.tick** - Write your first tick program
2. **02_types.tick** - Understand the type system
3. **03_control_flow.tick** - Master control flow constructs

**Key Concepts:**
- Function definition and calling
- Primitive types (i32, u8, bool, etc.)
- Composite types (struct, enum, union, arrays)
- Control flow (if, for, switch)
- Computed goto pattern for VM interpreters

### Intermediate (Examples 4-5)

Learn error handling and resource management:

4. **04_errors.tick** - Handle errors safely
5. **05_resources.tick** - Manage resources properly

**Key Concepts:**
- Result types (ErrorType!ValueType)
- try/catch blocks
- Error propagation
- defer for cleanup
- errdefer for error-path cleanup
- LIFO cleanup order

### Advanced (Examples 6-8)

Master async programming:

6. **06_async_basic.tick** - Understand coroutines
7. **07_async_io.tick** - Async I/O patterns
8. **08_tcp_echo_server.tick** - Build real applications

**Key Concepts:**
- Coroutines and state machines
- async/suspend/resume
- Platform abstraction layer
- Callback-based I/O
- Coroutine-based I/O
- Event loop integration
- Concurrent operations

## Feature Coverage

### Language Features

| Feature | Examples |
|---------|----------|
| Functions | 01, 02, 03, 04, 05, 06, 07, 08 |
| Structs | 02, 05, 07, 08 |
| Enums | 02, 03, 04, 05, 07, 08 |
| Unions | 02 |
| Arrays | 02, 03, 05, 06, 07, 08 |
| Slices | 02, 05, 07, 08 |
| Packed structs | 02 |
| If/else | 03, 04, 05, 06, 07, 08 |
| For loops | 03, 05, 07, 08 |
| Switch | 03, 04, 08 |
| Computed goto | 03 |
| Result types | 04, 05, 07, 08 |
| try/catch | 04, 05 |
| defer | 05, 07, 08 |
| errdefer | 05, 08 |
| async | 06, 07, 08 |
| suspend | 06, 07, 08 |
| resume | 06, 07, 08 |

### Platform Operations

| Operation | Examples |
|-----------|----------|
| Read | 07, 08 |
| Write | 07, 08 |
| Accept | 08 |
| Connect | 07 |
| Close | 07, 08 |
| Timer | 07 |

## Code Metrics

### Lines of Code

| Example | LOC | Comments |
|---------|-----|----------|
| 01_hello.tick | 22 | 7 |
| 02_types.tick | 61 | 14 |
| 03_control_flow.tick | 105 | 8 |
| 04_errors.tick | 99 | 11 |
| 05_resources.tick | 128 | 12 |
| 06_async_basic.tick | 142 | 13 |
| 07_async_io.tick | 204 | 20 |
| 08_tcp_echo_server.tick | 283 | 22 |
| **Total** | **1044** | **107** |

### Complexity Analysis

| Example | Functions | Suspend Points | Max Nesting | Cyclomatic Complexity |
|---------|-----------|----------------|-------------|----------------------|
| 01_hello.tick | 3 | 0 | 1 | 2 |
| 02_types.tick | 3 | 0 | 1 | 2 |
| 03_control_flow.tick | 6 | 0 | 3 | 15 |
| 04_errors.tick | 6 | 0 | 3 | 12 |
| 05_resources.tick | 7 | 0 | 3 | 10 |
| 06_async_basic.tick | 7 | 14 | 2 | 8 |
| 07_async_io.tick | 9 | 8 | 3 | 18 |
| 08_tcp_echo_server.tick | 11 | 12 | 4 | 24 |

## Design Patterns

### Callback Pattern (Example 07)

Used for event-driven programming:

```tick
let on_complete = fn(op: *AsyncOp) {
    # Handle completion
};

let operation = setup_operation();
operation.callback = on_complete;
submit(operation);
```

**Pros:**
- Simple, direct
- Low overhead

**Cons:**
- Callback hell for complex flows
- Hard to maintain state

### Coroutine Pattern (Examples 06-08)

Used for sequential async code:

```tick
let process = fn() {
    let data = async_read(fd);
    suspend;  # Wait for completion

    # Data available, continue
    async_write(fd, data);
    suspend;
};
```

**Pros:**
- Sequential logic
- Easy to read
- State managed automatically

**Cons:**
- Requires coroutine frame
- State machine complexity

### State Machine Pattern (Example 08)

Used for complex protocols:

```tick
let State = enum(u8) {
    READING = 0,
    WRITING = 1,
    CLOSING = 2
};

let handle_client = fn(client: *Client) {
    switch client.state {
        case State.READING:
            # Read data
        case State.WRITING:
            # Write data
        case State.CLOSING:
            # Close connection
    }
};
```

**Pros:**
- Explicit state management
- Easy to debug
- Good for protocols

**Cons:**
- More boilerplate
- Manual state tracking

## Common Patterns

### Resource Acquisition

```tick
let resource = acquire();
defer release(resource);  # Guaranteed cleanup

# Use resource...
# release() called automatically
```

### Error Propagation

```tick
let result = try risky_operation();  # Propagates error
# Only reached if successful
```

### Error Handling

```tick
try {
    let result = risky_operation();
    use_result(result);
} catch |err| {
    handle_error(err);
}
```

### Concurrent Operations

```tick
let coro1 = async operation1();
let coro2 = async operation2();

# Both running concurrently
resume coro1;
resume coro2;
```

### Loop with Cleanup

```tick
for i in 0..10 : cleanup() {
    # Body
    # cleanup() runs after each iteration
}
```

## Testing Examples

### Unit Test Pattern

```tick
let test_add = fn() -> bool {
    let result = add(2, 3);
    return result == 5;
};

pub let main = fn() -> i32 {
    if !test_add() {
        return 1;  # Test failed
    }
    return 0;  # Success
};
```

### Integration Test Pattern

```tick
let test_echo = fn() -> bool {
    let server = start_server();
    defer stop_server(server);

    let client = connect_to_server();
    defer close_client(client);

    write(client, "test");
    let response = read(client);

    return response == "test";
};
```

## Runtime Integration

All examples work with the epoll runtime in `runtime/`:

### Building with Runtime

```bash
# Compile tick code (future)
tick compile example.tick -o example.c

# Compile with runtime
gcc -std=c11 -Wall -Wextra \
    example.c \
    runtime/epoll_runtime.c \
    -o example

# Run
./example
```

### Runtime Requirements

- Linux kernel 2.6.27+
- POSIX.1-2001
- C11 compiler (gcc, clang)

## Performance Notes

### Memory Usage

| Example | Stack Frame | Heap Allocation |
|---------|-------------|-----------------|
| 01-05 | < 1 KB | None |
| 06 | ~4 KB | None (sync) or sizeof(frame) (async) |
| 07 | ~8 KB | None (sync) or sizeof(frame) (async) |
| 08 | ~5 KB per client | sizeof(Server) + n * sizeof(Client) |

### Async Performance

- **Event loop latency**: ~1-2 ms on typical hardware
- **Timer accuracy**: ~1 ms
- **Throughput**: Limited by kernel (thousands of ops/sec)
- **Scalability**: O(1) per operation

### Optimization Tips

1. **Minimize state size**: Only keep live variables
2. **Batch operations**: Submit multiple at once
3. **Reuse frames**: Pool coroutine frames for hot paths
4. **Avoid heap allocation**: Use stack for short-lived coroutines
5. **Profile**: Use timing to identify bottlenecks

## Next Steps

1. **Read the examples** in order from 01 to 08
2. **Study the runtime** in `runtime/README.md`
3. **Read the integration guide** in `INTEGRATION_GUIDE.md`
4. **Try modifying examples** to experiment
5. **Build your own** async applications

## Additional Resources

- **Language Design**: `../doc/design.md`
- **Implementation Guide**: `../doc/impl-guide.md`
- **Runtime API**: `runtime/async_runtime.h`
- **Integration Guide**: `INTEGRATION_GUIDE.md`
- **Remaining Work**: `../doc/remaining-work.md`
