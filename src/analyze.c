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
// Type Symbol Table
// ============================================================================

// Type symbol entry for user-defined types
typedef struct type_symbol_s {
  tick_buf_t name;
  tick_ast_node_t* decl;  // Pointer to STRUCT_DECL/ENUM_DECL/UNION_DECL node
  struct type_symbol_s* next;
} type_symbol_t;

// Register a type symbol
static tick_err_t register_type_symbol(type_symbol_t** head, tick_buf_t name,
                                       tick_ast_node_t* decl, tick_alloc_t alloc) {
  // Check for duplicates
  for (type_symbol_t* sym = *head; sym; sym = sym->next) {
    if (sym->name.sz == name.sz && memcmp(sym->name.buf, name.buf, name.sz) == 0) {
      // Duplicate type name - for now, just skip
      // TODO: Report error
      return TICK_OK;
    }
  }

  // Allocate new symbol
  tick_allocator_config_t config = { .flags = TICK_ALLOCATOR_ZEROMEM, .alignment2 = 0 };
  tick_buf_t buf = {0};
  CHECK_OK(alloc.realloc(alloc.ctx, &buf, sizeof(type_symbol_t), &config));

  type_symbol_t* sym = (type_symbol_t*)buf.buf;
  sym->name = name;
  sym->decl = decl;
  sym->next = *head;
  *head = sym;

  return TICK_OK;
}

// Look up a type symbol by name
static tick_ast_node_t* lookup_type_symbol(type_symbol_t* head, tick_buf_t name) {
  for (type_symbol_t* sym = head; sym; sym = sym->next) {
    if (sym->name.sz == name.sz && memcmp(sym->name.buf, name.buf, name.sz) == 0) {
      return sym->decl;
    }
  }
  return NULL;
}

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

// Helper to allocate a literal node
static tick_ast_node_t* alloc_literal_node(tick_alloc_t alloc, int64_t value) {
  tick_allocator_config_t config = { .flags = TICK_ALLOCATOR_ZEROMEM, .alignment2 = 0 };
  tick_buf_t buf = {0};
  if (alloc.realloc(alloc.ctx, &buf, sizeof(tick_ast_node_t), &config) != TICK_OK) return NULL;

  tick_ast_node_t* node = (tick_ast_node_t*)buf.buf;
  node->kind = TICK_AST_LITERAL;
  node->data.literal.kind = TICK_LIT_INT;
  node->data.literal.data.int_value = value;
  return node;
}

// ============================================================================
// Constant Expression Evaluation
// ============================================================================

// Evaluate a constant expression to an integer value
// Returns TICK_OK on success, TICK_ERR if the expression is not constant
static tick_err_t eval_const_expr(tick_ast_node_t* expr, int64_t* out_value) {
  if (!expr || !out_value) return TICK_ERR;

  switch (expr->kind) {
    case TICK_AST_LITERAL: {
      if (expr->data.literal.kind == TICK_LIT_INT) {
        *out_value = expr->data.literal.data.int_value;
        return TICK_OK;
      } else if (expr->data.literal.kind == TICK_LIT_UINT) {
        *out_value = (int64_t)expr->data.literal.data.uint_value;
        return TICK_OK;
      }
      return TICK_ERR;
    }

    case TICK_AST_BINARY_EXPR: {
      int64_t left, right;
      if (eval_const_expr(expr->data.binary_expr.left, &left) != TICK_OK) return TICK_ERR;
      if (eval_const_expr(expr->data.binary_expr.right, &right) != TICK_OK) return TICK_ERR;

      switch (expr->data.binary_expr.op) {
        case BINOP_ADD:
        case BINOP_SAT_ADD:
        case BINOP_WRAP_ADD:
          *out_value = left + right;
          return TICK_OK;

        case BINOP_SUB:
        case BINOP_SAT_SUB:
        case BINOP_WRAP_SUB:
          *out_value = left - right;
          return TICK_OK;

        case BINOP_MUL:
        case BINOP_SAT_MUL:
        case BINOP_WRAP_MUL:
          *out_value = left * right;
          return TICK_OK;

        case BINOP_DIV:
        case BINOP_SAT_DIV:
        case BINOP_WRAP_DIV:
          if (right == 0) return TICK_ERR;
          *out_value = left / right;
          return TICK_OK;

        case BINOP_MOD:
          if (right == 0) return TICK_ERR;
          *out_value = left % right;
          return TICK_OK;

        case BINOP_LSHIFT:
          *out_value = left << right;
          return TICK_OK;

        case BINOP_RSHIFT:
          *out_value = left >> right;
          return TICK_OK;

        case BINOP_AND:
          *out_value = left & right;
          return TICK_OK;

        case BINOP_OR:
          *out_value = left | right;
          return TICK_OK;

        case BINOP_XOR:
          *out_value = left ^ right;
          return TICK_OK;

        default:
          return TICK_ERR;
      }
    }

    case TICK_AST_UNARY_EXPR: {
      int64_t operand;
      if (eval_const_expr(expr->data.unary_expr.operand, &operand) != TICK_OK) return TICK_ERR;

      switch (expr->data.unary_expr.op) {
        case UNOP_NEG:
          *out_value = -operand;
          return TICK_OK;

        case UNOP_BIT_NOT:
          *out_value = ~operand;
          return TICK_OK;

        default:
          return TICK_ERR;
      }
    }

    default:
      return TICK_ERR;
  }
}

// Reduce a constant expression to a literal node
// Replaces *expr with a new LITERAL node containing the evaluated value
static tick_err_t reduce_to_literal(tick_ast_node_t** expr, tick_alloc_t alloc) {
  if (!expr || !*expr) return TICK_ERR;

  int64_t value;
  if (eval_const_expr(*expr, &value) != TICK_OK) return TICK_ERR;

  tick_ast_node_t* literal = alloc_literal_node(alloc, value);
  if (!literal) return TICK_ERR;

  // Preserve location information from original expression
  literal->loc = (*expr)->loc;

  *expr = literal;
  return TICK_OK;
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

// Forward declarations
static tick_ast_node_t* analyze_expr(tick_ast_node_t* expr, type_symbol_t* type_symbols, tick_alloc_t alloc, tick_buf_t errbuf);
static tick_err_t analyze_type(tick_ast_node_t* type_node, type_symbol_t* type_symbols, tick_alloc_t alloc);

// Analyze type node - resolve builtin type names and user-defined types
static tick_err_t analyze_type(tick_ast_node_t* type_node, type_symbol_t* type_symbols, tick_alloc_t alloc) {
  if (!type_node) return TICK_OK;

  switch (type_node->kind) {
    case TICK_AST_TYPE_NAMED:
      if (type_node->data.type_named.builtin_type == TICK_TYPE_UNKNOWN) {
        type_node->data.type_named.builtin_type =
          resolve_type_name(type_node->data.type_named.name);
      }

      // If it's a user-defined type, look it up in the symbol table
      if (type_node->data.type_named.builtin_type == TICK_TYPE_USER_DEFINED) {
        tick_ast_node_t* decl = lookup_type_symbol(type_symbols, type_node->data.type_named.name);
        type_node->data.type_named.type_decl = decl;
        // If not found, type_decl will be NULL (error will be caught later)
      }
      return TICK_OK;

    case TICK_AST_TYPE_POINTER:
      return analyze_type(type_node->data.type_pointer.pointee_type, type_symbols, alloc);

    case TICK_AST_TYPE_ARRAY:
      // Reduce array size to literal if it's a constant expression
      if (type_node->data.type_array.size) {
        // Try to reduce to literal, but don't fail if it's already a literal
        reduce_to_literal(&type_node->data.type_array.size, alloc);
      }
      return analyze_type(type_node->data.type_array.element_type, type_symbols, alloc);

    case TICK_AST_TYPE_FUNCTION: {
      // Analyze return type
      tick_err_t err = analyze_type(type_node->data.type_function.return_type, type_symbols, alloc);
      if (err != TICK_OK) return err;

      // Analyze parameter types
      tick_ast_node_t* param = type_node->data.type_function.params;
      while (param) {
        err = analyze_type(param->data.param.type, type_symbols, alloc);
        if (err != TICK_OK) return err;
        param = param->next;
      }
      return TICK_OK;
    }

    default:
      return TICK_OK;
  }
}

// Get the type of an expression (returns resolved_type or infers it)
static tick_ast_node_t* analyze_expr(tick_ast_node_t* expr, type_symbol_t* type_symbols, tick_alloc_t alloc, tick_buf_t errbuf) {
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
      tick_ast_node_t* left_type = analyze_expr(expr->data.binary_expr.left, type_symbols, alloc, errbuf);
      tick_ast_node_t* right_type = analyze_expr(expr->data.binary_expr.right, type_symbols, alloc, errbuf);

      tick_ast_node_t* result_type = NULL;

      // Special handling for orelse operator
      if (expr->data.binary_expr.op == BINOP_ORELSE) {
        // For 'a orelse b': a is ?T, b is T, result is T
        if (left_type && left_type->kind == TICK_AST_TYPE_OPTIONAL) {
          // Result type is the inner type of the optional
          result_type = left_type->data.type_optional.inner_type;
        } else if (right_type) {
          // Fallback: use right type
          result_type = right_type;
        }
      } else {
        // For simplicity, use left operand's type as result type
        // TODO: Proper type checking and type promotion
        if (left_type && left_type->kind == TICK_AST_TYPE_NAMED) {
          result_type = alloc_type_node(alloc, left_type->data.type_named.builtin_type);
        }
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
      tick_ast_node_t* operand_type = analyze_expr(expr->data.unary_expr.operand, type_symbols, alloc, errbuf);

      expr->data.unary_expr.resolved_type = operand_type;
      return operand_type;
    }

    case TICK_AST_CAST_EXPR: {
      // Check if already resolved
      if (expr->data.cast_expr.resolved_type) {
        return expr->data.cast_expr.resolved_type;
      }

      // Analyze source expression (for validation)
      analyze_expr(expr->data.cast_expr.expr, type_symbols, alloc, errbuf);

      // Result type is the cast target type
      analyze_type(expr->data.cast_expr.type, type_symbols, alloc);
      expr->data.cast_expr.resolved_type = expr->data.cast_expr.type;
      return expr->data.cast_expr.type;
    }

    case TICK_AST_STRUCT_INIT_EXPR: {
      // Analyze the struct type
      analyze_type(expr->data.struct_init_expr.type, type_symbols, alloc);

      // Analyze field initializer expressions
      for (tick_ast_node_t* field = expr->data.struct_init_expr.fields; field; field = field->next) {
        if (field->kind == TICK_AST_STRUCT_INIT_FIELD) {
          analyze_expr(field->data.struct_init_field.value, type_symbols, alloc, errbuf);
        }
      }

      return expr->data.struct_init_expr.type;
    }

    case TICK_AST_IDENTIFIER_EXPR: {
      // Check if this is an AT builtin (@identifier)
      if (expr->data.identifier_expr.name.sz > 0 &&
          expr->data.identifier_expr.name.buf[0] == '@') {
        // Resolve @builtin string to enum
        if (expr->data.identifier_expr.name.sz == 4 &&
            memcmp(expr->data.identifier_expr.name.buf, "@dbg", 4) == 0) {
          expr->data.identifier_expr.at_builtin = TICK_AT_BUILTIN_DBG;
        } else {
          // Unknown builtin
          snprintf((char*)errbuf.buf, errbuf.sz,
                   "%zu:%zu: unknown builtin '%.*s'\n",
                   expr->loc.line, expr->loc.col,
                   (int)expr->data.identifier_expr.name.sz,
                   expr->data.identifier_expr.name.buf);
          return NULL;
        }
      }

      // TODO: Look up identifier in symbol table to get its type
      // For now, assume i32
      return alloc_type_node(alloc, TICK_TYPE_I32);
    }

    case TICK_AST_CALL_EXPR: {
      // Analyze callee (this will resolve @builtin identifiers)
      analyze_expr(expr->data.call_expr.callee, type_symbols, alloc, errbuf);

      // Analyze arguments
      for (tick_ast_node_t* arg = expr->data.call_expr.args; arg; arg = arg->next) {
        analyze_expr(arg, type_symbols, alloc, errbuf);
      }

      // TODO: Get return type from function signature
      // For now, assume i32
      return alloc_type_node(alloc, TICK_TYPE_I32);
    }

    case TICK_AST_UNWRAP_PANIC_EXPR: {
      // Check if already resolved
      if (expr->data.unwrap_panic_expr.resolved_type) {
        return expr->data.unwrap_panic_expr.resolved_type;
      }

      // Analyze operand
      tick_ast_node_t* operand_type = analyze_expr(expr->data.unwrap_panic_expr.operand, type_symbols, alloc, errbuf);

      // For 'a.?': a is ?T, result is T
      tick_ast_node_t* result_type = NULL;
      if (operand_type && operand_type->kind == TICK_AST_TYPE_OPTIONAL) {
        // Result type is the inner type of the optional
        result_type = operand_type->data.type_optional.inner_type;
      }

      expr->data.unwrap_panic_expr.resolved_type = result_type;
      return result_type;
    }

    default:
      return NULL;
  }
}

// Analyze a statement
static tick_err_t analyze_stmt(tick_ast_node_t* stmt, type_symbol_t* type_symbols,
                                tick_alloc_t alloc, tick_buf_t errbuf) {
  if (!stmt) return TICK_OK;

  switch (stmt->kind) {
    case TICK_AST_BLOCK_STMT: {
      for (tick_ast_node_t* s = stmt->data.block_stmt.stmts; s; s = s->next) {
        CHECK_OK(analyze_stmt(s, type_symbols, alloc, errbuf));
      }
      return TICK_OK;
    }

    case TICK_AST_RETURN_STMT:
      analyze_expr(stmt->data.return_stmt.value, type_symbols, alloc, errbuf);
      return TICK_OK;

    case TICK_AST_LET_STMT:
    case TICK_AST_VAR_STMT:
    case TICK_AST_DECL: {
      // If type is NULL, infer from initializer
      if (stmt->data.decl.type == NULL && stmt->data.decl.init) {
        tick_ast_node_t* inferred_type = analyze_expr(stmt->data.decl.init, type_symbols, alloc, errbuf);
        stmt->data.decl.type = inferred_type;
      }
      // Analyze declaration type
      CHECK_OK(analyze_type(stmt->data.decl.type, type_symbols, alloc));
      // Analyze initializer if present
      analyze_expr(stmt->data.decl.init, type_symbols, alloc, errbuf);
      return TICK_OK;
    }

    case TICK_AST_ASSIGN_STMT:
      analyze_expr(stmt->data.assign_stmt.lhs, type_symbols, alloc, errbuf);
      analyze_expr(stmt->data.assign_stmt.rhs, type_symbols, alloc, errbuf);
      return TICK_OK;

    case TICK_AST_EXPR_STMT:
      analyze_expr(stmt->data.expr_stmt.expr, type_symbols, alloc, errbuf);
      return TICK_OK;

    case TICK_AST_IF_STMT:
      analyze_expr(stmt->data.if_stmt.condition, type_symbols, alloc, errbuf);
      CHECK_OK(analyze_stmt(stmt->data.if_stmt.then_block, type_symbols, alloc, errbuf));
      CHECK_OK(analyze_stmt(stmt->data.if_stmt.else_block, type_symbols, alloc, errbuf));
      return TICK_OK;

    default:
      return TICK_OK;
  }
}

// Analyze an enum declaration and populate auto-increment values
static tick_err_t analyze_enum_decl(tick_ast_node_t* decl, type_symbol_t* type_symbols,
                                     tick_alloc_t alloc, tick_buf_t errbuf) {
  UNUSED(errbuf);

  if (!decl || decl->kind != TICK_AST_DECL) return TICK_OK;

  tick_ast_node_t* enum_node = decl->data.decl.init;
  if (!enum_node || enum_node->kind != TICK_AST_ENUM_DECL) return TICK_OK;

  // Analyze underlying type
  CHECK_OK(analyze_type(enum_node->data.enum_decl.underlying_type, type_symbols, alloc));

  // Walk the enum values and calculate auto-increment
  // The list is in source order (parser builds it with append)
  // We can process it directly to assign auto-increment values

  // First pass: reduce all explicit values to literals
  for (tick_ast_node_t* value_node = enum_node->data.enum_decl.values;
       value_node;
       value_node = value_node->next) {
    if (value_node->kind != TICK_AST_ENUM_VALUE) continue;

    if (value_node->data.enum_value.value != NULL) {
      // Reduce to literal if needed
      reduce_to_literal(&value_node->data.enum_value.value, alloc);
    }
  }

  // Second pass: assign auto-increment values in source order
  int64_t current_value = 0;
  for (tick_ast_node_t* value_node = enum_node->data.enum_decl.values;
       value_node;
       value_node = value_node->next) {
    if (value_node->kind != TICK_AST_ENUM_VALUE) continue;

    if (value_node->data.enum_value.value == NULL) {
      // Auto-increment: create a literal node with current_value
      tick_ast_node_t* literal = alloc_literal_node(alloc, current_value);
      if (!literal) return TICK_ERR;
      value_node->data.enum_value.value = literal;
    } else {
      // Explicit value: already reduced in first pass
      current_value = value_node->data.enum_value.value->data.literal.data.int_value;
    }

    // Increment for next value
    current_value++;
  }

  return TICK_OK;
}

// Analyze a function declaration
static tick_err_t analyze_function(tick_ast_node_t* decl, type_symbol_t* type_symbols,
                                    tick_alloc_t alloc, tick_buf_t errbuf) {
  if (!decl || decl->kind != TICK_AST_DECL) return TICK_OK;

  tick_ast_node_t* func = decl->data.decl.init;
  if (!func || func->kind != TICK_AST_FUNCTION) return TICK_OK;

  // Analyze return type
  CHECK_OK(analyze_type(func->data.function.return_type, type_symbols, alloc));

  // Analyze parameters
  for (tick_ast_node_t* param = func->data.function.params; param; param = param->next) {
    if (param->kind == TICK_AST_PARAM) {
      CHECK_OK(analyze_type(param->data.param.type, type_symbols, alloc));
    }
  }

  // Analyze function body
  CHECK_OK(analyze_stmt(func->data.function.body, type_symbols, alloc, errbuf));

  return TICK_OK;
}

// Analyze struct declaration - resolve field types
static tick_err_t analyze_struct_decl(tick_ast_node_t* decl, type_symbol_t* type_symbols, tick_alloc_t alloc) {
  if (!decl || decl->kind != TICK_AST_DECL) return TICK_OK;

  tick_ast_node_t* struct_node = decl->data.decl.init;
  if (!struct_node || struct_node->kind != TICK_AST_STRUCT_DECL) return TICK_OK;

  // Analyze field types
  for (tick_ast_node_t* field = struct_node->data.struct_decl.fields; field; field = field->next) {
    if (field->kind == TICK_AST_FIELD) {
      CHECK_OK(analyze_type(field->data.field.type, type_symbols, alloc));
    }
  }

  return TICK_OK;
}

// Analyze union declaration - resolve field types
static tick_err_t analyze_union_decl(tick_ast_node_t* decl, type_symbol_t* type_symbols, tick_alloc_t alloc) {
  if (!decl || decl->kind != TICK_AST_DECL) return TICK_OK;

  tick_ast_node_t* union_node = decl->data.decl.init;
  if (!union_node || union_node->kind != TICK_AST_UNION_DECL) return TICK_OK;

  // Analyze tag type if present
  if (union_node->data.union_decl.tag_type) {
    CHECK_OK(analyze_type(union_node->data.union_decl.tag_type, type_symbols, alloc));
  }

  // Analyze field types
  for (tick_ast_node_t* field = union_node->data.union_decl.fields; field; field = field->next) {
    if (field->kind == TICK_AST_FIELD) {
      CHECK_OK(analyze_type(field->data.field.type, type_symbols, alloc));
    }
  }

  return TICK_OK;
}

tick_err_t tick_ast_analyze(tick_ast_t* ast, tick_alloc_t alloc, tick_buf_t errbuf) {
  if (!ast || !ast->root) return TICK_OK;

  if (ast->root->kind != TICK_AST_MODULE) {
    return TICK_ERR;
  }

  tick_ast_node_t* module = ast->root;

  // Create type symbol table
  type_symbol_t* type_symbols = NULL;

  // PASS 1: Register all type declarations
  for (tick_ast_node_t* decl = module->data.module.decls; decl; decl = decl->next) {
    if (decl->kind == TICK_AST_DECL && decl->data.decl.init) {
      tick_ast_node_t* init = decl->data.decl.init;

      // Register struct/enum/union types
      if (init->kind == TICK_AST_STRUCT_DECL ||
          init->kind == TICK_AST_ENUM_DECL ||
          init->kind == TICK_AST_UNION_DECL) {
        // The type name is in the parent DECL node
        CHECK_OK(register_type_symbol(&type_symbols, decl->data.decl.name, init, alloc));
      }
    }
  }

  // PASS 2: Analyze all declarations with type resolution
  for (tick_ast_node_t* decl = module->data.module.decls; decl; decl = decl->next) {
    if (decl->kind == TICK_AST_DECL) {
      if (decl->data.decl.init) {
        // Analyze functions
        if (decl->data.decl.init->kind == TICK_AST_FUNCTION) {
          CHECK_OK(analyze_function(decl, type_symbols, alloc, errbuf));
        }
        // Analyze enums
        else if (decl->data.decl.init->kind == TICK_AST_ENUM_DECL) {
          CHECK_OK(analyze_enum_decl(decl, type_symbols, alloc, errbuf));
        }
        // Analyze structs
        else if (decl->data.decl.init->kind == TICK_AST_STRUCT_DECL) {
          CHECK_OK(analyze_struct_decl(decl, type_symbols, alloc));
        }
        // Analyze unions
        else if (decl->data.decl.init->kind == TICK_AST_UNION_DECL) {
          CHECK_OK(analyze_union_decl(decl, type_symbols, alloc));
        }
        else {
          // Regular variable declaration with initializer
          // If type is NULL, infer from initializer
          if (decl->data.decl.type == NULL) {
            tick_ast_node_t* inferred_type = analyze_expr(decl->data.decl.init, type_symbols, alloc, errbuf);
            decl->data.decl.type = inferred_type;
          }
          // Analyze the type
          if (decl->data.decl.type) {
            CHECK_OK(analyze_type(decl->data.decl.type, type_symbols, alloc));
          }
          // Analyze the initializer expression
          analyze_expr(decl->data.decl.init, type_symbols, alloc, errbuf);
        }
      } else {
        // Declarations without initializers (e.g., extern declarations)
        // Still need to resolve their types
        if (decl->data.decl.type) {
          CHECK_OK(analyze_type(decl->data.decl.type, type_symbols, alloc));
        }
      }
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

// Helper to allocate a new AST node (for forward declarations)
static tick_ast_node_t* alloc_ast_node(tick_alloc_t alloc) {
  tick_allocator_config_t config = { .flags = TICK_ALLOCATOR_ZEROMEM, .alignment2 = 0 };
  tick_buf_t buf = {0};
  if (alloc.realloc(alloc.ctx, &buf, sizeof(tick_ast_node_t), &config) != TICK_OK) {
    return NULL;
  }
  return (tick_ast_node_t*)buf.buf;
}

tick_err_t tick_ast_lower(tick_ast_t* ast, tick_alloc_t alloc, tick_buf_t errbuf) {
  UNUSED(errbuf);

  if (!ast || !ast->root || ast->root->kind != TICK_AST_MODULE) {
    return TICK_OK;
  }

  tick_ast_node_t* module = ast->root;

  // PASS 1: Build new declaration list with forward declarations at the beginning
  // Create forward declarations for ALL struct/enum/union types
  tick_ast_node_t* new_decls = NULL;
  tick_ast_node_t* new_decls_tail = NULL;

  // First, emit forward declarations for all user-defined types
  for (tick_ast_node_t* decl = module->data.module.decls; decl; decl = decl->next) {
    if (decl->kind == TICK_AST_DECL && decl->data.decl.init) {
      tick_ast_node_t* init = decl->data.decl.init;

      // Create forward declaration for struct/enum/union types
      if (init->kind == TICK_AST_STRUCT_DECL ||
          init->kind == TICK_AST_ENUM_DECL ||
          init->kind == TICK_AST_UNION_DECL) {
        tick_ast_node_t* fwd_decl = alloc_ast_node(alloc);
        if (fwd_decl) {
          // Copy the declaration but mark as forward
          *fwd_decl = *decl;
          fwd_decl->data.decl.quals.is_forward_decl = true;
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
  tick_ast_node_t* curr = module->data.module.decls;
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
  module->data.module.decls = new_decls;

  return TICK_OK;
}
