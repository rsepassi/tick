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
    AST_SWITCH_STMT,
    AST_DEFER_STMT,
    AST_ERRDEFER_STMT,
    AST_SUSPEND_STMT,
    AST_RESUME_STMT,
    AST_BLOCK_STMT,
    AST_EXPR_STMT,

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

    // Types
    AST_TYPE_PRIMITIVE,
    AST_TYPE_ARRAY,
    AST_TYPE_SLICE,
    AST_TYPE_POINTER,
    AST_TYPE_FUNCTION,
    AST_TYPE_RESULT,
    AST_TYPE_NAMED,
} AstNodeKind;

// Forward declare Type
typedef struct Type Type;
typedef struct Symbol Symbol;

typedef struct AstNode {
    AstNodeKind kind;
    SourceLocation loc;

    // Type annotation (filled by typeck phase)
    struct Type* type;

    // Symbol reference (for identifiers)
    Symbol* symbol;

    // Union of all node-specific data
    union {
        // Declarations
        struct {
            const char* name;
            struct AstNode** imports;
            size_t import_count;
            struct AstNode** decls;
            size_t decl_count;
        } module;

        struct {
            const char* name;
            bool is_public;
            bool is_async;
            struct AstNode** params;
            size_t param_count;
            struct AstNode* return_type;
            struct AstNode* body;  // Block statement
        } function;

        struct {
            const char* name;
            bool is_public;
            struct AstNode** fields;
            size_t field_count;
        } struct_decl;

        // Statements
        struct {
            const char* name;
            struct AstNode* type_expr;
            struct AstNode* init;
        } let_stmt;

        struct {
            const char* name;
            struct AstNode* type_expr;
            struct AstNode* init;
        } var_stmt;

        struct {
            struct AstNode* expr;
        } return_stmt;

        struct {
            struct AstNode* condition;
            struct AstNode* then_block;
            struct AstNode* else_block;  // May be NULL
        } if_stmt;

        struct {
            struct AstNode* init;      // May be NULL
            struct AstNode* condition; // May be NULL
            struct AstNode* update;    // May be NULL
            struct AstNode* body;
        } for_stmt;

        struct {
            struct AstNode* expr;
            struct AstNode** cases;
            size_t case_count;
        } switch_stmt;

        struct {
            struct AstNode* stmt;
        } defer_stmt;

        struct {
            struct AstNode* stmt;
        } errdefer_stmt;

        struct {
            // Suspend statement - no additional data
        } suspend_stmt;

        struct {
            struct AstNode* handle_expr;
        } resume_stmt;

        struct {
            struct AstNode** stmts;
            size_t stmt_count;
        } block;

        struct {
            struct AstNode* expr;
        } expr_stmt;

        // Expressions
        struct {
            const char* op;
            struct AstNode* left;
            struct AstNode* right;
        } binary_expr;

        struct {
            const char* op;
            struct AstNode* operand;
        } unary_expr;

        struct {
            struct AstNode* callee;
            struct AstNode** args;
            size_t arg_count;
        } call_expr;

        struct {
            struct AstNode* callee;
            struct AstNode** args;
            size_t arg_count;
        } async_call_expr;

        struct {
            struct AstNode* object;
            const char* field_name;
        } field_access;

        struct {
            struct AstNode* array;
            struct AstNode* index;
        } index_expr;

        struct {
            union {
                int64_t int_val;
                double float_val;
                bool bool_val;
                const char* string_val;
            } value;
        } literal;

        struct {
            const char* name;
        } identifier;

        struct {
            struct AstNode* expr;
            struct AstNode* target_type;
        } cast_expr;

    } data;
} AstNode;

// Utilities for semantic analysis
void ast_print_debug(AstNode* node, FILE* out);
void ast_walk(AstNode* node, void (*visitor)(AstNode*, void*), void* ctx);

#endif // AST_H
