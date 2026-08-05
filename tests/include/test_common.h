#ifndef SYSCALL_THROTTLE_TEST_COMMON_H
#define SYSCALL_THROTTLE_TEST_COMMON_H

#include "syscall_throttle_ioctl.h"

#include <stdint.h>

#define TEST_DEVICE_PATH "/dev/syscall_throttle"

struct test_fixture {
    int fd;
    __u32 original_max;
    __u32 syscall_number;
    struct syscall_throttle_program program;
};

int test_open_device(void);
void test_close_device(int fd);

void test_pass(const char *description);

_Noreturn void test_fail(
    const char *format,
    ...
);

void test_ioctl_success(
    int fd,
    unsigned long request,
    void *argument,
    const char *description
);

void test_ioctl_failure(
    int fd,
    unsigned long request,
    void *argument,
    int expected_errno,
    const char *description
);

void test_expect_u32(
    const char *description,
    uint32_t actual,
    uint32_t expected
);


uint64_t test_monotonic_time_ns(void);
void test_set_thread_name(const char *name);
long test_invoke_syscall(long syscall_number);

void test_fixture_setup(
    struct test_fixture *fixture,
    const char *program_name,
    __u32 maximum,
    __u32 syscall_number
);

void test_fixture_enable_monitor(
    struct test_fixture *fixture
);

void test_fixture_disable_monitor(
    struct test_fixture *fixture
);

void test_fixture_cleanup(
    struct test_fixture *fixture
);

#endif
