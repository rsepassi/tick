
# Async Systems Language Compiler - Implementation Guide

## Overview

This document provides detailed implementation specifications for the async systems language compiler. The compiler translates source code to portable C11, using state machines for async operations.

## Lexical Specification

### Tokens

#### Integer Literals

- **Formats**:
  - Decimal: `123`, `1_000_000`
  - Hexadecimal: `0x1234`, `0xDEAD_BEEF`
  - Octal: `0o755`, `0o123`
  - Binary: `0b1010`, `0b1111_0000`
- **Underscores**: Allowed anywhere except at start/end or adjacent to prefix
- **No suffix notation**: Type determined by context and inference
- **Type handling**: Lex as largest possible type, semantic analysis narrows

#### String Literals

- **Encoding**: UTF-8
- **Escape sequences**: `\n`, `\t`, `\r`, `\\`, `\"`, `\xHH`, `\uHHHH`
- **No multiline strings**: Use `embed_file` for that use case
- **Runtime representation**:
  - Size-inferred arrays: null-terminated C strings
  - Otherwise: struct with length and `u8` array

#### Identifiers

- **Rules**: Start with letter or underscore, contain alphanumeric and underscore
- **Case sensitive**: Yes
- **Maximum length**: 255 characters (implementation defined)
- **Reserved patterns**: `__*` (double underscore prefix) for compiler internals
- **C keyword conflicts**: Handled by prefixing with `__u_` in generated code

#### Comments

- **Syntax**: `# single line comment`
- **No multiline comments**
- **Preserved in AST**: For auto-formatter support

#### Keywords

```
async suspend resume try catch defer errdefer pub import 
let var return if else for switch case default break continue 
packed volatile embed_file
```

#### Operators

- Arithmetic: `+` `-` `*` `/` `%`
- Bitwise: `&` `|` `^` `<<` `>>` `~`
- Logical: `&&` `||` `!`
- Comparison: `<` `>` `<=` `>=` `==` `!=`
- Assignment: `=` `+=` `-=` `*=` `/=` `%=` `&=` `|=` `^=` `<<=` `>>=`
- Other: `.` `->` `[` `]` `(` `)` `{` `}` `:` `;` `,` `..` `&` `*`

## Parser Specification

### Grammar Structure

- **Parser type**: LALR(1) table-driven
- **Uniform syntax**: Everything uses `let` bindings

### Operator Precedence (Simplified)

1. **Postfix** (highest): `.` `[]` `()`
1. **Prefix**: `-` `!` `~` `*` `&`
1. **Multiplicative**: `*` `/` `%`
1. **Additive**: `+` `-`
1. **Comparison**: `<` `>` `<=` `>=` `==` `!=`
1. **Assignment** (lowest): `=` `+=` `-=` etc.

**Ambiguity rules** (parser errors):

- Bitwise ops (`&` `|` `^` `<<` `>>`) require parentheses when mixed with other operators
- Logical ops (`&&` `||`) require parentheses when mixed with other operators
- Cannot chain different comparison operators without parentheses
- Example errors:
  - `a & b == 0` → Must write `(a & b) == 0`
  - `a && b || c` → Must write `(a && b) || c`
  - `a < b < c` → Must write `(a < b) && (b < c)`

### Declarations (all use `let` syntax)

#### Functions

```
let add = fn(x: i32, y: i32) -> i32 {
    return x + y;
};

pub let exported_func = fn() { ... };
```

#### Types

```
let Point = struct {
    x: i32,
    y: i32
};

let Point3D = struct packed {
    x: i32,
    y: i32,
    z: i32
};

let Status = enum(u8) {
    OK = 0,
    ERROR = 1,
    PENDING = 2
};

# Automatic tag
let Result = union {
    ok: i32,
    error: ErrorCode
};

# Explicit tag
let Result = union(StatusEnum) {
    ok: i32,
    error: ErrorCode
};
```

#### Constants and Variables

```
# Module level (compile-time constant)
let MAX_SIZE = 1024;

# Function level
let config = loadConfig();  # immutable binding
var counter = 0;            # mutable variable
```

### Control Flow

#### For Loops (Go-style, no parentheses, braces required)

```
# Range iteration
for i in 0..10 {
    # i goes from 0 to 9 (exclusive end)
}

# With continue clause
for i in 0..10 : updateProgress() { |i|
    # updateProgress() runs after each iteration
}

# Collection iteration  
for item in collection { |item|
    processItem(item);
}

# While-style
for condition {
    # ...
}

# Infinite with continue clause
for : tick() {
    # tick() runs after each iteration
}
```

#### Switch Statements

```
switch value {
    case 1, 2, 3:
        # No fallthrough, implicit break
        handleLow();
    case 4:
        handleMid();
    default:
        handleOther();
}
```

#### Computed Goto Pattern (for VM dispatch)

```
while switch state {
    case INIT:
        state = PROCESS;
        continue switch state;  # Jump to switch with new state
    case PROCESS:
        # ...
}
```

## Type System

### Primitive Types

- Signed integers: `i8`, `i16`, `i32`, `i64`, `isize` (ptrdiff_t)
- Unsigned integers: `u8`, `u16`, `u32`, `u64`, `usize` (size_t)
- Boolean: `bool`
- No floating point types

### Composite Types

- Arrays: `[10]i32` (fixed size)
- Slices: `[]i32` (runtime length + pointer)
- Structs: Support `packed` attribute and alignment
- Enums: Explicitly sized with underlying type `enum(u8)`
- Unions: Tagged (automatic or explicit enum tag)

### Special Types

- Result: `ErrorEnum!ValueType`
- Option: `Option(T)` equivalent to `union { some: T, none: void }`
- Function pointers: `fn(i32, i32) -> i32`

### Type Qualifiers

- `volatile` for hardware registers

### Type Construction

```
# Result type construction
return ErrorEnum.InvalidInput;  # Error case
return value;                   # Success case

# Option type usage
let maybe = Option(i32) { some: 42 };
```

## Module System

### File Organization

- **One module per file**
- **File name becomes module name**

### Import Syntax

```
let math = import "math";
let result = math.sqrt(16);
```

### Visibility

```
pub let exported_func = fn() { ... };  # Public
let internal_func = fn() { ... };      # Private (default)
```

## Async/Coroutine System

### Key Concepts

- **Caller-determined concurrency**: Any function can be called sync or async
- **Async calls**: Use `async` keyword at call site
- **Suspend points**: Bare `suspend` statement in function
- **Resume**: `resume coro;` statement
- **Await**: Library function `std.await(future)`

### Examples

```
let worker = fn() {
    processStart();
    suspend;
    processEnd();
};

let main = fn() {
    # Synchronous call (blocking)
    worker();
    
    # Asynchronous call (returns future)
    let future = async worker();
    
    # Wait for completion
    std.await(future);
    
    # Manual resume
    let coro = async longTask();
    resume coro;
};
```

### Platform Interface

```c
typedef struct {
    u32 opcode;
    i32 result;
    void* ctx;
    void (*callback)(struct async_t*);
    void* parent;  // For tracing
    u64 req_id;    // For tracing
} async_t;
```

Platform processes submissions/cancellations via ring buffers or linked lists.

## Error Handling

### Result Types

```
let divide = fn(a: i32, b: i32) -> MathError!i32 {
    if b == 0 {
        return MathError.DivideByZero;
    }
    return a / b;
};
```

### Try/Catch

```
try {
    let result = divide(10, 0);
    process(result);
} catch |err| {
    handleError(err);
}

# Try without catch propagates error
let result = try riskyOperation();
```

### Cleanup

```
let processFile = fn(path: []u8) -> Error!void {
    let file = openFile(path);
    defer closeFile(file);      # Always runs at scope exit
    
    let buffer = allocate(1024);
    errdefer free(buffer);       # Only runs if error returned
    
    # ... process file ...
};
```

## Memory Management

### Manual Management

- All allocation through explicit allocator interfaces
- Allocators passed as parameters, not global
- Library functions for allocation/deallocation

### Address Operations

```
let value = 42;
let ptr = &value;        # Take address
let deref = *ptr;        # Dereference
```

### Slices

```
let allocator = getDefaultAllocator();
let buffer = allocator.alloc(u8, 1024);  # Returns []u8
defer allocator.free(buffer);
```

## Builtins

### embed_file

```
let data = embed_file("assets/config.json");  # Type: []u8
```

Files must be in same directory as source file.

## Code Generation

### C Output Structure

```
module.odin → module.h + module.c
              └── lang_runtime.h (shared runtime)
```

### Identifier Prefixing

All user identifiers prefixed with `__u_` to avoid C keyword conflicts.

### State Machine Naming

```c
enum {
    STATE_0,  // Initial
    STATE_1,  // After first suspend
    STATE_2,  // After second suspend
    // ...
};
```

### Coroutine Frame (Tagged Union)

```c
typedef struct {
    enum { STATE_0, STATE_1, STATE_2 } tag;
    union {
        struct {
            // Locals live at STATE_0
            int32_t __u_x;
        } state_0;
        struct {
            // Locals live at STATE_1
            int32_t __u_y;
            uint8_t* __u_buffer;
        } state_1;
        // ...
    } data;
} coro_frame_worker;
```

### Source Mapping

`#line` directives on every line corresponding to user source for debugging.

### Compiler Flags

Always test with:

```
-Wall -Werror -Wextra -fwrapv -ffreestanding 
-fno-strict-aliasing -std=c11 -Wpedantic -Wvla
```

## Implementation Modules

### 1. lexer

- Input: UTF-8 source text
- Output: Token stream with source locations
- Preserve comments for formatter

### 2. parser

- Input: Token stream
- Output: Untyped AST using uniform `let` syntax
- LALR(1) table-driven parser
- Enforce precedence/ambiguity rules

### 3. resolver

- Input: Untyped AST
- Output: Symbol tables, module dependency graph
- Resolve imports and check visibility

### 4. typeck

- Input: AST with symbols
- Output: Typed AST
- Infer types for literals and local variables
- Check Result type usage
- Validate volatile qualifiers

### 5. coro_analyze

- Input: Typed AST
- Output: Coroutine metadata
- Find suspend points
- Compute liveness across suspends
- Design tagged union frame layout
- Calculate `coroSize()`

### 6. lower

- Input: Typed AST with coroutine metadata
- Output: Structured IR
- Transform coroutines to state machines
- Lower error handling (try/catch → if/goto)
- Lower cleanup (defer/errdefer)
- Lower computed goto patterns

### 7. codegen

- Input: IR
- Output: C11 code
- Generate state machine implementations
- Apply `__u_` prefixing
- Insert `#line` directives
- Generate async platform interface code

## Testing Strategy

1. **Unit tests** for each module (lexer through codegen)
1. **Round-trip tests**: Source → C → compile → execute
1. **Platform mock**: Simple async runtime for testing
1. **Compliance tests**: Verify C11 freestanding compatibility
1. **Size tests**: Verify coroutine frame size calculations
