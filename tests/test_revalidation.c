#define _GNU_SOURCE

#include "test_common.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>

#define PROGRAM_NAME "soa-revalidate"
#define BLOCK_CHECK_MS 200U
#define MAX_RELEASE_NS 500000000ULL

struct worker {
    pthread_barrier_t *barrier;
    atomic_bool completed;
    uint64_t completion_ns;
};

static void check_pthread(int result, const char *operation)
{
    if (result != 0)
        test_fail("%s fallita: %s",
                  operation, strerror(result));
}

static void wait_barrier(pthread_barrier_t *barrier)
{
    int result = pthread_barrier_wait(barrier);

    if (result != 0 &&
        result != PTHREAD_BARRIER_SERIAL_THREAD)
        test_fail("attesa barriera fallita: %s",
                  strerror(result));
}

static void *worker_main(void *argument)
{
    struct worker *worker = argument;

    test_set_thread_name(PROGRAM_NAME);
    wait_barrier(worker->barrier);

    test_invoke_syscall(SYS_getpid);
    worker->completion_ns = test_monotonic_time_ns();

    atomic_store_explicit(&worker->completed, true,
                          memory_order_release);
    return NULL;
}

int main(void)
{
    struct test_fixture fixture;
    struct worker worker;
    pthread_barrier_t barrier;
    pthread_t thread;
    uint64_t disable_ns;
    uint64_t release_ns;

    worker.barrier = &barrier;
    worker.completion_ns = 0;
    atomic_init(&worker.completed, false);

    test_fixture_setup(&fixture, PROGRAM_NAME, 1,
                       (__u32)SYS_getpid);
    test_fixture_enable_monitor(&fixture);

    check_pthread(
        pthread_barrier_init(&barrier, NULL, 2),
        "inizializzazione barriera"
    );

    /* Consuma l'unico token della finestra. */
    test_invoke_syscall(SYS_getpid);

    check_pthread(
        pthread_create(&thread, NULL,
                       worker_main, &worker),
        "creazione worker"
    );

    wait_barrier(&barrier);
    test_sleep_ms(BLOCK_CHECK_MS);

    if (atomic_load_explicit(&worker.completed,
                             memory_order_acquire))
        test_fail("il waiter non è rimasto bloccato");

    test_pass("waiter effettivamente bloccato");

    disable_ns = test_monotonic_time_ns();
    test_fixture_disable_monitor(&fixture);

    check_pthread(pthread_join(thread, NULL),
                  "join worker");

    if (worker.completion_ns < disable_ns)
        test_fail("waiter completato prima di monitor-off");

    release_ns = worker.completion_ns - disable_ns;

    printf("INFO: rilascio dopo monitor-off = "
           "%" PRIu64 " ms\n",
           (uint64_t)(release_ns / 1000000ULL));

    if (release_ns > MAX_RELEASE_NS)
        test_fail("rilascio troppo lento: %" PRIu64 " ms",
                  (uint64_t)(release_ns / 1000000ULL));

    test_pass("monitor-off ha rivalidato il waiter");

    check_pthread(pthread_barrier_destroy(&barrier),
                  "distruzione barriera");

    test_fixture_cleanup(&fixture);
    test_pass("test di rivalidazione completato");

    return EXIT_SUCCESS;
}
