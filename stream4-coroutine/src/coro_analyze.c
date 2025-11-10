#include "../coro_metadata.h"
#include <ast.h>
#include <symbol.h>
#include <type.h>
#include <arena.h>
#include <error.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// ============================================================================
// UTILITY FUNCTIONS - Variable Set Operations
// ============================================================================

bool var_is_in_set(const char* var, const char** set, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(var, set[i]) == 0) {
            return true;
        }
    }
    return false;
}

void var_set_add(const char*** set, size_t* count, const char* var, Arena* arena) {
    // Check if already in set
    if (var_is_in_set(var, *set, *count)) {
        return;
    }

    // Allocate new array with one more slot
    const char** new_set = arena_alloc(arena, sizeof(const char*) * (*count + 1), sizeof(void*));

    // Copy existing elements
    for (size_t i = 0; i < *count; i++) {
        new_set[i] = (*set)[i];
    }

    // Add new element
    new_set[*count] = var;
    *set = new_set;
    (*count)++;
}

void var_set_union(const char*** dest, size_t* dest_count,
                   const char** src, size_t src_count, Arena* arena) {
    for (size_t i = 0; i < src_count; i++) {
        var_set_add(dest, dest_count, src[i], arena);
    }
}

void var_set_diff(const char*** dest, size_t* dest_count,
                  const char** subtract, size_t subtract_count, Arena* arena) {
    const char** new_set = arena_alloc(arena, sizeof(const char*) * (*dest_count), sizeof(void*));
    size_t new_count = 0;

    for (size_t i = 0; i < *dest_count; i++) {
        if (!var_is_in_set((*dest)[i], subtract, subtract_count)) {
            new_set[new_count++] = (*dest)[i];
        }
    }

    *dest = new_set;
    *dest_count = new_count;
}

// ============================================================================
// BASIC BLOCK OPERATIONS
// ============================================================================

BasicBlock* basic_block_create(uint32_t id, Arena* arena) {
    BasicBlock* block = arena_alloc(arena, sizeof(BasicBlock), sizeof(void*));
    block->id = id;
    block->stmts = NULL;
    block->stmt_count = 0;
    block->stmt_capacity = 0;

    block->successors = NULL;
    block->succ_count = 0;
    block->succ_capacity = 0;

    block->predecessors = NULL;
    block->pred_count = 0;
    block->pred_capacity = 0;

    block->var_refs = NULL;
    block->var_ref_count = 0;
    block->var_ref_capacity = 0;

    block->gen_set = NULL;
    block->gen_count = 0;
    block->kill_set = NULL;
    block->kill_count = 0;
    block->live_in = NULL;
    block->live_in_count = 0;
    block->live_out = NULL;
    block->live_out_count = 0;

    block->has_suspend = false;
    block->suspend_state_id = 0;

    return block;
}

void block_add_stmt(BasicBlock* block, AstNode* stmt, Arena* arena) {
    if (block->stmt_count >= block->stmt_capacity) {
        size_t new_cap = block->stmt_capacity == 0 ? 8 : block->stmt_capacity * 2;
        AstNode** new_stmts = arena_alloc(arena, sizeof(AstNode*) * new_cap, sizeof(void*));

        for (size_t i = 0; i < block->stmt_count; i++) {
            new_stmts[i] = block->stmts[i];
        }

        block->stmts = new_stmts;
        block->stmt_capacity = new_cap;
    }

    block->stmts[block->stmt_count++] = stmt;
}

void block_add_successor(BasicBlock* block, BasicBlock* succ, Arena* arena) {
    // Check if already connected
    for (size_t i = 0; i < block->succ_count; i++) {
        if (block->successors[i] == succ) {
            return;
        }
    }

    if (block->succ_count >= block->succ_capacity) {
        size_t new_cap = block->succ_capacity == 0 ? 4 : block->succ_capacity * 2;
        BasicBlock** new_succs = arena_alloc(arena, sizeof(BasicBlock*) * new_cap, sizeof(void*));

        for (size_t i = 0; i < block->succ_count; i++) {
            new_succs[i] = block->successors[i];
        }

        block->successors = new_succs;
        block->succ_capacity = new_cap;
    }

    block->successors[block->succ_count++] = succ;
}

void block_add_predecessor(BasicBlock* block, BasicBlock* pred, Arena* arena) {
    // Check if already connected
    for (size_t i = 0; i < block->pred_count; i++) {
        if (block->predecessors[i] == pred) {
            return;
        }
    }

    if (block->pred_count >= block->pred_capacity) {
        size_t new_cap = block->pred_capacity == 0 ? 4 : block->pred_capacity * 2;
        BasicBlock** new_preds = arena_alloc(arena, sizeof(BasicBlock*) * new_cap, sizeof(void*));

        for (size_t i = 0; i < block->pred_count; i++) {
            new_preds[i] = block->predecessors[i];
        }

        block->predecessors = new_preds;
        block->pred_capacity = new_cap;
    }

    block->predecessors[block->pred_count++] = pred;
}

void block_add_var_ref(BasicBlock* block, const char* var_name, Type* var_type,
                       bool is_def, AstNode* node, Arena* arena) {
    if (block->var_ref_count >= block->var_ref_capacity) {
        size_t new_cap = block->var_ref_capacity == 0 ? 16 : block->var_ref_capacity * 2;
        VarRef* new_refs = arena_alloc(arena, sizeof(VarRef) * new_cap, sizeof(void*));

        for (size_t i = 0; i < block->var_ref_count; i++) {
            new_refs[i] = block->var_refs[i];
        }

        block->var_refs = new_refs;
        block->var_ref_capacity = new_cap;
    }

    block->var_refs[block->var_ref_count].var_name = var_name;
    block->var_refs[block->var_ref_count].var_type = var_type;
    block->var_refs[block->var_ref_count].is_def = is_def;
    block->var_refs[block->var_ref_count].node = node;
    block->var_ref_count++;
}

// ============================================================================
// CFG CONSTRUCTION
// ============================================================================

typedef struct CFGBuilder {
    CFG* cfg;
    Arena* arena;
    ErrorList* errors;
    Scope* scope;

    BasicBlock* current_block;
    uint32_t next_block_id;
    uint32_t next_state_id;

    // Break/continue targets for loops
    BasicBlock* loop_exit;
    BasicBlock* loop_continue;
} CFGBuilder;

void cfg_builder_init(CFGBuilder* builder, Arena* arena, Scope* scope, ErrorList* errors) {
    builder->cfg = arena_alloc(arena, sizeof(CFG), sizeof(void*));
    builder->cfg->blocks = NULL;
    builder->cfg->block_count = 0;
    builder->cfg->block_capacity = 0;
    builder->cfg->entry = NULL;
    builder->cfg->exit = NULL;
    builder->cfg->variables = NULL;
    builder->cfg->var_types = NULL;
    builder->cfg->var_count = 0;

    builder->arena = arena;
    builder->errors = errors;
    builder->scope = scope;
    builder->current_block = NULL;
    builder->next_block_id = 0;
    builder->next_state_id = 0;
    builder->loop_exit = NULL;
    builder->loop_continue = NULL;
}

void cfg_add_block(CFG* cfg, BasicBlock* block, Arena* arena) {
    if (cfg->block_count >= cfg->block_capacity) {
        size_t new_cap = cfg->block_capacity == 0 ? 16 : cfg->block_capacity * 2;
        BasicBlock** new_blocks = arena_alloc(arena, sizeof(BasicBlock*) * new_cap, sizeof(void*));

        for (size_t i = 0; i < cfg->block_count; i++) {
            new_blocks[i] = cfg->blocks[i];
        }

        cfg->blocks = new_blocks;
        cfg->block_capacity = new_cap;
    }

    cfg->blocks[cfg->block_count++] = block;
}

void cfg_add_edge(BasicBlock* from, BasicBlock* to, Arena* arena) {
    block_add_successor(from, to, arena);
    block_add_predecessor(to, from, arena);
}

BasicBlock* cfg_builder_new_block(CFGBuilder* builder) {
    BasicBlock* block = basic_block_create(builder->next_block_id++, builder->arena);
    cfg_add_block(builder->cfg, block, builder->arena);
    return block;
}

void cfg_builder_set_current(CFGBuilder* builder, BasicBlock* block) {
    builder->current_block = block;
}

// Forward declarations for expression/statement processing
void cfg_process_expr(CFGBuilder* builder, AstNode* expr);
void cfg_process_stmt(CFGBuilder* builder, AstNode* stmt);

void cfg_process_expr(CFGBuilder* builder, AstNode* expr) {
    if (!expr) return;

    switch (expr->kind) {
        case AST_IDENTIFIER_EXPR: {
            // Variable use
            const char* name = expr->data.identifier_expr.name;
            block_add_var_ref(builder->current_block, name, expr->type,
                            false, expr, builder->arena);
            break;
        }

        case AST_BINARY_EXPR:
            cfg_process_expr(builder, expr->data.binary_expr.left);
            cfg_process_expr(builder, expr->data.binary_expr.right);
            break;

        case AST_UNARY_EXPR:
            cfg_process_expr(builder, expr->data.unary_expr.operand);
            break;

        case AST_CALL_EXPR:
            cfg_process_expr(builder, expr->data.call_expr.callee);
            for (size_t i = 0; i < expr->data.call_expr.arg_count; i++) {
                cfg_process_expr(builder, expr->data.call_expr.args[i]);
            }
            break;

        case AST_ASYNC_CALL_EXPR:
            cfg_process_expr(builder, expr->data.async_call_expr.callee);
            for (size_t i = 0; i < expr->data.async_call_expr.arg_count; i++) {
                cfg_process_expr(builder, expr->data.async_call_expr.args[i]);
            }
            break;

        case AST_FIELD_ACCESS_EXPR:
            cfg_process_expr(builder, expr->data.field_access_expr.object);
            break;

        case AST_INDEX_EXPR:
            cfg_process_expr(builder, expr->data.index_expr.array);
            cfg_process_expr(builder, expr->data.index_expr.index);
            break;

        case AST_CAST_EXPR:
            cfg_process_expr(builder, expr->data.cast_expr.expr);
            break;

        case AST_LITERAL_EXPR:
            // No variable references
            break;

        default:
            break;
    }
}

void cfg_process_stmt(CFGBuilder* builder, AstNode* stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case AST_LET_STMT:
        case AST_VAR_STMT: {
            const char* name = stmt->kind == AST_LET_STMT ?
                             stmt->data.let_decl.name : stmt->data.var_decl.name;
            AstNode* init = stmt->kind == AST_LET_STMT ?
                          stmt->data.let_decl.init : stmt->data.var_decl.init;

            // Process initializer first (uses)
            if (init) {
                cfg_process_expr(builder, init);
            }

            // Then add definition
            block_add_stmt(builder->current_block, stmt, builder->arena);
            block_add_var_ref(builder->current_block, name, stmt->type,
                            true, stmt, builder->arena);
            break;
        }

        case AST_EXPR_STMT:
            cfg_process_expr(builder, stmt->data.expr_stmt.expr);
            block_add_stmt(builder->current_block, stmt, builder->arena);
            break;

        case AST_RETURN_STMT:
            if (stmt->data.return_stmt.value) {
                cfg_process_expr(builder, stmt->data.return_stmt.value);
            }
            block_add_stmt(builder->current_block, stmt, builder->arena);
            cfg_add_edge(builder->current_block, builder->cfg->exit, builder->arena);
            // Create new unreachable block for any following statements
            builder->current_block = cfg_builder_new_block(builder);
            break;

        case AST_IF_STMT: {
            cfg_process_expr(builder, stmt->data.if_stmt.condition);
            block_add_stmt(builder->current_block, stmt, builder->arena);

            BasicBlock* if_block = builder->current_block;
            BasicBlock* then_block = cfg_builder_new_block(builder);
            BasicBlock* else_block = stmt->data.if_stmt.else_block ?
                                    cfg_builder_new_block(builder) : NULL;
            BasicBlock* merge_block = cfg_builder_new_block(builder);

            // Process then branch
            cfg_add_edge(if_block, then_block, builder->arena);
            cfg_builder_set_current(builder, then_block);
            cfg_process_stmt(builder, stmt->data.if_stmt.then_block);
            cfg_add_edge(builder->current_block, merge_block, builder->arena);

            // Process else branch if it exists
            if (else_block) {
                cfg_add_edge(if_block, else_block, builder->arena);
                cfg_builder_set_current(builder, else_block);
                cfg_process_stmt(builder, stmt->data.if_stmt.else_block);
                cfg_add_edge(builder->current_block, merge_block, builder->arena);
            } else {
                cfg_add_edge(if_block, merge_block, builder->arena);
            }

            cfg_builder_set_current(builder, merge_block);
            break;
        }

        case AST_FOR_STMT: {
            BasicBlock* header_block = builder->current_block;
            BasicBlock* cond_block = cfg_builder_new_block(builder);
            BasicBlock* body_block = cfg_builder_new_block(builder);
            BasicBlock* continue_block = cfg_builder_new_block(builder);
            BasicBlock* exit_block = cfg_builder_new_block(builder);

            // Save loop targets
            BasicBlock* saved_exit = builder->loop_exit;
            BasicBlock* saved_continue = builder->loop_continue;
            builder->loop_exit = exit_block;
            builder->loop_continue = continue_block;

            // Process iterable/range if present
            if (stmt->data.for_stmt.iterable) {
                cfg_process_expr(builder, stmt->data.for_stmt.iterable);
            }
            cfg_add_edge(header_block, cond_block, builder->arena);

            // Process condition (or implicit true for infinite loop)
            cfg_builder_set_current(builder, cond_block);
            if (stmt->data.for_stmt.condition) {
                cfg_process_expr(builder, stmt->data.for_stmt.condition);
            }
            block_add_stmt(cond_block, stmt, builder->arena);
            cfg_add_edge(cond_block, body_block, builder->arena);
            cfg_add_edge(cond_block, exit_block, builder->arena);

            // Process body
            cfg_builder_set_current(builder, body_block);
            cfg_process_stmt(builder, stmt->data.for_stmt.body);
            cfg_add_edge(builder->current_block, continue_block, builder->arena);

            // Process continue expression (post-iteration update)
            cfg_builder_set_current(builder, continue_block);
            if (stmt->data.for_stmt.continue_expr) {
                cfg_process_expr(builder, stmt->data.for_stmt.continue_expr);
            }
            cfg_add_edge(continue_block, cond_block, builder->arena);

            // Restore loop targets
            builder->loop_exit = saved_exit;
            builder->loop_continue = saved_continue;

            cfg_builder_set_current(builder, exit_block);
            break;
        }

        case AST_SUSPEND_STMT: {
            block_add_stmt(builder->current_block, stmt, builder->arena);
            builder->current_block->has_suspend = true;
            builder->current_block->suspend_state_id = builder->next_state_id++;

            // Suspend creates a new basic block for resumption
            BasicBlock* resume_block = cfg_builder_new_block(builder);
            cfg_add_edge(builder->current_block, resume_block, builder->arena);
            cfg_builder_set_current(builder, resume_block);
            break;
        }

        case AST_BLOCK_STMT: {
            for (size_t i = 0; i < stmt->data.block_stmt.stmt_count; i++) {
                cfg_process_stmt(builder, stmt->data.block_stmt.stmts[i]);
            }
            break;
        }

        case AST_DEFER_STMT:
        case AST_ERRDEFER_STMT:
            // These are handled separately - not part of normal CFG
            break;

        default:
            block_add_stmt(builder->current_block, stmt, builder->arena);
            break;
    }
}

CFG* coro_build_cfg(AstNode* function, Scope* scope, Arena* arena, ErrorList* errors) {
    if (function->kind != AST_FUNCTION_DECL) {
        error_list_add(errors, ERROR_COROUTINE, function->loc,
                      "coro_build_cfg called on non-function node");
        return NULL;
    }

    CFGBuilder builder;
    cfg_builder_init(&builder, arena, scope, errors);

    // Create entry and exit blocks
    builder.cfg->entry = cfg_builder_new_block(&builder);
    builder.cfg->exit = cfg_builder_new_block(&builder);

    // Start at entry
    cfg_builder_set_current(&builder, builder.cfg->entry);

    // Process function body
    if (function->data.function_decl.body) {
        cfg_process_stmt(&builder, function->data.function_decl.body);
    }

    // Connect to exit if no explicit return
    if (builder.current_block != builder.cfg->exit) {
        cfg_add_edge(builder.current_block, builder.cfg->exit, arena);
    }

    return builder.cfg;
}

void cfg_print_debug(CFG* cfg, FILE* out) {
    fprintf(out, "CFG with %zu blocks:\n", cfg->block_count);
    for (size_t i = 0; i < cfg->block_count; i++) {
        BasicBlock* block = cfg->blocks[i];
        fprintf(out, "  Block %u: %zu stmts, %zu succs, %zu preds",
                block->id, block->stmt_count, block->succ_count, block->pred_count);
        if (block->has_suspend) {
            fprintf(out, " [SUSPEND state=%u]", block->suspend_state_id);
        }
        fprintf(out, "\n");
    }
}

// ============================================================================
// LIVENESS ANALYSIS
// ============================================================================

void block_compute_gen_kill(BasicBlock* block, CFG* cfg, Arena* arena) {
    // Gen = variables used before being defined in this block
    // Kill = variables defined in this block

    // Track which vars have been defined so far in this block
    const char** defined = NULL;
    size_t defined_count = 0;

    // Process variable references in order
    for (size_t i = 0; i < block->var_ref_count; i++) {
        VarRef* ref = &block->var_refs[i];

        if (ref->is_def) {
            // This is a definition - add to kill set
            var_set_add(&block->kill_set, &block->kill_count, ref->var_name, arena);
            var_set_add(&defined, &defined_count, ref->var_name, arena);
        } else {
            // This is a use - add to gen if not yet defined in this block
            if (!var_is_in_set(ref->var_name, defined, defined_count)) {
                var_set_add(&block->gen_set, &block->gen_count, ref->var_name, arena);
            }
        }
    }
}

bool liveness_dataflow_iterate(CFG* cfg, Arena* arena) {
    bool changed = false;

    // Process blocks in reverse order (better for backward dataflow)
    for (size_t i = cfg->block_count; i > 0; i--) {
        BasicBlock* block = cfg->blocks[i - 1];

        // Compute OUT[B] = union of IN[S] for all successors S
        const char** new_out = NULL;
        size_t new_out_count = 0;

        for (size_t j = 0; j < block->succ_count; j++) {
            BasicBlock* succ = block->successors[j];
            var_set_union(&new_out, &new_out_count,
                         succ->live_in, succ->live_in_count, arena);
        }

        // Check if OUT changed
        if (new_out_count != block->live_out_count) {
            changed = true;
        } else {
            for (size_t j = 0; j < new_out_count; j++) {
                if (!var_is_in_set(new_out[j], block->live_out, block->live_out_count)) {
                    changed = true;
                    break;
                }
            }
        }

        block->live_out = new_out;
        block->live_out_count = new_out_count;

        // Compute IN[B] = GEN[B] ∪ (OUT[B] - KILL[B])
        const char** new_in = NULL;
        size_t new_in_count = 0;

        // Start with GEN
        var_set_union(&new_in, &new_in_count,
                     block->gen_set, block->gen_count, arena);

        // Add (OUT - KILL)
        const char** out_minus_kill = arena_alloc(arena,
            sizeof(const char*) * block->live_out_count, sizeof(void*));
        size_t out_minus_kill_count = 0;

        for (size_t j = 0; j < block->live_out_count; j++) {
            if (!var_is_in_set(block->live_out[j], block->kill_set, block->kill_count)) {
                out_minus_kill[out_minus_kill_count++] = block->live_out[j];
            }
        }

        var_set_union(&new_in, &new_in_count,
                     out_minus_kill, out_minus_kill_count, arena);

        // Check if IN changed
        if (new_in_count != block->live_in_count) {
            changed = true;
        } else {
            for (size_t j = 0; j < new_in_count; j++) {
                if (!var_is_in_set(new_in[j], block->live_in, block->live_in_count)) {
                    changed = true;
                    break;
                }
            }
        }

        block->live_in = new_in;
        block->live_in_count = new_in_count;
    }

    return changed;
}

void coro_compute_liveness(CFG* cfg, Arena* arena) {
    // Compute gen/kill sets for all blocks
    for (size_t i = 0; i < cfg->block_count; i++) {
        block_compute_gen_kill(cfg->blocks[i], cfg, arena);
    }

    // Iterate until fixed point
    int iterations = 0;
    while (liveness_dataflow_iterate(cfg, arena)) {
        iterations++;
        if (iterations > 1000) {
            // Safety check to prevent infinite loops
            fprintf(stderr, "Warning: liveness analysis did not converge after 1000 iterations\n");
            break;
        }
    }
}

// ============================================================================
// FRAME LAYOUT COMPUTATION
// ============================================================================

size_t align_to(size_t offset, size_t alignment) {
    return (offset + alignment - 1) & ~(alignment - 1);
}

size_t compute_struct_alignment(StateField* fields, size_t count) {
    size_t max_align = 1;
    for (size_t i = 0; i < count; i++) {
        size_t field_align = type_alignof(fields[i].type);
        if (field_align > max_align) {
            max_align = field_align;
        }
    }
    return max_align;
}

size_t compute_struct_size(StateField* fields, size_t count) {
    if (count == 0) {
        return 0;
    }

    size_t offset = 0;
    size_t max_align = compute_struct_alignment(fields, count);

    for (size_t i = 0; i < count; i++) {
        size_t field_align = type_alignof(fields[i].type);
        offset = align_to(offset, field_align);
        fields[i].offset = offset;
        offset += type_sizeof(fields[i].type);
    }

    // Align total size to struct alignment
    return align_to(offset, max_align);
}

void coro_compute_frame_layout(CoroMetadata* meta, Arena* arena) {
    // Tag is a uint32_t for state discriminator
    meta->tag_size = sizeof(uint32_t);
    meta->tag_offset = 0;

    // Union starts after tag (aligned)
    size_t max_union_size = 0;
    size_t max_union_align = 1;

    // Compute size/alignment for each state struct
    for (size_t i = 0; i < meta->state_count; i++) {
        StateStruct* state = &meta->state_structs[i];

        if (state->field_count > 0) {
            state->alignment = compute_struct_alignment(state->fields, state->field_count);
            state->size = compute_struct_size(state->fields, state->field_count);
        } else {
            state->alignment = 1;
            state->size = 0;
        }

        if (state->size > max_union_size) {
            max_union_size = state->size;
        }
        if (state->alignment > max_union_align) {
            max_union_align = state->alignment;
        }
    }

    // Union offset is tag_offset + tag_size, aligned to union alignment
    meta->union_offset = align_to(meta->tag_offset + meta->tag_size, max_union_align);

    // Total frame size
    meta->frame_alignment = max_union_align;
    if (max_union_align < sizeof(uint32_t)) {
        meta->frame_alignment = sizeof(uint32_t);
    }

    meta->total_frame_size = align_to(meta->union_offset + max_union_size,
                                     meta->frame_alignment);
}

// ============================================================================
// MAIN ANALYSIS ENTRY POINT
// ============================================================================

void coro_metadata_init(CoroMetadata* meta, AstNode* function, Arena* arena) {
    meta->function_name = function->data.function_decl.name;
    meta->function_node = function;
    meta->cfg = NULL;
    meta->suspend_points = NULL;
    meta->suspend_count = 0;
    meta->state_structs = NULL;
    meta->state_count = 0;
    meta->total_frame_size = 0;
    meta->frame_alignment = 0;
    meta->tag_size = 0;
    meta->tag_offset = 0;
    meta->union_offset = 0;
    meta->errors = NULL;
}

void coro_analyze_function(AstNode* function, Scope* scope, CoroMetadata* meta,
                          Arena* arena, ErrorList* errors) {
    meta->errors = errors;

    // 1. Build CFG
    meta->cfg = coro_build_cfg(function, scope, arena, errors);
    if (!meta->cfg) {
        return;
    }

    // 2. Compute liveness
    coro_compute_liveness(meta->cfg, arena);

    // 3. Extract suspend points and their live variables
    meta->suspend_count = 0;

    // Count suspend points
    for (size_t i = 0; i < meta->cfg->block_count; i++) {
        if (meta->cfg->blocks[i]->has_suspend) {
            meta->suspend_count++;
        }
    }

    if (meta->suspend_count == 0) {
        // Not a coroutine - no suspend points
        return;
    }

    // Allocate suspend points and state structs
    meta->suspend_points = arena_alloc(arena,
        sizeof(SuspendPoint) * meta->suspend_count, sizeof(void*));
    meta->state_structs = arena_alloc(arena,
        sizeof(StateStruct) * meta->suspend_count, sizeof(void*));
    meta->state_count = meta->suspend_count;

    // Fill in suspend point info
    size_t sp_idx = 0;
    for (size_t i = 0; i < meta->cfg->block_count; i++) {
        BasicBlock* block = meta->cfg->blocks[i];
        if (!block->has_suspend) {
            continue;
        }

        SuspendPoint* sp = &meta->suspend_points[sp_idx];
        sp->state_id = block->suspend_state_id;
        sp->block = block;

        // Find suspend statement for location
        for (size_t j = 0; j < block->stmt_count; j++) {
            if (block->stmts[j]->kind == AST_SUSPEND_STMT) {
                sp->loc = block->stmts[j]->loc;
                break;
            }
        }

        // Live variables are those live-out from the suspend block
        sp->live_var_count = block->live_out_count;
        sp->live_vars = arena_alloc(arena,
            sizeof(VarLiveness) * sp->live_var_count, sizeof(void*));

        for (size_t j = 0; j < block->live_out_count; j++) {
            sp->live_vars[j].var_name = block->live_out[j];
            sp->live_vars[j].is_live = true;

            // Find type from variable references
            sp->live_vars[j].var_type = NULL;
            for (size_t k = 0; k < block->var_ref_count; k++) {
                if (strcmp(block->var_refs[k].var_name, block->live_out[j]) == 0) {
                    sp->live_vars[j].var_type = block->var_refs[k].var_type;
                    break;
                }
            }

            // If not found in this block, search other blocks
            if (!sp->live_vars[j].var_type) {
                for (size_t b = 0; b < meta->cfg->block_count && !sp->live_vars[j].var_type; b++) {
                    BasicBlock* other = meta->cfg->blocks[b];
                    for (size_t k = 0; k < other->var_ref_count; k++) {
                        if (strcmp(other->var_refs[k].var_name, block->live_out[j]) == 0) {
                            sp->live_vars[j].var_type = other->var_refs[k].var_type;
                            break;
                        }
                    }
                }
            }
        }

        // Create state struct for this suspend point
        StateStruct* state = &meta->state_structs[sp_idx];
        state->state_id = sp->state_id;
        state->field_count = sp->live_var_count;
        state->fields = arena_alloc(arena,
            sizeof(StateField) * state->field_count, sizeof(void*));

        for (size_t j = 0; j < sp->live_var_count; j++) {
            state->fields[j].name = sp->live_vars[j].var_name;
            state->fields[j].type = sp->live_vars[j].var_type;
            state->fields[j].offset = 0;  // Will be computed in layout phase
        }

        sp_idx++;
    }

    // 4. Compute frame layout
    coro_compute_frame_layout(meta, arena);
}
