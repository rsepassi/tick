# Remaining Work Items

This document tracks the remaining tasks to complete the full integration of the Tick compiler pipeline following the parallel reconciliation integration.

**Last Updated:** 2025-11-10
**Status:** Iteration 2 - Integration Phase

---

## Overview

All 7 streams have been updated to use the reconciled interfaces from `interfaces2/`. The next phase involves completing build infrastructure, fixing test suites, and running end-to-end integration tests.

---

## Critical Path Items

### 1. Stream 2 (Parser) - Lemon Parser Generator

**Status:** ✅ Complete (Grammar conflicts need resolution)
**Priority:** HIGH
**Progress:** Lemon working, parser files generated, but grammar has conflicts

**Completed Tasks:**
- [x] Obtain Lemon parser generator (from SQLite project via GitHub)
- [x] Build Lemon tool (compiled successfully, version 1.0)
- [x] Generate `grammar.c` from `grammar.y` (generated despite conflicts)
- [ ] Resolve 119 parsing conflicts in grammar.y
- [ ] Fix unused label warnings
- [ ] Verify parser compilation with generated grammar
- [ ] Run parser test suite (29 tests expected)

**Dependencies:** None - can proceed independently
**Estimated Effort:** 4-8 hours remaining (conflict resolution)

**Current Status:**
- ✅ Lemon added to `vendor/lemon/` (7,337 lines)
- ✅ Lemon executable built and working (version 1.0)
- ✅ Parser files generated: grammar.c (180KB), grammar.h (3.5KB)
- ⚠️ Grammar has 119 parsing conflicts (shift/reduce or reduce/reduce)
- ⚠️ Grammar has unused label warnings (non-critical)
- 📝 Detailed conflict report in `build/grammar.out` (518KB)

**Notes:**
- Lemon location: `vendor/lemon/`
- Source: https://github.com/sqlite/sqlite (master/tool/)
- Documentation: https://sqlite.org/lemon.html
- License: Public Domain
- Makefile updated to use vendor/lemon/lemon

---

### 2. Stream 3 (Semantic) - Test Suite Updates

**Status:** ⚠️ Partial
**Priority:** HIGH
**Component:** Implementation complete, tests need updates

**Tasks:**

#### test_resolver.c (15 compilation errors)
- [ ] Update `make_int_literal()`: `LIT_INT` → `LITERAL_INT`, `int_value` → `value.int_value`
- [ ] Update `make_block()`: `statements` → `stmts`
- [ ] Remove symbol field checks: `identifier_expr.symbol` doesn't exist in interfaces2
- [ ] Fix array comparison warning in `test_string_interning()`
- [ ] Update all AST construction helpers for reconciled field names

#### test_typeck.c (not yet updated)
- [ ] Update `make_typed_function()`: parameter and return type handling
- [ ] Update `make_struct_with_fields()`: `AstStructField` → `AstField`
- [ ] Update all literal creation helpers: `lit_kind` → `literal_kind`
- [ ] Update binary/unary operator enums: `BINOP_LAND` → `BINOP_LOGICAL_AND`, etc.
- [ ] Update expression field accesses throughout

**Dependencies:** None
**Estimated Effort:** 4-6 hours

**Current Errors:**
```
test_resolver.c:97: 'LIT_INT' undeclared; did you mean 'LITERAL_INT'?
test_resolver.c:98: no member named 'int_value' in literal_expr
test_resolver.c:105: no member named 'statements' in block_stmt
test_resolver.c:259: no member named 'symbol' in identifier_expr
```

---

### 3. Streams 4 & 5 - Build Infrastructure

**Status:** ⚠️ Missing
**Priority:** MEDIUM
**Issue:** No Makefiles present

**Tasks:**

#### Stream 4 (Coroutine)
- [ ] Create `stream4-coroutine/Makefile`
- [ ] Link against: arena, error, string_pool (stream7)
- [ ] Build `coro_analyze.o` and `test_coro_analyze`
- [ ] Verify compilation with reconciled interfaces
- [ ] Run test suite

#### Stream 5 (Lowering)
- [ ] Create `stream5-lowering/Makefile`
- [ ] Link against: arena, error, string_pool (stream7)
- [ ] Build `lower.o` and `test_lower`
- [ ] Verify compilation with reconciled interfaces
- [ ] Run test suite

**Dependencies:** None - can proceed independently
**Estimated Effort:** 2-3 hours per stream

**Template:** Use `stream1-lexer/Makefile` or `stream6-codegen/Makefile` as reference

---

## Integration Testing

### 4. Integration Test Suite Execution

**Status:** 📝 Ready (created but not run)
**Priority:** HIGH
**Location:** `integration/`

**Tasks:**
- [ ] Fix compilation of integration tests (may need mock implementations)
- [ ] Run `test_lexer_parser` - Lexer → Parser boundary
- [ ] Run `test_semantic` - Parser → Semantic boundary
- [ ] Run `test_coroutine` - Semantic → Coroutine boundary
- [ ] Run `test_lowering` - Coroutine → IR boundary
- [ ] Run `test_codegen` - IR → C code boundary
- [ ] Run `test_full_compile` - Complete pipeline + gcc verification
- [ ] Run `test_pipeline` - End-to-end comprehensive scenarios

**Dependencies:**
- Stream 2 (Parser) must be built
- Stream 3 tests must be fixed
- Streams 4 & 5 Makefiles must exist

**Estimated Effort:** 8-12 hours (including bug fixes)

**Expected Issues:**
- Interface mismatches between real implementations
- Mock functions may need updates
- Pipeline data flow issues
- Memory management bugs

---

## Secondary Items

### 5. Documentation Updates

**Status:** 📝 In Progress
**Priority:** LOW

**Tasks:**
- [ ] Update `doc/parallel-impl-plan.md` with Iteration 2 completion status
- [ ] Document interface reconciliation lessons learned
- [ ] Create integration testing guide
- [ ] Document build system conventions
- [ ] Add troubleshooting guide for common integration issues

**Estimated Effort:** 2-3 hours

---

### 6. Code Quality Improvements

**Status:** 🔄 Ongoing
**Priority:** LOW

**Tasks:**
- [ ] Fix compilation warnings in stream7-infrastructure (unused variables)
- [ ] Add missing function implementations (stubs currently)
- [ ] Improve error messages across all components
- [ ] Add input validation
- [ ] Memory leak checking with valgrind

**Estimated Effort:** 4-8 hours

---

### 7. Example Programs

**Status:** 📝 Not Started
**Priority:** LOW

**Tasks:**
- [ ] Create `examples/` directory
- [ ] Add simple function example
- [ ] Add async/await example
- [ ] Add error handling example (try/catch)
- [ ] Add defer/errdefer example
- [ ] Add struct and enum examples
- [ ] Add comprehensive language feature showcase

**Estimated Effort:** 4-6 hours

---

## Completion Criteria

The integration is considered complete when:

### Must Have
- ✅ All 7 streams use interfaces2/ (DONE)
- ✅ Integration test suite created (DONE)
- ⏳ All streams compile without errors
- ⏳ All unit tests pass (59+ currently passing)
- ⏳ Integration tests pass
- ⏳ Full pipeline generates valid C11 code

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

## Work Estimates

| Category | Items | Estimated Hours |
|----------|-------|-----------------|
| Critical Path | 3 items | 8-13 hours |
| Integration Testing | 1 item | 8-12 hours |
| Secondary Items | 3 items | 10-17 hours |
| **Total** | **7 items** | **26-42 hours** |

---

## Known Issues

### Build Environment
- Lemon parser generator not available
- Some streams missing Makefiles
- No CI/CD automation

### Code Issues
- Stream 3 tests incompatible with reconciled AST structure
- Some functions are stubs (implementation incomplete)
- Compiler warnings in infrastructure code

### Testing Gaps
- Integration tests not yet run
- No end-to-end validation
- No performance testing
- No memory leak checking

---

## Next Steps (Recommended Order)

1. **Immediate** (1-2 days)
   - Fix Stream 3 test suite (test_resolver.c, test_typeck.c)
   - Create Makefiles for Streams 4 & 5
   - Obtain/build Lemon parser generator

2. **Short-term** (3-5 days)
   - Compile all streams successfully
   - Run all unit tests
   - Begin integration test execution
   - Fix integration bugs as discovered

3. **Medium-term** (1-2 weeks)
   - Complete integration test suite
   - Generate valid C11 code end-to-end
   - Create example programs
   - Documentation updates

4. **Long-term** (2-4 weeks)
   - Performance optimization
   - Comprehensive error messages
   - Code quality improvements
   - CI/CD setup

---

## Questions / Decisions Needed

1. **Lemon Parser Generator**
   - Should we obtain Lemon or switch to Bison/Yacc?
   - Can we use a pre-built Lemon binary?

2. **Test Strategy**
   - Should we prioritize unit tests or integration tests?
   - What's the acceptable test coverage percentage?

3. **Stub Implementations**
   - Which stub functions are critical vs. nice-to-have?
   - Should we complete all stubs before integration testing?

4. **Error Handling**
   - What level of error message quality is required?
   - Should we implement error recovery in the parser?

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
