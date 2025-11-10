#define _POSIX_C_SOURCE 199309L
#include "async_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_EVENTS 64
#define INITIAL_FD_MAP_SIZE 64

// FD to operation mapping
typedef struct {
    int fd;
    async_t* op;
    uint32_t events;  // EPOLLIN, EPOLLOUT, etc.
} fd_mapping_t;

// Async runtime structure
struct async_runtime {
    int epoll_fd;

    // Submission queue (linked list)
    async_t* submit_head;
    async_t* submit_tail;

    // Completion queue (linked list)
    async_t* complete_head;
    async_t* complete_tail;

    // Timer queue (sorted linked list by deadline)
    async_t* timer_head;

    // FD to operation mapping
    fd_mapping_t* fd_map;
    size_t fd_map_size;
    size_t fd_map_capacity;

    // Stats
    uint64_t operations_submitted;
    uint64_t operations_completed;
};

// Helper: Set FD to non-blocking mode
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Helper: Find FD mapping
static fd_mapping_t* find_fd_mapping(async_runtime* rt, int fd) {
    for (size_t i = 0; i < rt->fd_map_size; i++) {
        if (rt->fd_map[i].fd == fd) {
            return &rt->fd_map[i];
        }
    }
    return NULL;
}

// Helper: Add FD mapping
static int add_fd_mapping(async_runtime* rt, int fd, async_t* op, uint32_t events) {
    // Check if already mapped
    fd_mapping_t* existing = find_fd_mapping(rt, fd);
    if (existing) {
        existing->op = op;
        existing->events = events;
        return 0;
    }

    // Expand if needed
    if (rt->fd_map_size >= rt->fd_map_capacity) {
        size_t new_cap = rt->fd_map_capacity * 2;
        fd_mapping_t* new_map = realloc(rt->fd_map, new_cap * sizeof(fd_mapping_t));
        if (!new_map) return -1;
        rt->fd_map = new_map;
        rt->fd_map_capacity = new_cap;
    }

    // Add mapping
    rt->fd_map[rt->fd_map_size].fd = fd;
    rt->fd_map[rt->fd_map_size].op = op;
    rt->fd_map[rt->fd_map_size].events = events;
    rt->fd_map_size++;

    return 0;
}

// Helper: Remove FD mapping
static void remove_fd_mapping(async_runtime* rt, int fd) {
    for (size_t i = 0; i < rt->fd_map_size; i++) {
        if (rt->fd_map[i].fd == fd) {
            // Swap with last element and reduce size
            rt->fd_map[i] = rt->fd_map[rt->fd_map_size - 1];
            rt->fd_map_size--;
            return;
        }
    }
}

// Helper: Add to completion queue
static void add_to_completion_queue(async_runtime* rt, async_t* op) {
    op->next = NULL;
    op->completed = true;

    if (rt->complete_tail) {
        rt->complete_tail->next = op;
        rt->complete_tail = op;
    } else {
        rt->complete_head = rt->complete_tail = op;
    }
}

// Helper: Insert timer in sorted order
static void insert_timer(async_runtime* rt, async_t* op, uint64_t deadline_ns) {
    op->deadline_ns = deadline_ns;
    op->next = NULL;

    // Empty list
    if (!rt->timer_head) {
        rt->timer_head = op;
        return;
    }

    // Insert at head
    if (deadline_ns < rt->timer_head->deadline_ns) {
        op->next = rt->timer_head;
        rt->timer_head = op;
        return;
    }

    // Find insertion point
    async_t* prev = rt->timer_head;
    async_t* curr = prev->next;

    while (curr && curr->deadline_ns <= deadline_ns) {
        prev = curr;
        curr = curr->next;
    }

    prev->next = op;
    op->next = curr;
}

// Helper: Process expired timers
static void process_timers(async_runtime* rt, uint64_t now_ns) {
    while (rt->timer_head && rt->timer_head->deadline_ns <= now_ns) {
        async_t* op = rt->timer_head;
        rt->timer_head = op->next;

        op->result = 0;  // Success
        add_to_completion_queue(rt, op);
    }
}

// Helper: Get next timer deadline
static int64_t get_next_timer_deadline(async_runtime* rt, uint64_t now_ns) {
    if (!rt->timer_head) {
        return -1;  // No timers
    }

    int64_t diff = (int64_t)(rt->timer_head->deadline_ns - now_ns);
    return diff > 0 ? diff : 0;
}

// Initialize runtime
async_runtime* async_runtime_create(void) {
    async_runtime* rt = calloc(1, sizeof(async_runtime));
    if (!rt) return NULL;

    rt->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (rt->epoll_fd == -1) {
        free(rt);
        return NULL;
    }

    rt->fd_map_capacity = INITIAL_FD_MAP_SIZE;
    rt->fd_map = malloc(rt->fd_map_capacity * sizeof(fd_mapping_t));
    if (!rt->fd_map) {
        close(rt->epoll_fd);
        free(rt);
        return NULL;
    }

    return rt;
}

// Destroy runtime
void async_runtime_destroy(async_runtime* rt) {
    if (!rt) return;

    if (rt->epoll_fd >= 0) {
        close(rt->epoll_fd);
    }

    free(rt->fd_map);
    free(rt);
}

// Get current monotonic time in nanoseconds
uint64_t async_time_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// Submit operation
void async_submit(async_runtime* rt, async_t* op) {
    if (!rt || !op || op->submitted) return;

    op->submitted = true;
    op->next = NULL;
    rt->operations_submitted++;

    switch (op->opcode) {
        case ASYNC_OP_READ: {
            async_read_t* read_op = (async_read_t*)op;
            set_nonblocking(read_op->fd);

            struct epoll_event ev = {0};
            ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
            ev.data.ptr = op;

            if (epoll_ctl(rt->epoll_fd, EPOLL_CTL_ADD, read_op->fd, &ev) == 0 ||
                errno == EEXIST) {
                if (errno == EEXIST) {
                    epoll_ctl(rt->epoll_fd, EPOLL_CTL_MOD, read_op->fd, &ev);
                }
                add_fd_mapping(rt, read_op->fd, op, EPOLLIN);
            } else {
                op->result = -errno;
                add_to_completion_queue(rt, op);
            }
            break;
        }

        case ASYNC_OP_WRITE: {
            async_write_t* write_op = (async_write_t*)op;
            set_nonblocking(write_op->fd);

            struct epoll_event ev = {0};
            ev.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
            ev.data.ptr = op;

            if (epoll_ctl(rt->epoll_fd, EPOLL_CTL_ADD, write_op->fd, &ev) == 0 ||
                errno == EEXIST) {
                if (errno == EEXIST) {
                    epoll_ctl(rt->epoll_fd, EPOLL_CTL_MOD, write_op->fd, &ev);
                }
                add_fd_mapping(rt, write_op->fd, op, EPOLLOUT);
            } else {
                op->result = -errno;
                add_to_completion_queue(rt, op);
            }
            break;
        }

        case ASYNC_OP_ACCEPT: {
            async_accept_t* accept_op = (async_accept_t*)op;
            set_nonblocking(accept_op->listen_fd);

            struct epoll_event ev = {0};
            ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
            ev.data.ptr = op;

            if (epoll_ctl(rt->epoll_fd, EPOLL_CTL_ADD, accept_op->listen_fd, &ev) == 0 ||
                errno == EEXIST) {
                if (errno == EEXIST) {
                    epoll_ctl(rt->epoll_fd, EPOLL_CTL_MOD, accept_op->listen_fd, &ev);
                }
                add_fd_mapping(rt, accept_op->listen_fd, op, EPOLLIN);
            } else {
                op->result = -errno;
                add_to_completion_queue(rt, op);
            }
            break;
        }

        case ASYNC_OP_CONNECT: {
            async_connect_t* connect_op = (async_connect_t*)op;
            set_nonblocking(connect_op->fd);

            struct sockaddr_in addr = {0};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = connect_op->addr;
            addr.sin_port = htons(connect_op->port);

            int ret = connect(connect_op->fd, (struct sockaddr*)&addr, sizeof(addr));
            if (ret == 0) {
                // Connected immediately
                op->result = 0;
                add_to_completion_queue(rt, op);
            } else if (errno == EINPROGRESS) {
                // Connection in progress, wait for writability
                struct epoll_event ev = {0};
                ev.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
                ev.data.ptr = op;

                if (epoll_ctl(rt->epoll_fd, EPOLL_CTL_ADD, connect_op->fd, &ev) == 0 ||
                    errno == EEXIST) {
                    if (errno == EEXIST) {
                        epoll_ctl(rt->epoll_fd, EPOLL_CTL_MOD, connect_op->fd, &ev);
                    }
                    add_fd_mapping(rt, connect_op->fd, op, EPOLLOUT);
                } else {
                    op->result = -errno;
                    add_to_completion_queue(rt, op);
                }
            } else {
                op->result = -errno;
                add_to_completion_queue(rt, op);
            }
            break;
        }

        case ASYNC_OP_CLOSE: {
            async_close_t* close_op = (async_close_t*)op;
            epoll_ctl(rt->epoll_fd, EPOLL_CTL_DEL, close_op->fd, NULL);
            remove_fd_mapping(rt, close_op->fd);
            int ret = close(close_op->fd);
            op->result = (ret == 0) ? 0 : -errno;
            add_to_completion_queue(rt, op);
            break;
        }

        case ASYNC_OP_TIMER: {
            async_timer_t* timer_op = (async_timer_t*)op;
            uint64_t now = async_time_now_ns();
            uint64_t deadline = now + timer_op->timeout_ns;
            insert_timer(rt, op, deadline);
            break;
        }

        default:
            op->result = -EINVAL;
            add_to_completion_queue(rt, op);
            break;
    }
}

// Run event loop once
int async_run_once(async_runtime* rt, int timeout_ms) {
    if (!rt) return -1;

    uint64_t now = async_time_now_ns();

    // Process any expired timers
    process_timers(rt, now);

    // Adjust timeout based on next timer
    int64_t next_timer_ns = get_next_timer_deadline(rt, now);
    if (next_timer_ns >= 0) {
        int timer_ms = (int)(next_timer_ns / 1000000);
        if (timeout_ms < 0 || timer_ms < timeout_ms) {
            timeout_ms = timer_ms;
        }
    }

    // Wait for events
    struct epoll_event events[MAX_EVENTS];
    int nfds = epoll_wait(rt->epoll_fd, events, MAX_EVENTS, timeout_ms);

    if (nfds < 0) {
        if (errno == EINTR) return 0;
        return -1;
    }

    // Process events
    for (int i = 0; i < nfds; i++) {
        async_t* op = (async_t*)events[i].data.ptr;
        if (!op) continue;

        switch (op->opcode) {
            case ASYNC_OP_READ: {
                async_read_t* read_op = (async_read_t*)op;
                ssize_t n = read(read_op->fd, read_op->buffer, read_op->count);
                if (n >= 0) {
                    op->result = (int32_t)n;
                    remove_fd_mapping(rt, read_op->fd);
                    epoll_ctl(rt->epoll_fd, EPOLL_CTL_DEL, read_op->fd, NULL);
                    add_to_completion_queue(rt, op);
                } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    op->result = -errno;
                    remove_fd_mapping(rt, read_op->fd);
                    epoll_ctl(rt->epoll_fd, EPOLL_CTL_DEL, read_op->fd, NULL);
                    add_to_completion_queue(rt, op);
                }
                break;
            }

            case ASYNC_OP_WRITE: {
                async_write_t* write_op = (async_write_t*)op;
                ssize_t n = write(write_op->fd, write_op->buffer, write_op->count);
                if (n >= 0) {
                    op->result = (int32_t)n;
                    remove_fd_mapping(rt, write_op->fd);
                    epoll_ctl(rt->epoll_fd, EPOLL_CTL_DEL, write_op->fd, NULL);
                    add_to_completion_queue(rt, op);
                } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    op->result = -errno;
                    remove_fd_mapping(rt, write_op->fd);
                    epoll_ctl(rt->epoll_fd, EPOLL_CTL_DEL, write_op->fd, NULL);
                    add_to_completion_queue(rt, op);
                }
                break;
            }

            case ASYNC_OP_ACCEPT: {
                async_accept_t* accept_op = (async_accept_t*)op;
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);

                int client_fd = accept(accept_op->listen_fd,
                                      (struct sockaddr*)&client_addr,
                                      &addr_len);
                if (client_fd >= 0) {
                    accept_op->client_fd = client_fd;
                    accept_op->client_addr = client_addr.sin_addr.s_addr;
                    accept_op->client_port = ntohs(client_addr.sin_port);
                    op->result = 0;
                    remove_fd_mapping(rt, accept_op->listen_fd);
                    epoll_ctl(rt->epoll_fd, EPOLL_CTL_DEL, accept_op->listen_fd, NULL);
                    add_to_completion_queue(rt, op);
                } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    op->result = -errno;
                    remove_fd_mapping(rt, accept_op->listen_fd);
                    epoll_ctl(rt->epoll_fd, EPOLL_CTL_DEL, accept_op->listen_fd, NULL);
                    add_to_completion_queue(rt, op);
                }
                break;
            }

            case ASYNC_OP_CONNECT: {
                async_connect_t* connect_op = (async_connect_t*)op;
                int err = 0;
                socklen_t err_len = sizeof(err);

                if (getsockopt(connect_op->fd, SOL_SOCKET, SO_ERROR, &err, &err_len) == 0) {
                    op->result = err ? -err : 0;
                } else {
                    op->result = -errno;
                }

                remove_fd_mapping(rt, connect_op->fd);
                epoll_ctl(rt->epoll_fd, EPOLL_CTL_DEL, connect_op->fd, NULL);
                add_to_completion_queue(rt, op);
                break;
            }

            default:
                break;
        }
    }

    // Process timers again in case some expired during event processing
    now = async_time_now_ns();
    process_timers(rt, now);

    // Process completion queue
    int completed = 0;
    while (rt->complete_head) {
        async_t* op = rt->complete_head;
        rt->complete_head = op->next;

        if (op == rt->complete_tail) {
            rt->complete_tail = NULL;
        }

        if (op->callback) {
            op->callback(op);
        }

        completed++;
        rt->operations_completed++;
    }

    return completed;
}

// Run until all operations complete
int async_run_until_complete(async_runtime* rt) {
    if (!rt) return -1;

    int total = 0;
    while (async_has_pending(rt)) {
        int n = async_run_once(rt, -1);
        if (n < 0) return -1;
        total += n;
    }

    return total;
}

// Check if there are pending operations
bool async_has_pending(async_runtime* rt) {
    if (!rt) return false;
    return rt->fd_map_size > 0 || rt->timer_head != NULL || rt->complete_head != NULL;
}

// Cancel operation
void async_cancel(async_runtime* rt, async_t* op) {
    if (!rt || !op) return;

    // Remove from epoll if it's an FD-based operation
    switch (op->opcode) {
        case ASYNC_OP_READ: {
            async_read_t* read_op = (async_read_t*)op;
            epoll_ctl(rt->epoll_fd, EPOLL_CTL_DEL, read_op->fd, NULL);
            remove_fd_mapping(rt, read_op->fd);
            break;
        }
        case ASYNC_OP_WRITE: {
            async_write_t* write_op = (async_write_t*)op;
            epoll_ctl(rt->epoll_fd, EPOLL_CTL_DEL, write_op->fd, NULL);
            remove_fd_mapping(rt, write_op->fd);
            break;
        }
        case ASYNC_OP_ACCEPT: {
            async_accept_t* accept_op = (async_accept_t*)op;
            epoll_ctl(rt->epoll_fd, EPOLL_CTL_DEL, accept_op->listen_fd, NULL);
            remove_fd_mapping(rt, accept_op->listen_fd);
            break;
        }
        case ASYNC_OP_CONNECT: {
            async_connect_t* connect_op = (async_connect_t*)op;
            epoll_ctl(rt->epoll_fd, EPOLL_CTL_DEL, connect_op->fd, NULL);
            remove_fd_mapping(rt, connect_op->fd);
            break;
        }
        case ASYNC_OP_TIMER: {
            // Remove from timer queue
            if (rt->timer_head == op) {
                rt->timer_head = op->next;
            } else {
                async_t* prev = rt->timer_head;
                while (prev && prev->next != op) {
                    prev = prev->next;
                }
                if (prev) {
                    prev->next = op->next;
                }
            }
            break;
        }
        default:
            break;
    }

    op->result = -ECANCELED;
    add_to_completion_queue(rt, op);
}
