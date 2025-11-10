# Stream 6: Code Generation

Final stage of the async systems language compiler - transforms lowered IR into freestanding C11 code.

## Quick Start

```bash
# Build and run tests
make test

# Clean build artifacts
make clean
```

## What This Stream Does

Converts intermediate representation (IR) from the lowering stage into:

- **Header files** (.h) - Function declarations, type definitions
- **Source files** (.c) - Function implementations
- **Runtime header** (lang_runtime.h) - Shared type definitions

## Key Features

### ✅ Freestanding C11 Output
- Only uses `<stdint.h>`, `<stddef.h>`, `<stdbool.h>`, `<stdalign.h>`
- No hosted library dependencies
- Compiles with `-ffreestanding` flag
- Portable across all C11 compilers

### ✅ Type Translation
- `i8..i64, isize` → `int8_t..int64_t, ptrdiff_t`
- `u8..u64, usize` → `uint8_t..uint64_t, size_t`
- `bool` → `bool` (stdbool.h)

### ✅ State Machine Generation
- Computed goto for efficient dispatch
- Per-state variable storage
- Minimal frame size (only live variables)

### ✅ Debug Support
- `#line` directives for source mapping
- Preserves original source locations
- Readable, indented output

### ✅ Name Safety
- All user identifiers prefixed with `__u_`
- Prevents collisions with C standard library
- Consistent naming convention

## Directory Structure

```
stream6-codegen/
├── include/
│   ├── codegen.h         - Public code generation API
│   ├── ir.h              - IR interface (extended)
│   ├── type.h            - Type system interface
│   ├── coro_metadata.h   - Coroutine metadata
│   ├── error.h           - Error reporting
│   └── arena.h           - Memory allocation
├── src/
│   └── codegen.c         - Code generator implementation
├── test/
│   └── test_codegen.c    - Comprehensive unit tests
├── Makefile              - Build configuration
├── README.md             - This file
├── subsystem-doc.md      - Detailed implementation docs
└── interface-changes.md  - Interface modifications log
```

## Usage Example

```c
#include "codegen.h"
#include "ir.h"

// Setup
Arena arena;
ErrorList errors;
arena_init(&arena, 4096);
error_list_init(&errors, &arena);

// Configure code generator
CodegenContext ctx;
codegen_init(&ctx, &arena, &errors, "my_module");

// Open output files
FILE* header = fopen("my_module.h", "w");
FILE* source = fopen("my_module.c", "w");
FILE* runtime = fopen("lang_runtime.h", "w");

ctx.header_out = header;
ctx.source_out = source;

// Generate code
codegen_emit_runtime_header(runtime);
codegen_emit_module(ir_module, &ctx);

// Cleanup
fclose(header);
fclose(source);
fclose(runtime);
arena_free(&arena);
```

## Generated Code Example

**Input IR** (conceptual):
```
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

**Generated Header** (my_module.h):
```c
#ifndef __u_my_module_H
#define __u_my_module_H

#include "lang_runtime.h"

int32_t __u_add(int32_t __u_a, int32_t __u_b);

#endif
```

**Generated Source** (my_module.c):
```c
#include "my_module.h"

#line 1 "source.lang"
int32_t __u_add(int32_t __u_a, int32_t __u_b) {
#line 2 "source.lang"
    return (__u_a + __u_b);
}
```

## Testing

Six comprehensive unit tests:

1. **Identifier Prefixing** - Verifies `__u_` prefix handling
2. **Type Translation** - Tests all type mappings
3. **Simple Function** - End-to-end function generation
4. **Runtime Header** - Validates runtime definitions
5. **Assignment** - Tests statement generation
6. **C11 Compilation** - Compiles output with strict flags

### Test Coverage

- ✅ All primitive types
- ✅ Function signatures
- ✅ Expressions and statements
- ✅ Identifier prefixing
- ✅ Line directive emission
- ✅ C11 freestanding compliance

### Running Tests

```bash
make test
```

Expected output:
```
=== Code Generation Tests ===

Test: Identifier prefixing... PASSED
Test: Type translation... PASSED
Test: Simple function generation... PASSED
Test: Runtime header generation... PASSED
Test: Assignment generation... PASSED
Test: C11 freestanding compilation... PASSED

All tests passed!
```

## Interface Dependencies

### What We Consume

- **IR** (`ir.h`) - Lowered intermediate representation
- **Types** (`type.h`) - Type information for translation
- **Coroutine Metadata** (`coro_metadata.h`) - State machine data
- **Arena** (`arena.h`) - Memory allocation
- **Errors** (`error.h`) - Error reporting

### What We Produce

- **C Header Files** - Function declarations
- **C Source Files** - Function implementations
- **Runtime Header** - Shared definitions
- **Error Messages** - Code generation errors

### What We Don't Depend On

- AST structures (consume IR only)
- Parser/lexer internals
- Symbol tables (names from IR)
- String pool (strings already interned)

## Design Principles

1. **Simplicity**: IR → C is a direct translation, no analysis
2. **Correctness**: Generated code must compile cleanly
3. **Readability**: Output should be human-readable
4. **Debuggability**: Preserve source locations
5. **Portability**: Use only standard C11

## State Machine Generation

For async/coroutine functions, generates computed goto state machines:

```c
// State struct with union of per-state data
struct function_state {
    void* state;
    union {
        struct { int32_t __u_x; } state_0;
        struct { int32_t __u_y; } state_1;
    } data;
} machine;

// Initialize and dispatch
machine.state = &&function_state_0;
goto *machine.state;

// State labels
function_state_0:
    // Restore variables
    // Execute code
    // Update state
    machine.state = &&function_state_1;
    return;

function_state_1:
    // Next state...
```

### Why Computed Goto?

- **Fast**: Direct jump, no switch overhead
- **Simple**: Clear IR mapping
- **Standard**: GCC/Clang extension
- **Debuggable**: Each state is visible

## Performance

### Memory
- Arena-based allocation (no malloc during generation)
- Single-pass traversal
- Direct IR → C emission

### Speed
- O(n) complexity where n = IR node count
- Streaming output (no buffering)
- Minimal string copying

## Limitations

Current implementation is functional but has areas for future work:

1. **State Machine Resume**: Generates structure but needs complete resume logic
2. **Complex Types**: Basic struct/enum support, needs field layout
3. **Optimization**: No dead code elimination yet
4. **Cross-Module**: Single module at a time

See `subsystem-doc.md` for detailed future work.

## Documentation

- **README.md** (this file) - Quick reference
- **subsystem-doc.md** - Detailed implementation documentation
- **interface-changes.md** - Interface modifications and recommendations

## Integration with Other Streams

### Upstream (What We Need)

From **Stream 5 (Lowering)**:
- Complete IR with all node types populated
- Type information on expression nodes
- Source locations on all nodes

From **Stream 4 (Coroutine Analysis)**:
- Complete `CoroMetadata` structures
- Accurate frame sizes
- Live variable information

### Downstream (What We Enable)

Our output can be:
- Compiled with any C11 compiler
- Linked with other C code
- Debugged with standard tools (gdb, lldb)
- Optimized by C compiler

## Contributing

When modifying code generation:

1. Update tests first (test-driven development)
2. Ensure all tests pass
3. Verify generated code compiles with `-ffreestanding`
4. Update documentation
5. Add integration tests for new features

## License

Part of the async systems language compiler project.

## Support

For questions or issues:
1. Check `subsystem-doc.md` for detailed documentation
2. Review test cases for examples
3. Check `interface-changes.md` for interface details
