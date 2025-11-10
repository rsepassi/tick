/* Generated code - do not edit */
#include "01_hello.h"

int32_t __u_add(int32_t __u_x, int32_t __u_y) {
    /* Temporaries */
    int32_t t0;

    t0 = __u_x + __u_y;
    return t0;
}

void __u_print_sum(int32_t __u_a, int32_t __u_b) {
    /* Temporaries */
    int32_t t1;
    void /* unsupported type */ __u_add;
    int32_t* __u_result;

    t1 = __u_add();
    __u_result = alloca(sizeof(int32_t));
    *__u_result = t1;
    return;
}

int32_t __u_main(void) {
    /* Temporaries */
    int64_t* __u_x;
    int64_t* __u_y;
    int32_t t3;
    void /* unsupported type */ __u_add;
    int32_t* __u_sum;
    void /* unsupported type */ __u_print_sum;

    __u_x = alloca(sizeof(int64_t));
    *__u_x = 10;
    __u_y = alloca(sizeof(int64_t));
    *__u_y = 20;
    t3 = __u_add();
    __u_sum = alloca(sizeof(int32_t));
    *__u_sum = t3;
    __u_print_sum();
    return 0;
}

