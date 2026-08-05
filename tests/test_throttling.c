#define _GNU_SOURCE

#include "test_common.h"
#include "syscall_throttle_ioctl.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define TEST_PROGRAM_NAME "soa-throttle"
#define TEST_MAX 1U

#define MINIMUM_EXPECTED_DELAY_NS 400000000ULL
#define MAXIMUM_ATTEMPTS 3

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

static void invoke_test_syscall(void)
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

static uint64_t measure_second_invocation(int fd)
{
    uint64_t start_ns;
    uint64_t end_ns;

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_DISABLE_MONITOR,
        NULL,
        "reset monitor prima della misura"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_ENABLE_MONITOR,
        NULL,
        "attivazione monitor per la misura"
    );

    /*
     * La prima invocazione consuma l'unico token
     * disponibile nella finestra corrente.
     */
    invoke_test_syscall();

    start_ns = monotonic_time_ns();

    /*
     * Con MAX=1 questa invocazione deve attendere
     * l'apertura della finestra successiva.
     */
    invoke_test_syscall();

    end_ns = monotonic_time_ns();

    return end_ns - start_ns;
}

static void verify_throttling_delay(int fd)
{
    uint64_t delay_ns;
    uint64_t delay_ms;
    unsigned int attempt;

    for (attempt = 1;
         attempt <= MAXIMUM_ATTEMPTS;
         ++attempt) {
        delay_ns = measure_second_invocation(fd);
        delay_ms = (uint64_t)(
            delay_ns / 1000000ULL
        );

        printf(
            "INFO: tentativo %u, ritardo seconda syscall = "
            "%" PRIu64 " ms\n",
            attempt,
            delay_ms
        );

        if (delay_ns >= MINIMUM_EXPECTED_DELAY_NS) {
            test_pass(
                "seconda syscall bloccata fino alla finestra successiva"
            );
            return;
        }
    }

    test_fail(
        "ritardo insufficiente dopo %u tentativi: "
        "ultimo valore = %" PRIu64 " ms",
        MAXIMUM_ATTEMPTS,
        delay_ms
    );
}

int main(void)
{
    struct syscall_throttle_program program;
    __u32 original_max;
    __u32 maximum;
    __u32 syscall_number;
    int fd;

    if (prctl(
            PR_SET_NAME,
            TEST_PROGRAM_NAME,
            0,
            0,
            0) != 0) {
        test_fail(
            "impostazione del nome del processo fallita: %s",
            strerror(errno)
        );
    }

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

    verify_throttling_delay(fd);

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
        "deregistrazione programma del test"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_SET_MAX,
        &original_max,
        "ripristino MAX originale"
    );

    test_close_device(fd);

    test_pass("test del throttling completato");

    return EXIT_SUCCESS;
}
