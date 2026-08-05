#include "test_common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int test_open_device(void)
{
    int fd;

    fd = open(TEST_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        test_fail(
            "apertura di %s fallita: %s",
            TEST_DEVICE_PATH,
            strerror(errno)
        );
    }

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

_Noreturn void test_fail(
    const char *format,
    ...)
{
    va_list arguments;

    fprintf(stderr, "FAIL: ");

    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);

    fputc('\n', stderr);

    exit(EXIT_FAILURE);
}

void test_ioctl_success(
    int fd,
    unsigned long request,
    void *argument,
    const char *description)
{
    if (ioctl(fd, request, argument) != 0) {
        test_fail(
            "%s: %s",
            description,
            strerror(errno)
        );
    }

    test_pass(description);
}

void test_ioctl_failure(
    int fd,
    unsigned long request,
    void *argument,
    int expected_errno,
    const char *description)
{
    int result;
    int actual_errno;

    errno = 0;
    result = ioctl(fd, request, argument);
    actual_errno = errno;

    if (result != -1) {
        test_fail(
            "%s: ioctl completata senza errore",
            description
        );
    }

    if (actual_errno != expected_errno) {
        test_fail(
            "%s: errno atteso %d, ottenuto %d (%s)",
            description,
            expected_errno,
            actual_errno,
            strerror(actual_errno)
        );
    }

    test_pass(description);
}

void test_expect_u32(
    const char *description,
    uint32_t actual,
    uint32_t expected)
{
    if (actual != expected) {
        test_fail(
            "%s: atteso %u, ottenuto %u",
            description,
            expected,
            actual
        );
    }

    test_pass(description);
}
