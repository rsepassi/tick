#include "tick.h"

// ============================================================================
// Analysis and Lowering Passes
// ============================================================================
//
// This module implements the analysis and lowering passes for the Tick AST.
//
// IMPORTANT: See tick.h "AST Pipeline and Lowering Target" for the complete
// specification of what these passes must establish for codegen.
//
// Summary:
// - Analysis: Resolve types, populate resolved_type fields, validate
// - Lowering: Transform high-level types to basic types (when implemented)
//
// See tick.h for full details on the pipeline contract.
//
// ============================================================================

// ============================================================================
// Type Analysis Helper Functions
// ============================================================================

// Helper to allocate a type node
static tick_ast_node_t* alloc_type_node(tick_alloc_t alloc, tick_builtin_type_t builtin_type) {
  tick_allocator_config_t config = {
    .flags = TICK_ALLOCATOR_ZEROMEM,
    .alignment2 = 0,
  };
  tick_buf_t buf = {0};
  tick_err_t err = alloc.realloc(alloc.ctx, &buf, sizeof(tick_ast_node_t), &config);
  if (err != TICK_OK) {
    return NULL;
  }

  tick_ast_node_t* node = (tick_ast_node_t*)buf.buf;
  node->kind = TICK_AST_TYPE_NAMED;
  node->data.type_named.builtin_type = builtin_type;
  node->loc.line = 0;  // Synthesized node
  node->loc.col = 0;
  return node;
}

// Map type name to builtin type enum
static tick_builtin_type_t resolve_type_name(tick_buf_t name) {
  #define TYPE_CMP(str, type) \
    if (name.sz == sizeof(str)-1 && memcmp(name.buf, str, sizeof(str)-1) == 0) return type

  TYPE_CMP("i8", TICK_TYPE_I8);
  TYPE_CMP("i16", TICK_TYPE_I16);
  TYPE_CMP("i32", TICK_TYPE_I32);
  TYPE_CMP("i64", TICK_TYPE_I64);
  TYPE_CMP("isz", TICK_TYPE_ISZ);
  TYPE_CMP("u8", TICK_TYPE_U8);
  TYPE_CMP("u16", TICK_TYPE_U16);
  TYPE_CMP("u32", TICK_TYPE_U32);
  TYPE_CMP("u64", TICK_TYPE_U64);
  TYPE_CMP("usz", TICK_TYPE_USZ);
  TYPE_CMP("bool", TICK_TYPE_BOOL);
  TYPE_CMP("void", TICK_TYPE_VOID);

  #undef TYPE_CMP
  return TICK_TYPE_USER_DEFINED;
}

// Forward declaration
static tick_ast_node_t* analyze_expr(tick_ast_node_t* expr, tick_alloc_t alloc, tick_buf_t errbuf);

// Analyze type node - resolve builtin type names
static tick_err_t analyze_type(tick_ast_node_t* type_node) {
  if (!type_node) return TICK_OK;

  switch (type_node->kind) {
    case TICK_AST_TYPE_NAMED:
      if (type_node->data.type_named.builtin_type == TICK_TYPE_UNKNOWN) {
        type_node->data.type_named.builtin_type =
          resolve_type_name(type_node->data.type_named.name);
      }
      return TICK_OK;

    case TICK_AST_TYPE_POINTER:
      return analyze_type(type_node->data.type_pointer.pointee_type);

    case TICK_AST_TYPE_ARRAY:
      return analyze_type(type_node->data.type_array.element_type);

    default:
      return TICK_OK;
  }
}

// Get the type of an expression (returns resolved_type or infers it)
static tick_ast_node_t* analyze_expr(tick_ast_node_t* expr, tick_alloc_t alloc, tick_buf_t errbuf) {
  UNUSED(errbuf);

  if (!expr) return NULL;

  switch (expr->kind) {
    case TICK_AST_LITERAL: {
      // For now, assume literals are i32
      // TODO: Better literal type inference
      return alloc_type_node(alloc, TICK_TYPE_I32);
    }

    case TICK_AST_BINARY_EXPR: {
      // Check if already resolved
      if (expr->data.binary_expr.resolved_type) {
        return expr->data.binary_expr.resolved_type;
      }

      // Analyze operands
      tick_ast_node_t* left_type = analyze_expr(expr->data.binary_expr.left, alloc, errbuf);
      UNUSED(analyze_expr(expr->data.binary_expr.right, alloc, errbuf));  // TODO: Type checking

      // For simplicity, use left operand's type as result type
      // TODO: Proper type checking and type promotion
      tick_ast_node_t* result_type = NULL;
      if (left_type && left_type->kind == TICK_AST_TYPE_NAMED) {
        result_type = alloc_type_node(alloc, left_type->data.type_named.builtin_type);
      }

      expr->data.binary_expr.resolved_type = result_type;
      return result_type;
    }

    case TICK_AST_UNARY_EXPR: {
      // Check if already resolved
      if (expr->data.unary_expr.resolved_type) {
        return expr->data.unary_expr.resolved_type;
      }

      // Result type is same as operand type for most unary ops
      tick_ast_node_t* operand_type = analyze_expr(expr->data.unary_expr.operand, alloc, errbuf);

      expr->data.unary_expr.resolved_type = operand_type;
      return operand_type;
    }

    case TICK_AST_CAST_EXPR: {
      // Check if already resolved
      if (expr->data.cast_expr.resolved_type) {
        return expr->data.cast_expr.resolved_type;
      }

      // Analyze source expression (for validation)
      analyze_expr(expr->data.cast_expr.expr, alloc, errbuf);

      // Result type is the cast target type
      analyze_type(expr->data.cast_expr.type);
      expr->data.cast_expr.resolved_type = expr->data.cast_expr.type;
      return expr->data.cast_expr.type;
    }

    case TICK_AST_IDENTIFIER_EXPR: {
      // TODO: Look up identifier in symbol table to get its type
      // For now, assume i32
      return alloc_type_node(alloc, TICK_TYPE_I32);
    }

    case TICK_AST_CALL_EXPR: {
      // TODO: Get return type from function signature
      // For now, assume i32
      return alloc_type_node(alloc, TICK_TYPE_I32);
    }

    default:
      return NULL;
  }
}

// Analyze a statement
static tick_err_t analyze_stmt(tick_ast_node_t* stmt, tick_alloc_t alloc, tick_buf_t errbuf) {
  if (!stmt) return TICK_OK;

  switch (stmt->kind) {
    case TICK_AST_BLOCK_STMT: {
      for (tick_ast_node_t* s = stmt->data.block_stmt.stmts; s; s = s->next) {
        CHECK_OK(analyze_stmt(s, alloc, errbuf));
      }
      return TICK_OK;
    }

    case TICK_AST_RETURN_STMT:
      analyze_expr(stmt->data.return_stmt.value, alloc, errbuf);
      return TICK_OK;

    case TICK_AST_LET_STMT:
    case TICK_AST_VAR_STMT:
    case TICK_AST_DECL: {
      // Analyze declaration type
      CHECK_OK(analyze_type(stmt->data.decl.type));
      // Analyze initializer if present
      analyze_expr(stmt->data.decl.init, alloc, errbuf);
      return TICK_OK;
    }

    case TICK_AST_ASSIGN_STMT:
      analyze_expr(stmt->data.assign_stmt.lhs, alloc, errbuf);
      analyze_expr(stmt->data.assign_stmt.rhs, alloc, errbuf);
      return TICK_OK;

    case TICK_AST_EXPR_STMT:
      analyze_expr(stmt->data.expr_stmt.expr, alloc, errbuf);
      return TICK_OK;

    case TICK_AST_IF_STMT:
      analyze_expr(stmt->data.if_stmt.condition, alloc, errbuf);
      CHECK_OK(analyze_stmt(stmt->data.if_stmt.then_block, alloc, errbuf));
      CHECK_OK(analyze_stmt(stmt->data.if_stmt.else_block, alloc, errbuf));
      return TICK_OK;

    default:
      return TICK_OK;
  }
}

// Analyze a function declaration
static tick_err_t analyze_function(tick_ast_node_t* decl, tick_alloc_t alloc, tick_buf_t errbuf) {
  if (!decl || decl->kind != TICK_AST_DECL) return TICK_OK;

  tick_ast_node_t* func = decl->data.decl.init;
  if (!func || func->kind != TICK_AST_FUNCTION) return TICK_OK;

  // Analyze return type
  CHECK_OK(analyze_type(func->data.function.return_type));

  // Analyze parameters
  for (tick_ast_node_t* param = func->data.function.params; param; param = param->next) {
    if (param->kind == TICK_AST_PARAM) {
      CHECK_OK(analyze_type(param->data.param.type));
    }
  }

  // Analyze function body
  CHECK_OK(analyze_stmt(func->data.function.body, alloc, errbuf));

  return TICK_OK;
}

tick_err_t tick_ast_analyze(tick_ast_t* ast, tick_alloc_t alloc, tick_buf_t errbuf) {
  if (!ast || !ast->root) return TICK_OK;

  if (ast->root->kind != TICK_AST_MODULE) {
    return TICK_ERR;
  }

  tick_ast_node_t* module = ast->root;

  // Walk module declarations
  for (tick_ast_node_t* decl = module->data.module.decls; decl; decl = decl->next) {
    if (decl->kind == TICK_AST_DECL) {
      // Check if it's a function
      if (decl->data.decl.init && decl->data.decl.init->kind == TICK_AST_FUNCTION) {
        CHECK_OK(analyze_function(decl, alloc, errbuf));
      }
      // TODO: Handle global variables, structs, etc.
    }
  }

  return TICK_OK;
}

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
tick_err_t tick_ast_lower(tick_ast_t* ast, tick_alloc_t alloc, tick_buf_t errbuf) {
  UNUSED(ast);
  UNUSED(alloc);
  UNUSED(errbuf);
  // Accept all ASTs for now (stub implementation)
  // When this is implemented, it should walk the AST and perform the
  // transformations described above.
  return TICK_OK;
}
