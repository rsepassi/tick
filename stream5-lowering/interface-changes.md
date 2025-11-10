# IR Lowering Interface Changes

This document tracks all changes made to interfaces during the implementation of Stream 5 (IR Lowering).

## Summary

The primary interface modified is `ir.h`, which was significantly expanded from a minimal stub to a complete IR representation. No other interfaces were modified, though the implementation makes extensive use of existing interfaces.

## Modified Interfaces

### 1. ir.h (Owned Interface - Extensively Expanded)

**Original State:**
The original `ir.h` was a minimal stub with:
- Basic `IrNodeKind` enum with ~15 node types
- Simple `IrParam` struct
- Generic `IrNode` union structure
- Basic allocation function

**Changes Made:**

#### A. Added Forward Declarations (Lines 16-20)
```c
typedef struct IrNode IrNode;
typedef struct IrValue IrValue;
typedef struct IrInstruction IrInstruction;
typedef struct IrBasicBlock IrBasicBlock;
```

**Rationale:** Enable proper type references before full definitions, avoiding circular dependencies.

---

#### B. Expanded IrNodeKind Enum (Lines 23-72)

**Added:**
- `IR_MODULE` - Top-level module node
- Control flow: `IR_LOOP`, `IR_COND_JUMP`
- Instructions: `IR_UNARY_OP`, `IR_ASYNC_CALL`, `IR_LOAD`, `IR_STORE`, `IR_ALLOCA`, `IR_GET_FIELD`, `IR_GET_INDEX`, `IR_CAST`, `IR_PHI`
- Coroutine ops: `IR_STATE_SAVE`, `IR_STATE_RESTORE`
- Error handling: `IR_ERRDEFER_REGION`, `IR_TRY_BEGIN`, `IR_TRY_END`, `IR_ERROR_CHECK`, `IR_ERROR_PROPAGATE`
- Values: `IR_TEMP`, `IR_CONSTANT`, `IR_PARAM`, `IR_GLOBAL`

**Rationale:** Complete coverage of all IR operations needed for:
1. Three-address code representation
2. Explicit memory operations
3. Coroutine state machines
4. Error handling transformation
5. Value representation

---

#### C. Added Operator Enums (Lines 74-86)

**IrBinaryOp:**
```c
typedef enum {
    IR_OP_ADD, IR_OP_SUB, IR_OP_MUL, IR_OP_DIV, IR_OP_MOD,
    IR_OP_AND, IR_OP_OR, IR_OP_XOR,
    IR_OP_SHL, IR_OP_SHR,
    IR_OP_EQ, IR_OP_NE, IR_OP_LT, IR_OP_LE, IR_OP_GT, IR_OP_GE,
    IR_OP_LOGICAL_AND, IR_OP_LOGICAL_OR,
} IrBinaryOp;
```

**IrUnaryOp:**
```c
typedef enum {
    IR_OP_NEG, IR_OP_NOT, IR_OP_DEREF, IR_OP_ADDR_OF, IR_OP_LOGICAL_NOT,
} IrUnaryOp;
```

**Rationale:**
- Explicit operator representation for IR instructions
- Decouples IR operators from AST operators
- Enables operator-specific optimizations in later phases

---

#### D. Added IrValue System (Lines 88-126)

**IrValueKind Enum:**
```c
typedef enum {
    IR_VALUE_TEMP,      // Temporary variable
    IR_VALUE_CONSTANT,  // Compile-time constant
    IR_VALUE_PARAM,     // Function parameter
    IR_VALUE_GLOBAL,    // Global variable
    IR_VALUE_NULL,      // Null/undefined
} IrValueKind;
```

**IrValue Struct:**
Complete value representation with typed union for different value kinds.

**Rationale:**
- Explicit representation of all value types
- Type information attached to each value
- Supports three-address code style
- Enables constant folding and propagation

---

#### E. Enhanced IrParam (Lines 128-133)

**Added:**
```c
uint32_t index;  // Parameter position
```

**Rationale:** Track parameter ordering for calling convention.

---

#### F. Added Supporting Structures (Lines 135-153)

**IrSwitchCase:**
```c
typedef struct IrSwitchCase {
    IrValue* value;
    IrBasicBlock* target;
} IrSwitchCase;
```

**IrDeferCleanup:**
```c
typedef struct IrDeferCleanup {
    IrInstruction** instructions;
    size_t instruction_count;
    bool is_errdefer;
} IrDeferCleanup;
```

**IrStateMachineState:**
```c
typedef struct IrStateMachineState {
    uint32_t state_id;
    IrBasicBlock* block;
    StateStruct* state_struct;
} IrStateMachineState;
```

**Rationale:**
- Switch: Multi-way branching support
- Cleanup: Defer/errdefer implementation
- State machine: Coroutine transformation

---

#### G. Added IrBasicBlock (Lines 155-168)

Complete basic block representation with:
- Unique ID and label
- Dynamic instruction array
- CFG edges (predecessors/successors)

**Rationale:**
- Structured control flow representation
- Enables CFG analysis and optimization
- Supports basic block scheduling

---

#### H. Added IrInstruction (Lines 170-321)

Comprehensive instruction representation covering:
- Arithmetic/logic operations
- Memory operations
- Function calls
- Control flow
- Coroutine operations
- Error handling
- Value operations

Each instruction variant is a separate struct in the union with appropriate fields.

**Rationale:**
- Complete instruction set for lowering
- Type-safe instruction representation
- Clear semantics for each operation
- Extensible for future operations

---

#### I. Added Control Flow Structures (Lines 323-358)

**IrIf:**
```c
typedef struct IrIf {
    IrValue* condition;
    IrBasicBlock* then_block;
    IrBasicBlock* else_block;
    IrBasicBlock* merge_block;
} IrIf;
```

Similar structures for Switch, Loop, DeferRegion, and StateMachine.

**Rationale:**
- Structured representation of control flow
- Maintains high-level semantics for optimization
- Simpler than pure basic block representation

---

#### J. Added IrFunction (Lines 360-388)

Complete function representation with:
- Signature (name, return type, parameters)
- Basic block array
- Entry block pointer
- Coroutine metadata
- Temporary/block ID generators
- Defer stack

**Rationale:**
- Centralized function state during lowering
- Manages temporary and block allocation
- Tracks coroutine transformation state
- Maintains defer stack for cleanup

---

#### K. Added IrModule (Lines 390-399)

Module representation with:
- Module name
- Function array
- Global variable array

**Rationale:**
- Top-level compilation unit
- Manages module-level state
- Enables whole-module optimization

---

#### L. Replaced IrNode Union (Lines 401-419)

**Original:**
```c
union {
    struct { /* function fields */ } function;
    struct { /* basic_block fields */ } basic_block;
} data;
```

**New:**
```c
union {
    IrModule* module;
    IrFunction* function;
    IrBasicBlock* block;
    IrInstruction* instruction;
    IrIf* if_stmt;
    IrSwitch* switch_stmt;
    IrLoop* loop;
    IrDeferRegion* defer_region;
    IrStateMachine* state_machine;
    IrValue* value;
} data;
```

**Rationale:**
- Pointer-based union for flexibility
- Supports all IR node types
- Enables polymorphic IR tree

---

#### M. Added Comprehensive API (Lines 421-459)

**IR Construction API:**
```c
IrNode* ir_alloc_node(IrNodeKind kind, Arena* arena);
IrValue* ir_alloc_value(IrValueKind kind, Type* type, Arena* arena);
IrInstruction* ir_alloc_instruction(IrNodeKind kind, Arena* arena);
IrBasicBlock* ir_alloc_block(uint32_t id, const char* label, Arena* arena);
```

**Function Builder API:**
```c
IrFunction* ir_function_create(...);
IrValue* ir_function_new_temp(...);
IrBasicBlock* ir_function_new_block(...);
void ir_function_add_block(...);
```

**Basic Block Builder API:**
```c
void ir_block_add_instruction(...);
void ir_block_add_predecessor(...);
void ir_block_add_successor(...);
```

**Lowering API:**
```c
IrModule* ir_lower_ast(AstNode* ast, Arena* arena);
IrFunction* ir_lower_function(...);
IrBasicBlock* ir_lower_block(...);
IrInstruction* ir_lower_stmt(...);
IrValue* ir_lower_expr(...);
```

**Transformation API:**
```c
IrStateMachine* ir_transform_to_state_machine(...);
void ir_insert_defer_cleanup(...);
void ir_insert_errdefer_cleanup(...);
```

**Debug API:**
```c
void ir_print_debug(IrNode* ir, FILE* out);
```

**Rationale:**
- Complete API for IR construction
- Layered abstraction (construction → building → lowering → transformation)
- Enables testability
- Clear separation of concerns

---

#### N. Added Missing Include (Line 6)

**Added:**
```c
#include <stdbool.h>
```

**Rationale:** Required for `bool` type used throughout the interface.

---

#### O. Added Forward Declarations (Lines 422-423)

**Added:**
```c
typedef struct AstNode AstNode;
```

**Rationale:** Enable lowering functions to reference AST nodes without circular dependency.

---

## Interfaces Not Modified

The following interfaces were used but not modified:

### ast.h
- **Status:** No changes
- **Usage:** Source of AST nodes for lowering
- **Notes:** Interface is sufficient as-is; provides all needed node types and structure

### type.h
- **Status:** No changes
- **Usage:** Type information for IR values and instructions
- **Notes:** Type system is complete and requires no extensions

### symbol.h
- **Status:** No changes
- **Usage:** Symbol lookup during lowering (future enhancement)
- **Notes:** Current implementation uses placeholders; full integration pending

### coro_metadata.h
- **Status:** No changes
- **Usage:** Coroutine state machine transformation
- **Notes:** Metadata structure is exactly what's needed for state machine generation

### arena.h
- **Status:** No changes
- **Usage:** Memory allocation for all IR structures
- **Notes:** Arena interface is perfect for IR allocation patterns

### error.h
- **Status:** No changes
- **Usage:** Source location tracking and error reporting
- **Notes:** No changes needed; error handling transformation doesn't require interface modifications

## Design Decisions

### 1. Structured vs. Unstructured IR

**Decision:** Use structured IR with explicit control flow constructs.

**Rationale:**
- Easier to read and debug
- Preserves high-level intent
- Still enables goto-based transformations where needed (error handling, coroutines)
- Better for optimization passes

### 2. Three-Address Code Style

**Decision:** Explicit temporaries with three-address instructions.

**Rationale:**
- Simplified instruction selection in codegen
- Clear data dependencies
- Standard compiler IR design
- Enables register allocation

### 3. Pointer-Based Union in IrNode

**Decision:** Use pointers in IrNode union instead of embedded structs.

**Rationale:**
- Flexible sizing (IrFunction is large)
- Consistent allocation pattern (everything from arena)
- Enables sharing (e.g., same IrBasicBlock referenced multiple times)

### 4. Separate IrInstruction Type

**Decision:** Create dedicated IrInstruction struct separate from IrNode.

**Rationale:**
- Instructions are most common IR entity
- Dedicated type is more efficient than generic IrNode
- Clearer semantics
- Better type safety

### 5. Explicit CFG Edges

**Decision:** Track predecessors and successors in basic blocks.

**Rationale:**
- Enables CFG analysis
- Required for optimization passes
- Standard compiler design
- Small memory overhead, large analysis benefit

### 6. State Machine as First-Class Construct

**Decision:** Dedicated IrStateMachine structure rather than encoding in basic blocks.

**Rationale:**
- Preserves high-level coroutine semantics
- Easier to verify transformation correctness
- Enables coroutine-specific optimizations
- Clear separation from regular control flow

## Integration Notes

### Upstream Dependencies
- **Stream 1 (Parser):** Consumes AST nodes
- **Stream 2 (Type Checker):** Uses type information
- **Stream 3 (Semantic Analysis):** Uses symbol tables (future)
- **Stream 4 (Coroutine Analysis):** Consumes coroutine metadata

### Downstream Consumers
- **Stream 6 (Code Generation):** Consumes IR modules
- **Future Optimization Passes:** Will operate on IR

### Interface Stability
- **ir.h:** Should be stable; covers all anticipated needs
- **Possible Future Additions:**
  - SSA phi node enhancements
  - Additional optimization hints
  - Profile-guided optimization metadata
- **Breaking Changes:** None anticipated

## Testing Coverage

All interface changes are tested in `test/test_lower.c`:
- ✅ All new allocation functions
- ✅ All builder functions
- ✅ All value types
- ✅ All instruction types
- ✅ State machine transformation
- ✅ CFG construction

## Documentation

All interface changes are documented in:
- **This file:** `interface-changes.md`
- **Subsystem documentation:** `subsystem-doc.md`
- **Inline comments:** `ir.h` has extensive inline documentation

## Conclusion

The IR interface has been expanded from a minimal stub to a complete, production-ready intermediate representation. The design follows established compiler design principles (three-address code, structured IR with CFG, explicit state machines) while being tailored to the specific needs of this async systems language.

All changes maintain compatibility with existing interfaces and provide a solid foundation for the code generation phase.
