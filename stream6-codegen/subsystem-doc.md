# Stream 6: Code Generation Subsystem Documentation

## Overview

The Code Generation subsystem is the final stage of the async systems language compiler pipeline. It transforms lowered IR into clean, readable, freestanding C11 code that can be compiled with any standard-compliant C compiler.

## Architecture

### Core Components

1. **CodegenContext** (`codegen.h`)
   - Maintains generation state across the compilation unit
   - Manages output streams (header and source files)
   - Tracks indentation level for proper formatting
   - Controls line directive emission
   - Holds arena allocator and error list

2. **Code Generator** (`codegen.c`)
   - Main entry point: `codegen_emit_module()`
   - Produces module.h + module.c pairs
   - Generates lang_runtime.h with shared definitions
   - Implements complete IR → C translation

### Output Structure

For each module, the code generator produces:

```
module_name.h      - Header with function declarations
module_name.c      - Implementation with function definitions
lang_runtime.h     - Shared runtime definitions (one per compilation)
```

## C Translation Strategy

### Type Mapping

The compiler maps language types to standard C11 types:

| Language Type | C11 Type      | Header     |
|--------------|---------------|------------|
| i8           | int8_t        | stdint.h   |
| i16          | int16_t       | stdint.h   |
| i32          | int32_t       | stdint.h   |
| i64          | int64_t       | stdint.h   |
| isize        | ptrdiff_t     | stddef.h   |
| u8           | uint8_t       | stdint.h   |
| u16          | uint16_t      | stdint.h   |
| u32          | uint32_t      | stdint.h   |
| u64          | uint64_t      | stdint.h   |
| usize        | size_t        | stddef.h   |
| bool         | bool          | stdbool.h  |
| void         | void          | (builtin)  |

**Rationale**: Using standard C types ensures portability and allows the compiler to generate truly freestanding code that works on any platform.

### Identifier Prefixing

All user identifiers are prefixed with `__u_` to avoid name collisions with:
- C standard library identifiers
- C keywords
- Compiler built-ins
- Platform-specific macros

**Implementation**: `codegen_prefix_identifier()` adds the prefix if not already present, preventing double-prefixing.

**Example**:
```
Language: fn add(a: i32, b: i32) -> i32
C Output: int32_t __u_add(int32_t __u_a, int32_t __u_b)
```

### Freestanding C11 Requirements

Generated code uses only freestanding headers:
- `<stdint.h>` - Fixed-width integer types
- `<stddef.h>` - Basic definitions (size_t, ptrdiff_t, NULL)
- `<stdbool.h>` - Boolean type
- `<stdalign.h>` - Alignment support

**Verification**: Tests compile with `-ffreestanding` flag to ensure compliance.

## State Machine Generation

### Computed Goto Strategy

For coroutines and async functions, we generate state machines using computed goto for maximum efficiency:

```c
struct function_state {
    void* state;        // Current state label
    union {
        struct { /* state 0 fields */ } state_0;
        struct { /* state 1 fields */ } state_1;
        // ... more states
    } data;
} machine;

// Initialize to first state
machine.state = &&function_state_0;

// Dispatch to current state
goto *machine.state;

function_state_0:
    // Restore live variables from machine.data.state_0
    // Execute state 0 code
    // Update state and suspend/return

function_state_1:
    // Restore live variables from machine.data.state_1
    // Execute state 1 code
    // ...
```

### Why Computed Goto?

1. **Performance**: Direct jump to state label, no switch overhead
2. **Simplicity**: Clear mapping from IR to C
3. **GCC/Clang Support**: Both major compilers support this extension
4. **Readability**: Each state is a labeled block of code

### State Struct Layout

Each suspend point gets its own struct in the union containing only live variables at that point:

```c
union {
    struct {
        int32_t __u_x;
        int32_t __u_y;
    } state_0;

    struct {
        int32_t __u_result;
    } state_1;
} data;
```

**Optimization**: Only live variables are stored, minimizing frame size.

## Debug Support

### Line Directives

The code generator emits `#line` directives to map generated C code back to original source locations:

```c
#line 42 "myfile.lang"
int32_t __u_x = 10;
```

This enables:
- Correct error messages pointing to original source
- Debugger integration (step through original code)
- Stack traces with original locations

**Control**: Can be disabled via `ctx->emit_line_directives = false` for debugging the generator itself.

### Generated Code Readability

The generator produces indented, readable C code:
- 4-space indentation
- Blank lines between functions
- Comments for complex constructs
- Predictable naming conventions

## Testing Strategy

### Unit Tests

Six comprehensive tests in `test/test_codegen.c`:

1. **test_identifier_prefixing**
   - Verifies `__u_` prefix is added correctly
   - Tests idempotency (no double-prefixing)

2. **test_type_translation**
   - Validates all primitive type mappings
   - Ensures correct C type selection

3. **test_simple_function**
   - End-to-end test: IR → C generation
   - Verifies function signature and body
   - Checks identifier prefixing in context

4. **test_runtime_header**
   - Validates lang_runtime.h generation
   - Ensures all required headers included
   - Checks runtime type definitions

5. **test_assignment**
   - Tests statement generation
   - Verifies expression translation

6. **test_c11_compilation**
   - **Critical test**: Compiles generated code
   - Uses `-Wall -Werror -Wextra -std=c11 -ffreestanding`
   - Ensures output is valid, warning-free C11

### Mock IR Infrastructure

Tests use mock implementations of:
- `ir_alloc_node()` - Creates IR nodes from arena
- `type_init_primitive()` - Initializes type structures
- Arena allocator - Full working implementation
- Error list - Simplified mock for testing

This allows testing without depending on other compiler streams.

### Compilation Verification

The test suite actually invokes GCC to compile generated code:

```c
system("gcc -Wall -Werror -Wextra -std=c11 -ffreestanding "
       "-I/tmp -c /tmp/compile_test.c -o /tmp/compile_test.o");
```

**Result**: Ensures generated code is not only syntactically correct but compiles cleanly.

## Code Generation Flow

### Module Generation

1. Emit header guard and includes (header file)
2. Emit `#include "lang_runtime.h"` (header file)
3. Emit source includes (source file)
4. For each function in module:
   - Generate declaration in header
   - Generate definition in source
   - Handle state machines if coroutine
5. Close header guard

### Function Generation

1. Emit line directive with source location
2. Generate signature with prefixed names
3. Open function body
4. If state machine:
   - Generate state struct
   - Generate computed goto dispatcher
   - Generate state labels and code
5. Else:
   - Generate regular function body
6. Close function body

### Expression/Statement Translation

Recursive descent through IR tree:
- **Literals**: Emit directly
- **Variable refs**: Emit with prefix
- **Binary ops**: Emit with parentheses for precedence
- **Calls**: Emit with prefixed function name
- **Assignments**: Emit with prefixed target
- **Control flow**: Emit as C control structures

## Runtime Support

The `lang_runtime.h` header provides:

### Result Types
```c
#define RESULT_OK(T) struct { bool is_ok; T value; }
#define RESULT_ERR(E) struct { bool is_ok; E error; }
```

### Option Types
```c
#define OPTION(T) struct { bool has_value; T value; }
```

### Coroutine Support
```c
typedef struct {
    void* state;
    void* data;
} __u_Coroutine;
```

These are implemented as macros to allow type-generic usage while maintaining C11 compatibility.

## Limitations and Future Work

### Current Limitations

1. **State Machine Resume**: Implementation generates state struct and labels but doesn't fully implement resume logic yet
2. **Defer Statements**: Basic implementation present, needs integration testing
3. **Switch/For Loops**: IR nodes defined but emission not fully implemented
4. **Complex Types**: Struct/enum emission is basic, needs field layout

### Future Enhancements

1. **Optimization**: Dead code elimination in generated C
2. **Better Pretty-Printing**: Column alignment, comment generation
3. **Cross-Module References**: Handle function calls across modules
4. **Inline Expansion**: Optional inlining for small functions
5. **Custom Allocators**: Support different allocation strategies

## Performance Characteristics

### Memory Usage

- **Arena-based allocation**: All strings allocated from arena
- **No dynamic allocation**: During generation (only from arena)
- **Minimal copying**: Direct IR traversal, emit to stream

### Generation Speed

- **Linear complexity**: O(n) where n = IR node count
- **Single-pass**: No multi-pass optimization (yet)
- **Streaming output**: Writes directly to files, no buffering

## Integration Points

### Inputs (What We Consume)

- **IR nodes** (`ir.h`): Lowered intermediate representation
- **Type information** (`type.h`): Type metadata for translation
- **Coroutine metadata** (`coro_metadata.h`): State machine info
- **Arena allocator** (`arena.h`): Memory allocation
- **Error list** (`error.h`): Error reporting

### Outputs (What We Produce)

- **Header files** (.h): Function declarations
- **Source files** (.c): Function implementations
- **Runtime header**: Shared type definitions
- **Errors**: Code generation errors in ErrorList

### No Dependencies On

- Parser/lexer internals
- AST structures (consumes IR only)
- Symbol table (names come from IR)
- String pool (strings in IR are already interned)

## Build Instructions

```bash
# Build and run tests
make test

# Clean build artifacts
make clean

# Verify freestanding compilation
make verify-freestanding
```

## Example Output

### Input IR (conceptual)
```
function add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

### Generated Header (test.h)
```c
/* Generated code - do not edit */
#ifndef __u_test_H
#define __u_test_H

#include "lang_runtime.h"

int32_t __u_add(int32_t __u_a, int32_t __u_b);

#endif /* __u_test_H */
```

### Generated Source (test.c)
```c
/* Generated code - do not edit */
#include "test.h"

#line 1 "test.lang"
int32_t __u_add(int32_t __u_a, int32_t __u_b) {
#line 2 "test.lang"
    return (__u_a + __u_b);
}
```

## Conclusion

The code generation subsystem successfully translates high-level async language IR into efficient, portable, freestanding C11 code. It handles type translation, state machine generation, and debugging support while producing clean, readable output that compiles without warnings under strict compiler settings.
