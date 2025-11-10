/* Generated code - do not edit */
#include "05_resources.h"

void /* unsupported type */ __u_file_open(void* __u_path) {
    /* Temporaries */
    int32_t t0;
    int32_t* __u_file;
    int32_t t2;

    t0 = alloca(sizeof(void));
    __u_file = alloca(sizeof(int32_t));
    *__u_file = t0;
    t2 = *__u_file;
    return t2;
}

void __u_file_close(void* __u_file) {
    /* Temporaries */
    int32_t t0;

    t0 = __u_file.__u_is_open;
    t0 = false;
}

void /* unsupported type */ __u_file_write(void* __u_file, void /* unsupported type */ __u_data) {
    /* Temporaries */
    int32_t t0;
    int32_t t1;
    int32_t t3;
    int32_t __u_Error;
    int32_t t4;

    t0 = __u_file.__u_is_open;
    t1 = !t0;
    if (t1) goto __u_if.then; else goto __u_if.merge;
__u_if.then:
    t3 = __u_Error.__u_InvalidFile;
    return t3;
    goto __u_if.merge;
__u_if.merge:
    t4 = __u_data.__u_len;
    return t4;
}

void /* unsupported type */ __u_process_file(void* __u_path) {
    /* Temporaries */
    void /* unsupported type */ t1;
    void /* unsupported type */ __u_file_open;
    void /* unsupported type */* __u_file;
    void* __u_buffer;
    void /* unsupported type */ t6;
    void /* unsupported type */ __u_file_write;
    void /* unsupported type */* __u_bytes_written;

    t1 = __u_file_open();
    if (!t1.is_ok) {
        t2 = t1.error;
        goto __u_try.error;
    }
    t1 = t1.value;
__u_try.error:
    return (void /* unsupported type */){ .is_ok = false, .error = t2 };
__u_try.success:
    __u_file = alloca(sizeof(void /* unsupported type */));
    *__u_file = t1;
    __u_buffer = alloca(sizeof(void));
    t6 = __u_file_write();
    if (!t6.is_ok) {
        t7 = t6.error;
        goto __u_try.error;
    }
    t6 = t6.value;
__u_try.error:
    return (void /* unsupported type */){ .is_ok = false, .error = t7 };
__u_try.success:
    __u_bytes_written = alloca(sizeof(void /* unsupported type */));
    *__u_bytes_written = t6;
    return;
}

void /* unsupported type */ __u_allocate_and_process(void* __u_alloc, size_t __u_size) {
    /* Temporaries */
    int32_t t0;
    int32_t* __u_buffer;
    int32_t t2;
    int32_t t3;
    int32_t t5;
    int32_t __u_Error;
    int32_t t6;

    t0 = __u_alloc.__u_alloc_fn;
    t0();
    __u_buffer = alloca(sizeof(int32_t));
    t2 = *__u_buffer;
    t3 = t2 == 0;
    if (t3) goto __u_if.then; else goto __u_if.merge;
__u_if.then:
    t5 = __u_Error.__u_OutOfMemory;
    return t5;
    goto __u_if.merge;
__u_if.merge:
    t6 = *__u_buffer;
    return t6;
}

void /* unsupported type */ __u_complex_cleanup(void) {
    /* Temporaries */
    void /* unsupported type */ t1;
    void /* unsupported type */ __u_file_open;
    void /* unsupported type */* __u_file1;
    void /* unsupported type */ t5;
    void /* unsupported type */ __u_file_open;
    void /* unsupported type */* __u_file2;
    void* __u_buffer;
    void /* unsupported type */ t10;
    void /* unsupported type */ __u_file_write;
    void /* unsupported type */* __u_n;

    t1 = __u_file_open();
    if (!t1.is_ok) {
        t2 = t1.error;
        goto __u_try.error;
    }
    t1 = t1.value;
__u_try.error:
    return (void /* unsupported type */){ .is_ok = false, .error = t2 };
__u_try.success:
    __u_file1 = alloca(sizeof(void /* unsupported type */));
    *__u_file1 = t1;
    t5 = __u_file_open();
    if (!t5.is_ok) {
        t6 = t5.error;
        goto __u_try.error;
    }
    t5 = t5.value;
__u_try.error:
    return (void /* unsupported type */){ .is_ok = false, .error = t6 };
__u_try.success:
    __u_file2 = alloca(sizeof(void /* unsupported type */));
    *__u_file2 = t5;
    __u_buffer = alloca(sizeof(void));
    t10 = __u_file_write();
    if (!t10.is_ok) {
        t11 = t10.error;
        goto __u_try.error;
    }
    t10 = t10.value;
__u_try.error:
    return (void /* unsupported type */){ .is_ok = false, .error = t11 };
__u_try.success:
    __u_n = alloca(sizeof(void /* unsupported type */));
    *__u_n = t10;
    return;
}

int32_t __u_nested_defer(void) {
    /* Temporaries */
    int64_t* __u_counter;
    int32_t t1;
    int32_t t2;
    int32_t t3;
    int32_t t4;
    int32_t t5;
    int32_t t6;
    int32_t t7;

    __u_counter = alloca(sizeof(int64_t));
    *__u_counter = 0;
    t1 = *__u_counter;
    t2 = t1 + 10;
    t3 = *__u_counter;
    t3 = t2;
    t4 = *__u_counter;
    t5 = t4 + 5;
    t6 = *__u_counter;
    t6 = t5;
    t7 = *__u_counter;
    return t7;
}

int32_t __u_main(void) {
    /* Temporaries */
    int32_t __u_process_file;
    int32_t t2;
    void /* unsupported type */ __u_nested_defer;
    int32_t* __u_result;

    goto __u_try;
__u_try:
    __u_process_file();
    goto __u_try_cont;
    return 0;
    goto __u_try_cont;
__u_try_cont:
    t2 = __u_nested_defer();
    __u_result = alloca(sizeof(int32_t));
    *__u_result = t2;
    return 0;
}

