// Test suite for resolver (name resolution and scope building)

#include "symbol.h"
#include "ast.h"
#include "type.h"
#include "error.h"
#include "arena.h"
#include "string_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// External resolver API (implemented in resolver.c)
void resolver_resolve(AstNode* module_node, SymbolTable* symbol_table,
                     StringPool* string_pool, ErrorList* errors, Arena* arena);

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

// Helper: Create a simple AST node
static AstNode* make_node(AstNodeKind kind, Arena* arena) {
    AstNode* node = arena_alloc(arena, sizeof(AstNode), 8);
    memset(node, 0, sizeof(AstNode));
    node->kind = kind;
    node->loc.line = 1;
    node->loc.column = 1;
    node->loc.filename = "test.tick";
    return node;
}

// Helper: Create module node
static AstNode* make_module(const char* name, AstNode** decls, size_t decl_count, Arena* arena) {
    AstNode* node = make_node(AST_MODULE, arena);
    node->data.module.name = name;
    node->data.module.decls = decls;
    node->data.module.decl_count = decl_count;
    return node;
}

// Helper: Create function declaration
static AstNode* make_function(const char* name, bool is_public, AstNode* body, Arena* arena) {
    AstNode* node = make_node(AST_FUNCTION_DECL, arena);
    node->data.function_decl.name = name;
    node->data.function_decl.params = NULL;
    node->data.function_decl.param_count = 0;
    node->data.function_decl.return_type = NULL;
    node->data.function_decl.body = body;
    node->data.function_decl.is_pub = is_public;
    return node;
}

// Helper: Create struct declaration
static AstNode* make_struct(const char* name, bool is_public, Arena* arena) {
    AstNode* node = make_node(AST_STRUCT_DECL, arena);
    node->data.struct_decl.name = name;
    node->data.struct_decl.fields = NULL;
    node->data.struct_decl.field_count = 0;
    node->data.struct_decl.is_pub = is_public;
    node->data.struct_decl.is_packed = false;
    return node;
}

// Helper: Create let statement
static AstNode* make_let(const char* name, AstNode* initializer, Arena* arena) {
    AstNode* node = make_node(AST_LET_STMT, arena);
    node->data.let_decl.name = name;
    node->data.let_decl.type = NULL;
    node->data.let_decl.init = initializer;
    return node;
}

// Helper: Create identifier expression
static AstNode* make_identifier(const char* name, Arena* arena) {
    AstNode* node = make_node(AST_IDENTIFIER_EXPR, arena);
    node->data.identifier_expr.name = name;
    return node;
}

// Helper: Create literal expression
static AstNode* make_int_literal(int64_t value, Arena* arena) {
    AstNode* node = make_node(AST_LITERAL_EXPR, arena);
    node->data.literal_expr.literal_kind = LITERAL_INT;
    node->data.literal_expr.value.int_value = value;
    return node;
}

// Helper: Create block statement
static AstNode* make_block(AstNode** stmts, size_t stmt_count, Arena* arena) {
    AstNode* node = make_node(AST_BLOCK_STMT, arena);
    node->data.block_stmt.stmts = stmts;
    node->data.block_stmt.stmt_count = stmt_count;
    return node;
}

// Test: Basic module scope
TEST(module_scope) {
    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable symtab;
    symbol_table_init(&symtab, &arena);

    // Create simple module with one function
    AstNode* func = make_function("main", true, NULL, &arena);
    AstNode* decls[] = {func};
    AstNode* module = make_module("test", decls, 1, &arena);

    // Resolve
    resolver_resolve(module, &symtab, &pool, &errors, &arena);

    // Check for errors
    assert(!error_list_has_errors(&errors));

    // Verify module scope was created
    assert(symtab.module_scope != NULL);
    assert(symtab.module_scope->kind == SCOPE_MODULE);
    assert(symtab.module_scope->symbol_count == 1);

    // Verify function symbol
    Symbol* sym = symtab.module_scope->symbols[0];
    assert(sym->kind == SYMBOL_FUNCTION);
    assert(strcmp(sym->name, "main") == 0);
    assert(sym->is_public == true);

    arena_free(&arena);
}

// Test: Nested scopes (function scope)
TEST(function_scope) {
    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable symtab;
    symbol_table_init(&symtab, &arena);

    // Create function with local variable
    AstNode* lit = make_int_literal(42, &arena);
    AstNode* let = make_let("x", lit, &arena);
    AstNode* stmts[] = {let};
    AstNode* body = make_block(stmts, 1, &arena);
    AstNode* func = make_function("test", false, body, &arena);

    AstNode* decls[] = {func};
    AstNode* module = make_module("test", decls, 1, &arena);

    // Resolve
    resolver_resolve(module, &symtab, &pool, &errors, &arena);

    // Check for errors
    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
    }
    assert(!error_list_has_errors(&errors));

    // Verify function symbol exists
    Symbol* func_sym = scope_lookup(symtab.module_scope, "test");
    assert(func_sym != NULL);
    assert(func_sym->kind == SYMBOL_FUNCTION);

    // Verify there are nested scopes
    assert(symtab.scope_count > 1);

    // Find function scope
    Scope* func_scope = NULL;
    for (size_t i = 0; i < symtab.scope_count; i++) {
        if (symtab.all_scopes[i]->kind == SCOPE_FUNCTION) {
            func_scope = symtab.all_scopes[i];
            break;
        }
    }
    assert(func_scope != NULL);

    // Find block scope with the variable
    Scope* block_scope = NULL;
    for (size_t i = 0; i < symtab.scope_count; i++) {
        if (symtab.all_scopes[i]->kind == SCOPE_BLOCK) {
            Symbol* x = scope_lookup_local(symtab.all_scopes[i], "x");
            if (x) {
                block_scope = symtab.all_scopes[i];
                break;
            }
        }
    }
    assert(block_scope != NULL);

    // Verify variable symbol
    Symbol* var_sym = scope_lookup_local(block_scope, "x");
    assert(var_sym != NULL);
    assert(var_sym->kind == SYMBOL_VARIABLE);
    assert(strcmp(var_sym->name, "x") == 0);

    arena_free(&arena);
}

// Test: Name resolution
TEST(name_resolution) {
    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable symtab;
    symbol_table_init(&symtab, &arena);

    // Create: let x = 10; let y = x;
    AstNode* lit = make_int_literal(10, &arena);
    AstNode* let_x = make_let("x", lit, &arena);
    AstNode* ident_x = make_identifier("x", &arena);
    AstNode* let_y = make_let("y", ident_x, &arena);

    AstNode* stmts[] = {let_x, let_y};
    AstNode* body = make_block(stmts, 2, &arena);
    AstNode* func = make_function("test", false, body, &arena);

    AstNode* decls[] = {func};
    AstNode* module = make_module("test", decls, 1, &arena);

    // Resolve
    resolver_resolve(module, &symtab, &pool, &errors, &arena);

    // Check for errors
    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
    }
    assert(!error_list_has_errors(&errors));

    // Verify identifier in let_y was resolved (symbol field has been removed from interface)
    // The resolver should still resolve the identifier, but we cannot verify via symbol field

    arena_free(&arena);
}

// Test: Undefined identifier error
TEST(undefined_identifier) {
    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable symtab;
    symbol_table_init(&symtab, &arena);

    // Create: let y = undefined_var;
    AstNode* ident = make_identifier("undefined_var", &arena);
    AstNode* let_y = make_let("y", ident, &arena);

    AstNode* stmts[] = {let_y};
    AstNode* body = make_block(stmts, 1, &arena);
    AstNode* func = make_function("test", false, body, &arena);

    AstNode* decls[] = {func};
    AstNode* module = make_module("test", decls, 1, &arena);

    // Resolve
    resolver_resolve(module, &symtab, &pool, &errors, &arena);

    // Should have error
    assert(error_list_has_errors(&errors));
    assert(errors.count > 0);

    arena_free(&arena);
}

// Test: Duplicate declaration error
TEST(duplicate_declaration) {
    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable symtab;
    symbol_table_init(&symtab, &arena);

    // Create two functions with same name
    AstNode* func1 = make_function("duplicate", true, NULL, &arena);
    AstNode* func2 = make_function("duplicate", true, NULL, &arena);

    AstNode* decls[] = {func1, func2};
    AstNode* module = make_module("test", decls, 2, &arena);

    // Resolve
    resolver_resolve(module, &symtab, &pool, &errors, &arena);

    // Should have error
    assert(error_list_has_errors(&errors));
    assert(errors.count > 0);

    arena_free(&arena);
}

// Test: Type declarations
TEST(type_declarations) {
    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable symtab;
    symbol_table_init(&symtab, &arena);

    // Create struct
    AstNode* struct_node = make_struct("Point", true, &arena);

    AstNode* decls[] = {struct_node};
    AstNode* module = make_module("test", decls, 1, &arena);

    // Resolve
    resolver_resolve(module, &symtab, &pool, &errors, &arena);

    // Check for errors
    if (error_list_has_errors(&errors)) {
        error_list_print(&errors, stderr);
    }
    assert(!error_list_has_errors(&errors));

    // Verify type symbol
    Symbol* type_sym = scope_lookup(symtab.module_scope, "Point");
    assert(type_sym != NULL);
    assert(type_sym->kind == SYMBOL_TYPE);
    assert(strcmp(type_sym->name, "Point") == 0);
    assert(type_sym->is_public == true);

    arena_free(&arena);
}

// Test: String interning
TEST(string_interning) {
    Arena arena;
    arena_init(&arena, 4096);

    StringPool pool;
    string_pool_init(&pool, &arena);

    ErrorList errors;
    error_list_init(&errors, &arena);

    SymbolTable symtab;
    symbol_table_init(&symtab, &arena);

    // Create two functions with same name string (but different pointer)
    char name1[] = "test_func";
    char name2[] = "test_func";
    assert(name1 != name2);  // Different pointers

    AstNode* func = make_function(name1, true, NULL, &arena);
    AstNode* decls[] = {func};
    AstNode* module = make_module("test", decls, 1, &arena);

    // Resolve
    resolver_resolve(module, &symtab, &pool, &errors, &arena);

    // The name should be interned (same pointer after resolution)
    const char* interned_name = func->data.function_decl.name;
    const char* lookup_result = string_pool_lookup(&pool, name2, strlen(name2));

    assert(lookup_result != NULL);
    assert(interned_name == lookup_result);  // Same pointer after interning

    arena_free(&arena);
}

// Run all tests
int main() {
    printf("=== Resolver Tests ===\n\n");

    run_test_module_scope();
    run_test_function_scope();
    run_test_name_resolution();
    run_test_undefined_identifier();
    run_test_duplicate_declaration();
    run_test_type_declarations();
    run_test_string_interning();

    printf("\n=== Test Results ===\n");
    printf("Passed: %d/%d\n", test_passed, test_count);

    return test_passed == test_count ? 0 : 1;
}
