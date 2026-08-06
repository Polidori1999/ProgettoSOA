#define _GNU_SOURCE

#include "test_common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <time.h>
#include <unistd.h>

int test_open_device(void)
{
    int fd = open(TEST_DEVICE_PATH, O_RDWR);
    if (fd < 0)
        test_fail("apertura di %s fallita: %s",
                  TEST_DEVICE_PATH, strerror(errno));
    test_pass("device aperto");
    return fd;
}

void test_close_device(int fd)
{
    if (close(fd) != 0)
        test_fail("chiusura del device fallita: %s",
                  strerror(errno));
    test_pass("device chiuso");
}

void test_pass(const char *description)
{
    printf("PASS: %s\n", description);
}

_Noreturn void test_fail(const char *format, ...)
{
    va_list arguments;
    fprintf(stderr, "FAIL: ");
    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

void test_ioctl_success(int fd, unsigned long request, void *argument,
                        const char *description)
{
    if (ioctl(fd, request, argument) != 0)
        test_fail("%s: %s", description, strerror(errno));
    test_pass(description);
}

void test_ioctl_failure(int fd, unsigned long request, void *argument,
                        int expected_errno, const char *description)
{
    int result;
    int actual_errno;

    errno = 0;
    result = ioctl(fd, request, argument);
    actual_errno = errno;

    if (result != -1)
        test_fail("%s: ioctl completata senza errore", description);
    if (actual_errno != expected_errno)
        test_fail("%s: errno atteso %d, ottenuto %d (%s)",
                  description, expected_errno, actual_errno,
                  strerror(actual_errno));
    test_pass(description);
}

void test_expect_u32(const char *description, uint32_t actual,
                     uint32_t expected)
{
    if (actual != expected)
        test_fail("%s: atteso %u, ottenuto %u",
                  description, expected, actual);
    test_pass(description);
}

uint64_t test_monotonic_time_ns(void)
{
    struct timespec timestamp;
    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0)
        test_fail("clock_gettime fallita: %s", strerror(errno));
    return (uint64_t)timestamp.tv_sec * 1000000000ULL +
           (uint64_t)timestamp.tv_nsec;
}

void test_sleep_ms(unsigned int milliseconds)
{
    struct timespec duration = {
        .tv_sec = milliseconds / 1000U,
        .tv_nsec = (long)(milliseconds % 1000U) * 1000000L
    };

    while (nanosleep(&duration, &duration) != 0)
        if (errno != EINTR)
            test_fail("nanosleep fallita: %s", strerror(errno));
}

void test_set_thread_name(const char *name)
{
    if (prctl(PR_SET_NAME, name, 0, 0, 0) != 0)
        test_fail("impostazione nome thread fallita: %s",
                  strerror(errno));
}

long test_invoke_syscall(long syscall_number)
{
    long result = syscall(syscall_number);
    if (result < 0)
        test_fail("syscall %ld fallita: %s",
                  syscall_number, strerror(errno));
    return result;
}

void test_fixture_setup(struct test_fixture *fixture,
                        const char *program_name, __u32 maximum,
                        __u32 syscall_number)
{
    size_t length = strlen(program_name);

    if (length >= sizeof(fixture->program.name))
        test_fail("nome programma troppo lungo: %s", program_name);

    memset(fixture, 0, sizeof(*fixture));
    memcpy(fixture->program.name, program_name, length + 1);
    fixture->syscall_number = syscall_number;
    test_set_thread_name(program_name);
    fixture->fd = test_open_device();

    test_ioctl_success(fixture->fd, SYSCALL_THROTTLE_IOC_GET_MAX,
                       &fixture->original_max, "lettura MAX originale");
    test_ioctl_success(fixture->fd, SYSCALL_THROTTLE_IOC_DISABLE_MONITOR,
                       NULL, "monitor inizialmente disattivato");
    test_ioctl_success(fixture->fd, SYSCALL_THROTTLE_IOC_SET_MAX,
                       &maximum, "impostazione MAX del test");
    test_ioctl_success(fixture->fd, SYSCALL_THROTTLE_IOC_REGISTER_PROGRAM,
                       &fixture->program, "registrazione programma del test");
    test_ioctl_success(fixture->fd, SYSCALL_THROTTLE_IOC_REGISTER_SYSCALL,
                       &fixture->syscall_number,
                       "registrazione syscall del test");
}

void test_fixture_enable_monitor(struct test_fixture *fixture)
{
    test_ioctl_success(fixture->fd, SYSCALL_THROTTLE_IOC_ENABLE_MONITOR,
                       NULL, "attivazione monitor");
}

void test_fixture_disable_monitor(struct test_fixture *fixture)
{
    test_ioctl_success(fixture->fd, SYSCALL_THROTTLE_IOC_DISABLE_MONITOR,
                       NULL, "disattivazione monitor");
}

void test_fixture_cleanup(struct test_fixture *fixture)
{
    test_fixture_disable_monitor(fixture);
    test_ioctl_success(fixture->fd, SYSCALL_THROTTLE_IOC_UNREGISTER_SYSCALL,
                       &fixture->syscall_number,
                       "deregistrazione syscall del test");
    test_ioctl_success(fixture->fd, SYSCALL_THROTTLE_IOC_UNREGISTER_PROGRAM,
                       &fixture->program,
                       "deregistrazione programma del test");
    test_ioctl_success(fixture->fd, SYSCALL_THROTTLE_IOC_SET_MAX,
                       &fixture->original_max, "ripristino MAX originale");
    test_close_device(fixture->fd);
}
