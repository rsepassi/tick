#ifndef SYMBOL_H
#define SYMBOL_H

#include <stddef.h>
#include <stdbool.h>
#include "type.h"
#include "error.h"

// Symbol Table Interface
// Purpose: Hierarchical name resolution and scope management
// Scope Hierarchy: Module → Function → Block (nested)
// No Forward Declarations: Single-pass resolution is sufficient

// Forward declarations
typedef struct AstNode AstNode;
typedef struct Arena Arena;

typedef enum {
    SCOPE_MODULE,
    SCOPE_FUNCTION,
    SCOPE_BLOCK,
} ScopeKind;

typedef enum {
    SYMBOL_FUNCTION,
    SYMBOL_TYPE,
    SYMBOL_VARIABLE,
    SYMBOL_CONSTANT,
    SYMBOL_MODULE,
    SYMBOL_PARAM,       // Function parameter
} SymbolKind;

typedef struct Symbol {
    SymbolKind kind;
    const char* name;        // Pointer into string pool
    Type* type;
    bool is_public;
    SourceLocation defined_at;

    union {
        struct {
            AstNode* ast_node;      // AST_FUNCTION_DECL
            bool is_coroutine;      // Filled by coro_analyze
            bool is_async;          // Has async keyword
        } function;

        struct {
            AstNode* ast_node;      // AST_STRUCT_DECL/AST_ENUM_DECL/AST_UNION_DECL
        } type_decl;

        struct {
            AstNode* ast_node;      // AST_LET_STMT or AST_VAR_STMT
            bool is_mutable;        // true for var, false for let
            bool is_volatile;       // For var declarations
        } variable;

        struct {
            AstNode* ast_node;      // Constant declaration node
        } constant;

        struct {
            const char* path;       // Module file path
            AstNode* ast_node;      // AST_MODULE
        } module;

        struct {
            AstNode* ast_node;      // Function parameter
            size_t param_index;     // Position in parameter list
        } param;
    } data;
} Symbol;

typedef struct Scope {
    ScopeKind kind;
    struct Scope* parent;       // NULL for module scope
    Symbol** symbols;           // Dynamic array of symbol pointers
    size_t symbol_count;
    size_t symbol_capacity;

    // Additional context
    const char* name;           // For debugging (function name, module name, etc.)
    AstNode* owner_node;        // AST node that owns this scope
} Scope;

// Symbol table: collection of all scopes in a module
typedef struct SymbolTable {
    Scope* module_scope;        // Root scope for this module
    Scope** all_scopes;         // All scopes (for cleanup)
    size_t scope_count;
    size_t scope_capacity;
    Arena* arena;               // Arena for allocations
} SymbolTable;

// Initialize symbol table
void symbol_table_init(SymbolTable* table, Arena* arena);

// Create new scope
Scope* scope_new(SymbolTable* table, ScopeKind kind, Scope* parent, const char* name);

// Initialize scope with caller-provided memory
void scope_init(Scope* scope, ScopeKind kind, Scope* parent, Arena* arena);

// Symbol creation
Symbol* symbol_new(SymbolKind kind, const char* name, SourceLocation loc, Arena* arena);
Symbol* symbol_new_function(const char* name, AstNode* ast_node, Type* type,
                            bool is_public, SourceLocation loc, Arena* arena);
Symbol* symbol_new_type(const char* name, AstNode* ast_node, Type* type,
                       bool is_public, SourceLocation loc, Arena* arena);
Symbol* symbol_new_variable(const char* name, AstNode* ast_node, Type* type,
                           bool is_mutable, bool is_volatile, SourceLocation loc, Arena* arena);
Symbol* symbol_new_param(const char* name, AstNode* ast_node, Type* type,
                        size_t param_index, SourceLocation loc, Arena* arena);

// Symbol operations
void scope_insert(Scope* scope, Symbol* sym, Arena* arena);
Symbol* scope_lookup(Scope* scope, const char* name);          // Search up hierarchy
Symbol* scope_lookup_local(Scope* scope, const char* name);    // Current scope only

// Check for duplicate symbols
bool scope_has_symbol(Scope* scope, const char* name);

// Debugging
void scope_print(Scope* scope, FILE* out, int indent);
void symbol_print(Symbol* sym, FILE* out);

#endif // SYMBOL_H
