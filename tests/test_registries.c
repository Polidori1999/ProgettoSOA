#include "test_common.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>

#define TEST_UID 60001U
#define TEST_PROGRAM_NAME "soa-registry"

static struct syscall_throttle_program make_program(
    const char *name)
{
    struct syscall_throttle_program program = {0};

    if (strlen(name) >= sizeof(program.name))
        test_fail("nome programma troppo lungo");

    strcpy(program.name, name);

    return program;
}

static void expect_uid(
    int fd,
    __u32 expected_count,
    __u32 expected_uid)
{
    struct syscall_throttle_uid_list list = {0};

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_GET_UIDS,
        &list,
        "lettura registro UID"
    );

    test_expect_u32(
        "numero UID registrati",
        list.count,
        expected_count
    );

    if (expected_count == 1 &&
        list.uids[0] != expected_uid) {
        test_fail(
            "UID atteso %u, ottenuto %u",
            expected_uid,
            list.uids[0]
        );
    }
}

static void expect_program(
    int fd,
    __u32 expected_count,
    const char *expected_name)
{
    struct syscall_throttle_program_list list = {0};

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_GET_PROGRAMS,
        &list,
        "lettura registro programmi"
    );

    test_expect_u32(
        "numero programmi registrati",
        list.count,
        expected_count
    );

    if (expected_count == 1 &&
        strcmp(
            list.programs[0].name,
            expected_name
        ) != 0) {
        test_fail(
            "programma atteso '%s', ottenuto '%s'",
            expected_name,
            list.programs[0].name
        );
    }
}

static void expect_syscall(
    int fd,
    __u32 expected_count,
    __u32 expected_number)
{
    struct syscall_throttle_syscall_list list = {0};

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_GET_SYSCALLS,
        &list,
        "lettura registro syscall"
    );

    test_expect_u32(
        "numero syscall registrate",
        list.count,
        expected_count
    );

    if (expected_count == 1 &&
        list.numbers[0] != expected_number) {
        test_fail(
            "syscall attesa %u, ottenuta %u",
            expected_number,
            list.numbers[0]
        );
    }
}

static void test_uid_registry(int fd)
{
    __u32 uid = TEST_UID;

    expect_uid(fd, 0, 0);

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_UID,
        &uid,
        "registrazione UID"
    );

    expect_uid(fd, 1, uid);

    test_ioctl_failure(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_UID,
        &uid,
        EEXIST,
        "UID duplicato rifiutato"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_UID,
        &uid,
        "deregistrazione UID"
    );

    test_ioctl_failure(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_UID,
        &uid,
        ENOENT,
        "UID assente rifiutato"
    );

    expect_uid(fd, 0, 0);
}

static void test_program_registry(int fd)
{
    struct syscall_throttle_program program;

    program = make_program(TEST_PROGRAM_NAME);

    expect_program(fd, 0, NULL);

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_PROGRAM,
        &program,
        "registrazione programma"
    );

    expect_program(fd, 1, TEST_PROGRAM_NAME);

    test_ioctl_failure(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_PROGRAM,
        &program,
        EEXIST,
        "programma duplicato rifiutato"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_PROGRAM,
        &program,
        "deregistrazione programma"
    );

    test_ioctl_failure(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_PROGRAM,
        &program,
        ENOENT,
        "programma assente rifiutato"
    );

    expect_program(fd, 0, NULL);
}

static void test_syscall_registry(int fd)
{
    __u32 syscall_number = (__u32)SYS_getpid;

    expect_syscall(fd, 0, 0);

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_SYSCALL,
        &syscall_number,
        "registrazione syscall"
    );

    expect_syscall(fd, 1, syscall_number);

    test_ioctl_failure(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_SYSCALL,
        &syscall_number,
        EEXIST,
        "syscall duplicata rifiutata"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_SYSCALL,
        &syscall_number,
        "deregistrazione syscall"
    );

    test_ioctl_failure(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_SYSCALL,
        &syscall_number,
        ENOENT,
        "syscall assente rifiutata"
    );

    expect_syscall(fd, 0, 0);
}

int main(void)
{
    int fd;

    fd = test_open_device();

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_DISABLE_MONITOR,
        NULL,
        "monitor disattivato"
    );

    test_uid_registry(fd);
    test_program_registry(fd);
    test_syscall_registry(fd);

    test_close_device(fd);

    test_pass("test dei registri completato");

    return EXIT_SUCCESS;
}
