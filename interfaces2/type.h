#ifndef TYPE_H
#define TYPE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

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

// Forward declare Arena
typedef struct Arena Arena;

// Struct field definition
typedef struct StructField {
    const char* name;           // Pointer into string pool
    struct Type* type;
    size_t offset;              // Offset within struct
    bool is_public;
} StructField;

// Enum variant definition
typedef struct EnumVariant {
    const char* name;           // Pointer into string pool
    int64_t value;              // Explicit value
} EnumVariant;

// Union variant definition
typedef struct UnionVariant {
    const char* name;           // Pointer into string pool
    struct Type* type;
    size_t tag_value;           // Tag value for tagged union
} UnionVariant;

// Function parameter definition
typedef struct FunctionParam {
    const char* name;           // Pointer into string pool (can be NULL for types only)
    struct Type* type;
} FunctionParam;

typedef struct Type {
    TypeKind kind;
    size_t size;        // Size in bytes (0 if not yet computed)
    size_t alignment;

    union {
        // Array type: T[N]
        struct {
            struct Type* elem_type;
            size_t length;
        } array;

        // Slice type: []T
        struct {
            struct Type* elem_type;
        } slice;

        // Pointer type: *T
        struct {
            struct Type* pointee_type;
        } pointer;

        // Struct type
        struct {
            const char* name;           // Pointer into string pool
            StructField* fields;
            size_t field_count;
            bool is_packed;
            size_t alignment_override;  // 0 if no override
        } struct_type;

        // Enum type: enum(underlying_type)
        struct {
            const char* name;           // Pointer into string pool
            struct Type* underlying_type;  // i8, i16, i32, i64, u8, u16, u32, u64
            EnumVariant* variants;
            size_t variant_count;
        } enum_type;

        // Union type (tagged)
        struct {
            const char* name;           // Pointer into string pool
            UnionVariant* variants;
            size_t variant_count;
            struct Type* tag_type;      // Type of the tag (usually an enum)
        } union_type;

        // Function type: fn(params) -> return_type
        struct {
            FunctionParam* params;
            size_t param_count;
            struct Type* return_type;
            bool is_variadic;
        } function;

        // Result type: Result<T, E>
        struct {
            struct Type* value_type;
            struct Type* error_type;
        } result;

        // Option type: Option<T>
        struct {
            struct Type* inner_type;
        } option;
    } data;
} Type;

// Type builders (allocate from arena)
Type* type_new_primitive(TypeKind kind, Arena* arena);
Type* type_new_array(Type* elem, size_t length, Arena* arena);
Type* type_new_slice(Type* elem, Arena* arena);
Type* type_new_pointer(Type* pointee, Arena* arena);
Type* type_new_struct(const char* name, Arena* arena);
Type* type_new_enum(const char* name, Type* underlying, Arena* arena);
Type* type_new_union(const char* name, Type* tag_type, Arena* arena);
Type* type_new_function(Type* return_type, Arena* arena);
Type* type_new_result(Type* value_type, Type* error_type, Arena* arena);
Type* type_new_option(Type* inner_type, Arena* arena);

// Struct/enum/union field operations
void type_struct_add_field(Type* t, const char* name, Type* field_type, Arena* arena);
void type_enum_add_variant(Type* t, const char* name, int64_t value, Arena* arena);
void type_union_add_variant(Type* t, const char* name, Type* variant_type, size_t tag_value, Arena* arena);
void type_function_add_param(Type* t, const char* name, Type* param_type, Arena* arena);

// Type queries
bool type_equals(Type* a, Type* b);
bool type_is_integer(Type* t);
bool type_is_signed_integer(Type* t);
bool type_is_unsigned_integer(Type* t);
bool type_is_numeric(Type* t);
bool type_is_pointer_like(Type* t);  // Pointer or slice
bool type_is_composite(Type* t);     // Struct, union, array

// Type properties
size_t type_sizeof(Type* t);
size_t type_alignof(Type* t);
const char* type_to_string(Type* t, Arena* arena);

// Type checking helpers
bool type_is_assignable_to(Type* from, Type* to);
bool type_can_cast_to(Type* from, Type* to);

// Primitive type singletons (initialized once)
extern Type* TYPE_I8_SINGLETON;
extern Type* TYPE_I16_SINGLETON;
extern Type* TYPE_I32_SINGLETON;
extern Type* TYPE_I64_SINGLETON;
extern Type* TYPE_ISIZE_SINGLETON;
extern Type* TYPE_U8_SINGLETON;
extern Type* TYPE_U16_SINGLETON;
extern Type* TYPE_U32_SINGLETON;
extern Type* TYPE_U64_SINGLETON;
extern Type* TYPE_USIZE_SINGLETON;
extern Type* TYPE_BOOL_SINGLETON;
extern Type* TYPE_VOID_SINGLETON;

// Initialize primitive singletons
void type_init_primitives(Arena* arena);

#endif // TYPE_H
