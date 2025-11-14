#include "tick.h"

// ============================================================================
// Lowering Pass
// ============================================================================
//
// See tick.h "AST Pipeline and Lowering Target" for specification.
//
// Current implementation is a stub. When implemented, this will transform:
// - High-level types (slices, optionals, error unions) → basic types
// - Defer statements → cleanup code
// - Async/await → state machines
//
// NOTE: Arithmetic operations are NOT lowered - codegen handles them directly.
//
// TODO: Implement decomposition of struct/array initializers:
//   - Walk STRUCT_INIT_EXPR and ARRAY_INIT_EXPR nodes
//   - For each field value / array element:
//     * If expression is LITERAL or IDENTIFIER_EXPR: leave as-is
//     * Otherwise: extract to temporary variable (__tmp{N})
//   - Example transformation:
//     * Point { x: foo() + 1, y: 2 } becomes:
//       let __tmp1: i32 = foo() + 1;
//       Point { x: __tmp1, y: 2 }
//   - This keeps codegen simple (only handles literals and identifiers)
//   - See tick.h line 388 for full specification
//

// Helper to allocate a new AST node (for forward declarations)
static tick_ast_node_t* alloc_ast_node(tick_alloc_t alloc) {
  tick_allocator_config_t config = {.flags = TICK_ALLOCATOR_ZEROMEM,
                                    .alignment2 = 0};
  tick_buf_t buf = {0};
  if (alloc.realloc(alloc.ctx, &buf, sizeof(tick_ast_node_t), &config) !=
      TICK_OK) {
    return NULL;
  }
  return (tick_ast_node_t*)buf.buf;
}

tick_err_t tick_ast_lower(tick_ast_t* ast, tick_alloc_t alloc,
                          tick_buf_t errbuf) {
  UNUSED(errbuf);

  if (!ast || !ast->root || ast->root->kind != TICK_AST_MODULE) {
    return TICK_OK;
  }

  tick_ast_node_t* module = ast->root;

  // PASS 1: Build new declaration list with forward declarations at the
  // beginning Create forward declarations for ALL struct/enum/union types
  tick_ast_node_t* new_decls = NULL;
  tick_ast_node_t* new_decls_tail = NULL;

  // First, emit forward declarations for all user-defined types
  for (tick_ast_node_t* decl = module->module.decls; decl; decl = decl->next) {
    if (decl->kind == TICK_AST_DECL && decl->decl.init) {
      tick_ast_node_t* init = decl->decl.init;

      // Create forward declaration for struct/union types only
      // Enums don't support forward declarations in C
      if (init->kind == TICK_AST_STRUCT_DECL ||
          init->kind == TICK_AST_UNION_DECL) {
        tick_ast_node_t* fwd_decl = alloc_ast_node(alloc);
        if (fwd_decl) {
          // Copy the declaration but mark as forward
          *fwd_decl = *decl;
          fwd_decl->decl.quals.is_forward_decl = true;
          fwd_decl->node_flags =
              TICK_NODE_FLAG_SYNTHETIC | TICK_NODE_FLAG_LOWERED;
          fwd_decl->next = NULL;

          // Append forward declaration
          if (!new_decls) {
            new_decls = fwd_decl;
            new_decls_tail = fwd_decl;
          } else {
            new_decls_tail->next = fwd_decl;
            new_decls_tail = fwd_decl;
          }
        }
      }
    }
  }

  // Now append all original declarations
  tick_ast_node_t* curr = module->module.decls;
  while (curr) {
    tick_ast_node_t* next = curr->next;  // Save next before we modify anything

    // Append the original declaration
    curr->next = NULL;
    if (!new_decls) {
      new_decls = curr;
      new_decls_tail = curr;
    } else {
      new_decls_tail->next = curr;
      new_decls_tail = curr;
    }

    curr = next;
  }

  // Replace module declarations with new list
  module->module.decls = new_decls;

  return TICK_OK;
}
