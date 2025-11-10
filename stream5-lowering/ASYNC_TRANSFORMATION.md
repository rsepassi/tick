# Async/Await Transformation - State Machine Generation

## Overview

The async/await transformation converts async functions containing `suspend` statements into resumable state machines. This document explains how the transformation works in the tick compiler's lowering phase.

## Key Concepts

### 1. State Machine Model

An async function is transformed into a state machine with:
- **State 0**: Initial entry point (function start)
- **State 1..N**: Resume points after each suspend statement
- **Coroutine Frame**: A tagged union storing live variables for each state
- **State Dispatch**: Logic to jump to the correct resume point based on saved state

### 2. Coroutine Frame Structure

The coroutine frame is a tagged union:
```c
struct CoroFrame {
    uint32_t state;  // Current state ID
    union {
        struct State0 { /* no fields - initial state */ };
        struct State1 { /* live vars after first suspend */ };
        struct State2 { /* live vars after second suspend */ };
        // ... one struct per suspend point
    } data;
};
```

Each state struct contains only the variables that are live at that resume point, minimizing memory usage.

### 3. Transformation Steps

1. **Analysis Phase** (stream4-coroutine):
   - Build Control Flow Graph (CFG)
   - Perform liveness analysis
   - Identify suspend points
   - Compute which variables are live at each suspend point
   - Calculate frame layout (tagged union size and field offsets)

2. **Lowering Phase** (stream5-lowering):
   - Transform AST to IR with suspend instructions
   - Split function at suspend points
   - Generate state save instructions before suspends
   - Generate state restore instructions at resume points
   - Create dispatch block for state-based jumping

## Example Transformation

### Source Code (Tick)

```tick
let compute = fn() -> i32 {
    var result = 0;

    result = result + 10;
    suspend;  // State 1

    result = result + 20;
    suspend;  // State 2

    result = result + 30;
    return result;  // Returns 60
};
```

### Analysis Results

**Live Variables:**
- After suspend 1: `result` (i32)
- After suspend 2: `result` (i32)

**State Structs:**
```
State 0: (empty - initial entry)
State 1: { result: i32 }
State 2: { result: i32 }
```

**Frame Layout:**
```
Total size: 8 bytes
  [0..3]  state: u32    (discriminator)
  [4..7]  union data    (max(State1.size, State2.size) = 4 bytes)
```

### Before Transformation (High-level IR)

```
function compute() -> i32:
  entry:
    %result = alloca i32
    store %result, 0

  block1:
    %t1 = load %result
    %t2 = add %t1, 10
    store %result, %t2
    suspend state=1

  block2:
    %t3 = load %result
    %t4 = add %t3, 20
    store %result, %t4
    suspend state=2

  block3:
    %t5 = load %result
    %t6 = add %t5, 30
    store %result, %t6
    return %t6
```

### After Transformation (State Machine IR)

```
function compute() -> i32:
  // Entry: Dispatch to correct state
  dispatch:
    %__coro_frame = param(0)  // Frame pointer passed as arg
    %__coro_state = load (%__coro_frame + 0)  // Load state discriminator

    // State dispatch (can be optimized to computed goto)
    %cmp0 = eq %__coro_state, 0
    br %cmp0, state_0_entry, state_check_1

  state_check_1:
    %cmp1 = eq %__coro_state, 1
    br %cmp1, resume_1, state_check_2

  state_check_2:
    %cmp2 = eq %__coro_state, 2
    br %cmp2, resume_2, state_0_entry  // Fallback to state 0

  // State 0: Initial entry
  state_0_entry:
    %result = alloca i32
    store %result, 0
    %t1 = load %result
    %t2 = add %t1, 10
    store %result, %t2

    // Save state before suspend
    state_save frame=%__coro_frame, state=1, vars=[%result]
    suspend state=1
    return void  // Return control to caller

  // State 1: Resume after first suspend
  resume_1:
    // Restore live variables from frame
    state_restore frame=%__coro_frame, state=1, vars=[%result]

    %t3 = load %result
    %t4 = add %t3, 20
    store %result, %t4

    // Save state before suspend
    state_save frame=%__coro_frame, state=2, vars=[%result]
    suspend state=2
    return void

  // State 2: Resume after second suspend
  resume_2:
    // Restore live variables from frame
    state_restore frame=%__coro_frame, state=2, vars=[%result]

    %t5 = load %result
    %t6 = add %t5, 30
    store %result, %t6
    return %t6  // Final return
```

## Generated C Code (Conceptual)

The IR would be lowered to C code like:

```c
struct CoroFrame_compute {
    uint32_t state;
    union {
        struct { /* empty */ } s0;
        struct { int32_t result; } s1;
        struct { int32_t result; } s2;
    } data;
};

int32_t compute_resume(struct CoroFrame_compute* frame) {
    // Dispatch to correct state
    static void* resume_targets[] = {
        &&STATE_0,
        &&STATE_1,
        &&STATE_2
    };

    goto *resume_targets[frame->state];

STATE_0:  // Initial entry
    {
        int32_t result = 0;
        result = result + 10;

        // Save state and suspend
        frame->state = 1;
        frame->data.s1.result = result;
        return -1;  // Signal: suspended
    }

STATE_1:  // Resume after first suspend
    {
        // Restore from frame
        int32_t result = frame->data.s1.result;

        result = result + 20;

        // Save state and suspend
        frame->state = 2;
        frame->data.s2.result = result;
        return -1;  // Signal: suspended
    }

STATE_2:  // Resume after second suspend
    {
        // Restore from frame
        int32_t result = frame->data.s2.result;

        result = result + 30;
        return result;  // Final result: 60
    }
}

// First call: allocate frame and start coroutine
struct CoroFrame_compute* compute_start() {
    struct CoroFrame_compute* frame = malloc(sizeof(*frame));
    frame->state = 0;
    compute_resume(frame);  // Run to first suspend
    return frame;
}
```

## Defer and Errdefer in Async Context

### Key Principles

1. **Defer executes on all exit paths** - including normal returns, errors, and coroutine destruction
2. **Errdefer executes only on error paths** - not on normal returns or suspends
3. **Defers do NOT execute at suspend points** - they execute when the function truly exits

### Example with Defer

```tick
let download = fn(url: String) -> Result<String> {
    let socket = try await connect(url);
    defer socket.close();  // Cleanup on ANY exit

    let data = try await socket.read();
    return data;
};
```

**Transformation considerations:**
- The defer `socket.close()` must execute on:
  - Normal return path
  - Error propagation from `connect`
  - Error propagation from `read`
  - Coroutine destruction (if suspended coroutine is dropped)

- The defer does NOT execute when:
  - The function suspends and will resume later

**Implementation:**
- Track active defers in coroutine frame metadata
- Generate cleanup code at each exit point
- For coroutine destruction, runtime calls cleanup based on current state

## State Dispatch Optimization

### Computed Goto (Fast)

```c
static void* resume_targets[] = {
    &&STATE_0,
    &&STATE_1,
    &&STATE_2
};
goto *resume_targets[frame->state];
```

**Pros:**
- Very fast (single indirect jump)
- No branches

**Cons:**
- GCC/Clang extension (not standard C)
- Requires labels-as-values support

### Switch Statement (Portable)

```c
switch (frame->state) {
    case 0: goto STATE_0;
    case 1: goto STATE_1;
    case 2: goto STATE_2;
}
```

**Pros:**
- Standard C
- Compiler can optimize to jump table

**Cons:**
- Slightly slower than computed goto
- Requires compiler optimization for best performance

## Memory Optimization: Tagged Unions

Instead of storing ALL variables in the frame, we use a tagged union where each state has its own struct containing ONLY the variables live at that state.

### Without Optimization (Naive)

```c
struct CoroFrame_naive {
    uint32_t state;
    int32_t x;      // Only live in states 1-2
    int32_t y;      // Only live in states 2-3
    int32_t z;      // Only live in state 3
    char* buffer;   // Only live in states 0-1
};
// Size: ~24 bytes (wasted space for inactive variables)
```

### With Tagged Union (Optimized)

```c
struct CoroFrame_optimized {
    uint32_t state;
    union {
        struct { char* buffer; } s0;           // 8 bytes
        struct { int32_t x; char* buffer; } s1; // 12 bytes
        struct { int32_t x; int32_t y; } s2;    // 8 bytes
        struct { int32_t y; int32_t z; } s3;    // 8 bytes
    } data;
};
// Size: 16 bytes (union size = max(8,12,8,8) = 12, aligned to 16)
// Saves ~8 bytes (33% reduction)
```

## Async I/O Integration

Async functions can integrate with platform I/O by:

1. **Submitting async operations** via `async_submit()` calls
2. **Suspending** while I/O is in progress
3. **Resuming** when I/O completes (via callback)

### Example

```tick
let read_file = fn(fd: i32) -> Result<i32> {
    var buffer: [4096]u8;
    var read_op: AsyncRead;

    // Setup operation
    read_op.base.opcode = ASYNC_OP_READ;
    read_op.fd = fd;
    read_op.buffer = &buffer[0];

    // Submit to runtime
    async_submit(runtime, &read_op.base);

    // Suspend until complete
    suspend;

    // Resume here when I/O completes
    return read_op.base.result;
};
```

**Generated IR includes:**
1. Call to `async_submit()` before suspend
2. State save with operation pointer
3. Resume point after I/O completes

## Implementation Files

### Source Files
- `/home/user/tick/stream5-lowering/src/lower.c` - Main transformation implementation
  - `ir_transform_to_state_machine()` - Main entry point
  - `generate_state_dispatch()` - Creates dispatch logic
  - `split_function_at_suspend_points()` - Splits function into states
  - `map_live_vars_to_values()` - Maps variables to IR values

### Test Files
- `/home/user/tick/stream5-lowering/test/test_lower.c` - Unit tests
- `/home/user/tick/integration/test_coroutine.c` - Integration tests

### Interface Files
- `/home/user/tick/interfaces2/ir.h` - IR structures
- `/home/user/tick/stream4-coroutine/coro_metadata.h` - Coroutine metadata

## Performance Characteristics

### Time Complexity
- **First call**: O(1) - allocate frame and start
- **Resume**: O(1) - computed goto or switch dispatch
- **State save/restore**: O(k) where k = number of live variables at that state

### Space Complexity
- **Frame size**: O(max_live) where max_live = maximum number of live variables at any suspend point
- With tagged unions: typically 2-4x smaller than naive approach

### Typical Sizes
- Simple coroutine (1-2 vars): 8-16 bytes
- Medium coroutine (5-10 vars): 32-64 bytes
- Complex coroutine (20+ vars): 128-256 bytes

## Future Enhancements

1. **Generator Optimization**: Special case for generator patterns
2. **Inlining**: Inline small coroutines to avoid frame allocation
3. **Zero-Cost Suspends**: Eliminate suspend overhead for tail-recursive cases
4. **Parallel Async**: Support for concurrent async operations
5. **Async Iterators**: First-class support for async iteration

## References

- Coroutine analysis: `/home/user/tick/stream4-coroutine/src/coro_analyze.c`
- IR lowering: `/home/user/tick/stream5-lowering/src/lower.c`
- Runtime interface: `/home/user/tick/examples/runtime/async_runtime.h`
