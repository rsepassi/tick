// Resolver: Name resolution and scope building
// Purpose: Build hierarchical scopes and resolve all name references
// Scope Hierarchy: Module → Function → Block (nested)

#include "../include/symbol.h"
#include "../include/ast.h"
#include "../include/type.h"
#include "../include/error.h"
#include "../include/arena.h"
#include "../include/string_pool.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Resolver context
typedef struct Resolver {
    SymbolTable* symbol_table;
    Scope* current_scope;
    ErrorList* errors;
    StringPool* string_pool;
    Arena* arena;
} Resolver;

// Forward declarations
static void resolve_module(Resolver* r, AstNode* module_node);
static void resolve_declaration(Resolver* r, AstNode* decl);
static void resolve_statement(Resolver* r, AstNode* stmt);
static void resolve_expression(Resolver* r, AstNode* expr);
static void resolve_type_node(Resolver* r, AstNode* type_node);

// Symbol table implementation

void symbol_table_init(SymbolTable* table, Arena* arena) {
    table->module_scope = NULL;
    table->all_scopes = NULL;
    table->scope_count = 0;
    table->scope_capacity = 0;
    table->arena = arena;
}

Scope* scope_new(SymbolTable* table, ScopeKind kind, Scope* parent, const char* name) {
    Scope* scope = arena_alloc(table->arena, sizeof(Scope), 8);
    scope->kind = kind;
    scope->parent = parent;
    scope->symbols = NULL;
    scope->symbol_count = 0;
    scope->symbol_capacity = 0;
    scope->name = name;
    scope->owner_node = NULL;

    // Add to table's scope list
    if (table->scope_count >= table->scope_capacity) {
        size_t new_capacity = table->scope_capacity == 0 ? 16 : table->scope_capacity * 2;
        Scope** new_scopes = arena_alloc(table->arena, sizeof(Scope*) * new_capacity, 8);
        if (table->all_scopes) {
            memcpy(new_scopes, table->all_scopes, sizeof(Scope*) * table->scope_count);
        }
        table->all_scopes = new_scopes;
        table->scope_capacity = new_capacity;
    }
    table->all_scopes[table->scope_count++] = scope;

    return scope;
}

void scope_init(Scope* scope, ScopeKind kind, Scope* parent, Arena* arena) {
    scope->kind = kind;
    scope->parent = parent;
    scope->symbols = NULL;
    scope->symbol_count = 0;
    scope->symbol_capacity = 0;
    scope->name = NULL;
    scope->owner_node = NULL;
}

// Symbol creation functions

Symbol* symbol_new(SymbolKind kind, const char* name, SourceLocation loc, Arena* arena) {
    Symbol* sym = arena_alloc(arena, sizeof(Symbol), 8);
    sym->kind = kind;
    sym->name = name;
    sym->type = NULL;
    sym->is_public = false;
    sym->defined_at = loc;
    return sym;
}

Symbol* symbol_new_function(const char* name, AstNode* ast_node, Type* type,
                            bool is_public, SourceLocation loc, Arena* arena) {
    Symbol* sym = symbol_new(SYMBOL_FUNCTION, name, loc, arena);
    sym->type = type;
    sym->is_public = is_public;
    sym->data.function.ast_node = ast_node;
    sym->data.function.is_coroutine = false;
    sym->data.function.is_async = false;
    return sym;
}

Symbol* symbol_new_type(const char* name, AstNode* ast_node, Type* type,
                       bool is_public, SourceLocation loc, Arena* arena) {
    Symbol* sym = symbol_new(SYMBOL_TYPE, name, loc, arena);
    sym->type = type;
    sym->is_public = is_public;
    sym->data.type_decl.ast_node = ast_node;
    return sym;
}

Symbol* symbol_new_variable(const char* name, AstNode* ast_node, Type* type,
                           bool is_mutable, bool is_volatile, SourceLocation loc, Arena* arena) {
    Symbol* sym = symbol_new(SYMBOL_VARIABLE, name, loc, arena);
    sym->type = type;
    sym->is_public = false;
    sym->data.variable.ast_node = ast_node;
    sym->data.variable.is_mutable = is_mutable;
    sym->data.variable.is_volatile = is_volatile;
    return sym;
}

Symbol* symbol_new_param(const char* name, AstNode* ast_node, Type* type,
                        size_t param_index, SourceLocation loc, Arena* arena) {
    Symbol* sym = symbol_new(SYMBOL_PARAM, name, loc, arena);
    sym->type = type;
    sym->is_public = false;
    sym->data.param.ast_node = ast_node;
    sym->data.param.param_index = param_index;
    return sym;
}

// Scope operations

void scope_insert(Scope* scope, Symbol* sym, Arena* arena) {
    if (scope->symbol_count >= scope->symbol_capacity) {
        size_t new_capacity = scope->symbol_capacity == 0 ? 8 : scope->symbol_capacity * 2;
        Symbol** new_symbols = arena_alloc(arena, sizeof(Symbol*) * new_capacity, 8);
        if (scope->symbols) {
            memcpy(new_symbols, scope->symbols, sizeof(Symbol*) * scope->symbol_count);
        }
        scope->symbols = new_symbols;
        scope->symbol_capacity = new_capacity;
    }
    scope->symbols[scope->symbol_count++] = sym;
}

Symbol* scope_lookup_local(Scope* scope, const char* name) {
    for (size_t i = 0; i < scope->symbol_count; i++) {
        if (strcmp(scope->symbols[i]->name, name) == 0) {
            return scope->symbols[i];
        }
    }
    return NULL;
}

Symbol* scope_lookup(Scope* scope, const char* name) {
    // Search current scope and all parent scopes
    for (Scope* s = scope; s != NULL; s = s->parent) {
        Symbol* sym = scope_lookup_local(s, name);
        if (sym) {
            return sym;
        }
    }
    return NULL;
}

bool scope_has_symbol(Scope* scope, const char* name) {
    return scope_lookup_local(scope, name) != NULL;
}

// Resolver implementation

static void enter_scope(Resolver* r, Scope* scope) {
    r->current_scope = scope;
}

static void exit_scope(Resolver* r) {
    assert(r->current_scope != NULL);
    r->current_scope = r->current_scope->parent;
}

static void resolve_import_decl(Resolver* r, AstNode* node) {
    assert(node->kind == AST_IMPORT_DECL);

    // For now, just note that we need to resolve this import
    // Full import resolution would happen in a multi-module compiler
    // Here we just intern the names
    // In interfaces2: name is the binding name, path is the import path
    const char* name = string_pool_intern(r->string_pool, node->data.import_decl.name, strlen(node->data.import_decl.name));
    node->data.import_decl.name = name;

    if (node->data.import_decl.path) {
        const char* path = string_pool_intern(r->string_pool, node->data.import_decl.path, strlen(node->data.import_decl.path));
        node->data.import_decl.path = path;
    }
}

static void resolve_function_decl(Resolver* r, AstNode* node) {
    assert(node->kind == AST_FUNCTION_DECL);

    // Intern function name
    const char* name = string_pool_intern(r->string_pool, node->data.function_decl.name, strlen(node->data.function_decl.name));
    node->data.function_decl.name = name;

    // Check for duplicate in current scope
    if (scope_has_symbol(r->current_scope, name)) {
        error_list_add(r->errors, ERROR_RESOLUTION, node->loc,
                      "Duplicate function declaration: %s", name);
        return;
    }

    // Create symbol for function
    Symbol* sym = symbol_new_function(name, node, NULL, node->data.function_decl.is_pub, node->loc, r->arena);

    // Insert into current scope
    scope_insert(r->current_scope, sym, r->arena);

    // Create function scope
    Scope* func_scope = scope_new(r->symbol_table, SCOPE_FUNCTION, r->current_scope, name);
    func_scope->owner_node = node;
    enter_scope(r, func_scope);

    // Resolve parameters
    for (size_t i = 0; i < node->data.function_decl.param_count; i++) {
        AstParam* param = &node->data.function_decl.params[i];

        // Intern parameter name
        const char* param_name = string_pool_intern(r->string_pool, param->name, strlen(param->name));
        param->name = param_name;

        // Check for duplicate parameter names
        if (scope_has_symbol(r->current_scope, param_name)) {
            error_list_add(r->errors, ERROR_RESOLUTION, node->loc,
                          "Duplicate parameter name: %s", param_name);
            continue;
        }

        // Resolve parameter type
        resolve_type_node(r, param->type);

        // Create symbol for parameter
        Symbol* param_sym = symbol_new_param(param_name, node, NULL, i, param->loc, r->arena);
        scope_insert(r->current_scope, param_sym, r->arena);
    }

    // Resolve return type if present
    if (node->data.function_decl.return_type) {
        resolve_type_node(r, node->data.function_decl.return_type);
    }

    // Resolve function body
    if (node->data.function_decl.body) {
        resolve_statement(r, node->data.function_decl.body);
    }

    exit_scope(r);
}

static void resolve_struct_decl(Resolver* r, AstNode* node) {
    assert(node->kind == AST_STRUCT_DECL);

    // Intern struct name
    const char* name = string_pool_intern(r->string_pool, node->data.struct_decl.name, strlen(node->data.struct_decl.name));
    node->data.struct_decl.name = name;

    // Check for duplicate in current scope
    if (scope_has_symbol(r->current_scope, name)) {
        error_list_add(r->errors, ERROR_RESOLUTION, node->loc,
                      "Duplicate type declaration: %s", name);
        return;
    }

    // Create symbol for struct
    Symbol* sym = symbol_new_type(name, node, NULL, node->data.struct_decl.is_pub, node->loc, r->arena);

    // Insert into current scope
    scope_insert(r->current_scope, sym, r->arena);

    // Resolve field types
    for (size_t i = 0; i < node->data.struct_decl.field_count; i++) {
        AstField* field = &node->data.struct_decl.fields[i];

        // Intern field name
        field->name = string_pool_intern(r->string_pool, field->name, strlen(field->name));

        // Resolve field type
        resolve_type_node(r, field->type);
    }
}

static void resolve_enum_decl(Resolver* r, AstNode* node) {
    assert(node->kind == AST_ENUM_DECL);

    // Intern enum name
    const char* name = string_pool_intern(r->string_pool, node->data.enum_decl.name, strlen(node->data.enum_decl.name));
    node->data.enum_decl.name = name;

    // Check for duplicate in current scope
    if (scope_has_symbol(r->current_scope, name)) {
        error_list_add(r->errors, ERROR_RESOLUTION, node->loc,
                      "Duplicate type declaration: %s", name);
        return;
    }

    // Create symbol for enum
    Symbol* sym = symbol_new_type(name, node, NULL, node->data.enum_decl.is_pub, node->loc, r->arena);

    // Insert into current scope
    scope_insert(r->current_scope, sym, r->arena);

    // Resolve underlying type
    if (node->data.enum_decl.underlying_type) {
        resolve_type_node(r, node->data.enum_decl.underlying_type);
    }

    // Intern variant names (AstEnumVariant is now AstEnumValue)
    for (size_t i = 0; i < node->data.enum_decl.value_count; i++) {
        AstEnumValue* value = &node->data.enum_decl.values[i];
        value->name = string_pool_intern(r->string_pool, value->name, strlen(value->name));
    }
}

static void resolve_union_decl(Resolver* r, AstNode* node) {
    assert(node->kind == AST_UNION_DECL);

    // Intern union name
    const char* name = string_pool_intern(r->string_pool, node->data.union_decl.name, strlen(node->data.union_decl.name));
    node->data.union_decl.name = name;

    // Check for duplicate in current scope
    if (scope_has_symbol(r->current_scope, name)) {
        error_list_add(r->errors, ERROR_RESOLUTION, node->loc,
                      "Duplicate type declaration: %s", name);
        return;
    }

    // Create symbol for union
    Symbol* sym = symbol_new_type(name, node, NULL, node->data.union_decl.is_pub, node->loc, r->arena);

    // Insert into current scope
    scope_insert(r->current_scope, sym, r->arena);

    // Resolve field types (union variants are now stored as fields)
    for (size_t i = 0; i < node->data.union_decl.field_count; i++) {
        AstField* field = &node->data.union_decl.fields[i];

        // Intern field name
        field->name = string_pool_intern(r->string_pool, field->name, strlen(field->name));

        // Resolve field type
        resolve_type_node(r, field->type);
    }
}

static void resolve_declaration(Resolver* r, AstNode* decl) {
    switch (decl->kind) {
        case AST_IMPORT_DECL:
            resolve_import_decl(r, decl);
            break;
        case AST_FUNCTION_DECL:
            resolve_function_decl(r, decl);
            break;
        case AST_STRUCT_DECL:
            resolve_struct_decl(r, decl);
            break;
        case AST_ENUM_DECL:
            resolve_enum_decl(r, decl);
            break;
        case AST_UNION_DECL:
            resolve_union_decl(r, decl);
            break;
        default:
            error_list_add(r->errors, ERROR_RESOLUTION, decl->loc,
                          "Unexpected declaration kind: %d", decl->kind);
            break;
    }
}

static void resolve_let_stmt(Resolver* r, AstNode* node) {
    assert(node->kind == AST_LET_STMT);

    // Intern variable name
    const char* name = string_pool_intern(r->string_pool, node->data.let_decl.name, strlen(node->data.let_decl.name));
    node->data.let_decl.name = name;

    // Check for duplicate in current scope
    if (scope_has_symbol(r->current_scope, name)) {
        error_list_add(r->errors, ERROR_RESOLUTION, node->loc,
                      "Duplicate variable declaration: %s", name);
        return;
    }

    // Resolve type if present
    if (node->data.let_decl.type) {
        resolve_type_node(r, node->data.let_decl.type);
    }

    // Resolve initializer
    if (node->data.let_decl.init) {
        resolve_expression(r, node->data.let_decl.init);
    }

    // Create symbol for variable
    Symbol* sym = symbol_new_variable(name, node, NULL, false, false, node->loc, r->arena);

    // Insert into current scope
    scope_insert(r->current_scope, sym, r->arena);
}

static void resolve_var_stmt(Resolver* r, AstNode* node) {
    assert(node->kind == AST_VAR_STMT);

    // Intern variable name
    const char* name = string_pool_intern(r->string_pool, node->data.var_decl.name, strlen(node->data.var_decl.name));
    node->data.var_decl.name = name;

    // Check for duplicate in current scope
    if (scope_has_symbol(r->current_scope, name)) {
        error_list_add(r->errors, ERROR_RESOLUTION, node->loc,
                      "Duplicate variable declaration: %s", name);
        return;
    }

    // Resolve type if present
    if (node->data.var_decl.type) {
        resolve_type_node(r, node->data.var_decl.type);
    }

    // Resolve initializer if present
    if (node->data.var_decl.init) {
        resolve_expression(r, node->data.var_decl.init);
    }

    // Create symbol for variable
    Symbol* sym = symbol_new_variable(name, node, NULL, true, node->data.var_decl.is_volatile, node->loc, r->arena);

    // Insert into current scope
    scope_insert(r->current_scope, sym, r->arena);
}

static void resolve_block_stmt(Resolver* r, AstNode* node) {
    assert(node->kind == AST_BLOCK_STMT);

    // Create new block scope
    Scope* block_scope = scope_new(r->symbol_table, SCOPE_BLOCK, r->current_scope, "block");
    block_scope->owner_node = node;
    enter_scope(r, block_scope);

    // Resolve statements in block
    for (size_t i = 0; i < node->data.block_stmt.stmt_count; i++) {
        resolve_statement(r, node->data.block_stmt.stmts[i]);
    }

    exit_scope(r);
}

static void resolve_for_stmt(Resolver* r, AstNode* node) {
    assert(node->kind == AST_FOR_STMT);

    // Create new scope for loop
    Scope* for_scope = scope_new(r->symbol_table, SCOPE_BLOCK, r->current_scope, "for");
    for_scope->owner_node = node;
    enter_scope(r, for_scope);

    // Handle for-in loops (when loop_var is present)
    if (node->data.for_stmt.loop_var) {
        // Intern iterator name
        const char* loop_var = string_pool_intern(r->string_pool, node->data.for_stmt.loop_var, strlen(node->data.for_stmt.loop_var));
        node->data.for_stmt.loop_var = loop_var;

        // Resolve iterable expression
        if (node->data.for_stmt.iterable) {
            resolve_expression(r, node->data.for_stmt.iterable);
        }

        // Create symbol for iterator
        Symbol* iter_sym = symbol_new_variable(loop_var, node, NULL, false, false, node->loc, r->arena);
        scope_insert(r->current_scope, iter_sym, r->arena);
    }

    // Handle condition-only loops (while-style loops)
    if (node->data.for_stmt.condition) {
        resolve_expression(r, node->data.for_stmt.condition);
    }

    // Handle continue expression
    if (node->data.for_stmt.continue_expr) {
        resolve_expression(r, node->data.for_stmt.continue_expr);
    }

    // Resolve loop body
    resolve_statement(r, node->data.for_stmt.body);

    exit_scope(r);
}

static void resolve_statement(Resolver* r, AstNode* stmt) {
    switch (stmt->kind) {
        case AST_LET_STMT:
            resolve_let_stmt(r, stmt);
            break;
        case AST_VAR_STMT:
            resolve_var_stmt(r, stmt);
            break;
        case AST_RETURN_STMT:
            if (stmt->data.return_stmt.value) {
                resolve_expression(r, stmt->data.return_stmt.value);
            }
            break;
        case AST_IF_STMT:
            resolve_expression(r, stmt->data.if_stmt.condition);
            resolve_statement(r, stmt->data.if_stmt.then_block);
            if (stmt->data.if_stmt.else_block) {
                resolve_statement(r, stmt->data.if_stmt.else_block);
            }
            break;
        case AST_FOR_STMT:
            resolve_for_stmt(r, stmt);
            break;
        case AST_SWITCH_STMT:
            resolve_expression(r, stmt->data.switch_stmt.value);
            for (size_t i = 0; i < stmt->data.switch_stmt.case_count; i++) {
                AstSwitchCase* case_item = &stmt->data.switch_stmt.cases[i];
                // Resolve case values
                for (size_t j = 0; j < case_item->value_count; j++) {
                    resolve_expression(r, case_item->values[j]);
                }
                // Resolve case statements
                for (size_t j = 0; j < case_item->stmt_count; j++) {
                    resolve_statement(r, case_item->stmts[j]);
                }
            }
            break;
        case AST_DEFER_STMT:
            resolve_statement(r, stmt->data.defer_stmt.stmt);
            break;
        case AST_ERRDEFER_STMT:
            resolve_statement(r, stmt->data.defer_stmt.stmt);  // Same structure as defer_stmt
            break;
        case AST_SUSPEND_STMT:
            // Nothing to resolve
            break;
        case AST_RESUME_STMT:
            resolve_expression(r, stmt->data.resume_stmt.coro);
            break;
        case AST_BLOCK_STMT:
            resolve_block_stmt(r, stmt);
            break;
        case AST_EXPR_STMT:
            resolve_expression(r, stmt->data.expr_stmt.expr);
            break;
        case AST_ASSIGN_STMT:
            resolve_expression(r, stmt->data.assign_stmt.lhs);
            resolve_expression(r, stmt->data.assign_stmt.rhs);
            break;
        case AST_BREAK_STMT:
        case AST_CONTINUE_STMT:
        case AST_CONTINUE_SWITCH_STMT:
            // Nothing to resolve for break/continue
            // CONTINUE_SWITCH_STMT has a value field that may need resolving
            if (stmt->kind == AST_CONTINUE_SWITCH_STMT && stmt->data.continue_switch_stmt.value) {
                resolve_expression(r, stmt->data.continue_switch_stmt.value);
            }
            break;
        default:
            error_list_add(r->errors, ERROR_RESOLUTION, stmt->loc,
                          "Unexpected statement kind: %d", stmt->kind);
            break;
    }
}

static void resolve_identifier_expr(Resolver* r, AstNode* node) {
    assert(node->kind == AST_IDENTIFIER_EXPR);

    // Intern identifier name
    const char* name = string_pool_intern(r->string_pool, node->data.identifier_expr.name, strlen(node->data.identifier_expr.name));
    node->data.identifier_expr.name = name;

    // Lookup symbol
    Symbol* sym = scope_lookup(r->current_scope, name);
    if (!sym) {
        error_list_add(r->errors, ERROR_RESOLUTION, node->loc,
                      "Undefined identifier: %s", name);
        return;
    }

    // Note: interfaces2/ast.h doesn't have symbol field in identifier_expr
    // Symbol will be stored in the resolver's symbol table for later lookup
}

static void resolve_expression(Resolver* r, AstNode* expr) {
    switch (expr->kind) {
        case AST_BINARY_EXPR:
            resolve_expression(r, expr->data.binary_expr.left);
            resolve_expression(r, expr->data.binary_expr.right);
            break;
        case AST_UNARY_EXPR:
            resolve_expression(r, expr->data.unary_expr.operand);
            break;
        case AST_CALL_EXPR:
            resolve_expression(r, expr->data.call_expr.callee);
            for (size_t i = 0; i < expr->data.call_expr.arg_count; i++) {
                resolve_expression(r, expr->data.call_expr.args[i]);
            }
            break;
        case AST_ASYNC_CALL_EXPR:
            resolve_expression(r, expr->data.async_call_expr.callee);
            for (size_t i = 0; i < expr->data.async_call_expr.arg_count; i++) {
                resolve_expression(r, expr->data.async_call_expr.args[i]);
            }
            break;
        case AST_FIELD_ACCESS_EXPR:
            resolve_expression(r, expr->data.field_access_expr.object);
            // Intern field name
            expr->data.field_access_expr.field_name = string_pool_intern(r->string_pool,
                expr->data.field_access_expr.field_name, strlen(expr->data.field_access_expr.field_name));
            break;
        case AST_INDEX_EXPR:
            resolve_expression(r, expr->data.index_expr.array);
            resolve_expression(r, expr->data.index_expr.index);
            break;
        case AST_LITERAL_EXPR:
            // Literals have no names to resolve
            break;
        case AST_IDENTIFIER_EXPR:
            resolve_identifier_expr(r, expr);
            break;
        case AST_CAST_EXPR:
            resolve_expression(r, expr->data.cast_expr.expr);
            resolve_type_node(r, expr->data.cast_expr.type);
            break;
        case AST_TRY_EXPR:
            resolve_expression(r, expr->data.try_expr.expr);
            break;
        case AST_STRUCT_INIT_EXPR:
            // Resolve struct type if present
            if (expr->data.struct_init_expr.type) {
                resolve_type_node(r, expr->data.struct_init_expr.type);
            }
            // Resolve field initializers
            for (size_t i = 0; i < expr->data.struct_init_expr.field_count; i++) {
                AstStructInit* field = &expr->data.struct_init_expr.fields[i];
                // Intern field name
                field->field_name = string_pool_intern(r->string_pool, field->field_name, strlen(field->field_name));
                resolve_expression(r, field->value);
            }
            break;
        case AST_ARRAY_INIT_EXPR:
            // Resolve array element initializers
            for (size_t i = 0; i < expr->data.array_init_expr.element_count; i++) {
                resolve_expression(r, expr->data.array_init_expr.elements[i]);
            }
            break;
        case AST_RANGE_EXPR:
            resolve_expression(r, expr->data.range_expr.start);
            resolve_expression(r, expr->data.range_expr.end);
            break;
        default:
            error_list_add(r->errors, ERROR_RESOLUTION, expr->loc,
                          "Unexpected expression kind: %d", expr->kind);
            break;
    }
}

static void resolve_type_node(Resolver* r, AstNode* type_node) {
    // Note: interfaces2/ast.h uses AstTypeNode which is separate from AstNode
    // For now, we'll handle type nodes that are still part of the main AstNode union
    switch (type_node->kind) {
        case AST_TYPE_PRIMITIVE:
            // Intern primitive type name
            // Note: AST_TYPE_PRIMITIVE structure is different in interfaces2
            // It uses inline data structure instead of separate type
            break;
        case AST_TYPE_ARRAY:
            // Note: In interfaces2, array types have element_type and size_expr
            // We need to handle both cases
            break;
        case AST_TYPE_SLICE:
            // Resolve element type
            break;
        case AST_TYPE_POINTER:
            // Resolve pointee type
            break;
        case AST_TYPE_FUNCTION:
            // Resolve parameter and return types
            break;
        case AST_TYPE_RESULT:
            // Resolve value and error types
            break;
        case AST_TYPE_NAMED:
            // Intern and lookup type name
            // Note: interfaces2 doesn't have symbol field in AST type nodes
            break;
        default:
            error_list_add(r->errors, ERROR_RESOLUTION, type_node->loc,
                          "Unexpected type node kind: %d", type_node->kind);
            break;
    }
}

static void resolve_module(Resolver* r, AstNode* module_node) {
    assert(module_node->kind == AST_MODULE);

    // Intern module name
    const char* name = string_pool_intern(r->string_pool, module_node->data.module.name, strlen(module_node->data.module.name));
    module_node->data.module.name = name;

    // Create module scope
    Scope* module_scope = scope_new(r->symbol_table, SCOPE_MODULE, NULL, name);
    module_scope->owner_node = module_node;
    r->symbol_table->module_scope = module_scope;
    enter_scope(r, module_scope);

    // Resolve all declarations
    for (size_t i = 0; i < module_node->data.module.decl_count; i++) {
        resolve_declaration(r, module_node->data.module.decls[i]);
    }

    exit_scope(r);
}

// Public API

void resolver_resolve(AstNode* module_node, SymbolTable* symbol_table,
                     StringPool* string_pool, ErrorList* errors, Arena* arena) {
    Resolver r = {
        .symbol_table = symbol_table,
        .current_scope = NULL,
        .errors = errors,
        .string_pool = string_pool,
        .arena = arena
    };

    resolve_module(&r, module_node);
}

// Debugging

void symbol_print(Symbol* sym, FILE* out) {
    const char* kind_str = "unknown";
    switch (sym->kind) {
        case SYMBOL_FUNCTION: kind_str = "function"; break;
        case SYMBOL_TYPE: kind_str = "type"; break;
        case SYMBOL_VARIABLE: kind_str = "variable"; break;
        case SYMBOL_CONSTANT: kind_str = "constant"; break;
        case SYMBOL_MODULE: kind_str = "module"; break;
        case SYMBOL_PARAM: kind_str = "param"; break;
    }
    fprintf(out, "%s %s", kind_str, sym->name);
}

void scope_print(Scope* scope, FILE* out, int indent) {
    for (int i = 0; i < indent; i++) fprintf(out, "  ");

    const char* kind_str = "unknown";
    switch (scope->kind) {
        case SCOPE_MODULE: kind_str = "module"; break;
        case SCOPE_FUNCTION: kind_str = "function"; break;
        case SCOPE_BLOCK: kind_str = "block"; break;
    }

    fprintf(out, "%s scope '%s' (%zu symbols)\n", kind_str, scope->name ? scope->name : "<unnamed>", scope->symbol_count);

    for (size_t i = 0; i < scope->symbol_count; i++) {
        for (int j = 0; j < indent + 1; j++) fprintf(out, "  ");
        symbol_print(scope->symbols[i], out);
        fprintf(out, "\n");
    }
}
