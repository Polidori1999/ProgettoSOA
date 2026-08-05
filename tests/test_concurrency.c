#define _GNU_SOURCE

#include "test_common.h"
#include "syscall_throttle_ioctl.h"

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define TEST_PROGRAM_NAME "soa-concurrent"
#define THREAD_COUNT 3U
#define TEST_MAX 1U

#define FIRST_COMPLETION_MAX_NS 500000000ULL
#define MINIMUM_COMPLETION_GAP_NS 500000000ULL

struct worker_context {
    pthread_barrier_t *start_barrier;
    uint64_t *start_ns;
    uint64_t completion_ns;
    int error_code;
};

static uint64_t monotonic_time_ns(void)
{
    struct timespec timestamp;

    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0) {
        test_fail(
            "clock_gettime fallita: %s",
            strerror(errno)
        );
    }

    return
        (uint64_t)timestamp.tv_sec * 1000000000ULL +
        (uint64_t)timestamp.tv_nsec;
}

static struct syscall_throttle_program make_test_program(void)
{
    struct syscall_throttle_program program = {0};

    memcpy(
        program.name,
        TEST_PROGRAM_NAME,
        sizeof(TEST_PROGRAM_NAME)
    );

    return program;
}

static void *worker_main(void *argument)
{
    struct worker_context *context;
    int barrier_result;
    long syscall_result;

    context = argument;

    if (prctl(
            PR_SET_NAME,
            TEST_PROGRAM_NAME,
            0,
            0,
            0) != 0) {
        context->error_code = errno;
    }

    barrier_result = pthread_barrier_wait(
        context->start_barrier
    );

    if (barrier_result != 0 &&
        barrier_result != PTHREAD_BARRIER_SERIAL_THREAD) {
        context->error_code = barrier_result;
        return NULL;
    }

    if (context->error_code != 0)
        return NULL;

    syscall_result = syscall(SYS_getpid);

    if (syscall_result <= 0) {
        context->error_code = EIO;
        return NULL;
    }

    context->completion_ns =
        monotonic_time_ns() - *context->start_ns;

    return NULL;
}

static void sort_completion_times(
    uint64_t *values,
    size_t count)
{
    size_t first;
    size_t second;

    for (first = 0; first < count; ++first) {
        for (second = first + 1;
             second < count;
             ++second) {
            uint64_t temporary;

            if (values[first] <= values[second])
                continue;

            temporary = values[first];
            values[first] = values[second];
            values[second] = temporary;
        }
    }
}

static void verify_completion_times(
    const struct worker_context *contexts)
{
    uint64_t completion_times[THREAD_COUNT];
    uint64_t first_gap;
    uint64_t second_gap;
    size_t index;

    for (index = 0; index < THREAD_COUNT; ++index) {
        if (contexts[index].error_code != 0) {
            test_fail(
                "worker %zu fallito: %s",
                index,
                strerror(contexts[index].error_code)
            );
        }

        completion_times[index] =
            contexts[index].completion_ns;
    }

    sort_completion_times(
        completion_times,
        THREAD_COUNT
    );

    for (index = 0; index < THREAD_COUNT; ++index) {
        printf(
            "INFO: completamento %zu = %" PRIu64 " ms\n",
            index + 1,
            (uint64_t)(
                completion_times[index] / 1000000ULL
            )
        );
    }

    if (completion_times[0] >
        FIRST_COMPLETION_MAX_NS) {
        test_fail(
            "la prima syscall non è stata eseguita "
            "immediatamente"
        );
    }

    first_gap =
        completion_times[1] - completion_times[0];

    second_gap =
        completion_times[2] - completion_times[1];

    if (first_gap < MINIMUM_COMPLETION_GAP_NS) {
        test_fail(
            "prima e seconda syscall completate "
            "nella stessa finestra: distanza %" PRIu64 " ms",
            (uint64_t)(first_gap / 1000000ULL)
        );
    }

    if (second_gap < MINIMUM_COMPLETION_GAP_NS) {
        test_fail(
            "seconda e terza syscall completate "
            "nella stessa finestra: distanza %" PRIu64 " ms",
            (uint64_t)(second_gap / 1000000ULL)
        );
    }

    test_pass(
        "una sola syscall completata per finestra"
    );
}

static void run_concurrent_invocations(int fd)
{
    struct worker_context contexts[THREAD_COUNT] = {0};
    pthread_t threads[THREAD_COUNT];
    pthread_barrier_t start_barrier;
    uint64_t start_ns;
    size_t index;
    int result;

    result = pthread_barrier_init(
        &start_barrier,
        NULL,
        THREAD_COUNT + 1U
    );

    if (result != 0) {
        test_fail(
            "inizializzazione barriera fallita: %s",
            strerror(result)
        );
    }

    for (index = 0; index < THREAD_COUNT; ++index) {
        contexts[index].start_barrier =
            &start_barrier;
        contexts[index].start_ns = &start_ns;

        result = pthread_create(
            &threads[index],
            NULL,
            worker_main,
            &contexts[index]
        );

        if (result != 0) {
            test_fail(
                "creazione worker %zu fallita: %s",
                index,
                strerror(result)
            );
        }
    }

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_ENABLE_MONITOR,
        NULL,
        "attivazione monitor"
    );

    start_ns = monotonic_time_ns();

    result = pthread_barrier_wait(&start_barrier);

    if (result != 0 &&
        result != PTHREAD_BARRIER_SERIAL_THREAD) {
        test_fail(
            "rilascio barriera fallito: %s",
            strerror(result)
        );
    }

    for (index = 0; index < THREAD_COUNT; ++index) {
        result = pthread_join(threads[index], NULL);

        if (result != 0) {
            test_fail(
                "join worker %zu fallita: %s",
                index,
                strerror(result)
            );
        }
    }

    result = pthread_barrier_destroy(&start_barrier);

    if (result != 0) {
        test_fail(
            "distruzione barriera fallita: %s",
            strerror(result)
        );
    }

    verify_completion_times(contexts);
}

int main(void)
{
    struct syscall_throttle_program program;
    __u32 original_max;
    __u32 maximum;
    __u32 syscall_number;
    int fd;

    program = make_test_program();
    maximum = TEST_MAX;
    syscall_number = (__u32)SYS_getpid;

    fd = test_open_device();

    original_max = 0;

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_GET_MAX,
        &original_max,
        "lettura MAX originale"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_DISABLE_MONITOR,
        NULL,
        "monitor inizialmente disattivato"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_SET_MAX,
        &maximum,
        "impostazione MAX=1"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_PROGRAM,
        &program,
        "registrazione programma concorrente"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_SYSCALL,
        &syscall_number,
        "registrazione syscall getpid"
    );

    run_concurrent_invocations(fd);

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_DISABLE_MONITOR,
        NULL,
        "disattivazione monitor finale"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_SYSCALL,
        &syscall_number,
        "deregistrazione syscall getpid"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_PROGRAM,
        &program,
        "deregistrazione programma concorrente"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_SET_MAX,
        &original_max,
        "ripristino MAX originale"
    );

    test_close_device(fd);

    test_pass("test di concorrenza completato");

    return EXIT_SUCCESS;
}
