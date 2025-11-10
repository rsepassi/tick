# Parse Failure Analysis for Examples 03, 04, 07, 08

**Date:** 2025-11-10
**Investigator:** Claude
**Compiler:** /home/user/tick/compiler/tickc

## Executive Summary

After systematic testing, I've identified the root causes of parse failures in examples 03, 04, 07, and 08. Two issues are grammar implementation bugs (fixable), one is a syntax documentation error (examples use wrong syntax), and one causes crashes in later compilation phases (requires codegen fixes).

## Detailed Findings

### Example 03: /home/user/tick/examples/03_control_flow.tick
**Status:** Parse Failure
**Root Cause:** Incomplete switch statement grammar implementation
**Failure Point:** Lines 43-52, 66-77 (switch statements)

#### Analysis
The file uses switch statements extensively, but the grammar implementation is incomplete:

1. **Grammar Rules Exist** - Switch statements are defined in grammar.y (lines 634-668)
2. **Implementation Incomplete** - The grammar actions don't properly build AST:
   - Line 655: `case_list(L) ::= case_list(Prev) case_clause(C). { L = C; }`
     - Bug: Throws away `Prev`, doesn't accumulate cases
   - Line 668: `case_value_list(L) ::= case_value_list(Prev) COMMA expr(E). { L = E; }`
     - Bug: Throws away `Prev`, doesn't accumulate values
   - Lines 657-665: `case_clause` actions don't use parameters V (values) or S (statements)

#### Test Confirmation
Created /home/user/tick/test_switch_simple.tick:
```
pub let main = fn() -> i32 {
    let x = 1;
    switch x {
        case 0:
            return 0;
        default:
            return 1;
    }
    return 2;
};
```
Result: "Parsing failed: returned NULL AST but no errors recorded."

Parser unit test also fails: `stream2-parser/test/test_parser.c:307` - switch_statement test

#### Fix Required
**Type:** Grammar Implementation (Medium effort)
**Location:** /home/user/tick/stream2-parser/grammar.y lines 654-668
**Actions:**
1. Fix `case_list` to accumulate cases using node_list_create/append pattern
2. Fix `case_value_list` to accumulate values similarly
3. Implement `case_clause` actions to properly store values and statements in AST nodes
4. Update AST building to properly construct AstSwitchCase structures

---

### Example 04: /home/user/tick/examples/04_errors.tick
**Status:** Segmentation Fault (after successful parsing)
**Root Cause:** Try/catch implementation incomplete in IR lowering or codegen
**Failure Point:** Lines 29-36, 67-80 (try/catch statements)

#### Analysis
Try/catch statements parse correctly but cause segfault during later compilation phases:

1. **Lexer:** Recognizes TOKEN_TRY, TOKEN_CATCH keywords ✓
2. **Grammar:** Rules defined and implemented correctly (grammar.y lines 714-730) ✓
3. **Parsing:** Successfully creates AST ✓
4. **Semantic Analysis:** Passes name resolution and type checking ✓
5. **IR Lowering or Codegen:** Crashes with segfault ✗

#### Test Confirmation
Created /home/user/tick/test_try_catch.tick:
```
let Error = enum(u8) {
    Failed = 0
};

let test = fn() -> Error!i32 {
    return Error.Failed;
};

pub let main = fn() -> i32 {
    try {
        let x = test();
    } catch |err| {
        return 1;
    }
    return 0;
};
```
Result: Segmentation fault (exit code 139) after parsing completes

#### Related Working Features
- Error return types (`Error!i32`) work correctly ✓
- Try expression for error propagation (`let x = try fn_call();`) works correctly ✓

#### Fix Required
**Type:** Codegen/IR Lowering Bug (Medium-High effort)
**Location:** Likely in /home/user/tick/stream6-codegen or IR lowering code
**Actions:**
1. Add null checks in codegen for try/catch AST nodes
2. Implement proper IR lowering for AST_TRY_CATCH_STMT
3. Generate correct control flow for exception handling
4. Add unit tests for try/catch codegen

---

### Example 07: /home/user/tick/examples/07_async_io.tick
**Status:** Parse Failure
**Root Cause:** Incorrect cast syntax in example code
**Failure Point:** Lines 59, 89, 115 (cast expressions using `as` keyword)

#### Analysis
The example uses `as` for type casts, but the language syntax is different:

**Example code (INCORRECT):**
```tick
let read_op = op as *AsyncRead;  // Line 59
let write_op = op as *AsyncWrite; // Line 89
let timer_op = op as *AsyncTimer; // Line 115
```

**Correct syntax:**
```tick
let read_op = op.(*AsyncRead);
let write_op = op.(*AsyncWrite);
let timer_op = op.(*AsyncTimer);
```

The grammar defines cast as `expr DOT LPAREN type RPAREN` (grammar.y line 1139), not `expr AS type`.

#### Test Confirmation
Wrong syntax: `let x = 42 as i32;` → Parse failure
Correct syntax: `let x = 42.(i32);` → Success ✓

#### Related Working Features
- Async call expressions (`async fn()`) work ✓
- Suspend statements work ✓
- Error return types work ✓
- Defer statements work ✓

#### Fix Required
**Type:** Documentation/Example Error (Low effort)
**Location:** /home/user/tick/examples/07_async_io.tick
**Actions:**
1. Replace all `as` casts with `.(type)` syntax
2. Update language documentation to clarify cast syntax
3. Consider adding `as` keyword to lexer/grammar if desired (architectural decision)

---

### Example 08: /home/user/tick/examples/08_tcp_echo_server.tick
**Status:** Parse Failure
**Root Cause:** Incorrect cast syntax in example code
**Failure Point:** Multiple lines using `as` keyword for casts

#### Analysis
Same issue as Example 07 - uses `as` for casts which isn't recognized:

**Lines with incorrect syntax:**
- Line 93: `return 0 as *Client;`
- Line 103: `return 0 as *Client;`
- Line 108: `let accept_op = op as *AsyncAccept;`
- Line 109: `let server = op.ctx as *Server;`
- Line 120: (close_op context casting)
- Line 137: `read_op.base.ctx = client as *void;`
- Line 151: `let read_op = op as *AsyncRead;`
- Line 152: `let client = op.ctx as *Client;`
- Line 160: `close_op.base.ctx = client as *void;`
- Line 174: `write_op.base.ctx = client as *void;`
- Line 184: `let write_op = op as *AsyncWrite;`
- Line 185: `let client = op.ctx as *Client;`
- Line 192: `close_op.base.ctx = client as *void;`
- Line 216: `read_op.base.ctx = client as *void;`

All should use `.(type)` syntax instead.

#### Test Confirmation
Simplified example with cast: Parse failure
Same code with correct syntax: Success ✓

#### Fix Required
**Type:** Documentation/Example Error (Low effort)
**Location:** /home/user/tick/examples/08_tcp_echo_server.tick
**Actions:**
1. Replace all `as *Type` with `.(*Type)` syntax
2. Verify no other syntax errors remain

---

## Summary Table

| Example | Issue | Type | Severity | Effort to Fix |
|---------|-------|------|----------|---------------|
| 03 | Switch statement grammar incomplete | Grammar Bug | High | Medium |
| 04 | Try/catch crashes in codegen | Codegen Bug | High | Medium-High |
| 07 | Wrong cast syntax (`as` instead of `.(type)`) | Example Error | Low | Low |
| 08 | Wrong cast syntax (`as` instead of `.(type)`) | Example Error | Low | Low |

## Related Issues Discovered

### Working Features (Not Causing Failures)
- ✓ Error return types (`Error!i32`, `Error!void`)
- ✓ Try expressions for error propagation (`try expr`)
- ✓ Async call expressions (`async fn()`)
- ✓ Suspend statements (`suspend;`)
- ✓ Resume statements (`resume coro;`)
- ✓ Defer statements (`defer stmt;` including `defer { ... }`)
- ✓ For loops with ranges (`for i in 0..10` - parses, codegen incomplete)
- ✓ Void return types

### Not Working Features
- ✗ Switch statements (grammar implementation incomplete)
- ✗ Try/catch statements (parse OK, segfault in codegen)
- ✗ Range expressions as values (`let x = 0..10;` - segfault)
- ✗ Cast with `as` keyword (not implemented, use `.(type)` instead)

## Recommended Fix Priority

### Priority 1: Enable Examples (Quick Wins)
1. **Fix Examples 07 & 08** - Replace `as` with `.(type)` syntax (15 minutes)
   - These are just syntax errors in examples, not compiler bugs
   - Will immediately make 2 more examples parse successfully

### Priority 2: Critical Grammar Bugs
2. **Fix Switch Statements** - Complete grammar implementation (2-4 hours)
   - Required for Example 03
   - High-value feature used in multiple examples
   - Clear fix path: implement list accumulation in grammar actions

### Priority 3: Codegen Features
3. **Fix Try/Catch Codegen** - Debug and implement (4-8 hours)
   - Required for Example 04
   - More complex: requires debugging segfault, implementing control flow
   - Parse already works, so lower priority than switch

### Priority 4: Nice to Have
4. **Range Expression Values** - Fix segfault (2-4 hours)
   - Not currently used in any examples
   - For loops with ranges work for iteration
   - Standalone range values are edge case

5. **Add `as` Keyword** - Architectural decision (4-6 hours if desired)
   - Current `.(type)` syntax works fine
   - Would require lexer, grammar, and documentation updates
   - More of a syntax sugar feature

## Testing Recommendations

### Created Test Files
Located in /home/user/tick/:
- `test_switch.tick`, `test_switch_simple.tick` - Demonstrate switch failure
- `test_try_catch.tick` - Demonstrate try/catch segfault
- `test_cast.tick` - Demonstrate `as` failure
- `test_cast_correct.tick` - Demonstrate correct `.(type)` syntax
- `test_range.tick` - Demonstrate range value segfault
- `test_for_range.tick` - Demonstrate range iteration (partial success)
- `test_error_type.tick` - Confirm error types work
- `test_async.tick` - Confirm async works
- `test_suspend.tick` - Confirm suspend works

### Parser Unit Tests
Run with: `cd /home/user/tick/stream2-parser && make test`

Current status:
- ✓ Most basic tests pass
- ✗ switch_statement test fails (line 307)
- ✗ while_switch_statement test would fail

## Conclusion

Of the 4 failing examples:
- **2 examples (07, 08)** - Just need syntax fixes in example code (15 min total)
- **1 example (03)** - Needs grammar bug fix (2-4 hours)
- **1 example (04)** - Needs codegen bug fix (4-8 hours)

The range expression issue mentioned in remaining-work.md is actually not blocking any examples - for loops with ranges work for parsing (codegen is incomplete but that's a separate issue). The real blockers are switch statements and incorrect cast syntax in examples.

**Immediate Action:** Fix the cast syntax in examples 07 and 08 to bring the success rate from 4/8 to 6/8 immediately.
