#include "../stream4-coroutine/coro_metadata.h"
#include <ast.h>
#include <symbol.h>
#include <type.h>
#include <arena.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mock AST creation helpers
AstNode* mock_node_create(AstNodeKind kind, Arena* arena) {
    AstNode* node = arena_alloc(arena, sizeof(AstNode), sizeof(void*));
    memset(node, 0, sizeof(AstNode));
    node->kind = kind;
    node->loc.line = 1;
    node->loc.column = 1;
    node->loc.filename = "demo.tick";
    return node;
}

Type* mock_type_i32(Arena* arena) {
    Type* t = arena_alloc(arena, sizeof(Type), sizeof(void*));
    t->kind = TYPE_I32;
    t->size = 4;
    t->alignment = 4;
    return t;
}

AstNode* mock_identifier(const char* name, Type* type, Arena* arena) {
    AstNode* node = mock_node_create(AST_IDENTIFIER_EXPR, arena);
    node->data.identifier_expr.name = name;
    node->type = type;
    return node;
}

AstNode* mock_literal_int(int64_t val, Arena* arena) {
    AstNode* node = mock_node_create(AST_LITERAL_EXPR, arena);
    node->data.literal_expr.literal_kind = LITERAL_INT;
    node->data.literal_expr.value.int_value = (uint64_t)val;
    node->type = mock_type_i32(arena);
    return node;
}

AstNode* mock_binary_expr(const char* op, AstNode* left, AstNode* right, Arena* arena) {
    AstNode* node = mock_node_create(AST_BINARY_EXPR, arena);
    if (strcmp(op, "+") == 0) node->data.binary_expr.op = BINOP_ADD;
    else node->data.binary_expr.op = BINOP_ADD;
    node->data.binary_expr.left = left;
    node->data.binary_expr.right = right;
    node->type = left->type;
    return node;
}

AstNode* mock_let_stmt(const char* name, Type* type, AstNode* init, Arena* arena) {
    AstNode* node = mock_node_create(AST_LET_STMT, arena);
    node->data.let_decl.name = name;
    node->data.let_decl.init = init;
    node->data.let_decl.type = NULL;
    node->data.let_decl.is_pub = false;
    node->type = type;
    return node;
}

AstNode* mock_suspend_stmt(Arena* arena) {
    return mock_node_create(AST_SUSPEND_STMT, arena);
}

AstNode* mock_return_stmt(AstNode* expr, Arena* arena) {
    AstNode* node = mock_node_create(AST_RETURN_STMT, arena);
    node->data.return_stmt.value = expr;
    return node;
}

AstNode* mock_block_stmt(AstNode** stmts, size_t count, Arena* arena) {
    AstNode* node = mock_node_create(AST_BLOCK_STMT, arena);
    node->data.block_stmt.stmts = stmts;
    node->data.block_stmt.stmt_count = count;
    return node;
}

AstNode* mock_function(const char* name, AstNode* body, Arena* arena) {
    AstNode* node = mock_node_create(AST_FUNCTION_DECL, arena);
    node->data.function_decl.name = name;
    node->data.function_decl.body = body;
    node->data.function_decl.params = NULL;
    node->data.function_decl.param_count = 0;
    node->data.function_decl.return_type = NULL;
    node->data.function_decl.is_pub = true;
    return node;
}

void print_analysis_results(CoroMetadata* meta) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  COROUTINE ANALYSIS RESULTS\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");

    printf("Function: %s\n\n", meta->function_name);

    // Print CFG info
    printf("─── Control Flow Graph ───\n");
    printf("Total blocks: %zu\n", meta->cfg->block_count);
    printf("Entry block: %u\n", meta->cfg->entry->id);
    printf("Exit block: %u\n\n", meta->cfg->exit->id);

    for (size_t i = 0; i < meta->cfg->block_count; i++) {
        BasicBlock* block = meta->cfg->blocks[i];
        printf("  Block %u:\n", block->id);
        printf("    Statements: %zu\n", block->stmt_count);
        printf("    Successors: %zu", block->succ_count);
        if (block->succ_count > 0) {
            printf(" [");
            for (size_t j = 0; j < block->succ_count; j++) {
                printf("%u", block->successors[j]->id);
                if (j < block->succ_count - 1) printf(", ");
            }
            printf("]");
        }
        printf("\n");

        if (block->has_suspend) {
            printf("    *** SUSPEND POINT (State %u) ***\n", block->suspend_state_id);
        }
        printf("\n");
    }

    // Print liveness info
    printf("─── Liveness Analysis ───\n");
    for (size_t i = 0; i < meta->cfg->block_count; i++) {
        BasicBlock* block = meta->cfg->blocks[i];

        printf("  Block %u:\n", block->id);

        printf("    GEN = {");
        for (size_t j = 0; j < block->gen_count; j++) {
            printf("%s", block->gen_set[j]);
            if (j < block->gen_count - 1) printf(", ");
        }
        printf("}\n");

        printf("    KILL = {");
        for (size_t j = 0; j < block->kill_count; j++) {
            printf("%s", block->kill_set[j]);
            if (j < block->kill_count - 1) printf(", ");
        }
        printf("}\n");

        printf("    LIVE_IN = {");
        for (size_t j = 0; j < block->live_in_count; j++) {
            printf("%s", block->live_in[j]);
            if (j < block->live_in_count - 1) printf(", ");
        }
        printf("}\n");

        printf("    LIVE_OUT = {");
        for (size_t j = 0; j < block->live_out_count; j++) {
            printf("%s", block->live_out[j]);
            if (j < block->live_out_count - 1) printf(", ");
        }
        printf("}\n\n");
    }

    // Print suspend points
    printf("─── Suspend Points ───\n");
    printf("Total suspend points: %zu\n\n", meta->suspend_count);

    for (size_t i = 0; i < meta->suspend_count; i++) {
        SuspendPoint* sp = &meta->suspend_points[i];
        printf("  Suspend Point %u:\n", sp->state_id);
        printf("    Location: %s:%d:%d\n", sp->loc.filename, sp->loc.line, sp->loc.column);
        printf("    Block: %u\n", sp->block->id);
        printf("    Live variables (%zu):\n", sp->live_var_count);

        for (size_t j = 0; j < sp->live_var_count; j++) {
            printf("      - %s", sp->live_vars[j].var_name);
            if (sp->live_vars[j].var_type) {
                printf(" : i32");  // Simplified
            }
            printf("\n");
        }
        printf("\n");
    }

    // Print state structs
    printf("─── State Structs ───\n");
    for (size_t i = 0; i < meta->state_count; i++) {
        StateStruct* state = &meta->state_structs[i];
        printf("  State %u:\n", state->state_id);
        printf("    Size: %zu bytes\n", state->size);
        printf("    Alignment: %zu bytes\n", state->alignment);
        printf("    Fields (%zu):\n", state->field_count);

        for (size_t j = 0; j < state->field_count; j++) {
            printf("      - %s : i32 (offset=%zu)\n",
                   state->fields[j].name, state->fields[j].offset);
        }
        printf("\n");
    }

    // Print frame layout
    printf("─── Frame Layout ───\n");
    printf("Total frame size: %zu bytes\n", meta->total_frame_size);
    printf("Frame alignment: %zu bytes\n", meta->frame_alignment);
    printf("Tag size: %zu bytes\n", meta->tag_size);
    printf("Tag offset: %zu bytes\n", meta->tag_offset);
    printf("Union offset: %zu bytes\n\n", meta->union_offset);

    printf("Frame structure:\n");
    printf("  struct CoroFrame {\n");
    printf("    uint32_t state_tag;  // offset=%zu, size=%zu\n",
           meta->tag_offset, meta->tag_size);
    printf("    union {              // offset=%zu\n", meta->union_offset);
    for (size_t i = 0; i < meta->state_count; i++) {
        printf("      struct {       // State %zu: size=%zu\n",
               i, meta->state_structs[i].size);
        for (size_t j = 0; j < meta->state_structs[i].field_count; j++) {
            printf("        i32 %s;  // offset=%zu\n",
                   meta->state_structs[i].fields[j].name,
                   meta->state_structs[i].fields[j].offset);
        }
        printf("      } state_%zu;\n", i);
    }
    printf("    } data;\n");
    printf("  };\n\n");

    printf("═══════════════════════════════════════════════════════════════\n\n");
}

int main() {
    printf("\n╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║  TICK COMPILER - COROUTINE ANALYSIS DEMONSTRATION            ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    // Initialize context
    Arena arena;
    arena_init(&arena, 4096);

    ErrorList errors;
    error_list_init(&errors, &arena);

    Scope global_scope;
    scope_init(&global_scope, SCOPE_MODULE, NULL, &arena);

    printf("\n");
    printf("Source Code:\n");
    printf("────────────────────────────────────────────────────────────────\n");
    printf("pub async fn example() -> i32 {\n");
    printf("    let x: i32 = 1;\n");
    printf("    suspend;                  // State 0: must save x\n");
    printf("    let y: i32 = x + 2;\n");
    printf("    suspend;                  // State 1: must save y\n");
    printf("    return y;\n");
    printf("}\n");
    printf("────────────────────────────────────────────────────────────────\n");

    // Build AST for: let x = 1; suspend; let y = x + 2; suspend; return y;
    AstNode* stmts[5];
    stmts[0] = mock_let_stmt("x", mock_type_i32(&arena),
                            mock_literal_int(1, &arena), &arena);
    stmts[1] = mock_suspend_stmt(&arena);
    stmts[2] = mock_let_stmt("y", mock_type_i32(&arena),
                            mock_binary_expr("+",
                                mock_identifier("x", mock_type_i32(&arena), &arena),
                                mock_literal_int(2, &arena), &arena),
                            &arena);
    stmts[3] = mock_suspend_stmt(&arena);
    stmts[4] = mock_return_stmt(
                    mock_identifier("y", mock_type_i32(&arena), &arena), &arena);

    AstNode* body = mock_block_stmt(stmts, 5, &arena);
    AstNode* func = mock_function("example", body, &arena);

    // Run analysis
    CoroMetadata meta;
    coro_metadata_init(&meta, func, &arena);
    coro_analyze_function(func, &global_scope, &meta, &arena, &errors);

    // Print detailed results
    print_analysis_results(&meta);

    printf("Key Observations:\n");
    printf("─────────────────\n");
    printf("1. Two suspend points detected (state 0 and state 1)\n");
    printf("2. At first suspend: x is live (needed after resume)\n");
    printf("3. At second suspend: y is live (needed for return)\n");
    printf("4. Each state struct contains only live variables\n");
    printf("5. Frame is a tagged union of all states\n");
    printf("6. Total frame size optimized based on liveness\n");
    printf("\n");

    printf("Space Efficiency:\n");
    printf("─────────────────\n");
    printf("Without analysis: would need to save all vars (x, y) = 8 bytes\n");
    printf("With liveness:    each state saves only what's needed\n");
    printf("  State 0: x only = 4 bytes\n");
    printf("  State 1: y only = 4 bytes\n");
    printf("  Union size: 4 bytes (max of states)\n");
    printf("  Total frame: 8 bytes (tag + union)\n");
    printf("\n");

    arena_free(&arena);
    return 0;
}
