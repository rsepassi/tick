/*
 * test_timer.c - Test the async timer functionality
 */

#include "async_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static int timer_count = 0;

void on_timer(async_t* op) {
    async_timer_t* timer = (async_timer_t*)op;
    timer_count++;
    printf("Timer %d expired (timeout was %lu ns)\n", timer_count, timer->timeout_ns);
}

int main(void) {
    printf("Testing async timer functionality...\n\n");

    async_runtime* rt = async_runtime_create();
    assert(rt != NULL);

    // Create multiple timers with different timeouts
    async_timer_t timer1, timer2, timer3;

    async_timer_init(&timer1, 100000000ULL, on_timer, NULL);  // 100ms
    async_timer_init(&timer2, 200000000ULL, on_timer, NULL);  // 200ms
    async_timer_init(&timer3, 50000000ULL, on_timer, NULL);   // 50ms

    printf("Submitting 3 timers: 50ms, 100ms, 200ms\n");

    async_submit(rt, &timer1.base);
    async_submit(rt, &timer2.base);
    async_submit(rt, &timer3.base);

    printf("Running event loop...\n\n");

    uint64_t start = async_time_now_ns();

    while (async_has_pending(rt)) {
        int completed = async_run_once(rt, -1);
        if (completed > 0) {
            uint64_t elapsed_ms = (async_time_now_ns() - start) / 1000000;
            printf("  Completed %d operations at %lu ms\n", completed, elapsed_ms);
        }
    }

    printf("\nAll timers expired. Total: %d\n", timer_count);
    assert(timer_count == 3);

    async_runtime_destroy(rt);

    printf("\nTest PASSED\n");
    return 0;
}
