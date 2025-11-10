#ifndef AST_H
#define AST_H

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include "error.h"

// AST Interface
// Purpose: Contract between parser and semantic analysis phases

typedef enum {
    // Top-level
    AST_MODULE,
    AST_IMPORT_DECL,

    // Declarations
    AST_FUNCTION_DECL,
    AST_STRUCT_DECL,
    AST_ENUM_DECL,
    AST_UNION_DECL,
    AST_LET_DECL,
    AST_VAR_DECL,

    // Statements
    AST_LET_STMT,
    AST_VAR_STMT,
    AST_ASSIGN_STMT,
    AST_RETURN_STMT,
    AST_IF_STMT,
    AST_FOR_STMT,
    AST_SWITCH_STMT,
    AST_BREAK_STMT,
    AST_CONTINUE_STMT,
    AST_CONTINUE_SWITCH_STMT,
    AST_DEFER_STMT,
    AST_ERRDEFER_STMT,
    AST_SUSPEND_STMT,
    AST_RESUME_STMT,
    AST_BLOCK_STMT,
    AST_EXPR_STMT,
    AST_TRY_CATCH_STMT,

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
    AST_STRUCT_INIT_EXPR,
    AST_ARRAY_INIT_EXPR,
    AST_RANGE_EXPR,

    // Types
    AST_TYPE_PRIMITIVE,
    AST_TYPE_ARRAY,
    AST_TYPE_SLICE,
    AST_TYPE_POINTER,
    AST_TYPE_FUNCTION,
    AST_TYPE_RESULT,
    AST_TYPE_NAMED,
} AstNodeKind;

typedef enum {
    BINOP_ADD,
    BINOP_SUB,
    BINOP_MUL,
    BINOP_DIV,
    BINOP_MOD,
    BINOP_AND,
    BINOP_OR,
    BINOP_XOR,
    BINOP_LSHIFT,
    BINOP_RSHIFT,
    BINOP_LOGICAL_AND,
    BINOP_LOGICAL_OR,
    BINOP_LT,
    BINOP_GT,
    BINOP_LT_EQ,
    BINOP_GT_EQ,
    BINOP_EQ_EQ,
    BINOP_BANG_EQ,
} BinaryOp;

typedef enum {
    UNOP_NEG,
    UNOP_NOT,
    UNOP_BIT_NOT,
    UNOP_DEREF,
    UNOP_ADDR,
} UnaryOp;

typedef enum {
    ASSIGN_SIMPLE,
    ASSIGN_ADD,
    ASSIGN_SUB,
    ASSIGN_MUL,
    ASSIGN_DIV,
    ASSIGN_MOD,
    ASSIGN_AND,
    ASSIGN_OR,
    ASSIGN_XOR,
    ASSIGN_LSHIFT,
    ASSIGN_RSHIFT,
} AssignOp;

// Forward declare Type
typedef struct Type Type;
typedef struct AstNode AstNode;

// Type nodes for AST
typedef struct AstTypeNode {
    AstNodeKind kind;
    SourceLocation loc;

    union {
        struct {
            const char* name;  // i32, u64, bool, etc
        } primitive;

        struct {
            AstNode* element_type;
            AstNode* size_expr;  // NULL for slice
        } array;

        struct {
            AstNode* element_type;
        } slice;

        struct {
            AstNode* pointee_type;
        } pointer;

        struct {
            AstNode** param_types;
            size_t param_count;
            AstNode* return_type;
        } function;

        struct {
            AstNode* error_type;
            AstNode* value_type;
        } result;

        struct {
            const char* name;
        } named;
    } data;
} AstTypeNode;

// Field/parameter declarations
typedef struct AstField {
    const char* name;
    AstNode* type;
    SourceLocation loc;
} AstField;

typedef struct AstParam {
    const char* name;
    AstNode* type;
    SourceLocation loc;
} AstParam;

typedef struct AstEnumValue {
    const char* name;
    AstNode* value;  // NULL for auto
    SourceLocation loc;
} AstEnumValue;

typedef struct AstSwitchCase {
    AstNode** values;  // NULL for default
    size_t value_count;
    AstNode** stmts;
    size_t stmt_count;
    SourceLocation loc;
} AstSwitchCase;

typedef struct AstStructInit {
    const char* field_name;
    AstNode* value;
} AstStructInit;

typedef struct AstNode {
    AstNodeKind kind;
    SourceLocation loc;

    // Type annotation (filled by typeck phase)
    Type* type;

    // Union of all node-specific data
    union {
        // AST_MODULE
        struct {
            const char* name;
            AstNode** decls;
            size_t decl_count;
        } module;

        // AST_IMPORT_DECL
        struct {
            const char* name;  // binding name
            const char* path;  // import path
            bool is_pub;
        } import_decl;

        // AST_FUNCTION_DECL
        struct {
            const char* name;
            AstParam* params;
            size_t param_count;
            AstNode* return_type;  // NULL for void
            AstNode* body;
            bool is_pub;
        } function_decl;

        // AST_STRUCT_DECL
        struct {
            const char* name;
            AstField* fields;
            size_t field_count;
            bool is_packed;
            bool is_pub;
        } struct_decl;

        // AST_ENUM_DECL
        struct {
            const char* name;
            AstNode* underlying_type;
            AstEnumValue* values;
            size_t value_count;
            bool is_pub;
        } enum_decl;

        // AST_UNION_DECL
        struct {
            const char* name;
            AstNode* tag_type;  // NULL for auto
            AstField* fields;
            size_t field_count;
            bool is_pub;
        } union_decl;

        // AST_LET_DECL, AST_LET_STMT
        struct {
            const char* name;
            AstNode* type;  // NULL for inferred
            AstNode* init;  // NULL for function/type decls
            bool is_pub;
        } let_decl;

        // AST_VAR_DECL, AST_VAR_STMT
        struct {
            const char* name;
            AstNode* type;  // NULL for inferred
            AstNode* init;  // NULL if no initializer
            bool is_volatile;
            bool is_pub;
        } var_decl;

        // AST_ASSIGN_STMT
        struct {
            AstNode* lhs;
            AssignOp op;
            AstNode* rhs;
        } assign_stmt;

        // AST_RETURN_STMT
        struct {
            AstNode* value;  // NULL for void return
        } return_stmt;

        // AST_IF_STMT
        struct {
            AstNode* condition;
            AstNode* then_block;
            AstNode* else_block;  // NULL if no else
        } if_stmt;

        // AST_FOR_STMT
        struct {
            const char* loop_var;  // NULL for condition-only or infinite
            AstNode* iterable;     // NULL for infinite, range expr or collection
            AstNode* condition;    // NULL for range/collection iteration
            AstNode* continue_expr; // NULL if no continue clause
            AstNode* body;
        } for_stmt;

        // AST_SWITCH_STMT
        struct {
            AstNode* value;
            AstSwitchCase* cases;
            size_t case_count;
            bool is_while_switch;  // true for "while switch"
        } switch_stmt;

        // AST_BREAK_STMT, AST_CONTINUE_STMT (no data)

        // AST_CONTINUE_SWITCH_STMT
        struct {
            AstNode* value;
        } continue_switch_stmt;

        // AST_DEFER_STMT, AST_ERRDEFER_STMT
        struct {
            AstNode* stmt;
        } defer_stmt;

        // AST_SUSPEND_STMT (no data)

        // AST_RESUME_STMT
        struct {
            AstNode* coro;
        } resume_stmt;

        // AST_BLOCK_STMT
        struct {
            AstNode** stmts;
            size_t stmt_count;
        } block_stmt;

        // AST_EXPR_STMT
        struct {
            AstNode* expr;
        } expr_stmt;

        // AST_TRY_CATCH_STMT
        struct {
            AstNode* try_block;
            const char* error_var;  // NULL if no catch
            AstNode* catch_block;   // NULL if no catch (try propagates)
        } try_catch_stmt;

        // AST_BINARY_EXPR
        struct {
            BinaryOp op;
            AstNode* left;
            AstNode* right;
        } binary_expr;

        // AST_UNARY_EXPR
        struct {
            UnaryOp op;
            AstNode* operand;
        } unary_expr;

        // AST_CALL_EXPR
        struct {
            AstNode* callee;
            AstNode** args;
            size_t arg_count;
        } call_expr;

        // AST_ASYNC_CALL_EXPR
        struct {
            AstNode* callee;
            AstNode** args;
            size_t arg_count;
        } async_call_expr;

        // AST_FIELD_ACCESS_EXPR
        struct {
            AstNode* object;
            const char* field_name;
            bool is_arrow;  // true for ->, false for .
        } field_access_expr;

        // AST_INDEX_EXPR
        struct {
            AstNode* array;
            AstNode* index;
        } index_expr;

        // AST_LITERAL_EXPR
        struct {
            enum {
                LITERAL_INT,
                LITERAL_STRING,
                LITERAL_BOOL,
            } literal_kind;

            union {
                uint64_t int_value;
                struct {
                    const char* str_value;
                    size_t str_length;
                } string;
                bool bool_value;
            } value;
        } literal_expr;

        // AST_IDENTIFIER_EXPR
        struct {
            const char* name;
        } identifier_expr;

        // AST_CAST_EXPR
        struct {
            AstNode* type;
            AstNode* expr;
        } cast_expr;

        // AST_TRY_EXPR
        struct {
            AstNode* expr;
        } try_expr;

        // AST_STRUCT_INIT_EXPR
        struct {
            AstNode* type;  // NULL for inferred
            AstStructInit* fields;
            size_t field_count;
        } struct_init_expr;

        // AST_ARRAY_INIT_EXPR
        struct {
            AstNode** elements;
            size_t element_count;
        } array_init_expr;

        // AST_RANGE_EXPR
        struct {
            AstNode* start;
            AstNode* end;  // exclusive
        } range_expr;

    } data;
} AstNode;

// Utilities for semantic analysis
void ast_print_debug(AstNode* node, FILE* out);
void ast_walk(AstNode* node, void (*visitor)(AstNode*, void*), void* ctx);

#endif // AST_H
