#ifndef MOCK_PLATFORM_H
#define MOCK_PLATFORM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Mock Platform Interface
// Purpose: Simulate async runtime for testing coroutine transformations

// Mock coroutine state
typedef enum {
    MOCK_CORO_READY,
    MOCK_CORO_RUNNING,
    MOCK_CORO_SUSPENDED,
    MOCK_CORO_COMPLETED,
} MockCoroState;

// Mock coroutine handle
typedef struct MockCoroutine {
    void* stack;
    size_t stack_size;
    void* resume_point;
    MockCoroState state;
    int result;
    void* user_data;
} MockCoroutine;

// Mock async event
typedef struct MockAsyncEvent {
    uint64_t id;
    bool completed;
    int result;
} MockAsyncEvent;

// Mock async runtime
typedef struct MockRuntime {
    MockCoroutine** coroutines;
    size_t coro_count;
    size_t coro_capacity;

    MockAsyncEvent** events;
    size_t event_count;
    size_t event_capacity;

    uint64_t next_event_id;
} MockRuntime;

// Runtime management
void mock_runtime_init(MockRuntime* runtime);
void mock_runtime_free(MockRuntime* runtime);

// Coroutine operations
MockCoroutine* mock_coro_create(MockRuntime* runtime, size_t stack_size);
void mock_coro_suspend(MockCoroutine* coro);
void mock_coro_resume(MockCoroutine* coro);
bool mock_coro_is_done(MockCoroutine* coro);
int mock_coro_get_result(MockCoroutine* coro);

// Event operations
MockAsyncEvent* mock_event_create(MockRuntime* runtime);
void mock_event_complete(MockAsyncEvent* event, int result);
bool mock_event_is_ready(MockAsyncEvent* event);
int mock_event_get_result(MockAsyncEvent* event);

// Test helpers
void mock_runtime_step(MockRuntime* runtime);
void mock_runtime_run_until_idle(MockRuntime* runtime);

#endif // MOCK_PLATFORM_H
