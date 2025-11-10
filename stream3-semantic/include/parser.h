#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "lexer.h"
#include "ast.h"
#include "error.h"

// Parser Interface
// Purpose: Lemon-based LALR(1) parser that constructs AST via callbacks

// Forward declare Arena
typedef struct Arena Arena;

typedef struct Parser {
    Lexer* lexer;
    void* lemon_parser;  // Opaque Lemon parser state
    AstNode* root;       // Completed AST root
    Arena* ast_arena;    // Arena for AST allocation
    ErrorList* errors;   // Error list for reporting
    bool has_error;
} Parser;

// Initialize parser with caller-provided memory
void parser_init(Parser* parser, Lexer* lexer, Arena* ast_arena, ErrorList* errors);

// Parse and return root AST node (NULL on error)
AstNode* parser_parse(Parser* parser);

// Cleanup parser state
void parser_cleanup(Parser* parser);

// Helper functions for AST construction (used by grammar actions)
AstNode* ast_alloc(Parser* parser, AstNodeKind kind, SourceLocation loc);
void parser_error(Parser* parser, SourceLocation loc, const char* fmt, ...);

#endif // PARSER_H
