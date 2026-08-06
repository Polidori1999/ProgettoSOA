#define _GNU_SOURCE

#include "test_common.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>

#define PROGRAM_NAME "soa-concurrent"
#define THREAD_COUNT 3
#define MAX_FIRST_NS 500000000ULL
#define MIN_GAP_NS 500000000ULL

struct worker {
    pthread_barrier_t *barrier;
    uint64_t *start_ns;
    uint64_t elapsed_ns;
};

static void check_pthread(int result, const char *operation)
{
    if (result != 0 &&
        result != PTHREAD_BARRIER_SERIAL_THREAD)
        test_fail("%s fallita: %s",
                  operation, strerror(result));
}

static void *worker_main(void *argument)
{
    struct worker *worker = argument;

    test_set_thread_name(PROGRAM_NAME);
    check_pthread(pthread_barrier_wait(worker->barrier),
                  "attesa barriera");

    test_invoke_syscall(SYS_getpid);
    worker->elapsed_ns =
        test_monotonic_time_ns() - *worker->start_ns;

    return NULL;
}

static int compare_u64(const void *left, const void *right)
{
    uint64_t a = *(const uint64_t *)left;
    uint64_t b = *(const uint64_t *)right;

    return (a > b) - (a < b);
}

int main(void)
{
    struct test_fixture fixture;
    struct worker workers[THREAD_COUNT] = {0};
    pthread_t threads[THREAD_COUNT];
    pthread_barrier_t barrier;
    uint64_t times[THREAD_COUNT];
    uint64_t start_ns;
    size_t index;

    test_fixture_setup(&fixture, PROGRAM_NAME, 1,
                       (__u32)SYS_getpid);

    check_pthread(
        pthread_barrier_init(&barrier, NULL,
                             THREAD_COUNT + 1),
        "inizializzazione barriera"
    );

    for (index = 0; index < THREAD_COUNT; ++index) {
        workers[index].barrier = &barrier;
        workers[index].start_ns = &start_ns;

        check_pthread(
            pthread_create(&threads[index], NULL,
                           worker_main, &workers[index]),
            "creazione worker"
        );
    }

    test_fixture_enable_monitor(&fixture);
    start_ns = test_monotonic_time_ns();

    check_pthread(pthread_barrier_wait(&barrier),
                  "rilascio barriera");

    for (index = 0; index < THREAD_COUNT; ++index) {
        check_pthread(pthread_join(threads[index], NULL),
                      "join worker");
        times[index] = workers[index].elapsed_ns;
    }

    check_pthread(pthread_barrier_destroy(&barrier),
                  "distruzione barriera");

    qsort(times, THREAD_COUNT, sizeof(times[0]), compare_u64);

    for (index = 0; index < THREAD_COUNT; ++index)
        printf("INFO: completamento %zu = %" PRIu64 " ms\n",
               index + 1,
               (uint64_t)(times[index] / 1000000ULL));

    if (times[0] > MAX_FIRST_NS ||
        times[1] - times[0] < MIN_GAP_NS ||
        times[2] - times[1] < MIN_GAP_NS)
        test_fail("più syscall completate nella stessa finestra");

    test_pass("una sola syscall completata per finestra");

    test_fixture_cleanup(&fixture);
    test_pass("test di concorrenza completato");

    return EXIT_SUCCESS;
}
