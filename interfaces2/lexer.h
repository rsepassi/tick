#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct StringPool StringPool;
typedef struct ErrorList ErrorList;

// Lexer Interface
// Purpose: Streaming tokenization for parser consumption

// Token codes must match the Lemon-generated parser token codes
// Order based on Lemon's precedence declarations and usage
typedef enum {
    TOKEN_COMMA = 1,
    TOKEN_EQ = 2,
    TOKEN_PLUS_EQ = 3,
    TOKEN_MINUS_EQ = 4,
    TOKEN_STAR_EQ = 5,
    TOKEN_SLASH_EQ = 6,
    TOKEN_PIPE_PIPE = 7,
    TOKEN_AMP_AMP = 8,
    TOKEN_PIPE = 9,
    TOKEN_CARET = 10,
    TOKEN_AMPERSAND = 11,
    TOKEN_EQ_EQ = 12,
    TOKEN_BANG_EQ = 13,
    TOKEN_LT = 14,
    TOKEN_GT = 15,
    TOKEN_LT_EQ = 16,
    TOKEN_GT_EQ = 17,
    TOKEN_LSHIFT = 18,
    TOKEN_RSHIFT = 19,
    TOKEN_PLUS = 20,
    TOKEN_MINUS = 21,
    TOKEN_STAR = 22,
    TOKEN_SLASH = 23,
    TOKEN_PERCENT = 24,
    TOKEN_BANG = 25,
    TOKEN_TILDE = 26,
    TOKEN_UNARY_MINUS = 27,
    TOKEN_UNARY_AMPERSAND = 28,
    TOKEN_UNARY_STAR = 29,
    TOKEN_DOT = 30,
    TOKEN_ARROW = 31,
    TOKEN_LBRACKET = 32,
    TOKEN_LPAREN = 33,
    TOKEN_LET = 34,
    TOKEN_IDENTIFIER = 35,
    TOKEN_IMPORT = 36,
    TOKEN_STRING_LITERAL = 37,
    TOKEN_SEMICOLON = 38,
    TOKEN_PUB = 39,
    TOKEN_COLON = 40,
    TOKEN_VAR = 41,
    TOKEN_VOLATILE = 42,
    TOKEN_FN = 43,
    TOKEN_RPAREN = 44,
    TOKEN_STRUCT = 45,
    TOKEN_LBRACE = 46,
    TOKEN_RBRACE = 47,
    TOKEN_PACKED = 48,
    TOKEN_ENUM = 49,
    TOKEN_UNION = 50,
    TOKEN_RETURN = 51,
    TOKEN_IF = 52,
    TOKEN_ELSE = 53,
    TOKEN_FOR = 54,
    TOKEN_IN = 55,
    TOKEN_SWITCH = 56,
    TOKEN_WHILE = 57,
    TOKEN_CASE = 58,
    TOKEN_DEFAULT = 59,
    TOKEN_BREAK = 60,
    TOKEN_CONTINUE = 61,
    TOKEN_DEFER = 62,
    TOKEN_ERRDEFER = 63,
    TOKEN_SUSPEND = 64,
    TOKEN_RESUME = 65,
    TOKEN_TRY = 66,
    TOKEN_CATCH = 67,
    TOKEN_ASYNC = 68,
    TOKEN_RBRACKET = 69,
    TOKEN_INT_LITERAL = 70,
    TOKEN_BOOL_LITERAL = 71,
    TOKEN_DOT_DOT = 72,

    // Special tokens - use values outside parser range but safe
    // These should never be sent to the Lemon parser
    TOKEN_LINE_COMMENT = 73,
    TOKEN_BLOCK_COMMENT = 74,
    TOKEN_EMBED_FILE = 75,

    TOKEN_EOF = 76,
    TOKEN_ERROR = 77
} TokenKind;

typedef struct Token {
    TokenKind kind;
    const char* start;      // Pointer into source text
    size_t length;
    uint32_t line;
    uint32_t column;

    // For literals
    union {
        uint64_t int_value;
        struct {
            const char* str_value;  // Pointer into string pool
            size_t str_length;
        };
    } literal;
} Token;

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

// Initialize lexer with caller-provided memory
void lexer_init(Lexer* lexer, const char* source, size_t len, const char* filename,
                StringPool* string_pool, ErrorList* error_list);

// Get next token (parser calls this repeatedly)
Token lexer_next_token(Lexer* lexer);

// Peek at current token without advancing
Token lexer_peek(Lexer* lexer);

#endif // LEXER_H
