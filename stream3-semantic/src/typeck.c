// Type Checker: Type checking and inference
// Purpose: Assign types to all AST nodes and verify type correctness

#include "type.h"
#include "ast.h"
#include "symbol.h"
#include "error.h"
#include "arena.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Type checker context
typedef struct TypeChecker {
    SymbolTable* symbol_table;
    ErrorList* errors;
    Arena* arena;
    Type* current_function_return_type;  // For checking return statements
} TypeChecker;

// Forward declarations
static Type* check_expression(TypeChecker* tc, AstNode* expr);
static void check_statement(TypeChecker* tc, AstNode* stmt);
static Type* resolve_type_from_node(TypeChecker* tc, AstNode* type_node);

// Primitive type singletons
Type* TYPE_I8_SINGLETON = NULL;
Type* TYPE_I16_SINGLETON = NULL;
Type* TYPE_I32_SINGLETON = NULL;
Type* TYPE_I64_SINGLETON = NULL;
Type* TYPE_ISIZE_SINGLETON = NULL;
Type* TYPE_U8_SINGLETON = NULL;
Type* TYPE_U16_SINGLETON = NULL;
Type* TYPE_U32_SINGLETON = NULL;
Type* TYPE_U64_SINGLETON = NULL;
Type* TYPE_USIZE_SINGLETON = NULL;
Type* TYPE_BOOL_SINGLETON = NULL;
Type* TYPE_VOID_SINGLETON = NULL;

void type_init_primitives(Arena* arena) {
    TYPE_I8_SINGLETON = type_new_primitive(TYPE_I8, arena);
    TYPE_I16_SINGLETON = type_new_primitive(TYPE_I16, arena);
    TYPE_I32_SINGLETON = type_new_primitive(TYPE_I32, arena);
    TYPE_I64_SINGLETON = type_new_primitive(TYPE_I64, arena);
    TYPE_ISIZE_SINGLETON = type_new_primitive(TYPE_ISIZE, arena);
    TYPE_U8_SINGLETON = type_new_primitive(TYPE_U8, arena);
    TYPE_U16_SINGLETON = type_new_primitive(TYPE_U16, arena);
    TYPE_U32_SINGLETON = type_new_primitive(TYPE_U32, arena);
    TYPE_U64_SINGLETON = type_new_primitive(TYPE_U64, arena);
    TYPE_USIZE_SINGLETON = type_new_primitive(TYPE_USIZE, arena);
    TYPE_BOOL_SINGLETON = type_new_primitive(TYPE_BOOL, arena);
    TYPE_VOID_SINGLETON = type_new_primitive(TYPE_VOID, arena);
}

// Type constructors

Type* type_new_primitive(TypeKind kind, Arena* arena) {
    Type* t = arena_alloc(arena, sizeof(Type), 8);
    t->kind = kind;
    t->alignment = 0;

    // Set sizes for primitive types
    switch (kind) {
        case TYPE_I8:
        case TYPE_U8:
        case TYPE_BOOL:
            t->size = 1;
            t->alignment = 1;
            break;
        case TYPE_I16:
        case TYPE_U16:
            t->size = 2;
            t->alignment = 2;
            break;
        case TYPE_I32:
        case TYPE_U32:
            t->size = 4;
            t->alignment = 4;
            break;
        case TYPE_I64:
        case TYPE_U64:
            t->size = 8;
            t->alignment = 8;
            break;
        case TYPE_ISIZE:
        case TYPE_USIZE:
            t->size = sizeof(void*);
            t->alignment = sizeof(void*);
            break;
        case TYPE_VOID:
            t->size = 0;
            t->alignment = 1;
            break;
        default:
            t->size = 0;
            break;
    }

    return t;
}

Type* type_new_array(Type* elem, size_t length, Arena* arena) {
    Type* t = arena_alloc(arena, sizeof(Type), 8);
    t->kind = TYPE_ARRAY;
    t->data.array.elem_type = elem;
    t->data.array.length = length;
    t->size = elem->size * length;
    t->alignment = elem->alignment;
    return t;
}

Type* type_new_slice(Type* elem, Arena* arena) {
    Type* t = arena_alloc(arena, sizeof(Type), 8);
    t->kind = TYPE_SLICE;
    t->data.slice.elem_type = elem;
    // Slice is a fat pointer: {ptr, len}
    t->size = sizeof(void*) + sizeof(size_t);
    t->alignment = sizeof(void*);
    return t;
}

Type* type_new_pointer(Type* pointee, Arena* arena) {
    Type* t = arena_alloc(arena, sizeof(Type), 8);
    t->kind = TYPE_POINTER;
    t->data.pointer.pointee_type = pointee;
    t->size = sizeof(void*);
    t->alignment = sizeof(void*);
    return t;
}

Type* type_new_struct(const char* name, Arena* arena) {
    Type* t = arena_alloc(arena, sizeof(Type), 8);
    t->kind = TYPE_STRUCT;
    t->data.struct_type.name = name;
    t->data.struct_type.fields = NULL;
    t->data.struct_type.field_count = 0;
    t->data.struct_type.is_packed = false;
    t->data.struct_type.alignment_override = 0;
    t->size = 0;
    t->alignment = 1;
    return t;
}

Type* type_new_enum(const char* name, Type* underlying, Arena* arena) {
    Type* t = arena_alloc(arena, sizeof(Type), 8);
    t->kind = TYPE_ENUM;
    t->data.enum_type.name = name;
    t->data.enum_type.underlying_type = underlying;
    t->data.enum_type.variants = NULL;
    t->data.enum_type.variant_count = 0;
    t->size = underlying->size;
    t->alignment = underlying->alignment;
    return t;
}

Type* type_new_union(const char* name, Type* tag_type, Arena* arena) {
    Type* t = arena_alloc(arena, sizeof(Type), 8);
    t->kind = TYPE_UNION;
    t->data.union_type.name = name;
    t->data.union_type.tag_type = tag_type;
    t->data.union_type.variants = NULL;
    t->data.union_type.variant_count = 0;
    t->size = 0;
    t->alignment = 1;
    return t;
}

Type* type_new_function(Type* return_type, Arena* arena) {
    Type* t = arena_alloc(arena, sizeof(Type), 8);
    t->kind = TYPE_FUNCTION;
    t->data.function.return_type = return_type;
    t->data.function.params = NULL;
    t->data.function.param_count = 0;
    t->data.function.is_variadic = false;
    t->size = sizeof(void*);  // Function pointer size
    t->alignment = sizeof(void*);
    return t;
}

Type* type_new_result(Type* value_type, Type* error_type, Arena* arena) {
    Type* t = arena_alloc(arena, sizeof(Type), 8);
    t->kind = TYPE_RESULT;
    t->data.result.value_type = value_type;
    t->data.result.error_type = error_type;
    // Result is a tagged union: {tag, union{value, error}}
    size_t value_size = value_type->size;
    size_t error_size = error_type->size;
    size_t max_size = value_size > error_size ? value_size : error_size;
    t->size = sizeof(int) + max_size;  // tag + max(value, error)
    t->alignment = value_type->alignment > error_type->alignment ? value_type->alignment : error_type->alignment;
    return t;
}

Type* type_new_option(Type* inner_type, Arena* arena) {
    Type* t = arena_alloc(arena, sizeof(Type), 8);
    t->kind = TYPE_OPTION;
    t->data.option.inner_type = inner_type;
    // Option is a tagged union: {tag, value}
    t->size = sizeof(int) + inner_type->size;
    t->alignment = inner_type->alignment;
    return t;
}

// Type field operations

void type_struct_add_field(Type* t, const char* name, Type* field_type, Arena* arena) {
    assert(t->kind == TYPE_STRUCT);

    // Grow fields array
    size_t new_count = t->data.struct_type.field_count + 1;
    StructField* new_fields = arena_alloc(arena, sizeof(StructField) * new_count, 8);
    if (t->data.struct_type.fields) {
        memcpy(new_fields, t->data.struct_type.fields, sizeof(StructField) * t->data.struct_type.field_count);
    }

    // Add new field
    new_fields[t->data.struct_type.field_count].name = name;
    new_fields[t->data.struct_type.field_count].type = field_type;
    new_fields[t->data.struct_type.field_count].offset = t->size;  // Simple sequential layout
    new_fields[t->data.struct_type.field_count].is_public = true;

    t->data.struct_type.fields = new_fields;
    t->data.struct_type.field_count = new_count;

    // Update struct size and alignment
    t->size += field_type->size;
    if (field_type->alignment > t->alignment) {
        t->alignment = field_type->alignment;
    }
}

void type_enum_add_variant(Type* t, const char* name, int64_t value, Arena* arena) {
    assert(t->kind == TYPE_ENUM);

    // Grow variants array
    size_t new_count = t->data.enum_type.variant_count + 1;
    EnumVariant* new_variants = arena_alloc(arena, sizeof(EnumVariant) * new_count, 8);
    if (t->data.enum_type.variants) {
        memcpy(new_variants, t->data.enum_type.variants, sizeof(EnumVariant) * t->data.enum_type.variant_count);
    }

    // Add new variant
    new_variants[t->data.enum_type.variant_count].name = name;
    new_variants[t->data.enum_type.variant_count].value = value;

    t->data.enum_type.variants = new_variants;
    t->data.enum_type.variant_count = new_count;
}

void type_union_add_variant(Type* t, const char* name, Type* variant_type, size_t tag_value, Arena* arena) {
    assert(t->kind == TYPE_UNION);

    // Grow variants array
    size_t new_count = t->data.union_type.variant_count + 1;
    UnionVariant* new_variants = arena_alloc(arena, sizeof(UnionVariant) * new_count, 8);
    if (t->data.union_type.variants) {
        memcpy(new_variants, t->data.union_type.variants, sizeof(UnionVariant) * t->data.union_type.variant_count);
    }

    // Add new variant
    new_variants[t->data.union_type.variant_count].name = name;
    new_variants[t->data.union_type.variant_count].type = variant_type;
    new_variants[t->data.union_type.variant_count].tag_value = tag_value;

    t->data.union_type.variants = new_variants;
    t->data.union_type.variant_count = new_count;

    // Update union size (max of all variants)
    if (variant_type->size > t->size) {
        t->size = variant_type->size;
    }
    if (variant_type->alignment > t->alignment) {
        t->alignment = variant_type->alignment;
    }
}

void type_function_add_param(Type* t, const char* name, Type* param_type, Arena* arena) {
    assert(t->kind == TYPE_FUNCTION);

    // Grow params array
    size_t new_count = t->data.function.param_count + 1;
    FunctionParam* new_params = arena_alloc(arena, sizeof(FunctionParam) * new_count, 8);
    if (t->data.function.params) {
        memcpy(new_params, t->data.function.params, sizeof(FunctionParam) * t->data.function.param_count);
    }

    // Add new param
    new_params[t->data.function.param_count].name = name;
    new_params[t->data.function.param_count].type = param_type;

    t->data.function.params = new_params;
    t->data.function.param_count = new_count;
}

// Type queries

bool type_equals(Type* a, Type* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;

    switch (a->kind) {
        case TYPE_I8: case TYPE_I16: case TYPE_I32: case TYPE_I64: case TYPE_ISIZE:
        case TYPE_U8: case TYPE_U16: case TYPE_U32: case TYPE_U64: case TYPE_USIZE:
        case TYPE_BOOL: case TYPE_VOID:
            return true;

        case TYPE_ARRAY:
            return a->data.array.length == b->data.array.length &&
                   type_equals(a->data.array.elem_type, b->data.array.elem_type);

        case TYPE_SLICE:
            return type_equals(a->data.slice.elem_type, b->data.slice.elem_type);

        case TYPE_POINTER:
            return type_equals(a->data.pointer.pointee_type, b->data.pointer.pointee_type);

        case TYPE_STRUCT:
            // For now, just compare by name
            return strcmp(a->data.struct_type.name, b->data.struct_type.name) == 0;

        case TYPE_ENUM:
            return strcmp(a->data.enum_type.name, b->data.enum_type.name) == 0;

        case TYPE_UNION:
            return strcmp(a->data.union_type.name, b->data.union_type.name) == 0;

        case TYPE_FUNCTION:
            if (!type_equals(a->data.function.return_type, b->data.function.return_type))
                return false;
            if (a->data.function.param_count != b->data.function.param_count)
                return false;
            for (size_t i = 0; i < a->data.function.param_count; i++) {
                if (!type_equals(a->data.function.params[i].type, b->data.function.params[i].type))
                    return false;
            }
            return true;

        case TYPE_RESULT:
            return type_equals(a->data.result.value_type, b->data.result.value_type) &&
                   type_equals(a->data.result.error_type, b->data.result.error_type);

        case TYPE_OPTION:
            return type_equals(a->data.option.inner_type, b->data.option.inner_type);

        default:
            return false;
    }
}

bool type_is_integer(Type* t) {
    return t->kind >= TYPE_I8 && t->kind <= TYPE_USIZE;
}

bool type_is_signed_integer(Type* t) {
    return t->kind >= TYPE_I8 && t->kind <= TYPE_ISIZE;
}

bool type_is_unsigned_integer(Type* t) {
    return t->kind >= TYPE_U8 && t->kind <= TYPE_USIZE;
}

bool type_is_numeric(Type* t) {
    return type_is_integer(t);
}

bool type_is_pointer_like(Type* t) {
    return t->kind == TYPE_POINTER || t->kind == TYPE_SLICE;
}

bool type_is_composite(Type* t) {
    return t->kind == TYPE_STRUCT || t->kind == TYPE_UNION || t->kind == TYPE_ARRAY;
}

size_t type_sizeof(Type* t) {
    return t->size;
}

size_t type_alignof(Type* t) {
    return t->alignment;
}

bool type_is_assignable_to(Type* from, Type* to) {
    // For now, require exact type match
    // Later could add implicit conversions
    return type_equals(from, to);
}

bool type_can_cast_to(Type* from, Type* to) {
    // Allow casts between integers
    if (type_is_integer(from) && type_is_integer(to)) {
        return true;
    }
    // Allow casts between pointers
    if (from->kind == TYPE_POINTER && to->kind == TYPE_POINTER) {
        return true;
    }
    // Allow integer to pointer and vice versa
    if (type_is_integer(from) && to->kind == TYPE_POINTER) {
        return true;
    }
    if (from->kind == TYPE_POINTER && type_is_integer(to)) {
        return true;
    }
    return false;
}

const char* type_to_string(Type* t, Arena* arena) {
    (void)arena;  // Unused for now, but reserved for future use
    if (!t) return "<null>";

    // Simple type to string conversion
    switch (t->kind) {
        case TYPE_I8: return "i8";
        case TYPE_I16: return "i16";
        case TYPE_I32: return "i32";
        case TYPE_I64: return "i64";
        case TYPE_ISIZE: return "isize";
        case TYPE_U8: return "u8";
        case TYPE_U16: return "u16";
        case TYPE_U32: return "u32";
        case TYPE_U64: return "u64";
        case TYPE_USIZE: return "usize";
        case TYPE_BOOL: return "bool";
        case TYPE_VOID: return "void";
        case TYPE_POINTER:
            return "*T";
        case TYPE_SLICE:
            return "[]T";
        case TYPE_ARRAY:
            return "[N]T";
        case TYPE_STRUCT:
            return t->data.struct_type.name;
        case TYPE_ENUM:
            return t->data.enum_type.name;
        case TYPE_UNION:
            return t->data.union_type.name;
        case TYPE_FUNCTION:
            return "fn";
        case TYPE_RESULT:
            return "Result<T,E>";
        case TYPE_OPTION:
            return "Option<T>";
        default:
            return "<unknown>";
    }
}

// Type resolution from AST nodes

static Type* resolve_type_from_node(TypeChecker* tc, AstNode* type_node) {
    if (!type_node) {
        return TYPE_VOID_SINGLETON;
    }

    // Cast to AstTypeNode for type-specific data access
    AstTypeNode* tnode = (AstTypeNode*)type_node;

    switch (type_node->kind) {
        case AST_TYPE_PRIMITIVE: {
            const char* name = tnode->data.primitive.name;

            if (strcmp(name, "i8") == 0) return TYPE_I8_SINGLETON;
            if (strcmp(name, "i16") == 0) return TYPE_I16_SINGLETON;
            if (strcmp(name, "i32") == 0) return TYPE_I32_SINGLETON;
            if (strcmp(name, "i64") == 0) return TYPE_I64_SINGLETON;
            if (strcmp(name, "isize") == 0) return TYPE_ISIZE_SINGLETON;
            if (strcmp(name, "u8") == 0) return TYPE_U8_SINGLETON;
            if (strcmp(name, "u16") == 0) return TYPE_U16_SINGLETON;
            if (strcmp(name, "u32") == 0) return TYPE_U32_SINGLETON;
            if (strcmp(name, "u64") == 0) return TYPE_U64_SINGLETON;
            if (strcmp(name, "usize") == 0) return TYPE_USIZE_SINGLETON;
            if (strcmp(name, "bool") == 0) return TYPE_BOOL_SINGLETON;
            if (strcmp(name, "void") == 0) return TYPE_VOID_SINGLETON;

            error_list_add(tc->errors, ERROR_TYPE, type_node->loc,
                          "Unknown primitive type: %s", name);
            return NULL;
        }

        case AST_TYPE_ARRAY: {
            Type* elem_type = resolve_type_from_node(tc, tnode->data.array.element_type);
            if (!elem_type) return NULL;
            // Note: size_expr would need evaluation, using 0 for now
            Type* array_type = type_new_array(elem_type, 0, tc->arena);
            type_node->type = array_type;
            return array_type;
        }

        case AST_TYPE_SLICE: {
            Type* elem_type = resolve_type_from_node(tc, tnode->data.slice.element_type);
            if (!elem_type) return NULL;
            Type* slice_type = type_new_slice(elem_type, tc->arena);
            type_node->type = slice_type;
            return slice_type;
        }

        case AST_TYPE_POINTER: {
            Type* pointee_type = resolve_type_from_node(tc, tnode->data.pointer.pointee_type);
            if (!pointee_type) return NULL;
            Type* ptr_type = type_new_pointer(pointee_type, tc->arena);
            type_node->type = ptr_type;
            return ptr_type;
        }

        case AST_TYPE_FUNCTION: {
            Type* return_type = resolve_type_from_node(tc, tnode->data.function.return_type);
            Type* func_type = type_new_function(return_type, tc->arena);

            for (size_t i = 0; i < tnode->data.function.param_count; i++) {
                Type* param_type = resolve_type_from_node(tc, tnode->data.function.param_types[i]);
                if (!param_type) continue;
                type_function_add_param(func_type, NULL, param_type, tc->arena);
            }

            type_node->type = func_type;
            return func_type;
        }

        case AST_TYPE_RESULT: {
            Type* value_type = resolve_type_from_node(tc, tnode->data.result.value_type);
            Type* error_type = resolve_type_from_node(tc, tnode->data.result.error_type);
            if (!value_type || !error_type) return NULL;
            Type* result_type = type_new_result(value_type, error_type, tc->arena);
            type_node->type = result_type;
            return result_type;
        }

        case AST_TYPE_NAMED: {
            const char* name = tnode->data.named.name;

            if (!name) {
                error_list_add(tc->errors, ERROR_TYPE, type_node->loc,
                              "Type name is NULL");
                return TYPE_VOID_SINGLETON;
            }

            // Check for primitive types
            if (strcmp(name, "i8") == 0) return TYPE_I8_SINGLETON;
            if (strcmp(name, "i16") == 0) return TYPE_I16_SINGLETON;
            if (strcmp(name, "i32") == 0) return TYPE_I32_SINGLETON;
            if (strcmp(name, "i64") == 0) return TYPE_I64_SINGLETON;
            if (strcmp(name, "isize") == 0) return TYPE_ISIZE_SINGLETON;
            if (strcmp(name, "u8") == 0) return TYPE_U8_SINGLETON;
            if (strcmp(name, "u16") == 0) return TYPE_U16_SINGLETON;
            if (strcmp(name, "u32") == 0) return TYPE_U32_SINGLETON;
            if (strcmp(name, "u64") == 0) return TYPE_U64_SINGLETON;
            if (strcmp(name, "usize") == 0) return TYPE_USIZE_SINGLETON;
            if (strcmp(name, "bool") == 0) return TYPE_BOOL_SINGLETON;
            if (strcmp(name, "void") == 0) return TYPE_VOID_SINGLETON;

            // TODO: proper symbol resolution for user-defined types
            error_list_add(tc->errors, ERROR_TYPE, type_node->loc,
                          "Unknown type: %s", name);
            Type* named_type = TYPE_VOID_SINGLETON; // placeholder
            type_node->type = named_type;
            return named_type;
        }

        default:
            error_list_add(tc->errors, ERROR_TYPE, type_node->loc,
                          "Unexpected type node kind: %d", type_node->kind);
            return NULL;
    }
}

// Type checking for declarations

static void check_struct_decl(TypeChecker* tc, AstNode* node) {
    assert(node->kind == AST_STRUCT_DECL);

    // Create struct type
    Type* struct_type = type_new_struct(node->data.struct_decl.name, tc->arena);
    struct_type->data.struct_type.is_packed = node->data.struct_decl.is_packed;

    // Resolve and add fields
    for (size_t i = 0; i < node->data.struct_decl.field_count; i++) {
        AstField* field = &node->data.struct_decl.fields[i];
        Type* field_type = resolve_type_from_node(tc, field->type);
        if (field_type) {
            type_struct_add_field(struct_type, field->name, field_type, tc->arena);
        }
    }

    node->type = struct_type;
}

static void check_enum_decl(TypeChecker* tc, AstNode* node) {
    assert(node->kind == AST_ENUM_DECL);

    // Resolve underlying type (default to i32)
    Type* underlying_type = TYPE_I32_SINGLETON;
    if (node->data.enum_decl.underlying_type) {
        underlying_type = resolve_type_from_node(tc, node->data.enum_decl.underlying_type);
        if (!underlying_type || !type_is_integer(underlying_type)) {
            error_list_add(tc->errors, ERROR_TYPE, node->loc,
                          "Enum underlying type must be an integer type");
            underlying_type = TYPE_I32_SINGLETON;
        }
    }

    // Create enum type
    Type* enum_type = type_new_enum(node->data.enum_decl.name, underlying_type, tc->arena);

    // Add variants
    int64_t next_value = 0;
    for (size_t i = 0; i < node->data.enum_decl.value_count; i++) {
        AstEnumValue* value = &node->data.enum_decl.values[i];
        int64_t val = value->value ? 1 : next_value;  // Simplified: would need to evaluate value->value
        type_enum_add_variant(enum_type, value->name, val, tc->arena);
        next_value = val + 1;
    }

    node->type = enum_type;
}

static void check_union_decl(TypeChecker* tc, AstNode* node) {
    assert(node->kind == AST_UNION_DECL);

    // For tagged unions, we need a tag type (usually an enum)
    // Resolve tag type if present, otherwise use i32
    Type* tag_type = TYPE_I32_SINGLETON;
    if (node->data.union_decl.tag_type) {
        tag_type = resolve_type_from_node(tc, node->data.union_decl.tag_type);
        if (!tag_type) {
            tag_type = TYPE_I32_SINGLETON;
        }
    }

    Type* union_type = type_new_union(node->data.union_decl.name, tag_type, tc->arena);

    // Resolve and add variants (union uses fields, not variants)
    for (size_t i = 0; i < node->data.union_decl.field_count; i++) {
        AstField* field = &node->data.union_decl.fields[i];
        Type* variant_type = resolve_type_from_node(tc, field->type);
        if (variant_type) {
            type_union_add_variant(union_type, field->name, variant_type, i, tc->arena);
        }
    }

    node->type = union_type;
}

static void check_function_decl(TypeChecker* tc, AstNode* node) {
    assert(node->kind == AST_FUNCTION_DECL);

    // Resolve return type
    Type* return_type = TYPE_VOID_SINGLETON;
    if (node->data.function_decl.return_type) {
        return_type = resolve_type_from_node(tc, node->data.function_decl.return_type);
        if (!return_type) return_type = TYPE_VOID_SINGLETON;
        // Set type on the return type node
        node->data.function_decl.return_type->type = return_type;
    }

    // Create function type
    Type* func_type = type_new_function(return_type, tc->arena);

    // Resolve parameter types
    for (size_t i = 0; i < node->data.function_decl.param_count; i++) {
        AstParam* param = &node->data.function_decl.params[i];
        Type* param_type = resolve_type_from_node(tc, param->type);
        if (param_type) {
            // Set type on the parameter type node
            param->type->type = param_type;
            type_function_add_param(func_type, param->name, param_type, tc->arena);
        }
    }

    node->type = func_type;

    // Check function body
    Type* saved_return_type = tc->current_function_return_type;
    tc->current_function_return_type = return_type;

    if (node->data.function_decl.body) {
        check_statement(tc, node->data.function_decl.body);
    }

    tc->current_function_return_type = saved_return_type;
}

// Type checking for statements

static void check_let_stmt(TypeChecker* tc, AstNode* node) {
    assert(node->kind == AST_LET_STMT);

    // Check initializer type
    Type* init_type = NULL;
    if (node->data.let_decl.init) {
        init_type = check_expression(tc, node->data.let_decl.init);
    }

    // Resolve declared type if present
    Type* decl_type = NULL;
    if (node->data.let_decl.type) {
        decl_type = resolve_type_from_node(tc, node->data.let_decl.type);
    }

    // Determine final type
    Type* final_type = NULL;
    if (decl_type && init_type) {
        // Both type and initializer present - check compatibility
        if (!type_is_assignable_to(init_type, decl_type)) {
            error_list_add(tc->errors, ERROR_TYPE, node->loc,
                          "Type mismatch: cannot assign %s to %s",
                          type_to_string(init_type, tc->arena),
                          type_to_string(decl_type, tc->arena));
        }
        final_type = decl_type;
    } else if (decl_type) {
        // Only type present
        final_type = decl_type;
    } else if (init_type) {
        // Only initializer present - infer type
        final_type = init_type;
    } else {
        error_list_add(tc->errors, ERROR_TYPE, node->loc,
                      "Cannot infer type without initializer or type annotation");
        return;
    }

    node->type = final_type;
}

static void check_var_stmt(TypeChecker* tc, AstNode* node) {
    assert(node->kind == AST_VAR_STMT);

    // Check initializer type if present
    Type* init_type = NULL;
    if (node->data.var_decl.init) {
        init_type = check_expression(tc, node->data.var_decl.init);
    }

    // Resolve declared type if present
    Type* decl_type = NULL;
    if (node->data.var_decl.type) {
        decl_type = resolve_type_from_node(tc, node->data.var_decl.type);
    }

    // Determine final type
    Type* final_type = NULL;
    if (decl_type && init_type) {
        // Both type and initializer present - check compatibility
        if (!type_is_assignable_to(init_type, decl_type)) {
            error_list_add(tc->errors, ERROR_TYPE, node->loc,
                          "Type mismatch: cannot assign %s to %s",
                          type_to_string(init_type, tc->arena),
                          type_to_string(decl_type, tc->arena));
        }
        final_type = decl_type;
    } else if (decl_type) {
        // Only type present
        final_type = decl_type;
    } else if (init_type) {
        // Only initializer present - infer type
        final_type = init_type;
    } else {
        // Var without initializer requires explicit type
        error_list_add(tc->errors, ERROR_TYPE, node->loc,
                      "Variable declaration requires type annotation or initializer");
        return;
    }

    node->type = final_type;
}

static void check_statement(TypeChecker* tc, AstNode* stmt) {
    switch (stmt->kind) {
        case AST_LET_STMT:
            check_let_stmt(tc, stmt);
            break;
        case AST_VAR_STMT:
            check_var_stmt(tc, stmt);
            break;
        case AST_RETURN_STMT: {
            Type* ret_type = TYPE_VOID_SINGLETON;
            if (stmt->data.return_stmt.value) {
                ret_type = check_expression(tc, stmt->data.return_stmt.value);
            }
            if (!type_is_assignable_to(ret_type, tc->current_function_return_type)) {
                error_list_add(tc->errors, ERROR_TYPE, stmt->loc,
                              "Return type mismatch: expected %s, got %s",
                              type_to_string(tc->current_function_return_type, tc->arena),
                              type_to_string(ret_type, tc->arena));
            }
            break;
        }
        case AST_IF_STMT: {
            Type* cond_type = check_expression(tc, stmt->data.if_stmt.condition);
            if (!type_equals(cond_type, TYPE_BOOL_SINGLETON)) {
                error_list_add(tc->errors, ERROR_TYPE, stmt->data.if_stmt.condition->loc,
                              "If condition must be bool, got %s",
                              type_to_string(cond_type, tc->arena));
            }
            check_statement(tc, stmt->data.if_stmt.then_block);
            if (stmt->data.if_stmt.else_block) {
                check_statement(tc, stmt->data.if_stmt.else_block);
            }
            break;
        }
        case AST_FOR_STMT: {
            if (stmt->data.for_stmt.iterable) {
                Type* coll_type = check_expression(tc, stmt->data.for_stmt.iterable);
                // TODO: Check that collection is iterable (array or slice)
                (void)coll_type;
            }
            if (stmt->data.for_stmt.condition) {
                Type* cond_type = check_expression(tc, stmt->data.for_stmt.condition);
                if (!type_equals(cond_type, TYPE_BOOL_SINGLETON)) {
                    error_list_add(tc->errors, ERROR_TYPE, stmt->data.for_stmt.condition->loc,
                                  "For condition must be bool, got %s",
                                  type_to_string(cond_type, tc->arena));
                }
            }
            check_statement(tc, stmt->data.for_stmt.body);
            break;
        }
        case AST_SWITCH_STMT: {
            Type* value_type = check_expression(tc, stmt->data.switch_stmt.value);
            for (size_t i = 0; i < stmt->data.switch_stmt.case_count; i++) {
                AstSwitchCase* case_item = &stmt->data.switch_stmt.cases[i];
                // Check case values
                for (size_t j = 0; j < case_item->value_count; j++) {
                    if (case_item->values[j]) {
                        Type* case_type = check_expression(tc, case_item->values[j]);
                        if (!type_equals(case_type, value_type)) {
                            error_list_add(tc->errors, ERROR_TYPE, case_item->values[j]->loc,
                                          "Case value type mismatch: expected %s, got %s",
                                          type_to_string(value_type, tc->arena),
                                          type_to_string(case_type, tc->arena));
                        }
                    }
                }
                // Check case statements
                for (size_t j = 0; j < case_item->stmt_count; j++) {
                    check_statement(tc, case_item->stmts[j]);
                }
            }
            break;
        }
        case AST_DEFER_STMT:
            check_statement(tc, stmt->data.defer_stmt.stmt);
            break;
        case AST_ERRDEFER_STMT:
            check_statement(tc, stmt->data.defer_stmt.stmt);
            break;
        case AST_SUSPEND_STMT:
            // Nothing to check
            break;
        case AST_RESUME_STMT:
            check_expression(tc, stmt->data.resume_stmt.coro);
            break;
        case AST_BLOCK_STMT: {
            for (size_t i = 0; i < stmt->data.block_stmt.stmt_count; i++) {
                check_statement(tc, stmt->data.block_stmt.stmts[i]);
            }
            break;
        }
        case AST_EXPR_STMT:
            check_expression(tc, stmt->data.expr_stmt.expr);
            break;
        case AST_ASSIGN_STMT: {
            Type* target_type = check_expression(tc, stmt->data.assign_stmt.lhs);
            Type* value_type = check_expression(tc, stmt->data.assign_stmt.rhs);
            if (!type_is_assignable_to(value_type, target_type)) {
                error_list_add(tc->errors, ERROR_TYPE, stmt->loc,
                              "Assignment type mismatch: cannot assign %s to %s",
                              type_to_string(value_type, tc->arena),
                              type_to_string(target_type, tc->arena));
            }
            break;
        }
        case AST_BREAK_STMT:
        case AST_CONTINUE_STMT:
            // Nothing to check
            break;
        default:
            error_list_add(tc->errors, ERROR_TYPE, stmt->loc,
                          "Unexpected statement kind: %d", stmt->kind);
            break;
    }
}

// Type checking for expressions

static Type* check_expression(TypeChecker* tc, AstNode* expr) {
    switch (expr->kind) {
        case AST_LITERAL_EXPR: {
            switch (expr->data.literal_expr.literal_kind) {
                case LITERAL_INT:
                    expr->type = TYPE_I64_SINGLETON;  // Default to i64 for integer literals
                    return expr->type;
                case LITERAL_BOOL:
                    expr->type = TYPE_BOOL_SINGLETON;
                    return expr->type;
                case LITERAL_STRING:
                    // String literal is a slice of u8
                    expr->type = type_new_slice(TYPE_U8_SINGLETON, tc->arena);
                    return expr->type;
                default:
                    return NULL;
            }
        }

        case AST_IDENTIFIER_EXPR: {
            // Look up identifier in symbol table
            Symbol* sym = scope_lookup(tc->symbol_table->module_scope, expr->data.identifier_expr.name);
            if (!sym) {
                error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                              "Undefined identifier: %s", expr->data.identifier_expr.name);
                return NULL;
            }

            // Get type from symbol
            Type* sym_type = sym->type;

            // For functions, get type from ast_node if not set on symbol
            if (!sym_type && sym->kind == SYMBOL_FUNCTION && sym->data.function.ast_node) {
                sym_type = sym->data.function.ast_node->type;
            }

            if (!sym_type) {
                error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                              "Symbol has no type: %s", expr->data.identifier_expr.name);
                return NULL;
            }

            expr->type = sym_type;
            return expr->type;
        }

        case AST_BINARY_EXPR: {
            Type* left_type = check_expression(tc, expr->data.binary_expr.left);
            Type* right_type = check_expression(tc, expr->data.binary_expr.right);

            if (!left_type || !right_type) return NULL;

            // Type check binary operation
            switch (expr->data.binary_expr.op) {
                case BINOP_ADD: case BINOP_SUB: case BINOP_MUL: case BINOP_DIV: case BINOP_MOD:
                case BINOP_AND: case BINOP_OR: case BINOP_XOR:
                case BINOP_LSHIFT: case BINOP_RSHIFT:
                    // Arithmetic and bitwise operations require numeric types
                    if (!type_is_numeric(left_type) || !type_is_numeric(right_type)) {
                        error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                                      "Arithmetic operation requires numeric types");
                        return NULL;
                    }
                    if (!type_equals(left_type, right_type)) {
                        error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                                      "Type mismatch in binary operation: %s vs %s",
                                      type_to_string(left_type, tc->arena),
                                      type_to_string(right_type, tc->arena));
                        return NULL;
                    }
                    expr->type = left_type;
                    return expr->type;

                case BINOP_LOGICAL_AND: case BINOP_LOGICAL_OR:
                    // Logical operations require bool
                    if (!type_equals(left_type, TYPE_BOOL_SINGLETON) || !type_equals(right_type, TYPE_BOOL_SINGLETON)) {
                        error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                                      "Logical operation requires bool operands");
                        return NULL;
                    }
                    expr->type = TYPE_BOOL_SINGLETON;
                    return expr->type;

                case BINOP_EQ_EQ: case BINOP_BANG_EQ:
                case BINOP_LT: case BINOP_LT_EQ: case BINOP_GT: case BINOP_GT_EQ:
                    // Comparison operations
                    if (!type_equals(left_type, right_type)) {
                        error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                                      "Type mismatch in comparison: %s vs %s",
                                      type_to_string(left_type, tc->arena),
                                      type_to_string(right_type, tc->arena));
                        return NULL;
                    }
                    expr->type = TYPE_BOOL_SINGLETON;
                    return expr->type;

                default:
                    return NULL;
            }
        }

        case AST_UNARY_EXPR: {
            Type* operand_type = check_expression(tc, expr->data.unary_expr.operand);
            if (!operand_type) return NULL;

            switch (expr->data.unary_expr.op) {
                case UNOP_NEG:
                    if (!type_is_numeric(operand_type)) {
                        error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                                      "Unary negation requires numeric type");
                        return NULL;
                    }
                    expr->type = operand_type;
                    return expr->type;

                case UNOP_NOT:
                    if (!type_equals(operand_type, TYPE_BOOL_SINGLETON)) {
                        error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                                      "Logical not requires bool type");
                        return NULL;
                    }
                    expr->type = TYPE_BOOL_SINGLETON;
                    return expr->type;

                case UNOP_BIT_NOT:
                    if (!type_is_integer(operand_type)) {
                        error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                                      "Bitwise not requires integer type");
                        return NULL;
                    }
                    expr->type = operand_type;
                    return expr->type;

                case UNOP_ADDR:
                    expr->type = type_new_pointer(operand_type, tc->arena);
                    return expr->type;

                case UNOP_DEREF:
                    if (operand_type->kind != TYPE_POINTER) {
                        error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                                      "Dereference requires pointer type");
                        return NULL;
                    }
                    expr->type = operand_type->data.pointer.pointee_type;
                    return expr->type;

                default:
                    return NULL;
            }
        }

        case AST_CALL_EXPR: {
            Type* callee_type = check_expression(tc, expr->data.call_expr.callee);
            if (!callee_type || callee_type->kind != TYPE_FUNCTION) {
                error_list_add(tc->errors, ERROR_TYPE, expr->data.call_expr.callee->loc,
                              "Expression is not callable");
                return NULL;
            }

            // Check argument count
            if (expr->data.call_expr.arg_count != callee_type->data.function.param_count) {
                error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                              "Function expects %zu arguments, got %zu",
                              callee_type->data.function.param_count, expr->data.call_expr.arg_count);
            }

            // Check argument types
            for (size_t i = 0; i < expr->data.call_expr.arg_count && i < callee_type->data.function.param_count; i++) {
                Type* arg_type = check_expression(tc, expr->data.call_expr.args[i]);
                Type* param_type = callee_type->data.function.params[i].type;
                if (!type_is_assignable_to(arg_type, param_type)) {
                    error_list_add(tc->errors, ERROR_TYPE, expr->data.call_expr.args[i]->loc,
                                  "Argument type mismatch: expected %s, got %s",
                                  type_to_string(param_type, tc->arena),
                                  type_to_string(arg_type, tc->arena));
                }
            }

            expr->type = callee_type->data.function.return_type;
            return expr->type;
        }

        case AST_ASYNC_CALL_EXPR: {
            // Similar to regular call, but wraps result in coroutine type
            Type* callee_type = check_expression(tc, expr->data.async_call_expr.callee);
            if (!callee_type || callee_type->kind != TYPE_FUNCTION) {
                error_list_add(tc->errors, ERROR_TYPE, expr->data.async_call_expr.callee->loc,
                              "Expression is not callable");
                return NULL;
            }

            // Check arguments (same as regular call)
            if (expr->data.async_call_expr.arg_count != callee_type->data.function.param_count) {
                error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                              "Function expects %zu arguments, got %zu",
                              callee_type->data.function.param_count, expr->data.async_call_expr.arg_count);
            }

            for (size_t i = 0; i < expr->data.async_call_expr.arg_count && i < callee_type->data.function.param_count; i++) {
                Type* arg_type = check_expression(tc, expr->data.async_call_expr.args[i]);
                Type* param_type = callee_type->data.function.params[i].type;
                if (!type_is_assignable_to(arg_type, param_type)) {
                    error_list_add(tc->errors, ERROR_TYPE, expr->data.async_call_expr.args[i]->loc,
                                  "Argument type mismatch: expected %s, got %s",
                                  type_to_string(param_type, tc->arena),
                                  type_to_string(arg_type, tc->arena));
                }
            }

            // For async call, return type is the same (coroutine analysis handles the rest)
            expr->type = callee_type->data.function.return_type;
            return expr->type;
        }

        case AST_FIELD_ACCESS_EXPR: {
            Type* object_type = check_expression(tc, expr->data.field_access_expr.object);
            if (!object_type) return NULL;

            if (object_type->kind != TYPE_STRUCT) {
                error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                              "Field access requires struct type, got %s",
                              type_to_string(object_type, tc->arena));
                return NULL;
            }

            // Find field
            for (size_t i = 0; i < object_type->data.struct_type.field_count; i++) {
                if (strcmp(object_type->data.struct_type.fields[i].name, expr->data.field_access_expr.field_name) == 0) {
                    expr->type = object_type->data.struct_type.fields[i].type;
                    return expr->type;
                }
            }

            error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                          "No field '%s' in struct '%s'",
                          expr->data.field_access_expr.field_name, object_type->data.struct_type.name);
            return NULL;
        }

        case AST_INDEX_EXPR: {
            Type* array_type = check_expression(tc, expr->data.index_expr.array);
            Type* index_type = check_expression(tc, expr->data.index_expr.index);

            if (!array_type || !index_type) return NULL;

            if (array_type->kind != TYPE_ARRAY && array_type->kind != TYPE_SLICE) {
                error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                              "Index operation requires array or slice type");
                return NULL;
            }

            if (!type_is_integer(index_type)) {
                error_list_add(tc->errors, ERROR_TYPE, expr->data.index_expr.index->loc,
                              "Array index must be integer type");
                return NULL;
            }

            Type* elem_type = array_type->kind == TYPE_ARRAY ?
                             array_type->data.array.elem_type :
                             array_type->data.slice.elem_type;
            expr->type = elem_type;
            return expr->type;
        }

        case AST_CAST_EXPR: {
            Type* src_type = check_expression(tc, expr->data.cast_expr.expr);
            Type* dst_type = resolve_type_from_node(tc, expr->data.cast_expr.type);

            if (!src_type || !dst_type) return NULL;

            if (!type_can_cast_to(src_type, dst_type)) {
                error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                              "Cannot cast from %s to %s",
                              type_to_string(src_type, tc->arena),
                              type_to_string(dst_type, tc->arena));
                return NULL;
            }

            expr->type = dst_type;
            return expr->type;
        }

        case AST_TRY_EXPR: {
            Type* result_type = check_expression(tc, expr->data.try_expr.expr);

            if (!result_type || result_type->kind != TYPE_RESULT) {
                error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                              "Try operator requires Result type, got %s",
                              type_to_string(result_type, tc->arena));
                return NULL;
            }

            // Unwrap Result<T, E> to T
            expr->type = result_type->data.result.value_type;
            return expr->type;
        }

        default:
            error_list_add(tc->errors, ERROR_TYPE, expr->loc,
                          "Unexpected expression kind: %d", expr->kind);
            return NULL;
    }
}

// Public API

void typeck_check_module(AstNode* module_node, SymbolTable* symbol_table,
                        ErrorList* errors, Arena* arena) {
    assert(module_node->kind == AST_MODULE);

    // Initialize primitives if not already done
    if (!TYPE_I32_SINGLETON) {
        type_init_primitives(arena);
    }

    TypeChecker tc = {
        .symbol_table = symbol_table,
        .errors = errors,
        .arena = arena,
        .current_function_return_type = NULL
    };

    // First pass: check type declarations
    for (size_t i = 0; i < module_node->data.module.decl_count; i++) {
        AstNode* decl = module_node->data.module.decls[i];
        switch (decl->kind) {
            case AST_STRUCT_DECL:
                check_struct_decl(&tc, decl);
                break;
            case AST_ENUM_DECL:
                check_enum_decl(&tc, decl);
                break;
            case AST_UNION_DECL:
                check_union_decl(&tc, decl);
                break;
            default:
                break;
        }
    }

    // Second pass: check function declarations
    for (size_t i = 0; i < module_node->data.module.decl_count; i++) {
        AstNode* decl = module_node->data.module.decls[i];
        if (decl->kind == AST_FUNCTION_DECL) {
            check_function_decl(&tc, decl);
        }
    }
}
