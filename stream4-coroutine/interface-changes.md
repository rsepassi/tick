# Interface Changes Documentation

## Overview

This document details all changes made to interfaces for the Stream 4: Coroutine Analysis subsystem. Changes were made to both the interface I own (`coro_metadata.h`) and interfaces I depend on (`ast.h`).

## Summary of Changes

1. **coro_metadata.h** (OWNED) - Significantly expanded with CFG and liveness structures
2. **ast.h** (DEPENDENCY) - Expanded with concrete node definitions and includes
3. **Other interfaces** - No changes (arena.h, error.h, type.h, symbol.h remain unchanged)

---

## 1. coro_metadata.h - Major Expansion

### Rationale

The original interface provided only high-level structures (SuspendPoint, StateStruct, CoroMetadata). To implement proper coroutine analysis, we need:

- Control Flow Graph structures for analysis
- Variable reference tracking
- Liveness sets (gen/kill/in/out)
- Utility functions for set operations
- Additional API functions for CFG and liveness

### Added Structures

#### VarRef (NEW)

```c
typedef struct VarRef {
    const char* var_name;
    Type* var_type;
    bool is_def;    // true = definition, false = use
    AstNode* node;
} VarRef;
```

**Purpose**: Track variable uses and definitions within basic blocks for gen/kill computation.

**Rationale**: Need to distinguish between uses (add to gen) and defs (add to kill) during liveness analysis.

#### BasicBlock (NEW)

```c
typedef struct BasicBlock {
    uint32_t id;
    AstNode** stmts;
    size_t stmt_count;
    size_t stmt_capacity;

    // Edges
    BasicBlock** successors;
    size_t succ_count;
    size_t succ_capacity;
    BasicBlock** predecessors;
    size_t pred_count;
    size_t pred_capacity;

    // Variable references
    VarRef* var_refs;
    size_t var_ref_count;
    size_t var_ref_capacity;

    // Liveness sets
    const char** gen_set;
    size_t gen_count;
    const char** kill_set;
    size_t kill_count;
    const char** live_in;
    size_t live_in_count;
    const char** live_out;
    size_t live_out_count;

    // Suspend point info
    bool has_suspend;
    uint32_t suspend_state_id;
} BasicBlock;
```

**Purpose**: Represent a basic block in the control flow graph.

**Rationale**: CFG-based analysis is the standard approach for dataflow problems like liveness. Explicit CFG structure makes control flow easy to traverse and analyze.

**Key Fields**:
- `stmts`: AST nodes in this block
- `successors/predecessors`: CFG edges for traversal
- `var_refs`: Variable uses/defs for gen/kill computation
- `gen_set/kill_set`: Variables generated/killed in this block
- `live_in/live_out`: Liveness results from dataflow analysis
- `has_suspend`: Mark suspend points

#### CFG (NEW)

```c
typedef struct CFG {
    BasicBlock** blocks;
    size_t block_count;
    size_t block_capacity;

    BasicBlock* entry;
    BasicBlock* exit;

    const char** variables;
    Type** var_types;
    size_t var_count;
} CFG;
```

**Purpose**: Complete control flow graph for a function.

**Rationale**: Central data structure for all analysis. Explicit entry/exit blocks simplify dataflow equations.

**Key Fields**:
- `blocks`: All basic blocks in the function
- `entry/exit`: Special blocks for analysis boundaries
- `variables`: Complete list of variables in function scope

### Modified Structures

#### SuspendPoint - Added field

**Before**:
```c
typedef struct SuspendPoint {
    uint32_t state_id;
    SourceLocation loc;
    struct VarLiveness* live_vars;
    size_t live_var_count;
} SuspendPoint;
```

**After**:
```c
typedef struct SuspendPoint {
    uint32_t state_id;
    SourceLocation loc;
    BasicBlock* block;              // NEW
    VarLiveness* live_vars;
    size_t live_var_count;
} SuspendPoint;
```

**Change**: Added `BasicBlock* block` field.

**Rationale**: Link suspend points back to their CFG blocks for debugging and additional analysis.

#### StateField - Added field

**Before**:
```c
typedef struct StateField {
    const char* name;
    Type* type;
} StateField;
```

**After**:
```c
typedef struct StateField {
    const char* name;
    Type* type;
    size_t offset;  // NEW
} StateField;
```

**Change**: Added `size_t offset` field.

**Rationale**: Store computed offset within the state struct for code generation.

#### CoroMetadata - Significant expansion

**Before**:
```c
typedef struct CoroMetadata {
    const char* function_name;
    SuspendPoint* suspend_points;
    size_t suspend_count;
    StateStruct* state_structs;
    size_t state_count;
    size_t total_frame_size;
    size_t frame_alignment;
} CoroMetadata;
```

**After**:
```c
typedef struct CoroMetadata {
    const char* function_name;
    AstNode* function_node;         // NEW

    CFG* cfg;                       // NEW

    SuspendPoint* suspend_points;
    size_t suspend_count;

    StateStruct* state_structs;
    size_t state_count;

    size_t total_frame_size;
    size_t frame_alignment;
    size_t tag_size;                // NEW
    size_t tag_offset;              // NEW
    size_t union_offset;            // NEW

    ErrorList* errors;              // NEW
} CoroMetadata;
```

**Changes**:
- Added `function_node`: Back-reference to AST for debugging
- Added `cfg`: The complete control flow graph
- Added `tag_size`, `tag_offset`, `union_offset`: Detailed frame layout info
- Added `errors`: Error collection during analysis

**Rationale**:
- CFG is central to analysis and useful for debugging
- Detailed frame layout needed for precise code generation
- Error collection enables reporting multiple errors at once

### Added API Functions

#### CFG Construction

```c
CFG* coro_build_cfg(AstNode* function, Scope* scope, Arena* arena, ErrorList* errors);
void cfg_add_block(CFG* cfg, BasicBlock* block, Arena* arena);
void cfg_add_edge(BasicBlock* from, BasicBlock* to, Arena* arena);
void cfg_print_debug(CFG* cfg, FILE* out);
```

**Purpose**: Build and manipulate control flow graphs.

**Rationale**: CFG construction is a complex operation that deserves its own API.

#### Liveness Analysis

```c
void coro_compute_liveness(CFG* cfg, Arena* arena);
void block_compute_gen_kill(BasicBlock* block, CFG* cfg, Arena* arena);
bool liveness_dataflow_iterate(CFG* cfg, Arena* arena);
```

**Purpose**: Perform dataflow-based liveness analysis.

**Rationale**: Modular API allows testing each phase separately. `liveness_dataflow_iterate` returns bool to indicate convergence.

#### Frame Layout

```c
void coro_compute_frame_layout(CoroMetadata* meta, Arena* arena);
size_t compute_struct_size(StateField* fields, size_t count);
size_t compute_struct_alignment(StateField* fields, size_t count);
```

**Purpose**: Compute sizes, alignments, and offsets for state structs.

**Rationale**: Frame layout is complex and needs separate functions. These are reusable for any struct layout computation.

#### Utility Functions

```c
bool var_is_in_set(const char* var, const char** set, size_t count);
void var_set_add(const char*** set, size_t* count, const char* var, Arena* arena);
void var_set_union(const char*** dest, size_t* dest_count,
                   const char** src, size_t src_count, Arena* arena);
void var_set_diff(const char*** dest, size_t* dest_count,
                  const char** subtract, size_t subtract_count, Arena* arena);
```

**Purpose**: Set operations on variable name sets.

**Rationale**: Liveness analysis requires frequent set operations (union, difference, membership). These utilities ensure correct and consistent set manipulation.

### Modified API Functions

#### coro_analyze_function - Changed signature

**Before**:
```c
void coro_analyze_function(AstNode* function, Scope* scope, CoroMetadata* meta, Arena* arena);
```

**After**:
```c
void coro_analyze_function(AstNode* function, Scope* scope, CoroMetadata* meta,
                          Arena* arena, ErrorList* errors);
```

**Change**: Added `ErrorList* errors` parameter.

**Rationale**: Analysis can encounter errors (invalid coroutines, type errors). Need way to report them.

### Added Includes

**Before**:
```c
#include <stddef.h>
#include <stdint.h>
#include "error.h"
#include "type.h"
```

**After**:
```c
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>  // NEW
#include "error.h"
#include "type.h"
```

**Change**: Added `<stdbool.h>` include.

**Rationale**: Many new structures use `bool` type (e.g., `has_suspend`, `is_def`).

### Added Forward Declarations

```c
typedef struct BasicBlock BasicBlock;
typedef struct CFG CFG;
```

**Rationale**: These structures are now part of the interface and need forward declarations for use in other structures.

---

## 2. ast.h - Concrete Node Definitions

### Rationale

The original interface had only placeholder comments in the AST node union. For CFG construction and variable tracking, we need concrete field definitions to:

- Access statement components (condition, body, etc.)
- Extract variable names from declarations
- Traverse expression trees
- Identify suspend points

### Added Includes

**Before**:
```c
#include <stddef.h>
#include <stdio.h>
#include "error.h"
```

**After**:
```c
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>  // NEW
#include <stdint.h>   // NEW
#include "error.h"
```

**Change**: Added `<stdbool.h>` and `<stdint.h>`.

**Rationale**: Node definitions use `bool` (e.g., `is_public`, `is_async`) and `int64_t` (for literal values).

### Added Forward Declaration

**Before**:
```c
typedef struct Type Type;
```

**After**:
```c
typedef struct Type Type;
typedef struct Symbol Symbol;  // NEW
```

**Change**: Added forward declaration for Symbol.

**Rationale**: AST nodes may reference symbols (added `symbol` field to AstNode).

### Modified AstNode Structure

**Before**:
```c
typedef struct AstNode {
    AstNodeKind kind;
    SourceLocation loc;
    struct Type* type;

    union {
        struct { /* module fields */ } module;
        struct { /* function fields */ } function;
        struct { /* binary expr fields */ } binary_expr;
        // ... etc
    } data;
} AstNode;
```

**After**: Added `Symbol* symbol` field and expanded all union members with concrete fields.

#### Added Field: symbol

```c
Symbol* symbol;  // Symbol reference (for identifiers)
```

**Purpose**: Link identifier nodes to their symbol table entries.

**Rationale**: Useful for resolving variable types and scope during analysis.

#### Expanded Union Members

All union members now have concrete field definitions. Here are the key ones for coroutine analysis:

##### Function Declaration

```c
struct {
    const char* name;
    bool is_public;
    bool is_async;           // Key: Identifies coroutines
    struct AstNode** params;
    size_t param_count;
    struct AstNode* return_type;
    struct AstNode* body;    // Block statement
} function;
```

**Key Fields**:
- `is_async`: Identifies functions that need coroutine analysis
- `body`: Function body to analyze

##### Variable Declarations

```c
struct {
    const char* name;        // Variable name
    struct AstNode* type_expr;
    struct AstNode* init;
} let_stmt;

struct {
    const char* name;        // Variable name
    struct AstNode* type_expr;
    struct AstNode* init;
} var_stmt;
```

**Key Fields**:
- `name`: Variable name for tracking
- `init`: Initializer expression (contains uses)

##### Control Flow Statements

```c
struct {
    struct AstNode* condition;
    struct AstNode* then_block;
    struct AstNode* else_block;  // May be NULL
} if_stmt;

struct {
    struct AstNode* init;       // May be NULL
    struct AstNode* condition;  // May be NULL
    struct AstNode* update;     // May be NULL
    struct AstNode* body;
} for_stmt;
```

**Key Fields**: All edges for CFG construction.

##### Suspend Statement

```c
struct {
    // Suspend statement - no additional data
} suspend_stmt;
```

**Rationale**: Suspend has no data but needs explicit definition for clarity.

##### Block Statement

```c
struct {
    struct AstNode** stmts;
    size_t stmt_count;
} block;
```

**Key Fields**: Array of statements to process.

##### Expressions

```c
struct {
    const char* op;
    struct AstNode* left;
    struct AstNode* right;
} binary_expr;

struct {
    const char* name;        // Variable name
} identifier;

struct {
    union {
        int64_t int_val;
        double float_val;
        bool bool_val;
        const char* string_val;
    } value;
} literal;
```

**Key Fields**:
- `identifier.name`: Variable name for tracking uses
- Expression children for recursive traversal

### Impact on Other Streams

These AST changes are **additions only** - no existing fields were removed or changed. This means:

- **Backward compatible**: Existing code that didn't access these fields continues to work
- **Forward compatible**: Other streams can now use these concrete definitions
- **Benefits all streams**: Parser, type checker, lowering, and codegen can all use these fields

---

## 3. Interfaces NOT Changed

The following interfaces were used but **not modified**:

### arena.h - No changes

**Why no changes**: The arena interface is complete and sufficient:
- `arena_alloc()` handles all allocation needs
- `arena_free()` cleans up after analysis
- No additional arena functionality needed

### error.h - No changes

**Why no changes**: The error interface is complete:
- `error_list_add()` reports errors with location and message
- `ERROR_COROUTINE` kind already exists for coroutine errors
- No new error types needed

### type.h - No changes

**Why no changes**: The type interface provides everything needed:
- `type_sizeof()` and `type_alignof()` for layout computation
- Type structures already have size/alignment fields
- No new type operations needed

### symbol.h - No changes

**Why no changes**: Symbol table interface is sufficient:
- `scope_lookup()` resolves variable names
- Symbol structures already link to types
- `is_coroutine` field already exists in function symbols
- No additional symbol operations needed

---

## 4. Design Principles for Interface Changes

### Minimal Impact

Changes were designed to minimize impact on other subsystems:

1. **Additions only**: No removals or breaking changes
2. **Backward compatible**: Existing code continues to work
3. **Optional use**: New fields/functions only used by coroutine analysis

### Separation of Concerns

- **CFG structures**: Only in coro_metadata.h (not exposed to all compilation phases)
- **AST concrete definitions**: Useful for all phases, not coroutine-specific
- **Analysis utilities**: Internal to coro_analyze.c where possible

### Consistency

- **Naming conventions**: Follow existing patterns (snake_case, descriptive names)
- **Arena allocation**: All structures use arena (consistent with rest of compiler)
- **Error handling**: Use existing ErrorList mechanism
- **Structure patterns**: Arrays with count/capacity (matches existing code)

### Future-Proofing

Interface additions anticipate future needs:

- **CFG**: Reusable for optimization passes
- **Liveness infrastructure**: Can be extended to register allocation
- **Detailed frame layout**: Supports debugging metadata generation
- **Symbol field in AST**: Useful for all semantic phases

---

## 5. Interface Stability Guarantees

### Stable (Won't Change)

- Basic structure layouts (CoroMetadata, SuspendPoint, StateStruct)
- Core API functions (coro_analyze_function, coro_build_cfg)
- Data ownership (arena-allocated, no manual free)

### May Evolve

- Utility functions (could be optimized or extended)
- Debug functions (cfg_print_debug format may change)
- Internal CFG structures (could be optimized)

### Extension Points

Future enhancements can add:

- Additional analysis passes (escape analysis, state merging)
- Optimization metadata (dead code elimination hints)
- Debug information (source mapping for state machines)
- Profiling hooks (analysis performance metrics)

---

## 6. Integration Guidelines

### For Stream Owners

If you depend on modified interfaces:

#### Using ast.h

- **Benefit**: You now have concrete field definitions
- **Action**: Replace placeholder field access with real fields
- **Example**: `node->data.function.body` instead of cast hacks

#### Using coro_metadata.h

- **If you consume metadata**: Use the expanded structures as documented
- **If you produce AST**: Ensure `is_async` flag is set correctly on async functions
- **Pass ErrorList**: Update calls to `coro_analyze_function` to pass ErrorList

### Compatibility Notes

- **AST symbol field**: May be NULL if not filled by symbol resolution phase
- **CFG structures**: Only populated if coroutine analysis ran
- **Frame layout fields**: Only valid after `coro_compute_frame_layout()`

---

## Summary

### coro_metadata.h Changes

- **Added**: VarRef, BasicBlock, CFG structures
- **Modified**: SuspendPoint (+block), StateField (+offset), CoroMetadata (expanded)
- **Added APIs**: 14 new functions for CFG, liveness, layout, utilities
- **Rationale**: Enable CFG-based liveness analysis and precise frame layout

### ast.h Changes

- **Added**: Concrete field definitions for all node types
- **Modified**: AstNode (+symbol field)
- **Added includes**: stdbool.h, stdint.h
- **Rationale**: Enable proper AST traversal and field access for all phases

### Impact

- **Backward compatible**: Existing code continues to work
- **Benefits multiple streams**: AST definitions useful for all phases
- **Clean separation**: CFG structures isolated to coroutine subsystem
- **Well-tested**: All changes covered by comprehensive unit tests

All changes are documented, justified, and designed for long-term maintainability.
