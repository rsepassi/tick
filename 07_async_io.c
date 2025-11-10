/* Generated code - do not edit */
#include "07_async_io.h"

void __u_on_read_complete(void* __u_op) {
    /* Temporaries */
    int32_t t0;
    int32_t* __u_read_op;
    int32_t t2;
    int32_t t3;

    t0 = (void)__u_op;
    __u_read_op = alloca(sizeof(int32_t));
    *__u_read_op = t0;
    t2 = __u_op.__u_result;
    t3 = t2 > 0;
    if (t3) goto __u_if.then; else goto __u_if.else;
__u_if.then:
    goto __u_if.merge;
    goto __u_if.merge;
__u_if.merge:
}

void __u_async_read_file(int32_t __u_fd) {
    /* Temporaries */
    void* __u_buffer;
    void* __u_read_op;
    int32_t t3;
    int32_t t4;
    int32_t t5;
    int32_t __u_ASYNC_OP_READ;
    int32_t t6;
    int32_t t7;
    int32_t t8;
    int32_t t10;
    int32_t t11;
    int32_t t12;
    void /* unsupported type */ __u_on_read_complete;
    int32_t t13;
    int32_t t14;
    int32_t t15;
    int32_t t16;
    int32_t t17;
    int32_t t18;
    int32_t t19;
    int32_t t20;
    int32_t t21;

    __u_buffer = alloca(sizeof(void));
    __u_read_op = alloca(sizeof(void));
    t3 = *__u_read_op;
    t4 = t3.__u_base;
    t5 = t4.__u_opcode;
    t5 = __u_ASYNC_OP_READ;
    t6 = *__u_read_op;
    t7 = t6.__u_base;
    t8 = t7.__u_result;
    t8 = 0;
    t10 = *__u_read_op;
    t11 = t10.__u_base;
    t12 = t11.__u_callback;
    t12 = __u_on_read_complete;
    t13 = *__u_read_op;
    t14 = t13.__u_fd;
    t14 = __u_fd;
    t15 = *__u_buffer;
    t16 = t15[0];
    t17 = &t16;
    t18 = *__u_read_op;
    t19 = t18.__u_buffer;
    t19 = t17;
    t20 = *__u_read_op;
    t21 = t20.__u_count;
    t21 = 4096;
    return;
}

void __u_on_write_complete(void* __u_op) {
    /* Temporaries */
    int32_t t0;
    int32_t* __u_write_op;
    int32_t t2;
    int32_t t3;

    t0 = (void)__u_op;
    __u_write_op = alloca(sizeof(int32_t));
    *__u_write_op = t0;
    t2 = __u_op.__u_result;
    t3 = t2 > 0;
    if (t3) goto __u_if.then; else goto __u_if.else;
__u_if.then:
    goto __u_if.merge;
    goto __u_if.merge;
__u_if.merge:
}

void __u_async_write_file(int32_t __u_fd, void /* unsupported type */ __u_data) {
    /* Temporaries */
    void* __u_write_op;
    int32_t t2;
    int32_t t3;
    int32_t t4;
    int32_t __u_ASYNC_OP_WRITE;
    int32_t t5;
    int32_t t6;
    int32_t t7;
    int32_t t9;
    int32_t t10;
    int32_t t11;
    void /* unsupported type */ __u_on_write_complete;
    int32_t t12;
    int32_t t13;
    int32_t t14;
    int32_t t15;
    int32_t t16;
    int32_t t17;
    int32_t t18;
    int32_t t19;

    __u_write_op = alloca(sizeof(void));
    t2 = *__u_write_op;
    t3 = t2.__u_base;
    t4 = t3.__u_opcode;
    t4 = __u_ASYNC_OP_WRITE;
    t5 = *__u_write_op;
    t6 = t5.__u_base;
    t7 = t6.__u_result;
    t7 = 0;
    t9 = *__u_write_op;
    t10 = t9.__u_base;
    t11 = t10.__u_callback;
    t11 = __u_on_write_complete;
    t12 = *__u_write_op;
    t13 = t12.__u_fd;
    t13 = __u_fd;
    t14 = __u_data.__u_ptr;
    t15 = *__u_write_op;
    t16 = t15.__u_buffer;
    t16 = t14;
    t17 = __u_data.__u_len;
    t18 = *__u_write_op;
    t19 = t18.__u_count;
    t19 = t17;
    return;
}

void __u_on_timer_expired(void* __u_op) {
    /* Temporaries */
    int32_t t0;
    int32_t* __u_timer_op;

    t0 = (void)__u_op;
    __u_timer_op = alloca(sizeof(int32_t));
    *__u_timer_op = t0;
}

void __u_schedule_timer(uint64_t __u_timeout_ms) {
    /* Temporaries */
    void* __u_timer_op;
    int32_t t2;
    int32_t t3;
    int32_t t4;
    int32_t __u_ASYNC_OP_TIMER;
    int32_t t6;
    int32_t t7;
    int32_t t8;
    void /* unsupported type */ __u_on_timer_expired;
    int32_t t9;
    int32_t t10;
    int32_t t11;

    __u_timer_op = alloca(sizeof(void));
    t2 = *__u_timer_op;
    t3 = t2.__u_base;
    t4 = t3.__u_opcode;
    t4 = __u_ASYNC_OP_TIMER;
    t6 = *__u_timer_op;
    t7 = t6.__u_base;
    t8 = t7.__u_callback;
    t8 = __u_on_timer_expired;
    t9 = __u_timeout_ms * 1000000;
    t10 = *__u_timer_op;
    t11 = t10.__u_timeout_ns;
    t11 = t9;
    return;
}

void /* unsupported type */ __u_read_file_coro(int32_t __u_fd) {
    /* Temporaries */
    void* __u_buffer;
    void* __u_read_op;
    int32_t t3;
    int32_t t4;
    int32_t t5;
    int32_t __u_ASYNC_OP_READ;
    int32_t t6;
    int32_t t7;
    int32_t t8;
    int32_t t9;
    int32_t t10;
    int32_t t11;
    int32_t t12;
    int32_t t13;
    int32_t t14;
    int32_t t15;
    int32_t t16;
    int32_t t17;
    int32_t t18;
    int32_t t20;
    int32_t __u_IOError;
    int32_t t21;
    int32_t t22;
    int32_t t23;
    int32_t t24;

    __u_buffer = alloca(sizeof(void));
    __u_read_op = alloca(sizeof(void));
    t3 = *__u_read_op;
    t4 = t3.__u_base;
    t5 = t4.__u_opcode;
    t5 = __u_ASYNC_OP_READ;
    t6 = *__u_read_op;
    t7 = t6.__u_fd;
    t7 = __u_fd;
    t8 = *__u_buffer;
    t9 = t8[0];
    t10 = &t9;
    t11 = *__u_read_op;
    t12 = t11.__u_buffer;
    t12 = t10;
    t13 = *__u_read_op;
    t14 = t13.__u_count;
    t14 = 4096;
    /* SUSPEND at state_0 - save 0 live variables and yield control */
    machine.resume_point = &&__u_read_file_coro_state_0;
    machine.state = 0;
    return (void /* unsupported type */){0} /* suspended */;
__u_suspend_resume:
    t15 = *__u_read_op;
    t16 = t15.__u_base;
    t17 = t16.__u_result;
    t18 = t17 < 0;
    if (t18) goto __u_if.then; else goto __u_if.merge;
__u_if.then:
    t20 = __u_IOError.__u_BAD_FD;
    return t20;
    goto __u_if.merge;
__u_if.merge:
    t21 = *__u_read_op;
    t22 = t21.__u_base;
    t23 = t22.__u_result;
    t24 = (void)t23;
    return t24;
}

void /* unsupported type */ __u_copy_file(int32_t __u_src_fd, int32_t __u_dst_fd) {
    /* Temporaries */
    void /* unsupported type */* __u_total_copied;
    void* __u_buffer;
    int32_t t2;

    __u_total_copied = alloca(sizeof(void /* unsupported type */));
    *__u_total_copied = 0;
    __u_buffer = alloca(sizeof(void));
    goto __u_loop.header;
__u_loop.header:
    if () goto __u_loop.body; else goto __u_loop.exit;
__u_loop.body:
    goto __u_loop.header;
__u_loop.exit:
    t2 = *__u_total_copied;
    return t2;
}

void __u_parallel_reads(int32_t __u_fd1, int32_t __u_fd2) {
    /* Temporaries */
    void /* unsupported type */ t1;
    void /* unsupported type */ __u_read_file_coro;
    void /* unsupported type */* __u_coro1;
    void /* unsupported type */ t4;
    void /* unsupported type */ __u_read_file_coro;
    void /* unsupported type */* __u_coro2;
    int32_t t6;
    int32_t t7;

    t1 = __u_read_file_coro();
    __u_coro1 = alloca(sizeof(void /* unsupported type */));
    *__u_coro1 = t1;
    t4 = __u_read_file_coro();
    __u_coro2 = alloca(sizeof(void /* unsupported type */));
    *__u_coro2 = t4;
    t6 = *__u_coro1;
    /* RESUME state_0 */
    goto *t6->state;
    t7 = *__u_coro2;
    /* RESUME state_0 */
    goto *t7->state;
    return;
}

int32_t __u_main(void) {
    /* Temporaries */
    int64_t* __u_fd;
    void /* unsupported type */ __u_async_read_file;
    int32_t __u_read_file_coro;
    int32_t* __u_bytes;
    void /* unsupported type */ __u_schedule_timer;

    __u_fd = alloca(sizeof(int64_t));
    *__u_fd = 0;
    __u_async_read_file();
    goto __u_try;
__u_try:
    __u_read_file_coro();
    __u_bytes = alloca(sizeof(int32_t));
    goto __u_try_cont;
    return 0;
    goto __u_try_cont;
__u_try_cont:
    __u_schedule_timer();
    return 0;
}

