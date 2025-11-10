# Stream 5: IR Lowering Subsystem Documentation

## Overview

The IR Lowering subsystem transforms the high-level Abstract Syntax Tree (AST) produced by the parser into a lower-level Intermediate Representation (IR) suitable for code generation. This phase bridges the gap between the source language's semantic structure and the target machine's execution model.

## Design Principles

### 1. Structured IR
- Uses structured control flow constructs (if/then/else, loops, switch)
- Maintains readability while enabling optimization
- Preserves source location information for debugging

### 2. Explicit Temporaries
- Three-address code style: `dest = left op right`
- All intermediate values explicitly named
- Simplifies later optimization passes

### 3. State Machine Transformation
- Coroutines transformed into explicit state machines
- Live variable analysis guides frame layout
- Suspend points become state transitions

### 4. Error Handling Transformation
- Result types lowered to explicit error checks
- `try`/`catch` becomes `if (is_error) goto error_label`
- Structured cleanup via defer/errdefer

## Architecture

### Module Structure

```
stream5-lowering/
├── ir.h                 # IR interface (expanded)
├── ast.h                # AST interface (dependency)
├── type.h               # Type system (dependency)
├── symbol.h             # Symbol tables (dependency)
├── coro_metadata.h      # Coroutine metadata (dependency)
├── arena.h              # Memory allocation (dependency)
├── error.h              # Error reporting (dependency)
├── src/
│   └── lower.c          # Main lowering implementation
└── test/
    └── test_lower.c     # Unit tests
```

### Key Components

#### 1. IR Node Types (`ir.h`)

**Top-Level Nodes:**
- `IR_MODULE`: Top-level compilation unit
- `IR_FUNCTION`: Function definition with parameters and body

**Control Flow:**
- `IR_BASIC_BLOCK`: Sequence of instructions with single entry/exit
- `IR_IF`: Conditional branching
- `IR_SWITCH`: Multi-way branching
- `IR_LOOP`: Loop structure with header/body/exit
- `IR_JUMP`: Unconditional jump
- `IR_COND_JUMP`: Conditional jump
- `IR_RETURN`: Function return

**Instructions:**
- `IR_ASSIGN`: Simple assignment
- `IR_BINARY_OP`: Binary operations (add, sub, mul, etc.)
- `IR_UNARY_OP`: Unary operations (neg, not, deref, etc.)
- `IR_CALL`: Regular function call
- `IR_ASYNC_CALL`: Asynchronous function call
- `IR_LOAD`: Memory load
- `IR_STORE`: Memory store
- `IR_ALLOCA`: Stack allocation
- `IR_GET_FIELD`: Structure field access
- `IR_GET_INDEX`: Array/slice indexing
- `IR_CAST`: Type conversion
- `IR_PHI`: SSA-like phi node for control flow merges

**Coroutine Operations:**
- `IR_STATE_MACHINE`: State machine structure
- `IR_SUSPEND`: Suspend execution
- `IR_RESUME`: Resume execution
- `IR_STATE_SAVE`: Save live variables to frame
- `IR_STATE_RESTORE`: Restore live variables from frame

**Cleanup and Error Handling:**
- `IR_DEFER_REGION`: Defer cleanup scope
- `IR_ERRDEFER_REGION`: Error-only cleanup scope
- `IR_TRY_BEGIN`: Begin error handling region
- `IR_TRY_END`: End error handling region
- `IR_ERROR_CHECK`: Check for error value
- `IR_ERROR_PROPAGATE`: Propagate error to caller

**Values:**
- `IR_TEMP`: Temporary variable
- `IR_CONSTANT`: Compile-time constant
- `IR_PARAM`: Function parameter
- `IR_GLOBAL`: Global variable

#### 2. IR Value Representation

Values in the IR are explicitly typed and categorized:

```c
struct IrValue {
    IrValueKind kind;
    Type* type;
    union {
        struct { uint32_t id; const char* name; } temp;
        struct { /* int, float, bool, string */ } constant;
        struct { uint32_t index; const char* name; } param;
        struct { const char* name; } global;
    } data;
};
```

#### 3. Control Flow Representation

Control flow is represented structurally:

**If Statement:**
```c
typedef struct IrIf {
    IrValue* condition;
    IrBasicBlock* then_block;
    IrBasicBlock* else_block;  // NULL if no else
    IrBasicBlock* merge_block;
} IrIf;
```

**Switch Statement:**
```c
typedef struct IrSwitch {
    IrValue* value;
    IrSwitchCase* cases;
    size_t case_count;
    IrBasicBlock* default_block;
    IrBasicBlock* merge_block;
} IrSwitch;
```

**Loop:**
```c
typedef struct IrLoop {
    IrBasicBlock* header;
    IrBasicBlock* body;
    IrBasicBlock* exit;
} IrLoop;
```

## Transformation Strategies

### 1. Expression Lowering

Expressions are lowered to sequences of three-address code instructions:

**Source:**
```
let result = (a + b) * c;
```

**IR:**
```
t1 = a + b
t2 = t1 * c
result = t2
```

**Implementation:**
- Recursively lower sub-expressions
- Allocate temporaries for intermediate results
- Emit instructions in evaluation order
- Handle side effects explicitly

### 2. Statement Lowering

#### Let/Var Statements
```
let x: i32 = 42;
```
Lowered to:
```
t1 = alloca i32, 1
store 42, t1
```

#### Return Statements
```
return value;
```
Lowered to:
```
// Execute defers in reverse order
defer_cleanup_1
defer_cleanup_0
return value
```

#### If Statements
```
if condition {
    then_body
} else {
    else_body
}
```

Lowered to:
```
entry:
    t1 = condition
    cond_jump t1, then, else

then:
    then_body
    jump merge

else:
    else_body
    jump merge

merge:
    // continue
```

#### For Loops
```
for init; condition; increment {
    body
}
```

Lowered to:
```
entry:
    init
    jump header

header:
    t1 = condition
    cond_jump t1, body, exit

body:
    body_stmts
    increment
    jump header

exit:
    // continue
```

### 3. Coroutine State Machine Transformation

Coroutines are transformed into explicit state machines using metadata from the coroutine analysis phase.

#### Transformation Steps

1. **Allocate Coroutine Frame**
   - Create frame pointer temporary
   - Allocate storage for live variables at each suspend point

2. **Create State Variable**
   - Track current execution state
   - Initialize to state 0

3. **Transform Suspend Points**
   - Each suspend point becomes a state
   - Generate state save instructions
   - Generate resume dispatch code

4. **Generate State Machine Dispatcher**
   ```c
   switch (state) {
       case 0: goto state_0;
       case 1: goto state_1;
       // ...
   }
   ```

5. **State Save/Restore**
   - Save live variables before suspend
   - Restore live variables after resume

#### Example Transformation

**Source:**
```
async fn example(x: i32) -> i32 {
    let y = await other_async();
    return x + y;
}
```

**IR State Machine:**
```
function example(x: i32) -> i32:
  entry:
    frame = alloca CoroFrame
    state = load frame.state
    switch state:
      case 0: goto state_0
      case 1: goto state_1

  state_0:
    // Initial state
    t1 = call other_async
    // Save live vars to frame
    store x, frame.saved_x
    store 0, frame.state  // Will be set to 1 on resume
    suspend 1

  state_1:
    // Resume after suspend
    x = load frame.saved_x
    y = resume_value
    t2 = x + y
    return t2
```

### 4. Error Handling Transformation

Result types are lowered to explicit error checking:

**Source:**
```
fn fallible() !i32 {
    let x = try risky_operation();
    return x * 2;
}
```

**IR:**
```
function fallible() -> Result<i32>:
  entry:
    t1 = call risky_operation
    error_check t1, error_handler, t2
    t3 = t2 * 2
    t4 = make_ok(t3)
    return t4

  error_handler:
    t5 = extract_error(t1)
    errdefer_cleanup  // Only on error path
    error_propagate t5
```

**Error Check Instruction:**
```c
// IR_ERROR_CHECK: if (value is error) goto error_label
struct {
    IrValue* value;
    IrBasicBlock* error_label;
    IrValue* error_dest;  // Where to store error value
} error_check;
```

### 5. Cleanup Transformation (Defer/Errdefer)

Defer and errdefer statements create cleanup regions:

#### Defer
- Executed on all exit paths (return, break, continue)
- Stored on a defer stack
- Executed in LIFO order

#### Errdefer
- Only executed on error exit paths
- Useful for resource cleanup on failure
- Also executed in LIFO order

**Source:**
```
fn resource_handler() !void {
    let file = open("file.txt");
    defer close(file);
    errdefer log_error("Failed to process file");

    try process(file);
}
```

**IR:**
```
function resource_handler() -> Result<void>:
  entry:
    file = call open("file.txt")
    // Defer: close(file)
    // Errdefer: log_error(...)

    t1 = call process(file)
    error_check t1, error_path, t2

  success_path:
    call close(file)  // defer
    return ok()

  error_path:
    call log_error("...")  // errdefer
    call close(file)  // defer (also on error)
    error_propagate t1
```

**Implementation:**
- Maintain defer stack during lowering
- Track whether each cleanup is defer or errdefer
- Insert appropriate cleanups at exit points
- Ensure LIFO order

### 6. Computed Goto Patterns

For certain control flow patterns (while switch, continue switch), we use computed goto:

**Source:**
```
while switch (state) {
    case .Running => { /* ... */ continue .Waiting; }
    case .Waiting => { /* ... */ continue .Running; }
    case .Done => break;
}
```

**IR:**
```
loop_header:
    switch state:
      case Running: goto case_running
      case Waiting: goto case_waiting
      case Done: goto loop_exit

case_running:
    // ...
    state = Waiting
    goto loop_header

case_waiting:
    // ...
    state = Running
    goto loop_header

loop_exit:
    // continue
```

## API Reference

### IR Construction

```c
// Allocate IR nodes
IrNode* ir_alloc_node(IrNodeKind kind, Arena* arena);
IrValue* ir_alloc_value(IrValueKind kind, Type* type, Arena* arena);
IrInstruction* ir_alloc_instruction(IrNodeKind kind, Arena* arena);
IrBasicBlock* ir_alloc_block(uint32_t id, const char* label, Arena* arena);
```

### Function Builder

```c
// Create and manage IR functions
IrFunction* ir_function_create(const char* name, Type* return_type,
                               IrParam* params, size_t param_count, Arena* arena);
IrValue* ir_function_new_temp(IrFunction* func, Type* type, Arena* arena);
IrBasicBlock* ir_function_new_block(IrFunction* func, const char* label, Arena* arena);
void ir_function_add_block(IrFunction* func, IrBasicBlock* block, Arena* arena);
```

### Basic Block Builder

```c
// Build basic blocks
void ir_block_add_instruction(IrBasicBlock* block, IrInstruction* instr, Arena* arena);
void ir_block_add_predecessor(IrBasicBlock* block, IrBasicBlock* pred, Arena* arena);
void ir_block_add_successor(IrBasicBlock* block, IrBasicBlock* succ, Arena* arena);
```

### Lowering API

```c
// AST to IR transformation
IrModule* ir_lower_ast(AstNode* ast, Arena* arena);
IrFunction* ir_lower_function(AstNode* func_node, CoroMetadata* coro_meta, Arena* arena);
IrBasicBlock* ir_lower_block(AstNode* block_node, IrFunction* func, Arena* arena);
IrInstruction* ir_lower_stmt(AstNode* stmt, IrFunction* func, IrBasicBlock* block, Arena* arena);
IrValue* ir_lower_expr(AstNode* expr, IrFunction* func, IrBasicBlock* block, Arena* arena);
```

### State Machine Transformation

```c
// Coroutine transformation
IrStateMachine* ir_transform_to_state_machine(IrFunction* func, CoroMetadata* meta, Arena* arena);
```

### Cleanup Transformation

```c
// Defer/errdefer handling
void ir_insert_defer_cleanup(IrFunction* func, IrBasicBlock* block, Arena* arena);
void ir_insert_errdefer_cleanup(IrFunction* func, IrBasicBlock* block, Arena* arena);
```

### Debug Output

```c
// Debugging and inspection
void ir_print_debug(IrNode* ir, FILE* out);
```

## Testing Strategy

### Unit Tests

The test suite (`test/test_lower.c`) covers:

1. **IR Construction Tests**
   - Node allocation
   - Value creation
   - Instruction creation
   - Basic block creation

2. **Function Builder Tests**
   - Function creation
   - Temporary generation
   - Block creation and addition
   - Parameter handling

3. **Basic Block Tests**
   - Instruction insertion
   - Predecessor/successor management
   - CFG construction

4. **Value Tests**
   - Temporary values
   - Constants
   - Parameters
   - Globals

5. **Instruction Tests**
   - Assignment
   - Binary/unary operations
   - Function calls
   - Control flow instructions

6. **Lowering Tests**
   - Expression lowering
   - Statement lowering
   - Function lowering
   - Module lowering

7. **State Machine Tests**
   - State machine construction
   - Suspend/resume transformation
   - Live variable handling

8. **Cleanup Tests**
   - Defer insertion
   - Errdefer insertion
   - LIFO order verification

### Test Approach

- **Mock AST Nodes**: Create minimal AST structures for testing
- **Mock Types**: Use simple type representations
- **Mock Metadata**: Construct minimal coroutine metadata
- **Verification**: Assert IR structure matches expectations

### Running Tests

```bash
cd stream5-lowering/test
gcc -o test_lower test_lower.c ../src/lower.c -I.. -I../../interfaces
./test_lower
```

## Memory Management

All IR nodes are allocated from an arena:
- Fast allocation (no per-object free)
- Bulk deallocation at end of compilation
- No memory leaks by design
- Pointer stability within arena

## Integration with Other Phases

### Inputs
- **AST**: From parser (stream 1)
- **Type Information**: From type checker (stream 2)
- **Symbol Tables**: From semantic analysis (stream 3)
- **Coroutine Metadata**: From coroutine analyzer (stream 4)

### Outputs
- **IR Module**: To code generator (stream 6)
- **Debug Information**: Source locations preserved

## Future Enhancements

Potential improvements:
1. **SSA Transformation**: Full SSA form with phi nodes
2. **Optimization Passes**: Dead code elimination, constant folding
3. **Register Allocation Hints**: Pre-assign registers for codegen
4. **Liveness Analysis**: More precise than coroutine metadata
5. **Escape Analysis**: Stack vs heap allocation decisions

## References

- AST Interface: `ast.h`
- Type System: `type.h`
- Symbol Tables: `symbol.h`
- Coroutine Metadata: `coro_metadata.h`
- IR Interface: `ir.h`
- Implementation: `src/lower.c`
- Tests: `test/test_lower.c`
