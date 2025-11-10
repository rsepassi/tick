/* Generated code - do not edit */
#ifndef __u_08_tcp_echo_server_H
#define __u_08_tcp_echo_server_H

#include "lang_runtime.h"

void __u_server_init(void* __u_server, int32_t __u_listen_fd);

void* __u_server_find_free_client(void* __u_server);

void __u_on_accept_complete(void* __u_op);

void __u_on_client_read(void* __u_op);

void __u_on_client_write(void* __u_op);

void __u_on_client_close(void* __u_op);

void /* unsupported type */ __u_handle_client_coro(int32_t __u_client_fd);

void __u_server_accept_loop(int32_t __u_listen_fd);

int32_t __u_main(void);

#endif /* __u_08_tcp_echo_server_H */
