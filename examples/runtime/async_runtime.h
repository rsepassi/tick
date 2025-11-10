#ifndef ASYNC_RUNTIME_H
#define ASYNC_RUNTIME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * Async Runtime - Platform abstraction for async I/O
 *
 * This header defines the interface between generated code and the
 * platform-specific async runtime (epoll, io_uring, kqueue, etc.)
 */

// Forward declarations
typedef struct async_t async_t;
typedef struct async_runtime async_runtime;

// Async operation callback
typedef void (*async_callback_t)(async_t* op);

// Base async operation structure
// All async operations embed this as the first member
struct async_t {
    uint32_t opcode;           // Operation type
    int32_t result;            // Operation result (bytes transferred, error code, etc.)
    void* ctx;                 // User context pointer
    async_callback_t callback; // Completion callback
    void* parent;              // Parent operation (for tracing)
    uint64_t req_id;           // Request ID (for tracing/debugging)

    // Runtime internal fields
    async_t* next;             // For linked lists
    uint64_t deadline_ns;      // Absolute deadline for timers
    bool submitted;            // Has been submitted to runtime
    bool completed;            // Operation has completed
};

// Operation codes
typedef enum {
    ASYNC_OP_NOP = 0,
    ASYNC_OP_READ,
    ASYNC_OP_WRITE,
    ASYNC_OP_ACCEPT,
    ASYNC_OP_CONNECT,
    ASYNC_OP_CLOSE,
    ASYNC_OP_TIMER,
    ASYNC_OP_CANCEL,
} async_opcode_t;

// Read operation
typedef struct {
    async_t base;
    int fd;
    void* buffer;
    size_t count;
} async_read_t;

// Write operation
typedef struct {
    async_t base;
    int fd;
    const void* buffer;
    size_t count;
} async_write_t;

// Accept operation
typedef struct {
    async_t base;
    int listen_fd;
    int client_fd;  // Result: accepted client fd
    uint32_t client_addr;  // IPv4 address
    uint16_t client_port;
} async_accept_t;

// Connect operation
typedef struct {
    async_t base;
    int fd;
    uint32_t addr;  // IPv4 address in network byte order
    uint16_t port;  // Port in host byte order
} async_connect_t;

// Close operation
typedef struct {
    async_t base;
    int fd;
} async_close_t;

// Timer operation
typedef struct {
    async_t base;
    uint64_t timeout_ns;  // Timeout in nanoseconds
} async_timer_t;

// Cancel operation
typedef struct {
    async_t base;
    async_t* target;  // Operation to cancel
} async_cancel_t;

// Runtime API

// Initialize the async runtime
// Returns NULL on failure
async_runtime* async_runtime_create(void);

// Destroy the runtime and free resources
void async_runtime_destroy(async_runtime* rt);

// Submit an async operation to the runtime
// The operation's callback will be invoked when it completes
void async_submit(async_runtime* rt, async_t* op);

// Cancel a pending operation
void async_cancel(async_runtime* rt, async_t* op);

// Run the event loop for one iteration
// timeout_ms: maximum time to wait for events (-1 = infinite, 0 = non-blocking)
// Returns: number of operations completed
int async_run_once(async_runtime* rt, int timeout_ms);

// Run the event loop until all operations complete
// Returns: total number of operations completed
int async_run_until_complete(async_runtime* rt);

// Check if there are any pending operations
bool async_has_pending(async_runtime* rt);

// Get current time in nanoseconds (monotonic)
uint64_t async_time_now_ns(void);

// Helper functions for initializing operations

static inline void async_read_init(async_read_t* op, int fd, void* buffer, size_t count,
                                   async_callback_t callback, void* ctx) {
    op->base.opcode = ASYNC_OP_READ;
    op->base.result = 0;
    op->base.ctx = ctx;
    op->base.callback = callback;
    op->base.parent = NULL;
    op->base.req_id = 0;
    op->base.next = NULL;
    op->base.submitted = false;
    op->base.completed = false;
    op->fd = fd;
    op->buffer = buffer;
    op->count = count;
}

static inline void async_write_init(async_write_t* op, int fd, const void* buffer, size_t count,
                                    async_callback_t callback, void* ctx) {
    op->base.opcode = ASYNC_OP_WRITE;
    op->base.result = 0;
    op->base.ctx = ctx;
    op->base.callback = callback;
    op->base.parent = NULL;
    op->base.req_id = 0;
    op->base.next = NULL;
    op->base.submitted = false;
    op->base.completed = false;
    op->fd = fd;
    op->buffer = buffer;
    op->count = count;
}

static inline void async_accept_init(async_accept_t* op, int listen_fd,
                                     async_callback_t callback, void* ctx) {
    op->base.opcode = ASYNC_OP_ACCEPT;
    op->base.result = 0;
    op->base.ctx = ctx;
    op->base.callback = callback;
    op->base.parent = NULL;
    op->base.req_id = 0;
    op->base.next = NULL;
    op->base.submitted = false;
    op->base.completed = false;
    op->listen_fd = listen_fd;
    op->client_fd = -1;
    op->client_addr = 0;
    op->client_port = 0;
}

static inline void async_connect_init(async_connect_t* op, int fd, uint32_t addr, uint16_t port,
                                      async_callback_t callback, void* ctx) {
    op->base.opcode = ASYNC_OP_CONNECT;
    op->base.result = 0;
    op->base.ctx = ctx;
    op->base.callback = callback;
    op->base.parent = NULL;
    op->base.req_id = 0;
    op->base.next = NULL;
    op->base.submitted = false;
    op->base.completed = false;
    op->fd = fd;
    op->addr = addr;
    op->port = port;
}

static inline void async_timer_init(async_timer_t* op, uint64_t timeout_ns,
                                    async_callback_t callback, void* ctx) {
    op->base.opcode = ASYNC_OP_TIMER;
    op->base.result = 0;
    op->base.ctx = ctx;
    op->base.callback = callback;
    op->base.parent = NULL;
    op->base.req_id = 0;
    op->base.next = NULL;
    op->base.submitted = false;
    op->base.completed = false;
    op->timeout_ns = timeout_ns;
    op->base.deadline_ns = 0;  // Set by runtime
}

static inline void async_close_init(async_close_t* op, int fd,
                                    async_callback_t callback, void* ctx) {
    op->base.opcode = ASYNC_OP_CLOSE;
    op->base.result = 0;
    op->base.ctx = ctx;
    op->base.callback = callback;
    op->base.parent = NULL;
    op->base.req_id = 0;
    op->base.next = NULL;
    op->base.submitted = false;
    op->base.completed = false;
    op->fd = fd;
}

#endif // ASYNC_RUNTIME_H
