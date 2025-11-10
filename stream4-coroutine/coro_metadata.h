#ifndef CORO_METADATA_H
#define CORO_METADATA_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "error.h"
#include "type.h"

// Coroutine Metadata Interface
// Purpose: State machine transformation information
// Analysis Strategy: Start from exported (`pub`) functions and analyze call chains downward

// Forward declarations
typedef struct Arena Arena;
typedef struct AstNode AstNode;
typedef struct Scope Scope;
typedef struct BasicBlock BasicBlock;
typedef struct CFG CFG;

// Variable reference - tracks uses/defs in CFG
typedef struct VarRef {
    const char* var_name;
    Type* var_type;
    bool is_def;    // true = definition, false = use
    AstNode* node;
} VarRef;

// Basic block in CFG
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

    // Variable references in this block
    VarRef* var_refs;
    size_t var_ref_count;
    size_t var_ref_capacity;

    // Liveness sets (computed by dataflow analysis)
    const char** gen_set;      // Variables used before definition
    size_t gen_count;
    const char** kill_set;     // Variables defined
    size_t kill_count;
    const char** live_in;      // Variables live at block entry
    size_t live_in_count;
    const char** live_out;     // Variables live at block exit
    size_t live_out_count;

    // Suspend point info
    bool has_suspend;
    uint32_t suspend_state_id;
} BasicBlock;

// Control flow graph
typedef struct CFG {
    BasicBlock** blocks;
    size_t block_count;
    size_t block_capacity;

    BasicBlock* entry;
    BasicBlock* exit;

    // All variables in function scope
    const char** variables;
    Type** var_types;
    size_t var_count;
} CFG;

// Variable liveness at specific program point
typedef struct VarLiveness {
    const char* var_name;
    Type* var_type;
    bool is_live;
} VarLiveness;

// Suspend point with liveness information
typedef struct SuspendPoint {
    uint32_t state_id;
    SourceLocation loc;
    BasicBlock* block;
    VarLiveness* live_vars;
    size_t live_var_count;
} SuspendPoint;

// Field in a state struct
typedef struct StateField {
    const char* name;
    Type* type;
    size_t offset;  // Offset within the struct
} StateField;

// One struct per suspend state
typedef struct StateStruct {
    uint32_t state_id;
    StateField* fields;  // Live vars at this state
    size_t field_count;
    size_t size;
    size_t alignment;
} StateStruct;

// Complete coroutine metadata
typedef struct CoroMetadata {
    const char* function_name;
    AstNode* function_node;

    // CFG for analysis
    CFG* cfg;

    // Suspend points
    SuspendPoint* suspend_points;
    size_t suspend_count;

    // State structs (one per suspend point)
    StateStruct* state_structs;
    size_t state_count;

    // Frame layout (tagged union)
    size_t total_frame_size;  // Size of tagged union
    size_t frame_alignment;
    size_t tag_size;          // Size of state discriminator
    size_t tag_offset;        // Offset of tag in frame
    size_t union_offset;      // Offset of union in frame

    // Analysis errors
    ErrorList* errors;
} CoroMetadata;

// API Functions

// Initialize metadata structure
void coro_metadata_init(CoroMetadata* meta, AstNode* function, Arena* arena);

// Main analysis entry point
void coro_analyze_function(AstNode* function, Scope* scope, CoroMetadata* meta,
                          Arena* arena, ErrorList* errors);

// CFG construction
CFG* coro_build_cfg(AstNode* function, Scope* scope, Arena* arena, ErrorList* errors);
void cfg_add_block(CFG* cfg, BasicBlock* block, Arena* arena);
void cfg_add_edge(BasicBlock* from, BasicBlock* to, Arena* arena);
void cfg_print_debug(CFG* cfg, FILE* out);

// Liveness analysis
void coro_compute_liveness(CFG* cfg, Arena* arena);
void block_compute_gen_kill(BasicBlock* block, CFG* cfg, Arena* arena);
bool liveness_dataflow_iterate(CFG* cfg, Arena* arena);

// Frame layout
void coro_compute_frame_layout(CoroMetadata* meta, Arena* arena);
size_t compute_struct_size(StateField* fields, size_t count);
size_t compute_struct_alignment(StateField* fields, size_t count);

// Utilities
bool var_is_in_set(const char* var, const char** set, size_t count);
void var_set_add(const char*** set, size_t* count, const char* var, Arena* arena);
void var_set_union(const char*** dest, size_t* dest_count,
                   const char** src, size_t src_count, Arena* arena);
void var_set_diff(const char*** dest, size_t* dest_count,
                  const char** subtract, size_t subtract_count, Arena* arena);

#endif // CORO_METADATA_H
