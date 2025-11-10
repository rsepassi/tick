# Integration Tests for Tick Compiler

This directory contains comprehensive integration tests for the Tick compiler pipeline. These tests verify that all compiler components work together correctly from source code to generated C11 code.

## Overview

The integration tests validate the complete compilation pipeline:
```
Tick Source → Lexer → Parser → Semantic → Coroutine → Lowering → Codegen → C11 Code
```

Each test suite focuses on different aspects of the pipeline integration:

## Test Suites

### 1. test_lexer_parser
**Tests:** Lexer + Parser integration
**Focus:** Token stream → AST construction

Tests the boundary between lexer and parser:
- Simple function parsing
- Async function with suspend points
- Try/catch error handling
- Defer/errdefer statements
- Struct definitions
- Syntax error detection
- Multiple functions in one module

**Run:**
```bash
make test_lexer_parser
./test_lexer_parser
```

### 2. test_semantic
**Tests:** Parser + Semantic Analysis integration
**Focus:** AST → Typed AST with symbol tables

Tests semantic analysis after parsing:
- Type checking expressions
- Type error detection
- Symbol resolution across functions
- Undefined symbol detection
- Function signature checking
- Struct type validation
- Error type propagation (Result types)
- Nested scope handling

**Run:**
```bash
make test_semantic
./test_semantic
```

### 3. test_coroutine
**Tests:** Semantic + Coroutine Analysis integration
**Focus:** Typed AST → CoroMetadata with state machine info

Tests coroutine transformation analysis:
- Simple async function analysis
- Live variable tracking across suspend points
- Multiple suspend points
- Async with defer blocks
- State struct sizing calculation
- Nested async function calls
- Error handling in async context

**Run:**
```bash
make test_coroutine
./test_coroutine
```

### 4. test_lowering
**Tests:** Coroutine + IR Lowering integration
**Focus:** AST + CoroMetadata → IR (Intermediate Representation)

Tests the lowering of AST to IR:
- Simple function lowering
- Async function state machine generation
- Control flow (if/else) lowering
- Defer statement lowering
- Loop lowering
- State persistence across suspend points
- Error handling lowering

**Run:**
```bash
make test_lowering
./test_lowering
```

### 5. test_codegen
**Tests:** IR + Code Generation integration
**Focus:** IR → C11 source code

Tests C code generation from IR:
- Simple function code generation
- State machine code generation
- Type translation to C types
- Control flow code generation
- Defer block code generation
- Error handling code generation
- #line directive generation

**Run:**
```bash
make test_codegen
./test_codegen
```

### 6. test_full_compile
**Tests:** Complete pipeline with C compiler verification
**Focus:** Source → C code → Compiled object file

Tests the full pipeline and verifies generated C compiles:
- Simple function compilation
- Multiple functions compilation
- Control flow compilation
- Struct types compilation
- Loop compilation
- Strict compilation flags verification

**Verification:** Generated C code is compiled with:
```bash
gcc -Wall -Wextra -Werror -std=c11 -ffreestanding
```

**Run:**
```bash
make test_full_compile
./test_full_compile
```

### 7. test_pipeline (Main End-to-End Tests)
**Tests:** Comprehensive end-to-end scenarios
**Focus:** Real-world Tick programs → Working C code

Comprehensive tests covering all major features:
- Simple functions (no async)
- Async functions with suspend points
- Error handling (try/catch)
- Defer/errdefer resource management
- Type checking complex expressions
- Symbol resolution across modules
- State machine generation for coroutines
- Nested async function calls
- Complex control flow (fibonacci)
- Mixed async and sync functions

**Run:**
```bash
make test_pipeline
./test_pipeline
```

## Running All Tests

To build and run all integration tests:

```bash
make all        # Build all tests
make test       # Run all tests
make clean      # Clean build artifacts
```

## Test Output

Each test reports:
- Test name and description
- Phase-by-phase progress (for pipeline tests)
- Pass/fail status
- Error messages if failures occur
- Final summary with counts

Example output:
```
==========================================
Integration Test Suite
==========================================

Running Lexer + Parser Integration...
  Test: Simple function parsing ... PASS
  Test: Async function parsing ... PASS
  ...

==========================================
Tests run: 7
Tests passed: 7
Tests failed: 0
==========================================
```

## Key Integration Test Scenarios

### Success Cases

1. **Simple Functions**
   - Basic arithmetic operations
   - Function calls
   - Local variables
   - Control flow (if/else, loops)

2. **Async Functions**
   - Suspend points
   - Live variable tracking
   - State machine generation
   - Resume operations

3. **Error Handling**
   - Try/catch blocks
   - Result types (!T)
   - Error propagation
   - Errdefer blocks

4. **Resource Management**
   - Defer blocks
   - Errdefer blocks
   - Cleanup ordering

5. **Type System**
   - Primitive types (i32, u64, bool, etc.)
   - Struct types
   - Array/slice types
   - Pointer types
   - Function types

### Error Cases

Tests also verify error detection:
- Syntax errors
- Type mismatches
- Undefined symbols
- Invalid async operations
- Scope violations

## Generated Files

Tests generate C files during execution:
- `generated_*.c` - Generated C source files
- `pipeline_*.c` - Pipeline test outputs
- `*.o` - Compiled object files (when testing compilation)

These files are automatically cleaned with `make clean`.

## Requirements

- GCC compiler with C11 support
- Make build system
- All stream implementations (streams 1-7)

## Build System

The Makefile:
- Links all stream implementations
- Provides individual test targets
- Runs tests with proper error reporting
- Cleans all generated files

Include paths:
- `/interfaces` - Shared interface definitions
- `/stream1-lexer/include` - Lexer implementation
- `/stream2-parser` - Parser implementation
- `/stream3-semantic/include` - Semantic analysis
- `/stream4-coroutine` - Coroutine analysis
- `/stream5-lowering` - IR lowering
- `/stream6-codegen/include` - Code generation
- `/stream7-infrastructure` - Core utilities

## Expected Test Results

All tests should pass when:
- All stream implementations are complete
- Interfaces are correctly implemented
- Data flows properly between components
- Generated C code is valid C11

Tests are designed to fail early and provide clear error messages when component integration fails.

## Troubleshooting

### Test fails at parse phase
- Check lexer token generation
- Verify parser grammar rules
- Check AST node construction

### Test fails at semantic phase
- Check symbol table construction
- Verify type inference
- Check scope management

### Test fails at coroutine phase
- Check async function detection
- Verify live variable analysis
- Check state struct generation

### Test fails at lowering phase
- Check IR node generation
- Verify control flow translation
- Check state machine lowering

### Test fails at codegen phase
- Check C code emission
- Verify type translation
- Check generated code syntax

### Generated C fails to compile
- Check C11 compliance
- Verify include directives
- Check type declarations
- Enable `-v` flag on gcc for details

## Contributing

When adding new features to Tick:

1. Add integration tests for the feature
2. Test all pipeline stages
3. Verify generated C compiles
4. Add both success and error cases
5. Update this README with new test descriptions

## License

Part of the Tick compiler project.
