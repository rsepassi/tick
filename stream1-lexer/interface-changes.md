# Interface Changes Documentation

This document describes all changes made to interfaces during the implementation of the Stream 1 lexer.

## Summary

The following interface files were modified:
1. `lexer.h` - Added dependencies, tracking fields, and comment token types

No changes were made to:
- `error.h` - Used as-is
- `string_pool.h` - Used as-is
- `arena.h` - Used as-is

## Detailed Changes

### 1. lexer.h - Lexer Interface

**File:** `include/lexer.h`

#### Change 1: Added Forward Declarations

**Location:** Lines 8-10

**Added:**
```c
// Forward declarations
typedef struct StringPool StringPool;
typedef struct ErrorList ErrorList;
```

**Rationale:**
- The lexer depends on StringPool for identifier interning
- The lexer depends on ErrorList for error reporting
- Forward declarations allow the Lexer struct to hold pointers without including the full headers
- Maintains clean separation between interfaces

#### Change 2: Added Comment Token Types

**Location:** Lines 48-50

**Added:**
```c
// Comments (preserved for formatter)
TOKEN_LINE_COMMENT,
TOKEN_BLOCK_COMMENT,
```

**Rationale:**
- The design doc mentions comments should be preserved for the formatter
- Line comments (`//`) and block comments (`/* */`) are distinct token types
- Allows parser/formatter to handle comments appropriately
- Enables nested block comment support

#### Change 3: Enhanced Lexer Struct with Tracking Fields

**Location:** Lines 74-86

**Before:**
```c
typedef struct Lexer {
    const char* source;
    size_t source_len;
    const char* filename;

    const char* current;
    uint32_t line;
    uint32_t column;

    Token current_token;
    bool has_error;
} Lexer;
```

**After:**
```c
typedef struct Lexer {
    const char* source;
    size_t source_len;
    const char* filename;

    const char* current;
    const char* token_start;  // Start of current token
    uint32_t line;
    uint32_t column;
    uint32_t token_line;      // Line where current token started
    uint32_t token_column;    // Column where current token started

    Token current_token;
    bool has_error;

    // Dependencies
    StringPool* string_pool;
    ErrorList* error_list;
} Lexer;
```

**Changes:**
1. **Added `token_start`** - Pointer to the start of the current token being scanned
2. **Added `token_line`** - Line number where the current token started
3. **Added `token_column`** - Column number where the current token started
4. **Added `string_pool`** - Pointer to string pool for identifier interning
5. **Added `error_list`** - Pointer to error list for error reporting

**Rationale:**

*Token tracking fields (`token_start`, `token_line`, `token_column`):*
- The lexer needs to know where each token started for proper Token construction
- `current`, `line`, `column` track the scan position, not the token start
- Multi-character tokens (keywords, numbers, strings) advance the scan position
- Token location must be captured at token start, not token end
- Enables accurate source location in error messages

*Dependency pointers (`string_pool`, `error_list`):*
- String pool is needed to intern identifier strings
- Error list is needed to report lexical errors
- Following "caller-provided memory" principle - these are injected dependencies
- No dynamic allocation in lexer - all resources provided by caller
- Clean dependency injection pattern

#### Change 4: Updated lexer_init Signature

**Location:** Lines 89-90

**Before:**
```c
void lexer_init(Lexer* lexer, const char* source, size_t len, const char* filename);
```

**After:**
```c
void lexer_init(Lexer* lexer, const char* source, size_t len, const char* filename,
                StringPool* string_pool, ErrorList* error_list);
```

**Changes:**
- Added `string_pool` parameter
- Added `error_list` parameter

**Rationale:**
- Dependencies must be provided during initialization
- Follows dependency injection pattern
- Caller controls allocation of all resources
- No hidden allocations or global state
- Makes dependencies explicit and testable

## Interface Design Principles Applied

### 1. Caller-Provided Memory
All interfaces follow the "init, not create" pattern:
- `Lexer` struct allocated by caller
- Dependencies (StringPool, ErrorList) provided by caller
- No malloc/free in lexer code
- Clean lifecycle management

### 2. No Struct Hiding
All struct fields are visible:
- Lexer struct is fully defined in header
- No opaque pointers
- Enables debugging and inspection
- Follows "transparent data structures" principle

### 3. Explicit Dependencies
Dependencies are explicit, not hidden:
- StringPool and ErrorList passed as parameters
- Forward declarations keep headers minimal
- No global state or singletons
- Testable and modular

### 4. Simple Over Complex
Design choices favor simplicity:
- Linear keyword search (not hash table or trie)
- Simple token struct (union for literal values)
- Streaming interface (not batch processing)
- Direct pointer arithmetic (not iterator abstraction)

## Breaking Changes

The following changes are **breaking changes** relative to the original `lexer.h`:

1. **lexer_init signature changed** - Added two new parameters
   - **Migration:** Callers must provide StringPool and ErrorList
   - **Impact:** All code using lexer_init must be updated

2. **Lexer struct layout changed** - Added new fields
   - **Migration:** Code directly accessing Lexer fields may need updates
   - **Impact:** Minimal - most code should use the API functions

3. **New token types added** - TOKEN_LINE_COMMENT, TOKEN_BLOCK_COMMENT
   - **Migration:** Parser/formatter must handle new token types
   - **Impact:** Must update switch statements handling TokenKind

## Non-Breaking Changes

The following changes are **non-breaking**:

1. **Forward declarations added** - No impact on existing code
2. **Internal tracking fields added** - Implementation detail, not API change
3. **Token struct unchanged** - Existing token handling code works as-is

## Testing Impact

All changes are fully tested:
- `test/test_lexer.c` - 33 comprehensive tests
- Tests cover all token types including comments
- Tests verify string interning via string pool
- Tests verify error reporting via error list
- Tests verify line/column tracking accuracy

## Future Compatibility

These interfaces are designed for future extension:

1. **Token type space** - Room for additional token types (floats, chars, etc.)
2. **Lexer fields** - Can add more tracking without breaking API
3. **Error categories** - ErrorList supports multiple error kinds
4. **String pool** - Can be upgraded to trie without changing interface

## Dependencies Graph

```
lexer.h
  ├─> string_pool.h (forward declaration)
  │     └─> arena.h (forward declaration)
  └─> error.h (forward declaration)
        └─> arena.h (forward declaration)
```

All dependencies use forward declarations in the interface, keeping compilation dependencies minimal.

## Recommendations for Other Streams

Based on this implementation, recommendations for other streams:

1. **Use dependency injection** - Pass dependencies as init parameters
2. **Add tracking fields as needed** - Don't hesitate to add state for correctness
3. **Preserve comments** - Keep TOKEN_LINE_COMMENT and TOKEN_BLOCK_COMMENT
4. **Use string pool** - Intern all identifiers for fast comparison
5. **Report errors properly** - Use ErrorList for all error reporting
6. **Test thoroughly** - Write tests for all token types and edge cases

## Summary of Changes

| Interface | Modified? | Changes | Breaking? |
|-----------|-----------|---------|-----------|
| lexer.h | ✅ Yes | Added dependencies, tracking fields, comment tokens | ✅ Yes (init signature) |
| error.h | ❌ No | Used as-is | ❌ No |
| string_pool.h | ❌ No | Used as-is | ❌ No |
| arena.h | ❌ No | Used as-is | ❌ No |

**Total Breaking Changes:** 1 (lexer_init signature)

**Mitigation:** All breaking changes are necessary and well-documented. Migration is straightforward - provide the two new dependencies.

## Validation

All interface changes have been validated through:
1. ✅ Successful compilation with -Wall -Wextra
2. ✅ All 33 unit tests passing
3. ✅ Integration tests with realistic code snippets
4. ✅ Error handling tests with invalid input
5. ✅ Manual review of interface consistency

The interfaces are ready for integration with Stream 2 (Parser) and other parallel streams.
