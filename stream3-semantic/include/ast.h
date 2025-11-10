#ifndef AST_H
#define AST_H

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "error.h"

// AST Interface
// Purpose: Contract between parser and semantic analysis phases

typedef enum {
    AST_MODULE,
    AST_IMPORT_DECL,
    AST_FUNCTION_DECL,
    AST_STRUCT_DECL,
    AST_ENUM_DECL,
    AST_UNION_DECL,
    AST_LET_STMT,
    AST_VAR_STMT,
    AST_RETURN_STMT,
    AST_IF_STMT,
    AST_FOR_STMT,
    AST_WHILE_STMT,
    AST_SWITCH_STMT,
    AST_DEFER_STMT,
    AST_ERRDEFER_STMT,
    AST_SUSPEND_STMT,
    AST_RESUME_STMT,
    AST_BLOCK_STMT,
    AST_EXPR_STMT,
    AST_BREAK_STMT,
    AST_CONTINUE_STMT,
    AST_ASSIGN_STMT,

    // Expressions
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_CALL_EXPR,
    AST_ASYNC_CALL_EXPR,
    AST_FIELD_ACCESS_EXPR,
    AST_INDEX_EXPR,
    AST_LITERAL_EXPR,
    AST_IDENTIFIER_EXPR,
    AST_CAST_EXPR,
    AST_TRY_EXPR,

    // Types
    AST_TYPE_PRIMITIVE,
    AST_TYPE_ARRAY,
    AST_TYPE_SLICE,
    AST_TYPE_POINTER,
    AST_TYPE_FUNCTION,
    AST_TYPE_RESULT,
    AST_TYPE_OPTION,
    AST_TYPE_NAMED,
} AstNodeKind;

typedef enum {
    BINOP_ADD, BINOP_SUB, BINOP_MUL, BINOP_DIV, BINOP_MOD,
    BINOP_AND, BINOP_OR, BINOP_XOR, BINOP_LSHIFT, BINOP_RSHIFT,
    BINOP_LAND, BINOP_LOR,
    BINOP_EQ, BINOP_NE, BINOP_LT, BINOP_LE, BINOP_GT, BINOP_GE,
} BinaryOp;

typedef enum {
    UNOP_NEG, UNOP_NOT, UNOP_BNOT, UNOP_ADDR, UNOP_DEREF,
} UnaryOp;

typedef enum {
    LIT_INT, LIT_BOOL, LIT_STRING,
} LiteralKind;

// Forward declare Type and Symbol
typedef struct Type Type;
typedef struct Symbol Symbol;

typedef struct AstNode AstNode;

// Module: top-level container
typedef struct AstModule {
    const char* name;           // Module name (from filename or explicit)
    AstNode** declarations;     // Functions, structs, enums, imports, etc.
    size_t decl_count;
} AstModule;

// Import declaration
typedef struct AstImportDecl {
    const char* module_name;    // Name to import
    const char* alias;          // NULL if no alias
} AstImportDecl;

// Function parameter
typedef struct AstParam {
    const char* name;
    AstNode* type_node;         // AST node representing the type
    Type* type;                 // Resolved type (filled by typeck)
} AstParam;

// Function declaration
typedef struct AstFunctionDecl {
    const char* name;
    AstParam* params;
    size_t param_count;
    AstNode* return_type_node;  // AST node representing return type (NULL for void)
    Type* return_type;          // Resolved return type (filled by typeck)
    AstNode* body;              // Block statement
    bool is_public;
    bool is_async;              // Contains async/suspend/resume
    Symbol* symbol;             // Resolved symbol (filled by resolver)
} AstFunctionDecl;

// Struct field
typedef struct AstStructField {
    const char* name;
    AstNode* type_node;
    Type* type;                 // Resolved type (filled by typeck)
    bool is_public;
} AstStructField;

// Struct declaration
typedef struct AstStructDecl {
    const char* name;
    AstStructField* fields;
    size_t field_count;
    bool is_public;
    bool is_packed;
    size_t alignment;           // 0 if no explicit alignment
    Symbol* symbol;             // Resolved symbol (filled by resolver)
} AstStructDecl;

// Enum variant
typedef struct AstEnumVariant {
    const char* name;
    int64_t value;              // Explicit value
    bool has_explicit_value;
} AstEnumVariant;

// Enum declaration
typedef struct AstEnumDecl {
    const char* name;
    AstNode* underlying_type_node;  // Type node for underlying integer type
    Type* underlying_type;          // Resolved type (filled by typeck)
    AstEnumVariant* variants;
    size_t variant_count;
    bool is_public;
    Symbol* symbol;                 // Resolved symbol (filled by resolver)
} AstEnumDecl;

// Union variant
typedef struct AstUnionVariant {
    const char* name;
    AstNode* type_node;
    Type* type;                 // Resolved type (filled by typeck)
} AstUnionVariant;

// Union declaration
typedef struct AstUnionDecl {
    const char* name;
    AstUnionVariant* variants;
    size_t variant_count;
    bool is_public;
    Symbol* symbol;             // Resolved symbol (filled by resolver)
} AstUnionDecl;

// Let statement: let name: type = expr
typedef struct AstLetStmt {
    const char* name;
    AstNode* type_node;         // NULL for type inference
    Type* type;                 // Resolved type (filled by typeck)
    AstNode* initializer;       // Expression
    Symbol* symbol;             // Resolved symbol (filled by resolver)
} AstLetStmt;

// Var statement: var name: type = expr
typedef struct AstVarStmt {
    const char* name;
    AstNode* type_node;         // NULL for type inference
    Type* type;                 // Resolved type (filled by typeck)
    AstNode* initializer;       // Expression (can be NULL)
    bool is_volatile;
    Symbol* symbol;             // Resolved symbol (filled by resolver)
} AstVarStmt;

// Return statement
typedef struct AstReturnStmt {
    AstNode* value;             // NULL for void return
} AstReturnStmt;

// If statement
typedef struct AstIfStmt {
    AstNode* condition;
    AstNode* then_block;
    AstNode* else_block;        // NULL if no else
} AstIfStmt;

// For statement: for (item in collection)
typedef struct AstForStmt {
    const char* iterator_name;  // Loop variable name
    AstNode* collection;        // Expression to iterate over
    AstNode* body;              // Block statement
    Symbol* iterator_symbol;    // Resolved symbol (filled by resolver)
} AstForStmt;

// While statement: while (condition)
typedef struct AstWhileStmt {
    AstNode* condition;
    AstNode* body;              // Block statement
} AstWhileStmt;

// Switch case
typedef struct AstSwitchCase {
    AstNode* value;             // NULL for default case
    AstNode* body;              // Block statement
    bool is_default;
} AstSwitchCase;

// Switch statement
typedef struct AstSwitchStmt {
    AstNode* value;             // Expression to switch on
    AstSwitchCase* cases;
    size_t case_count;
} AstSwitchStmt;

// Defer statement
typedef struct AstDeferStmt {
    AstNode* statement;         // Statement to defer
} AstDeferStmt;

// Errdefer statement
typedef struct AstErrdeferStmt {
    AstNode* statement;         // Statement to execute on error
} AstErrdeferStmt;

// Suspend statement
typedef struct AstSuspendStmt {
    // Empty for now - just marks a suspension point
} AstSuspendStmt;

// Resume statement
typedef struct AstResumeStmt {
    AstNode* coroutine_expr;    // Expression evaluating to coroutine to resume
} AstResumeStmt;

// Block statement
typedef struct AstBlockStmt {
    AstNode** statements;
    size_t stmt_count;
} AstBlockStmt;

// Expression statement
typedef struct AstExprStmt {
    AstNode* expression;
} AstExprStmt;

// Assignment statement
typedef struct AstAssignStmt {
    AstNode* target;            // Lvalue expression
    AstNode* value;             // Rvalue expression
} AstAssignStmt;

// Binary expression
typedef struct AstBinaryExpr {
    BinaryOp op;
    AstNode* left;
    AstNode* right;
} AstBinaryExpr;

// Unary expression
typedef struct AstUnaryExpr {
    UnaryOp op;
    AstNode* operand;
} AstUnaryExpr;

// Call expression
typedef struct AstCallExpr {
    AstNode* callee;            // Expression
    AstNode** args;
    size_t arg_count;
} AstCallExpr;

// Async call expression
typedef struct AstAsyncCallExpr {
    AstNode* callee;            // Expression
    AstNode** args;
    size_t arg_count;
} AstAsyncCallExpr;

// Field access expression
typedef struct AstFieldAccessExpr {
    AstNode* object;            // Expression
    const char* field_name;
    size_t field_index;         // Filled by typeck (index in struct)
} AstFieldAccessExpr;

// Index expression
typedef struct AstIndexExpr {
    AstNode* array;             // Expression
    AstNode* index;             // Expression
} AstIndexExpr;

// Literal expression
typedef struct AstLiteralExpr {
    LiteralKind lit_kind;
    union {
        int64_t int_value;
        bool bool_value;
        struct {
            const char* str_value;
            size_t str_length;
        };
    };
} AstLiteralExpr;

// Identifier expression
typedef struct AstIdentifierExpr {
    const char* name;
    Symbol* symbol;             // Resolved symbol (filled by resolver)
} AstIdentifierExpr;

// Cast expression
typedef struct AstCastExpr {
    AstNode* expr;
    AstNode* target_type_node;
    Type* target_type;          // Resolved type (filled by typeck)
} AstCastExpr;

// Try expression (for Result unwrapping)
typedef struct AstTryExpr {
    AstNode* expr;              // Expression that returns Result<T, E>
} AstTryExpr;

// Type nodes
typedef struct AstTypePrimitive {
    const char* name;           // "i32", "u64", "bool", etc.
    Type* type;                 // Resolved type (filled by typeck)
} AstTypePrimitive;

typedef struct AstTypeArray {
    AstNode* elem_type_node;
    size_t length;
    Type* type;                 // Resolved type (filled by typeck)
} AstTypeArray;

typedef struct AstTypeSlice {
    AstNode* elem_type_node;
    Type* type;                 // Resolved type (filled by typeck)
} AstTypeSlice;

typedef struct AstTypePointer {
    AstNode* pointee_type_node;
    Type* type;                 // Resolved type (filled by typeck)
} AstTypePointer;

typedef struct AstTypeFunction {
    AstNode** param_type_nodes;
    size_t param_count;
    AstNode* return_type_node;
    Type* type;                 // Resolved type (filled by typeck)
} AstTypeFunction;

typedef struct AstTypeResult {
    AstNode* value_type_node;
    AstNode* error_type_node;
    Type* type;                 // Resolved type (filled by typeck)
} AstTypeResult;

typedef struct AstTypeOption {
    AstNode* inner_type_node;
    Type* type;                 // Resolved type (filled by typeck)
} AstTypeOption;

typedef struct AstTypeNamed {
    const char* name;
    Symbol* symbol;             // Resolved symbol (filled by resolver)
    Type* type;                 // Resolved type (filled by typeck)
} AstTypeNamed;

// Main AST node structure
struct AstNode {
    AstNodeKind kind;
    SourceLocation loc;

    // Type annotation (filled by typeck phase)
    Type* type;

    // Union of all node-specific data
    union {
        AstModule module;
        AstImportDecl import_decl;
        AstFunctionDecl function_decl;
        AstStructDecl struct_decl;
        AstEnumDecl enum_decl;
        AstUnionDecl union_decl;
        AstLetStmt let_stmt;
        AstVarStmt var_stmt;
        AstReturnStmt return_stmt;
        AstIfStmt if_stmt;
        AstForStmt for_stmt;
        AstWhileStmt while_stmt;
        AstSwitchStmt switch_stmt;
        AstDeferStmt defer_stmt;
        AstErrdeferStmt errdefer_stmt;
        AstSuspendStmt suspend_stmt;
        AstResumeStmt resume_stmt;
        AstBlockStmt block_stmt;
        AstExprStmt expr_stmt;
        AstAssignStmt assign_stmt;

        AstBinaryExpr binary_expr;
        AstUnaryExpr unary_expr;
        AstCallExpr call_expr;
        AstAsyncCallExpr async_call_expr;
        AstFieldAccessExpr field_access_expr;
        AstIndexExpr index_expr;
        AstLiteralExpr literal_expr;
        AstIdentifierExpr identifier_expr;
        AstCastExpr cast_expr;
        AstTryExpr try_expr;

        AstTypePrimitive type_primitive;
        AstTypeArray type_array;
        AstTypeSlice type_slice;
        AstTypePointer type_pointer;
        AstTypeFunction type_function;
        AstTypeResult type_result;
        AstTypeOption type_option;
        AstTypeNamed type_named;
    } data;
};

// Forward declare Arena
typedef struct Arena Arena;

// AST node allocation
AstNode* ast_new_node(AstNodeKind kind, SourceLocation loc, Arena* arena);

// Utilities for semantic analysis
void ast_print_debug(AstNode* node, FILE* out);
void ast_walk(AstNode* node, void (*visitor)(AstNode*, void*), void* ctx);

#endif // AST_H
