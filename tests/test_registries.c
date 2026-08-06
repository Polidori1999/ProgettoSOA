#include "test_common.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>

#define TEST_UID 60001U

static void expect_uid(int fd, __u32 count, __u32 value)
{
    struct syscall_throttle_uid_list list = {0};

    test_ioctl_success(fd, SYSCALL_THROTTLE_IOC_GET_UIDS,
                       &list, "lettura registro UID");

    if (list.count != count ||
        (count == 1 && list.uids[0] != value))
        test_fail("contenuto registro UID errato");
}

static void expect_program(int fd, __u32 count, const char *name)
{
    struct syscall_throttle_program_list list = {0};

    test_ioctl_success(fd, SYSCALL_THROTTLE_IOC_GET_PROGRAMS,
                       &list, "lettura registro programmi");

    if (list.count != count ||
        (count == 1 &&
         strcmp(list.programs[0].name, name) != 0))
        test_fail("contenuto registro programmi errato");
}

static void expect_syscall(int fd, __u32 count, __u32 number)
{
    struct syscall_throttle_syscall_list list = {0};

    test_ioctl_success(fd, SYSCALL_THROTTLE_IOC_GET_SYSCALLS,
                       &list, "lettura registro syscall");

    if (list.count != count ||
        (count == 1 && list.numbers[0] != number))
        test_fail("contenuto registro syscall errato");
}

int main(void)
{
    struct syscall_throttle_program program = {
        .name = "soa-registry"
    };
    __u32 syscall_number = (__u32)SYS_getpid;
    __u32 uid = TEST_UID;
    int fd = test_open_device();

    test_ioctl_success(fd, SYSCALL_THROTTLE_IOC_DISABLE_MONITOR,
                       NULL, "monitor disattivato");

    expect_uid(fd, 0, 0);
    test_ioctl_success(fd, SYSCALL_THROTTLE_IOC_REGISTER_UID,
                       &uid, "registrazione UID");
    expect_uid(fd, 1, uid);
    test_ioctl_failure(fd, SYSCALL_THROTTLE_IOC_REGISTER_UID,
                       &uid, EEXIST, "UID duplicato rifiutato");
    test_ioctl_success(fd, SYSCALL_THROTTLE_IOC_UNREGISTER_UID,
                       &uid, "deregistrazione UID");
    test_ioctl_failure(fd, SYSCALL_THROTTLE_IOC_UNREGISTER_UID,
                       &uid, ENOENT, "UID assente rifiutato");
    expect_uid(fd, 0, 0);
    test_pass("registro UID verificato");

    expect_program(fd, 0, NULL);
    test_ioctl_success(fd, SYSCALL_THROTTLE_IOC_REGISTER_PROGRAM,
                       &program, "registrazione programma");
    expect_program(fd, 1, program.name);
    test_ioctl_failure(fd, SYSCALL_THROTTLE_IOC_REGISTER_PROGRAM,
                       &program, EEXIST,
                       "programma duplicato rifiutato");
    test_ioctl_success(fd, SYSCALL_THROTTLE_IOC_UNREGISTER_PROGRAM,
                       &program, "deregistrazione programma");
    test_ioctl_failure(fd, SYSCALL_THROTTLE_IOC_UNREGISTER_PROGRAM,
                       &program, ENOENT,
                       "programma assente rifiutato");
    expect_program(fd, 0, NULL);
    test_pass("registro programmi verificato");

    expect_syscall(fd, 0, 0);
    test_ioctl_success(fd, SYSCALL_THROTTLE_IOC_REGISTER_SYSCALL,
                       &syscall_number, "registrazione syscall");
    expect_syscall(fd, 1, syscall_number);
    test_ioctl_failure(fd, SYSCALL_THROTTLE_IOC_REGISTER_SYSCALL,
                       &syscall_number, EEXIST,
                       "syscall duplicata rifiutata");
    test_ioctl_success(fd, SYSCALL_THROTTLE_IOC_UNREGISTER_SYSCALL,
                       &syscall_number, "deregistrazione syscall");
    test_ioctl_failure(fd, SYSCALL_THROTTLE_IOC_UNREGISTER_SYSCALL,
                       &syscall_number, ENOENT,
                       "syscall assente rifiutata");
    expect_syscall(fd, 0, 0);
    test_pass("registro syscall verificato");

    test_close_device(fd);
    test_pass("test dei registri completato");

    return EXIT_SUCCESS;
}
