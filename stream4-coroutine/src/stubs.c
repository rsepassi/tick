// Temporary stub implementations for missing dependencies
// These will be replaced with proper implementations once stream3-semantic is reconciled

#include <type.h>
#include <symbol.h>
#include <arena.h>
#include <stddef.h>
#include <string.h>

// Type utility functions (normally from stream3-semantic/typeck.c)
size_t type_sizeof(Type* t) {
    if (!t) return 0;
    return t->size;
}

size_t type_alignof(Type* t) {
    if (!t) return 1;
    return t->alignment;
}

// Scope utility functions (normally from stream3-semantic/resolver.c)
void scope_init(Scope* scope, ScopeKind kind, Scope* parent, Arena* arena) {
    (void)arena; // unused
    memset(scope, 0, sizeof(Scope));
    scope->kind = kind;
    scope->parent = parent;
}
