#include "../../interfaces2/parser.h"
#include "../../interfaces2/ast.h"
#include "../../interfaces2/lexer.h"
#include "../../interfaces2/arena.h"
#include "../../interfaces2/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

// Forward declarations for Lemon-generated parser
void* ParseAlloc(void* (*allocProc)(size_t));
void ParseFree(void* parser, void (*freeProc)(void*));
void Parse(void* parser, int tokenCode, Token token, Parser* state);
void ParseTrace(FILE* stream, char* prefix);

// Initialize parser with caller-provided memory
void parser_init(Parser* parser, Lexer* lexer, Arena* ast_arena, ErrorList* errors) {
    parser->lexer = lexer;
    parser->lemon_parser = ParseAlloc(malloc);
    parser->root = NULL;
    parser->ast_arena = ast_arena;
    parser->errors = errors;
    parser->has_error = false;

#ifdef PARSER_DEBUG
    ParseTrace(stderr, "Parser: ");
#endif
}

// Parse and return root AST node (NULL on error)
AstNode* parser_parse(Parser* parser) {
    if (!parser || !parser->lexer || !parser->lemon_parser) {
        return NULL;
    }

    Token token;
    do {
        token = lexer_next_token(parser->lexer);

        if (token.kind == TOKEN_ERROR) {
            parser_error(parser,
                (SourceLocation){token.line, token.column, parser->lexer->filename},
                "Lexical error");
            parser->has_error = true;
            break;
        }

        // Don't feed TOKEN_EOF to parser - Lemon expects 0 for end-of-input
        if (token.kind == TOKEN_EOF) {
            break;
        }

        // Skip special tokens that should never be sent to the parser
        if (token.kind == TOKEN_LINE_COMMENT || token.kind == TOKEN_BLOCK_COMMENT || token.kind == TOKEN_EMBED_FILE) {
            continue;
        }

        // Feed token to Lemon parser
        Parse(parser->lemon_parser, token.kind, token, parser);

        if (parser->has_error) {
            break;
        }

    } while (true);

    // Signal end of input with token code 0
    if (!parser->has_error) {
        Parse(parser->lemon_parser, 0, token, parser);
    }

    return parser->has_error ? NULL : parser->root;
}

// Cleanup parser state
void parser_cleanup(Parser* parser) {
    if (parser && parser->lemon_parser) {
        ParseFree(parser->lemon_parser, free);
        parser->lemon_parser = NULL;
    }
}

// Helper function for AST construction (used by grammar actions)
AstNode* ast_alloc(Parser* parser, AstNodeKind kind, SourceLocation loc) {
    if (!parser || !parser->ast_arena) {
        return NULL;
    }

    AstNode* node = (AstNode*)arena_alloc(parser->ast_arena, sizeof(AstNode), _Alignof(AstNode));
    if (!node) {
        parser_error(parser, loc, "Out of memory allocating AST node");
        parser->has_error = true;
        return NULL;
    }

    memset(node, 0, sizeof(AstNode));
    node->kind = kind;
    node->loc = loc;
    node->type = NULL;

    return node;
}

// Error reporting helper
void parser_error(Parser* parser, SourceLocation loc, const char* fmt, ...) {
    if (!parser || !parser->errors) {
        return;
    }

    va_list args;
    va_start(args, fmt);

    // Format the error message
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);

    va_end(args);

    // Add to error list
    error_list_add(parser->errors, ERROR_SYNTAX, loc, "%s", buffer);
    parser->has_error = true;
}

// Helper functions for building AST node lists (used by grammar)

typedef struct AstNodeList {
    AstNode** nodes;
    size_t count;
    size_t capacity;
} AstNodeList;

AstNodeList* node_list_create(Parser* parser) {
    AstNodeList* list = (AstNodeList*)arena_alloc(parser->ast_arena,
        sizeof(AstNodeList), _Alignof(AstNodeList));
    if (!list) return NULL;

    list->capacity = 8;
    list->count = 0;
    list->nodes = (AstNode**)arena_alloc(parser->ast_arena,
        sizeof(AstNode*) * list->capacity, _Alignof(AstNode*));

    return list;
}

void node_list_append(Parser* parser, AstNodeList* list, AstNode* node) {
    if (!list || !node) return;

    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        AstNode** new_nodes = (AstNode**)arena_alloc(parser->ast_arena,
            sizeof(AstNode*) * new_capacity, _Alignof(AstNode*));
        if (!new_nodes) return;

        memcpy(new_nodes, list->nodes, sizeof(AstNode*) * list->count);
        list->nodes = new_nodes;
        list->capacity = new_capacity;
    }

    list->nodes[list->count++] = node;
}

// Helper functions for building parameter lists
typedef struct AstParamList {
    AstParam* params;
    size_t count;
    size_t capacity;
} AstParamList;

AstParamList* param_list_create(Parser* parser) {
    AstParamList* list = (AstParamList*)arena_alloc(parser->ast_arena,
        sizeof(AstParamList), _Alignof(AstParamList));
    if (!list) return NULL;

    list->capacity = 8;
    list->count = 0;
    list->params = (AstParam*)arena_alloc(parser->ast_arena,
        sizeof(AstParam) * list->capacity, _Alignof(AstParam));

    return list;
}

void param_list_append(Parser* parser, AstParamList* list, const char* name, AstNode* type, SourceLocation loc) {
    if (!list) return;

    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        AstParam* new_params = (AstParam*)arena_alloc(parser->ast_arena,
            sizeof(AstParam) * new_capacity, _Alignof(AstParam));
        if (!new_params) return;

        memcpy(new_params, list->params, sizeof(AstParam) * list->count);
        list->params = new_params;
        list->capacity = new_capacity;
    }

    list->params[list->count].name = name;
    list->params[list->count].type = type;
    list->params[list->count].loc = loc;
    list->count++;
}

// Helper functions for building field lists
typedef struct AstFieldList {
    AstField* fields;
    size_t count;
    size_t capacity;
} AstFieldList;

__attribute__((unused)) static AstFieldList* field_list_create(Parser* parser) {
    AstFieldList* list = (AstFieldList*)arena_alloc(parser->ast_arena,
        sizeof(AstFieldList), _Alignof(AstFieldList));
    if (!list) return NULL;

    list->capacity = 8;
    list->count = 0;
    list->fields = (AstField*)arena_alloc(parser->ast_arena,
        sizeof(AstField) * list->capacity, _Alignof(AstField));

    return list;
}

__attribute__((unused)) static void field_list_append(Parser* parser, AstFieldList* list, const char* name, AstNode* type, SourceLocation loc) {
    if (!list) return;

    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        AstField* new_fields = (AstField*)arena_alloc(parser->ast_arena,
            sizeof(AstField) * new_capacity, _Alignof(AstField));
        if (!new_fields) return;

        memcpy(new_fields, list->fields, sizeof(AstField) * list->count);
        list->fields = new_fields;
        list->capacity = new_capacity;
    }

    list->fields[list->count].name = name;
    list->fields[list->count].type = type;
    list->fields[list->count].loc = loc;
    list->count++;
}

// Helper functions for building enum value lists
typedef struct AstEnumValueList {
    AstEnumValue* values;
    size_t count;
    size_t capacity;
} AstEnumValueList;

__attribute__((unused)) static AstEnumValueList* enum_value_list_create(Parser* parser) {
    AstEnumValueList* list = (AstEnumValueList*)arena_alloc(parser->ast_arena,
        sizeof(AstEnumValueList), _Alignof(AstEnumValueList));
    if (!list) return NULL;

    list->capacity = 8;
    list->count = 0;
    list->values = (AstEnumValue*)arena_alloc(parser->ast_arena,
        sizeof(AstEnumValue) * list->capacity, _Alignof(AstEnumValue));

    return list;
}

__attribute__((unused)) static void enum_value_list_append(Parser* parser, AstEnumValueList* list, const char* name, AstNode* value, SourceLocation loc) {
    if (!list) return;

    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        AstEnumValue* new_values = (AstEnumValue*)arena_alloc(parser->ast_arena,
            sizeof(AstEnumValue) * new_capacity, _Alignof(AstEnumValue));
        if (!new_values) return;

        memcpy(new_values, list->values, sizeof(AstEnumValue) * list->count);
        list->values = new_values;
        list->capacity = new_capacity;
    }

    list->values[list->count].name = name;
    list->values[list->count].value = value;
    list->values[list->count].loc = loc;
    list->count++;
}

// Helper functions for building switch case lists
typedef struct AstSwitchCaseList {
    AstSwitchCase* cases;
    size_t count;
    size_t capacity;
} AstSwitchCaseList;

AstSwitchCaseList* switch_case_list_create(Parser* parser) {
    AstSwitchCaseList* list = (AstSwitchCaseList*)arena_alloc(parser->ast_arena,
        sizeof(AstSwitchCaseList), _Alignof(AstSwitchCaseList));
    if (!list) return NULL;

    list->capacity = 8;
    list->count = 0;
    list->cases = (AstSwitchCase*)arena_alloc(parser->ast_arena,
        sizeof(AstSwitchCase) * list->capacity, _Alignof(AstSwitchCase));

    return list;
}

void switch_case_list_append(Parser* parser, AstSwitchCaseList* list,
                             AstNode** values, size_t value_count,
                             AstNode** stmts, size_t stmt_count,
                             SourceLocation loc) {
    if (!list) return;

    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        AstSwitchCase* new_cases = (AstSwitchCase*)arena_alloc(parser->ast_arena,
            sizeof(AstSwitchCase) * new_capacity, _Alignof(AstSwitchCase));
        if (!new_cases) return;

        memcpy(new_cases, list->cases, sizeof(AstSwitchCase) * list->count);
        list->cases = new_cases;
        list->capacity = new_capacity;
    }

    list->cases[list->count].values = values;
    list->cases[list->count].value_count = value_count;
    list->cases[list->count].stmts = stmts;
    list->cases[list->count].stmt_count = stmt_count;
    list->cases[list->count].loc = loc;
    list->count++;
}

// Helper functions for building struct init lists
typedef struct AstStructInitList {
    AstStructInit* fields;
    size_t count;
    size_t capacity;
} AstStructInitList;

__attribute__((unused)) static AstStructInitList* struct_init_list_create(Parser* parser) {
    AstStructInitList* list = (AstStructInitList*)arena_alloc(parser->ast_arena,
        sizeof(AstStructInitList), _Alignof(AstStructInitList));
    if (!list) return NULL;

    list->capacity = 8;
    list->count = 0;
    list->fields = (AstStructInit*)arena_alloc(parser->ast_arena,
        sizeof(AstStructInit) * list->capacity, _Alignof(AstStructInit));

    return list;
}

__attribute__((unused)) static void struct_init_list_append(Parser* parser, AstStructInitList* list, const char* field_name, AstNode* value) {
    if (!list) return;

    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        AstStructInit* new_fields = (AstStructInit*)arena_alloc(parser->ast_arena,
            sizeof(AstStructInit) * new_capacity, _Alignof(AstStructInit));
        if (!new_fields) return;

        memcpy(new_fields, list->fields, sizeof(AstStructInit) * list->count);
        list->fields = new_fields;
        list->capacity = new_capacity;
    }

    list->fields[list->count].field_name = field_name;
    list->fields[list->count].value = value;
    list->count++;
}
