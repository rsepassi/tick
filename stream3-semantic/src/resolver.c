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
    AstImportDecl* import = &node->data.import_decl;

    // For now, just note that we need to resolve this import
    // Full import resolution would happen in a multi-module compiler
    // Here we just intern the names
    import->module_name = string_pool_intern(r->string_pool, import->module_name, strlen(import->module_name));
    if (import->alias) {
        import->alias = string_pool_intern(r->string_pool, import->alias, strlen(import->alias));
    }
}

static void resolve_function_decl(Resolver* r, AstNode* node) {
    assert(node->kind == AST_FUNCTION_DECL);
    AstFunctionDecl* func = &node->data.function_decl;

    // Intern function name
    func->name = string_pool_intern(r->string_pool, func->name, strlen(func->name));

    // Check for duplicate in current scope
    if (scope_has_symbol(r->current_scope, func->name)) {
        error_list_add(r->errors, ERROR_RESOLUTION, node->loc,
                      "Duplicate function declaration: %s", func->name);
        return;
    }

    // Create symbol for function
    Symbol* sym = symbol_new_function(func->name, node, NULL, func->is_public, node->loc, r->arena);
    sym->data.function.is_async = func->is_async;
    func->symbol = sym;

    // Insert into current scope
    scope_insert(r->current_scope, sym, r->arena);

    // Create function scope
    Scope* func_scope = scope_new(r->symbol_table, SCOPE_FUNCTION, r->current_scope, func->name);
    func_scope->owner_node = node;
    enter_scope(r, func_scope);

    // Resolve parameters
    for (size_t i = 0; i < func->param_count; i++) {
        AstParam* param = &func->params[i];

        // Intern parameter name
        param->name = string_pool_intern(r->string_pool, param->name, strlen(param->name));

        // Check for duplicate parameter names
        if (scope_has_symbol(r->current_scope, param->name)) {
            error_list_add(r->errors, ERROR_RESOLUTION, node->loc,
                          "Duplicate parameter name: %s", param->name);
            continue;
        }

        // Resolve parameter type
        resolve_type_node(r, param->type_node);

        // Create symbol for parameter
        Symbol* param_sym = symbol_new_param(param->name, node, NULL, i, node->loc, r->arena);
        scope_insert(r->current_scope, param_sym, r->arena);
    }

    // Resolve return type if present
    if (func->return_type_node) {
        resolve_type_node(r, func->return_type_node);
    }

    // Resolve function body
    if (func->body) {
        resolve_statement(r, func->body);
    }

    exit_scope(r);
}

static void resolve_struct_decl(Resolver* r, AstNode* node) {
    assert(node->kind == AST_STRUCT_DECL);
    AstStructDecl* struct_decl = &node->data.struct_decl;

    // Intern struct name
    struct_decl->name = string_pool_intern(r->string_pool, struct_decl->name, strlen(struct_decl->name));

    // Check for duplicate in current scope
    if (scope_has_symbol(r->current_scope, struct_decl->name)) {
        error_list_add(r->errors, ERROR_RESOLUTION, node->loc,
                      "Duplicate type declaration: %s", struct_decl->name);
        return;
    }

    // Create symbol for struct
    Symbol* sym = symbol_new_type(struct_decl->name, node, NULL, struct_decl->is_public, node->loc, r->arena);
    struct_decl->symbol = sym;

    // Insert into current scope
    scope_insert(r->current_scope, sym, r->arena);

    // Resolve field types
    for (size_t i = 0; i < struct_decl->field_count; i++) {
        AstStructField* field = &struct_decl->fields[i];

        // Intern field name
        field->name = string_pool_intern(r->string_pool, field->name, strlen(field->name));

        // Resolve field type
        resolve_type_node(r, field->type_node);
    }
}

static void resolve_enum_decl(Resolver* r, AstNode* node) {
    assert(node->kind == AST_ENUM_DECL);
    AstEnumDecl* enum_decl = &node->data.enum_decl;

    // Intern enum name
    enum_decl->name = string_pool_intern(r->string_pool, enum_decl->name, strlen(enum_decl->name));

    // Check for duplicate in current scope
    if (scope_has_symbol(r->current_scope, enum_decl->name)) {
        error_list_add(r->errors, ERROR_RESOLUTION, node->loc,
                      "Duplicate type declaration: %s", enum_decl->name);
        return;
    }

    // Create symbol for enum
    Symbol* sym = symbol_new_type(enum_decl->name, node, NULL, enum_decl->is_public, node->loc, r->arena);
    enum_decl->symbol = sym;

    // Insert into current scope
    scope_insert(r->current_scope, sym, r->arena);

    // Resolve underlying type
    if (enum_decl->underlying_type_node) {
        resolve_type_node(r, enum_decl->underlying_type_node);
    }

    // Intern variant names
    for (size_t i = 0; i < enum_decl->variant_count; i++) {
        AstEnumVariant* variant = &enum_decl->variants[i];
        variant->name = string_pool_intern(r->string_pool, variant->name, strlen(variant->name));
    }
}

static void resolve_union_decl(Resolver* r, AstNode* node) {
    assert(node->kind == AST_UNION_DECL);
    AstUnionDecl* union_decl = &node->data.union_decl;

    // Intern union name
    union_decl->name = string_pool_intern(r->string_pool, union_decl->name, strlen(union_decl->name));

    // Check for duplicate in current scope
    if (scope_has_symbol(r->current_scope, union_decl->name)) {
        error_list_add(r->errors, ERROR_RESOLUTION, node->loc,
                      "Duplicate type declaration: %s", union_decl->name);
        return;
    }

    // Create symbol for union
    Symbol* sym = symbol_new_type(union_decl->name, node, NULL, union_decl->is_public, node->loc, r->arena);
    union_decl->symbol = sym;

    // Insert into current scope
    scope_insert(r->current_scope, sym, r->arena);

    // Resolve variant types
    for (size_t i = 0; i < union_decl->variant_count; i++) {
        AstUnionVariant* variant = &union_decl->variants[i];

        // Intern variant name
        variant->name = string_pool_intern(r->string_pool, variant->name, strlen(variant->name));

        // Resolve variant type
        resolve_type_node(r, variant->type_node);
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
    AstLetStmt* let = &node->data.let_stmt;

    // Intern variable name
    let->name = string_pool_intern(r->string_pool, let->name, strlen(let->name));

    // Check for duplicate in current scope
    if (scope_has_symbol(r->current_scope, let->name)) {
        error_list_add(r->errors, ERROR_RESOLUTION, node->loc,
                      "Duplicate variable declaration: %s", let->name);
        return;
    }

    // Resolve type if present
    if (let->type_node) {
        resolve_type_node(r, let->type_node);
    }

    // Resolve initializer
    if (let->initializer) {
        resolve_expression(r, let->initializer);
    }

    // Create symbol for variable
    Symbol* sym = symbol_new_variable(let->name, node, NULL, false, false, node->loc, r->arena);
    let->symbol = sym;

    // Insert into current scope
    scope_insert(r->current_scope, sym, r->arena);
}

static void resolve_var_stmt(Resolver* r, AstNode* node) {
    assert(node->kind == AST_VAR_STMT);
    AstVarStmt* var = &node->data.var_stmt;

    // Intern variable name
    var->name = string_pool_intern(r->string_pool, var->name, strlen(var->name));

    // Check for duplicate in current scope
    if (scope_has_symbol(r->current_scope, var->name)) {
        error_list_add(r->errors, ERROR_RESOLUTION, node->loc,
                      "Duplicate variable declaration: %s", var->name);
        return;
    }

    // Resolve type if present
    if (var->type_node) {
        resolve_type_node(r, var->type_node);
    }

    // Resolve initializer if present
    if (var->initializer) {
        resolve_expression(r, var->initializer);
    }

    // Create symbol for variable
    Symbol* sym = symbol_new_variable(var->name, node, NULL, true, var->is_volatile, node->loc, r->arena);
    var->symbol = sym;

    // Insert into current scope
    scope_insert(r->current_scope, sym, r->arena);
}

static void resolve_block_stmt(Resolver* r, AstNode* node) {
    assert(node->kind == AST_BLOCK_STMT);
    AstBlockStmt* block = &node->data.block_stmt;

    // Create new block scope
    Scope* block_scope = scope_new(r->symbol_table, SCOPE_BLOCK, r->current_scope, "block");
    block_scope->owner_node = node;
    enter_scope(r, block_scope);

    // Resolve statements in block
    for (size_t i = 0; i < block->stmt_count; i++) {
        resolve_statement(r, block->statements[i]);
    }

    exit_scope(r);
}

static void resolve_for_stmt(Resolver* r, AstNode* node) {
    assert(node->kind == AST_FOR_STMT);
    AstForStmt* for_stmt = &node->data.for_stmt;

    // Create new scope for loop
    Scope* for_scope = scope_new(r->symbol_table, SCOPE_BLOCK, r->current_scope, "for");
    for_scope->owner_node = node;
    enter_scope(r, for_scope);

    // Intern iterator name
    for_stmt->iterator_name = string_pool_intern(r->string_pool, for_stmt->iterator_name, strlen(for_stmt->iterator_name));

    // Resolve collection expression
    resolve_expression(r, for_stmt->collection);

    // Create symbol for iterator
    Symbol* iter_sym = symbol_new_variable(for_stmt->iterator_name, node, NULL, false, false, node->loc, r->arena);
    for_stmt->iterator_symbol = iter_sym;
    scope_insert(r->current_scope, iter_sym, r->arena);

    // Resolve loop body
    resolve_statement(r, for_stmt->body);

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
        case AST_IF_STMT: {
            AstIfStmt* if_stmt = &stmt->data.if_stmt;
            resolve_expression(r, if_stmt->condition);
            resolve_statement(r, if_stmt->then_block);
            if (if_stmt->else_block) {
                resolve_statement(r, if_stmt->else_block);
            }
            break;
        }
        case AST_FOR_STMT:
            resolve_for_stmt(r, stmt);
            break;
        case AST_WHILE_STMT: {
            AstWhileStmt* while_stmt = &stmt->data.while_stmt;
            resolve_expression(r, while_stmt->condition);
            resolve_statement(r, while_stmt->body);
            break;
        }
        case AST_SWITCH_STMT: {
            AstSwitchStmt* switch_stmt = &stmt->data.switch_stmt;
            resolve_expression(r, switch_stmt->value);
            for (size_t i = 0; i < switch_stmt->case_count; i++) {
                if (switch_stmt->cases[i].value) {
                    resolve_expression(r, switch_stmt->cases[i].value);
                }
                resolve_statement(r, switch_stmt->cases[i].body);
            }
            break;
        }
        case AST_DEFER_STMT:
            resolve_statement(r, stmt->data.defer_stmt.statement);
            break;
        case AST_ERRDEFER_STMT:
            resolve_statement(r, stmt->data.errdefer_stmt.statement);
            break;
        case AST_SUSPEND_STMT:
            // Nothing to resolve
            break;
        case AST_RESUME_STMT:
            resolve_expression(r, stmt->data.resume_stmt.coroutine_expr);
            break;
        case AST_BLOCK_STMT:
            resolve_block_stmt(r, stmt);
            break;
        case AST_EXPR_STMT:
            resolve_expression(r, stmt->data.expr_stmt.expression);
            break;
        case AST_ASSIGN_STMT: {
            AstAssignStmt* assign = &stmt->data.assign_stmt;
            resolve_expression(r, assign->target);
            resolve_expression(r, assign->value);
            break;
        }
        case AST_BREAK_STMT:
        case AST_CONTINUE_STMT:
            // Nothing to resolve
            break;
        default:
            error_list_add(r->errors, ERROR_RESOLUTION, stmt->loc,
                          "Unexpected statement kind: %d", stmt->kind);
            break;
    }
}

static void resolve_identifier_expr(Resolver* r, AstNode* node) {
    assert(node->kind == AST_IDENTIFIER_EXPR);
    AstIdentifierExpr* ident = &node->data.identifier_expr;

    // Intern identifier name
    ident->name = string_pool_intern(r->string_pool, ident->name, strlen(ident->name));

    // Lookup symbol
    Symbol* sym = scope_lookup(r->current_scope, ident->name);
    if (!sym) {
        error_list_add(r->errors, ERROR_RESOLUTION, node->loc,
                      "Undefined identifier: %s", ident->name);
        return;
    }

    ident->symbol = sym;
}

static void resolve_expression(Resolver* r, AstNode* expr) {
    switch (expr->kind) {
        case AST_BINARY_EXPR: {
            AstBinaryExpr* bin = &expr->data.binary_expr;
            resolve_expression(r, bin->left);
            resolve_expression(r, bin->right);
            break;
        }
        case AST_UNARY_EXPR: {
            AstUnaryExpr* un = &expr->data.unary_expr;
            resolve_expression(r, un->operand);
            break;
        }
        case AST_CALL_EXPR: {
            AstCallExpr* call = &expr->data.call_expr;
            resolve_expression(r, call->callee);
            for (size_t i = 0; i < call->arg_count; i++) {
                resolve_expression(r, call->args[i]);
            }
            break;
        }
        case AST_ASYNC_CALL_EXPR: {
            AstAsyncCallExpr* call = &expr->data.async_call_expr;
            resolve_expression(r, call->callee);
            for (size_t i = 0; i < call->arg_count; i++) {
                resolve_expression(r, call->args[i]);
            }
            break;
        }
        case AST_FIELD_ACCESS_EXPR: {
            AstFieldAccessExpr* field = &expr->data.field_access_expr;
            resolve_expression(r, field->object);
            // Intern field name
            field->field_name = string_pool_intern(r->string_pool, field->field_name, strlen(field->field_name));
            break;
        }
        case AST_INDEX_EXPR: {
            AstIndexExpr* idx = &expr->data.index_expr;
            resolve_expression(r, idx->array);
            resolve_expression(r, idx->index);
            break;
        }
        case AST_LITERAL_EXPR:
            // Literals have no names to resolve
            break;
        case AST_IDENTIFIER_EXPR:
            resolve_identifier_expr(r, expr);
            break;
        case AST_CAST_EXPR: {
            AstCastExpr* cast = &expr->data.cast_expr;
            resolve_expression(r, cast->expr);
            resolve_type_node(r, cast->target_type_node);
            break;
        }
        case AST_TRY_EXPR: {
            AstTryExpr* try_expr = &expr->data.try_expr;
            resolve_expression(r, try_expr->expr);
            break;
        }
        default:
            error_list_add(r->errors, ERROR_RESOLUTION, expr->loc,
                          "Unexpected expression kind: %d", expr->kind);
            break;
    }
}

static void resolve_type_node(Resolver* r, AstNode* type_node) {
    switch (type_node->kind) {
        case AST_TYPE_PRIMITIVE: {
            AstTypePrimitive* prim = &type_node->data.type_primitive;
            // Intern primitive type name
            prim->name = string_pool_intern(r->string_pool, prim->name, strlen(prim->name));
            break;
        }
        case AST_TYPE_ARRAY: {
            AstTypeArray* arr = &type_node->data.type_array;
            resolve_type_node(r, arr->elem_type_node);
            break;
        }
        case AST_TYPE_SLICE: {
            AstTypeSlice* slice = &type_node->data.type_slice;
            resolve_type_node(r, slice->elem_type_node);
            break;
        }
        case AST_TYPE_POINTER: {
            AstTypePointer* ptr = &type_node->data.type_pointer;
            resolve_type_node(r, ptr->pointee_type_node);
            break;
        }
        case AST_TYPE_FUNCTION: {
            AstTypeFunction* func = &type_node->data.type_function;
            for (size_t i = 0; i < func->param_count; i++) {
                resolve_type_node(r, func->param_type_nodes[i]);
            }
            if (func->return_type_node) {
                resolve_type_node(r, func->return_type_node);
            }
            break;
        }
        case AST_TYPE_RESULT: {
            AstTypeResult* result = &type_node->data.type_result;
            resolve_type_node(r, result->value_type_node);
            resolve_type_node(r, result->error_type_node);
            break;
        }
        case AST_TYPE_OPTION: {
            AstTypeOption* opt = &type_node->data.type_option;
            resolve_type_node(r, opt->inner_type_node);
            break;
        }
        case AST_TYPE_NAMED: {
            AstTypeNamed* named = &type_node->data.type_named;
            // Intern type name
            named->name = string_pool_intern(r->string_pool, named->name, strlen(named->name));

            // Lookup type symbol
            Symbol* sym = scope_lookup(r->current_scope, named->name);
            if (!sym) {
                error_list_add(r->errors, ERROR_RESOLUTION, type_node->loc,
                              "Undefined type: %s", named->name);
                return;
            }
            if (sym->kind != SYMBOL_TYPE) {
                error_list_add(r->errors, ERROR_RESOLUTION, type_node->loc,
                              "'%s' is not a type", named->name);
                return;
            }
            named->symbol = sym;
            break;
        }
        default:
            error_list_add(r->errors, ERROR_RESOLUTION, type_node->loc,
                          "Unexpected type node kind: %d", type_node->kind);
            break;
    }
}

static void resolve_module(Resolver* r, AstNode* module_node) {
    assert(module_node->kind == AST_MODULE);
    AstModule* module = &module_node->data.module;

    // Intern module name
    module->name = string_pool_intern(r->string_pool, module->name, strlen(module->name));

    // Create module scope
    Scope* module_scope = scope_new(r->symbol_table, SCOPE_MODULE, NULL, module->name);
    module_scope->owner_node = module_node;
    r->symbol_table->module_scope = module_scope;
    enter_scope(r, module_scope);

    // Resolve all declarations
    for (size_t i = 0; i < module->decl_count; i++) {
        resolve_declaration(r, module->declarations[i]);
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
