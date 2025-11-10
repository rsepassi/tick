# Tick Compiler Test Report
**Date:** 2025-11-10
**Compiler Version:** Current HEAD (commit c4788bf)

## Executive Summary

Testing of all 8 example programs revealed **critical defects** in the compiler's IR lowering and code generation phases. Only example 01 successfully compiles to C, but the generated C code is invalid and won't compile with GCC due to fundamental issues with identifier resolution in the IR lowering phase.

**Status:**
- ✅ **1 example generates C** (with errors)
- ❌ **4 examples fail during parsing**
- ❌ **3 examples fail during IR lowering**
- ❌ **0 examples produce compilable C code**

## Test Results by Example

### Example 01: 01_hello.tick - Basic Functions ⚠️ PARTIAL
**Status:** Generates C code with critical errors

**What Works:**
- Lexing and parsing succeed
- Name resolution completes
- Type checking completes (after fix)
- IR lowering completes
- Code generation completes

**Critical Bugs Found:**
1. **Function call arguments are missing** - All function calls generate with 0 arguments
   ```c
   // Generated (incorrect):
   t1 = __u_add();

   // Should be:
   t1 = __u_add(__u_a, __u_b);
   ```

2. **Incorrect alloca usage** - Wrong type for returned pointer
   ```c
   // Generated (incorrect):
   t2 = alloca(sizeof(int32_t));  // t2 is int32_t, not int32_t*
   *t2 = t1;  // ERROR: cannot dereference int32_t
   ```

**GCC Compilation Result:** FAIL
```
01_hello.c:20:10: error: too few arguments to function '__u_add'
01_hello.c:22:5: error: invalid type argument of unary '*' (have 'int32_t')
```

### Example 02: 02_types.tick - Type System ❌ FAIL
**Status:** Parsing fails silently

**Error:**
```
Parsing failed: returned NULL AST but no errors recorded.
```

**Likely Cause:** Parser doesn't support:
- Struct literals (e.g., `Point { x: x, y: y }`)
- Enum member access (e.g., `Status.OK`)
- Array types and indexing (e.g., `[10]u8`, `buffer[0]`)

### Example 03: 03_control_flow.tick - Control Structures ❌ FAIL
**Status:** IR lowering crashes

**Error:**
```
Assertion failed: stmt->kind == AST_LET_STMT
in lower_let_stmt at lower.c:665
```

**Root Cause:** Example uses `var` declarations which create `AST_VAR_STMT` nodes, but the lowering phase doesn't have a handler for this statement kind - it only handles `AST_LET_STMT`.

### Example 04: 04_errors.tick - Error Handling ❌ FAIL
**Status:** IR lowering crashes

**Error:**
```
Assertion failed: 0 && "Unhandled statement kind"
in lower_stmt at lower.c:1060
```

**Root Cause:** Uses advanced error handling constructs not yet implemented in lowering phase.

### Example 05: 05_resources.tick - Resource Management ❌ FAIL
**Status:** Parsing fails silently

**Error:**
```
Parsing failed: returned NULL AST but no errors recorded.
```

**Likely Cause:** Uses `defer` statements and advanced syntax not yet supported in parser.

### Example 06: 06_async_basic.tick - Basic Async/Await ❌ FAIL
**Status:** IR lowering crashes

**Error:**
```
Assertion failed: stmt->kind == AST_LET_STMT
in lower_let_stmt at lower.c:665
```

**Root Cause:** Same as example 03 - uses `var` declarations.

### Example 07: 07_async_io.tick - Async I/O Operations ❌ FAIL
**Status:** Parsing fails silently

**Error:**
```
Parsing failed: returned NULL AST but no errors recorded.
```

**Likely Cause:** Complex async syntax not yet supported.

### Example 08: 08_tcp_echo_server.tick - TCP Server ❌ FAIL
**Status:** Parsing fails silently

**Error:**
```
Parsing failed: returned NULL AST but no errors recorded.
```

**Likely Cause:** Advanced async/network syntax not yet supported.

## Bugs Fixed During Testing

### Fix #1: Identifier Type Resolution in Type Checker
**File:** `/home/user/tick/stream3-semantic/src/typeck.c`
**Lines:** 906-931

**Problem:** The type checker had a stub implementation that returned an error for all identifier expressions:
```c
case AST_IDENTIFIER_EXPR: {
    error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                  "Identifier resolution not implemented: %s", ...);
    return NULL;
}
```

**Solution:** Implemented proper symbol table lookup:
```c
case AST_IDENTIFIER_EXPR: {
    Symbol* sym = scope_lookup(tc->symbol_table->module_scope,
                               expr->data.identifier_expr.name);
    if (!sym) {
        error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                      "Undefined identifier: %s", ...);
        return NULL;
    }

    Type* sym_type = sym->type;
    if (!sym_type && sym->kind == SYMBOL_FUNCTION &&
        sym->data.function.ast_node) {
        sym_type = sym->data.function.ast_node->type;
    }

    expr->type = sym_type;
    return expr->type;
}
```

**Impact:** This fix was required to get past type checking. Without it, the compiler crashed with a segfault during IR lowering when trying to access `expr->type` on call expressions.

### Fix #2: NULL Check in IR Call Lowering
**File:** `/home/user/tick/stream5-lowering/src/lower.c`
**Line:** 294

**Problem:** Code was accessing `expr->type->kind` without checking if `expr->type` was NULL.

**Solution:** Added NULL check:
```c
if (expr->type && expr->type->kind != TYPE_VOID) {
    dest = ir_function_new_temp(ctx->current_function, expr->type, ctx->arena);
}
```

**Impact:** Defensive fix to prevent segfaults, though the underlying issue is that types should always be set by type checking.

## Critical Issues Requiring Fixes

### Priority 1: Identifier Resolution in IR Lowering (CRITICAL)
**File:** `/home/user/tick/stream5-lowering/src/lower.c`
**Function:** `lower_identifier_expr` (lines 183-193)

**Problem:** The IR lowering phase creates a NEW temporary for every identifier reference instead of looking up the actual parameter, variable, or global symbol. This is why function calls have 0 arguments - the argument identifiers create new temps instead of referencing the actual parameter values.

**Current (Broken) Code:**
```c
static IrValue* lower_identifier_expr(LowerContext* ctx, AstNode* expr) {
    const char* name = expr->data.identifier_expr.name;
    // BUG: Creates a new temp instead of looking up the identifier!
    IrValue* value = ir_function_new_temp(ctx->current_function, expr->type, ctx->arena);
    value->data.temp.name = name;
    return value;
}
```

**Required Fix:**
1. Add a hash map to `LowerContext` to track:
   - Function parameters (name → IR_VALUE_PARAM)
   - Local variables (name → allocated IR_VALUE_TEMP from alloca)
   - Global functions (name → IR_VALUE_GLOBAL)

2. When lowering function parameters, register them in this map

3. When lowering let/var statements, register the allocated variable in this map

4. In `lower_identifier_expr`, look up the identifier:
   ```c
   static IrValue* lower_identifier_expr(LowerContext* ctx, AstNode* expr) {
       const char* name = expr->data.identifier_expr.name;

       // Look up in local variables/parameters
       IrValue* value = lookup_in_context(ctx, name);
       if (value) return value;

       // Look up as global function
       Symbol* sym = scope_lookup(ctx->symbol_table, name);
       if (sym && sym->kind == SYMBOL_FUNCTION) {
           IrValue* global = ir_alloc_value(IR_VALUE_GLOBAL, expr->type, ctx->arena);
           global->data.global.name = name;
           return global;
       }

       // Error: undefined
       assert(0 && "Undefined identifier in lowering");
       return NULL;
   }
   ```

**Estimated Effort:** 4-6 hours

### Priority 2: Add Support for VAR Statements
**File:** `/home/user/tick/stream5-lowering/src/lower.c`
**Function:** `lower_stmt` (line ~1000-1070)

**Problem:** The lowering phase only handles `AST_LET_STMT` but not `AST_VAR_STMT`. Examples 03 and 06 use `var` declarations and crash.

**Required Fix:**
```c
case AST_VAR_STMT:
    lower_var_stmt(ctx, stmt);
    break;
```

And implement `lower_var_stmt` (similar to `lower_let_stmt` but marks variable as mutable).

**Estimated Effort:** 1-2 hours

### Priority 3: Fix Parser Silent Failures
**Files:** Multiple parser files
**Examples Affected:** 02, 05, 07, 08

**Problem:** Parser returns NULL without recording any errors. This makes debugging very difficult.

**Required Fix:**
1. Add error reporting for unimplemented grammar rules
2. Implement missing grammar rules for:
   - Struct literals
   - Enum member access
   - Array indexing
   - Advanced control flow (defer, try/catch, async/await)

**Estimated Effort:** 8-12 hours

### Priority 4: Fix Alloca Code Generation
**File:** Likely in codegen or lowering

**Problem:** Variables are being allocated with `alloca` but the generated code treats the integer temp as if it's a pointer.

**Investigation Needed:** Determine if this is a codegen bug or a lowering bug. The alloca instruction should return a pointer type, not the base type.

**Estimated Effort:** 2-3 hours

## Summary of Working Features

✅ **Lexer** - Tokenizes source code correctly
✅ **Parser** - Handles basic function definitions, simple expressions, basic control flow
✅ **Name Resolution** - Creates symbol table and resolves names (after fix #1)
✅ **Type Checking** - Assigns types to expressions (after fix #1)
✅ **IR Lowering** - Converts AST to IR for simple cases (with critical bugs)
✅ **Code Generation** - Emits C code (with critical bugs)

## Summary of Broken Features

❌ **Identifier resolution in IR** - Creates new temps instead of lookup (Priority 1)
❌ **Function call arguments** - All calls have 0 arguments due to above
❌ **Var statement handling** - Only let statements work
❌ **Struct literals** - Parser doesn't support
❌ **Array syntax** - Parser doesn't support
❌ **Enum member access** - Parser doesn't support
❌ **Defer statements** - Parser doesn't support
❌ **Try/catch** - Parser doesn't support
❌ **Async/await** - Parser doesn't support
❌ **Variable allocation** - Alloca generates wrong types

## Recommended Action Plan

### Phase 1: Make Basic Examples Work (1-2 days)
1. **Fix Priority 1** - Implement proper identifier resolution in IR lowering
2. **Fix Priority 4** - Fix alloca type generation
3. **Fix Priority 2** - Add VAR statement support
4. **Verify:** Examples 01 and 03 should produce valid C code

### Phase 2: Expand Parser Support (2-3 days)
1. **Add struct literal syntax**
2. **Add array type and indexing syntax**
3. **Add enum member access syntax**
4. **Verify:** Example 02 should work

### Phase 3: Advanced Features (1-2 weeks)
1. **Implement defer/errdefer**
2. **Implement try/catch error handling**
3. **Implement async/await**
4. **Verify:** Examples 04-08 should work

## Conclusion

The tick compiler has a solid foundation with working lexer, parser (for basic syntax), name resolution, and type checking phases. However, **critical bugs in the IR lowering phase make it unusable** for even the simplest programs.

The most urgent fix is **Priority 1: Identifier Resolution in IR Lowering**. Without this fix, no program can successfully compile to working C code. Once this is fixed, the compiler should be able to handle basic function calls and arithmetic, making it suitable for initial testing and development.

The parser also needs significant work to support the full tick language syntax, but this can be done incrementally as needed.
