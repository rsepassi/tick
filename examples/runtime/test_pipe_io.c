/*
 * test_pipe_io.c - Test async read/write with pipes
 */

#include "async_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

typedef struct {
    async_runtime* rt;
    int read_fd;
    int write_fd;
    char read_buffer[BUFFER_SIZE];
    char write_buffer[BUFFER_SIZE];
    int phase;
} test_ctx_t;

void on_write_complete(async_t* op);
void on_read_complete(async_t* op);

void on_write_complete(async_t* op) {
    test_ctx_t* ctx = (test_ctx_t*)op->ctx;
    printf("Write completed: %d bytes\n", op->result);
    assert(op->result > 0);

    // Now read what we wrote
    static async_read_t read_op;
    async_read_init(&read_op, ctx->read_fd, ctx->read_buffer, BUFFER_SIZE,
                   on_read_complete, ctx);
    async_submit(ctx->rt, &read_op.base);
}

void on_read_complete(async_t* op) {
    test_ctx_t* ctx = (test_ctx_t*)op->ctx;
    printf("Read completed: %d bytes\n", op->result);
    assert(op->result > 0);

    async_read_t* read_op = (async_read_t*)op;
    ctx->read_buffer[op->result] = '\0';
    printf("Read data: '%s'\n", ctx->read_buffer);

    // Verify data matches
    assert(strcmp(ctx->read_buffer, ctx->write_buffer) == 0);

    ctx->phase++;
}

int main(void) {
    printf("Testing async pipe I/O...\n\n");

    // Create a pipe
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    printf("Created pipe: read_fd=%d, write_fd=%d\n", pipefd[0], pipefd[1]);

    async_runtime* rt = async_runtime_create();
    assert(rt != NULL);

    test_ctx_t ctx = {
        .rt = rt,
        .read_fd = pipefd[0],
        .write_fd = pipefd[1],
        .phase = 0
    };

    // Prepare data to write
    const char* message = "Hello, async world!";
    strncpy(ctx.write_buffer, message, BUFFER_SIZE);

    printf("Writing '%s' to pipe...\n", ctx.write_buffer);

    // Submit write operation
    static async_write_t write_op;
    async_write_init(&write_op, ctx.write_fd, ctx.write_buffer, strlen(ctx.write_buffer),
                    on_write_complete, &ctx);
    async_submit(rt, &write_op.base);

    // Run event loop
    printf("Running event loop...\n\n");
    int total_completed = async_run_until_complete(rt);

    printf("\nCompleted %d operations\n", total_completed);
    assert(ctx.phase == 1);  // Should have completed write and read

    // Cleanup
    close(pipefd[0]);
    close(pipefd[1]);
    async_runtime_destroy(rt);

    printf("\nTest PASSED\n");
    return 0;
}
