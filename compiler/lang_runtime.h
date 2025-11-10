/* Runtime support for async systems language */
#ifndef LANG_RUNTIME_H
#define LANG_RUNTIME_H

/* Freestanding C11 headers */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdalign.h>

/* Result type support */
#define RESULT_OK(T) struct { bool is_ok; T value; }
#define RESULT_ERR(E) struct { bool is_ok; E error; }

/* Option type support */
#define OPTION(T) struct { bool has_value; T value; }

/* Coroutine support */
typedef struct {
    void* state;
    void* data;
} __u_Coroutine;

#endif /* LANG_RUNTIME_H */
