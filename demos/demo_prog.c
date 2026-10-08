#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <time.h>

#define COMM_LEN 16

static long elapsed_ms(const struct timespec *start,
                       const struct timespec *end)
{
    return (end->tv_sec - start->tv_sec) * 1000L +
           (end->tv_nsec - start->tv_nsec) / 1000000L;
}

int main(void)
{
    struct timespec start;
    char comm[COMM_LEN] = "unknown";

    setvbuf(stdout, NULL, _IONBF, 0);

    if (prctl(PR_GET_NAME, comm, 0, 0, 0) != 0) {
        perror("prctl");
        return 1;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("clock_gettime");
        return 1;
    }

    printf("Demo sequential getpid throttling\n");
    printf("Task comm: %s\n", comm);
    printf("Syscall: getpid\n\n");

    for (int i = 1; i <= 4; i++) {
        struct timespec before;
        struct timespec after;

        clock_gettime(CLOCK_MONOTONIC, &before);

        long pid = syscall(SYS_getpid);

        clock_gettime(CLOCK_MONOTONIC, &after);

        printf("call %d -> pid=%ld | completata a +%ld ms | attesa=%ld ms\n",
               i,
               pid,
               elapsed_ms(&start, &after),
               elapsed_ms(&before, &after));
    }

    return 0;
}