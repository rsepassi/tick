// Lemon grammar for Tick language
// LALR(1) parser specification

// =============================================================================
// C code
// =============================================================================
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
    T = ast_alloc(parse, TICK_AST_TYPE_NAMED, Name.line, Name.col); \
    T->data.type_named.name = Name.text; \
} while (0)

#define BINOP_RULE(E, L, T, R, op_name, binop_enum) do { \
    PLOG("Parsing " op_name); \
    E = ast_alloc(parse, TICK_AST_BINARY_EXPR, T.line, T.col); \
    E->data.binary_expr.op = binop_enum; \
    E->data.binary_expr.left = L; \
    E->data.binary_expr.right = R; \
} while (0)

#define UNOP_RULE(E, T, Operand, op_name, unop_enum) do { \
    PLOG("Parsing " op_name); \
    E = ast_alloc(parse, TICK_AST_UNARY_EXPR, T.line, T.col); \
    E->data.unary_expr.op = unop_enum; \
    E->data.unary_expr.operand = Operand; \
} while (0)

#define ASSIGN_RULE(S, L, T, R, assign_enum) do { \
    S = ast_alloc(parse, TICK_AST_ASSIGN_STMT, T.line, T.col); \
    S->data.assign_stmt.lhs = L; \
    S->data.assign_stmt.op = assign_enum; \
    S->data.assign_stmt.rhs = R; \
} while (0)
}

// Syntax error handler
%syntax_error {
    // TOKEN is the current lookahead token that caused the error
    char tok_buf[256];
    const char* tok_str = tick_tok_format(&TOKEN, tok_buf, sizeof(tok_buf));

    char err_msg[512];
    snprintf(err_msg, sizeof(err_msg), "Syntax error at %s", tok_str);
    parse_error(parse, err_msg);
}

// Parse failure handler
%parse_failure {
    parse_error(parse, "Parse failed - too many syntax errors");
}

// =============================================================================
// Lemon directives
// =============================================================================
%token_prefix TICK_TOK_
%token_type { tick_tok_t }
%extra_argument { tick_parse_t* parse }

// Declare tokens in the same order as tick_tok_type_t enum
// This ensures Lemon's generated IDs match our enum values
%token EOF ERR.
%token IDENT AT_BUILTIN.
%token UINT_LITERAL INT_LITERAL STRING_LITERAL BOOL_LITERAL NULL.
%token BOOL I8 I16 I32 I64 ISZ U8 U16 U32 U64 USZ VOID.
%token AND AS ASYNC BREAK CASE CATCH CONTINUE DEFAULT DEFER ELSE.
%token ENUM ERRDEFER EXTERN FN FOR IF IMPORT LET ALIGN.
%token OR ORELSE PACKED PUB RESUME RETURN STRUCT SUSPEND SWITCH TRY UNDEFINED.
%token UNION VAR VOLATILE.
%token LPAREN RPAREN LBRACE RBRACE LBRACKET RBRACKET.
%token COMMA SEMICOLON COLON DOT QUESTION DOT_QUESTION UNDERSCORE AT.
%token PLUS MINUS STAR SLASH PERCENT AMPERSAND PIPE CARET TILDE BANG.
%token EQ LT GT.
%token PLUS_PIPE MINUS_PIPE STAR_PIPE SLASH_PIPE.
%token PLUS_PERCENT MINUS_PERCENT STAR_PERCENT SLASH_PERCENT.
%token BANG_EQ EQ_EQ LT_EQ GT_EQ LSHIFT RSHIFT.
%token COMMENT.

// Precedence-only tokens (not real tokens, just for precedence declarations)
%token PREC_UNARY_MINUS PREC_UNARY_AMPERSAND PREC_UNARY_STAR.
%token PREC_RETURN_TYPE.

// Default type for non-terminals
%default_type { tick_ast_node_t* }

// =============================================================================
// RULES
// =============================================================================
%start_symbol module

// Module: list of declarations
module(M) ::= decl_list(D). {
    PLOG("Parsing module");
    M = ast_alloc(parse, TICK_AST_MODULE, 1, 1);
    M->data.module.name = (tick_buf_t){.buf = (u8*)"main", .sz = 4};
    M->data.module.decls = D;
    parse->root.root = M;
}

// Declaration list - builds linked list
decl_list(L) ::= . { L = NULL; }
decl_list(L) ::= decl_list(Prev) decl(D). { PLOG("Adding to decl list"); L = D; L->next = Prev; }

// Identifier - immediately copy from ephemeral token to prevent data loss
%type name { tick_ident_t }
name(N) ::= IDENT(T). {
    N.text = T.text;  // Copy buf pointer and size while token is still valid
    N.line = T.line;
    N.col = T.col;
}

// Literals - immediately copy from ephemeral token to prevent data loss
%type uint_literal { tick_literal_t }
uint_literal(L) ::= UINT_LITERAL(T). {
    L.value = T.literal;
    L.line = T.line;
    L.col = T.col;
}

%type int_literal { tick_literal_t }
int_literal(L) ::= INT_LITERAL(T). {
    L.value = T.literal;
    L.line = T.line;
    L.col = T.col;
}

%type bool_literal { tick_literal_t }
bool_literal(L) ::= BOOL_LITERAL(T). {
    L.value = T.literal;
    L.line = T.line;
    L.col = T.col;
}

// String literal - preserves text buffer
%type string_literal { tick_ident_t }
string_literal(S) ::= STRING_LITERAL(T). {
    S.text = T.text;
    S.line = T.line;
    S.col = T.col;
}

// Declaration qualifiers (can appear in any order)
%type decl_qualifiers { tick_qualifier_flags_t }
decl_qualifiers(Q) ::= . { Q = (tick_qualifier_flags_t){0}; }
decl_qualifiers(Q) ::= decl_qualifiers(Prev) LET. { Q = Prev; Q.is_var = false; }
decl_qualifiers(Q) ::= decl_qualifiers(Prev) VAR. { Q = Prev; Q.is_var = true; }
decl_qualifiers(Q) ::= decl_qualifiers(Prev) PUB. { Q = Prev; Q.is_pub = true; }
decl_qualifiers(Q) ::= decl_qualifiers(Prev) VOLATILE. { Q = Prev; Q.is_volatile = true; }
decl_qualifiers(Q) ::= decl_qualifiers(Prev) EXTERN. { Q = Prev; Q.is_extern = true; }

decl(D) ::= decl_qualifiers(Q) name(Name) COLON type(Ty) EQ expr(E) SEMICOLON. {
    PLOG("Parsing declaration with type and initializer");
    D = ast_alloc(parse, TICK_AST_DECL, Name.line, Name.col);
    D->data.decl.name = Name.text;
    D->data.decl.type = Ty;
    D->data.decl.init = E;
    D->data.decl.quals = Q;
}

decl(D) ::= decl_qualifiers(Q) name(Name) EQ expr(E) SEMICOLON. {
    PLOG("Parsing declaration with type inference");
    D = ast_alloc(parse, TICK_AST_DECL, Name.line, Name.col);
    D->data.decl.name = Name.text;
    D->data.decl.type = NULL;
    D->data.decl.init = E;
    D->data.decl.quals = Q;
}

decl(D) ::= decl_qualifiers(Q) name(Name) COLON type(Ty) SEMICOLON. {
    PLOG("Parsing declaration without initializer (extern)");
    D = ast_alloc(parse, TICK_AST_DECL, Name.line, Name.col);
    D->data.decl.name = Name.text;
    D->data.decl.type = Ty;
    D->data.decl.init = NULL;
    D->data.decl.quals = Q;
}

// Optional return type
return_type(R) ::= . [PREC_RETURN_TYPE] { R = NULL; }  // NULL means void
return_type(R) ::= type(T). [PREC_RETURN_TYPE] { R = T; }

// Function expression: fn (params) return_type { body }
expr(E) ::= FN(T) fn_params(P) return_type(Ret) block(B). {
    PLOG("Parsing function");
    E = ast_alloc(parse, TICK_AST_FUNCTION, T.line, T.col);
    E->data.function.params = P;
    E->data.function.return_type = Ret;
    E->data.function.body = B;
    E->data.function.quals.is_var = false;
    E->data.function.quals.is_pub = false;
    E->data.function.quals.is_volatile = false;
}

fn_params(L) ::= . { L = NULL; }
fn_params(L) ::= LPAREN RPAREN. { L = NULL; }
fn_params(L) ::= LPAREN param_list(R) RPAREN. { L = R; }

// Struct qualifiers
%type struct_quals { tick_struct_quals_t }
struct_quals(Q) ::= . { Q.is_packed = false; Q.alignment = NULL; }
struct_quals(Q) ::= PACKED. { Q.is_packed = true; Q.alignment = NULL; }
struct_quals(Q) ::= ALIGN LPAREN expr(E) RPAREN. { Q.is_packed = false; Q.alignment = E; }
struct_quals(Q) ::= PACKED ALIGN LPAREN expr(E) RPAREN. { Q.is_packed = true; Q.alignment = E; }
struct_quals(Q) ::= ALIGN LPAREN expr(E) RPAREN PACKED. { Q.is_packed = true; Q.alignment = E; }

// Struct definition
expr(E) ::= STRUCT(T) struct_quals(Q) LBRACE field_list(F) RBRACE. {
    PLOG("Parsing struct");
    E = ast_alloc(parse, TICK_AST_STRUCT_DECL, T.line, T.col);
    E->data.struct_decl.fields = F;
    E->data.struct_decl.is_packed = Q.is_packed;
    E->data.struct_decl.alignment = Q.alignment;
}

// Field list - builds linked list
field_list(L) ::= . { L = NULL; }
field_list(L) ::= field(F). { L = F; L->next = NULL; }
field_list(L) ::= field_list(Prev) COMMA field(F). { L = F; L->next = Prev; }
field_list(L) ::= field_list(Prev) COMMA. { L = Prev; }  // Trailing comma

// Field
field(F) ::= name(Name) COLON type(T) align_spec(Align). {
    PLOG("Parsing field");
    F = ast_alloc(parse, TICK_AST_FIELD, Name.line, Name.col);
    F->data.field.name = Name.text;
    F->data.field.type = T;
    F->data.field.alignment = Align;
}

// Enum definition
expr(E) ::= ENUM(T) LPAREN type(Ty) RPAREN LBRACE enum_value_list(V) RBRACE. {
    PLOG("Parsing enum");
    E = ast_alloc(parse, TICK_AST_ENUM_DECL, T.line, T.col);
    E->data.enum_decl.underlying_type = Ty; E->data.enum_decl.values = V;
}

// Enum value list - builds linked list
enum_value_list(L) ::= . { L = NULL; }
enum_value_list(L) ::= enum_value(V). { L = V; L->next = NULL; }
enum_value_list(L) ::= enum_value_list(Prev) COMMA enum_value(V). { L = V; L->next = Prev; }
enum_value_list(L) ::= enum_value_list(Prev) COMMA. { L = Prev; }  // Trailing comma

// Enum value
enum_value(V) ::= name(Name). {
    PLOG("Parsing enum value without explicit value");
    V = ast_alloc(parse, TICK_AST_ENUM_VALUE, Name.line, Name.col);
    V->data.enum_value.name = Name.text;
    V->data.enum_value.value = NULL;  // Auto-increment
}

enum_value(V) ::= name(Name) EQ expr(E). {
    PLOG("Parsing enum value with explicit value");
    V = ast_alloc(parse, TICK_AST_ENUM_VALUE, Name.line, Name.col);
    V->data.enum_value.name = Name.text;
    V->data.enum_value.value = E;
}

// Optional tag type for union
tag_type(T) ::= . { T = NULL; }  // Automatic tag
tag_type(T) ::= LPAREN type(Ty) RPAREN. { T = Ty; }

// Optional alignment specifier
align_spec(A) ::= . { A = NULL; }
align_spec(A) ::= ALIGN LPAREN expr(E) RPAREN. { A = E; }

// Union definition
expr(E) ::= UNION(T) tag_type(Tag) align_spec(Align) LBRACE field_list(F) RBRACE. {
    PLOG("Parsing union");
    E = ast_alloc(parse, TICK_AST_UNION_DECL, T.line, T.col);
    E->data.union_decl.tag_type = Tag;
    E->data.union_decl.fields = F;
    E->data.union_decl.alignment = Align;
}

// Parameter list - builds linked list using next pointer
param_list(L) ::= param(P). { L = P; L->next = NULL; }
param_list(L) ::= param_list(Prev) COMMA param(P). { L = P; L->next = Prev; }
param_list(L) ::= param_list(Prev) COMMA. { L = Prev; }  // Trailing comma

// Parameter
param(P) ::= name(Name) COLON type(T). {
    P = ast_alloc(parse, TICK_AST_PARAM, Name.line, Name.col);
    P->data.param.name = Name.text; P->data.param.type = T;
}

// Types
// Named types (identifiers and primitives)
type(T) ::= name(Name). {
    T = ast_alloc(parse, TICK_AST_TYPE_NAMED, Name.line, Name.col);
    T->data.type_named.name = Name.text;
}
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
type(T) ::= VOID(Name).  { TYPE_RULE(T, Name); }
type(T) ::= BOOL(Name).  { TYPE_RULE(T, Name); }

// Array type: [i32; 10] or [i32; _]
type(T) ::= LBRACKET(Tok) type(Elem) SEMICOLON expr(Size) RBRACKET. {
    PLOG("Parsing array type");
    T = ast_alloc(parse, TICK_AST_TYPE_ARRAY, Tok.line, Tok.col);
    T->data.type_array.element_type = Elem; T->data.type_array.size = Size;
}
type(T) ::= LBRACKET(Tok) type(Elem) SEMICOLON UNDERSCORE(U) RBRACKET. {
    PLOG("Parsing array type with inferred length");
    T = ast_alloc(parse, TICK_AST_TYPE_ARRAY, Tok.line, Tok.col);
    T->data.type_array.element_type = Elem;
    tick_ast_node_t* size = ast_alloc(parse, TICK_AST_LITERAL, U.line, U.col);
    size->data.literal.kind = TICK_LIT_UINT;
    size->data.literal.data.uint_value = 0;  // Special marker for inferred
    T->data.type_array.size = size;
}

// Array type (alternate syntax): [10]i32
type(T) ::= LBRACKET(Tok) uint_literal(Size) RBRACKET type(Elem). {
    PLOG("Parsing array type (size-first syntax)");
    T = ast_alloc(parse, TICK_AST_TYPE_ARRAY, Tok.line, Tok.col);
    T->data.type_array.element_type = Elem;
    // Convert literal to expression node
    tick_ast_node_t* size_expr = ast_alloc(parse, TICK_AST_LITERAL, Size.line, Size.col);
    size_expr->data.literal.kind = TICK_LIT_UINT;
    size_expr->data.literal.data.uint_value = Size.value.u64;
    T->data.type_array.size = size_expr;
}

// Slice type: []i32
type(T) ::= LBRACKET(Tok) RBRACKET type(Elem). {
    PLOG("Parsing slice type");
    T = ast_alloc(parse, TICK_AST_TYPE_SLICE, Tok.line, Tok.col);
    T->data.type_array.element_type = Elem; T->data.type_array.size = NULL;
}

// Pointer type: *i32
type(T) ::= STAR(Tok) type(Pointee). {
    PLOG("Parsing pointer type");
    T = ast_alloc(parse, TICK_AST_TYPE_POINTER, Tok.line, Tok.col);
    T->data.type_pointer.pointee_type = Pointee;
}

// Optional type: ?i32
type(T) ::= QUESTION(Tok) type(Inner). {
    PLOG("Parsing optional type");
    T = ast_alloc(parse, TICK_AST_TYPE_OPTIONAL, Tok.line, Tok.col);
    T->data.type_optional.inner_type = Inner;
}

// Error union type: A!B (error type A OR value type B)
type(T) ::= type(ErrorType) BANG(Tok) type(ValueType). {
    PLOG("Parsing error union type");
    T = ast_alloc(parse, TICK_AST_TYPE_ERROR_UNION, Tok.line, Tok.col);
    T->data.type_error_union.error_type = ErrorType;
    T->data.type_error_union.value_type = ValueType;
}

// Error union type shorthand: !T (default error type)
type(T) ::= BANG(Tok) type(ValueType). {
    PLOG("Parsing error union type with default error");
    T = ast_alloc(parse, TICK_AST_TYPE_ERROR_UNION, Tok.line, Tok.col);
    T->data.type_error_union.error_type = NULL;  // NULL indicates default/generic error
    T->data.type_error_union.value_type = ValueType;
}

// Function type: fn(params) return_type
type(T) ::= FN(Tok) fn_type_params(P) return_type(Ret). {
    PLOG("Parsing function type");
    T = ast_alloc(parse, TICK_AST_TYPE_FUNCTION, Tok.line, Tok.col);
    T->data.type_function.params = P;
    T->data.type_function.return_type = Ret;
}

// Function type parameters (just types, no names)
fn_type_params(L) ::= LPAREN RPAREN. { L = NULL; }
fn_type_params(L) ::= LPAREN fn_type_param_list(R) RPAREN. { L = R; }

// Function type parameter list - supports both name:type and just type
fn_type_param_list(L) ::= fn_type_param(P). {
    L = P;
    L->next = NULL;
}
fn_type_param_list(L) ::= fn_type_param_list(Prev) COMMA fn_type_param(P). {
    L = P;
    L->next = Prev;
}
fn_type_param_list(L) ::= fn_type_param_list(Prev) COMMA. { L = Prev; }  // Trailing comma

// Function type parameter - can be "name: type" or just "type"
fn_type_param(P) ::= name(Name) COLON type(T). {
    P = ast_alloc(parse, TICK_AST_PARAM, Name.line, Name.col);
    P->data.param.name = Name.text;
    P->data.param.type = T;
}
fn_type_param(P) ::= type(T). {
    P = ast_alloc(parse, TICK_AST_PARAM, T->loc.line, T->loc.col);
    P->data.param.name = (tick_buf_t){.buf = NULL, .sz = 0};  // No name
    P->data.param.type = T;
}

// Statements
// Note: For local declarations, we require LET or VAR at the start to avoid ambiguity with assignments
stmt(S) ::= LET name(Name) COLON type(Ty) EQ expr(E) SEMICOLON. {
    PLOG("Parsing local let declaration with type");
    S = ast_alloc(parse, TICK_AST_DECL, Name.line, Name.col);
    S->data.decl.name = Name.text;
    S->data.decl.type = Ty;
    S->data.decl.init = E;
    S->data.decl.quals = (tick_qualifier_flags_t){.is_var = false};
}
stmt(S) ::= LET name(Name) EQ expr(E) SEMICOLON. {
    PLOG("Parsing local let declaration with type inference");
    S = ast_alloc(parse, TICK_AST_DECL, Name.line, Name.col);
    S->data.decl.name = Name.text;
    S->data.decl.type = NULL;
    S->data.decl.init = E;
    S->data.decl.quals = (tick_qualifier_flags_t){.is_var = false};
}
stmt(S) ::= VAR name(Name) COLON type(Ty) EQ expr(E) SEMICOLON. {
    PLOG("Parsing local var declaration with type");
    S = ast_alloc(parse, TICK_AST_DECL, Name.line, Name.col);
    S->data.decl.name = Name.text;
    S->data.decl.type = Ty;
    S->data.decl.init = E;
    S->data.decl.quals = (tick_qualifier_flags_t){.is_var = true};
}
stmt(S) ::= VAR name(Name) EQ expr(E) SEMICOLON. {
    PLOG("Parsing local var declaration with type inference");
    S = ast_alloc(parse, TICK_AST_DECL, Name.line, Name.col);
    S->data.decl.name = Name.text;
    S->data.decl.type = NULL;
    S->data.decl.init = E;
    S->data.decl.quals = (tick_qualifier_flags_t){.is_var = true};
}
stmt(S) ::= assign_stmt(A). { S = A; }
stmt(S) ::= return_stmt(R). { S = R; }
stmt(S) ::= if_stmt(I). { S = I; }
stmt(S) ::= for_stmt(F). { S = F; }
stmt(S) ::= switch_stmt(Sw). { S = Sw; }
stmt(S) ::= break_stmt(B). { S = B; }
stmt(S) ::= continue_stmt(C). { S = C; }
stmt(S) ::= block(Bl). { S = Bl; }
stmt(S) ::= expr_stmt(E). { S = E; }

// Optional return value
return_value(V) ::= . { V = NULL; }
return_value(V) ::= expr(E). { V = E; }

// Return statement
return_stmt(S) ::= RETURN(T) return_value(V) SEMICOLON. {
    PLOG("Parsing return statement");
    S = ast_alloc(parse, TICK_AST_RETURN_STMT, T.line, T.col);
    S->data.return_stmt.value = V;
}

// Expression statement
expr_stmt(S) ::= expr(E) SEMICOLON. {
    S = ast_alloc(parse, TICK_AST_EXPR_STMT, E->loc.line, E->loc.col);
    S->data.expr_stmt.expr = E;
}

// Assignment statements
assign_stmt(S) ::= expr(L) EQ(T) expr(R) SEMICOLON. { ASSIGN_RULE(S, L, T, R, ASSIGN_EQ); }

// If statements
if_stmt(S) ::= IF(T) expr(Cond) block(Then). {
    PLOG("Parsing if statement");
    S = ast_alloc(parse, TICK_AST_IF_STMT, T.line, T.col);
    S->data.if_stmt.condition = Cond;
    S->data.if_stmt.then_block = Then;
    S->data.if_stmt.else_block = NULL;
}

if_stmt(S) ::= IF(T) expr(Cond) block(Then) ELSE block(Else). {
    PLOG("Parsing if-else statement");
    S = ast_alloc(parse, TICK_AST_IF_STMT, T.line, T.col);
    S->data.if_stmt.condition = Cond;
    S->data.if_stmt.then_block = Then;
    S->data.if_stmt.else_block = Else;
}

if_stmt(S) ::= IF(T) expr(Cond) block(Then) ELSE if_stmt(Elif). {
    PLOG("Parsing if-else if statement");
    S = ast_alloc(parse, TICK_AST_IF_STMT, T.line, T.col);
    S->data.if_stmt.condition = Cond;
    S->data.if_stmt.then_block = Then;
    S->data.if_stmt.else_block = Elif;
}

// For loops (Go-style)
// Simple statements (without trailing semicolon, for use in for loop headers)
simple_stmt(S) ::= . { S = NULL; }

simple_stmt(S) ::= expr(L) EQ(T) expr(R). {
    ASSIGN_RULE(S, L, T, R, ASSIGN_EQ);
}

simple_stmt(S) ::= LET name(Name) COLON type(Ty) EQ expr(E). {
    S = ast_alloc(parse, TICK_AST_DECL, Name.line, Name.col);
    S->data.decl.name = Name.text;
    S->data.decl.type = Ty;
    S->data.decl.init = E;
    S->data.decl.quals = (tick_qualifier_flags_t){.is_var = false};
}

simple_stmt(S) ::= LET name(Name) EQ expr(E). {
    S = ast_alloc(parse, TICK_AST_DECL, Name.line, Name.col);
    S->data.decl.name = Name.text;
    S->data.decl.type = NULL;
    S->data.decl.init = E;
    S->data.decl.quals = (tick_qualifier_flags_t){.is_var = false};
}

simple_stmt(S) ::= VAR name(Name) COLON type(Ty) EQ expr(E). {
    S = ast_alloc(parse, TICK_AST_DECL, Name.line, Name.col);
    S->data.decl.name = Name.text;
    S->data.decl.type = Ty;
    S->data.decl.init = E;
    S->data.decl.quals = (tick_qualifier_flags_t){.is_var = true};
}

simple_stmt(S) ::= VAR name(Name) EQ expr(E). {
    S = ast_alloc(parse, TICK_AST_DECL, Name.line, Name.col);
    S->data.decl.name = Name.text;
    S->data.decl.type = NULL;
    S->data.decl.init = E;
    S->data.decl.quals = (tick_qualifier_flags_t){.is_var = true};
}

simple_stmt(S) ::= expr(E). {
    S = ast_alloc(parse, TICK_AST_EXPR_STMT, E->loc.line, E->loc.col);
    S->data.expr_stmt.expr = E;
}

// For loop: infinite, while, or C-style
for_stmt(S) ::= FOR(T) block(Body). {
    PLOG("Parsing infinite for loop");
    S = ast_alloc(parse, TICK_AST_FOR_STMT, T.line, T.col);
    S->data.for_stmt.init_stmt = NULL;
    S->data.for_stmt.condition = NULL;
    S->data.for_stmt.step_stmt = NULL;
    S->data.for_stmt.body = Body;
}

for_stmt(S) ::= FOR(T) expr(Cond) block(Body). {
    PLOG("Parsing while-style for loop");
    S = ast_alloc(parse, TICK_AST_FOR_STMT, T.line, T.col);
    S->data.for_stmt.init_stmt = NULL;
    S->data.for_stmt.condition = Cond;
    S->data.for_stmt.step_stmt = NULL;
    S->data.for_stmt.body = Body;
}

for_stmt(S) ::= FOR(T) simple_stmt(Init) SEMICOLON expr(Cond) SEMICOLON simple_stmt(Step) block(Body). {
    PLOG("Parsing C-style for loop");
    S = ast_alloc(parse, TICK_AST_FOR_STMT, T.line, T.col);
    S->data.for_stmt.init_stmt = Init;
    S->data.for_stmt.condition = Cond;
    S->data.for_stmt.step_stmt = Step;
    S->data.for_stmt.body = Body;
}

// For-switch statement (computed goto pattern)
stmt(S) ::= FOR(T) SWITCH expr(Val) LBRACE case_list(Cases) RBRACE. {
    PLOG("Parsing for-switch statement");
    S = ast_alloc(parse, TICK_AST_FOR_SWITCH_STMT, T.line, T.col);
    S->data.for_switch_stmt.value = Val;
    S->data.for_switch_stmt.body = Cases;
}

// Switch statement
switch_stmt(S) ::= SWITCH(T) expr(Val) LBRACE case_list(Cases) RBRACE. {
    PLOG("Parsing switch statement");
    S = ast_alloc(parse, TICK_AST_SWITCH_STMT, T.line, T.col);
    S->data.switch_stmt.value = Val; S->data.switch_stmt.cases = Cases;
}

// Case list - builds linked list
case_list(L) ::= . { L = NULL; }
case_list(L) ::= case_list(Prev) case_clause(C). { L = C; L->next = Prev; }

// Case clause
case_clause(C) ::= CASE(T) case_value_list(V) COLON stmt_list(S). {
    PLOG("Parsing case clause");
    C = ast_alloc(parse, TICK_AST_SWITCH_CASE, T.line, T.col);
    C->data.switch_case.values = V;
    C->data.switch_case.stmts = S;
}

case_clause(C) ::= DEFAULT(T) COLON stmt_list(S). {
    PLOG("Parsing default clause");
    C = ast_alloc(parse, TICK_AST_SWITCH_CASE, T.line, T.col);
    C->data.switch_case.values = NULL;  // NULL for default case
    C->data.switch_case.stmts = S;
}

// Case value list - builds linked list
case_value_list(L) ::= . { L = NULL; }
case_value_list(L) ::= expr(E). { L = E; L->next = NULL; }
case_value_list(L) ::= case_value_list(Prev) COMMA expr(E). { L = E; L->next = Prev; }
case_value_list(L) ::= case_value_list(Prev) COMMA. { L = Prev; }  // Trailing comma

// Break and continue statements
break_stmt(S) ::= BREAK(T) SEMICOLON. {
    PLOG("Parsing break statement");
    S = ast_alloc(parse, TICK_AST_BREAK_STMT, T.line, T.col);
}
continue_stmt(S) ::= CONTINUE(T) SEMICOLON. {
    PLOG("Parsing continue statement");
    S = ast_alloc(parse, TICK_AST_CONTINUE_STMT, T.line, T.col);
}

// Continue-switch statement (computed goto pattern)
stmt(S) ::= CONTINUE(T) SWITCH expr(Val) SEMICOLON. {
    PLOG("Parsing continue-switch statement");
    S = ast_alloc(parse, TICK_AST_CONTINUE_SWITCH_STMT, T.line, T.col);
    S->data.continue_switch_stmt.value = Val;
}

// Coroutine statements
stmt(S) ::= SUSPEND(T) SEMICOLON. {
    PLOG("Parsing suspend statement");
    S = ast_alloc(parse, TICK_AST_SUSPEND_STMT, T.line, T.col);
}

stmt(S) ::= RESUME(T) expr(Handle) SEMICOLON. {
    PLOG("Parsing resume statement");
    S = ast_alloc(parse, TICK_AST_RESUME_STMT, T.line, T.col);
    S->data.resume_stmt.handle = Handle;
}

// Capture rule for |identifier| syntax
%type capture { tick_buf_t }
capture(C) ::= PIPE name(Name) PIPE. {
    PLOG("Parsing capture");
    C = Name.text;
}

// Try-catch statement
stmt(S) ::= TRY(T) expr(Call) CATCH capture(Cap) stmt(CatchStmt). {
    PLOG("Parsing try-catch statement");
    S = ast_alloc(parse, TICK_AST_TRY_CATCH_STMT, T.line, T.col);
    S->data.try_catch_stmt.call = Call;
    S->data.try_catch_stmt.capture = Cap;
    S->data.try_catch_stmt.catch_stmt = CatchStmt;
}

// Defer and errdefer statements
stmt(S) ::= DEFER(T) stmt(Deferred). {
    PLOG("Parsing defer statement");
    S = ast_alloc(parse, TICK_AST_DEFER_STMT, T.line, T.col);
    S->data.defer_stmt.stmt = Deferred;
}

stmt(S) ::= ERRDEFER(T) stmt(Deferred). {
    PLOG("Parsing errdefer statement");
    S = ast_alloc(parse, TICK_AST_ERRDEFER_STMT, T.line, T.col);
    S->data.errdefer_stmt.stmt = Deferred;
}

// Block
block(B) ::= LBRACE(T) stmt_list(S) RBRACE. {
    PLOG("Parsing block");
    B = ast_alloc(parse, TICK_AST_BLOCK_STMT, T.line, T.col);
    B->data.block_stmt.stmts = S;
}

// Statement list - builds linked list
stmt_list(L) ::= . { L = NULL; }
stmt_list(L) ::= stmt_list(Prev) stmt(S). { L = S; L->next = Prev; }

// Expressions - Binary operators
// Arithmetic
expr(E) ::= expr(L) PLUS(T) expr(R).    { BINOP_RULE(E, L, T, R, "addition", BINOP_ADD); }
expr(E) ::= expr(L) MINUS(T) expr(R).   { BINOP_RULE(E, L, T, R, "subtraction", BINOP_SUB); }
expr(E) ::= expr(L) STAR(T) expr(R).    { BINOP_RULE(E, L, T, R, "multiplication", BINOP_MUL); }
expr(E) ::= expr(L) SLASH(T) expr(R).   { BINOP_RULE(E, L, T, R, "division", BINOP_DIV); }
expr(E) ::= expr(L) PERCENT(T) expr(R). { BINOP_RULE(E, L, T, R, "modulo", BINOP_MOD); }

// Saturating arithmetic
expr(E) ::= expr(L) PLUS_PIPE(T) expr(R).   { BINOP_RULE(E, L, T, R, "saturating addition", BINOP_SAT_ADD); }
expr(E) ::= expr(L) MINUS_PIPE(T) expr(R).  { BINOP_RULE(E, L, T, R, "saturating subtraction", BINOP_SAT_SUB); }
expr(E) ::= expr(L) STAR_PIPE(T) expr(R).   { BINOP_RULE(E, L, T, R, "saturating multiplication", BINOP_SAT_MUL); }
expr(E) ::= expr(L) SLASH_PIPE(T) expr(R).  { BINOP_RULE(E, L, T, R, "saturating division", BINOP_SAT_DIV); }

// Wrapping arithmetic
expr(E) ::= expr(L) PLUS_PERCENT(T) expr(R).   { BINOP_RULE(E, L, T, R, "wrapping addition", BINOP_WRAP_ADD); }
expr(E) ::= expr(L) MINUS_PERCENT(T) expr(R).  { BINOP_RULE(E, L, T, R, "wrapping subtraction", BINOP_WRAP_SUB); }
expr(E) ::= expr(L) STAR_PERCENT(T) expr(R).   { BINOP_RULE(E, L, T, R, "wrapping multiplication", BINOP_WRAP_MUL); }
expr(E) ::= expr(L) SLASH_PERCENT(T) expr(R).  { BINOP_RULE(E, L, T, R, "wrapping division", BINOP_WRAP_DIV); }

// Comparison
expr(E) ::= expr(L) EQ_EQ(T) expr(R).   { BINOP_RULE(E, L, T, R, "equality", BINOP_EQ); }
expr(E) ::= expr(L) BANG_EQ(T) expr(R). { BINOP_RULE(E, L, T, R, "inequality", BINOP_NE); }
expr(E) ::= expr(L) LT(T) expr(R).      { BINOP_RULE(E, L, T, R, "less than", BINOP_LT); }
expr(E) ::= expr(L) GT(T) expr(R).      { BINOP_RULE(E, L, T, R, "greater than", BINOP_GT); }
expr(E) ::= expr(L) LT_EQ(T) expr(R).   { BINOP_RULE(E, L, T, R, "less or equal", BINOP_LE); }
expr(E) ::= expr(L) GT_EQ(T) expr(R).   { BINOP_RULE(E, L, T, R, "greater or equal", BINOP_GE); }

// Bitwise (not in precedence table - must use parens when mixing)
expr(E) ::= expr(L) AMPERSAND(T) expr(R). { BINOP_RULE(E, L, T, R, "bitwise and", BINOP_AND); }
expr(E) ::= expr(L) PIPE(T) expr(R).      { BINOP_RULE(E, L, T, R, "bitwise or", BINOP_OR); }
expr(E) ::= expr(L) CARET(T) expr(R).     { BINOP_RULE(E, L, T, R, "bitwise xor", BINOP_XOR); }
expr(E) ::= expr(L) LSHIFT(T) expr(R).    { BINOP_RULE(E, L, T, R, "left shift", BINOP_LSHIFT); }
expr(E) ::= expr(L) RSHIFT(T) expr(R).    { BINOP_RULE(E, L, T, R, "right shift", BINOP_RSHIFT); }

// Logical
expr(E) ::= expr(L) AND(T) expr(R). { BINOP_RULE(E, L, T, R, "logical and", BINOP_LOGICAL_AND); }
expr(E) ::= expr(L) OR(T) expr(R).  { BINOP_RULE(E, L, T, R, "logical or", BINOP_LOGICAL_OR); }

// Optional operators
expr(E) ::= expr(L) ORELSE(T) expr(R). { BINOP_RULE(E, L, T, R, "orelse", BINOP_ORELSE); }

// Unary operators
expr(E) ::= MINUS(T) expr(Operand). [PREC_UNARY_MINUS] { UNOP_RULE(E, T, Operand, "negation", UNOP_NEG); }
expr(E) ::= BANG(T) expr(Operand). { UNOP_RULE(E, T, Operand, "logical not", UNOP_NOT); }
expr(E) ::= TILDE(T) expr(Operand). { UNOP_RULE(E, T, Operand, "bitwise not", UNOP_BIT_NOT); }
expr(E) ::= AMPERSAND(T) expr(Operand). [PREC_UNARY_AMPERSAND] { UNOP_RULE(E, T, Operand, "address-of", UNOP_ADDR); }
expr(E) ::= STAR(T) expr(Operand). [PREC_UNARY_STAR] { UNOP_RULE(E, T, Operand, "dereference", UNOP_DEREF); }

// Parenthesized expressions
expr(E) ::= LPAREN expr(Ex) RPAREN. { PLOG("Parsing parenthesized expression"); E = Ex; }

// Primary expressions
expr(E) ::= name(N). {
    PLOG("Parsing identifier");
    E = ast_alloc(parse, TICK_AST_IDENTIFIER_EXPR, N.line, N.col);
    E->data.identifier_expr.name = N.text;
    E->data.identifier_expr.at_builtin = TICK_AT_BUILTIN_UNKNOWN;
}

expr(E) ::= AT_BUILTIN(T). {
    PLOG("Parsing AT builtin");
    E = ast_alloc(parse, TICK_AST_IDENTIFIER_EXPR, T.line, T.col);
    E->data.identifier_expr.name = T.text;
    E->data.identifier_expr.at_builtin = TICK_AT_BUILTIN_UNKNOWN;  // Will be resolved by analysis
}

expr(E) ::= IMPORT(T) string_literal(Path). {
    PLOG("Parsing import expression");
    E = ast_alloc(parse, TICK_AST_IMPORT, T.line, T.col);
    E->data.import.path = Path.text;
}

expr(E) ::= ASYNC(T) expr(Call) COMMA expr(Frame). {
    PLOG("Parsing async expression");
    E = ast_alloc(parse, TICK_AST_ASYNC_EXPR, T.line, T.col);
    E->data.async_expr.call = Call;
    E->data.async_expr.frame = Frame;
}

expr(E) ::= uint_literal(L). {
    PLOG("Parsing uint literal");
    E = ast_alloc(parse, TICK_AST_LITERAL, L.line, L.col);
    E->data.literal.kind = TICK_LIT_UINT;
    E->data.literal.data.uint_value = L.value.u64;
}
expr(E) ::= int_literal(L). {
    PLOG("Parsing int literal");
    E = ast_alloc(parse, TICK_AST_LITERAL, L.line, L.col);
    E->data.literal.kind = TICK_LIT_INT;
    E->data.literal.data.int_value = L.value.i64;
}
expr(E) ::= string_literal(S). {
    PLOG("Parsing string literal");
    E = ast_alloc(parse, TICK_AST_LITERAL, S.line, S.col);
    E->data.literal.kind = TICK_LIT_STRING;
    E->data.literal.data.string_value = S.text;
}
expr(E) ::= bool_literal(L). {
    PLOG("Parsing bool literal");
    E = ast_alloc(parse, TICK_AST_LITERAL, L.line, L.col);
    E->data.literal.kind = TICK_LIT_BOOL;
    E->data.literal.data.bool_value = (L.value.u64 != 0);
}
expr(E) ::= UNDEFINED(T). {
    PLOG("Parsing undefined");
    E = ast_alloc(parse, TICK_AST_LITERAL, T.line, T.col);
    E->data.literal.kind = TICK_LIT_UNDEFINED;
}
expr(E) ::= NULL(T). {
    PLOG("Parsing null");
    E = ast_alloc(parse, TICK_AST_LITERAL, T.line, T.col);
    E->data.literal.kind = TICK_LIT_NULL;
}

// Function calls
expr(E) ::= expr(Callee) LPAREN(T) expr_list(Args) RPAREN. {
    PLOG("Parsing function call");
    E = ast_alloc(parse, TICK_AST_CALL_EXPR, T.line, T.col);
    E->data.call_expr.callee = Callee; E->data.call_expr.args = Args;
}

// Field access (dot implies dereference for pointers)
expr(E) ::= expr(Obj) DOT(T) name(Field). {
    PLOG("Parsing field access");
    E = ast_alloc(parse, TICK_AST_FIELD_ACCESS_EXPR, T.line, T.col);
    E->data.field_access_expr.object = Obj;
    E->data.field_access_expr.field_name = Field.text;
    E->data.field_access_expr.is_arrow = false;
}

// Unwrap panic: a.? (unwrap optional or panic)
expr(E) ::= expr(Operand) DOT_QUESTION(T). {
    PLOG("Parsing unwrap panic");
    E = ast_alloc(parse, TICK_AST_UNWRAP_PANIC_EXPR, T.line, T.col);
    E->data.unwrap_panic_expr.operand = Operand;
    E->data.unwrap_panic_expr.resolved_type = NULL;
}

// Array indexing
expr(E) ::= expr(Arr) LBRACKET(T) expr(Idx) RBRACKET. {
    PLOG("Parsing array index");
    E = ast_alloc(parse, TICK_AST_INDEX_EXPR, T.line, T.col);
    E->data.index_expr.array = Arr; E->data.index_expr.index = Idx;
}

// Struct literal: Point@{ field: value, ... }
expr(E) ::= name(TypeName) AT LBRACE(T) struct_init_list(Fields) RBRACE. {
    PLOG("Parsing named struct literal");
    E = ast_alloc(parse, TICK_AST_STRUCT_INIT_EXPR, T.line, T.col);
    tick_ast_node_t* type_node = ast_alloc(parse, TICK_AST_TYPE_NAMED, TypeName.line, TypeName.col);
    type_node->data.type_named.name = TypeName.text;
    E->data.struct_init_expr.type = type_node;
    E->data.struct_init_expr.fields = Fields;
}

// Anonymous struct literal: @{ field: value, ... }
expr(E) ::= AT LBRACE(T) struct_init_list(Fields) RBRACE. {
    PLOG("Parsing anonymous struct literal");
    E = ast_alloc(parse, TICK_AST_STRUCT_INIT_EXPR, T.line, T.col);
    E->data.struct_init_expr.type = NULL;
    E->data.struct_init_expr.fields = Fields;
}

// Struct initializer field list - builds linked list
struct_init_list(L) ::= . { L = NULL; }
struct_init_list(L) ::= struct_init_field(F). { L = F; L->next = NULL; }
struct_init_list(L) ::= struct_init_list(Prev) COMMA struct_init_field(F). { L = F; L->next = Prev; }
struct_init_list(L) ::= struct_init_list(Prev) COMMA. { L = Prev; }  // Trailing comma

// Struct initializer field
struct_init_field(F) ::= name(Name) COLON expr(Val). {
    PLOG("Parsing struct init field");
    F = ast_alloc(parse, TICK_AST_STRUCT_INIT_FIELD, Name.line, Name.col);
    F->data.struct_init_field.field_name = Name.text;
    F->data.struct_init_field.value = Val;
}

// Array literal: [1, 2, 3, 4]
expr(E) ::= LBRACKET(T) expr_list(Elements) RBRACKET. [COMMA] {
    PLOG("Parsing array literal");
    E = ast_alloc(parse, TICK_AST_ARRAY_INIT_EXPR, T.line, T.col);
    E->data.array_init_expr.elements = Elements;
}

// Type cast: val.(i64)
expr(E) ::= expr(Operand) DOT(T) LPAREN type(Ty) RPAREN. {
    PLOG("Parsing type cast");
    E = ast_alloc(parse, TICK_AST_CAST_EXPR, T.line, T.col);
    E->data.cast_expr.type = Ty; E->data.cast_expr.expr = Operand;
}

// Type cast with 'as': val as i64
expr(E) ::= expr(Operand) AS(T) type(Ty). {
    PLOG("Parsing as cast");
    E = ast_alloc(parse, TICK_AST_CAST_EXPR, T.line, T.col);
    E->data.cast_expr.type = Ty; E->data.cast_expr.expr = Operand;
}

// Expression list - builds linked list
expr_list(L) ::= . { L = NULL; }
expr_list(L) ::= expr(E). { L = E; L->next = NULL; }
expr_list(L) ::= expr_list(Prev) COMMA expr(E). { L = E; L->next = Prev; }
expr_list(L) ::= expr_list(Prev) COMMA. { L = Prev; }  // Trailing comma

// =============================================================================
// Precedence (lowest to highest)
// =============================================================================
%left PREC_RETURN_TYPE.
%left COMMA.
%right EQ.
%nonassoc ORELSE AS.
%nonassoc LT GT LT_EQ GT_EQ EQ_EQ BANG_EQ OR AND PIPE CARET AMPERSAND.
%left PLUS MINUS PLUS_PIPE MINUS_PIPE PLUS_PERCENT MINUS_PERCENT.
%left STAR SLASH PERCENT STAR_PIPE SLASH_PIPE STAR_PERCENT SLASH_PERCENT LSHIFT RSHIFT.
%right QUESTION.
%right BANG TILDE PREC_UNARY_MINUS PREC_UNARY_AMPERSAND PREC_UNARY_STAR.
%left DOT DOT_QUESTION LBRACKET LPAREN.
