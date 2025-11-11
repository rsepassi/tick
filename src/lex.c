#include "tick.h"

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
  tick_tok_type_t type;
} keyword_entry_t;

static const keyword_entry_t keywords[] = {
  {"and", TICK_TOK_AND},
  {"async", TICK_TOK_ASYNC},
  {"bool", TICK_TOK_BOOL},
  {"break", TICK_TOK_BREAK},
  {"case", TICK_TOK_CASE},
  {"catch", TICK_TOK_CATCH},
  {"continue", TICK_TOK_CONTINUE},
  {"default", TICK_TOK_DEFAULT},
  {"defer", TICK_TOK_DEFER},
  {"else", TICK_TOK_ELSE},
  {"embed_file", TICK_TOK_EMBED_FILE},
  {"enum", TICK_TOK_ENUM},
  {"errdefer", TICK_TOK_ERRDEFER},
  {"export", TICK_TOK_EXPORT},
  {"false", TICK_TOK_BOOL_LITERAL},
  {"fn", TICK_TOK_FN},
  {"for", TICK_TOK_FOR},
  {"if", TICK_TOK_IF},
  {"i16", TICK_TOK_I16},
  {"i32", TICK_TOK_I32},
  {"i64", TICK_TOK_I64},
  {"i8", TICK_TOK_I8},
  {"import", TICK_TOK_IMPORT},
  {"isz", TICK_TOK_ISZ},
  {"in", TICK_TOK_IN},
  {"let", TICK_TOK_LET},
  {"or", TICK_TOK_OR},
  {"packed", TICK_TOK_PACKED},
  {"pub", TICK_TOK_PUB},
  {"resume", TICK_TOK_RESUME},
  {"return", TICK_TOK_RETURN},
  {"struct", TICK_TOK_STRUCT},
  {"suspend", TICK_TOK_SUSPEND},
  {"switch", TICK_TOK_SWITCH},
  {"true", TICK_TOK_BOOL_LITERAL},
  {"try", TICK_TOK_TRY},
  {"u16", TICK_TOK_U16},
  {"u32", TICK_TOK_U32},
  {"u64", TICK_TOK_U64},
  {"u8", TICK_TOK_U8},
  {"union", TICK_TOK_UNION},
  {"usz", TICK_TOK_USZ},
  {"var", TICK_TOK_VAR},
  {"void", TICK_TOK_VOID},
  {"volatile", TICK_TOK_VOLATILE},
  {"while", TICK_TOK_WHILE},
};

static const usz keyword_count = sizeof(keywords) / sizeof(keywords[0]);

// Forward declarations
static bool is_at_end(tick_lex_t* lex);
static char advance(tick_lex_t* lex);
static char peek(tick_lex_t* lex);
static bool match(tick_lex_t* lex, char expected);
static void skip_whitespace(tick_lex_t* lex);
static void scan_token(tick_lex_t* lex, tick_tok_t* tok, usz token_start, usz token_line, usz token_col);
static void make_token(tick_lex_t* lex, tick_tok_t* tok, tick_tok_type_t type, usz start);
static void error_token(tick_lex_t* lex, tick_tok_t* tok, const char* message, usz token_line, usz token_col);
static void scan_identifier(tick_lex_t* lex, tick_tok_t* tok, usz start);
static void scan_number(tick_lex_t* lex, tick_tok_t* tok, usz start, bool is_negative);
static void scan_string(tick_lex_t* lex, tick_tok_t* tok, usz start, usz token_line, usz token_col);
static void scan_comment(tick_lex_t* lex, tick_tok_t* tok, usz start);
static tick_tok_type_t check_keyword(const u8* start, usz length);

void tick_lex_init(tick_lex_t* lex, tick_buf_t input, tick_alloc_t alloc, tick_buf_t errbuf) {
  lex->input = input;
  lex->alloc = alloc;
  lex->errbuf = errbuf;
  lex->pos = 0;
  lex->line = 1;
  lex->col = 1;
}

void tick_lex_next(tick_lex_t* lex, tick_tok_t* tok) {
  skip_whitespace(lex);

  // Save token start position
  usz token_start = lex->pos;
  usz token_line = lex->line;
  usz token_col = lex->col;

  if (is_at_end(lex)) {
    tok->type = TICK_TOK_EOF;
    tok->text.buf = lex->input.buf + lex->pos;
    tok->text.sz = 0;
    tok->line = token_line;
    tok->col = token_col;
    return;
  }

  scan_token(lex, tok, token_start, token_line, token_col);
}

static bool is_at_end(tick_lex_t* lex) {
  return lex->pos >= lex->input.sz;
}

static char advance(tick_lex_t* lex) {
  if (is_at_end(lex)) return '\0';

  char c = lex->input.buf[lex->pos++];
  if (c == '\n') {
    lex->line++;
    lex->col = 1;
  } else {
    lex->col++;
  }
  return c;
}

static char peek(tick_lex_t* lex) {
  if (is_at_end(lex)) return '\0';
  return lex->input.buf[lex->pos];
}

static bool match(tick_lex_t* lex, char expected) {
  if (is_at_end(lex)) return false;
  if (lex->input.buf[lex->pos] != expected) return false;
  advance(lex);
  return true;
}

static void skip_whitespace(tick_lex_t* lex) {
  while (!is_at_end(lex)) {
    char c = peek(lex);
    switch (c) {
      case ' ':
      case '\r':
      case '\t':
      case '\n':
        advance(lex);
        break;
      default:
        return;
    }
  }
}

static void make_token(tick_lex_t* lex, tick_tok_t* tok, tick_tok_type_t type, usz start) {
  tok->type = type;
  tok->text.buf = lex->input.buf + start;
  tok->text.sz = lex->pos - start;
  tok->literal.u64 = 0;  // Initialize to zero
}

static void error_token(tick_lex_t* lex, tick_tok_t* tok, const char* message, usz token_line, usz token_col) {
  tok->type = TICK_TOK_ERR;
  tok->text.buf = NULL;
  tok->text.sz = 0;
  tok->line = token_line;
  tok->col = token_col;

  // Write error message to errbuf if there's space
  if (lex->errbuf.buf && lex->errbuf.sz > 0) {
    // Format: "line:col: error message\n"
    snprintf((char*)lex->errbuf.buf, lex->errbuf.sz,
             "%zu:%zu: %s\n", token_line, token_col, message);
  }
}

static uint64_t parse_hex_number(const u8* start, usz length, bool is_negative) {
  uint64_t value = 0;
  usz offset = is_negative ? 3 : 2;  // Skip '-0x' or '0x'
  for (usz i = offset; i < length; i++) {
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

static uint64_t parse_octal_number(const u8* start, usz length, bool is_negative) {
  uint64_t value = 0;
  usz offset = is_negative ? 3 : 2;  // Skip '-0o' or '0o'
  for (usz i = offset; i < length; i++) {
    char c = start[i];
    if (c == '_') continue;
    value = value * 8 + (c - '0');
  }
  return value;
}

static uint64_t parse_binary_number(const u8* start, usz length, bool is_negative) {
  uint64_t value = 0;
  usz offset = is_negative ? 3 : 2;  // Skip '-0b' or '0b'
  for (usz i = offset; i < length; i++) {
    char c = start[i];
    if (c == '_') continue;
    value = value * 2 + (c - '0');
  }
  return value;
}

static uint64_t parse_decimal_number(const u8* start, usz length, bool is_negative) {
  uint64_t value = 0;
  usz offset = is_negative ? 1 : 0;  // Skip '-' if negative
  for (usz i = offset; i < length; i++) {
    char c = start[i];
    if (c == '_') continue;
    value = value * 10 + (c - '0');
  }
  return value;
}

static tick_tok_type_t check_keyword(const u8* start, usz length) {
  // Linear search through keywords
  for (usz i = 0; i < keyword_count; i++) {
    const char* kw = keywords[i].keyword;
    usz kw_len = strlen(kw);
    if (length == kw_len && memcmp(start, kw, length) == 0) {
      return keywords[i].type;
    }
  }
  return TICK_TOK_IDENT;
}

static void scan_identifier(tick_lex_t* lex, tick_tok_t* tok, usz start) {
  while (IS_ALNUM(peek(lex))) {
    advance(lex);
  }

  usz length = lex->pos - start;
  tick_tok_type_t type = check_keyword(lex->input.buf + start, length);

  make_token(lex, tok, type, start);

  // For identifiers (not keywords), allocate a copy
  if (type == TICK_TOK_IDENT) {
    tick_buf_t ident_buf = {0};
    tick_allocator_config_t config = {0};

    if (lex->alloc.realloc(lex->alloc.ctx, &ident_buf, length + 1, &config) == TICK_OK) {
      memcpy(ident_buf.buf, lex->input.buf + start, length);
      ident_buf.buf[length] = '\0';
      tok->text.buf = ident_buf.buf;
      tok->text.sz = length;
    }
  } else if (type == TICK_TOK_BOOL_LITERAL) {
    // Store boolean value: true=1, false=0
    tok->literal.u64 = (lex->input.buf[start] == 't') ? 1 : 0;
  }
}

static void scan_number(tick_lex_t* lex, tick_tok_t* tok, usz start, bool is_negative) {
  // Check for hex, octal, or binary prefix
  // Note: when is_negative, lex->pos points to the first digit (not yet consumed)
  // when !is_negative, lex->pos points past the first digit (already consumed)
  char first_char = is_negative ? peek(lex) : lex->input.buf[start];

  if (first_char == '0') {
    // Advance past '0' if we haven't already
    if (is_negative) advance(lex);

    if (!is_at_end(lex)) {
      char next = peek(lex);

      if (next == 'x' || next == 'X') {
      // Hexadecimal
      advance(lex);  // 'x'

      if (!IS_HEX_DIGIT(peek(lex))) {
        error_token(lex, tok, "Expected hex digit after '0x'", tok->line, tok->col);
        return;
      }

      while (IS_HEX_DIGIT(peek(lex)) || peek(lex) == '_') {
        advance(lex);
      }

      uint64_t value = parse_hex_number(lex->input.buf + start, lex->pos - start, is_negative);
      make_token(lex, tok, is_negative ? TICK_TOK_INT_LITERAL : TICK_TOK_UINT_LITERAL, start);
      if (is_negative) {
        tok->literal.i64 = -(int64_t)value;
      } else {
        tok->literal.u64 = value;
      }
      return;
    } else if (next == 'o' || next == 'O') {
      // Octal
      advance(lex);  // 'o'

      if (!IS_OCTAL_DIGIT(peek(lex))) {
        error_token(lex, tok, "Expected octal digit after '0o'", tok->line, tok->col);
        return;
      }

      while (IS_OCTAL_DIGIT(peek(lex)) || peek(lex) == '_') {
        advance(lex);
      }

      uint64_t value = parse_octal_number(lex->input.buf + start, lex->pos - start, is_negative);
      make_token(lex, tok, is_negative ? TICK_TOK_INT_LITERAL : TICK_TOK_UINT_LITERAL, start);
      if (is_negative) {
        tok->literal.i64 = -(int64_t)value;
      } else {
        tok->literal.u64 = value;
      }
      return;
    } else if (next == 'b' || next == 'B') {
      // Binary
      advance(lex);  // 'b'

      if (!IS_BINARY_DIGIT(peek(lex))) {
        error_token(lex, tok, "Expected binary digit after '0b'", tok->line, tok->col);
        return;
      }

      while (IS_BINARY_DIGIT(peek(lex)) || peek(lex) == '_') {
        advance(lex);
      }

      uint64_t value = parse_binary_number(lex->input.buf + start, lex->pos - start, is_negative);
      make_token(lex, tok, is_negative ? TICK_TOK_INT_LITERAL : TICK_TOK_UINT_LITERAL, start);
      if (is_negative) {
        tok->literal.i64 = -(int64_t)value;
      } else {
        tok->literal.u64 = value;
      }
      return;
    }
    }
  }

  // Decimal number
  // If is_negative, we haven't consumed any digits yet, so consume them now
  // If !is_negative, we've already consumed the first digit
  if (is_negative) {
    // Consume the first digit (which we peeked at earlier)
    if (IS_DIGIT(peek(lex))) {
      advance(lex);
    }
  }

  while (IS_DIGIT(peek(lex)) || peek(lex) == '_') {
    advance(lex);
  }

  uint64_t value = parse_decimal_number(lex->input.buf + start, lex->pos - start, is_negative);
  make_token(lex, tok, is_negative ? TICK_TOK_INT_LITERAL : TICK_TOK_UINT_LITERAL, start);
  if (is_negative) {
    tok->literal.i64 = -(int64_t)value;
  } else {
    tok->literal.u64 = value;
  }
}

static char parse_escape_sequence(tick_lex_t* lex) {
  char c = advance(lex);
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

static void scan_string(tick_lex_t* lex, tick_tok_t* tok, usz start, usz token_line, usz token_col) {
  UNUSED(start);
  // Build the string with escape sequences processed
  tick_buf_t str_buf = {0};
  usz capacity = 64;
  usz length = 0;

  tick_allocator_config_t config = {0};
  if (lex->alloc.realloc(lex->alloc.ctx, &str_buf, capacity, &config) != TICK_OK) {
    error_token(lex, tok, "Failed to allocate string buffer", token_line, token_col);
    return;
  }

  while (!is_at_end(lex) && peek(lex) != '"') {
    if (peek(lex) == '\n') {
      error_token(lex, tok, "Unterminated string literal", token_line, token_col);
      return;
    }

    char c;
    if (peek(lex) == '\\') {
      advance(lex);  // consume '\'
      c = parse_escape_sequence(lex);
    } else {
      c = advance(lex);
    }

    // Grow buffer if needed
    if (length >= capacity) {
      capacity *= 2;
      if (lex->alloc.realloc(lex->alloc.ctx, &str_buf, capacity, &config) != TICK_OK) {
        error_token(lex, tok, "Failed to grow string buffer", token_line, token_col);
        return;
      }
    }

    str_buf.buf[length++] = c;
  }

  if (is_at_end(lex)) {
    error_token(lex, tok, "Unterminated string literal", token_line, token_col);
    return;
  }

  // Closing quote
  advance(lex);

  // Null-terminate and shrink to fit
  if (lex->alloc.realloc(lex->alloc.ctx, &str_buf, length + 1, &config) == TICK_OK) {
    str_buf.buf[length] = '\0';
  }

  tok->type = TICK_TOK_STRING_LITERAL;
  tok->text.buf = str_buf.buf;
  tok->text.sz = length;
}

static void scan_comment(tick_lex_t* lex, tick_tok_t* tok, usz start) {
  // Consume until end of line
  while (!is_at_end(lex) && peek(lex) != '\n') {
    advance(lex);
  }

  make_token(lex, tok, TICK_TOK_COMMENT, start);
}

static const char* tok_type_name(tick_tok_type_t type) {
  switch (type) {
    case TICK_TOK_EOF: return "EOF";
    case TICK_TOK_ERR: return "ERR";
    case TICK_TOK_IDENT: return "IDENT";
    case TICK_TOK_UINT_LITERAL: return "UINT_LITERAL";
    case TICK_TOK_INT_LITERAL: return "INT_LITERAL";
    case TICK_TOK_STRING_LITERAL: return "STRING_LITERAL";
    case TICK_TOK_BOOL_LITERAL: return "BOOL_LITERAL";
    case TICK_TOK_BOOL: return "bool";
    case TICK_TOK_I8: return "i8";
    case TICK_TOK_I16: return "i16";
    case TICK_TOK_I32: return "i32";
    case TICK_TOK_I64: return "i64";
    case TICK_TOK_ISZ: return "isz";
    case TICK_TOK_U8: return "u8";
    case TICK_TOK_U16: return "u16";
    case TICK_TOK_U32: return "u32";
    case TICK_TOK_U64: return "u64";
    case TICK_TOK_USZ: return "usz";
    case TICK_TOK_VOID: return "void";
    case TICK_TOK_AND: return "and";
    case TICK_TOK_ASYNC: return "async";
    case TICK_TOK_BREAK: return "break";
    case TICK_TOK_CASE: return "case";
    case TICK_TOK_CATCH: return "catch";
    case TICK_TOK_CONTINUE: return "continue";
    case TICK_TOK_DEFAULT: return "default";
    case TICK_TOK_DEFER: return "defer";
    case TICK_TOK_ELSE: return "else";
    case TICK_TOK_EMBED_FILE: return "embed_file";
    case TICK_TOK_ENUM: return "enum";
    case TICK_TOK_ERRDEFER: return "errdefer";
    case TICK_TOK_EXPORT: return "export";
    case TICK_TOK_FN: return "fn";
    case TICK_TOK_FOR: return "for";
    case TICK_TOK_IF: return "if";
    case TICK_TOK_IMPORT: return "import";
    case TICK_TOK_IN: return "in";
    case TICK_TOK_LET: return "let";
    case TICK_TOK_OR: return "or";
    case TICK_TOK_PACKED: return "packed";
    case TICK_TOK_PUB: return "pub";
    case TICK_TOK_RESUME: return "resume";
    case TICK_TOK_RETURN: return "return";
    case TICK_TOK_STRUCT: return "struct";
    case TICK_TOK_SUSPEND: return "suspend";
    case TICK_TOK_SWITCH: return "switch";
    case TICK_TOK_TRY: return "try";
    case TICK_TOK_UNION: return "union";
    case TICK_TOK_VAR: return "var";
    case TICK_TOK_VOLATILE: return "volatile";
    case TICK_TOK_WHILE: return "while";
    case TICK_TOK_LPAREN: return "(";
    case TICK_TOK_RPAREN: return ")";
    case TICK_TOK_LBRACE: return "{";
    case TICK_TOK_RBRACE: return "}";
    case TICK_TOK_LBRACKET: return "[";
    case TICK_TOK_RBRACKET: return "]";
    case TICK_TOK_COMMA: return ",";
    case TICK_TOK_SEMICOLON: return ";";
    case TICK_TOK_COLON: return ":";
    case TICK_TOK_DOT: return ".";
    case TICK_TOK_DOT_DOT: return "..";
    case TICK_TOK_QUESTION: return "?";
    case TICK_TOK_PLUS: return "+";
    case TICK_TOK_MINUS: return "-";
    case TICK_TOK_STAR: return "*";
    case TICK_TOK_SLASH: return "/";
    case TICK_TOK_PERCENT: return "%";
    case TICK_TOK_AMPERSAND: return "&";
    case TICK_TOK_PIPE: return "|";
    case TICK_TOK_CARET: return "^";
    case TICK_TOK_TILDE: return "~";
    case TICK_TOK_BANG: return "!";
    case TICK_TOK_EQ: return "=";
    case TICK_TOK_LT: return "<";
    case TICK_TOK_GT: return ">";
    case TICK_TOK_PLUS_EQ: return "+=";
    case TICK_TOK_MINUS_EQ: return "-=";
    case TICK_TOK_STAR_EQ: return "*=";
    case TICK_TOK_SLASH_EQ: return "/=";
    case TICK_TOK_PLUS_PIPE: return "+|";
    case TICK_TOK_MINUS_PIPE: return "-|";
    case TICK_TOK_STAR_PIPE: return "*|";
    case TICK_TOK_SLASH_PIPE: return "/|";
    case TICK_TOK_PLUS_PERCENT: return "+%";
    case TICK_TOK_MINUS_PERCENT: return "-%";
    case TICK_TOK_STAR_PERCENT: return "*%";
    case TICK_TOK_SLASH_PERCENT: return "/%";
    case TICK_TOK_BANG_EQ: return "!=";
    case TICK_TOK_EQ_EQ: return "==";
    case TICK_TOK_LT_EQ: return "<=";
    case TICK_TOK_GT_EQ: return ">=";
    case TICK_TOK_LSHIFT: return "<<";
    case TICK_TOK_RSHIFT: return ">>";
    case TICK_TOK_COMMENT: return "COMMENT";
    default: return "UNKNOWN";
  }
}

const char* tick_tok_format(tick_tok_t* tok, char* buf, usz buf_sz) {
  const char* type_name = tok_type_name(tok->type);

  switch (tok->type) {
    case TICK_TOK_IDENT:
      snprintf(buf, buf_sz, "%zu:%zu %s '%.*s'",
               tok->line, tok->col, type_name,
               (int)tok->text.sz, tok->text.buf);
      break;
    case TICK_TOK_UINT_LITERAL:
      snprintf(buf, buf_sz, "%zu:%zu %s %llu",
               tok->line, tok->col, type_name, (unsigned long long)tok->literal.u64);
      break;
    case TICK_TOK_INT_LITERAL:
      snprintf(buf, buf_sz, "%zu:%zu %s %lld",
               tok->line, tok->col, type_name, (long long)tok->literal.i64);
      break;
    case TICK_TOK_STRING_LITERAL:
      snprintf(buf, buf_sz, "%zu:%zu %s \"%.*s\"",
               tok->line, tok->col, type_name,
               (int)tok->text.sz, tok->text.buf);
      break;
    case TICK_TOK_BOOL_LITERAL:
      snprintf(buf, buf_sz, "%zu:%zu %s %s",
               tok->line, tok->col, type_name,
               tok->literal.u64 ? "true" : "false");
      break;
    case TICK_TOK_COMMENT:
      snprintf(buf, buf_sz, "%zu:%zu %s '%.*s'",
               tok->line, tok->col, type_name,
               (int)tok->text.sz, tok->text.buf);
      break;
    case TICK_TOK_EOF:
      snprintf(buf, buf_sz, "%zu:%zu %s",
               tok->line, tok->col, type_name);
      break;
    case TICK_TOK_ERR:
      snprintf(buf, buf_sz, "%zu:%zu %s",
               tok->line, tok->col, type_name);
      break;
    default:
      // For keywords and operators, just show the type name
      snprintf(buf, buf_sz, "%zu:%zu %s",
               tok->line, tok->col, type_name);
      break;
  }

  return buf;
}

static void scan_token(tick_lex_t* lex, tick_tok_t* tok, usz token_start, usz token_line, usz token_col) {
  UNUSED(token_start);
  usz start = lex->pos;
  char c = advance(lex);

  tok->line = token_line;
  tok->col = token_col;

  // Identifiers and keywords
  if (IS_ALPHA(c)) {
    scan_identifier(lex, tok, start);
    return;
  }

  // Numbers
  if (IS_DIGIT(c)) {
    scan_number(lex, tok, start, false);
    return;
  }

  switch (c) {
    // Single-character tokens
    case '(': make_token(lex, tok, TICK_TOK_LPAREN, start); return;
    case ')': make_token(lex, tok, TICK_TOK_RPAREN, start); return;
    case '{': make_token(lex, tok, TICK_TOK_LBRACE, start); return;
    case '}': make_token(lex, tok, TICK_TOK_RBRACE, start); return;
    case '[': make_token(lex, tok, TICK_TOK_LBRACKET, start); return;
    case ']': make_token(lex, tok, TICK_TOK_RBRACKET, start); return;
    case ',': make_token(lex, tok, TICK_TOK_COMMA, start); return;
    case ';': make_token(lex, tok, TICK_TOK_SEMICOLON, start); return;
    case ':': make_token(lex, tok, TICK_TOK_COLON, start); return;
    case '~': make_token(lex, tok, TICK_TOK_TILDE, start); return;
    case '?': make_token(lex, tok, TICK_TOK_QUESTION, start); return;
    case '^': make_token(lex, tok, TICK_TOK_CARET, start); return;

    // Operators that can be compound
    case '+':
      if (match(lex, '=')) {
        make_token(lex, tok, TICK_TOK_PLUS_EQ, start);
      } else if (match(lex, '|')) {
        make_token(lex, tok, TICK_TOK_PLUS_PIPE, start);
      } else if (match(lex, '%')) {
        make_token(lex, tok, TICK_TOK_PLUS_PERCENT, start);
      } else {
        make_token(lex, tok, TICK_TOK_PLUS, start);
      }
      return;
    case '-':
      // Check if this is a negative number literal
      if (IS_DIGIT(peek(lex))) {
        scan_number(lex, tok, start, true);
      } else if (match(lex, '=')) {
        make_token(lex, tok, TICK_TOK_MINUS_EQ, start);
      } else if (match(lex, '|')) {
        make_token(lex, tok, TICK_TOK_MINUS_PIPE, start);
      } else if (match(lex, '%')) {
        make_token(lex, tok, TICK_TOK_MINUS_PERCENT, start);
      } else {
        make_token(lex, tok, TICK_TOK_MINUS, start);
      }
      return;
    case '*':
      if (match(lex, '=')) {
        make_token(lex, tok, TICK_TOK_STAR_EQ, start);
      } else if (match(lex, '|')) {
        make_token(lex, tok, TICK_TOK_STAR_PIPE, start);
      } else if (match(lex, '%')) {
        make_token(lex, tok, TICK_TOK_STAR_PERCENT, start);
      } else {
        make_token(lex, tok, TICK_TOK_STAR, start);
      }
      return;
    case '/':
      if (match(lex, '=')) {
        make_token(lex, tok, TICK_TOK_SLASH_EQ, start);
      } else if (match(lex, '|')) {
        make_token(lex, tok, TICK_TOK_SLASH_PIPE, start);
      } else if (match(lex, '%')) {
        make_token(lex, tok, TICK_TOK_SLASH_PERCENT, start);
      } else {
        make_token(lex, tok, TICK_TOK_SLASH, start);
      }
      return;
    case '&':
      make_token(lex, tok, TICK_TOK_AMPERSAND, start);
      return;
    case '|':
      make_token(lex, tok, TICK_TOK_PIPE, start);
      return;
    case '%':
      make_token(lex, tok, TICK_TOK_PERCENT, start);
      return;
    case '!':
      make_token(lex, tok, match(lex, '=') ? TICK_TOK_BANG_EQ : TICK_TOK_BANG, start);
      return;
    case '=':
      make_token(lex, tok, match(lex, '=') ? TICK_TOK_EQ_EQ : TICK_TOK_EQ, start);
      return;
    case '<':
      if (match(lex, '<')) {
        make_token(lex, tok, TICK_TOK_LSHIFT, start);
      } else {
        make_token(lex, tok, match(lex, '=') ? TICK_TOK_LT_EQ : TICK_TOK_LT, start);
      }
      return;
    case '>':
      if (match(lex, '>')) {
        make_token(lex, tok, TICK_TOK_RSHIFT, start);
      } else {
        make_token(lex, tok, match(lex, '=') ? TICK_TOK_GT_EQ : TICK_TOK_GT, start);
      }
      return;
    case '.':
      make_token(lex, tok, match(lex, '.') ? TICK_TOK_DOT_DOT : TICK_TOK_DOT, start);
      return;

    // String literal
    case '"':
      scan_string(lex, tok, start, token_line, token_col);
      return;

    // Comment
    case '#':
      scan_comment(lex, tok, start);
      return;

    default:
      error_token(lex, tok, "Unexpected character", token_line, token_col);
      return;
  }
}
