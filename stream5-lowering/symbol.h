#ifndef SYMBOL_H
#define SYMBOL_H

#include <stddef.h>
#include <stdbool.h>
#include "type.h"
#include "error.h"
#include "ast.h"

// Symbol Table Interface
// Purpose: Hierarchical name resolution and scope management
// Scope Hierarchy: Module → Function → Block (nested)
// No Forward Declarations: Single-pass resolution is sufficient

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
} SymbolKind;

typedef struct Symbol {
    SymbolKind kind;
    const char* name;        // Pointer into string pool
    Type* type;
    bool is_public;
    SourceLocation defined_at;

    union {
        struct {
            AstNode* function_node;
            bool is_coroutine;  // Filled by coro_analyze
        } function;

        struct {
            AstNode* var_node;
            bool is_volatile;
        } variable;

        // ... etc
    } data;
} Symbol;

typedef struct Scope {
    ScopeKind kind;
    struct Scope* parent;  // NULL for module scope
    Symbol** symbols;
    size_t symbol_count;
    size_t symbol_capacity;
} Scope;

// Forward declare Arena
typedef struct Arena Arena;

// Initialize scope with caller-provided memory
void scope_init(Scope* scope, ScopeKind kind, Scope* parent, Arena* arena);

// Symbol operations
void scope_insert(Scope* scope, Symbol* sym, Arena* arena);
Symbol* scope_lookup(Scope* scope, const char* name);          // Search up hierarchy
Symbol* scope_lookup_local(Scope* scope, const char* name);    // Current scope only

#endif // SYMBOL_H
