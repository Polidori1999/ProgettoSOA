#define _GNU_SOURCE

#include "test_common.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>

#define TEST_PROGRAM_NAME "soa-concurrent"
#define THREAD_COUNT 3U
#define TEST_MAX 1U

#define FIRST_COMPLETION_MAX_NS 500000000ULL
#define MINIMUM_COMPLETION_GAP_NS 500000000ULL

struct worker_context {
    pthread_barrier_t *barrier;
    const uint64_t *start_ns;
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
        "attesa sulla barriera"
    );

    test_invoke_syscall(SYS_getpid);

    context->completion_ns =
        test_monotonic_time_ns() -
        *context->start_ns;

    return NULL;
}

static int compare_u64(
    const void *left,
    const void *right)
{
    const uint64_t left_value =
        *(const uint64_t *)left;

    const uint64_t right_value =
        *(const uint64_t *)right;

    return
        (left_value > right_value) -
        (left_value < right_value);
}

static void verify_completion_times(
    const struct worker_context *contexts)
{
    uint64_t completion_times[THREAD_COUNT];
    uint64_t first_gap;
    uint64_t second_gap;
    size_t index;

    for (index = 0; index < THREAD_COUNT; ++index) {
        completion_times[index] =
            contexts[index].completion_ns;
    }

    qsort(
        completion_times,
        THREAD_COUNT,
        sizeof(completion_times[0]),
        compare_u64
    );

    for (index = 0; index < THREAD_COUNT; ++index) {
        printf(
            "INFO: completamento %zu = "
            "%" PRIu64 " ms\n",
            index + 1,
            (uint64_t)(
                completion_times[index] /
                1000000ULL
            )
        );
    }

    if (completion_times[0] >
        FIRST_COMPLETION_MAX_NS) {
        test_fail(
            "la prima syscall non è stata "
            "eseguita immediatamente"
        );
    }

    first_gap =
        completion_times[1] -
        completion_times[0];

    second_gap =
        completion_times[2] -
        completion_times[1];

    if (first_gap < MINIMUM_COMPLETION_GAP_NS) {
        test_fail(
            "prima e seconda syscall nella stessa "
            "finestra: distanza %" PRIu64 " ms",
            (uint64_t)(first_gap / 1000000ULL)
        );
    }

    if (second_gap < MINIMUM_COMPLETION_GAP_NS) {
        test_fail(
            "seconda e terza syscall nella stessa "
            "finestra: distanza %" PRIu64 " ms",
            (uint64_t)(second_gap / 1000000ULL)
        );
    }

    test_pass(
        "una sola syscall completata per finestra"
    );
}

static void run_concurrent_invocations(
    struct test_fixture *fixture)
{
    struct worker_context contexts[THREAD_COUNT] = {0};
    pthread_t threads[THREAD_COUNT];
    pthread_barrier_t barrier;
    uint64_t start_ns;
    size_t index;

    check_pthread_result(
        pthread_barrier_init(
            &barrier,
            NULL,
            THREAD_COUNT + 1U
        ),
        "inizializzazione barriera"
    );

    for (index = 0; index < THREAD_COUNT; ++index) {
        contexts[index].barrier = &barrier;
        contexts[index].start_ns = &start_ns;

        check_pthread_result(
            pthread_create(
                &threads[index],
                NULL,
                worker_main,
                &contexts[index]
            ),
            "creazione worker"
        );
    }

    test_fixture_enable_monitor(fixture);

    start_ns = test_monotonic_time_ns();

    check_barrier_result(
        pthread_barrier_wait(&barrier),
        "rilascio barriera"
    );

    for (index = 0; index < THREAD_COUNT; ++index) {
        check_pthread_result(
            pthread_join(threads[index], NULL),
            "join worker"
        );
    }

    check_pthread_result(
        pthread_barrier_destroy(&barrier),
        "distruzione barriera"
    );

    verify_completion_times(contexts);
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

    run_concurrent_invocations(&fixture);

    test_fixture_cleanup(&fixture);

    test_pass("test di concorrenza completato");

    return EXIT_SUCCESS;
}
