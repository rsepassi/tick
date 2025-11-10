// Test suite for type checker

#include "type.h"
#include "ast.h"
#include "symbol.h"
#include "error.h"
#include "arena.h"
#include "string_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// External APIs
void resolver_resolve(AstNode* module_node, SymbolTable* symbol_table,
                     StringPool* string_pool, ErrorList* errors, Arena* arena);
void typeck_check_module(AstNode* module_node, SymbolTable* symbol_table,
                        ErrorList* errors, Arena* arena);

// Test harness
static int test_count = 0;
static int test_passed = 0;

#define TEST(name) \
    static void test_##name(); \
    static void run_test_##name() { \
        test_count++; \
        printf("Running test: %s\n", #name); \
        test_##name(); \
        test_passed++; \
        printf("  PASSED\n"); \
    } \
    static void test_##name()

// Helper: Create AST node
static AstNode* make_node(AstNodeKind kind, Arena* arena) {
    AstNode* node = arena_alloc(arena, sizeof(AstNode), 8);
    memset(node, 0, sizeof(AstNode));
    node->kind = kind;
    node->loc.line = 1;
    node->loc.column = 1;
    node->loc.filename = "test.tick";
    return node;
}

// Helper: Create module
static AstNode* make_module(const char* name, AstNode** decls, size_t decl_count, Arena* arena) {
    AstNode* node = make_node(AST_MODULE, arena);
    node->data.module.name = name;
    node->data.module.decls = decls;
    node->data.module.decl_count = decl_count;
    return node;
}

// Helper: Create primitive type node
// Note: Type nodes use AstTypeNode struct, cast AstNode for compatibility
static AstNode* make_prim_type(const char* name, Arena* arena) {
    AstNode* node = make_node(AST_TYPE_PRIMITIVE, arena);
    // Cast to AstTypeNode to access type-specific data
    AstTypeNode* type_node = (AstTypeNode*)node;
    type_node->data.primitive.name = name;
    return node;
}

// Helper: Create function with typed parameters
static AstNode* make_typed_function(const char* name, AstParam* params, size_t param_count,
                                   AstNode* return_type, AstNode* body, Arena* arena) {
    AstNode* node = make_node(AST_FUNCTION_DECL, arena);
    node->data.function_decl.name = name;
    node->data.function_decl.params = params;
    node->data.function_decl.param_count = param_count;
    node->data.function_decl.return_type = return_type;
    node->data.function_decl.body = body;
    node->data.function_decl.is_pub = true;
    return node;
}

// Helper: Create struct with fields
static AstNode* make_struct_with_fields(const char* name, AstField* fields,
                                       size_t field_count, Arena* arena) {
    AstNode* node = make_node(AST_STRUCT_DECL, arena);
    node->data.struct_decl.name = name;
    node->data.struct_decl.fields = fields;
    node->data.struct_decl.field_count = field_count;
    node->data.struct_decl.is_pub = true;
    node->data.struct_decl.is_packed = false;
    return node;
}

// Helper: Create literal
static AstNode* make_int_literal(int64_t value, Arena* arena) {
    AstNode* node = make_node(AST_LITERAL_EXPR, arena);
    node->data.literal_expr.literal_kind = LITERAL_INT;
    node->data.literal_expr.value.int_value = value;
    return node;
}

static AstNode* make_bool_literal(bool value, Arena* arena) {
    AstNode* node = make_node(AST_LITERAL_EXPR, arena);
    node->data.literal_expr.literal_kind = LITERAL_BOOL;
    node->data.literal_expr.value.bool_value = value;
    return node;
}

// Helper: Create binary expression
static AstNode* make_binary(BinaryOp op, AstNode* left, AstNode* right, Arena* arena) {
    AstNode* node = make_node(AST_BINARY_EXPR, arena);
    node->data.binary_expr.op = op;
    node->data.binary_expr.left = left;
    node->data.binary_expr.right = right;
    return node;
}

// Helper: Create block
static AstNode* make_block(AstNode** stmts, size_t stmt_count, Arena* arena) {
    AstNode* node = make_node(AST_BLOCK_STMT, arena);
    node->data.block_stmt.stmts = stmts;
    node->data.block_stmt.stmt_count = stmt_count;
    return node;
}

// Helper: Create let statement
static AstNode* make_let_typed(const char* name, AstNode* type_node, AstNode* init_expr, Arena* arena) {
    AstNode* node = make_node(AST_LET_STMT, arena);
    node->data.let_decl.name = name;
    node->data.let_decl.type = type_node;
    node->data.let_decl.init = init_expr;
    return node;
}

// Helper: Create return statement
static AstNode* make_return(AstNode* value, Arena* arena) {
    AstNode* node = make_node(AST_RETURN_STMT, arena);
    node->data.return_stmt.value = value;
    return node;
}

// Helper: Run resolver and typeck
static void resolve_and_check(AstNode* module, SymbolTable* symtab,
                             StringPool* pool, ErrorList* errors, Arena* arena) {
    resolver_resolve(module, symtab, pool, errors, arena);
    if (!error_list_has_errors(errors)) {
        typeck_check_module(module, symtab, errors, arena);
    }
}

// Test: Integer literal type inference
TEST(integer_literal) {
    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable symtab;
    symbol_table_init(&symtab, &arena);

    // Create: fn test() -> i64 { return 42; }
    AstNode* lit = make_int_literal(42, &arena);
    AstNode* ret = make_return(lit, &arena);
    AstNode* stmts[] = {ret};
    AstNode* body = make_block(stmts, 1, &arena);
    AstNode* ret_type = make_prim_type("i64", &arena);
    AstNode* func = make_typed_function("test", NULL, 0, ret_type, body, &arena);

    AstNode* decls[] = {func};
    AstNode* module = make_module("test", decls, 1, &arena);

    // Resolve and check
    resolve_and_check(module, &symtab, &pool, &errors, &arena);

    // Should have no errors
    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
    }
    assert(!error_list_has_errors(&errors));

    // Verify literal has type
    assert(lit->type != NULL);
    assert(lit->type == TYPE_I64_SINGLETON);

    arena_free(&arena);
}

// Test: Boolean literal type
TEST(boolean_literal) {
    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable symtab;
    symbol_table_init(&symtab, &arena);

    // Create: fn test() -> bool { return true; }
    AstNode* lit = make_bool_literal(true, &arena);
    AstNode* ret = make_return(lit, &arena);
    AstNode* stmts[] = {ret};
    AstNode* body = make_block(stmts, 1, &arena);
    AstNode* ret_type = make_prim_type("bool", &arena);
    AstNode* func = make_typed_function("test", NULL, 0, ret_type, body, &arena);

    AstNode* decls[] = {func};
    AstNode* module = make_module("test", decls, 1, &arena);

    // Resolve and check
    resolve_and_check(module, &symtab, &pool, &errors, &arena);

    // Should have no errors
    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
    }
    assert(!error_list_has_errors(&errors));

    // Verify literal has type
    assert(lit->type != NULL);
    assert(lit->type == TYPE_BOOL_SINGLETON);

    arena_free(&arena);
}

// Test: Binary expression type checking
TEST(binary_expression) {
    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable symtab;
    symbol_table_init(&symtab, &arena);

    // Create: fn test() -> i64 { return 10 + 32; }
    AstNode* left = make_int_literal(10, &arena);
    AstNode* right = make_int_literal(32, &arena);
    AstNode* add = make_binary(BINOP_ADD, left, right, &arena);
    AstNode* ret = make_return(add, &arena);
    AstNode* stmts[] = {ret};
    AstNode* body = make_block(stmts, 1, &arena);
    AstNode* ret_type = make_prim_type("i64", &arena);
    AstNode* func = make_typed_function("test", NULL, 0, ret_type, body, &arena);

    AstNode* decls[] = {func};
    AstNode* module = make_module("test", decls, 1, &arena);

    // Resolve and check
    resolve_and_check(module, &symtab, &pool, &errors, &arena);

    // Should have no errors
    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
    }
    assert(!error_list_has_errors(&errors));

    // Verify expression has type
    assert(add->type != NULL);
    assert(add->type == TYPE_I64_SINGLETON);

    arena_free(&arena);
}

// Test: Type mismatch in binary expression (should error)
TEST(binary_type_mismatch) {
    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable symtab;
    symbol_table_init(&symtab, &arena);

    // Create: fn test() -> bool { return 10 + true; }  // Type error!
    AstNode* left = make_int_literal(10, &arena);
    AstNode* right = make_bool_literal(true, &arena);
    AstNode* add = make_binary(BINOP_ADD, left, right, &arena);
    AstNode* ret = make_return(add, &arena);
    AstNode* stmts[] = {ret};
    AstNode* body = make_block(stmts, 1, &arena);
    AstNode* ret_type = make_prim_type("bool", &arena);
    AstNode* func = make_typed_function("test", NULL, 0, ret_type, body, &arena);

    AstNode* decls[] = {func};
    AstNode* module = make_module("test", decls, 1, &arena);

    // Resolve and check
    resolve_and_check(module, &symtab, &pool, &errors, &arena);

    // Should have errors
    assert(error_list_has_errors(&errors));

    arena_free(&arena);
}

// Test: Local variable type inference
TEST(variable_inference) {
    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable symtab;
    symbol_table_init(&symtab, &arena);

    // Create: fn test() { let x = 42; }
    AstNode* lit = make_int_literal(42, &arena);
    AstNode* let = make_let_typed("x", NULL, lit, &arena);  // No type annotation
    AstNode* stmts[] = {let};
    AstNode* body = make_block(stmts, 1, &arena);
    AstNode* func = make_typed_function("test", NULL, 0, NULL, body, &arena);

    AstNode* decls[] = {func};
    AstNode* module = make_module("test", decls, 1, &arena);

    // Resolve and check
    resolve_and_check(module, &symtab, &pool, &errors, &arena);

    // Should have no errors
    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
    }
    assert(!error_list_has_errors(&errors));

    // Verify variable type was inferred
    assert(let->data.let_decl.type != NULL);
    assert(let->data.let_decl.type == TYPE_I64_SINGLETON);

    arena_free(&arena);
}

// Test: Explicit type annotation matches initializer
TEST(explicit_type_match) {
    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable symtab;
    symbol_table_init(&symtab, &arena);

    // Create: fn test() { let x: i64 = 42; }
    AstNode* lit = make_int_literal(42, &arena);
    AstNode* type_node = make_prim_type("i64", &arena);
    AstNode* let = make_let_typed("x", type_node, lit, &arena);
    AstNode* stmts[] = {let};
    AstNode* body = make_block(stmts, 1, &arena);
    AstNode* func = make_typed_function("test", NULL, 0, NULL, body, &arena);

    AstNode* decls[] = {func};
    AstNode* module = make_module("test", decls, 1, &arena);

    // Resolve and check
    resolve_and_check(module, &symtab, &pool, &errors, &arena);

    // Should have no errors
    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
    }
    assert(!error_list_has_errors(&errors));

    // Verify variable has correct type
    assert(let->data.let_decl.type != NULL);
    assert(let->data.let_decl.type == TYPE_I64_SINGLETON);

    arena_free(&arena);
}

// Test: Type mismatch between annotation and initializer (should error)
TEST(explicit_type_mismatch) {
    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable symtab;
    symbol_table_init(&symtab, &arena);

    // Create: fn test() { let x: bool = 42; }  // Type error!
    AstNode* lit = make_int_literal(42, &arena);
    AstNode* type_node = make_prim_type("bool", &arena);
    AstNode* let = make_let_typed("x", type_node, lit, &arena);
    AstNode* stmts[] = {let};
    AstNode* body = make_block(stmts, 1, &arena);
    AstNode* func = make_typed_function("test", NULL, 0, NULL, body, &arena);

    AstNode* decls[] = {func};
    AstNode* module = make_module("test", decls, 1, &arena);

    // Resolve and check
    resolve_and_check(module, &symtab, &pool, &errors, &arena);

    // Should have errors
    assert(error_list_has_errors(&errors));

    arena_free(&arena);
}

// Test: Struct type creation
TEST(struct_type) {
    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable symtab;
    symbol_table_init(&symtab, &arena);

    // Create: struct Point { x: i32, y: i32 }
    AstField* fields = arena_alloc(&arena, sizeof(AstField) * 2, 8);
    fields[0].name = "x";
    fields[0].type = make_prim_type("i32", &arena);
    fields[1].name = "y";
    fields[1].type = make_prim_type("i32", &arena);

    AstNode* struct_node = make_struct_with_fields("Point", fields, 2, &arena);

    AstNode* decls[] = {struct_node};
    AstNode* module = make_module("test", decls, 1, &arena);

    // Resolve and check
    resolve_and_check(module, &symtab, &pool, &errors, &arena);

    // Should have no errors
    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
    }
    assert(!error_list_has_errors(&errors));

    // Verify struct type was created
    assert(struct_node->type != NULL);
    assert(struct_node->type->kind == TYPE_STRUCT);
    assert(struct_node->type->data.struct_type.field_count == 2);
    assert(strcmp(struct_node->type->data.struct_type.name, "Point") == 0);

    arena_free(&arena);
}

// Test: Return type checking
TEST(return_type_check) {
    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable symtab;
    symbol_table_init(&symtab, &arena);

    // Create: fn test() -> i32 { return 42; }
    AstNode* lit = make_int_literal(42, &arena);
    AstNode* ret = make_return(lit, &arena);
    AstNode* stmts[] = {ret};
    AstNode* body = make_block(stmts, 1, &arena);
    AstNode* ret_type = make_prim_type("i32", &arena);
    AstNode* func = make_typed_function("test", NULL, 0, ret_type, body, &arena);

    AstNode* decls[] = {func};
    AstNode* module = make_module("test", decls, 1, &arena);

    // Resolve and check
    resolve_and_check(module, &symtab, &pool, &errors, &arena);

    // Should have no errors
    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
    }
    assert(!error_list_has_errors(&errors));

    arena_free(&arena);
}

// Test: Return type mismatch (should error)
TEST(return_type_mismatch) {
    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable symtab;
    symbol_table_init(&symtab, &arena);

    // Create: fn test() -> bool { return 42; }  // Type error!
    AstNode* lit = make_int_literal(42, &arena);
    AstNode* ret = make_return(lit, &arena);
    AstNode* stmts[] = {ret};
    AstNode* body = make_block(stmts, 1, &arena);
    AstNode* ret_type = make_prim_type("bool", &arena);
    AstNode* func = make_typed_function("test", NULL, 0, ret_type, body, &arena);

    AstNode* decls[] = {func};
    AstNode* module = make_module("test", decls, 1, &arena);

    // Resolve and check
    resolve_and_check(module, &symtab, &pool, &errors, &arena);

    // Should have errors
    assert(error_list_has_errors(&errors));

    arena_free(&arena);
}

// Run all tests
int main() {
    printf("=== Type Checker Tests ===\n\n");

    run_test_integer_literal();
    run_test_boolean_literal();
    run_test_binary_expression();
    run_test_binary_type_mismatch();
    run_test_variable_inference();
    run_test_explicit_type_match();
    run_test_explicit_type_mismatch();
    run_test_struct_type();
    run_test_return_type_check();
    run_test_return_type_mismatch();

    printf("\n=== Test Results ===\n");
    printf("Passed: %d/%d\n", test_passed, test_count);

    return test_passed == test_count ? 0 : 1;
}
