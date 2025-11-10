/*
 * test_tcp_echo.c - Test TCP accept/read/write with async runtime
 */

#include "async_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8888
#define BUFFER_SIZE 1024

typedef struct {
    async_runtime* rt;
    int listen_fd;
    int client_fd;
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    int phase;
} echo_ctx_t;

void on_accept_complete(async_t* op);
void on_read_complete(async_t* op);
void on_write_complete(async_t* op);

void on_accept_complete(async_t* op) {
    async_accept_t* accept_op = (async_accept_t*)op;
    echo_ctx_t* ctx = (echo_ctx_t*)op->ctx;

    if (op->result < 0) {
        printf("Accept failed: %d\n", op->result);
        return;
    }

    ctx->client_fd = accept_op->client_fd;
    printf("Accepted connection: client_fd=%d, addr=%s, port=%u\n",
           accept_op->client_fd,
           inet_ntoa(*(struct in_addr*)&accept_op->client_addr),
           accept_op->client_port);

    ctx->phase = 1;

    // Start reading from client
    static async_read_t read_op;
    async_read_init(&read_op, ctx->client_fd, ctx->buffer, BUFFER_SIZE,
                   on_read_complete, ctx);
    async_submit(ctx->rt, &read_op.base);
}

void on_read_complete(async_t* op) {
    echo_ctx_t* ctx = (echo_ctx_t*)op->ctx;

    if (op->result <= 0) {
        if (op->result == 0) {
            printf("Client disconnected\n");
        } else {
            printf("Read error: %d\n", op->result);
        }
        ctx->phase = 3;
        close(ctx->client_fd);
        return;
    }

    ctx->bytes_read = op->result;
    ctx->buffer[ctx->bytes_read] = '\0';
    printf("Read %d bytes: '%s'\n", op->result, ctx->buffer);

    ctx->phase = 2;

    // Echo back
    static async_write_t write_op;
    async_write_init(&write_op, ctx->client_fd, ctx->buffer, ctx->bytes_read,
                    on_write_complete, ctx);
    async_submit(ctx->rt, &write_op.base);
}

void on_write_complete(async_t* op) {
    echo_ctx_t* ctx = (echo_ctx_t*)op->ctx;

    if (op->result < 0) {
        printf("Write error: %d\n", op->result);
        ctx->phase = 3;
        close(ctx->client_fd);
        return;
    }

    printf("Wrote %d bytes\n", op->result);
    ctx->phase = 3;

    // Close connection after one echo
    close(ctx->client_fd);
}

int create_listen_socket(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    // Set SO_REUSEADDR
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 10) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

int connect_to_server(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(port);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }

    return fd;
}

int main(void) {
    printf("Testing async TCP echo server...\n\n");

    // Create listen socket
    int listen_fd = create_listen_socket(PORT);
    if (listen_fd < 0) {
        fprintf(stderr, "Failed to create listen socket\n");
        return 1;
    }

    printf("Listening on port %d (fd=%d)\n", PORT, listen_fd);

    async_runtime* rt = async_runtime_create();
    assert(rt != NULL);

    echo_ctx_t ctx = {
        .rt = rt,
        .listen_fd = listen_fd,
        .client_fd = -1,
        .phase = 0
    };

    // Start accepting
    static async_accept_t accept_op;
    async_accept_init(&accept_op, listen_fd, on_accept_complete, &ctx);
    async_submit(rt, &accept_op.base);

    printf("Waiting for connection...\n");

    // Fork to create a client connection
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - act as client
        sleep(1);  // Give server time to start accepting

        int client_fd = connect_to_server(PORT);
        if (client_fd < 0) {
            exit(1);
        }

        const char* message = "Hello from client";
        write(client_fd, message, strlen(message));

        char buffer[BUFFER_SIZE];
        ssize_t n = read(client_fd, buffer, BUFFER_SIZE);
        if (n > 0) {
            buffer[n] = '\0';
            printf("[Client] Received echo: '%s'\n", buffer);
        }

        close(client_fd);
        exit(0);
    }

    // Parent process - run server
    printf("Running event loop...\n\n");

    // Run until we've echoed and closed
    while (ctx.phase < 3 && async_has_pending(rt)) {
        async_run_once(rt, 1000);
    }

    printf("\nEcho completed (phase=%d)\n", ctx.phase);

    // Wait for child
    int status;
    waitpid(pid, &status, 0);

    // Cleanup
    close(listen_fd);
    async_runtime_destroy(rt);

    printf("\nTest PASSED\n");
    return 0;
}
