#include "tick.h"
#include "hashmap.h"
#include <string.h>

// ============================================================================
// Hashmap Adapters
// ============================================================================

// Global allocator for hashmap (set during context init)
static tick_alloc_t g_hashmap_alloc = {0};

static void* hashmap_malloc_adapter(size_t size) {
  tick_allocator_config_t config = {.flags = TICK_ALLOCATOR_ZEROMEM,
                                    .alignment2 = 0};
  tick_buf_t buf = {0};
  if (g_hashmap_alloc.realloc(g_hashmap_alloc.ctx, &buf, size, &config) !=
      TICK_OK) {
    return NULL;
  }
  return buf.buf;
}

static void* hashmap_realloc_adapter(void* ptr, size_t size) {
  tick_allocator_config_t config = {.flags = 0, .alignment2 = 0};
  tick_buf_t buf = {.buf = (u8*)ptr, .sz = 0};  // sz=0 means we don't know old size
  if (size == 0) {
    // Free - hashmap passes size=0 to free
    // For tick allocator, we can't free individual allocations
    // This is fine for arena-style allocators
    return NULL;
  }
  if (g_hashmap_alloc.realloc(g_hashmap_alloc.ctx, &buf, size, &config) !=
      TICK_OK) {
    return NULL;
  }
  return buf.buf;
}

static void hashmap_free_adapter(void* ptr) {
  UNUSED(ptr);
  // No-op for arena allocators
  // If using seglist allocator, individual frees aren't tracked
}

// ============================================================================
// Hash and Compare Functions
// ============================================================================

// Hash function for symbol entries (using name as key)
static uint64_t hash_symbol(const void* item, uint64_t seed0, uint64_t seed1) {
  const tick_symbol_t* sym = item;
  return hashmap_sip(sym->name.buf, sym->name.sz, seed0, seed1);
}

// Compare function for symbol entries
static int compare_symbol(const void* a, const void* b, void* udata) {
  UNUSED(udata);
  const tick_symbol_t* sym_a = a;
  const tick_symbol_t* sym_b = b;

  if (sym_a->name.sz != sym_b->name.sz) {
    return (int)sym_a->name.sz - (int)sym_b->name.sz;
  }
  return memcmp(sym_a->name.buf, sym_b->name.buf, sym_a->name.sz);
}

// Hash function for type entries
static uint64_t hash_type(const void* item, uint64_t seed0, uint64_t seed1) {
  const tick_type_entry_t* entry = item;
  return hashmap_sip(entry->name.buf, entry->name.sz, seed0, seed1);
}

// Compare function for type entries
static int compare_type(const void* a, const void* b, void* udata) {
  UNUSED(udata);
  const tick_type_entry_t* entry_a = a;
  const tick_type_entry_t* entry_b = b;

  if (entry_a->name.sz != entry_b->name.sz) {
    return (int)entry_a->name.sz - (int)entry_b->name.sz;
  }
  return memcmp(entry_a->name.buf, entry_b->name.buf, entry_a->name.sz);
}

// ============================================================================
// Scope Lifecycle
// ============================================================================

tick_scope_t* tick_scope_create(tick_scope_t* parent, tick_alloc_t alloc) {
  // Allocate scope structure
  tick_allocator_config_t config = {.flags = TICK_ALLOCATOR_ZEROMEM,
                                    .alignment2 = 0};
  tick_buf_t buf = {0};
  if (alloc.realloc(alloc.ctx, &buf, sizeof(tick_scope_t), &config) !=
      TICK_OK) {
    return NULL;
  }

  tick_scope_t* scope = (tick_scope_t*)buf.buf;
  scope->parent = parent;
  scope->alloc = alloc;

  // Set global allocator for hashmap
  g_hashmap_alloc = alloc;

  // Create symbol hashmap
  scope->symbols = hashmap_new_with_allocator(
      hashmap_malloc_adapter, hashmap_realloc_adapter, hashmap_free_adapter,
      sizeof(tick_symbol_t), 16, 0, 0, hash_symbol, compare_symbol, NULL, NULL);

  if (!scope->symbols) {
    return NULL;
  }

  return scope;
}

void tick_scope_destroy(tick_scope_t* scope) {
  if (!scope) return;
  if (scope->symbols) {
    hashmap_free(scope->symbols);
  }
  // Note: We don't free the scope itself - arena allocator handles it
}

// ============================================================================
// Symbol Operations
// ============================================================================

tick_err_t tick_scope_insert_symbol(tick_scope_t* scope, tick_buf_t name,
                                    tick_ast_node_t* decl) {
  if (!scope || !decl) return TICK_ERR;

  // Check for duplicate in current scope only
  tick_symbol_t lookup_key = {.name = name};
  if (hashmap_get(scope->symbols, &lookup_key) != NULL) {
    // Duplicate symbol in same scope
    return TICK_ERR;
  }

  // Create new symbol entry
  tick_symbol_t new_symbol = {
      .name = name,
      .decl = decl,
      .type = NULL,  // Filled during type analysis
  };

  // Insert into hashmap
  const void* result = hashmap_set(scope->symbols, &new_symbol);
  if (!result) {
    return TICK_ERR;
  }

  return TICK_OK;
}

tick_symbol_t* tick_scope_lookup_symbol(tick_scope_t* scope, tick_buf_t name) {
  if (!scope) return NULL;

  // Walk up the scope chain
  for (tick_scope_t* current = scope; current; current = current->parent) {
    tick_symbol_t lookup_key = {.name = name};
    const tick_symbol_t* result = hashmap_get(current->symbols, &lookup_key);
    if (result) {
      // Return mutable pointer (hashmap stores by value, but we can cast away const)
      return (tick_symbol_t*)result;
    }
  }

  return NULL;
}

tick_symbol_t* tick_scope_lookup_local(tick_scope_t* scope, tick_buf_t name) {
  if (!scope) return NULL;

  tick_symbol_t lookup_key = {.name = name};
  const tick_symbol_t* result = hashmap_get(scope->symbols, &lookup_key);
  return (tick_symbol_t*)result;
}

// ============================================================================
// Type Operations
// ============================================================================

tick_err_t tick_types_insert(struct hashmap* types, tick_buf_t name,
                             tick_ast_node_t* decl,
                             tick_builtin_type_t builtin_type,
                             tick_alloc_t alloc) {
  UNUSED(alloc);
  if (!types) return TICK_ERR;

  // Check for duplicate
  tick_type_entry_t lookup_key = {.name = name};
  if (hashmap_get(types, &lookup_key) != NULL) {
    // Duplicate type
    return TICK_ERR;
  }

  // Create new type entry
  tick_type_entry_t new_entry = {
      .name = name,
      .decl = decl,  // NULL for builtin types
      .builtin_type = builtin_type,
  };

  // Insert into hashmap
  const void* result = hashmap_set(types, &new_entry);
  if (!result) {
    return TICK_ERR;
  }

  return TICK_OK;
}

tick_type_entry_t* tick_types_lookup(struct hashmap* types, tick_buf_t name) {
  if (!types) return NULL;

  tick_type_entry_t lookup_key = {.name = name};
  const tick_type_entry_t* result = hashmap_get(types, &lookup_key);
  return (tick_type_entry_t*)result;
}

// ============================================================================
// Context Management
// ============================================================================

// Helper to register builtin types
static tick_err_t register_builtin_type(struct hashmap* types,
                                       const char* name,
                                       tick_builtin_type_t builtin_type,
                                       tick_alloc_t alloc) {
  tick_buf_t name_buf = {.buf = (u8*)name, .sz = strlen(name)};
  return tick_types_insert(types, name_buf, NULL, builtin_type, alloc);
}

void tick_analyze_ctx_init(tick_analyze_ctx_t* ctx, tick_alloc_t alloc,
                           tick_buf_t errbuf) {
  if (!ctx) return;

  ctx->alloc = alloc;
  ctx->errbuf = errbuf;

  // Set global allocator for hashmap
  g_hashmap_alloc = alloc;

  // Create global type table
  ctx->types = hashmap_new_with_allocator(
      hashmap_malloc_adapter, hashmap_realloc_adapter, hashmap_free_adapter,
      sizeof(tick_type_entry_t), 32, 0, 0, hash_type, compare_type, NULL, NULL);

  // Pre-populate with builtin types
  register_builtin_type(ctx->types, "i8", TICK_TYPE_I8, alloc);
  register_builtin_type(ctx->types, "i16", TICK_TYPE_I16, alloc);
  register_builtin_type(ctx->types, "i32", TICK_TYPE_I32, alloc);
  register_builtin_type(ctx->types, "i64", TICK_TYPE_I64, alloc);
  register_builtin_type(ctx->types, "isz", TICK_TYPE_ISZ, alloc);
  register_builtin_type(ctx->types, "u8", TICK_TYPE_U8, alloc);
  register_builtin_type(ctx->types, "u16", TICK_TYPE_U16, alloc);
  register_builtin_type(ctx->types, "u32", TICK_TYPE_U32, alloc);
  register_builtin_type(ctx->types, "u64", TICK_TYPE_U64, alloc);
  register_builtin_type(ctx->types, "usz", TICK_TYPE_USZ, alloc);
  register_builtin_type(ctx->types, "bool", TICK_TYPE_BOOL, alloc);
  register_builtin_type(ctx->types, "void", TICK_TYPE_VOID, alloc);

  // Create module scope
  ctx->module_scope = tick_scope_create(NULL, alloc);
  ctx->current_scope = ctx->module_scope;
}

void tick_analyze_ctx_destroy(tick_analyze_ctx_t* ctx) {
  if (!ctx) return;

  // Free type table
  if (ctx->types) {
    hashmap_free(ctx->types);
  }

  // Free all scopes (walk from current up to module)
  // Note: For arena allocators, this is mostly a no-op
  tick_scope_t* scope = ctx->current_scope;
  while (scope) {
    tick_scope_t* parent = scope->parent;
    tick_scope_destroy(scope);
    scope = parent;
  }
}

void tick_scope_push(tick_analyze_ctx_t* ctx) {
  if (!ctx) return;

  // Create new scope with current scope as parent
  tick_scope_t* new_scope = tick_scope_create(ctx->current_scope, ctx->alloc);
  ctx->current_scope = new_scope;
}

void tick_scope_pop(tick_analyze_ctx_t* ctx) {
  if (!ctx || !ctx->current_scope) return;

  // Move to parent scope
  tick_scope_t* old_scope = ctx->current_scope;
  ctx->current_scope = old_scope->parent;

  // Destroy old scope
  tick_scope_destroy(old_scope);
}
