/* Generated code - do not edit */
#include "02_types.h"

void __u_create_point(int32_t __u_x, int32_t __u_y) {
    /* Temporaries */
    int32_t t0;
    int32_t* __u_p;
    int32_t t2;

    t0 = alloca(sizeof(void));
    __u_p = alloca(sizeof(int32_t));
    *__u_p = t0;
    t2 = *__u_p;
    return t2;
}

int32_t __u_distance_squared(void __u_p) {
    /* Temporaries */
    int32_t t0;
    int32_t t1;
    int32_t t2;
    int32_t t3;
    int32_t t4;
    int32_t t5;
    int32_t t6;

    t0 = __u_p.__u_x;
    t1 = __u_p.__u_x;
    t2 = t0 * t1;
    t3 = __u_p.__u_y;
    t4 = __u_p.__u_y;
    t5 = t3 * t4;
    t6 = t2 + t5;
    return t6;
}

int32_t __u_main(void) {
    /* Temporaries */
    void /* unsupported type */ __u_create_point;
    int32_t* __u_p;
    int32_t t3;
    void /* unsupported type */ __u_distance_squared;
    int32_t* __u_dist_sq;
    int32_t t6;
    int32_t __u_Status;
    int32_t* __u_status;
    int32_t t8;
    int32_t* __u_val;
    void* __u_buffer;
    int32_t t11;
    int32_t t12;
    int32_t t13;
    int32_t t14;

    __u_create_point();
    __u_p = alloca(sizeof(int32_t));
    t3 = __u_distance_squared();
    __u_dist_sq = alloca(sizeof(int32_t));
    *__u_dist_sq = t3;
    t6 = __u_Status.__u_OK;
    __u_status = alloca(sizeof(int32_t));
    *__u_status = t6;
    t8 = alloca(sizeof(void));
    __u_val = alloca(sizeof(int32_t));
    *__u_val = t8;
    __u_buffer = alloca(sizeof(void));
    t11 = *__u_buffer;
    t12 = t11[0];
    t12 = 1;
    t13 = *__u_buffer;
    t14 = t13[1];
    t14 = 2;
    return 0;
}

