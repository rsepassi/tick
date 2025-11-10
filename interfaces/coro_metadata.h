#ifndef CORO_METADATA_H
#define CORO_METADATA_H

#include <stddef.h>
#include <stdint.h>
#include "error.h"
#include "type.h"

// Coroutine Metadata Interface
// Purpose: State machine transformation information
// Analysis Strategy: Start from exported (`pub`) functions and analyze call chains downward

typedef struct VarLiveness {
    const char* var_name;
    Type* var_type;
    bool is_live;
} VarLiveness;

typedef struct SuspendPoint {
    uint32_t state_id;
    SourceLocation loc;
    struct VarLiveness* live_vars;
    size_t live_var_count;
} SuspendPoint;

typedef struct StateField {
    const char* name;
    Type* type;
} StateField;

typedef struct StateStruct {
    uint32_t state_id;
    StateField* fields;  // Live vars at this state
    size_t field_count;
    size_t size;
    size_t alignment;
} StateStruct;

typedef struct CoroMetadata {
    const char* function_name;
    SuspendPoint* suspend_points;
    size_t suspend_count;

    StateStruct* state_structs;
    size_t state_count;

    size_t total_frame_size;  // Size of tagged union
    size_t frame_alignment;
} CoroMetadata;

// Forward declare Arena, AstNode, Scope
typedef struct Arena Arena;
typedef struct AstNode AstNode;
typedef struct Scope Scope;

// Coro analyzer produces (allocated from arena)
void coro_metadata_init(CoroMetadata* meta, AstNode* function, Arena* arena);
void coro_analyze_function(AstNode* function, Scope* scope, CoroMetadata* meta, Arena* arena);

#endif // CORO_METADATA_H
