#include "test_common.h"
#include "syscall_throttle_ioctl.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    __u32 monitor_enabled;
    __u32 maximum;
    int fd;

    fd = test_open_device();

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_PING,
        NULL,
        "ioctl PING"
    );

    maximum = 0;

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_GET_MAX,
        &maximum,
        "lettura di MAX"
    );

    if (maximum == 0)
        test_fail("MAX non può essere uguale a zero");

    printf("INFO: MAX corrente = %u\n", maximum);

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_DISABLE_MONITOR,
        NULL,
        "disattivazione monitor"
    );

    monitor_enabled = UINT32_MAX;

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_GET_MONITOR,
        &monitor_enabled,
        "lettura monitor disattivato"
    );

    test_expect_u32(
        "monitor effettivamente disattivato",
        monitor_enabled,
        0
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_ENABLE_MONITOR,
        NULL,
        "attivazione monitor"
    );

    monitor_enabled = 0;

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_GET_MONITOR,
        &monitor_enabled,
        "lettura monitor attivato"
    );

    test_expect_u32(
        "monitor effettivamente attivato",
        monitor_enabled,
        1
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_DISABLE_MONITOR,
        NULL,
        "ripristino monitor disattivato"
    );

    test_close_device(fd);

    test_pass("test control plane completato");

    return EXIT_SUCCESS;
}
