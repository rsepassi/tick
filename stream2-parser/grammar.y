// Lemon grammar for async systems language
// LALR(1) parser specification

%include {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../interfaces2/parser.h"
#include "../interfaces2/ast.h"
#include "../interfaces2/lexer.h"
#include "../interfaces2/arena.h"

// Forward declare list types from parser.c
typedef struct AstNodeList {
    AstNode** nodes;
    size_t count;
    size_t capacity;
} AstNodeList;

typedef struct AstParamList {
    AstParam* params;
    size_t count;
    size_t capacity;
} AstParamList;

typedef struct AstSwitchCaseList {
    AstSwitchCase* cases;
    size_t count;
    size_t capacity;
} AstSwitchCaseList;

// Forward declare helper functions from parser.c
AstNodeList* node_list_create(Parser* parser);
void node_list_append(Parser* parser, AstNodeList* list, AstNode* node);
AstParamList* param_list_create(Parser* parser);
void param_list_append(Parser* parser, AstParamList* list, const char* name, AstNode* type, SourceLocation loc);
AstSwitchCaseList* switch_case_list_create(Parser* parser);
void switch_case_list_append(Parser* parser, AstSwitchCaseList* list, AstNode** values, size_t value_count, AstNode** stmts, size_t stmt_count, SourceLocation loc);
}

%token_prefix TOKEN_
%token_type { Token }
%extra_argument { Parser* parser }

// Default type for non-terminals
%default_type { AstNode* }

// Start symbol
%start_symbol module

// Precedence and associativity (lowest to highest)
%left COMMA.
%right EQ PLUS_EQ MINUS_EQ STAR_EQ SLASH_EQ.
%left PIPE_PIPE.
%left AMP_AMP.
%left PIPE.
%left CARET.
%left AMPERSAND.
%left EQ_EQ BANG_EQ.
%left LT GT LT_EQ GT_EQ.
%left LSHIFT RSHIFT.
%left PLUS MINUS.
%left STAR SLASH PERCENT.
%right BANG TILDE UNARY_MINUS UNARY_AMPERSAND UNARY_STAR.
%left DOT ARROW LBRACKET LPAREN.

// Module
module(M) ::= decl_list(D). {
    AstNodeList* list = (AstNodeList*)D;
    M = ast_alloc(parser, AST_MODULE, list->count > 0 ? list->nodes[0]->loc : (SourceLocation){1, 1, ""});
    if (M) {
        M->data.module.name = "main";
        M->data.module.decls = list->nodes;
        M->data.module.decl_count = list->count;
    }
    parser->root = M;
}

module(M) ::= . {
    M = ast_alloc(parser, AST_MODULE, (SourceLocation){0, 0, ""});
    if (M) {
        M->data.module.name = "main";
        M->data.module.decls = NULL;
        M->data.module.decl_count = 0;
    }
    parser->root = M;
}

// Declaration list type
%type decl_list { AstNodeList* }
%type param_list { AstParamList* }
%type stmt_list { void* }
%type expr_list { void* }
%type field_list { void* }
%type enum_value_list { void* }
%type case_list { void* }
%type case_clause { void* }
%type case_value_list { void* }
%type struct_init_list { void* }

decl_list(L) ::= decl(D). {
    L = node_list_create(parser);
    node_list_append(parser, L, D);
}

decl_list(L) ::= decl_list(Prev) decl(D). {
    L = Prev;
    node_list_append(parser, L, D);
}

// Declarations (all use let/var syntax)
decl(D) ::= import_decl(I). { D = I; }
decl(D) ::= let_decl(L). { D = L; }
decl(D) ::= var_decl(V). { D = V; }

// Import declaration
import_decl(I) ::= LET(T) IDENTIFIER(Name) EQ IMPORT STRING_LITERAL(Path) SEMICOLON. {
    I = ast_alloc(parser, AST_IMPORT_DECL, (SourceLocation){T.line, T.column, T.start});
    if (I) {
        I->data.import_decl.name = Name.literal.str_value;
        I->data.import_decl.path = Path.literal.str_value;
        I->data.import_decl.is_pub = false;
    }
}

import_decl(I) ::= PUB LET(T) IDENTIFIER(Name) EQ IMPORT STRING_LITERAL(Path) SEMICOLON. {
    I = ast_alloc(parser, AST_IMPORT_DECL, (SourceLocation){T.line, T.column, T.start});
    if (I) {
        I->data.import_decl.name = Name.literal.str_value;
        I->data.import_decl.path = Path.literal.str_value;
        I->data.import_decl.is_pub = true;
    }
}

// Let declarations
let_decl(D) ::= LET(T) IDENTIFIER(Name) EQ function_def(F) SEMICOLON. {
    D = F;
    if (D) {
        D->data.function_decl.name = Name.literal.str_value;
        D->data.function_decl.is_pub = false;
        D->loc = (SourceLocation){T.line, T.column, T.start};
    }
}

let_decl(D) ::= PUB LET(T) IDENTIFIER(Name) EQ function_def(F) SEMICOLON. {
    D = F;
    if (D) {
        D->data.function_decl.name = Name.literal.str_value;
        D->data.function_decl.is_pub = true;
        D->loc = (SourceLocation){T.line, T.column, T.start};
    }
}

let_decl(D) ::= LET(T) IDENTIFIER(Name) EQ struct_def(S) SEMICOLON. {
    D = S;
    if (D) {
        D->data.struct_decl.name = Name.literal.str_value;
        D->data.struct_decl.is_pub = false;
        D->loc = (SourceLocation){T.line, T.column, T.start};
    }
}

let_decl(D) ::= PUB LET(T) IDENTIFIER(Name) EQ struct_def(S) SEMICOLON. {
    D = S;
    if (D) {
        D->data.struct_decl.name = Name.literal.str_value;
        D->data.struct_decl.is_pub = true;
        D->loc = (SourceLocation){T.line, T.column, T.start};
    }
}

let_decl(D) ::= LET(T) IDENTIFIER(Name) EQ enum_def(E) SEMICOLON. {
    D = E;
    if (D) {
        D->data.enum_decl.name = Name.literal.str_value;
        D->data.enum_decl.is_pub = false;
        D->loc = (SourceLocation){T.line, T.column, T.start};
    }
}

let_decl(D) ::= PUB LET(T) IDENTIFIER(Name) EQ enum_def(E) SEMICOLON. {
    D = E;
    if (D) {
        D->data.enum_decl.name = Name.literal.str_value;
        D->data.enum_decl.is_pub = true;
        D->loc = (SourceLocation){T.line, T.column, T.start};
    }
}

let_decl(D) ::= LET(T) IDENTIFIER(Name) EQ union_def(U) SEMICOLON. {
    D = U;
    if (D) {
        D->data.union_decl.name = Name.literal.str_value;
        D->data.union_decl.is_pub = false;
        D->loc = (SourceLocation){T.line, T.column, T.start};
    }
}

let_decl(D) ::= PUB LET(T) IDENTIFIER(Name) EQ union_def(U) SEMICOLON. {
    D = U;
    if (D) {
        D->data.union_decl.name = Name.literal.str_value;
        D->data.union_decl.is_pub = true;
        D->loc = (SourceLocation){T.line, T.column, T.start};
    }
}

let_decl(D) ::= LET(T) IDENTIFIER(Name) EQ expr(E) SEMICOLON. {
    D = ast_alloc(parser, AST_LET_DECL, (SourceLocation){T.line, T.column, T.start});
    if (D) {
        D->data.let_decl.name = Name.literal.str_value;
        D->data.let_decl.type = NULL;
        D->data.let_decl.init = E;
        D->data.let_decl.is_pub = false;
    }
}

let_decl(D) ::= PUB LET(T) IDENTIFIER(Name) EQ expr(E) SEMICOLON. {
    D = ast_alloc(parser, AST_LET_DECL, (SourceLocation){T.line, T.column, T.start});
    if (D) {
        D->data.let_decl.name = Name.literal.str_value;
        D->data.let_decl.type = NULL;
        D->data.let_decl.init = E;
        D->data.let_decl.is_pub = true;
    }
}

let_decl(D) ::= LET(T) IDENTIFIER(Name) COLON type(Ty) EQ expr(E) SEMICOLON. {
    D = ast_alloc(parser, AST_LET_DECL, (SourceLocation){T.line, T.column, T.start});
    if (D) {
        D->data.let_decl.name = Name.literal.str_value;
        D->data.let_decl.type = Ty;
        D->data.let_decl.init = E;
        D->data.let_decl.is_pub = false;
    }
}

// Var declarations
var_decl(D) ::= VAR(T) IDENTIFIER(Name) COLON type(Ty) SEMICOLON. {
    D = ast_alloc(parser, AST_VAR_DECL, (SourceLocation){T.line, T.column, T.start});
    if (D) {
        D->data.var_decl.name = Name.literal.str_value;
        D->data.var_decl.type = Ty;
        D->data.var_decl.init = NULL;
        D->data.var_decl.is_volatile = false;
        D->data.var_decl.is_pub = false;
    }
}

var_decl(D) ::= VAR(T) IDENTIFIER(Name) COLON type(Ty) EQ expr(E) SEMICOLON. {
    D = ast_alloc(parser, AST_VAR_DECL, (SourceLocation){T.line, T.column, T.start});
    if (D) {
        D->data.var_decl.name = Name.literal.str_value;
        D->data.var_decl.type = Ty;
        D->data.var_decl.init = E;
        D->data.var_decl.is_volatile = false;
        D->data.var_decl.is_pub = false;
    }
}

var_decl(D) ::= VAR(T) IDENTIFIER(Name) EQ expr(E) SEMICOLON. {
    D = ast_alloc(parser, AST_VAR_DECL, (SourceLocation){T.line, T.column, T.start});
    if (D) {
        D->data.var_decl.name = Name.literal.str_value;
        D->data.var_decl.type = NULL;
        D->data.var_decl.init = E;
        D->data.var_decl.is_volatile = false;
        D->data.var_decl.is_pub = false;
    }
}

var_decl(D) ::= VOLATILE VAR(T) IDENTIFIER(Name) COLON type(Ty) SEMICOLON. {
    D = ast_alloc(parser, AST_VAR_DECL, (SourceLocation){T.line, T.column, T.start});
    if (D) {
        D->data.var_decl.name = Name.literal.str_value;
        D->data.var_decl.type = Ty;
        D->data.var_decl.init = NULL;
        D->data.var_decl.is_volatile = true;
        D->data.var_decl.is_pub = false;
    }
}

// Function definition
function_def(F) ::= FN(T) LPAREN param_list(P) RPAREN block(B). {
    F = ast_alloc(parser, AST_FUNCTION_DECL, (SourceLocation){T.line, T.column, T.start});
    if (F) {
        AstParamList* plist = (AstParamList*)P;
        F->data.function_decl.params = plist->params;
        F->data.function_decl.param_count = plist->count;
        F->data.function_decl.return_type = NULL;
        F->data.function_decl.body = B;
    }
}

function_def(F) ::= FN(T) LPAREN param_list(P) RPAREN ARROW type(Ret) block(B). {
    F = ast_alloc(parser, AST_FUNCTION_DECL, (SourceLocation){T.line, T.column, T.start});
    if (F) {
        AstParamList* plist = (AstParamList*)P;
        F->data.function_decl.params = plist->params;
        F->data.function_decl.param_count = plist->count;
        F->data.function_decl.return_type = Ret;
        F->data.function_decl.body = B;
    }
}

function_def(F) ::= FN(T) LPAREN RPAREN block(B). {
    F = ast_alloc(parser, AST_FUNCTION_DECL, (SourceLocation){T.line, T.column, T.start});
    if (F) {
        F->data.function_decl.params = NULL;
        F->data.function_decl.param_count = 0;
        F->data.function_decl.return_type = NULL;
        F->data.function_decl.body = B;
    }
}

function_def(F) ::= FN(T) LPAREN RPAREN ARROW type(Ret) block(B). {
    F = ast_alloc(parser, AST_FUNCTION_DECL, (SourceLocation){T.line, T.column, T.start});
    if (F) {
        F->data.function_decl.params = NULL;
        F->data.function_decl.param_count = 0;
        F->data.function_decl.return_type = Ret;
        F->data.function_decl.body = B;
    }
}

// Parameter list
%type param { void* }

param_list(L) ::= param(P). {
    L = param_list_create(parser);
    param_list_append(parser, L, ((AstParam*)P)->name, ((AstParam*)P)->type, ((AstParam*)P)->loc);
}

param_list(L) ::= param_list(Prev) COMMA param(P). {
    L = Prev;
    param_list_append(parser, L, ((AstParam*)P)->name, ((AstParam*)P)->type, ((AstParam*)P)->loc);
}

param(P) ::= IDENTIFIER(Name) COLON type(T). {
    // Create a temporary param struct to pass name and type
    AstParam* temp = (AstParam*)arena_alloc(parser->ast_arena, sizeof(AstParam), _Alignof(AstParam));
    temp->name = Name.literal.str_value;
    temp->type = T;
    temp->loc = (SourceLocation){Name.line, Name.column, Name.start};
    P = temp;
}

// Struct definition
struct_def(S) ::= STRUCT(T) LBRACE field_list(F) RBRACE. {
    S = ast_alloc(parser, AST_STRUCT_DECL, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.struct_decl.fields = (AstField*)F;
        S->data.struct_decl.field_count = 0; // Set by parser
        S->data.struct_decl.is_packed = false;
    }
}

struct_def(S) ::= STRUCT(T) PACKED LBRACE field_list(F) RBRACE. {
    S = ast_alloc(parser, AST_STRUCT_DECL, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.struct_decl.fields = (AstField*)F;
        S->data.struct_decl.field_count = 0; // Set by parser
        S->data.struct_decl.is_packed = true;
    }
}

struct_def(S) ::= STRUCT(T) LBRACE RBRACE. {
    S = ast_alloc(parser, AST_STRUCT_DECL, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.struct_decl.fields = NULL;
        S->data.struct_decl.field_count = 0;
        S->data.struct_decl.is_packed = false;
    }
}

// Field list
field_list(L) ::= field(F). { L = F; }
field_list(L) ::= field_list(Prev) COMMA field(F). { L = F; }

field(F) ::= IDENTIFIER(Name) COLON type(T). {
    F = ast_alloc(parser, AST_LET_DECL, (SourceLocation){Name.line, Name.column, Name.literal.str_value});
    // Store as AstField later
}

// Enum definition
enum_def(E) ::= ENUM(T) LPAREN type(Ty) RPAREN LBRACE enum_value_list(V) RBRACE. {
    E = ast_alloc(parser, AST_ENUM_DECL, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.enum_decl.underlying_type = Ty;
        E->data.enum_decl.values = (AstEnumValue*)V;
        E->data.enum_decl.value_count = 0; // Set by parser
    }
}

enum_value_list(L) ::= enum_value(V). { L = V; }
enum_value_list(L) ::= enum_value_list(Prev) COMMA enum_value(V). { L = V; }

enum_value(V) ::= IDENTIFIER(Name). {
    V = ast_alloc(parser, AST_LET_DECL, (SourceLocation){Name.line, Name.column, Name.literal.str_value});
    // Store as AstEnumValue later
}

enum_value(V) ::= IDENTIFIER(Name) EQ expr(E). {
    V = ast_alloc(parser, AST_LET_DECL, (SourceLocation){Name.line, Name.column, Name.literal.str_value});
    // Store as AstEnumValue later with expression
}

// Union definition
union_def(U) ::= UNION(T) LBRACE field_list(F) RBRACE. {
    U = ast_alloc(parser, AST_UNION_DECL, (SourceLocation){T.line, T.column, T.start});
    if (U) {
        U->data.union_decl.tag_type = NULL;
        U->data.union_decl.fields = (AstField*)F;
        U->data.union_decl.field_count = 0; // Set by parser
    }
}

union_def(U) ::= UNION(T) LPAREN type(Tag) RPAREN LBRACE field_list(F) RBRACE. {
    U = ast_alloc(parser, AST_UNION_DECL, (SourceLocation){T.line, T.column, T.start});
    if (U) {
        U->data.union_decl.tag_type = Tag;
        U->data.union_decl.fields = (AstField*)F;
        U->data.union_decl.field_count = 0; // Set by parser
    }
}

// Statements
stmt(S) ::= let_stmt(L). { S = L; }
stmt(S) ::= var_stmt(V). { S = V; }
stmt(S) ::= assign_stmt(A). { S = A; }
stmt(S) ::= return_stmt(R). { S = R; }
stmt(S) ::= if_stmt(I). { S = I; }
stmt(S) ::= for_stmt(F). { S = F; }
stmt(S) ::= switch_stmt(Sw). { S = Sw; }
stmt(S) ::= break_stmt(B). { S = B; }
stmt(S) ::= continue_stmt(C). { S = C; }
stmt(S) ::= continue_switch_stmt(C). { S = C; }
stmt(S) ::= defer_stmt(D). { S = D; }
stmt(S) ::= errdefer_stmt(E). { S = E; }
stmt(S) ::= suspend_stmt(Su). { S = Su; }
stmt(S) ::= resume_stmt(R). { S = R; }
stmt(S) ::= try_catch_stmt(T). { S = T; }
stmt(S) ::= block(B). { S = B; }
stmt(S) ::= expr_stmt(E). { S = E; }

// Let statement
let_stmt(S) ::= LET(T) IDENTIFIER(Name) EQ expr(E) SEMICOLON. {
    S = ast_alloc(parser, AST_LET_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.let_decl.name = Name.literal.str_value;
        S->data.let_decl.type = NULL;
        S->data.let_decl.init = E;
    }
}

let_stmt(S) ::= LET(T) IDENTIFIER(Name) COLON type(Ty) EQ expr(E) SEMICOLON. {
    S = ast_alloc(parser, AST_LET_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.let_decl.name = Name.literal.str_value;
        S->data.let_decl.type = Ty;
        S->data.let_decl.init = E;
    }
}

// Var statement
var_stmt(S) ::= VAR(T) IDENTIFIER(Name) COLON type(Ty) SEMICOLON. {
    S = ast_alloc(parser, AST_VAR_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.var_decl.name = Name.literal.str_value;
        S->data.var_decl.type = Ty;
        S->data.var_decl.init = NULL;
        S->data.var_decl.is_volatile = false;
    }
}

var_stmt(S) ::= VAR(T) IDENTIFIER(Name) COLON type(Ty) EQ expr(E) SEMICOLON. {
    S = ast_alloc(parser, AST_VAR_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.var_decl.name = Name.literal.str_value;
        S->data.var_decl.type = Ty;
        S->data.var_decl.init = E;
        S->data.var_decl.is_volatile = false;
    }
}

var_stmt(S) ::= VAR(T) IDENTIFIER(Name) EQ expr(E) SEMICOLON. {
    S = ast_alloc(parser, AST_VAR_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.var_decl.name = Name.literal.str_value;
        S->data.var_decl.type = NULL;
        S->data.var_decl.init = E;
        S->data.var_decl.is_volatile = false;
    }
}

// Assignment statement
assign_stmt(S) ::= expr(L) EQ(T) expr(R) SEMICOLON. {
    S = ast_alloc(parser, AST_ASSIGN_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.assign_stmt.lhs = L;
        S->data.assign_stmt.op = ASSIGN_SIMPLE;
        S->data.assign_stmt.rhs = R;
    }
}

assign_stmt(S) ::= expr(L) PLUS_EQ(T) expr(R) SEMICOLON. {
    S = ast_alloc(parser, AST_ASSIGN_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.assign_stmt.lhs = L;
        S->data.assign_stmt.op = ASSIGN_ADD;
        S->data.assign_stmt.rhs = R;
    }
}

assign_stmt(S) ::= expr(L) MINUS_EQ(T) expr(R) SEMICOLON. {
    S = ast_alloc(parser, AST_ASSIGN_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.assign_stmt.lhs = L;
        S->data.assign_stmt.op = ASSIGN_SUB;
        S->data.assign_stmt.rhs = R;
    }
}

assign_stmt(S) ::= expr(L) STAR_EQ(T) expr(R) SEMICOLON. {
    S = ast_alloc(parser, AST_ASSIGN_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.assign_stmt.lhs = L;
        S->data.assign_stmt.op = ASSIGN_MUL;
        S->data.assign_stmt.rhs = R;
    }
}

assign_stmt(S) ::= expr(L) SLASH_EQ(T) expr(R) SEMICOLON. {
    S = ast_alloc(parser, AST_ASSIGN_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.assign_stmt.lhs = L;
        S->data.assign_stmt.op = ASSIGN_DIV;
        S->data.assign_stmt.rhs = R;
    }
}

// Return statement
return_stmt(S) ::= RETURN(T) SEMICOLON. {
    S = ast_alloc(parser, AST_RETURN_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.return_stmt.value = NULL;
    }
}

return_stmt(S) ::= RETURN(T) expr(E) SEMICOLON. {
    S = ast_alloc(parser, AST_RETURN_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.return_stmt.value = E;
    }
}

// If statement
if_stmt(S) ::= IF(T) expr(Cond) block(Then). {
    S = ast_alloc(parser, AST_IF_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.if_stmt.condition = Cond;
        S->data.if_stmt.then_block = Then;
        S->data.if_stmt.else_block = NULL;
    }
}

if_stmt(S) ::= IF(T) expr(Cond) block(Then) ELSE block(Else). {
    S = ast_alloc(parser, AST_IF_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.if_stmt.condition = Cond;
        S->data.if_stmt.then_block = Then;
        S->data.if_stmt.else_block = Else;
    }
}

if_stmt(S) ::= IF(T) expr(Cond) block(Then) ELSE if_stmt(Elif). {
    S = ast_alloc(parser, AST_IF_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.if_stmt.condition = Cond;
        S->data.if_stmt.then_block = Then;
        S->data.if_stmt.else_block = Elif;
    }
}

// For loops (Go-style)
for_stmt(S) ::= FOR(T) IDENTIFIER(Var) IN expr(Iter) block(Body). {
    S = ast_alloc(parser, AST_FOR_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.for_stmt.loop_var = Var.start;
        S->data.for_stmt.iterable = Iter;
        S->data.for_stmt.condition = NULL;
        S->data.for_stmt.continue_expr = NULL;
        S->data.for_stmt.body = Body;
    }
}

for_stmt(S) ::= FOR(T) IDENTIFIER(Var) IN expr(Iter) COLON expr(Cont) block(Body). {
    S = ast_alloc(parser, AST_FOR_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.for_stmt.loop_var = Var.start;
        S->data.for_stmt.iterable = Iter;
        S->data.for_stmt.condition = NULL;
        S->data.for_stmt.continue_expr = Cont;
        S->data.for_stmt.body = Body;
    }
}

for_stmt(S) ::= FOR(T) expr(Cond) block(Body). {
    S = ast_alloc(parser, AST_FOR_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.for_stmt.loop_var = NULL;
        S->data.for_stmt.iterable = NULL;
        S->data.for_stmt.condition = Cond;
        S->data.for_stmt.continue_expr = NULL;
        S->data.for_stmt.body = Body;
    }
}

for_stmt(S) ::= FOR(T) block(Body). {
    S = ast_alloc(parser, AST_FOR_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.for_stmt.loop_var = NULL;
        S->data.for_stmt.iterable = NULL;
        S->data.for_stmt.condition = NULL;
        S->data.for_stmt.continue_expr = NULL;
        S->data.for_stmt.body = Body;
    }
}

for_stmt(S) ::= FOR(T) COLON expr(Cont) block(Body). {
    S = ast_alloc(parser, AST_FOR_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.for_stmt.loop_var = NULL;
        S->data.for_stmt.iterable = NULL;
        S->data.for_stmt.condition = NULL;
        S->data.for_stmt.continue_expr = Cont;
        S->data.for_stmt.body = Body;
    }
}

// Switch statement (parentheses required around expression to avoid ambiguity with struct literals)
switch_stmt(S) ::= SWITCH(T) LPAREN expr(Val) RPAREN LBRACE case_list(Cases) RBRACE. {
    S = ast_alloc(parser, AST_SWITCH_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        AstSwitchCaseList* case_list = (AstSwitchCaseList*)Cases;
        S->data.switch_stmt.value = Val;
        S->data.switch_stmt.cases = case_list->cases;
        S->data.switch_stmt.case_count = case_list->count;
        S->data.switch_stmt.is_while_switch = false;
    }
}

switch_stmt(S) ::= WHILE SWITCH(T) LPAREN expr(Val) RPAREN LBRACE case_list(Cases) RBRACE. {
    S = ast_alloc(parser, AST_SWITCH_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        AstSwitchCaseList* case_list = (AstSwitchCaseList*)Cases;
        S->data.switch_stmt.value = Val;
        S->data.switch_stmt.cases = case_list->cases;
        S->data.switch_stmt.case_count = case_list->count;
        S->data.switch_stmt.is_while_switch = true;
    }
}

case_list(L) ::= case_clause(C). {
    L = switch_case_list_create(parser);
    AstSwitchCase* case_ptr = (AstSwitchCase*)C;
    switch_case_list_append(parser, L, case_ptr->values, case_ptr->value_count,
                           case_ptr->stmts, case_ptr->stmt_count, case_ptr->loc);
}

case_list(L) ::= case_list(Prev) case_clause(C). {
    L = Prev;
    AstSwitchCase* case_ptr = (AstSwitchCase*)C;
    switch_case_list_append(parser, L, case_ptr->values, case_ptr->value_count,
                           case_ptr->stmts, case_ptr->stmt_count, case_ptr->loc);
}

case_clause(C) ::= CASE(T) case_value_list(V) COLON stmt_list(S). {
    // Create a temporary AstSwitchCase struct
    AstSwitchCase* temp = (AstSwitchCase*)arena_alloc(parser->ast_arena, sizeof(AstSwitchCase), _Alignof(AstSwitchCase));
    if (temp) {
        AstNodeList* value_list = (AstNodeList*)V;
        AstNodeList* stmt_list = (AstNodeList*)S;
        temp->values = value_list->nodes;
        temp->value_count = value_list->count;
        temp->stmts = stmt_list->nodes;
        temp->stmt_count = stmt_list->count;
        temp->loc = (SourceLocation){T.line, T.column, T.start};
    }
    C = temp;
}

case_clause(C) ::= DEFAULT(T) COLON stmt_list(S). {
    // Create a temporary AstSwitchCase struct with NULL values for default
    AstSwitchCase* temp = (AstSwitchCase*)arena_alloc(parser->ast_arena, sizeof(AstSwitchCase), _Alignof(AstSwitchCase));
    if (temp) {
        AstNodeList* stmt_list = (AstNodeList*)S;
        temp->values = NULL;
        temp->value_count = 0;
        temp->stmts = stmt_list->nodes;
        temp->stmt_count = stmt_list->count;
        temp->loc = (SourceLocation){T.line, T.column, T.start};
    }
    C = temp;
}

case_value_list(L) ::= expr(E). {
    L = node_list_create(parser);
    node_list_append(parser, L, E);
}

case_value_list(L) ::= case_value_list(Prev) COMMA expr(E). {
    L = Prev;
    node_list_append(parser, L, E);
}

// Control flow
break_stmt(S) ::= BREAK(T) SEMICOLON. {
    S = ast_alloc(parser, AST_BREAK_STMT, (SourceLocation){T.line, T.column, T.start});
}

continue_stmt(S) ::= CONTINUE(T) SEMICOLON. {
    S = ast_alloc(parser, AST_CONTINUE_STMT, (SourceLocation){T.line, T.column, T.start});
}

continue_switch_stmt(S) ::= CONTINUE SWITCH(T) LPAREN expr(E) RPAREN SEMICOLON. {
    S = ast_alloc(parser, AST_CONTINUE_SWITCH_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.continue_switch_stmt.value = E;
    }
}

// Defer/errdefer
defer_stmt(S) ::= DEFER(T) stmt(St). {
    S = ast_alloc(parser, AST_DEFER_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.defer_stmt.stmt = St;
    }
}

errdefer_stmt(S) ::= ERRDEFER(T) stmt(St). {
    S = ast_alloc(parser, AST_ERRDEFER_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.defer_stmt.stmt = St;
    }
}

// Async/suspend/resume
suspend_stmt(S) ::= SUSPEND(T) SEMICOLON. {
    S = ast_alloc(parser, AST_SUSPEND_STMT, (SourceLocation){T.line, T.column, T.start});
}

resume_stmt(S) ::= RESUME(T) expr(E) SEMICOLON. {
    S = ast_alloc(parser, AST_RESUME_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.resume_stmt.coro = E;
    }
}

// Try/catch
try_catch_stmt(S) ::= TRY(T) block(Try). {
    S = ast_alloc(parser, AST_TRY_CATCH_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.try_catch_stmt.try_block = Try;
        S->data.try_catch_stmt.error_var = NULL;
        S->data.try_catch_stmt.catch_block = NULL;
    }
}

try_catch_stmt(S) ::= TRY(T) block(Try) CATCH PIPE IDENTIFIER(Err) PIPE block(Catch). {
    S = ast_alloc(parser, AST_TRY_CATCH_STMT, (SourceLocation){T.line, T.column, T.start});
    if (S) {
        S->data.try_catch_stmt.try_block = Try;
        S->data.try_catch_stmt.error_var = Err.start;
        S->data.try_catch_stmt.catch_block = Catch;
    }
}

// Block
block(B) ::= LBRACE(T) stmt_list(S) RBRACE. {
    AstNodeList* list = (AstNodeList*)S;
    B = ast_alloc(parser, AST_BLOCK_STMT, (SourceLocation){T.line, T.column, T.start});
    if (B) {
        B->data.block_stmt.stmts = list->nodes;
        B->data.block_stmt.stmt_count = list->count;
    }
}

block(B) ::= LBRACE(T) RBRACE. {
    B = ast_alloc(parser, AST_BLOCK_STMT, (SourceLocation){T.line, T.column, T.start});
    if (B) {
        B->data.block_stmt.stmts = NULL;
        B->data.block_stmt.stmt_count = 0;
    }
}

stmt_list(L) ::= stmt(S). {
    L = node_list_create(parser);
    node_list_append(parser, L, S);
}

stmt_list(L) ::= stmt_list(Prev) stmt(S). {
    L = Prev;
    node_list_append(parser, L, S);
}

// Expression statement
expr_stmt(S) ::= expr(E) SEMICOLON. {
    S = ast_alloc(parser, AST_EXPR_STMT, E->loc);
    if (S) {
        S->data.expr_stmt.expr = E;
    }
}

// Expressions
expr(E) ::= binary_expr(B). { E = B; }
expr(E) ::= unary_expr(U). { E = U; }
expr(E) ::= postfix_expr(P). { E = P; }
expr(E) ::= primary_expr(Pr). { E = Pr; }

// Binary expressions
binary_expr(E) ::= expr(L) PLUS(T) expr(R). {
    E = ast_alloc(parser, AST_BINARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.binary_expr.op = BINOP_ADD;
        E->data.binary_expr.left = L;
        E->data.binary_expr.right = R;
    }
}

binary_expr(E) ::= expr(L) MINUS(T) expr(R). {
    E = ast_alloc(parser, AST_BINARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.binary_expr.op = BINOP_SUB;
        E->data.binary_expr.left = L;
        E->data.binary_expr.right = R;
    }
}

binary_expr(E) ::= expr(L) STAR(T) expr(R). {
    E = ast_alloc(parser, AST_BINARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.binary_expr.op = BINOP_MUL;
        E->data.binary_expr.left = L;
        E->data.binary_expr.right = R;
    }
}

binary_expr(E) ::= expr(L) SLASH(T) expr(R). {
    E = ast_alloc(parser, AST_BINARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.binary_expr.op = BINOP_DIV;
        E->data.binary_expr.left = L;
        E->data.binary_expr.right = R;
    }
}

binary_expr(E) ::= expr(L) PERCENT(T) expr(R). {
    E = ast_alloc(parser, AST_BINARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.binary_expr.op = BINOP_MOD;
        E->data.binary_expr.left = L;
        E->data.binary_expr.right = R;
    }
}

binary_expr(E) ::= expr(L) AMPERSAND(T) expr(R). {
    E = ast_alloc(parser, AST_BINARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.binary_expr.op = BINOP_AND;
        E->data.binary_expr.left = L;
        E->data.binary_expr.right = R;
    }
}

binary_expr(E) ::= expr(L) PIPE(T) expr(R). {
    E = ast_alloc(parser, AST_BINARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.binary_expr.op = BINOP_OR;
        E->data.binary_expr.left = L;
        E->data.binary_expr.right = R;
    }
}

binary_expr(E) ::= expr(L) CARET(T) expr(R). {
    E = ast_alloc(parser, AST_BINARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.binary_expr.op = BINOP_XOR;
        E->data.binary_expr.left = L;
        E->data.binary_expr.right = R;
    }
}

binary_expr(E) ::= expr(L) LSHIFT(T) expr(R). {
    E = ast_alloc(parser, AST_BINARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.binary_expr.op = BINOP_LSHIFT;
        E->data.binary_expr.left = L;
        E->data.binary_expr.right = R;
    }
}

binary_expr(E) ::= expr(L) RSHIFT(T) expr(R). {
    E = ast_alloc(parser, AST_BINARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.binary_expr.op = BINOP_RSHIFT;
        E->data.binary_expr.left = L;
        E->data.binary_expr.right = R;
    }
}

binary_expr(E) ::= expr(L) AMP_AMP(T) expr(R). {
    E = ast_alloc(parser, AST_BINARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.binary_expr.op = BINOP_LOGICAL_AND;
        E->data.binary_expr.left = L;
        E->data.binary_expr.right = R;
    }
}

binary_expr(E) ::= expr(L) PIPE_PIPE(T) expr(R). {
    E = ast_alloc(parser, AST_BINARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.binary_expr.op = BINOP_LOGICAL_OR;
        E->data.binary_expr.left = L;
        E->data.binary_expr.right = R;
    }
}

binary_expr(E) ::= expr(L) LT(T) expr(R). {
    E = ast_alloc(parser, AST_BINARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.binary_expr.op = BINOP_LT;
        E->data.binary_expr.left = L;
        E->data.binary_expr.right = R;
    }
}

binary_expr(E) ::= expr(L) GT(T) expr(R). {
    E = ast_alloc(parser, AST_BINARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.binary_expr.op = BINOP_GT;
        E->data.binary_expr.left = L;
        E->data.binary_expr.right = R;
    }
}

binary_expr(E) ::= expr(L) LT_EQ(T) expr(R). {
    E = ast_alloc(parser, AST_BINARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.binary_expr.op = BINOP_LT_EQ;
        E->data.binary_expr.left = L;
        E->data.binary_expr.right = R;
    }
}

binary_expr(E) ::= expr(L) GT_EQ(T) expr(R). {
    E = ast_alloc(parser, AST_BINARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.binary_expr.op = BINOP_GT_EQ;
        E->data.binary_expr.left = L;
        E->data.binary_expr.right = R;
    }
}

binary_expr(E) ::= expr(L) EQ_EQ(T) expr(R). {
    E = ast_alloc(parser, AST_BINARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.binary_expr.op = BINOP_EQ_EQ;
        E->data.binary_expr.left = L;
        E->data.binary_expr.right = R;
    }
}

binary_expr(E) ::= expr(L) BANG_EQ(T) expr(R). {
    E = ast_alloc(parser, AST_BINARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.binary_expr.op = BINOP_BANG_EQ;
        E->data.binary_expr.left = L;
        E->data.binary_expr.right = R;
    }
}

// Unary expressions
unary_expr(E) ::= MINUS(T) expr(Operand). [UNARY_MINUS] {
    E = ast_alloc(parser, AST_UNARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.unary_expr.op = UNOP_NEG;
        E->data.unary_expr.operand = Operand;
    }
}

unary_expr(E) ::= BANG(T) expr(Operand). {
    E = ast_alloc(parser, AST_UNARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.unary_expr.op = UNOP_NOT;
        E->data.unary_expr.operand = Operand;
    }
}

unary_expr(E) ::= TILDE(T) expr(Operand). {
    E = ast_alloc(parser, AST_UNARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.unary_expr.op = UNOP_BIT_NOT;
        E->data.unary_expr.operand = Operand;
    }
}

unary_expr(E) ::= AMPERSAND(T) expr(Operand). [UNARY_AMPERSAND] {
    E = ast_alloc(parser, AST_UNARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.unary_expr.op = UNOP_ADDR;
        E->data.unary_expr.operand = Operand;
    }
}

unary_expr(E) ::= STAR(T) expr(Operand). [UNARY_STAR] {
    E = ast_alloc(parser, AST_UNARY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.unary_expr.op = UNOP_DEREF;
        E->data.unary_expr.operand = Operand;
    }
}

unary_expr(E) ::= TRY(T) expr(Operand). {
    E = ast_alloc(parser, AST_TRY_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.try_expr.expr = Operand;
    }
}

// Postfix expressions
postfix_expr(E) ::= expr(Callee) LPAREN(T) expr_list(Args) RPAREN. {
    E = ast_alloc(parser, AST_CALL_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.call_expr.callee = Callee;
        E->data.call_expr.args = (AstNode**)Args;
        E->data.call_expr.arg_count = 0; // Set by parser
    }
}

postfix_expr(E) ::= expr(Callee) LPAREN(T) RPAREN. {
    E = ast_alloc(parser, AST_CALL_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.call_expr.callee = Callee;
        E->data.call_expr.args = NULL;
        E->data.call_expr.arg_count = 0;
    }
}

postfix_expr(E) ::= ASYNC(T) expr(Callee) LPAREN expr_list(Args) RPAREN. {
    E = ast_alloc(parser, AST_ASYNC_CALL_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.async_call_expr.callee = Callee;
        E->data.async_call_expr.args = (AstNode**)Args;
        E->data.async_call_expr.arg_count = 0; // Set by parser
    }
}

postfix_expr(E) ::= ASYNC(T) expr(Callee) LPAREN RPAREN. {
    E = ast_alloc(parser, AST_ASYNC_CALL_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.async_call_expr.callee = Callee;
        E->data.async_call_expr.args = NULL;
        E->data.async_call_expr.arg_count = 0;
    }
}

postfix_expr(E) ::= expr(Obj) DOT(T) IDENTIFIER(Field). {
    E = ast_alloc(parser, AST_FIELD_ACCESS_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.field_access_expr.object = Obj;
        E->data.field_access_expr.field_name = Field.literal.str_value;
        E->data.field_access_expr.is_arrow = false;
    }
}

postfix_expr(E) ::= expr(Obj) ARROW(T) IDENTIFIER(Field). {
    E = ast_alloc(parser, AST_FIELD_ACCESS_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.field_access_expr.object = Obj;
        E->data.field_access_expr.field_name = Field.literal.str_value;
        E->data.field_access_expr.is_arrow = true;
    }
}

postfix_expr(E) ::= expr(Arr) LBRACKET(T) expr(Idx) RBRACKET. {
    E = ast_alloc(parser, AST_INDEX_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.index_expr.array = Arr;
        E->data.index_expr.index = Idx;
    }
}

// Struct literal with explicit type name (e.g., Point { x: 1, y: 2 })
// NOTE: Now unambiguous since switch statements require parentheses: switch (x) { ... }
postfix_expr(E) ::= IDENTIFIER(Name) LBRACE(T) struct_init_list(Fields) RBRACE. {
    // Create a named type node for the struct type
    AstNode* type_node = ast_alloc(parser, AST_TYPE_NAMED, (SourceLocation){Name.line, Name.column, Name.literal.str_value});
    if (type_node) {
        AstTypeNode* tnode = (AstTypeNode*)type_node;
        tnode->data.named.name = Name.literal.str_value;
    }

    E = ast_alloc(parser, AST_STRUCT_INIT_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.struct_init_expr.type = type_node;
        E->data.struct_init_expr.fields = (AstStructInit*)Fields;
        E->data.struct_init_expr.field_count = 0; // Set by parser
    }
}

postfix_expr(E) ::= type(T) LBRACE struct_init_list(Fields) RBRACE. {
    E = ast_alloc(parser, AST_STRUCT_INIT_EXPR, T->loc);
    if (E) {
        E->data.struct_init_expr.type = T;
        E->data.struct_init_expr.fields = (AstStructInit*)Fields;
        E->data.struct_init_expr.field_count = 0; // Set by parser
    }
}

postfix_expr(E) ::= LBRACE(T) struct_init_list(Fields) RBRACE. {
    E = ast_alloc(parser, AST_STRUCT_INIT_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.struct_init_expr.type = NULL;
        E->data.struct_init_expr.fields = (AstStructInit*)Fields;
        E->data.struct_init_expr.field_count = 0; // Set by parser
    }
}

struct_init_list(L) ::= struct_init_field(F). { L = F; }
struct_init_list(L) ::= struct_init_list(Prev) COMMA struct_init_field(F). { L = F; }

struct_init_field(F) ::= IDENTIFIER(Name) COLON expr(E). {
    F = ast_alloc(parser, AST_LET_DECL, (SourceLocation){Name.line, Name.column, Name.literal.str_value});
    // Store as AstStructInit later
}

postfix_expr(E) ::= LBRACKET(T) expr_list(Elements) RBRACKET. {
    E = ast_alloc(parser, AST_ARRAY_INIT_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.array_init_expr.elements = (AstNode**)Elements;
        E->data.array_init_expr.element_count = 0; // Set by parser
    }
}

expr_list(L) ::= expr(E). { L = E; }
expr_list(L) ::= expr_list(Prev) COMMA expr(E). { L = E; }

// Primary expressions
primary_expr(E) ::= INT_LITERAL(T). {
    E = ast_alloc(parser, AST_LITERAL_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.literal_expr.literal_kind = LITERAL_INT;
        E->data.literal_expr.value.int_value = T.literal.int_value;
    }
}

primary_expr(E) ::= STRING_LITERAL(T). {
    E = ast_alloc(parser, AST_LITERAL_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.literal_expr.literal_kind = LITERAL_STRING;
        E->data.literal_expr.value.string.str_value = T.literal.str_value;
        E->data.literal_expr.value.string.str_length = T.literal.str_length;
    }
}

primary_expr(E) ::= BOOL_LITERAL(T). {
    E = ast_alloc(parser, AST_LITERAL_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.literal_expr.literal_kind = LITERAL_BOOL;
        // Bool value needs to be extracted from token
    }
}

primary_expr(E) ::= IDENTIFIER(T). {
    E = ast_alloc(parser, AST_IDENTIFIER_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.identifier_expr.name = T.literal.str_value;
    }
}

primary_expr(E) ::= LPAREN expr(Ex) RPAREN. {
    E = Ex; // Parenthesized expression
}

primary_expr(E) ::= expr(Operand) DOT LPAREN type(T) RPAREN. {
    E = ast_alloc(parser, AST_CAST_EXPR, T->loc);
    if (E) {
        E->data.cast_expr.type = T;
        E->data.cast_expr.expr = Operand;
    }
}

primary_expr(E) ::= expr(Start) DOT_DOT(T) expr(End). {
    E = ast_alloc(parser, AST_RANGE_EXPR, (SourceLocation){T.line, T.column, T.start});
    if (E) {
        E->data.range_expr.start = Start;
        E->data.range_expr.end = End;
    }
}

// Types
type(T) ::= IDENTIFIER(Name). {
    T = ast_alloc(parser, AST_TYPE_NAMED, (SourceLocation){Name.line, Name.column, Name.literal.str_value});
    if (T) {
        // Store type name
        AstTypeNode* tnode = (AstTypeNode*)T;
        tnode->data.named.name = Name.literal.str_value;
    }
}

type(T) ::= LBRACKET(Tok) expr(Size) RBRACKET type(Elem). {
    T = ast_alloc(parser, AST_TYPE_ARRAY, (SourceLocation){Tok.line, Tok.column, Tok.start});
    // Store array type
}

type(T) ::= LBRACKET(Tok) RBRACKET type(Elem). {
    T = ast_alloc(parser, AST_TYPE_SLICE, (SourceLocation){Tok.line, Tok.column, Tok.start});
    // Store slice type
}

type(T) ::= STAR(Tok) type(Pointee). {
    T = ast_alloc(parser, AST_TYPE_POINTER, (SourceLocation){Tok.line, Tok.column, Tok.start});
    // Store pointer type
}

type(T) ::= FN(Tok) LPAREN type_list RPAREN. {
    T = ast_alloc(parser, AST_TYPE_FUNCTION, (SourceLocation){Tok.line, Tok.column, Tok.start});
    // Store function type
}

type(T) ::= FN(Tok) LPAREN type_list RPAREN ARROW type(Ret). {
    T = ast_alloc(parser, AST_TYPE_FUNCTION, (SourceLocation){Tok.line, Tok.column, Tok.start});
    // Store function type with return
}

type(T) ::= type(Err) BANG type(Val). {
    T = ast_alloc(parser, AST_TYPE_RESULT, Err->loc);
    // Store result type
}

%type type_list { void* }
type_list(L) ::= type(T). { L = T; }
type_list(L) ::= type_list(Prev) COMMA type(T). { L = T; }

// Error handling
%syntax_error {
    parser->has_error = true;
    parser_error(parser, (SourceLocation){0, 0, ""}, "Syntax error");
}

%parse_failure {
    parser->has_error = true;
    parser_error(parser, (SourceLocation){0, 0, ""}, "Parse failure");
}
