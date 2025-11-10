# Remaining Work Items

This document tracks the remaining tasks to complete the full integration of the Tick compiler pipeline following the interface standardization.

**Last Updated:** 2025-11-10 (Interface Standardization Complete)
**Status:** Iteration 2 - Integration Phase - Interfaces Stabilized

---

## Overview

✅ **MAJOR MILESTONE ACHIEVED**: All 7 streams now use `interfaces2/` as the single source of truth.

**What was completed:**
- ✅ Deleted `interfaces/` directory to eliminate ambiguity
- ✅ Removed 34 duplicate interface files from stream directories
- ✅ Created Makefiles for Streams 4 & 5
- ✅ Updated all Makefiles to reference `interfaces2/`
- ✅ Verified 5 streams compile successfully (1, 4, 5, 6, 7)

**Current Status:**
- `interfaces2/` is now the **stable linkage point** for parallel work
- Teams can work independently on their streams
- Integration testing can proceed once remaining compilation issues are fixed

---

## Stream 1: Lexer

**Status:** ✅ Compiling and all tests pass

**Completed:**
- [x] Remove duplicate interface files (4 files removed)
- [x] Update Makefile to use `interfaces2/`
- [x] Update source files to reference centralized headers
- [x] Verify compilation
- [x] Run tests: **33/33 passing**

---

## Stream 2: Parser

**Status:** ❌ Compilation blocked

**Tasks:**
- [ ] **Fix grammar.y:43 type safety bug**
  - Current: `M = ast_alloc(parser, AST_MODULE, D->loc);`
  - Problem: `decl_list(D)` is `void*`, cannot dereference `D->loc`
- [ ] Resolve 119 parsing conflicts in grammar.y
- [ ] Fix 22 unused label warnings
- [ ] Verify parser compiles with generated grammar.c
- [ ] Run parser test suite

**Completed:**
- [x] Lemon parser generator integrated at `vendor/lemon/`
- [x] Makefile updated to use `interfaces2/`
- [x] No duplicate interface files found

**Notes:**
- Grammar conflicts may be acceptable for initial testing
- Type safety bug must be fixed before compilation can succeed

---

## Stream 3: Semantic Analysis

**Status:** ❌ Test compilation blocked

**Tasks:**
- [ ] Update test files to use reconciled AST interface

**Required Changes (15 total):**

### Field Name Updates:
- [ ] `module.declarations` → `module.decls`
- [ ] `function_decl.return_type_node` → `function_decl.return_type`
- [ ] `function_decl.is_public` → `function_decl.is_pub`
- [ ] `struct_decl.is_public` → `struct_decl.is_pub`
- [ ] `let_stmt` → `let_decl`
- [ ] `let_decl.type_node` → `let_decl.type`
- [ ] `literal_expr.lit_kind` → `literal_expr.literal_kind`
- [ ] `literal_expr.int_value` → `literal_expr.value.int_value`
- [ ] `block_stmt.statements` → `block_stmt.stmts`

### Enum Updates:
- [ ] `LIT_INT` → `LITERAL_INT`
- [ ] `LIT_STRING` → `LITERAL_STRING`
- [ ] `LIT_BOOL` → `LITERAL_BOOL`

### Remove References to Deleted Fields:
- [ ] Remove `function_decl.is_async` (field no longer exists)
- [ ] Remove `struct_decl.alignment` (field no longer exists)
- [ ] Remove `identifier_expr.symbol` (field no longer exists, use `.name` only)

**Completed:**
- [x] Removed 10 duplicate interface files
- [x] Updated Makefile to use `interfaces2/`
- [x] Updated source files (resolver.c, typeck.c)
- [x] Added missing `#include <stdint.h>` to `interfaces2/type.h`

---

## Stream 4: Coroutine Analysis

**Status:** ✅ Compiling and all tests pass

**Completed:**
- [x] Created `Makefile`
- [x] Removed 5 duplicate interface files
- [x] Updated to use `interfaces2/`
- [x] Verify compilation
- [x] Run tests: **8/8 passing**

**Important Note on Stubs:**
- Created `src/stubs.c` with minimal stub implementations for `type_sizeof()`, `type_alignof()`, and `scope_init()`
- **These stubs MUST be replaced with full, correct implementations**
- Stubs currently allow independent compilation but are NOT production-ready
- Integration testing will require linking against full implementations from Stream 3

---

## Stream 5: IR Lowering

**Status:** ✅ Compiling (test has data initialization issue)

**Tasks:**
- [ ] Fix test data initialization
  - Problem: `ir_lower_function_with_coro_metadata` test creates `suspend_count=2` but doesn't allocate `suspend_points` array
  - Causes segfault at `src/lower.c:892` when accessing `meta->suspend_points[i]`

**Completed:**
- [x] Created `Makefile`
- [x] Removed 7 duplicate interface files
- [x] Updated to use `interfaces2/`
- [x] Verify compilation succeeds

---

## Stream 6: Code Generation

**Status:** ✅ Compiling and all tests pass

**Completed:**
- [x] Removed 5 duplicate interface files
- [x] Makefile already used `interfaces2/`
- [x] Updated source files to reference centralized headers
- [x] Verify compilation
- [x] Run tests: **6/6 passing**

---

## Stream 7: Core Infrastructure

**Status:** ✅ Compiling and all tests pass

**Tasks:**
- [ ] Fix unused variable warning in `src/arena.c:71` (variable `padding`)
- [ ] Fix unused function warning in `src/string_pool.c:13` (`hash_string`)

**Completed:**
- [x] Removed 3 duplicate interface files
- [x] Updated Makefile to use `interfaces2/`
- [x] Verify compilation
- [x] Run tests: **20/20 passing**

---

## Integration Testing

**Status:** Integration Makefile updated, awaiting Stream 2 & 3 fixes

**Tasks:**
- [ ] Update integration Makefile (DONE - now uses `interfaces2/`)
- [ ] Fix compilation of integration tests
- [ ] Run `test_lexer_parser` - Lexer → Parser boundary
- [ ] Run `test_semantic` - Parser → Semantic boundary
- [ ] Run `test_coroutine` - Semantic → Coroutine boundary
- [ ] Run `test_lowering` - Coroutine → IR boundary
- [ ] Run `test_codegen` - IR → C code boundary
- [ ] Run `test_full_compile` - Complete pipeline + gcc verification
- [ ] Run `test_pipeline` - End-to-end comprehensive scenarios

**Blockers:**
- Stream 2 (Parser) must compile
- Stream 3 tests must be fixed
- Stream 4 stubs must be replaced with full implementations

---

## Documentation

**Tasks:**
- [ ] Update `doc/parallel-impl-plan.md` with interface standardization completion
- [ ] Document interface reconciliation lessons learned
- [ ] Create integration testing guide
- [ ] Document build system conventions
- [ ] Add troubleshooting guide for common integration issues

---

## Code Quality

**Tasks:**
- [ ] Fix compilation warnings in stream7-infrastructure
- [ ] Replace all stub implementations with full, correct implementations
  - Stream 4: `type_sizeof()`, `type_alignof()`, `scope_init()`
- [ ] Improve error messages across all components
- [ ] Add input validation
- [ ] Memory leak checking with valgrind

**Important:** All stub implementations are temporary workarounds for independent compilation. They MUST be replaced with full, correct implementations before the project is considered complete.

---

## Example Programs

**Tasks:**
- [ ] Create `examples/` directory
- [ ] Add simple function example
- [ ] Add async/await example
- [ ] Add error handling example (try/catch)
- [ ] Add defer/errdefer example
- [ ] Add struct and enum examples
- [ ] Add comprehensive language feature showcase

---

## Completion Criteria

### Must Have
- ✅ All 7 streams use interfaces2/ (DONE)
- ✅ Integration test suite created (DONE)
- ✅ Makefiles exist for all streams (DONE)
- ⏳ All streams compile without errors
- ⏳ All unit tests pass
- ⏳ Integration tests pass
- ⏳ Full pipeline generates valid C11 code
- ⏳ All stub implementations replaced with full implementations

### Should Have
- ⏳ Generated C compiles with strict flags: `-Wall -Werror -Wextra -std=c11 -ffreestanding`
- ⏳ End-to-end examples compile and run
- ⏳ No memory leaks (valgrind clean)
- ⏳ Documentation complete

### Nice to Have
- ⏳ Comprehensive error messages
- ⏳ Multiple example programs
- ⏳ Performance benchmarks
- ⏳ CI/CD pipeline

---

## Resources

### Documentation
- `doc/parallel-impl-plan.md` - Overall implementation strategy
- `doc/impl-guide.md` - Language implementation details
- `doc/design.md` - Language design decisions
- `interfaces2/RECONCILIATION.md` - Interface reconciliation details
- `integration/README.md` - Integration testing guide

### Key Directories
- `interfaces2/` - Reconciled interfaces (authoritative)
- `integration/` - Integration test suite
- `stream*/` - Individual component implementations

### External Resources
- Lemon Parser: https://sqlite.org/src/doc/trunk/doc/lemon.html
- C11 Standard: ISO/IEC 9899:2011
- Freestanding C: https://en.cppreference.com/w/c/language/conformance

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-11-10 | 1.0 | Initial document after reconciliation integration |
| 2025-11-10 | 2.0 | Interface standardization complete, updated with task focus |
