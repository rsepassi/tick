/* Generated code - do not edit */
#include "08_tcp_echo_server.h"

void __u_server_init(void* __u_server, int32_t __u_listen_fd) {
    /* Temporaries */
    int32_t t0;
    int32_t t1;
    int32_t t2;

    t0 = __u_server.__u_listen_fd;
    t0 = __u_listen_fd;
    t1 = __u_server.__u_client_count;
    t1 = 0;
    t2 = __u_server.__u_running;
    t2 = false;
}

void* __u_server_find_free_client(void* __u_server) {
    /* Temporaries */
    int32_t t0;
    int32_t t1;
    void* t2;
    void* t3;

    t0 = __u_server.__u_client_count;
    t1 = t0 >= 128;
    if (t1) goto __u_if.then; else goto __u_if.merge;
__u_if.then:
    t2 = (void*)0;
    return t2;
    goto __u_if.merge;
__u_if.merge:
    goto __u_loop.header;
__u_loop.header:
    if () goto __u_loop.body; else goto __u_loop.exit;
__u_loop.body:
    goto __u_loop.header;
__u_loop.exit:
    t3 = (void*)0;
    return t3;
}

void __u_on_accept_complete(void* __u_op) {
    /* Temporaries */
    int32_t t0;
    int32_t* __u_accept_op;
    int32_t t2;
    int32_t t3;
    int32_t* __u_server;
    int32_t t5;
    int32_t t6;
    void* t8;
    void /* unsupported type */ __u_server_find_free_client;
    void** __u_client;
    int32_t t10;
    int32_t t11;
    void* __u_close_op;
    int32_t t14;
    int32_t t15;
    int32_t t16;
    int32_t __u_ASYNC_OP_CLOSE;
    int32_t t17;
    int32_t t18;
    int32_t t19;
    int32_t t20;
    int32_t t21;
    int32_t t22;
    int32_t t23;
    int32_t t24;
    int32_t t26;
    int32_t __u_ClientState;
    int32_t t27;
    int32_t t28;
    int32_t t29;
    int32_t t30;
    int32_t t31;
    int32_t t32;
    int32_t t33;
    int32_t t34;
    int32_t t35;
    int32_t t36;
    int32_t t37;
    void* __u_read_op;
    int32_t t40;
    int32_t t41;
    int32_t t42;
    int32_t __u_ASYNC_OP_READ;
    int32_t t43;
    int32_t t44;
    int32_t t45;
    int32_t t46;
    int32_t t47;
    int32_t t49;
    int32_t t50;
    int32_t t51;
    int32_t __u_on_client_read;
    int32_t t52;
    int32_t t53;
    int32_t t54;
    int32_t t55;
    int32_t t56;
    int32_t t57;
    int32_t t58;
    int32_t t59;
    int32_t t60;
    int32_t t61;
    int32_t t62;
    int32_t t63;
    int64_t t64;
    int32_t t65;
    int32_t t66;

    t0 = (void)__u_op;
    __u_accept_op = alloca(sizeof(int32_t));
    *__u_accept_op = t0;
    t2 = __u_op.__u_ctx;
    t3 = (void)t2;
    __u_server = alloca(sizeof(int32_t));
    *__u_server = t3;
    t5 = __u_op.__u_result;
    t6 = t5 < 0;
    if (t6) goto __u_if.then; else goto __u_if.merge;
__u_if.then:
    return;
    goto __u_if.merge;
__u_if.merge:
    t8 = __u_server_find_free_client();
    __u_client = alloca(sizeof(void*));
    *__u_client = t8;
    t10 = *__u_client;
    t11 = t10 == 0;
    if (t11) goto __u_if.then; else goto __u_if.merge;
__u_if.then:
    __u_close_op = alloca(sizeof(void));
    t14 = *__u_close_op;
    t15 = t14.__u_base;
    t16 = t15.__u_opcode;
    t16 = __u_ASYNC_OP_CLOSE;
    t17 = *__u_accept_op;
    t18 = t17.__u_client_fd;
    t19 = *__u_close_op;
    t20 = t19.__u_fd;
    t20 = t18;
    return;
    goto __u_if.merge;
__u_if.merge:
    t21 = *__u_accept_op;
    t22 = t21.__u_client_fd;
    t23 = *__u_client;
    t24 = t23.__u_fd;
    t24 = t22;
    t26 = __u_ClientState.__u_READING;
    t27 = *__u_client;
    t28 = t27.__u_state;
    t28 = t26;
    t29 = *__u_client;
    t30 = t29.__u_read_pos;
    t30 = 0;
    t31 = *__u_client;
    t32 = t31.__u_write_pos;
    t32 = 0;
    t33 = *__u_server;
    t34 = t33.__u_client_count;
    t35 = t34 + 1;
    t36 = *__u_server;
    t37 = t36.__u_client_count;
    t37 = t35;
    __u_read_op = alloca(sizeof(void));
    t40 = *__u_read_op;
    t41 = t40.__u_base;
    t42 = t41.__u_opcode;
    t42 = __u_ASYNC_OP_READ;
    t43 = *__u_client;
    t44 = (void)t43;
    t45 = *__u_read_op;
    t46 = t45.__u_base;
    t47 = t46.__u_ctx;
    t47 = t44;
    t49 = *__u_read_op;
    t50 = t49.__u_base;
    t51 = t50.__u_callback;
    t51 = __u_on_client_read;
    t52 = *__u_client;
    t53 = t52.__u_fd;
    t54 = *__u_read_op;
    t55 = t54.__u_fd;
    t55 = t53;
    t56 = *__u_client;
    t57 = t56.__u_buffer;
    t58 = t57[0];
    t59 = &t58;
    t60 = *__u_read_op;
    t61 = t60.__u_buffer;
    t61 = t59;
    t62 = *__u_read_op;
    t63 = t62.__u_count;
    t63 = 4096;
    t64 = -1;
    t65 = *__u_accept_op;
    t66 = t65.__u_client_fd;
    t66 = t64;
}

void __u_on_client_read(void* __u_op) {
    /* Temporaries */
    int32_t t0;
    int32_t* __u_read_op;
    int32_t t2;
    int32_t t3;
    int32_t* __u_client;
    int32_t t5;
    int32_t t6;
    int32_t t8;
    int32_t __u_ClientState;
    int32_t t9;
    int32_t t10;
    void* __u_close_op;
    int32_t t13;
    int32_t t14;
    int32_t t15;
    int32_t __u_ASYNC_OP_CLOSE;
    int32_t t16;
    int32_t t17;
    int32_t t18;
    int32_t t19;
    int32_t t20;
    int32_t t22;
    int32_t t23;
    int32_t t24;
    int32_t __u_on_client_close;
    int32_t t25;
    int32_t t26;
    int32_t t27;
    int32_t t28;
    int32_t t29;
    int32_t t30;
    int32_t t31;
    int32_t t32;
    int32_t t33;
    int32_t t34;
    int32_t t36;
    int32_t __u_ClientState;
    int32_t t37;
    int32_t t38;
    void* __u_write_op;
    int32_t t41;
    int32_t t42;
    int32_t t43;
    int32_t __u_ASYNC_OP_WRITE;
    int32_t t44;
    int32_t t45;
    int32_t t46;
    int32_t t47;
    int32_t t48;
    int32_t t50;
    int32_t t51;
    int32_t t52;
    int32_t __u_on_client_write;
    int32_t t53;
    int32_t t54;
    int32_t t55;
    int32_t t56;
    int32_t t57;
    int32_t t58;
    int32_t t59;
    int32_t t60;
    int32_t t61;
    int32_t t62;
    int32_t t63;
    int32_t t64;
    int32_t t65;
    int32_t t66;

    t0 = (void)__u_op;
    __u_read_op = alloca(sizeof(int32_t));
    *__u_read_op = t0;
    t2 = __u_op.__u_ctx;
    t3 = (void)t2;
    __u_client = alloca(sizeof(int32_t));
    *__u_client = t3;
    t5 = __u_op.__u_result;
    t6 = t5 <= 0;
    if (t6) goto __u_if.then; else goto __u_if.merge;
__u_if.then:
    t8 = __u_ClientState.__u_CLOSING;
    t9 = *__u_client;
    t10 = t9.__u_state;
    t10 = t8;
    __u_close_op = alloca(sizeof(void));
    t13 = *__u_close_op;
    t14 = t13.__u_base;
    t15 = t14.__u_opcode;
    t15 = __u_ASYNC_OP_CLOSE;
    t16 = *__u_client;
    t17 = (void)t16;
    t18 = *__u_close_op;
    t19 = t18.__u_base;
    t20 = t19.__u_ctx;
    t20 = t17;
    t22 = *__u_close_op;
    t23 = t22.__u_base;
    t24 = t23.__u_callback;
    t24 = __u_on_client_close;
    t25 = *__u_client;
    t26 = t25.__u_fd;
    t27 = *__u_close_op;
    t28 = t27.__u_fd;
    t28 = t26;
    return;
    goto __u_if.merge;
__u_if.merge:
    t29 = __u_op.__u_result;
    t30 = (void)t29;
    t31 = *__u_client;
    t32 = t31.__u_read_pos;
    t32 = t30;
    t33 = *__u_client;
    t34 = t33.__u_write_pos;
    t34 = 0;
    t36 = __u_ClientState.__u_WRITING;
    t37 = *__u_client;
    t38 = t37.__u_state;
    t38 = t36;
    __u_write_op = alloca(sizeof(void));
    t41 = *__u_write_op;
    t42 = t41.__u_base;
    t43 = t42.__u_opcode;
    t43 = __u_ASYNC_OP_WRITE;
    t44 = *__u_client;
    t45 = (void)t44;
    t46 = *__u_write_op;
    t47 = t46.__u_base;
    t48 = t47.__u_ctx;
    t48 = t45;
    t50 = *__u_write_op;
    t51 = t50.__u_base;
    t52 = t51.__u_callback;
    t52 = __u_on_client_write;
    t53 = *__u_client;
    t54 = t53.__u_fd;
    t55 = *__u_write_op;
    t56 = t55.__u_fd;
    t56 = t54;
    t57 = *__u_client;
    t58 = t57.__u_buffer;
    t59 = t58[0];
    t60 = &t59;
    t61 = *__u_write_op;
    t62 = t61.__u_buffer;
    t62 = t60;
    t63 = *__u_client;
    t64 = t63.__u_read_pos;
    t65 = *__u_write_op;
    t66 = t65.__u_count;
    t66 = t64;
}

void __u_on_client_write(void* __u_op) {
    /* Temporaries */
    int32_t t0;
    int32_t* __u_write_op;
    int32_t t2;
    int32_t t3;
    int32_t* __u_client;
    int32_t t5;
    int32_t t6;
    int32_t t8;
    int32_t __u_ClientState;
    int32_t t9;
    int32_t t10;
    void* __u_close_op;
    int32_t t13;
    int32_t t14;
    int32_t t15;
    int32_t __u_ASYNC_OP_CLOSE;
    int32_t t16;
    int32_t t17;
    int32_t t18;
    int32_t t19;
    int32_t t20;
    int32_t t22;
    int32_t t23;
    int32_t t24;
    int32_t __u_on_client_close;
    int32_t t25;
    int32_t t26;
    int32_t t27;
    int32_t t28;
    int32_t t29;
    int32_t t30;
    int32_t t31;
    int32_t t32;
    int32_t t33;
    int32_t t34;
    int32_t t35;
    int32_t t36;
    int32_t t37;
    int32_t t38;
    int32_t t39;
    int32_t t40;
    int32_t t41;
    int32_t t42;
    int32_t t43;
    int32_t t44;
    int32_t t45;
    int32_t* __u_remaining;
    int32_t t47;
    int32_t t48;
    int32_t t49;
    int32_t t50;
    int32_t t51;
    int32_t t52;
    int32_t t53;
    int32_t t54;
    int32_t t55;
    int32_t t56;
    int32_t t57;
    int32_t t59;
    int32_t __u_ClientState;
    int32_t t60;
    int32_t t61;
    int32_t t62;
    int32_t t63;
    int32_t t64;
    int32_t t65;
    void* __u_read_op;
    int32_t t68;
    int32_t t69;
    int32_t t70;
    int32_t __u_ASYNC_OP_READ;
    int32_t t71;
    int32_t t72;
    int32_t t73;
    int32_t t74;
    int32_t t75;
    int32_t t77;
    int32_t t78;
    int32_t t79;
    void /* unsupported type */ __u_on_client_read;
    int32_t t80;
    int32_t t81;
    int32_t t82;
    int32_t t83;
    int32_t t84;
    int32_t t85;
    int32_t t86;
    int32_t t87;
    int32_t t88;
    int32_t t89;
    int32_t t90;
    int32_t t91;

    t0 = (void)__u_op;
    __u_write_op = alloca(sizeof(int32_t));
    *__u_write_op = t0;
    t2 = __u_op.__u_ctx;
    t3 = (void)t2;
    __u_client = alloca(sizeof(int32_t));
    *__u_client = t3;
    t5 = __u_op.__u_result;
    t6 = t5 < 0;
    if (t6) goto __u_if.then; else goto __u_if.merge;
__u_if.then:
    t8 = __u_ClientState.__u_CLOSING;
    t9 = *__u_client;
    t10 = t9.__u_state;
    t10 = t8;
    __u_close_op = alloca(sizeof(void));
    t13 = *__u_close_op;
    t14 = t13.__u_base;
    t15 = t14.__u_opcode;
    t15 = __u_ASYNC_OP_CLOSE;
    t16 = *__u_client;
    t17 = (void)t16;
    t18 = *__u_close_op;
    t19 = t18.__u_base;
    t20 = t19.__u_ctx;
    t20 = t17;
    t22 = *__u_close_op;
    t23 = t22.__u_base;
    t24 = t23.__u_callback;
    t24 = __u_on_client_close;
    t25 = *__u_client;
    t26 = t25.__u_fd;
    t27 = *__u_close_op;
    t28 = t27.__u_fd;
    t28 = t26;
    return;
    goto __u_if.merge;
__u_if.merge:
    t29 = *__u_client;
    t30 = t29.__u_write_pos;
    t31 = __u_op.__u_result;
    t32 = (void)t31;
    t33 = t30 + t32;
    t34 = *__u_client;
    t35 = t34.__u_write_pos;
    t35 = t33;
    t36 = *__u_client;
    t37 = t36.__u_write_pos;
    t38 = *__u_client;
    t39 = t38.__u_read_pos;
    t40 = t37 < t39;
    if (t40) goto __u_if.then; else goto __u_if.else;
__u_if.then:
    t41 = *__u_client;
    t42 = t41.__u_read_pos;
    t43 = *__u_client;
    t44 = t43.__u_write_pos;
    t45 = t42 - t44;
    __u_remaining = alloca(sizeof(int32_t));
    *__u_remaining = t45;
    t47 = *__u_client;
    t48 = t47.__u_buffer;
    t49 = *__u_client;
    t50 = t49.__u_write_pos;
    t51 = t48[t50];
    t52 = &t51;
    t53 = *__u_write_op;
    t54 = t53.__u_buffer;
    t54 = t52;
    t55 = *__u_remaining;
    t56 = *__u_write_op;
    t57 = t56.__u_count;
    t57 = t55;
    goto __u_if.merge;
    t59 = __u_ClientState.__u_READING;
    t60 = *__u_client;
    t61 = t60.__u_state;
    t61 = t59;
    t62 = *__u_client;
    t63 = t62.__u_read_pos;
    t63 = 0;
    t64 = *__u_client;
    t65 = t64.__u_write_pos;
    t65 = 0;
    __u_read_op = alloca(sizeof(void));
    t68 = *__u_read_op;
    t69 = t68.__u_base;
    t70 = t69.__u_opcode;
    t70 = __u_ASYNC_OP_READ;
    t71 = *__u_client;
    t72 = (void)t71;
    t73 = *__u_read_op;
    t74 = t73.__u_base;
    t75 = t74.__u_ctx;
    t75 = t72;
    t77 = *__u_read_op;
    t78 = t77.__u_base;
    t79 = t78.__u_callback;
    t79 = __u_on_client_read;
    t80 = *__u_client;
    t81 = t80.__u_fd;
    t82 = *__u_read_op;
    t83 = t82.__u_fd;
    t83 = t81;
    t84 = *__u_client;
    t85 = t84.__u_buffer;
    t86 = t85[0];
    t87 = &t86;
    t88 = *__u_read_op;
    t89 = t88.__u_buffer;
    t89 = t87;
    t90 = *__u_read_op;
    t91 = t90.__u_count;
    t91 = 4096;
    goto __u_if.merge;
__u_if.merge:
}

void __u_on_client_close(void* __u_op) {
    /* Temporaries */
    int32_t t0;
    int32_t t1;
    int32_t* __u_client;
    int64_t t3;
    int32_t t4;
    int32_t t5;

    t0 = __u_op.__u_ctx;
    t1 = (void)t0;
    __u_client = alloca(sizeof(int32_t));
    *__u_client = t1;
    t3 = -1;
    t4 = *__u_client;
    t5 = t4.__u_fd;
    t5 = t3;
}

void /* unsupported type */ __u_handle_client_coro(int32_t __u_client_fd) {
    /* Temporaries */
    void* __u_buffer;

    __u_buffer = alloca(sizeof(void));
    goto __u_loop.header;
__u_loop.header:
    if () goto __u_loop.body; else goto __u_loop.exit;
__u_loop.body:
    goto __u_loop.header;
__u_loop.exit:
    return;
}

void __u_server_accept_loop(int32_t __u_listen_fd) {
    goto __u_loop.header;
__u_loop.header:
    if () goto __u_loop.body; else goto __u_loop.exit;
__u_loop.body:
    goto __u_loop.header;
__u_loop.exit:
}

int32_t __u_main(void) {
    /* Temporaries */
    int64_t* __u_listen_fd;
    void* __u_server;
    void /* unsupported type */ __u_server_init;
    void /* unsupported type */ __u_server_accept_loop;

    __u_listen_fd = alloca(sizeof(int64_t));
    *__u_listen_fd = 3;
    __u_server = alloca(sizeof(void));
    __u_server_init();
    __u_server_accept_loop();
    return 0;
}

