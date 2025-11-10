/* Generated code - do not edit */
#include "04_errors.h"

void /* unsupported type */ __u_divide(int32_t __u_a, int32_t __u_b) {
    /* Temporaries */
    int32_t t0;
    int32_t t2;
    int32_t __u_MathError;
    int32_t t3;

    t0 = __u_b == 0;
    if (t0) goto __u_if.then; else goto __u_if.merge;
__u_if.then:
    t2 = __u_MathError.__u_DivideByZero;
    return t2;
    goto __u_if.merge;
__u_if.merge:
    t3 = __u_a / __u_b;
    return t3;
}

void /* unsupported type */ __u_checked_add(int32_t __u_a, int32_t __u_b) {
    /* Temporaries */
    int64_t* __u_max;
    int32_t t1;
    int32_t t2;
    int32_t t3;
    int32_t t5;
    int32_t __u_MathError;
    int32_t t6;

    __u_max = alloca(sizeof(int64_t));
    *__u_max = 2147483647;
    t1 = *__u_max;
    t2 = t1 - __u_b;
    t3 = __u_a > t2;
    if (t3) goto __u_if.then; else goto __u_if.merge;
__u_if.then:
    t5 = __u_MathError.__u_Overflow;
    return t5;
    goto __u_if.merge;
__u_if.merge:
    t6 = __u_a + __u_b;
    return t6;
}

int32_t __u_safe_divide(int32_t __u_a, int32_t __u_b) {
    /* Temporaries */
    int32_t __u_divide;
    int32_t* __u_result;
    int32_t t2;

    goto __u_try;
__u_try:
    __u_divide();
    __u_result = alloca(sizeof(int32_t));
    t2 = *__u_result;
    return t2;
    goto __u_try_cont;
    return 0;
    goto __u_try_cont;
__u_try_cont:
}

void /* unsupported type */ __u_divide_chain(int32_t __u_a, int32_t __u_b, int32_t __u_c) {
    /* Temporaries */
    void /* unsupported type */ t1;
    void /* unsupported type */ __u_divide;
    void /* unsupported type */* __u_temp;
    void /* unsupported type */ t5;
    void /* unsupported type */ __u_divide;
    void /* unsupported type */* __u_result;
    int32_t t8;

    t1 = __u_divide();
    if (!t1.is_ok) {
        t2 = t1.error;
        goto __u_try.error;
    }
    t1 = t1.value;
__u_try.error:
    return (void /* unsupported type */){ .is_ok = false, .error = t2 };
__u_try.success:
    __u_temp = alloca(sizeof(void /* unsupported type */));
    *__u_temp = t1;
    t5 = __u_divide();
    if (!t5.is_ok) {
        t6 = t5.error;
        goto __u_try.error;
    }
    t5 = t5.value;
__u_try.error:
    return (void /* unsupported type */){ .is_ok = false, .error = t6 };
__u_try.success:
    __u_result = alloca(sizeof(void /* unsupported type */));
    *__u_result = t5;
    t8 = *__u_result;
    return t8;
}

void /* unsupported type */ __u_parse_positive(int32_t __u_value) {
    /* Temporaries */
    int32_t t0;
    int32_t t2;
    int32_t __u_Error;

    t0 = __u_value < 0;
    if (t0) goto __u_if.then; else goto __u_if.merge;
__u_if.then:
    t2 = __u_Error.__u_InvalidInput;
    return t2;
    goto __u_if.merge;
__u_if.merge:
    return __u_value;
}

int32_t __u_main(void) {
    /* Temporaries */
    int32_t t1;
    void /* unsupported type */ __u_safe_divide;
    int32_t* __u_result1;
    int32_t t4;
    void /* unsupported type */ __u_safe_divide;
    int32_t* __u_result2;
    int32_t __u_divide_chain;
    int32_t* __u_result;

    t1 = __u_safe_divide();
    __u_result1 = alloca(sizeof(int32_t));
    *__u_result1 = t1;
    t4 = __u_safe_divide();
    __u_result2 = alloca(sizeof(int32_t));
    *__u_result2 = t4;
    goto __u_try;
__u_try:
    __u_divide_chain();
    __u_result = alloca(sizeof(int32_t));
    goto __u_try_cont;
    goto __u_try_cont;
__u_try_cont:
    return 0;
}

