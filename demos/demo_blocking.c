#define _GNU_SOURCE

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "syscall_throttle_ioctl.h"

#define READER_NAME "soa-read-demo"
#define WRITER_NAME "soa-writer"

static int pipe_fds[2];
static int device_fd;
static pthread_barrier_t start_barrier;
static uint64_t start_ns;

static void fail(const char *operation)
{
    perror(operation);
    exit(EXIT_FAILURE);
}

static void check_pthread(int result, const char *operation)
{
    if (result != 0) {
        fprintf(stderr, "%s: %s\n", operation, strerror(result));
        exit(EXIT_FAILURE);
    }
}

static void wait_start(void)
{
    int result = pthread_barrier_wait(&start_barrier);

    if (result != PTHREAD_BARRIER_SERIAL_THREAD)
        check_pthread(result, "pthread_barrier_wait");
}

static uint64_t now_ns(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        fail("clock_gettime");

    return (uint64_t)now.tv_sec * UINT64_C(1000000000) +
           (uint64_t)now.tv_nsec;
}

static double elapsed_ms(uint64_t timestamp)
{
    return (double)(timestamp - start_ns) / 1000000.0;
}

static void sleep_ms(unsigned int milliseconds)
{
    struct timespec remaining = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (long)(milliseconds % 1000) * 1000000L
    };

    while (nanosleep(&remaining, &remaining) != 0) {
        if (errno != EINTR)
            fail("nanosleep");
    }
}

static void print_waiters(const char *description)
{
    struct syscall_throttle_statistics stats = {0};

    if (ioctl(device_fd, SYSCALL_THROTTLE_IOC_GET_STATS, &stats) != 0)
        fail("GET_STATS");

    printf("  +%.3f ms | %s | waiter del monitor=%u\n",
           elapsed_ms(now_ns()), description,
           stats.current_blocked_threads);
}

static void *writer_main(void *argument)
{
    ssize_t written;

    (void)argument;

    /* Il writer deve poter fornire dati senza essere selezionato. */
    if (prctl(PR_SET_NAME, WRITER_NAME, 0, 0, 0) != 0)
        fail("PR_SET_NAME writer");

    wait_start();
    sleep_ms(200);

    print_waiters("prima di fornire dati alla pipe");

    do {
        written = write(pipe_fds[1], "AB", 2);
    } while (written < 0 && errno == EINTR);

    if (written != 2) {
        if (written >= 0)
            errno = EIO;
        fail("write pipe");
    }

    sleep_ms(200);
    print_waiters("dopo aver fornito entrambi i byte");
    return NULL;
}

static void read_one_byte(unsigned int number)
{
    char byte;
    uint64_t before = now_ns();
    uint64_t after;
    long result;

    /* read bloccante reale, con richiesta esplicita di un byte. */
    result = syscall(SYS_read, pipe_fds[0], &byte, 1);
    after = now_ns();

    if (result != 1) {
        if (result >= 0)
            errno = EIO;
        fail("read pipe");
    }

    printf("  read %u -> '%c' | completata a +%.3f ms | durata=%.3f ms\n",
           number, byte, elapsed_ms(after),
           (double)(after - before) / 1000000.0);
}

int main(void)
{
    pthread_t writer;
    int available;
    __u32 monitor;

    setvbuf(stdout, NULL, _IONBF, 0);

    if (pipe(pipe_fds) != 0)
        fail("pipe");

    device_fd = open("/dev/syscall_throttle", O_RDWR);
    if (device_fd < 0)
        fail("open /dev/syscall_throttle");

    if (ioctl(device_fd, SYSCALL_THROTTLE_IOC_GET_MONITOR, &monitor) != 0)
        fail("GET_MONITOR");

    check_pthread(pthread_barrier_init(&start_barrier, NULL, 2),
                  "pthread_barrier_init");
    check_pthread(pthread_create(&writer, NULL, writer_main, NULL),
                  "pthread_create");

    /*
     * Seleziona il reader solo dopo l'inizializzazione: le read
     * del loader non devono consumare il budget della demo.
     */
    if (prctl(PR_SET_NAME, READER_NAME, 0, 0, 0) != 0)
        fail("PR_SET_NAME reader");

    printf("Monitor: %s | reader: %s | syscall read: %ld\n",
           monitor ? "ON" : "OFF", READER_NAME, (long)SYS_read);
    printf("Il writer fornisce due byte dopo circa 200 ms.\n");

    start_ns = now_ns();
    wait_start();

    read_one_byte(1);

    if (ioctl(pipe_fds[0], FIONREAD, &available) != 0)
        fail("FIONREAD");

    printf("  Prima della seconda read: byte gia' nella pipe=%d\n",
           available);
    read_one_byte(2);

    check_pthread(pthread_join(writer, NULL), "pthread_join");
    check_pthread(pthread_barrier_destroy(&start_barrier),
                  "pthread_barrier_destroy");

    if (close(pipe_fds[0]) != 0 || close(pipe_fds[1]) != 0 ||
        close(device_fd) != 0)
        fail("close");

    return EXIT_SUCCESS;
}
