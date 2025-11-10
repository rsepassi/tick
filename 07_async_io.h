/* Generated code - do not edit */
#ifndef __u_07_async_io_H
#define __u_07_async_io_H

#include "lang_runtime.h"

void __u_on_read_complete(void* __u_op);

void __u_async_read_file(int32_t __u_fd);

void __u_on_write_complete(void* __u_op);

void __u_async_write_file(int32_t __u_fd, void /* unsupported type */ __u_data);

void __u_on_timer_expired(void* __u_op);

void __u_schedule_timer(uint64_t __u_timeout_ms);

void /* unsupported type */ __u_read_file_coro(int32_t __u_fd);

void /* unsupported type */ __u_copy_file(int32_t __u_src_fd, int32_t __u_dst_fd);

void __u_parallel_reads(int32_t __u_fd1, int32_t __u_fd2);

int32_t __u_main(void);

#endif /* __u_07_async_io_H */
