# Async Systems Language Compiler Design

## Overview

Source: ASCII → Target: C11
Domain: Systems programming, embedded systems
Philosophy: Extreme portability via pure state machines, simple over complex

## Core Architecture

**Inside view:** Straightline coroutines (async/suspend/resume)
**Outside view:** C ABI state machines with async struct interface
**Platform:** Abstracted via ring buffers/linked lists for submissions/completions/cancellations

-----

## Compilation Pipeline

### 1. Lexer

- ASCII input
- Token stream output
- Keywords: async, suspend, resume, try, catch, defer, errdefer, pub, import, struct, enum, union, fn, let, var, return, if, else, for, switch, case, default, break, continue, while, packed, volatile
- Operators: arithmetic, bitwise, logical, assignment, comparison
- Literals: integers, strings, booleans
- Identifiers, punctuation

### 2. Parser

- LALR(1) grammar (table-driven)
- Output: Untyped AST
- Modules, imports, declarations (fn, struct, enum, union, const, static)
- Struct attributes: packed, alignment
- Enum: explicitly sized with integer type (e.g., enum(u8), enum(i32))
- Variable qualifiers: volatile
- Statements: let, var, assignment, if, for, switch/case/default, return, block, expression statements
- Cleanup: defer, errdefer (for cleanup and “goto fail” patterns)
- Computed goto pattern: while switch expr, continue switch expr (for VM dispatch loops)
- Expressions: literals, variables, calls, operators, field access, array index, cast
- Special: async expressions, suspend statements, resume calls, try/catch blocks

### 3. Semantic Analysis

**3a. Module Resolution**

- Build module graph (single compilation unit, multiple modules)
- Resolve imports
- Check visibility (pub)
- Build symbol tables per module

**3b. Type Checking**

- Types: i8, i16, i32, i64, u8, u16, u32, u64, isize, usize, bool
  - isize: pointer-diff sized (arch dependent)
  - usize: pointer sized (arch dependent)
- Bitfields, arrays, slices (fat pointer: ptr + len)
- Structs (with packed attribute, alignment specs), enums (explicitly sized with integer type), tagged unions
- Variable qualifiers: volatile
- Error union types: E!T (error type E OR value type T), shorthand: !T (default error)
- Optional types: ?T
- Simple type inference (local variables, return types when unambiguous)
- Type checking expressions, statements, function signatures

**3c. Coroutine Analysis**

- Identify coroutine functions (those containing async/suspend/resume)
- Build control flow graph per function
- Find suspend points
- Liveness analysis: determine which locals are live across each suspend point
- Compute frame layout as tagged union:
  - Tag field: current state
  - Union of structs, one per state (between suspend points)
  - Each state struct contains only locals live at that point
- Compute coroutine size: sizeof(tagged union)
- Upper bound if recursion or dynamic behavior
- Generate coroSize() constant

### 4. IR Generation/Lowering

**4a. AST → IR**

- Structured control flow (if/then/else, loops, switch)
- Explicit temporaries
- No compound expressions
- Three-address code style

**4b. Coroutine Transformation**

- Each coroutine function becomes a state machine
- Suspend points → state labels
- Local variables → coroutine frame (tagged union)
  - Tag: current state enum
  - Union: one struct per state, containing live locals
- Parent-child relationship: nested calls run to completion
- Concurrent only via explicit async
- State machine inlining opportunities

**4c. Cleanup Lowering**

- defer → code executed at scope exit (success or error paths)
- errdefer → code executed only on error paths
- Track cleanup stack per scope
- Generate cleanup calls before return/break/continue/error propagation

**4d. Error Handling Lowering**

- try/catch blocks → if (result.is_error) goto catch_block
- Error union type unpacking (E!T → tagged union with error/value discriminant)

**4e. Computed Goto Pattern Lowering**

- while switch expr → loop with switch on expr (for VM dispatch loops)
- continue switch expr → update switch variable and continue to loop top
- Enables indirect jump patterns without goto keyword

**4g. Platform Abstraction**

- Identify I/O operations
- Generate async struct instances (opcode, result, ctx, callback)
- Generate submission/completion/cancellation queue interactions
- Standard library ops: time, RNG, network, block, filesystem

### 5. C Code Generation

**5a. Type Translation**

- sized integers → stdint.h types (int32_t, uint64_t, etc)
- usize → size_t, isize → ptrdiff_t
- structs (packed with **attribute**((packed)), alignment with _Alignas), enums (with explicit underlying integer type), unions → C equivalents
- volatile qualifier → C volatile
- slices → struct { T* ptr; size_t len; }
- coroutine frames → tagged unions (enum tag + union of state structs)

**5b. Function Generation**

- Regular functions → C functions
- Coroutine functions → state machine functions with computed goto
- State machine: `goto *machine->state;` (C11 computed goto)
- State labels: `state_N:` for each suspend point
- Resume logic: save state (tag), return; on resume, goto saved state
- Thorough use of #line directives for source mapping

**5c. Memory Operations**

- Checked arithmetic: __builtin_add_overflow, etc (or manual checks)
- Allocator calls via interface

**5d. Cleanup Generation**

- defer → cleanup code before all exit points (return, break, continue, scope end)
- errdefer → cleanup code before error returns/propagation only
- Track cleanup stack, generate in reverse order (LIFO)

**5e. Computed Goto Pattern Generation**

- while switch expr → for(;;) { switch(expr) { … } }
- continue switch expr → expr = new_value; continue;
- Enables indirect jumps for VM interpreters without goto

**5f. Platform Interface**

- async struct definition
- Ring buffer/linked list manipulation code
- Callback registration
- Tracing support (parent pointer, request id for Perfetto)

**5g. Output Structure**

- Per module: 1 .h file (declarations) + 1 .c file (implementations)
- Shared: 1 common .h file for language runtime definitions
- Freestanding headers only: stdint.h, stdbool.h, stddef.h, stdalign.h
- Include guards
- Forward declarations
- Type definitions
- Function declarations (in .h)
- Function implementations (in .c)
- State machine implementations (in .c)
- Thorough #line directives throughout for debugging

-----

## Data Structures

### Symbol Table

- Nested scopes (module, function, block)
- Name → Type + Visibility + Location

### Type System

- Type representation (primitive, composite, pointer, slice)
- Type equality

### AST Nodes

- Declarations, statements, expressions
- Metadata: source location, type annotations

### IR Nodes

- Structured control flow (if, for, switch, while switch)
- Cleanup constructs (defer, errdefer)
- Instructions (assign, binop, call, return, suspend, resume, continue switch)
- CFG per function
- Coroutine metadata (suspend points, frame layout as tagged union)

### Coroutine Frame

- Tagged union: enum tag (state) + union of structs (one per state)
- Each state struct contains only locals live across that suspend point
- Size computation for coroSize()

### Platform Operations

- async struct: opcode (u32), result (u32), ctx (void*), callback (fn ptr), parent, req_id
- Operation-specific structs embedding async struct

-----

## Module Summary

1. **lexer**: ASCII → tokens
1. **parser**: tokens → AST (LALR(1))
1. **resolver**: AST → symbol tables + module graph
1. **typeck**: AST + symbols → typed AST + type errors
1. **coro_analyze**: typed AST → coroutine metadata (sizes, suspend points, liveness)
1. **lower**: typed AST → IR
1. **codegen**: IR → C11 code

-----

## Key Design Choices

- **Simplicity:** No generics, no floats, LALR(1) parsing, structured IR, direct C generation, no goto keyword
- **Portability:** Pure state machines, platform abstraction via queues, freestanding headers only
- **Debuggability:** Human-readable C output, #line directives, tracing support built-in
- **Efficiency:** Tagged union frames (only live locals per state), computed goto state machines
- **Cleanup:** defer/errdefer for resource management and error handling patterns
- **Control flow:** while switch / continue switch for VM dispatch patterns without goto
- **Concurrency:** Explicit (async keyword), transformed to state machines
- **ABI:** Stable C interface for state machine integration
- **Memory:** Caller-provided coroutine memory, allocator interface, manual management
- **Types:** Explicitly sized enums, arch-dependent usize/isize
