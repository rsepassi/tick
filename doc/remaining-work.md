# Remaining Work for Production Readiness

**Last Updated:** 2025-11-10
**Status:** ⚠️ BASIC COMPILATION WORKING - Type system and async features incomplete

---

## Current State

The compiler pipeline now has a working end-to-end flow but **cannot correctly compile programs yet** due to type system bugs and incomplete features.

**Working compiler executable:** `compiler/tickc` (300KB binary)

**Current output for simple function:**
```c
// Input: let add = fn(x: i32, y: i32) -> i32 { return x + y; };
// Generated C:
void __u_add(void __u_x, void __u_y) {
}
```

**Issues:**
- Parameter types show as "void" instead of "i32" (type resolution bug)
- Return type shows as "void" instead of "i32" (type resolution bug)
- Function body is empty (statement lowering/codegen issue)

---

## What Actually Works

### Compiler Infrastructure ✅
- **Compiler driver:** `compiler/tickc` executable exists and runs
- **Command-line interface:** `-o`, `-emit-c`, `-v`, `-h` options work
- **File I/O:** Reads `.tick` files, writes `.c`/`.h` files
- **Error reporting:** Multi-phase error collection and display
- **Build system:** Makefile builds entire pipeline

### Lexer ✅
- **Status:** Fully functional (33/33 tests pass)
- **Features:** All tokens, keywords, operators, literals
- **Recent fix:** Added `#` line comment support (tick syntax)

### Parser ✅
- **Status:** Functional (25/26 tests pass, 119 shift/reduce conflicts)
- **Recent fixes:**
  - Declaration collection (AstNodeList with proper counts)
  - Parameter collection (AstParamList with proper counts)
  - Identifier name extraction (using interned strings)
- **Working:** Parses all language constructs, builds AST

### Semantic Analysis ⚠️
- **Name resolution:** Working (resolves symbols, scopes)
- **Type checking:** Infrastructure working but bugs remain
- **Recent fixes:**
  - Sets `->type` field on parameter type nodes
  - Sets `->type` field on return type nodes
- **Problem:** Type nodes may not be constructed correctly by parser

### IR Lowering ⚠️
- **Infrastructure:** Complete
- **Function lowering:** Basic structure works
- **Recent fixes:** Extract return type from correct AST field
- **Problems:**
  - Statement lowering may not emit instructions
  - Expression lowering needs verification
  - Async transformation not implemented

### Code Generation ⚠️
- **Infrastructure:** Complete (~560 LOC)
- **Type translation:** Primitive types work
- **Function signatures:** Generated correctly (with name prefixing)
- **Problems:**
  - Function bodies empty (no instructions emitted)
  - Need to verify instruction emission pipeline

### Epoll Async Runtime ✅
- **Location:** `examples/runtime/`
- **Status:** Fully functional and tested
- **Tests:** 3/3 passing (timer, pipe I/O, TCP echo)

---

## Critical Bugs to Fix

### 1. Type Resolution Bug

**Problem:** Parameter and return types not being populated correctly.

**Symptoms:**
- All parameter types emit as "void"
- Return types emit as "void"

**Likely Causes:**
- Parser may not be creating type AST nodes correctly
- Type nodes may be missing when passed to type checker
- Type resolution happens but doesn't store results in right place

**Investigation Needed:**
- Verify `param(P)` grammar rule creates proper type nodes
- Check if type nodes have correct kind (AST_TYPE_PRIMITIVE, etc.)
- Trace type resolution through `resolve_type_from_node`
- Verify type is stored in parameter's `type->type` field

**Files:**
- `stream2-parser/grammar.y` - Type node creation in grammar
- `stream3-semantic/src/typeck.c` - Type resolution logic
- `stream5-lowering/src/lower.c` - Type extraction from AST

---

### 2. Empty Function Bodies

**Problem:** Generated C functions have empty bodies.

**Symptoms:**
```c
void __u_add(void __u_x, void __u_y) {
}  // No function body statements
```

**Likely Causes:**
- Statement lowering not emitting IR instructions
- Basic block not being populated
- Codegen not iterating over instructions
- Instructions exist but emission is broken

**Investigation Needed:**
- Check if `lower_stmt` creates instructions
- Verify instructions are added to basic block
- Check if basic block has instruction count > 0
- Verify `emit_instruction` is called for each instruction

**Files:**
- `stream5-lowering/src/lower.c` - Statement and expression lowering
- `stream6-codegen/src/codegen.c` - Instruction emission

---

## Remaining Implementation Work

### 1. Fix Type System (Critical)

**What's Needed:**
- Debug type node creation in parser
- Ensure all type nodes have correct AST kind
- Verify type resolution stores results correctly
- Test with simple function: `fn(x: i32) -> i32`

**Acceptance Criteria:**
```c
// Input: let add = fn(x: i32, y: i32) -> i32 { ... };
// Output:
int32_t __u_add(int32_t __u_x, int32_t __u_y) {
    // (body may still be incomplete)
}
```

---

### 2. Complete Expression & Statement Lowering (Critical)

**What's Needed:**
- Verify expression lowering creates IR values
- Verify statement lowering creates IR instructions
- Debug why instructions aren't being added to blocks
- Test with: `return x + y;`

**Acceptance Criteria:**
```c
// Input: let add = fn(x: i32, y: i32) -> i32 { return x + y; };
// Output:
int32_t __u_add(int32_t __u_x, int32_t __u_y) {
    int32_t t0 = __u_x + __u_y;
    return t0;
}
```

---

### 3. Complete Non-Async Features

**What's Needed:**
- All primitive types (i8, i16, i32, i64, u8, u16, u32, u64, bool, isize, usize)
- Binary operations (+, -, *, /, %, &, |, ^, <<, >>, ==, !=, <, >, <=, >=, &&, ||)
- Unary operations (-, !, ~, &, *)
- Variable declarations (let, var)
- Assignments (=, +=, -=, *=, /=)
- Control flow (if/else, for, while, switch/case)
- Function calls
- Struct types and field access
- Array types and indexing
- Pointer types and dereferencing

**Test with examples:**
- `examples/01_hello.tick` - Basic functions
- `examples/02_types.tick` - Type system
- `examples/03_control_flow.tick` - Control structures

---

### 4. Implement Coroutine Analysis

**What's Needed:**
- Analyze functions to detect async/await usage
- Identify suspend points (await expressions)
- Perform live variable analysis at each suspend point
- Compute coroutine frame layout (which variables to save)
- Annotate IR with coroutine metadata

**Files to Complete:**
- `stream4-coroutine/src/analysis.c` - Main analysis implementation
- `stream4-coroutine/src/liveness.c` - Live variable analysis
- `stream4-coroutine/src/frame_layout.c` - Frame packing

**Acceptance Criteria:**
- Can analyze `async fn() -> i32` functions
- Correctly identifies suspend points
- Computes minimal frame size
- Handles nested async calls

---

### 5. Implement Async/Await Transformation

**What's Needed:**
- Transform async functions into state machines
- Generate state labels for each suspend point
- Pack live variables into coroutine frames (tagged unions)
- Generate resume logic (jump to saved state)
- Handle defer/errdefer in async context
- Generate async_submit calls for I/O operations

**Files to Complete:**
- `stream5-lowering/src/lower.c` - Async transformation logic (~2000 LOC)
- Integration with coroutine analysis results

**Acceptance Criteria:**
- Can lower `async fn` with await expressions
- State machine structure correctly generated
- Coroutine frames are tagged unions
- Resume logic uses computed goto

---

### 6. Complete Async Code Generation

**What's Needed:**
- Generate coroutine frame structs (tagged unions)
- Generate state machine switch/computed goto
- Generate async_submit platform calls
- Generate frame allocation/deallocation
- Handle cleanup in state machine context

**Files to Complete:**
- `stream6-codegen/src/codegen.c` - State machine emission (~500 LOC)

**Acceptance Criteria:**
- Generated async functions compile with gcc
- State machines work with epoll runtime
- Can compile and run `examples/06_async_basic.tick`

---

### 7. End-to-End Testing

**Test Matrix:**
| Example | Parse | Type Check | Lower | Codegen | Compile | Run |
|---------|-------|------------|-------|---------|---------|-----|
| 01_hello.tick | ✅ | ⚠️ | ⚠️ | ⚠️ | ❌ | ❌ |
| 02_types.tick | ✅ | ⚠️ | ❌ | ❌ | ❌ | ❌ |
| 03_control_flow.tick | ✅ | ⚠️ | ❌ | ❌ | ❌ | ❌ |
| 04_errors.tick | ✅ | ⚠️ | ❌ | ❌ | ❌ | ❌ |
| 05_resources.tick | ✅ | ⚠️ | ❌ | ❌ | ❌ | ❌ |
| 06_async_basic.tick | ✅ | ⚠️ | ❌ | ❌ | ❌ | ❌ |
| 07_async_io.tick | ✅ | ⚠️ | ❌ | ❌ | ❌ | ❌ |
| 08_tcp_echo_server.tick | ✅ | ⚠️ | ❌ | ❌ | ❌ | ❌ |

**Legend:**
- ✅ Working
- ⚠️ Partial (infrastructure exists, bugs remain)
- ❌ Not working

---

### 8. Integration Tests (Currently 63% Stubs)

**Stub Tests to Implement:**
- `integration/test_coroutine.c` - 7 stub tests
- `integration/test_lowering.c` - 5 stub tests
- `integration/test_codegen.c` - 5 stub tests
- `integration/test_full_compile.c` - 5 stub tests
- `integration/test_pipeline.c` - 5 stub tests

**What's Needed:**
- Replace stubs with real tests that validate functionality
- Add negative tests (error handling)
- Ensure tests fail on invalid input

---

## Known Issues

### Parser
- 119 shift/reduce conflicts (works but could be cleaner)
- 1 test failing (struct initialization syntax)

### Type System
- Type nodes may not be created correctly in all grammar rules
- Need comprehensive type resolution testing

### Error Messages
- Could be more descriptive
- Need better source location tracking

### Testing
- No fuzzing
- No memory leak checking (valgrind)
- No test coverage analysis

---

## Development Approach

### Recommended Order

1. **Fix type resolution bug** (1-2 hours)
   - Debug why types are void
   - Ensure type nodes created properly
   - Verify type storage in AST

2. **Fix empty function bodies** (1-2 hours)
   - Debug statement lowering
   - Ensure instructions are emitted
   - Verify basic blocks populated

3. **Complete non-async features** (1-2 days)
   - All expressions
   - All statements
   - Control flow
   - Test with examples 01-05

4. **Implement coroutine analysis** (2-3 days)
   - Live variable analysis
   - Frame layout computation
   - Suspend point identification

5. **Implement async transformation** (3-4 days)
   - State machine generation
   - Frame packing
   - Resume logic

6. **Complete async codegen** (1-2 days)
   - Frame struct emission
   - State machine C generation
   - Platform integration

7. **End-to-end testing** (1-2 days)
   - Test all 8 examples
   - Fix issues found
   - Validate with runtime

---

## Definition of Done

The tick compiler is **production-ready** when:

### Must Have:
1. ✅ Compiler executable exists (`compiler/tickc`)
2. ✅ Can parse all 8 example programs
3. ❌ Type system works correctly (no "void" bugs)
4. ❌ Can compile all 8 example programs to C
5. ❌ Generated C compiles with: `gcc -std=c11 -Wall -Wextra -Werror -ffreestanding`
6. ❌ Compiled executables run correctly
7. ❌ TCP echo server works with epoll runtime
8. ❌ All integration tests are real (no stubs)
9. ❌ All tests pass

### Should Have:
- Parser conflicts resolved
- Comprehensive error messages
- Memory leak free (valgrind clean)
- Multi-architecture testing (x86_64, ARM)

### Nice to Have:
- Fuzzing for robustness
- Code coverage > 80%
- Optimization passes
- Advanced debugging support

---

## Progress Summary

**Phase 1: Infrastructure** ✅ COMPLETE
- Compiler driver built
- All 7 streams integrated
- File I/O working
- Error reporting in place

**Phase 2: Basic Compilation** ⚠️ IN PROGRESS
- Parsing works ✅
- Type checking infrastructure exists ✅
- Type resolution has bugs ❌
- IR lowering infrastructure exists ✅
- Statement/expression lowering incomplete ❌
- Code generation infrastructure exists ✅
- Function body emission broken ❌

**Phase 3: Non-Async Features** ❌ NOT STARTED
- Expressions
- Statements
- Control flow
- Examples 01-05

**Phase 4: Async Features** ❌ NOT STARTED
- Coroutine analysis
- Async transformation
- State machine codegen
- Examples 06-08

**Phase 5: Production Polish** ❌ NOT STARTED
- Real integration tests
- Memory leak checking
- Error message improvements
- Multi-architecture testing
