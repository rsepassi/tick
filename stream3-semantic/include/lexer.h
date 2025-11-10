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

typedef enum {
    // Literals
    TOKEN_INT_LITERAL,
    TOKEN_STRING_LITERAL,
    TOKEN_BOOL_LITERAL,

    // Identifiers
    TOKEN_IDENTIFIER,

    // Keywords
    TOKEN_ASYNC, TOKEN_SUSPEND, TOKEN_RESUME,
    TOKEN_TRY, TOKEN_CATCH, TOKEN_DEFER, TOKEN_ERRDEFER,
    TOKEN_PUB, TOKEN_IMPORT, TOKEN_LET, TOKEN_VAR,
    TOKEN_FN, TOKEN_RETURN, TOKEN_IF, TOKEN_ELSE,
    TOKEN_FOR, TOKEN_SWITCH, TOKEN_CASE, TOKEN_DEFAULT,
    TOKEN_BREAK, TOKEN_CONTINUE, TOKEN_PACKED, TOKEN_VOLATILE,
    TOKEN_EMBED_FILE, TOKEN_STRUCT, TOKEN_ENUM, TOKEN_UNION,
    TOKEN_IN, TOKEN_WHILE,

    // Operators
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
    TOKEN_AMPERSAND, TOKEN_PIPE, TOKEN_CARET, TOKEN_TILDE,
    TOKEN_LSHIFT, TOKEN_RSHIFT,
    TOKEN_AMP_AMP, TOKEN_PIPE_PIPE, TOKEN_BANG,
    TOKEN_LT, TOKEN_GT, TOKEN_LT_EQ, TOKEN_GT_EQ, TOKEN_EQ_EQ, TOKEN_BANG_EQ,
    TOKEN_EQ, TOKEN_PLUS_EQ, TOKEN_MINUS_EQ, TOKEN_STAR_EQ, TOKEN_SLASH_EQ,

    // Delimiters
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE,
    TOKEN_LBRACKET, TOKEN_RBRACKET,
    TOKEN_DOT, TOKEN_ARROW, TOKEN_COLON, TOKEN_SEMICOLON,
    TOKEN_COMMA, TOKEN_DOT_DOT,

    // Comments (preserved for formatter)
    TOKEN_LINE_COMMENT,
    TOKEN_BLOCK_COMMENT,

    TOKEN_EOF,
    TOKEN_ERROR
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
