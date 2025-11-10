/* Generated code - do not edit */
#include "06_async_basic.h"

void __u_worker(void) {
    /* Temporaries */
    int64_t* __u_x;
    int64_t* __u_y;
    int32_t t2;
    int32_t t3;
    int32_t t4;
    int32_t* __u_z;

    __u_x = alloca(sizeof(int64_t));
    *__u_x = 1;
    __u_y = alloca(sizeof(int64_t));
    *__u_y = 2;
    /* SUSPEND at state_0 - save 0 live variables and yield control */
    machine.resume_point = &&__u_worker_state_0;
    machine.state = 0;
    return;
__u_suspend_resume:
    t2 = *__u_x;
    t3 = *__u_y;
    t4 = t2 + t3;
    __u_z = alloca(sizeof(int32_t));
    *__u_z = t4;
    return;
}

int32_t __u_compute(void) {
    /* Temporaries */
    int64_t* __u_result;
    int32_t t1;
    int32_t t2;
    int32_t t3;
    int32_t t4;
    int32_t t5;
    int32_t t6;
    int32_t t7;
    int32_t t8;
    int32_t t9;
    int32_t t10;

    __u_result = alloca(sizeof(int64_t));
    *__u_result = 0;
    t1 = *__u_result;
    t2 = t1 + 10;
    t3 = *__u_result;
    t3 = t2;
    /* SUSPEND at state_0 - save 0 live variables and yield control */
    machine.resume_point = &&__u_compute_state_0;
    machine.state = 0;
    return (int32_t){0} /* suspended */;
__u_suspend_resume:
    t4 = *__u_result;
    t5 = t4 + 20;
    t6 = *__u_result;
    t6 = t5;
    /* SUSPEND at state_0 - save 0 live variables and yield control */
    machine.resume_point = &&__u_compute_state_0;
    machine.state = 0;
    return (int32_t){0} /* suspended */;
__u_suspend_resume:
    t7 = *__u_result;
    t8 = t7 + 30;
    t9 = *__u_result;
    t9 = t8;
    t10 = *__u_result;
    return t10;
}

int32_t __u_multi_step(void) {
    /* Temporaries */
    int64_t* __u_step;
    int32_t t1;
    int32_t t2;
    int32_t t3;
    int32_t t4;

    __u_step = alloca(sizeof(int64_t));
    *__u_step = 0;
    t1 = *__u_step;
    t1 = 1;
    /* SUSPEND at state_0 - save 0 live variables and yield control */
    machine.resume_point = &&__u_multi_step_state_0;
    machine.state = 0;
    return (int32_t){0} /* suspended */;
__u_suspend_resume:
    t2 = *__u_step;
    t2 = 2;
    /* SUSPEND at state_0 - save 0 live variables and yield control */
    machine.resume_point = &&__u_multi_step_state_0;
    machine.state = 0;
    return (int32_t){0} /* suspended */;
__u_suspend_resume:
    t3 = *__u_step;
    t3 = 3;
    /* SUSPEND at state_0 - save 0 live variables and yield control */
    machine.resume_point = &&__u_multi_step_state_0;
    machine.state = 0;
    return (int32_t){0} /* suspended */;
__u_suspend_resume:
    t4 = *__u_step;
    return t4;
}

int32_t __u_sync_caller(void) {
    /* Temporaries */
    void /* unsupported type */ __u_worker;
    int32_t t2;
    void /* unsupported type */ __u_compute;
    int32_t* __u_result;
    int32_t t4;

    __u_worker();
    t2 = __u_compute();
    __u_result = alloca(sizeof(int32_t));
    *__u_result = t2;
    t4 = *__u_result;
    return t4;
}

void __u_async_caller(void) {
    /* Temporaries */
    void /* unsupported type */ __u_worker;
    int32_t* __u_coro1;
    int64_t* __u_x;
    int32_t t3;
    int32_t t5;
    void /* unsupported type */ __u_compute;
    int32_t* __u_coro2;
    int32_t t8;
    void /* unsupported type */ __u_multi_step;
    int32_t* __u_coro3;
    int32_t t10;
    int32_t t11;
    int32_t t12;
    int32_t t13;
    int32_t t14;
    int32_t t15;

    __u_worker();
    __u_coro1 = alloca(sizeof(int32_t));
    __u_x = alloca(sizeof(int64_t));
    *__u_x = 10;
    t3 = *__u_coro1;
    /* RESUME state_0 */
    goto *t3->state;
    t5 = __u_compute();
    __u_coro2 = alloca(sizeof(int32_t));
    *__u_coro2 = t5;
    t8 = __u_multi_step();
    __u_coro3 = alloca(sizeof(int32_t));
    *__u_coro3 = t8;
    t10 = *__u_coro2;
    /* RESUME state_0 */
    goto *t10->state;
    t11 = *__u_coro2;
    /* RESUME state_0 */
    goto *t11->state;
    t12 = *__u_coro2;
    /* RESUME state_0 */
    goto *t12->state;
    t13 = *__u_coro3;
    /* RESUME state_0 */
    goto *t13->state;
    t14 = *__u_coro3;
    /* RESUME state_0 */
    goto *t14->state;
    t15 = *__u_coro3;
    /* RESUME state_0 */
    goto *t15->state;
    return;
}

int32_t __u_caller_coro(void) {
    /* Temporaries */
    int32_t t1;
    void /* unsupported type */ __u_sync_caller;
    int32_t* __u_temp;
    int32_t t4;
    void /* unsupported type */ __u_compute;
    int32_t* __u_coro;
    int32_t t6;
    int32_t t7;
    int32_t t8;
    int32_t t9;

    t1 = __u_sync_caller();
    __u_temp = alloca(sizeof(int32_t));
    *__u_temp = t1;
    /* SUSPEND at state_0 - save 0 live variables and yield control */
    machine.resume_point = &&__u_caller_coro_state_0;
    machine.state = 0;
    return (int32_t){0} /* suspended */;
__u_suspend_resume:
    t4 = __u_compute();
    __u_coro = alloca(sizeof(int32_t));
    *__u_coro = t4;
    t6 = *__u_coro;
    /* RESUME state_0 */
    goto *t6->state;
    t7 = *__u_coro;
    /* RESUME state_0 */
    goto *t7->state;
    t8 = *__u_coro;
    /* RESUME state_0 */
    goto *t8->state;
    t9 = *__u_temp;
    return t9;
}

void __u_state_machine_coro(void) {
    /* Temporaries */
    int32_t t1;
    int32_t __u_State;
    int32_t* __u_state;
    int32_t t4;
    int32_t __u_State;
    int32_t t5;
    int32_t t7;
    int32_t __u_State;
    int32_t t8;
    int32_t t10;
    int32_t __u_State;
    int32_t t11;
    int32_t t12;

    t1 = __u_State.__u_INIT;
    __u_state = alloca(sizeof(int32_t));
    *__u_state = t1;
    t4 = __u_State.__u_RUNNING;
    t5 = *__u_state;
    t5 = t4;
    /* SUSPEND at state_0 - save 0 live variables and yield control */
    machine.resume_point = &&__u_state_machine_coro_state_0;
    machine.state = 0;
    return;
__u_suspend_resume:
    t7 = __u_State.__u_WAITING;
    t8 = *__u_state;
    t8 = t7;
    /* SUSPEND at state_0 - save 0 live variables and yield control */
    machine.resume_point = &&__u_state_machine_coro_state_0;
    machine.state = 0;
    return;
__u_suspend_resume:
    t10 = __u_State.__u_DONE;
    t11 = *__u_state;
    t11 = t10;
    /* SUSPEND at state_0 - save 0 live variables and yield control */
    machine.resume_point = &&__u_state_machine_coro_state_0;
    machine.state = 0;
    return;
__u_suspend_resume:
    t12 = *__u_state;
    return t12;
}

int32_t __u_main(void) {
    /* Temporaries */
    int32_t t1;
    void /* unsupported type */ __u_sync_caller;
    int32_t* __u_result1;
    void /* unsupported type */ __u_async_caller;
    int32_t t5;
    void /* unsupported type */ __u_multi_step;
    int32_t* __u_coro;
    int32_t t7;
    int32_t t8;
    int32_t t9;

    t1 = __u_sync_caller();
    __u_result1 = alloca(sizeof(int32_t));
    *__u_result1 = t1;
    __u_async_caller();
    t5 = __u_multi_step();
    __u_coro = alloca(sizeof(int32_t));
    *__u_coro = t5;
    t7 = *__u_coro;
    /* RESUME state_0 */
    goto *t7->state;
    t8 = *__u_coro;
    /* RESUME state_0 */
    goto *t8->state;
    t9 = *__u_coro;
    /* RESUME state_0 */
    goto *t9->state;
    return 0;
}

