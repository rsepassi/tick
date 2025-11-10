#include "mock_platform.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define INITIAL_CORO_CAPACITY 16
#define INITIAL_EVENT_CAPACITY 16

void mock_runtime_init(MockRuntime* runtime) {
    assert(runtime != NULL);

    runtime->coroutines = (MockCoroutine**)malloc(INITIAL_CORO_CAPACITY * sizeof(MockCoroutine*));
    runtime->coro_count = 0;
    runtime->coro_capacity = INITIAL_CORO_CAPACITY;

    runtime->events = (MockAsyncEvent**)malloc(INITIAL_EVENT_CAPACITY * sizeof(MockAsyncEvent*));
    runtime->event_count = 0;
    runtime->event_capacity = INITIAL_EVENT_CAPACITY;

    runtime->next_event_id = 1;
}

void mock_runtime_free(MockRuntime* runtime) {
    assert(runtime != NULL);

    // Free all coroutines
    for (size_t i = 0; i < runtime->coro_count; i++) {
        if (runtime->coroutines[i]) {
            if (runtime->coroutines[i]->stack) {
                free(runtime->coroutines[i]->stack);
            }
            free(runtime->coroutines[i]);
        }
    }
    free(runtime->coroutines);

    // Free all events
    for (size_t i = 0; i < runtime->event_count; i++) {
        if (runtime->events[i]) {
            free(runtime->events[i]);
        }
    }
    free(runtime->events);

    runtime->coroutines = NULL;
    runtime->events = NULL;
    runtime->coro_count = 0;
    runtime->event_count = 0;
}

MockCoroutine* mock_coro_create(MockRuntime* runtime, size_t stack_size) {
    assert(runtime != NULL);

    // Grow coroutines array if needed
    if (runtime->coro_count >= runtime->coro_capacity) {
        size_t new_capacity = runtime->coro_capacity * 2;
        MockCoroutine** new_coros = (MockCoroutine**)realloc(runtime->coroutines,
                                                              new_capacity * sizeof(MockCoroutine*));
        if (!new_coros) {
            return NULL;
        }
        runtime->coroutines = new_coros;
        runtime->coro_capacity = new_capacity;
    }

    // Create coroutine
    MockCoroutine* coro = (MockCoroutine*)malloc(sizeof(MockCoroutine));
    if (!coro) {
        return NULL;
    }

    // Allocate stack if requested
    if (stack_size > 0) {
        coro->stack = malloc(stack_size);
        if (!coro->stack) {
            free(coro);
            return NULL;
        }
    } else {
        coro->stack = NULL;
    }

    coro->stack_size = stack_size;
    coro->resume_point = NULL;
    coro->state = MOCK_CORO_READY;
    coro->result = 0;
    coro->user_data = NULL;

    runtime->coroutines[runtime->coro_count++] = coro;

    return coro;
}

void mock_coro_suspend(MockCoroutine* coro) {
    assert(coro != NULL);
    assert(coro->state == MOCK_CORO_RUNNING);

    coro->state = MOCK_CORO_SUSPENDED;
}

void mock_coro_resume(MockCoroutine* coro) {
    assert(coro != NULL);
    assert(coro->state == MOCK_CORO_READY || coro->state == MOCK_CORO_SUSPENDED);

    coro->state = MOCK_CORO_RUNNING;
}

bool mock_coro_is_done(MockCoroutine* coro) {
    assert(coro != NULL);
    return coro->state == MOCK_CORO_COMPLETED;
}

int mock_coro_get_result(MockCoroutine* coro) {
    assert(coro != NULL);
    assert(coro->state == MOCK_CORO_COMPLETED);
    return coro->result;
}

MockAsyncEvent* mock_event_create(MockRuntime* runtime) {
    assert(runtime != NULL);

    // Grow events array if needed
    if (runtime->event_count >= runtime->event_capacity) {
        size_t new_capacity = runtime->event_capacity * 2;
        MockAsyncEvent** new_events = (MockAsyncEvent**)realloc(runtime->events,
                                                                 new_capacity * sizeof(MockAsyncEvent*));
        if (!new_events) {
            return NULL;
        }
        runtime->events = new_events;
        runtime->event_capacity = new_capacity;
    }

    // Create event
    MockAsyncEvent* event = (MockAsyncEvent*)malloc(sizeof(MockAsyncEvent));
    if (!event) {
        return NULL;
    }

    event->id = runtime->next_event_id++;
    event->completed = false;
    event->result = 0;

    runtime->events[runtime->event_count++] = event;

    return event;
}

void mock_event_complete(MockAsyncEvent* event, int result) {
    assert(event != NULL);

    event->completed = true;
    event->result = result;
}

bool mock_event_is_ready(MockAsyncEvent* event) {
    assert(event != NULL);
    return event->completed;
}

int mock_event_get_result(MockAsyncEvent* event) {
    assert(event != NULL);
    assert(event->completed);
    return event->result;
}

void mock_runtime_step(MockRuntime* runtime) {
    assert(runtime != NULL);

    // Simple step: try to resume any suspended coroutines
    for (size_t i = 0; i < runtime->coro_count; i++) {
        MockCoroutine* coro = runtime->coroutines[i];
        if (coro && coro->state == MOCK_CORO_SUSPENDED) {
            // In a real runtime, we'd check if the event is ready
            // For this mock, we'll just leave it suspended
            // The test code can manually call mock_coro_resume
        }
    }
}

void mock_runtime_run_until_idle(MockRuntime* runtime) {
    assert(runtime != NULL);

    // Run until no coroutines are running
    bool has_running = true;
    while (has_running) {
        has_running = false;
        for (size_t i = 0; i < runtime->coro_count; i++) {
            MockCoroutine* coro = runtime->coroutines[i];
            if (coro && coro->state == MOCK_CORO_RUNNING) {
                has_running = true;
                break;
            }
        }

        if (has_running) {
            mock_runtime_step(runtime);
        }
    }
}
