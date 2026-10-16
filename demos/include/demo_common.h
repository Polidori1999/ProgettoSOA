#ifndef DEMO_COMMON_H
#define DEMO_COMMON_H

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Utilita user-space condivise: nessuna configurazione del monitor. */
static inline void demo_fail(const char *operation)
{
    perror(operation);
    exit(EXIT_FAILURE);
}

static inline void demo_check_pthread(int result, const char *operation)
{
    if (result != 0) {
        fprintf(stderr, "%s: %s\n", operation, strerror(result));
        exit(EXIT_FAILURE);
    }
}

static inline void demo_wait_barrier(pthread_barrier_t *barrier)
{
    int result = pthread_barrier_wait(barrier);

    if (result != PTHREAD_BARRIER_SERIAL_THREAD)
        demo_check_pthread(result, "pthread_barrier_wait");
}

static inline struct timespec demo_now(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        demo_fail("clock_gettime");

    return now;
}

static inline uint64_t demo_now_ns(void)
{
    struct timespec now = demo_now();

    return (uint64_t)now.tv_sec * UINT64_C(1000000000) +
           (uint64_t)now.tv_nsec;
}

static inline long demo_elapsed_ms(const struct timespec *start,
                                   const struct timespec *end)
{
    return (end->tv_sec - start->tv_sec) * 1000L +
           (end->tv_nsec - start->tv_nsec) / 1000000L;
}

#endif /* DEMO_COMMON_H */
