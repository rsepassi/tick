// Lemon grammar for Tick language
// LALR(1) parser specification

%include {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "tick.h"

// Forward declaration
static void parse_error(tick_parse_t* parse, const char* msg);

// Parser-specific logging macro
#define PLOG(fmt, ...) DLOG("[parse] " fmt, ##__VA_ARGS__)

// Helper function to allocate AST node
static tick_ast_node_t* ast_alloc(tick_parse_t* parse, tick_ast_node_kind_t kind, usz line, usz col) {
    tick_allocator_config_t config = {
        .flags = TICK_ALLOCATOR_ZEROMEM,
        .alignment2 = 0,
    };
    tick_buf_t buf = {0};
    tick_err_t err = parse->alloc.realloc(parse->alloc.ctx, &buf, sizeof(tick_ast_node_t), &config);
    if (err != TICK_OK) {
        parse_error(parse, "Out of memory");
        // Return static dummy to avoid null crashes - error will be caught in main loop
        static tick_ast_node_t dummy = {0};
        return &dummy;
    }
    tick_ast_node_t* node = (tick_ast_node_t*)buf.buf;
    node->kind = kind;
    node->loc.line = line;
    node->loc.col = col;
    return node;
}

// Helper function to report parse error
static void parse_error(tick_parse_t* parse, const char* msg) {
    parse->has_error = true;

    // Write error message to errbuf if there's space
    if (parse->errbuf.sz > 0) {
        usz current_len = strlen((const char*)parse->errbuf.buf);
        usz remaining = parse->errbuf.sz - current_len;

        if (remaining > 1) {
            // Append error message
            usz msg_len = strlen(msg);
            usz to_copy = msg_len < remaining - 1 ? msg_len : remaining - 1;
            memcpy(parse->errbuf.buf + current_len, msg, to_copy);
            parse->errbuf.buf[current_len + to_copy] = '\0';
        }
    }
}

// Macros to reduce repetition in grammar rules
#define TYPE_RULE(T, Name) do { \
    T = ast_alloc(parse, TICK_AST_TYPE_NAMED, Name->line, Name->col); \
    T->data.type_named.name = Name->text; \
} while (0)

#define BINOP_RULE(E, L, T, R, op_name, binop_enum) do { \
    PLOG("Parsing " op_name); \
    E = ast_alloc(parse, TICK_AST_BINARY_EXPR, T->line, T->col); \
    E->data.binary_expr.op = binop_enum; \
    E->data.binary_expr.left = L; \
    E->data.binary_expr.right = R; \
} while (0)
}

%token_prefix TICK_TOK_
%token_type { tick_tok_t* }
%extra_argument { tick_parse_t* parse }

// Declare tokens in the same order as tick_tok_type_t enum
// This ensures Lemon's generated IDs match our enum values
%token EOF ERR.
%token IDENT.
%token UINT_LITERAL INT_LITERAL STRING_LITERAL BOOL_LITERAL.
%token BOOL I8 I16 I32 I64 ISZ U8 U16 U32 U64 USZ VOID.
%token AND ASYNC BREAK CASE CATCH CONTINUE DEFAULT DEFER ELSE.
%token EMBED_FILE ENUM ERRDEFER EXPORT FN FOR IF IMPORT IN LET.
%token OR PACKED PUB RESUME RETURN STRUCT SUSPEND SWITCH TRY.
%token UNION VAR VOLATILE WHILE.
%token LPAREN RPAREN LBRACE RBRACE LBRACKET RBRACKET.
%token COMMA SEMICOLON COLON DOT DOT_DOT QUESTION.
%token PLUS MINUS STAR SLASH PERCENT AMPERSAND PIPE CARET TILDE BANG.
%token EQ LT GT PLUS_EQ MINUS_EQ STAR_EQ SLASH_EQ.
%token PLUS_PIPE MINUS_PIPE STAR_PIPE SLASH_PIPE.
%token PLUS_PERCENT MINUS_PERCENT STAR_PERCENT SLASH_PERCENT.
%token BANG_EQ EQ_EQ LT_EQ GT_EQ LSHIFT RSHIFT.
%token COMMENT.

// Default type for non-terminals
%default_type { tick_ast_node_t* }

// Start symbol
%start_symbol module

// Precedence (lowest to highest)
%left COMMA.
%left PLUS MINUS.
%left STAR SLASH.

// Syntax error handler
%syntax_error {
    parse_error(parse, "Syntax error");
}

// Parse failure handler
%parse_failure {
    parse_error(parse, "Parse failed");
}

// =============================================================================
// RULES
// =============================================================================

// Module: list of declarations
module(M) ::= decl_list(D). {
    PLOG("Parsing module with declarations");
    M = ast_alloc(parse, TICK_AST_MODULE, 1, 1);
    M->data.module.name = (tick_buf_t){.buf = (u8*)"main", .sz = 4};
    M->data.module.decls = NULL;  // Will be converted from linked list later
    M->data.module.decl_count = 0;
    M->next = D;  // Store linked list in next pointer temporarily
    parse->root.root = M;
}

module(M) ::= . {
    PLOG("Parsing empty module");
    M = ast_alloc(parse, TICK_AST_MODULE, 1, 1);
    M->data.module.name = (tick_buf_t){.buf = (u8*)"main", .sz = 4};
    M->data.module.decls = NULL;
    M->data.module.decl_count = 0;
    parse->root.root = M;
}

// Declaration list - builds linked list
decl_list(L) ::= decl(D). {
    PLOG("Starting decl list");
    L = D;
    L->next = NULL;
}

decl_list(L) ::= decl_list(Prev) decl(D). {
    PLOG("Adding to decl list");
    L = D;
    L->next = Prev;
}

// Declarations
decl(D) ::= import_decl(I). { D = I; }
decl(D) ::= let_decl(L). { D = L; }
decl(D) ::= export_stmt(E). { D = E; }

// Import declaration: let std = import "std";
import_decl(I) ::= LET(T) IDENT(Name) EQ IMPORT STRING_LITERAL(Path) SEMICOLON. {
    PLOG("Parsing import declaration");
    I = ast_alloc(parse, TICK_AST_IMPORT_DECL, T->line, T->col);
    I->data.import_decl.name = Name->text;
    I->data.import_decl.path = Path->text;
    I->data.import_decl.is_pub = false;
}

// Let declaration: let name = value;
let_decl(D) ::= LET(T) IDENT(Name) EQ expr(E) SEMICOLON. {
    PLOG("Parsing let declaration");
    D = ast_alloc(parse, TICK_AST_LET_DECL, T->line, T->col);
    D->data.let_decl.name = Name->text;
    D->data.let_decl.type = NULL;
    D->data.let_decl.init = E;
    D->data.let_decl.is_pub = false;
}

// Let declaration with type: let name: type = value;
let_decl(D) ::= LET(T) IDENT(Name) COLON type(Ty) EQ expr(E) SEMICOLON. {
    PLOG("Parsing let declaration with type");
    D = ast_alloc(parse, TICK_AST_LET_DECL, T->line, T->col);
    D->data.let_decl.name = Name->text;
    D->data.let_decl.type = Ty;
    D->data.let_decl.init = E;
    D->data.let_decl.is_pub = false;
}

// Function expression: fn (params) return_type { body }
expr(E) ::= FN(T) LPAREN param_list(P) RPAREN type(Ret) block(B). {
    PLOG("Parsing function with params");
    E = ast_alloc(parse, TICK_AST_FUNCTION_DECL, T->line, T->col);
    E->data.function_decl.name = (tick_buf_t){0};
    E->data.function_decl.params = NULL;  // Will convert from linked list later
    E->data.function_decl.param_count = 0;
    E->data.function_decl.return_type = Ret;
    E->data.function_decl.body = B;
    E->data.function_decl.is_pub = false;
    // Store param list temporarily in the function's next pointer
    E->next = P;
}

expr(E) ::= FN(T) LPAREN RPAREN type(Ret) block(B). {
    PLOG("Parsing function with no params");
    E = ast_alloc(parse, TICK_AST_FUNCTION_DECL, T->line, T->col);
    E->data.function_decl.name = (tick_buf_t){0};
    E->data.function_decl.params = NULL;
    E->data.function_decl.param_count = 0;
    E->data.function_decl.return_type = Ret;
    E->data.function_decl.body = B;
    E->data.function_decl.is_pub = false;
}

// Parameter list - builds linked list using next pointer
%type param_list { tick_ast_node_t* }
param_list(L) ::= param(P). {
    L = P;
    L->next = NULL;
}

param_list(L) ::= param_list(Prev) COMMA param(P). {
    L = P;
    L->next = Prev;
}

// Parameter - store as a special AST node type for now
param(P) ::= IDENT(Name) COLON type(T). {
    P = ast_alloc(parse, TICK_AST_LET_DECL, Name->line, Name->col);  // Temporary type
    P->data.let_decl.name = Name->text;
    P->data.let_decl.type = T;
    P->data.let_decl.init = NULL;
}

// Types
type(T) ::= IDENT(Name). { TYPE_RULE(T, Name); }
type(T) ::= USZ(Name).   { TYPE_RULE(T, Name); }
type(T) ::= U64(Name).   { TYPE_RULE(T, Name); }
type(T) ::= U32(Name).   { TYPE_RULE(T, Name); }
type(T) ::= U16(Name).   { TYPE_RULE(T, Name); }
type(T) ::= U8(Name).    { TYPE_RULE(T, Name); }
type(T) ::= ISZ(Name).   { TYPE_RULE(T, Name); }
type(T) ::= I64(Name).   { TYPE_RULE(T, Name); }
type(T) ::= I32(Name).   { TYPE_RULE(T, Name); }
type(T) ::= I16(Name).   { TYPE_RULE(T, Name); }
type(T) ::= I8(Name).    { TYPE_RULE(T, Name); }

// Statements
stmt(S) ::= return_stmt(R). { S = R; }
stmt(S) ::= expr_stmt(E). { S = E; }

// Return statement
return_stmt(S) ::= RETURN(T) expr(E) SEMICOLON. {
    PLOG("Parsing return statement");
    S = ast_alloc(parse, TICK_AST_RETURN_STMT, T->line, T->col);
    S->data.return_stmt.value = E;
}

return_stmt(S) ::= RETURN(T) SEMICOLON. {
    PLOG("Parsing empty return statement");
    S = ast_alloc(parse, TICK_AST_RETURN_STMT, T->line, T->col);
    S->data.return_stmt.value = NULL;
}

// Expression statement
expr_stmt(S) ::= expr(E) SEMICOLON. {
    S = ast_alloc(parse, TICK_AST_EXPR_STMT, E->loc.line, E->loc.col);
    S->data.expr_stmt.expr = E;
}

// Block
block(B) ::= LBRACE(T) stmt_list(S) RBRACE. {
    PLOG("Parsing block with statements");
    B = ast_alloc(parse, TICK_AST_BLOCK_STMT, T->line, T->col);
    B->data.block_stmt.stmts = NULL;  // Will convert from linked list later
    B->data.block_stmt.stmt_count = 0;
    B->next = S;  // Store linked list temporarily
}

block(B) ::= LBRACE(T) RBRACE. {
    PLOG("Parsing empty block");
    B = ast_alloc(parse, TICK_AST_BLOCK_STMT, T->line, T->col);
    B->data.block_stmt.stmts = NULL;
    B->data.block_stmt.stmt_count = 0;
}

// Statement list - builds linked list
stmt_list(L) ::= stmt(S). {
    L = S;
    L->next = NULL;
}

stmt_list(L) ::= stmt_list(Prev) stmt(S). {
    L = S;
    L->next = Prev;
}

// Expressions
expr(E) ::= expr(L) PLUS(T) expr(R).  { BINOP_RULE(E, L, T, R, "addition", BINOP_ADD); }
expr(E) ::= expr(L) MINUS(T) expr(R). { BINOP_RULE(E, L, T, R, "subtraction", BINOP_SUB); }
expr(E) ::= expr(L) STAR(T) expr(R).  { BINOP_RULE(E, L, T, R, "multiplication", BINOP_MUL); }
expr(E) ::= expr(L) SLASH(T) expr(R). { BINOP_RULE(E, L, T, R, "division", BINOP_DIV); }

expr(E) ::= IDENT(T). {
    PLOG("Parsing identifier");
    E = ast_alloc(parse, TICK_AST_IDENTIFIER_EXPR, T->line, T->col);
    E->data.identifier_expr.name = T->text;
}

expr(E) ::= UINT_LITERAL(T). {
    PLOG("Parsing uint literal");
    E = ast_alloc(parse, TICK_AST_LITERAL, T->line, T->col);
    E->data.literal.value = T->literal.u64;
}

expr(E) ::= INT_LITERAL(T). {
    PLOG("Parsing int literal");
    E = ast_alloc(parse, TICK_AST_LITERAL, T->line, T->col);
    E->data.literal.value = (uint64_t)T->literal.i64;
}

// Export statement
export_stmt(E) ::= EXPORT(T) IDENT(Name) SEMICOLON. {
    PLOG("Parsing export statement");
    E = ast_alloc(parse, TICK_AST_EXPORT_STMT, T->line, T->col);
    E->data.export_stmt.name = Name->text;
}
