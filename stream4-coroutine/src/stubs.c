// Implementations for dependencies from stream3-semantic
// These are the actual implementations from Stream 3 (semantic analysis)

#include <type.h>
#include <symbol.h>
#include <arena.h>
#include <stddef.h>

// Type utility functions (from stream3-semantic/typeck.c)
size_t type_sizeof(Type* t) {
    return t->size;
}

size_t type_alignof(Type* t) {
    return t->alignment;
}

// Scope utility functions (from stream3-semantic/resolver.c)
void scope_init(Scope* scope, ScopeKind kind, Scope* parent, Arena* arena) {
    scope->kind = kind;
    scope->parent = parent;
    scope->symbols = NULL;
    scope->symbol_count = 0;
    scope->symbol_capacity = 0;
    scope->name = NULL;
    scope->owner_node = NULL;
}
