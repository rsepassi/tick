#ifndef TYPE_H
#define TYPE_H

#include <stddef.h>
#include <stdbool.h>

// Type System Interface
// Purpose: Shared type representation across all semantic phases

typedef enum {
    TYPE_I8, TYPE_I16, TYPE_I32, TYPE_I64, TYPE_ISIZE,
    TYPE_U8, TYPE_U16, TYPE_U32, TYPE_U64, TYPE_USIZE,
    TYPE_BOOL,
    TYPE_VOID,
    TYPE_ARRAY,
    TYPE_SLICE,
    TYPE_POINTER,
    TYPE_STRUCT,
    TYPE_ENUM,
    TYPE_UNION,
    TYPE_FUNCTION,
    TYPE_RESULT,
    TYPE_OPTION,
} TypeKind;

typedef struct StructField StructField;
typedef struct EnumVariant EnumVariant;

typedef struct Type {
    TypeKind kind;
    size_t size;        // Size in bytes (0 if not yet computed)
    size_t alignment;

    union {
        struct {
            struct Type* elem_type;
            size_t length;
        } array;

        struct {
            struct Type* elem_type;
        } slice;

        struct {
            struct Type* pointee_type;
        } pointer;

        struct {
            const char* name;
            struct StructField* fields;
            size_t field_count;
            bool is_packed;
        } struct_type;

        struct {
            const char* name;
            struct Type* underlying_type;
            struct EnumVariant* variants;
            size_t variant_count;
        } enum_type;

        struct {
            struct Type* value_type;
            struct Type* error_type;
        } result;

        // ... etc
    } data;
} Type;

// Forward declare Arena
typedef struct Arena Arena;
typedef struct SymbolTable SymbolTable;

// Type builders (allocate from arena)
void type_init_primitive(Type* t, TypeKind kind, Arena* arena);
void type_init_array(Type* t, Type* elem, size_t length, Arena* arena);
void type_init_slice(Type* t, Type* elem, Arena* arena);

// Type utilities
bool type_equals(Type* a, Type* b);
size_t type_sizeof(Type* t);
size_t type_alignof(Type* t);

#endif // TYPE_H
