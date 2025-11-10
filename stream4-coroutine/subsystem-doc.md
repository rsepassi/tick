# Stream 4: Coroutine Analysis Subsystem Documentation

## Overview

The Coroutine Analysis subsystem is responsible for analyzing asynchronous functions (coroutines) in the Async Systems Language compiler. It performs control flow analysis, liveness analysis, and computes the frame layout for coroutine state machines.

## Purpose

This module transforms coroutines into state machines by:

1. **Building a Control Flow Graph (CFG)** from the typed AST
2. **Identifying suspend points** where execution can be paused
3. **Computing variable liveness** across suspend points using dataflow analysis
4. **Generating frame layouts** as tagged unions with one struct per state
5. **Producing metadata** for the lowering and code generation phases

## Architecture

### Key Components

```
stream4-coroutine/
├── coro_metadata.h      - Extended interface with CFG and liveness structures
├── ast.h               - Expanded AST with concrete node definitions
├── src/
│   └── coro_analyze.c  - Core implementation
└── test/
    └── test_coro_analyze.c - Comprehensive unit tests
```

### Dependencies

- **ast.h** - Typed AST from semantic analysis
- **symbol.h** - Symbol tables for variable resolution
- **type.h** - Type information for frame layout
- **arena.h** - Fast allocation for analysis structures
- **error.h** - Error reporting

## Implementation Details

### 1. Control Flow Graph Construction

#### Algorithm

The CFG builder processes the AST recursively, creating basic blocks and edges:

```
CFG Construction:
1. Create entry and exit blocks
2. Start at entry block
3. For each statement:
   - Linear statements: Add to current block
   - Branches (if/switch): Create new blocks and edges
   - Loops (for/while): Create blocks for condition, body, update
   - Suspend: Mark block, create new block for resumption
   - Return: Connect to exit, create unreachable block
4. Track variable references (uses/defs) in each block
```

#### Basic Block Structure

```c
BasicBlock {
    id: unique identifier
    stmts: array of AST nodes in this block
    successors: outgoing edges
    predecessors: incoming edges
    var_refs: variable uses and definitions
    gen/kill sets: for liveness analysis
    live_in/live_out: liveness results
    has_suspend: true if this block contains a suspend point
    suspend_state_id: state machine state number
}
```

#### Edge Construction

- **Sequential flow**: Connect blocks in execution order
- **Conditional branches**: Create edges for both paths
- **Loop back-edges**: Connect update block to condition block
- **Suspend points**: Create edge to resumption block

### 2. Variable Reference Tracking

The CFG builder tracks every variable reference during construction:

- **Definitions**: `let x = ...`, `var x = ...`, assignments
- **Uses**: Variable in expressions, right-hand sides

This information is used to compute gen/kill sets for liveness analysis.

### 3. Liveness Analysis

#### Algorithm

Classic backward dataflow analysis:

```
Gen/Kill Computation (per block):
  GEN[B] = variables used before being defined in B
  KILL[B] = variables defined in B

Dataflow Iteration:
  Initialize all LIVE_IN, LIVE_OUT to empty
  Repeat until convergence:
    For each block B (in reverse order):
      LIVE_OUT[B] = ∪ LIVE_IN[S] for all successors S
      LIVE_IN[B] = GEN[B] ∪ (LIVE_OUT[B] - KILL[B])
```

#### Why Backward Analysis?

Liveness is a backward problem - a variable is live at a point if it **will be used** later. We need to know what's needed in the future, so we propagate from exit to entry.

#### Convergence

The algorithm iterates until a fixed point is reached. In practice, this converges quickly (typically 2-5 iterations for most functions) because:

- The lattice is finite (set of all variables)
- The transfer functions are monotonic
- We process blocks in reverse order (better for backward analysis)

### 4. Suspend Point Analysis

For each block with a suspend statement:

1. **Identify the suspend point** and assign a unique state ID
2. **Extract live variables** from LIVE_OUT of the suspend block
3. **Collect type information** for each live variable by searching variable references
4. **Create state struct** containing exactly the live variables at that point

#### Why LIVE_OUT?

Variables in LIVE_OUT of a suspend block are needed after resumption. These are exactly the variables that must be saved in the coroutine frame.

### 5. Frame Layout Computation

#### Tagged Union Design

The coroutine frame is a tagged union:

```c
struct CoroFrame {
    uint32_t state_tag;    // Current state ID
    union {
        State0 state_0;    // Variables live at suspend point 0
        State1 state_1;    // Variables live at suspend point 1
        ...
    } data;
}
```

#### Layout Algorithm

```
For each state struct:
  1. Compute alignment = max(alignof(field) for all fields)
  2. For each field in order:
     a. Align offset to field's alignment
     b. Place field at current offset
     c. Advance offset by field size
  3. Round total size up to struct alignment

Frame layout:
  1. Place tag at offset 0 (size = 4, align = 4)
  2. Compute union alignment = max(align of all states)
  3. Place union at aligned offset after tag
  4. Union size = max(size of all states)
  5. Total frame size = aligned(union_offset + union_size)
```

#### Size Optimization

Each state struct contains **only** the variables live at that suspend point. This minimizes frame size:

- **State 0**: Variables live at first suspend
- **State 1**: Variables live at second suspend
- Different states can have completely different fields

### 6. Metadata Generation

The final `CoroMetadata` contains:

- **CFG**: For debugging and verification
- **Suspend points**: Location, state ID, live variables
- **State structs**: Fields, sizes, alignments, offsets
- **Frame layout**: Total size, alignment, tag/union offsets

This metadata is consumed by:

- **Lowering phase**: Transform coroutines to state machines
- **Code generation**: Allocate frames, generate resume logic

## Example Analysis

### Input Code

```rust
pub async fn example() -> i32 {
    let x = 1;
    suspend;
    let y = x + 2;
    suspend;
    return y;
}
```

### CFG

```
Block 0 (entry):
  let x = 1
  → Block 1

Block 1 (suspend):
  suspend [state=0]
  → Block 2

Block 2:
  let y = x + 2
  → Block 3

Block 3 (suspend):
  suspend [state=1]
  → Block 4

Block 4:
  return y
  → Block 5 (exit)
```

### Liveness Results

```
Block 1 (first suspend):
  GEN = {}
  KILL = {}
  LIVE_IN = {x}
  LIVE_OUT = {x}  ← x needed after resume

Block 3 (second suspend):
  GEN = {}
  KILL = {}
  LIVE_IN = {y}
  LIVE_OUT = {y}  ← y needed after resume
```

### State Structs

```c
// State 0: Variables live at first suspend
struct State0 {
    i32 x;  // offset=0, size=4, align=4
};
// size=4, align=4

// State 1: Variables live at second suspend
struct State1 {
    i32 y;  // offset=0, size=4, align=4
};
// size=4, align=4
```

### Frame Layout

```c
struct CoroFrame {
    uint32_t tag;  // offset=0, size=4
    union {        // offset=4 (aligned)
        State0 s0; // size=4
        State1 s1; // size=4
    } data;        // union size=4
};
// Total size=8, alignment=4
```

## Testing Strategy

### Test Coverage

1. **CFG Construction**
   - Linear sequences
   - Conditional branches (if/else)
   - Loops (for/while)
   - Suspend statements
   - Return statements

2. **Variable Reference Tracking**
   - Definitions (let/var)
   - Uses (expressions)
   - Scoping

3. **Gen/Kill Sets**
   - Use before def → gen
   - Def → kill
   - Multiple defs/uses

4. **Liveness Analysis**
   - Live across suspend
   - Dead variables eliminated
   - Dataflow convergence

5. **Frame Layout**
   - Different sized types
   - Alignment requirements
   - Empty states
   - Tagged union size

6. **Complete Pipeline**
   - Multiple suspend points
   - Different live sets per state
   - End-to-end metadata generation

### Mock AST Building

Tests use mock AST builders to create typed ASTs without running the full parser and type checker:

```c
AstNode* func = mock_function("test",
    mock_block_stmt((AstNode*[]){
        mock_let_stmt("x", i32_type, mock_literal_int(1)),
        mock_suspend_stmt(),
        mock_return_stmt(mock_identifier("x", i32_type))
    }, 3)
);
```

### Verification

Tests assert:
- CFG structure (block count, edges)
- Suspend point detection
- Variable reference counts
- Gen/kill set contents
- Liveness results
- Frame size calculations
- Alignment correctness

## Design Decisions

### Why CFG-Based Analysis?

**Alternative**: Analyze AST directly without CFG

**Chosen**: Build explicit CFG first

**Rationale**:
- CFG makes control flow explicit and easy to traverse
- Standard dataflow algorithms work on CFG structure
- Easier to debug and visualize
- Reusable for other analyses (escape analysis, optimization)

### Why Backward Dataflow?

**Alternative**: Forward analysis from entry

**Chosen**: Backward analysis from exit

**Rationale**:
- Liveness is inherently backward (what's needed later)
- Natural fit for the problem
- Standard textbook algorithm

### Why Tagged Union?

**Alternative**: Single large struct with all variables

**Chosen**: Tagged union with state-specific structs

**Rationale**:
- **Space efficiency**: Each state only stores live variables
- **Type safety**: Impossible to access wrong state's data
- **Cache locality**: Smaller frames fit better in cache
- **Clear semantics**: State machine structure is explicit

### Why Arena Allocation?

**Alternative**: malloc/free for each structure

**Chosen**: Arena allocator for all analysis data

**Rationale**:
- **Performance**: Bulk allocation is much faster
- **Simplicity**: No need to free individual structures
- **Lifetime**: All analysis data has same lifetime (function scope)
- **Fragmentation**: No heap fragmentation

## Performance Characteristics

### Time Complexity

- **CFG Construction**: O(n) where n = number of AST nodes
- **Gen/Kill Computation**: O(n × v) where v = number of variables per block
- **Liveness Iteration**: O(b × v × i) where b = blocks, v = variables, i = iterations
  - Typically i < 10, often i < 5
- **Frame Layout**: O(s × f) where s = states, f = fields per state

**Overall**: O(n + b×v) - linear in program size

### Space Complexity

- **CFG**: O(b + e) where b = blocks, e = edges
- **Variable References**: O(r) where r = total references
- **Liveness Sets**: O(b × v) for in/out/gen/kill sets
- **Metadata**: O(s × f) for state structs

**Overall**: O(n + b×v) - linear in program size

### Scalability

The analysis is designed to handle:
- Functions with 100+ basic blocks
- 50+ variables
- 10+ suspend points
- Complex control flow (nested loops, branches)

## Future Enhancements

### Possible Optimizations

1. **Sparse Analysis**: Only track variables that are actually live somewhere
2. **Path Compression**: Merge linear chains of basic blocks
3. **State Merging**: Combine states with identical live sets
4. **Field Reordering**: Optimize struct layout for alignment

### Additional Analyses

1. **Escape Analysis**: Determine if coroutines escape to heap
2. **Lifetime Analysis**: Ensure borrowed data doesn't outlive suspends
3. **Async Call Graph**: Track coroutine call chains
4. **State Machine Optimization**: Eliminate unreachable states

## Integration with Other Phases

### Input from Type Checking

- Fully typed AST with type annotations
- Symbol tables with variable declarations
- Type information for size/alignment

### Output to Lowering

- Suspend points with state IDs
- Live variable sets per state
- Frame layout specifications
- State transition metadata

### Output to Codegen

- Frame size for allocation
- Field offsets for state access
- State machine structure
- Resume point locations

## Error Handling

The analysis reports errors for:

- **Invalid coroutines**: Functions with suspend but not marked async
- **Type errors**: Variables with unknown types
- **CFG construction failures**: Malformed AST
- **Analysis convergence**: Infinite loops in dataflow (with iteration limit)

All errors are collected in an `ErrorList` and can be reported together.

## Debugging Support

### CFG Visualization

`cfg_print_debug()` outputs CFG structure:

```
CFG with 5 blocks:
  Block 0: 1 stmts, 1 succs, 0 preds
  Block 1: 1 stmts, 1 succs, 1 preds [SUSPEND state=0]
  Block 2: 1 stmts, 1 succs, 1 preds
  ...
```

### Liveness Inspection

Each block maintains gen/kill/in/out sets for inspection during debugging.

### Metadata Dump

Test code can print complete metadata including all state structs and frame layout.

## Conclusion

The Coroutine Analysis subsystem provides a robust, efficient, and well-tested implementation of CFG construction, liveness analysis, and frame layout computation. It follows standard compiler design principles and integrates cleanly with other compilation phases.

The tagged union approach to frame layout optimizes for space efficiency while maintaining type safety. The dataflow-based liveness analysis ensures correctness while being fast enough for interactive compilation.
