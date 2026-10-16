#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <time.h>

#include "include/demo_common.h"

#define COMM_LEN 16

int main(void)
{
    struct timespec start;
    char comm[COMM_LEN] = "unknown";

    setvbuf(stdout, NULL, _IONBF, 0);

    if (prctl(PR_GET_NAME, comm, 0, 0, 0) != 0) {
        perror("prctl");
        return 1;
    }

    start = demo_now();

    printf("Demo sequential getpid throttling\n");
    printf("Task comm: %s\n", comm);
    printf("Syscall: getpid\n\n");

    for (int i = 1; i <= 4; i++) {
        struct timespec before;
        struct timespec after;

        before = demo_now();

        long pid = syscall(SYS_getpid);

        after = demo_now();

        printf("call %d -> pid=%ld | completata a +%ld ms | attesa=%ld ms\n",
               i,
               pid,
               demo_elapsed_ms(&start, &after),
               demo_elapsed_ms(&before, &after));
    }

    return 0;
}