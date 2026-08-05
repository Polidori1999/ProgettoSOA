#ifndef SYSCALL_THROTTLE_TEST_COMMON_H
#define SYSCALL_THROTTLE_TEST_COMMON_H

#include <stdint.h>

#define TEST_DEVICE_PATH "/dev/syscall_throttle"

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

#endif
