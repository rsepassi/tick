#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "lexer.h"
#include "ast.h"

// Parser Interface
// Purpose: Lemon-based LALR(1) parser that constructs AST via callbacks

// Forward declare Arena
typedef struct Arena Arena;

typedef struct Parser {
    Lexer* lexer;
    void* lemon_parser;  // Opaque Lemon parser state
    AstNode* root;       // Completed AST root
    Arena* ast_arena;    // Arena for AST allocation
    bool has_error;
} Parser;

// Initialize parser with caller-provided memory
void parser_init(Parser* parser, Lexer* lexer, Arena* ast_arena);

// Parse and return root AST node
AstNode* parser_parse(Parser* parser);

// Called by Lemon on token match
void parser_on_match(Parser* parser /* Lemon callback args */);

#endif // PARSER_H
