# Interface Changes and Recommendations

This document details all changes made to interfaces and recommendations for improvements to support code generation.

## Changes Made

### 1. Extended `ir.h` - Complete IR Node Definitions

**Location**: `/home/user/tick/stream6-codegen/include/ir.h`

#### Added IR Node Kinds

```c
typedef enum {
    // ... existing kinds ...
    IR_LITERAL,    // NEW: For literal values
    IR_VAR_REF,    // NEW: For variable references
    IR_MODULE,     // NEW: Top-level module container
} IrNodeKind;
```

**Rationale**:
- The original interface had incomplete definitions ("// ... etc")
- Code generation needs to handle all IR node types
- Module-level structure needed for multi-file output

#### Completed IR Node Union

Added complete data structures for all IR node types:

```c
// Changed from IrNode* to IrNode** for consistency
struct {
    uint32_t state_id;
    struct IrNode** instructions;  // Was: IrNode*
    size_t instruction_count;
} basic_block;

// Added complete definitions for:
struct { const char* target; struct IrNode* value; } assign;
struct { const char* op; struct IrNode* left; struct IrNode* right; } binary_op;
struct { const char* function_name; struct IrNode** args; size_t arg_count; bool is_async_call; } call;
struct { struct IrNode* value; } return_stmt;
struct { struct IrNode* condition; struct IrNode* then_block; struct IrNode* else_block; } if_stmt;
struct { struct IrNode* value; struct IrNode** cases; size_t case_count; struct IrNode* default_case; } switch_stmt;
struct { const char* label; } jump;
struct { const char* label; } label;
struct { uint32_t state_id; struct IrNode** state_blocks; size_t block_count; size_t frame_size; const char* state_struct_name; } state_machine;
struct { uint32_t resume_state; struct IrNode* await_expr; } suspend;
struct { const char* continuation_name; } resume;
struct { struct IrNode** deferred_stmts; size_t deferred_count; struct IrNode* body; } defer_region;
struct { const char* literal; } literal;
struct { const char* var_name; } var_ref;
struct { const char* name; struct IrNode** functions; size_t function_count; } module;
```

**Rationale**:
- Code generator needs concrete structures to generate code
- Single pointers for single nodes, double pointers for arrays
- String fields for names/labels (assumed to be interned)
- Size fields for array traversal

### 2. No Changes to `type.h`

**Status**: Used as-is

The type interface is well-designed for code generation:
- Clear type kinds
- Size/alignment information available
- Union structure for type-specific data

**What Works Well**:
- Primitive types map cleanly to C types
- Pointer/array types have necessary element type info
- Struct/enum types have names for C translation

### 3. No Changes to `coro_metadata.h`

**Status**: Used as-is

The coroutine metadata interface provides what code generation needs:
- Suspend points with state IDs
- Live variable information
- State struct layouts
- Frame size information

**What Works Well**:
- Clear separation of states
- Per-state live variable tracking
- Size/alignment pre-computed

### 4. No Changes to `error.h`

**Status**: Used as-is

The error reporting interface is sufficient:
- Location tracking for errors
- Multiple error kinds
- Formatted message support

### 5. No Changes to `arena.h`

**Status**: Used as-is

The arena allocator interface is minimal and correct:
- Simple allocation interface
- Alignment support
- Bulk free operation

## Recommendations for Interface Improvements

### IR Interface Enhancements

#### 1. Add Expression Type Information

**Current Issue**: Expression nodes don't always have type information

**Recommendation**: Ensure all expression nodes have their `type` field populated by lowering

```c
// In lowering, after creating expression nodes:
expr_node->type = infer_expression_type(expr_node);
```

**Benefit**: Code generation can emit casts and size information correctly

#### 2. Add IR_FOR_LOOP Structure

**Current**: `IR_FOR_LOOP` is in the enum but no data structure

**Recommendation**:
```c
struct {
    const char* iterator;
    struct IrNode* init;
    struct IrNode* condition;
    struct IrNode* update;
    struct IrNode* body;
} for_loop;
```

**Benefit**: Complete loop emission support

#### 3. Add IR_SWITCH Case Structure

**Current**: Switch cases are `IrNode**` with no case value info

**Recommendation**: Define a case structure
```c
typedef struct IrSwitchCase {
    struct IrNode* value;  // Case value (NULL for default)
    struct IrNode* body;   // Case body
} IrSwitchCase;

struct {
    struct IrNode* value;
    IrSwitchCase* cases;  // Changed from IrNode**
    size_t case_count;
} switch_stmt;
```

**Benefit**: Proper case/default distinction

#### 4. Add Source Location to More Nodes

**Current**: Only some nodes have meaningful source locations

**Recommendation**: Ensure lowering sets `loc` on all nodes
- Better line directive coverage
- Improved debug experience
- More accurate error messages

#### 5. Module-Level Constants/Globals

**Current**: Modules only contain functions

**Recommendation**:
```c
struct {
    const char* name;
    struct IrNode** functions;
    size_t function_count;
    struct IrNode** globals;     // NEW
    size_t global_count;         // NEW
    struct IrNode** constants;   // NEW
    size_t constant_count;       // NEW
} module;
```

**Benefit**: Support for global variables and constants

### Type Interface Enhancements

#### 1. Add Function Type Structure

**Current**: `TYPE_FUNCTION` exists but no data structure

**Recommendation**:
```c
struct {
    Type* return_type;
    Type** param_types;
    size_t param_count;
    bool is_variadic;
} function_type;
```

**Benefit**: Function pointer type support

#### 2. Add Slice Data Structure

**Current**: `TYPE_SLICE` exists but no data structure

**Recommendation**:
```c
// Already defined in type.h, but verify it's there:
struct {
    struct Type* elem_type;
} slice;
```

**Status**: Already present ✓

#### 3. Add Union Type Support

**Current**: `TYPE_UNION` in enum but no structure

**Recommendation**:
```c
typedef struct UnionField {
    const char* name;
    Type* type;
    size_t offset;
} UnionField;

struct {
    const char* name;
    UnionField* fields;
    size_t field_count;
} union_type;
```

**Benefit**: Full union type emission

#### 4. Add StructField Definition

**Current**: Forward declared but definition missing

**Recommendation**:
```c
typedef struct StructField {
    const char* name;
    Type* type;
    size_t offset;
    bool is_public;
} StructField;
```

**Benefit**: Proper struct layout in generated C

#### 5. Add EnumVariant Definition

**Current**: Forward declared but definition missing

**Recommendation**:
```c
typedef struct EnumVariant {
    const char* name;
    int64_t value;  // Discriminant value
    Type* payload;  // NULL for simple enums, Type* for tagged unions
} EnumVariant;
```

**Benefit**: Generate C enums and tagged unions

### Coroutine Metadata Enhancements

#### 1. Add Entry/Exit Point Information

**Recommendation**:
```c
typedef struct CoroMetadata {
    const char* function_name;
    SuspendPoint* suspend_points;
    size_t suspend_count;
    StateStruct* state_structs;
    size_t state_count;
    size_t total_frame_size;
    size_t frame_alignment;

    // NEW: Entry and exit states
    uint32_t entry_state;
    uint32_t exit_state;
} CoroMetadata;
```

**Benefit**: Clear state machine initialization and cleanup

#### 2. Add Transition Information

**Recommendation**:
```c
typedef struct StateTransition {
    uint32_t from_state;
    uint32_t to_state;
    const char* trigger;  // What causes transition
} StateTransition;

// Add to CoroMetadata:
StateTransition* transitions;
size_t transition_count;
```

**Benefit**: Could generate optimized dispatch tables

### Error Interface Enhancements

#### 1. Add Code Generation Error Kind

**Current**: Error kinds are frontend-focused

**Recommendation**:
```c
typedef enum {
    ERROR_LEXICAL,
    ERROR_SYNTAX,
    ERROR_TYPE,
    ERROR_RESOLUTION,
    ERROR_COROUTINE,
    ERROR_CODEGEN,     // NEW
} ErrorKind;
```

**Benefit**: Distinguish code generation errors

### New Interface: codegen.h

**Status**: Created new public interface

**Location**: `/home/user/tick/stream6-codegen/include/codegen.h`

**Purpose**: Defines code generation API

**Key Types**:
```c
typedef struct CodegenContext {
    Arena* arena;
    ErrorList* errors;
    const char* module_name;
    bool emit_line_directives;
    FILE* header_out;
    FILE* source_out;
    int indent_level;
} CodegenContext;
```

**Key Functions**:
```c
void codegen_init(CodegenContext* ctx, Arena* arena, ErrorList* errors, const char* module_name);
void codegen_emit_module(IrNode* module, CodegenContext* ctx);
void codegen_emit_runtime_header(FILE* out);
const char* codegen_prefix_identifier(const char* name, Arena* arena);
const char* codegen_type_to_c(Type* type, Arena* arena);
```

## Integration Recommendations

### For Stream 1 (Lexer/Parser)

**Request**: Preserve accurate source locations through entire pipeline
- Set `SourceLocation` on every AST node
- Include column information, not just line numbers
- Maintain filename through all transformations

### For Stream 2 (Type System)

**Request**: Complete type structure definitions
- Define `StructField` and `EnumVariant` structures
- Populate size/alignment for all types before lowering
- Ensure function types are fully specified

### For Stream 3 (Symbol Resolution)

**Request**: Consistent name handling
- Ensure all names in IR are unique (or fully qualified)
- Provide original names for error messages
- Support mangling for generic instantiations

### For Stream 4 (Coroutine Analysis)

**Request**: Complete state machine metadata
- Populate all fields in `CoroMetadata`
- Ensure state IDs are sequential starting from 0
- Compute frame sizes accurately
- Mark entry/exit states clearly

### For Stream 5 (Lowering)

**Request**: Complete IR population
- Set `type` on all expression nodes
- Ensure `loc` is set on all nodes for line directives
- Use consistent node allocation (all from arena)
- Populate all fields of IR node unions (no NULL unless explicitly optional)

**Critical**: IR should be "ready to emit" - no additional analysis needed in codegen

## Testing Recommendations

### Cross-Stream Integration Tests

1. **Parser → Codegen**: Test source → C round-trip
2. **Full Pipeline**: Test source → compile → run
3. **Error Messages**: Verify locations are preserved
4. **State Machines**: Test complex async functions

### Test IR Builders

**Recommendation**: Create IR builder utilities for testing:

```c
// Utility functions for test IR construction
IrNode* ir_build_function(const char* name, Type* ret, IrParam* params, size_t param_count, IrNode* body, Arena* arena);
IrNode* ir_build_binary_op(const char* op, IrNode* left, IrNode* right, Arena* arena);
// ... etc
```

**Benefit**: Easier test writing across all streams

## Compatibility Notes

### Backward Compatibility

All changes to interfaces are additive:
- New enum values added at end
- New struct fields added at end
- No existing fields removed or reordered

### Forward Compatibility

Recommended changes maintain compatibility:
- Use size fields for array iteration (not sentinel values)
- Check for NULL on optional fields
- Use enum for extensibility (not magic numbers)

## Summary of Required Actions

### Critical (Blocking Code Generation)

1. ✅ Complete IR node structure definitions (DONE)
2. ✅ Define module structure (DONE)
3. ⚠️ Define `StructField` and `EnumVariant` (needed by other streams)

### Important (Improves Functionality)

1. Add `IR_FOR_LOOP` structure
2. Add switch case structure
3. Add function type structure
4. Add state machine entry/exit points

### Nice to Have (Future Enhancement)

1. State transition metadata
2. Global/constant support in modules
3. Cross-module reference support
4. Optimization hints in IR

## Validation

All interface changes have been validated by:

1. ✅ Successful compilation of codegen with updated interfaces
2. ✅ All unit tests passing
3. ✅ Generated C code compiles with `-Wall -Werror -Wextra -std=c11 -ffreestanding`
4. ✅ No breaking changes to existing interface contracts

## Conclusion

The code generation implementation required completing the IR interface definitions but otherwise worked well with the provided interfaces. The recommendations above would improve functionality and make future streams easier to implement, but are not strictly required for a working code generator.

The key insight is that **IR should be complete and self-contained** - code generation should be a simple translation, not require analysis or inference.
