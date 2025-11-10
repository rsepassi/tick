#include "../coro_metadata.h"
#include "../ast.h"
#include "../symbol.h"
#include "../type.h"
#include "../arena.h"
#include "../error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// ============================================================================
// TEST UTILITIES - Mock AST Building
// ============================================================================

typedef struct TestContext {
    Arena arena;
    ErrorList errors;
    Scope global_scope;
} TestContext;

void test_context_init(TestContext* ctx) {
    arena_init(&ctx->arena, 4096);
    error_list_init(&ctx->errors, &ctx->arena);
    scope_init(&ctx->global_scope, SCOPE_MODULE, NULL, &ctx->arena);
}

void test_context_cleanup(TestContext* ctx) {
    arena_free(&ctx->arena);
}

AstNode* mock_node_create(AstNodeKind kind, Arena* arena) {
    AstNode* node = arena_alloc(arena, sizeof(AstNode), sizeof(void*));
    memset(node, 0, sizeof(AstNode));
    node->kind = kind;
    node->loc.line = 1;
    node->loc.column = 1;
    node->loc.filename = "test.tick";
    return node;
}

Type* mock_type_i32(Arena* arena) {
    Type* t = arena_alloc(arena, sizeof(Type), sizeof(void*));
    t->kind = TYPE_I32;
    t->size = 4;
    t->alignment = 4;
    return t;
}

Type* mock_type_i64(Arena* arena) {
    Type* t = arena_alloc(arena, sizeof(Type), sizeof(void*));
    t->kind = TYPE_I64;
    t->size = 8;
    t->alignment = 8;
    return t;
}

Type* mock_type_bool(Arena* arena) {
    Type* t = arena_alloc(arena, sizeof(Type), sizeof(void*));
    t->kind = TYPE_BOOL;
    t->size = 1;
    t->alignment = 1;
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
    // Map string op to BinaryOp enum (simplified for testing)
    if (strcmp(op, "+") == 0) node->data.binary_expr.op = BINOP_ADD;
    else if (strcmp(op, ">") == 0) node->data.binary_expr.op = BINOP_GT;
    else node->data.binary_expr.op = BINOP_ADD; // Default
    node->data.binary_expr.left = left;
    node->data.binary_expr.right = right;
    node->type = left->type;
    return node;
}

AstNode* mock_var_stmt(const char* name, Type* type, AstNode* init, Arena* arena) {
    AstNode* node = mock_node_create(AST_VAR_STMT, arena);
    node->data.var_decl.name = name;
    node->data.var_decl.init = init;
    node->data.var_decl.type = NULL;
    node->data.var_decl.is_volatile = false;
    node->data.var_decl.is_pub = false;
    node->type = type;
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
    AstNode* node = mock_node_create(AST_SUSPEND_STMT, arena);
    return node;
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

AstNode* mock_if_stmt(AstNode* cond, AstNode* then_block, AstNode* else_block, Arena* arena) {
    AstNode* node = mock_node_create(AST_IF_STMT, arena);
    node->data.if_stmt.condition = cond;
    node->data.if_stmt.then_block = then_block;
    node->data.if_stmt.else_block = else_block;
    return node;
}

AstNode* mock_expr_stmt(AstNode* expr, Arena* arena) {
    AstNode* node = mock_node_create(AST_EXPR_STMT, arena);
    node->data.expr_stmt.expr = expr;
    return node;
}

AstNode* mock_function(const char* name, AstNode* body, bool is_async, Arena* arena) {
    AstNode* node = mock_node_create(AST_FUNCTION_DECL, arena);
    node->data.function_decl.name = name;
    node->data.function_decl.body = body;
    node->data.function_decl.params = NULL;
    node->data.function_decl.param_count = 0;
    node->data.function_decl.return_type = NULL;
    node->data.function_decl.is_pub = true;
    return node;
}

// ============================================================================
// TEST 1: Basic CFG Construction
// ============================================================================

void test_cfg_construction_linear() {
    printf("TEST: CFG construction - linear sequence\n");

    TestContext ctx;
    test_context_init(&ctx);

    // Create: let x = 1; let y = 2; return x + y;
    AstNode* stmts[3];
    stmts[0] = mock_let_stmt("x", mock_type_i32(&ctx.arena),
                            mock_literal_int(1, &ctx.arena), &ctx.arena);
    stmts[1] = mock_let_stmt("y", mock_type_i32(&ctx.arena),
                            mock_literal_int(2, &ctx.arena), &ctx.arena);
    stmts[2] = mock_return_stmt(
        mock_binary_expr("+",
            mock_identifier("x", mock_type_i32(&ctx.arena), &ctx.arena),
            mock_identifier("y", mock_type_i32(&ctx.arena), &ctx.arena),
            &ctx.arena),
        &ctx.arena);

    AstNode* body = mock_block_stmt(stmts, 3, &ctx.arena);
    AstNode* func = mock_function("test_func", body, false, &ctx.arena);

    CFG* cfg = coro_build_cfg(func, &ctx.global_scope, &ctx.arena, &ctx.errors);

    assert(cfg != NULL);
    assert(cfg->entry != NULL);
    assert(cfg->exit != NULL);
    assert(cfg->block_count >= 2);  // At least entry and exit

    printf("  PASS: CFG has %zu blocks\n", cfg->block_count);

    test_context_cleanup(&ctx);
}

void test_cfg_construction_if_stmt() {
    printf("TEST: CFG construction - if statement\n");

    TestContext ctx;
    test_context_init(&ctx);

    // Create: let x = 1; if (x > 0) { let y = 2; } return x;
    AstNode* then_stmts[1];
    then_stmts[0] = mock_let_stmt("y", mock_type_i32(&ctx.arena),
                                 mock_literal_int(2, &ctx.arena), &ctx.arena);
    AstNode* then_block = mock_block_stmt(then_stmts, 1, &ctx.arena);

    AstNode* condition = mock_binary_expr(">",
        mock_identifier("x", mock_type_i32(&ctx.arena), &ctx.arena),
        mock_literal_int(0, &ctx.arena), &ctx.arena);

    AstNode* stmts[3];
    stmts[0] = mock_let_stmt("x", mock_type_i32(&ctx.arena),
                            mock_literal_int(1, &ctx.arena), &ctx.arena);
    stmts[1] = mock_if_stmt(condition, then_block, NULL, &ctx.arena);
    stmts[2] = mock_return_stmt(
        mock_identifier("x", mock_type_i32(&ctx.arena), &ctx.arena), &ctx.arena);

    AstNode* body = mock_block_stmt(stmts, 3, &ctx.arena);
    AstNode* func = mock_function("test_if", body, false, &ctx.arena);

    CFG* cfg = coro_build_cfg(func, &ctx.global_scope, &ctx.arena, &ctx.errors);

    assert(cfg != NULL);
    assert(cfg->block_count >= 4);  // entry, if, then, merge, exit

    printf("  PASS: CFG with if statement has %zu blocks\n", cfg->block_count);

    test_context_cleanup(&ctx);
}

// ============================================================================
// TEST 2: Suspend Point Detection
// ============================================================================

void test_suspend_point_detection() {
    printf("TEST: Suspend point detection\n");

    TestContext ctx;
    test_context_init(&ctx);

    // Create: let x = 1; suspend; let y = 2; return y;
    AstNode* stmts[4];
    stmts[0] = mock_let_stmt("x", mock_type_i32(&ctx.arena),
                            mock_literal_int(1, &ctx.arena), &ctx.arena);
    stmts[1] = mock_suspend_stmt(&ctx.arena);
    stmts[2] = mock_let_stmt("y", mock_type_i32(&ctx.arena),
                            mock_literal_int(2, &ctx.arena), &ctx.arena);
    stmts[3] = mock_return_stmt(
        mock_identifier("y", mock_type_i32(&ctx.arena), &ctx.arena), &ctx.arena);

    AstNode* body = mock_block_stmt(stmts, 4, &ctx.arena);
    AstNode* func = mock_function("test_suspend", body, true, &ctx.arena);

    CFG* cfg = coro_build_cfg(func, &ctx.global_scope, &ctx.arena, &ctx.errors);

    assert(cfg != NULL);

    // Count suspend points
    size_t suspend_count = 0;
    for (size_t i = 0; i < cfg->block_count; i++) {
        if (cfg->blocks[i]->has_suspend) {
            suspend_count++;
        }
    }

    assert(suspend_count == 1);
    printf("  PASS: Found %zu suspend point(s)\n", suspend_count);

    test_context_cleanup(&ctx);
}

// ============================================================================
// TEST 3: Variable Reference Tracking
// ============================================================================

void test_var_ref_tracking() {
    printf("TEST: Variable reference tracking\n");

    TestContext ctx;
    test_context_init(&ctx);

    // Create: let x = 1; let y = x + 2; return y;
    AstNode* stmts[3];
    stmts[0] = mock_let_stmt("x", mock_type_i32(&ctx.arena),
                            mock_literal_int(1, &ctx.arena), &ctx.arena);
    stmts[1] = mock_let_stmt("y", mock_type_i32(&ctx.arena),
                            mock_binary_expr("+",
                                mock_identifier("x", mock_type_i32(&ctx.arena), &ctx.arena),
                                mock_literal_int(2, &ctx.arena), &ctx.arena),
                            &ctx.arena);
    stmts[2] = mock_return_stmt(
        mock_identifier("y", mock_type_i32(&ctx.arena), &ctx.arena), &ctx.arena);

    AstNode* body = mock_block_stmt(stmts, 3, &ctx.arena);
    AstNode* func = mock_function("test_refs", body, false, &ctx.arena);

    CFG* cfg = coro_build_cfg(func, &ctx.global_scope, &ctx.arena, &ctx.errors);

    assert(cfg != NULL);

    // Check that entry block has variable references
    BasicBlock* entry = cfg->entry;
    assert(entry->var_ref_count > 0);

    // Count defs and uses
    size_t def_count = 0;
    size_t use_count = 0;
    for (size_t i = 0; i < entry->var_ref_count; i++) {
        if (entry->var_refs[i].is_def) {
            def_count++;
        } else {
            use_count++;
        }
    }

    assert(def_count == 2);  // x and y
    assert(use_count >= 2);  // x in y's init, y in return

    printf("  PASS: Found %zu defs, %zu uses\n", def_count, use_count);

    test_context_cleanup(&ctx);
}

// ============================================================================
// TEST 4: Gen/Kill Set Computation
// ============================================================================

void test_gen_kill_computation() {
    printf("TEST: Gen/Kill set computation\n");

    TestContext ctx;
    test_context_init(&ctx);

    // Create: let y = x + 1;  (x is in gen, y is in kill)
    AstNode* stmts[1];
    stmts[0] = mock_let_stmt("y", mock_type_i32(&ctx.arena),
                            mock_binary_expr("+",
                                mock_identifier("x", mock_type_i32(&ctx.arena), &ctx.arena),
                                mock_literal_int(1, &ctx.arena), &ctx.arena),
                            &ctx.arena);

    AstNode* body = mock_block_stmt(stmts, 1, &ctx.arena);
    AstNode* func = mock_function("test_genkill", body, false, &ctx.arena);

    CFG* cfg = coro_build_cfg(func, &ctx.global_scope, &ctx.arena, &ctx.errors);

    // Compute gen/kill
    for (size_t i = 0; i < cfg->block_count; i++) {
        block_compute_gen_kill(cfg->blocks[i], cfg, &ctx.arena);
    }

    // Entry block should have x in gen, y in kill
    BasicBlock* entry = cfg->entry;
    bool found_x_in_gen = false;
    bool found_y_in_kill = false;

    for (size_t i = 0; i < entry->gen_count; i++) {
        if (strcmp(entry->gen_set[i], "x") == 0) {
            found_x_in_gen = true;
        }
    }

    for (size_t i = 0; i < entry->kill_count; i++) {
        if (strcmp(entry->kill_set[i], "y") == 0) {
            found_y_in_kill = true;
        }
    }

    assert(found_x_in_gen);
    assert(found_y_in_kill);

    printf("  PASS: Gen set contains x, Kill set contains y\n");

    test_context_cleanup(&ctx);
}

// ============================================================================
// TEST 5: Liveness Analysis
// ============================================================================

void test_liveness_analysis() {
    printf("TEST: Liveness analysis\n");

    TestContext ctx;
    test_context_init(&ctx);

    // Create: let x = 1; suspend; return x;
    // x should be live across the suspend point
    AstNode* stmts[3];
    stmts[0] = mock_let_stmt("x", mock_type_i32(&ctx.arena),
                            mock_literal_int(1, &ctx.arena), &ctx.arena);
    stmts[1] = mock_suspend_stmt(&ctx.arena);
    stmts[2] = mock_return_stmt(
        mock_identifier("x", mock_type_i32(&ctx.arena), &ctx.arena), &ctx.arena);

    AstNode* body = mock_block_stmt(stmts, 3, &ctx.arena);
    AstNode* func = mock_function("test_liveness", body, true, &ctx.arena);

    CFG* cfg = coro_build_cfg(func, &ctx.global_scope, &ctx.arena, &ctx.errors);

    // Run liveness analysis
    coro_compute_liveness(cfg, &ctx.arena);

    // Find suspend block
    BasicBlock* suspend_block = NULL;
    for (size_t i = 0; i < cfg->block_count; i++) {
        if (cfg->blocks[i]->has_suspend) {
            suspend_block = cfg->blocks[i];
            break;
        }
    }

    assert(suspend_block != NULL);

    // x should be in live_out of suspend block
    bool x_is_live = false;
    for (size_t i = 0; i < suspend_block->live_out_count; i++) {
        if (strcmp(suspend_block->live_out[i], "x") == 0) {
            x_is_live = true;
            break;
        }
    }

    assert(x_is_live);
    printf("  PASS: Variable x is live across suspend point\n");

    test_context_cleanup(&ctx);
}

// ============================================================================
// TEST 6: Frame Layout Computation
// ============================================================================

void test_frame_layout() {
    printf("TEST: Frame layout computation\n");

    TestContext ctx;
    test_context_init(&ctx);

    // Create metadata with two state structs of different sizes
    CoroMetadata meta;
    meta.state_count = 2;
    meta.state_structs = arena_alloc(&ctx.arena,
        sizeof(StateStruct) * 2, sizeof(void*));

    // State 0: { i32 x; i64 y; }
    meta.state_structs[0].state_id = 0;
    meta.state_structs[0].field_count = 2;
    meta.state_structs[0].fields = arena_alloc(&ctx.arena,
        sizeof(StateField) * 2, sizeof(void*));
    meta.state_structs[0].fields[0].name = "x";
    meta.state_structs[0].fields[0].type = mock_type_i32(&ctx.arena);
    meta.state_structs[0].fields[1].name = "y";
    meta.state_structs[0].fields[1].type = mock_type_i64(&ctx.arena);

    // State 1: { i32 z; }
    meta.state_structs[1].state_id = 1;
    meta.state_structs[1].field_count = 1;
    meta.state_structs[1].fields = arena_alloc(&ctx.arena,
        sizeof(StateField) * 1, sizeof(void*));
    meta.state_structs[1].fields[0].name = "z";
    meta.state_structs[1].fields[0].type = mock_type_i32(&ctx.arena);

    coro_compute_frame_layout(&meta, &ctx.arena);

    // Verify layout
    assert(meta.tag_size == sizeof(uint32_t));
    assert(meta.total_frame_size > 0);
    assert(meta.frame_alignment >= sizeof(uint32_t));

    // State 0 should be larger than State 1
    assert(meta.state_structs[0].size > meta.state_structs[1].size);

    printf("  PASS: Frame size=%zu, alignment=%zu\n",
           meta.total_frame_size, meta.frame_alignment);
    printf("       State 0: size=%zu, State 1: size=%zu\n",
           meta.state_structs[0].size, meta.state_structs[1].size);

    test_context_cleanup(&ctx);
}

// ============================================================================
// TEST 7: Complete Analysis Pipeline
// ============================================================================

void test_complete_analysis() {
    printf("TEST: Complete coroutine analysis pipeline\n");

    TestContext ctx;
    test_context_init(&ctx);

    // Create: let x = 1; suspend; let y = x; suspend; return y;
    // Should have 2 suspend points with different live variables
    AstNode* stmts[5];
    stmts[0] = mock_let_stmt("x", mock_type_i32(&ctx.arena),
                            mock_literal_int(1, &ctx.arena), &ctx.arena);
    stmts[1] = mock_suspend_stmt(&ctx.arena);
    stmts[2] = mock_let_stmt("y", mock_type_i32(&ctx.arena),
                            mock_identifier("x", mock_type_i32(&ctx.arena), &ctx.arena),
                            &ctx.arena);
    stmts[3] = mock_suspend_stmt(&ctx.arena);
    stmts[4] = mock_return_stmt(
        mock_identifier("y", mock_type_i32(&ctx.arena), &ctx.arena), &ctx.arena);

    AstNode* body = mock_block_stmt(stmts, 5, &ctx.arena);
    AstNode* func = mock_function("test_complete", body, true, &ctx.arena);

    CoroMetadata meta;
    coro_metadata_init(&meta, func, &ctx.arena);
    coro_analyze_function(func, &ctx.global_scope, &meta, &ctx.arena, &ctx.errors);

    // Should have 2 suspend points
    assert(meta.suspend_count == 2);

    // Should have 2 state structs
    assert(meta.state_count == 2);

    // Frame should be computed
    assert(meta.total_frame_size > 0);

    // Verify each suspend point has live variables
    for (size_t i = 0; i < meta.suspend_count; i++) {
        printf("  Suspend point %u: %zu live vars\n",
               meta.suspend_points[i].state_id,
               meta.suspend_points[i].live_var_count);
    }

    printf("  PASS: Complete analysis with %zu suspend points\n",
           meta.suspend_count);

    test_context_cleanup(&ctx);
}

// ============================================================================
// TEST 8: Variable Set Operations
// ============================================================================

void test_var_set_operations() {
    printf("TEST: Variable set operations\n");

    TestContext ctx;
    test_context_init(&ctx);

    const char** set = NULL;
    size_t count = 0;

    // Test add
    var_set_add(&set, &count, "x", &ctx.arena);
    assert(count == 1);
    assert(var_is_in_set("x", set, count));

    // Test duplicate add
    var_set_add(&set, &count, "x", &ctx.arena);
    assert(count == 1);

    // Test add more
    var_set_add(&set, &count, "y", &ctx.arena);
    var_set_add(&set, &count, "z", &ctx.arena);
    assert(count == 3);

    // Test union
    const char** set2 = NULL;
    size_t count2 = 0;
    var_set_add(&set2, &count2, "a", &ctx.arena);
    var_set_add(&set2, &count2, "x", &ctx.arena);

    var_set_union(&set, &count, set2, count2, &ctx.arena);
    assert(count == 4);  // x, y, z, a
    assert(var_is_in_set("a", set, count));

    // Test diff
    const char** subtract = NULL;
    size_t sub_count = 0;
    var_set_add(&subtract, &sub_count, "x", &ctx.arena);
    var_set_add(&subtract, &sub_count, "y", &ctx.arena);

    var_set_diff(&set, &count, subtract, sub_count, &ctx.arena);
    assert(count == 2);  // z, a
    assert(!var_is_in_set("x", set, count));
    assert(!var_is_in_set("y", set, count));
    assert(var_is_in_set("z", set, count));
    assert(var_is_in_set("a", set, count));

    printf("  PASS: Variable set operations work correctly\n");

    test_context_cleanup(&ctx);
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main() {
    printf("=== Coroutine Analysis Test Suite ===\n\n");

    test_var_set_operations();
    test_cfg_construction_linear();
    test_cfg_construction_if_stmt();
    test_suspend_point_detection();
    test_var_ref_tracking();
    test_gen_kill_computation();
    test_liveness_analysis();
    test_frame_layout();
    test_complete_analysis();

    printf("\n=== All Tests Passed! ===\n");
    return 0;
}
