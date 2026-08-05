#define _GNU_SOURCE

#include "test_common.h"
#include "syscall_throttle_ioctl.h"

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define TEST_PROGRAM_NAME "soa-revalidate"
#define TEST_MAX 1U

#define BLOCK_CHECK_DELAY_NS 200000000L
#define MAXIMUM_RELEASE_DELAY_NS 500000000ULL

struct worker_context {
    pthread_barrier_t *start_barrier;
    atomic_bool completed;
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

static void sleep_ns(long nanoseconds)
{
    struct timespec duration;

    duration.tv_sec = 0;
    duration.tv_nsec = nanoseconds;

    while (nanosleep(&duration, &duration) != 0) {
        if (errno != EINTR) {
            test_fail(
                "nanosleep fallita: %s",
                strerror(errno)
            );
        }
    }
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

static void set_test_program_name(void)
{
    if (prctl(
            PR_SET_NAME,
            TEST_PROGRAM_NAME,
            0,
            0,
            0) != 0) {
        test_fail(
            "impostazione del nome del thread fallita: %s",
            strerror(errno)
        );
    }
}

static void invoke_getpid(void)
{
    long result;

    result = syscall(SYS_getpid);

    if (result <= 0) {
        test_fail(
            "getpid ha restituito un valore non valido: %ld",
            result
        );
    }
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

    context->completion_ns = monotonic_time_ns();

    atomic_store_explicit(
        &context->completed,
        true,
        memory_order_release
    );

    return NULL;
}

static void verify_monitor_off_revalidation(int fd)
{
    struct worker_context context;
    pthread_barrier_t start_barrier;
    pthread_t worker;
    uint64_t disable_start_ns;
    uint64_t release_delay_ns;
    int result;

    memset(&context, 0, sizeof(context));

    atomic_init(&context.completed, false);

    result = pthread_barrier_init(
        &start_barrier,
        NULL,
        2
    );

    if (result != 0) {
        test_fail(
            "inizializzazione barriera fallita: %s",
            strerror(result)
        );
    }

    context.start_barrier = &start_barrier;

    /*
     * Consuma l'unico token disponibile.
     */
    invoke_getpid();

    result = pthread_create(
        &worker,
        NULL,
        worker_main,
        &context
    );

    if (result != 0) {
        test_fail(
            "creazione worker fallita: %s",
            strerror(result)
        );
    }

    result = pthread_barrier_wait(&start_barrier);

    if (result != 0 &&
        result != PTHREAD_BARRIER_SERIAL_THREAD) {
        test_fail(
            "rilascio barriera fallito: %s",
            strerror(result)
        );
    }

    /*
     * Il worker deve essere ancora bloccato:
     * il tick successivo è previsto dopo circa un secondo.
     */
    sleep_ns(BLOCK_CHECK_DELAY_NS);

    if (atomic_load_explicit(
            &context.completed,
            memory_order_acquire)) {
        test_fail(
            "il waiter è terminato prima della "
            "disattivazione del monitor"
        );
    }

    test_pass("waiter effettivamente bloccato");

    disable_start_ns = monotonic_time_ns();

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_DISABLE_MONITOR,
        NULL,
        "disattivazione monitor con waiter bloccato"
    );

    result = pthread_join(worker, NULL);

    if (result != 0) {
        test_fail(
            "join del worker fallita: %s",
            strerror(result)
        );
    }

    if (context.error_code != 0) {
        test_fail(
            "worker fallito: %s",
            strerror(context.error_code)
        );
    }

    if (!atomic_load_explicit(
            &context.completed,
            memory_order_acquire)) {
        test_fail(
            "il waiter non è terminato dopo monitor-off"
        );
    }

    if (context.completion_ns < disable_start_ns) {
        release_delay_ns = 0;
    } else {
        release_delay_ns =
            context.completion_ns - disable_start_ns;
    }

    printf(
        "INFO: rilascio dopo monitor-off = %" PRIu64 " ms\n",
        (uint64_t)(release_delay_ns / 1000000ULL)
    );

    if (release_delay_ns > MAXIMUM_RELEASE_DELAY_NS) {
        test_fail(
            "rilascio del waiter troppo lento: %" PRIu64 " ms",
            (uint64_t)(release_delay_ns / 1000000ULL)
        );
    }

    test_pass(
        "monitor-off ha risvegliato e rivalidato il waiter"
    );

    result = pthread_barrier_destroy(&start_barrier);

    if (result != 0) {
        test_fail(
            "distruzione barriera fallita: %s",
            strerror(result)
        );
    }
}

int main(void)
{
    struct syscall_throttle_program program;
    __u32 original_max;
    __u32 maximum;
    __u32 syscall_number;
    int fd;

    set_test_program_name();

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
        "registrazione programma del test"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_SYSCALL,
        &syscall_number,
        "registrazione syscall getpid"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_ENABLE_MONITOR,
        NULL,
        "attivazione monitor"
    );

    verify_monitor_off_revalidation(fd);

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
        "deregistrazione programma del test"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_SET_MAX,
        &original_max,
        "ripristino MAX originale"
    );

    test_close_device(fd);

    test_pass("test di rivalidazione completato");

    return EXIT_SUCCESS;
}
