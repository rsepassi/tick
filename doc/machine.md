# Async Runtime Specification

## Core Types

```pseudocode
let Machine = struct {
  // Memory layout
  state_size: usz,
  state_align2: u8,
  state: ?*void,

  // Function pointer
  tick: *fn (machine: *Machine, now: Timestamp),

  // Communication ports
  port_in: Port,
  port_out: Port,

  // Output signaling (context is event loop for roots, parent for children)
  signal_ctx: *void,
  signal_output_fn: *fn(ctx: *void, machine: *Machine),

  // Hierarchy
  id: MachineRef,
  parent: ?*Machine,
  children: []Machine,

  // Intrusive list linkage for event loop scheduling
  has_input: bool,
  next_with_input: ?*Machine,
  has_output: bool,
  next_with_output: ?*Machine,
};

let Port = struct {
  submissions: AsyncOpQ,    // New async requests
  cancellations: AsyncOpQ,  // Cancel requests
  completions: AsyncOpQ,    // One-shot completions
  notifications: AsyncOpQ,  // Standing op notifications
};

let AsyncOpQ = struct {
  head: ?*AsyncOp,
  tail: ?*AsyncOp,
};

let AsyncOp = struct {
  // Identity
  id: OpId,

  // Tracing
  trace_id: TraceId,
  parent_op_id: OpId,       // 0 if root operation

  // Operation
  op: u32,                  // OP_READ, OP_WRITE, etc.
  result: i32,              // Result code

  // Routing
  destination: ?MachineRef,
  source: ?MachineRef,

  // Intrusive list
  next: *AsyncOp,
  prev: *AsyncOp,
};

let MachineRef = struct {
  thread_id: ThreadId,
  machine_id: MachineId,
};

let ThreadId = u32;
let MachineId = u32;
let Timestamp = u64;
let OpId = u64;
let TraceId = u64;
```

## Memory Ownership

- **AsyncOp**: Always owned by initiator until completion
- **Queues**: Intrusive - no allocations, just pointer manipulation
- **Buffers**: Provided by requester with overflow policy
- **State**: Parent owns Machine and its state

## Threading Model

### Single Writer per Queue

- `port_in` ← written by event loop
- `port_out` ← written by machine

### Cross-Thread Communication

```pseudocode
let ThreadLocalEventLoop = struct {
  thread_id: ThreadId,
  app_machine: *Machine,
  platform_machine: *Machine,
  timer_heap: TimerHeap,       // Outstanding timers ordered by deadline
  inbox: LockFreeQueue,        // Other threads write here
  outboxes: []*LockFreeQueue,  // One per remote thread
  machines_with_input: ?*Machine,   // Intrusive list of machines with pending input
  machines_with_output: ?*Machine,  // Intrusive list of machines with pending output
};
```

Event loop handles cross-thread routing explicitly. Machines never touch
lock-free queues.

## Operation Lifecycle

### One-Shot Operations
1. Machine allocates AsyncOp from its pool
2. Submits via port_out
3. Platform/target processes request
4. Returns same AsyncOp with result filled
5. Machine receives completion and frees AsyncOp

### Standing Operations
1. Machine allocates AsyncOp + shared memory (ring buffer, etc.)
2. Submits via port_out
3. Platform writes to shared memory, rings doorbell (enqueues AsyncOp)
4. Machine processes doorbell, reads shared memory
5. AsyncOp can be re-enqueued (doorbell is idempotent)
6. Continues until error or cancellation
7. Upon error or successful cancellation, Machine receives completion and frees
   AsyncOp + shared memory

### Doorbell Mechanics

```pseudocode
fn ring_doorbell(op: *AsyncOp) {
  if (op.next == null) {  // already enqueued
    enqueue(&destination.port_in.notifications, op);
  }
}

fn handle_doorbell(op: *AsyncOp) {
  // Process accumulated data from shared memory
  // Note: op.next/prev cleared by dequeue, can be re-enqueued
}
```

## Cancellation

- **Semantics**: Best-effort, no races
- **Guarantee**: Always receive completion or cancellation
- **Standing ops**: Cancellation completes the op

```pseudocode
fn submit_cancellation(op: *AsyncOp) {
  enqueue(&op.source.port_out.cancellations, op);
}
```

## Shared Memory Patterns

### Network Receive

```pseudocode
let NetworkRecvOp = struct {
  base: AsyncOp,
  ring: *RingBuffer,
  stats: *RecvStats,
  overflow_policy: enum { DropNewest, OverwriteOldest },
};
```

### Timer

```pseudocode
let TimerOp = struct {
  base: AsyncOp,
  deadline: Timestamp,
};
```

### Machine Channel

```pseudocode
let ChannelOp = struct {
  base: AsyncOp,
  spsc: *SPSCQueue,
};
```

## Flow Control

- **Natural backpressure**: Machines initiate AsyncOps and either statically
  allocate or otherwise bound their own usage
- **Bounded buffers**: Requester provides buffers with overflow policy
- **No hidden allocations**: All resources pre-allocated or explicitly managed

## Event Loop

```pseudocode
fn event_loop_tick(loop: *ThreadLocalEventLoop) {
  // Phase 1: Wait for I/O or next timer (single blocking call)
  let deadline = timer_heap_next_deadline(&loop.timer_heap);
  let now = platform_wait(deadline);  // Blocks on I/O + timers, returns time

  // Phase 2: Process expired timers
  while (timer_heap_pop_expired(&loop.timer_heap, now)) |timer| {
    complete(timer);
  }

  // Phase 3: Drain cross-thread inbox (bounded to work at start)
  let count = inbox_count(&loop.inbox);
  for (i in 0..count) {
    if (let op = loop.inbox.dequeue()) {
      route_to_local_machine(loop, op);
    }
  }

  // Phase 4: Route machine outputs (only machines with pending output)
  let machine = loop.machines_with_output;
  loop.machines_with_output = null;
  while (machine != null) {
    let next = machine.next_with_output;
    machine.has_output = false;
    machine.next_with_output = null;
    drain_and_route(loop, machine);
    machine = next;
  }

  // Phase 5: Tick machines (only machines with pending input)
  let machine = loop.machines_with_input;
  loop.machines_with_input = null;
  while (machine != null) {
    let next = machine.next_with_input;
    machine.has_input = false;
    machine.next_with_input = null;
    machine.tick(machine, now);
    machine = next;
  }
}

fn drain_and_route(loop: *ThreadLocalEventLoop, machine: *Machine) {
  // Route submissions
  while (let op = dequeue(&machine.port_out.submissions)) {
    route_submission(loop, op);
  }

  // Route cancellations
  while (let op = dequeue(&machine.port_out.cancellations)) {
    route_cancellation(loop, op);
  }
}

fn route_submission(loop: *ThreadLocalEventLoop, op: *AsyncOp) {
  // Apply default destination if null (app ↔ platform)
  let dest = op.destination ?? get_default_destination(loop, op.source);

  // Intercept timer operations
  if (op.op == OP_TIMER) {
    timer_heap_insert(&loop.timer_heap, op);
    return;
  }

  // Route based on thread
  if (dest.thread_id == loop.thread_id) {
    // Local routing - enqueue and mark machine as having input
    let machine = find_local_machine(loop, dest.machine_id);
    enqueue_machine_input(loop, machine, &machine.port_in.submissions, op);
  } else {
    // Remote routing - cross-thread outbox
    loop.outboxes[dest.thread_id].enqueue(op);
  }
}

fn route_cancellation(loop: *ThreadLocalEventLoop, op: *AsyncOp) {
  // Similar to route_submission but for cancellations
  let dest = op.destination ?? get_default_destination(loop, op.source);

  if (dest.thread_id == loop.thread_id) {
    let machine = find_local_machine(loop, dest.machine_id);
    enqueue_machine_input(loop, machine, &machine.port_in.cancellations, op);
  } else {
    loop.outboxes[dest.thread_id].enqueue(op);
  }
}

fn get_default_destination(loop: *ThreadLocalEventLoop, source: MachineRef) {
  // Default routing: app ↔ platform
  if (source == loop.app_machine.id) {
    return loop.platform_machine.id;
  } else if (source == loop.platform_machine.id) {
    return loop.app_machine.id;
  } else {
    @panic("No default destination for child machine");
  }
}

fn enqueue_machine_input(loop: *ThreadLocalEventLoop, machine: *Machine,
                         queue: *AsyncOpQ, op: *AsyncOp) {
  enqueue(queue, op);

  // Add machine to input list if not already there
  if (!machine.has_input) {
    machine.has_input = true;
    machine.next_with_input = loop.machines_with_input;
    loop.machines_with_input = machine;
  }
}

fn machine_signal_output(machine: *Machine) {
  // Delegate to the configured signaling function
  machine.signal_output_fn(machine.signal_ctx, machine);
}

// Signaling function for root machines
fn root_machine_signal_output(ctx: *void, machine: *Machine) {
  let loop = (ThreadLocalEventLoop*)ctx;

  // Add machine to output list if not already there
  if (!machine.has_output) {
    machine.has_output = true;
    machine.next_with_output = loop.machines_with_output;
    loop.machines_with_output = machine;
  }
}

// Signaling function for child machines
fn child_machine_signal_output(ctx: *void, machine: *Machine) {
  let parent = (Machine*)ctx;

  // Signal the parent to service this child's output
  // Parent will drain child outputs during its tick
  parent.signal_output_fn(parent.signal_ctx, parent);
}
```

**platform_wait behavior:**
- Blocks until I/O is ready OR deadline is reached
- Returns current timestamp
- Uses epoll/kqueue/IOCP for I/O readiness
- Single blocking point in entire event loop

## Hierarchy

- **Root**: App machine + Platform machine only
- **Children**: Created by parents, destroyed before parent
- **Cleanup**: Cancel all ops, drain completions, then free

### Machine Initialization

**Root machines** (app and platform):
```pseudocode
fn init_root_machine(loop: *ThreadLocalEventLoop, ...) -> *Machine {
  let machine = allocate_machine(...);
  machine.parent = null;
  machine.signal_ctx = loop;
  machine.signal_output_fn = root_machine_signal_output;
  return machine;
}
```

**Child machines**:
```pseudocode
fn create_child_machine(parent: *Machine, ...) -> *Machine {
  let child = allocate_machine(...);
  child.parent = parent;
  child.signal_ctx = parent;
  child.signal_output_fn = child_machine_signal_output;
  append(&parent.children, child);
  return child;
}
```

This design ensures:
- Child machines cannot bypass their parent to reach the event loop
- Parents can intercept and process child outputs
- Clear abstraction boundary for machine composition

### Parent Handling Child Outputs

When a parent machine is ticked by the event loop, it should check and drain
child outputs before/after its own logic:

```pseudocode
fn parent_machine_tick(machine: *Machine, now: Timestamp) {
  // Process own inputs
  while (let op = dequeue(&machine.port_in.submissions)) {
    handle_submission(machine, op);
  }

  // Execute state machine logic
  run_state_machine_logic(machine, now);

  // Drain child outputs and route them
  for (child in machine.children) {
    while (let op = dequeue(&child.port_out.submissions)) {
      // Parent routes child's operations
      // Can intercept, transform, or forward to own port_out
      handle_child_operation(machine, child, op);
    }

    while (let op = dequeue(&child.port_out.cancellations)) {
      handle_child_cancellation(machine, child, op);
    }
  }
}
```

The parent can:
- Forward child operations to its own `port_out` (transparent passthrough)
- Intercept and handle child operations locally (virtualization)
- Transform operations (protocol translation)
- Aggregate multiple child operations

## Correctness Properties

1. **No data races**: Single writer per queue
2. **No allocation races**: Initiator owns AsyncOp lifetime
3. **No cancellation races**: Must wait for confirmation
4. **No queue overflow**: Intrusive queues + bounded pools
5. **No cross-thread corruption**: Explicit routing through event loop
6. **No resource leaks**: Hierarchical cleanup
7. **No lost completions**: Idempotent doorbell for standing ops

## Key Invariants

- AsyncOp memory valid until completion received
- Standing ops remain valid across multiple doorbells
- Queues are FIFO per source-destination pair
- Parent outlives all children
- Platform machine outlives app machine
