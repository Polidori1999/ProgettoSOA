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

#define TEST_PROGRAM_NAME "soa-revalidate"
#define TEST_MAX 1U

#define BLOCK_CHECK_DELAY_MS 200U
#define MAXIMUM_RELEASE_DELAY_NS 500000000ULL

struct worker_context {
    pthread_barrier_t *barrier;
    atomic_bool completed;
    uint64_t completion_ns;
};

static void check_pthread_result(
    int result,
    const char *operation)
{
    if (result != 0) {
        test_fail(
            "%s fallita: %s",
            operation,
            strerror(result)
        );
    }
}

static void check_barrier_result(
    int result,
    const char *operation)
{
    if (result != 0 &&
        result != PTHREAD_BARRIER_SERIAL_THREAD) {
        test_fail(
            "%s fallita: %s",
            operation,
            strerror(result)
        );
    }
}

static void *worker_main(void *argument)
{
    struct worker_context *context;

    context = argument;

    test_set_thread_name(TEST_PROGRAM_NAME);

    check_barrier_result(
        pthread_barrier_wait(context->barrier),
        "attesa worker sulla barriera"
    );

    test_invoke_syscall(SYS_getpid);

    context->completion_ns =
        test_monotonic_time_ns();

    atomic_store_explicit(
        &context->completed,
        true,
        memory_order_release
    );

    return NULL;
}

static void verify_monitor_off_revalidation(
    struct test_fixture *fixture)
{
    struct worker_context context;
    pthread_barrier_t barrier;
    pthread_t worker;
    uint64_t disable_start_ns;
    uint64_t release_delay_ns;

    context.barrier = &barrier;
    context.completion_ns = 0;

    atomic_init(&context.completed, false);

    check_pthread_result(
        pthread_barrier_init(&barrier, NULL, 2),
        "inizializzazione barriera"
    );

    /*
     * Consuma l'unico token disponibile.
     */
    test_invoke_syscall(fixture->syscall_number);

    check_pthread_result(
        pthread_create(
            &worker,
            NULL,
            worker_main,
            &context
        ),
        "creazione worker"
    );

    check_barrier_result(
        pthread_barrier_wait(&barrier),
        "rilascio barriera"
    );

    test_sleep_ms(BLOCK_CHECK_DELAY_MS);

    if (atomic_load_explicit(
            &context.completed,
            memory_order_acquire)) {
        test_fail(
            "il waiter è terminato prima "
            "della disattivazione del monitor"
        );
    }

    test_pass("waiter effettivamente bloccato");

    disable_start_ns = test_monotonic_time_ns();

    test_fixture_disable_monitor(fixture);

    check_pthread_result(
        pthread_join(worker, NULL),
        "join worker"
    );

    if (!atomic_load_explicit(
            &context.completed,
            memory_order_acquire)) {
        test_fail(
            "il waiter non è terminato "
            "dopo monitor-off"
        );
    }

    if (context.completion_ns < disable_start_ns) {
        release_delay_ns = 0;
    } else {
        release_delay_ns =
            context.completion_ns -
            disable_start_ns;
    }

    printf(
        "INFO: rilascio dopo monitor-off = "
        "%" PRIu64 " ms\n",
        (uint64_t)(release_delay_ns / 1000000ULL)
    );

    if (release_delay_ns >
        MAXIMUM_RELEASE_DELAY_NS) {
        test_fail(
            "rilascio del waiter troppo lento: "
            "%" PRIu64 " ms",
            (uint64_t)(release_delay_ns / 1000000ULL)
        );
    }

    test_pass(
        "monitor-off ha risvegliato "
        "e rivalidato il waiter"
    );

    check_pthread_result(
        pthread_barrier_destroy(&barrier),
        "distruzione barriera"
    );
}

int main(void)
{
    struct test_fixture fixture;

    test_fixture_setup(
        &fixture,
        TEST_PROGRAM_NAME,
        TEST_MAX,
        (__u32)SYS_getpid
    );

    test_fixture_enable_monitor(&fixture);

    verify_monitor_off_revalidation(&fixture);

    test_fixture_cleanup(&fixture);

    test_pass("test di rivalidazione completato");

    return EXIT_SUCCESS;
}
