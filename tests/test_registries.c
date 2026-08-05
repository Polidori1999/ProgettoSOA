#include "test_common.h"
#include "syscall_throttle_ioctl.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static void expect_uid_list(
    int fd,
    const __u32 *expected,
    __u32 expected_count)
{
    struct syscall_throttle_uid_list list = {0};
    __u32 index;

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

    for (index = 0; index < expected_count; ++index) {
        if (list.uids[index] != expected[index]) {
            test_fail(
                "UID[%u]: atteso %u, ottenuto %u",
                index,
                expected[index],
                list.uids[index]
            );
        }
    }

    test_pass("contenuto registro UID corretto");
}

static struct syscall_throttle_program make_program(
    const char *name)
{
    struct syscall_throttle_program program = {0};
    size_t length;

    length = strlen(name);

    if (length >= sizeof(program.name))
        test_fail("nome programma troppo lungo: %s", name);

    memcpy(program.name, name, length + 1);

    return program;
}

static void expect_program_list(
    int fd,
    const char *const *expected,
    __u32 expected_count)
{
    struct syscall_throttle_program_list list = {0};
    __u32 index;

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

    for (index = 0; index < expected_count; ++index) {
        if (strcmp(list.programs[index].name,
                   expected[index]) != 0) {
            test_fail(
                "programma[%u]: atteso '%s', ottenuto '%s'",
                index,
                expected[index],
                list.programs[index].name
            );
        }
    }

    test_pass("contenuto registro programmi corretto");
}

static void expect_syscall_list(
    int fd,
    const __u32 *expected,
    __u32 expected_count)
{
    struct syscall_throttle_syscall_list list = {0};
    __u32 index;

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

    for (index = 0; index < expected_count; ++index) {
        if (list.numbers[index] != expected[index]) {
            test_fail(
                "syscall[%u]: attesa %u, ottenuta %u",
                index,
                expected[index],
                list.numbers[index]
            );
        }
    }

    test_pass("contenuto registro syscall corretto");
}

static void test_uid_registry(int fd)
{
    __u32 uid_a = 60001;
    __u32 uid_b = 60002;
    __u32 uid_c = 60003;

    const __u32 complete[] = {
        60001, 60002, 60003
    };

    const __u32 after_middle_removal[] = {
        60001, 60003
    };

    expect_uid_list(fd, NULL, 0);

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_UID,
        &uid_a,
        "registrazione UID A"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_UID,
        &uid_b,
        "registrazione UID B"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_UID,
        &uid_c,
        "registrazione UID C"
    );

    expect_uid_list(fd, complete, 3);

    test_ioctl_failure(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_UID,
        &uid_b,
        EEXIST,
        "UID duplicato rifiutato"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_UID,
        &uid_b,
        "rimozione UID centrale"
    );

    expect_uid_list(fd, after_middle_removal, 2);

    test_ioctl_failure(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_UID,
        &uid_b,
        ENOENT,
        "UID assente rifiutato"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_UID,
        &uid_a,
        "pulizia UID A"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_UID,
        &uid_c,
        "pulizia UID C"
    );

    expect_uid_list(fd, NULL, 0);
}

static void test_program_registry(int fd)
{
    struct syscall_throttle_program program_a =
        make_program("soa-a");
    struct syscall_throttle_program program_b =
        make_program("soa-b");
    struct syscall_throttle_program program_c =
        make_program("soa-c");

    const char *complete[] = {
        "soa-a", "soa-b", "soa-c"
    };

    const char *after_middle_removal[] = {
        "soa-a", "soa-c"
    };

    expect_program_list(fd, NULL, 0);

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_PROGRAM,
        &program_a,
        "registrazione programma A"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_PROGRAM,
        &program_b,
        "registrazione programma B"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_PROGRAM,
        &program_c,
        "registrazione programma C"
    );

    expect_program_list(fd, complete, 3);

    test_ioctl_failure(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_PROGRAM,
        &program_b,
        EEXIST,
        "programma duplicato rifiutato"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_PROGRAM,
        &program_b,
        "rimozione programma centrale"
    );

    expect_program_list(fd, after_middle_removal, 2);

    test_ioctl_failure(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_PROGRAM,
        &program_b,
        ENOENT,
        "programma assente rifiutato"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_PROGRAM,
        &program_a,
        "pulizia programma A"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_PROGRAM,
        &program_c,
        "pulizia programma C"
    );

    expect_program_list(fd, NULL, 0);
}

static void test_syscall_registry(int fd)
{
    __u32 syscall_a = 39;
    __u32 syscall_b = 110;
    __u32 syscall_c = 186;

    const __u32 complete[] = {
        39, 110, 186
    };

    const __u32 after_middle_removal[] = {
        39, 186
    };

    expect_syscall_list(fd, NULL, 0);

    /*
     * L'ordine di registrazione è intenzionalmente diverso:
     * la bitmap deve restituire numeri ordinati.
     */
    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_SYSCALL,
        &syscall_c,
        "registrazione syscall C"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_SYSCALL,
        &syscall_a,
        "registrazione syscall A"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_SYSCALL,
        &syscall_b,
        "registrazione syscall B"
    );

    expect_syscall_list(fd, complete, 3);

    test_ioctl_failure(
        fd,
        SYSCALL_THROTTLE_IOC_REGISTER_SYSCALL,
        &syscall_b,
        EEXIST,
        "syscall duplicata rifiutata"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_SYSCALL,
        &syscall_b,
        "deregistrazione syscall centrale"
    );

    expect_syscall_list(fd, after_middle_removal, 2);

    test_ioctl_failure(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_SYSCALL,
        &syscall_b,
        ENOENT,
        "syscall assente rifiutata"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_SYSCALL,
        &syscall_a,
        "pulizia syscall A"
    );

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_UNREGISTER_SYSCALL,
        &syscall_c,
        "pulizia syscall C"
    );

    expect_syscall_list(fd, NULL, 0);
}

int main(void)
{
    int fd;

    fd = test_open_device();

    test_ioctl_success(
        fd,
        SYSCALL_THROTTLE_IOC_DISABLE_MONITOR,
        NULL,
        "monitor disattivato durante il test"
    );

    test_uid_registry(fd);
    test_program_registry(fd);
    test_syscall_registry(fd);

    test_close_device(fd);

    test_pass("test dei registri completato");

    return EXIT_SUCCESS;
}
