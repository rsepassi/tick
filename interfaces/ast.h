#ifndef AST_H
#define AST_H

#include <stddef.h>
#include <stdio.h>
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

typedef struct AstNode {
    AstNodeKind kind;
    SourceLocation loc;

    // Type annotation (filled by typeck phase)
    struct Type* type;

    // Union of all node-specific data
    union {
        struct { /* module fields */ } module;
        struct { /* function fields */ } function;
        struct { /* binary expr fields */ } binary_expr;
        // ... etc
    } data;
} AstNode;

// Utilities for semantic analysis
void ast_print_debug(AstNode* node, FILE* out);
void ast_walk(AstNode* node, void (*visitor)(AstNode*, void*), void* ctx);

#endif // AST_H
