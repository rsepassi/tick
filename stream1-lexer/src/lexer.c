#include "../include/lexer.h"
#include "../include/string_pool.h"
#include "../include/error.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

// Helper macros
#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')
#define IS_HEX_DIGIT(c) (IS_DIGIT(c) || ((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F'))
#define IS_OCTAL_DIGIT(c) ((c) >= '0' && (c) <= '7')
#define IS_BINARY_DIGIT(c) ((c) == '0' || (c) == '1')
#define IS_ALPHA(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z') || (c) == '_')
#define IS_ALNUM(c) (IS_ALPHA(c) || IS_DIGIT(c))

// Keyword lookup table
typedef struct {
    const char* keyword;
    TokenKind kind;
} KeywordEntry;

static const KeywordEntry keywords[] = {
    {"async", TOKEN_ASYNC},
    {"break", TOKEN_BREAK},
    {"case", TOKEN_CASE},
    {"catch", TOKEN_CATCH},
    {"continue", TOKEN_CONTINUE},
    {"default", TOKEN_DEFAULT},
    {"defer", TOKEN_DEFER},
    {"else", TOKEN_ELSE},
    {"embed_file", TOKEN_EMBED_FILE},
    {"enum", TOKEN_ENUM},
    {"errdefer", TOKEN_ERRDEFER},
    {"false", TOKEN_BOOL_LITERAL},
    {"fn", TOKEN_FN},
    {"for", TOKEN_FOR},
    {"if", TOKEN_IF},
    {"import", TOKEN_IMPORT},
    {"in", TOKEN_IN},
    {"let", TOKEN_LET},
    {"packed", TOKEN_PACKED},
    {"pub", TOKEN_PUB},
    {"resume", TOKEN_RESUME},
    {"return", TOKEN_RETURN},
    {"struct", TOKEN_STRUCT},
    {"suspend", TOKEN_SUSPEND},
    {"switch", TOKEN_SWITCH},
    {"true", TOKEN_BOOL_LITERAL},
    {"try", TOKEN_TRY},
    {"union", TOKEN_UNION},
    {"var", TOKEN_VAR},
    {"volatile", TOKEN_VOLATILE},
    {"while", TOKEN_WHILE},
};

static const size_t keyword_count = sizeof(keywords) / sizeof(keywords[0]);

// Forward declarations
static Token make_token(Lexer* lexer, TokenKind kind);
static Token error_token(Lexer* lexer, const char* message);
static bool is_at_end(Lexer* lexer);
static char advance(Lexer* lexer);
static char peek(Lexer* lexer);
static char peek_next(Lexer* lexer);
static bool match(Lexer* lexer, char expected);
static void skip_whitespace(Lexer* lexer);
static Token scan_token(Lexer* lexer);
static Token identifier(Lexer* lexer);
static Token number(Lexer* lexer);
static Token string_literal(Lexer* lexer);
static Token line_comment(Lexer* lexer);
static Token block_comment(Lexer* lexer);

void lexer_init(Lexer* lexer, const char* source, size_t len, const char* filename,
                StringPool* string_pool, ErrorList* error_list) {
    lexer->source = source;
    lexer->source_len = len;
    lexer->filename = filename;
    lexer->current = source;
    lexer->token_start = source;
    lexer->line = 1;
    lexer->column = 1;
    lexer->token_line = 1;
    lexer->token_column = 1;
    lexer->has_error = false;
    lexer->string_pool = string_pool;
    lexer->error_list = error_list;

    // Initialize current_token to invalid state
    lexer->current_token.kind = TOKEN_ERROR;
    lexer->current_token.start = NULL;
    lexer->current_token.length = 0;
    lexer->current_token.line = 0;
    lexer->current_token.column = 0;
}

Token lexer_next_token(Lexer* lexer) {
    skip_whitespace(lexer);

    lexer->token_start = lexer->current;
    lexer->token_line = lexer->line;
    lexer->token_column = lexer->column;

    if (is_at_end(lexer)) {
        return make_token(lexer, TOKEN_EOF);
    }

    Token token = scan_token(lexer);
    lexer->current_token = token;
    return token;
}

Token lexer_peek(Lexer* lexer) {
    return lexer->current_token;
}

static bool is_at_end(Lexer* lexer) {
    return lexer->current >= lexer->source + lexer->source_len;
}

static char advance(Lexer* lexer) {
    if (is_at_end(lexer)) return '\0';

    char c = *lexer->current++;
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return c;
}

static char peek(Lexer* lexer) {
    if (is_at_end(lexer)) return '\0';
    return *lexer->current;
}

static char peek_next(Lexer* lexer) {
    if (lexer->current + 1 >= lexer->source + lexer->source_len) return '\0';
    return lexer->current[1];
}

static bool match(Lexer* lexer, char expected) {
    if (is_at_end(lexer)) return false;
    if (*lexer->current != expected) return false;
    advance(lexer);
    return true;
}

static void skip_whitespace(Lexer* lexer) {
    while (!is_at_end(lexer)) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
            case '\n':
                advance(lexer);
                break;
            default:
                return;
        }
    }
}

static Token make_token(Lexer* lexer, TokenKind kind) {
    Token token;
    token.kind = kind;
    token.start = lexer->token_start;
    token.length = (size_t)(lexer->current - lexer->token_start);
    token.line = lexer->token_line;
    token.column = lexer->token_column;
    token.literal.int_value = 0;
    return token;
}

static Token error_token(Lexer* lexer, const char* message) {
    lexer->has_error = true;

    SourceLocation loc = {
        .line = lexer->token_line,
        .column = lexer->token_column,
        .filename = lexer->filename
    };

    if (lexer->error_list) {
        error_list_add(lexer->error_list, ERROR_LEXICAL, loc, "%s", message);
    }

    Token token = make_token(lexer, TOKEN_ERROR);
    return token;
}

static TokenKind check_keyword(const char* start, size_t length) {
    // Linear search through keywords (simple and reliable for small keyword count)
    for (size_t i = 0; i < keyword_count; i++) {
        const char* kw = keywords[i].keyword;
        size_t kw_len = strlen(kw);
        if (length == kw_len && memcmp(start, kw, length) == 0) {
            return keywords[i].kind;
        }
    }
    return TOKEN_IDENTIFIER;
}

static Token identifier(Lexer* lexer) {
    while (IS_ALNUM(peek(lexer))) {
        advance(lexer);
    }

    size_t length = (size_t)(lexer->current - lexer->token_start);
    TokenKind kind = check_keyword(lexer->token_start, length);

    Token token = make_token(lexer, kind);

    // For identifiers (not keywords), intern the string
    if (kind == TOKEN_IDENTIFIER && lexer->string_pool) {
        token.literal.str_value = string_pool_intern(lexer->string_pool,
                                                       lexer->token_start, length);
        token.literal.str_length = length;
    } else if (kind == TOKEN_BOOL_LITERAL) {
        // For bool literals, store the value
        token.literal.int_value = (lexer->token_start[0] == 't') ? 1 : 0;
    }

    return token;
}

static uint64_t parse_hex_number(const char* start, size_t length) {
    uint64_t value = 0;
    for (size_t i = 2; i < length; i++) {  // Skip '0x'
        char c = start[i];
        if (c == '_') continue;

        value *= 16;
        if (c >= '0' && c <= '9') {
            value += c - '0';
        } else if (c >= 'a' && c <= 'f') {
            value += c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            value += c - 'A' + 10;
        }
    }
    return value;
}

static uint64_t parse_octal_number(const char* start, size_t length) {
    uint64_t value = 0;
    for (size_t i = 2; i < length; i++) {  // Skip '0o'
        char c = start[i];
        if (c == '_') continue;
        value = value * 8 + (c - '0');
    }
    return value;
}

static uint64_t parse_binary_number(const char* start, size_t length) {
    uint64_t value = 0;
    for (size_t i = 2; i < length; i++) {  // Skip '0b'
        char c = start[i];
        if (c == '_') continue;
        value = value * 2 + (c - '0');
    }
    return value;
}

static uint64_t parse_decimal_number(const char* start, size_t length) {
    uint64_t value = 0;
    for (size_t i = 0; i < length; i++) {
        char c = start[i];
        if (c == '_') continue;
        value = value * 10 + (c - '0');
    }
    return value;
}

static Token number(Lexer* lexer) {
    // Check for hex, octal, or binary prefix
    // We've already consumed the first digit in scan_token()
    if (*lexer->token_start == '0' && !is_at_end(lexer)) {
        char next = peek(lexer);

        if (next == 'x' || next == 'X') {
            // Hexadecimal
            advance(lexer);  // 'x'

            if (!IS_HEX_DIGIT(peek(lexer))) {
                return error_token(lexer, "Expected hex digit after '0x'");
            }

            while (IS_HEX_DIGIT(peek(lexer)) || peek(lexer) == '_') {
                advance(lexer);
            }

            Token token = make_token(lexer, TOKEN_INT_LITERAL);
            token.literal.int_value = parse_hex_number(lexer->token_start, token.length);
            return token;
        } else if (next == 'o' || next == 'O') {
            // Octal
            advance(lexer);  // 'o'

            if (!IS_OCTAL_DIGIT(peek(lexer))) {
                return error_token(lexer, "Expected octal digit after '0o'");
            }

            while (IS_OCTAL_DIGIT(peek(lexer)) || peek(lexer) == '_') {
                advance(lexer);
            }

            Token token = make_token(lexer, TOKEN_INT_LITERAL);
            token.literal.int_value = parse_octal_number(lexer->token_start, token.length);
            return token;
        } else if (next == 'b' || next == 'B') {
            // Binary
            advance(lexer);  // 'b'

            if (!IS_BINARY_DIGIT(peek(lexer))) {
                return error_token(lexer, "Expected binary digit after '0b'");
            }

            while (IS_BINARY_DIGIT(peek(lexer)) || peek(lexer) == '_') {
                advance(lexer);
            }

            Token token = make_token(lexer, TOKEN_INT_LITERAL);
            token.literal.int_value = parse_binary_number(lexer->token_start, token.length);
            return token;
        }
    }

    // Decimal number
    while (IS_DIGIT(peek(lexer)) || peek(lexer) == '_') {
        advance(lexer);
    }

    Token token = make_token(lexer, TOKEN_INT_LITERAL);
    token.literal.int_value = parse_decimal_number(lexer->token_start, token.length);
    return token;
}

static char parse_escape_sequence(Lexer* lexer) {
    char c = advance(lexer);
    switch (c) {
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case '\\': return '\\';
        case '"': return '"';
        case '\'': return '\'';
        case '0': return '\0';
        default:
            return c;  // Unknown escape, just return the character
    }
}

static Token string_literal(Lexer* lexer) {
    // We need to build the string with escape sequences processed
    // For now, we'll use a simple approach and allocate from the string pool

    char buffer[4096];  // Temporary buffer for string processing
    size_t buf_pos = 0;

    while (!is_at_end(lexer) && peek(lexer) != '"') {
        if (peek(lexer) == '\n') {
            return error_token(lexer, "Unterminated string literal");
        }

        if (peek(lexer) == '\\') {
            advance(lexer);  // consume '\'
            char escaped = parse_escape_sequence(lexer);
            if (buf_pos < sizeof(buffer) - 1) {
                buffer[buf_pos++] = escaped;
            }
        } else {
            if (buf_pos < sizeof(buffer) - 1) {
                buffer[buf_pos++] = peek(lexer);
            }
            advance(lexer);
        }
    }

    if (is_at_end(lexer)) {
        return error_token(lexer, "Unterminated string literal");
    }

    // Closing quote
    advance(lexer);

    buffer[buf_pos] = '\0';

    Token token = make_token(lexer, TOKEN_STRING_LITERAL);

    // Intern the processed string
    if (lexer->string_pool) {
        token.literal.str_value = string_pool_intern(lexer->string_pool, buffer, buf_pos);
        token.literal.str_length = buf_pos;
    } else {
        token.literal.str_value = NULL;
        token.literal.str_length = buf_pos;
    }

    return token;
}

static Token line_comment(Lexer* lexer) {
    // Consume until end of line
    while (!is_at_end(lexer) && peek(lexer) != '\n') {
        advance(lexer);
    }

    return make_token(lexer, TOKEN_LINE_COMMENT);
}

static Token block_comment(Lexer* lexer) {
    int depth = 1;

    while (!is_at_end(lexer) && depth > 0) {
        if (peek(lexer) == '/' && peek_next(lexer) == '*') {
            advance(lexer);
            advance(lexer);
            depth++;
        } else if (peek(lexer) == '*' && peek_next(lexer) == '/') {
            advance(lexer);
            advance(lexer);
            depth--;
        } else {
            advance(lexer);
        }
    }

    if (depth > 0) {
        return error_token(lexer, "Unterminated block comment");
    }

    return make_token(lexer, TOKEN_BLOCK_COMMENT);
}

static Token scan_token(Lexer* lexer) {
    char c = advance(lexer);

    // Identifiers and keywords
    if (IS_ALPHA(c)) {
        return identifier(lexer);
    }

    // Numbers
    if (IS_DIGIT(c)) {
        return number(lexer);
    }

    switch (c) {
        // Single-character tokens
        case '(': return make_token(lexer, TOKEN_LPAREN);
        case ')': return make_token(lexer, TOKEN_RPAREN);
        case '{': return make_token(lexer, TOKEN_LBRACE);
        case '}': return make_token(lexer, TOKEN_RBRACE);
        case '[': return make_token(lexer, TOKEN_LBRACKET);
        case ']': return make_token(lexer, TOKEN_RBRACKET);
        case ',': return make_token(lexer, TOKEN_COMMA);
        case ';': return make_token(lexer, TOKEN_SEMICOLON);
        case ':': return make_token(lexer, TOKEN_COLON);
        case '~': return make_token(lexer, TOKEN_TILDE);
        case '%': return make_token(lexer, TOKEN_PERCENT);
        case '^': return make_token(lexer, TOKEN_CARET);

        // Operators that can be compound
        case '+':
            return make_token(lexer, match(lexer, '=') ? TOKEN_PLUS_EQ : TOKEN_PLUS);
        case '-':
            if (match(lexer, '=')) return make_token(lexer, TOKEN_MINUS_EQ);
            if (match(lexer, '>')) return make_token(lexer, TOKEN_ARROW);
            return make_token(lexer, TOKEN_MINUS);
        case '*':
            return make_token(lexer, match(lexer, '=') ? TOKEN_STAR_EQ : TOKEN_STAR);
        case '/':
            if (match(lexer, '=')) return make_token(lexer, TOKEN_SLASH_EQ);
            if (match(lexer, '/')) return line_comment(lexer);
            if (match(lexer, '*')) return block_comment(lexer);
            return make_token(lexer, TOKEN_SLASH);
        case '&':
            if (match(lexer, '&')) return make_token(lexer, TOKEN_AMP_AMP);
            return make_token(lexer, TOKEN_AMPERSAND);
        case '|':
            if (match(lexer, '|')) return make_token(lexer, TOKEN_PIPE_PIPE);
            return make_token(lexer, TOKEN_PIPE);
        case '!':
            return make_token(lexer, match(lexer, '=') ? TOKEN_BANG_EQ : TOKEN_BANG);
        case '=':
            return make_token(lexer, match(lexer, '=') ? TOKEN_EQ_EQ : TOKEN_EQ);
        case '<':
            if (match(lexer, '<')) return make_token(lexer, TOKEN_LSHIFT);
            return make_token(lexer, match(lexer, '=') ? TOKEN_LT_EQ : TOKEN_LT);
        case '>':
            if (match(lexer, '>')) return make_token(lexer, TOKEN_RSHIFT);
            return make_token(lexer, match(lexer, '=') ? TOKEN_GT_EQ : TOKEN_GT);
        case '.':
            return make_token(lexer, match(lexer, '.') ? TOKEN_DOT_DOT : TOKEN_DOT);

        // String literal
        case '"':
            return string_literal(lexer);

        default:
            return error_token(lexer, "Unexpected character");
    }
}
