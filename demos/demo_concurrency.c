#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include "include/demo_common.h"

#define NUM_WORKERS 3

struct result {
    int worker;
    long pid;
    long wait_ms;
    long completion_ms;
};

static pthread_barrier_t ready_barrier;
static pthread_barrier_t start_barrier;
static struct timespec global_start;
static struct result results[NUM_WORKERS];

static void *worker(void *arg)
{
    int id = *(int *)arg;
    struct timespec before;
    struct timespec after;

    /*
     * Tutti i worker comunicano di essere pronti.
     */
    demo_wait_barrier(&ready_barrier);

    /*
     * Attendono il segnale di partenza comune.
     */
    demo_wait_barrier(&start_barrier);

    before = demo_now();

    long pid = syscall(SYS_getpid);

    after = demo_now();

    results[id].worker = id + 1;
    results[id].pid = pid;
    results[id].wait_ms = demo_elapsed_ms(&before, &after);
    results[id].completion_ms = demo_elapsed_ms(&global_start, &after);

    return NULL;
}

static int compare_results(const void *a, const void *b)
{
    const struct result *ra = a;
    const struct result *rb = b;

    if (ra->completion_ms < rb->completion_ms)
        return -1;
    if (ra->completion_ms > rb->completion_ms)
        return 1;

    return 0;
}

int main(void)
{
    pthread_t threads[NUM_WORKERS];
    int ids[NUM_WORKERS];

    setvbuf(stdout, NULL, _IONBF, 0);

    demo_check_pthread(pthread_barrier_init(&ready_barrier, NULL, NUM_WORKERS + 1),
                       "pthread_barrier_init ready");
    demo_check_pthread(pthread_barrier_init(&start_barrier, NULL, NUM_WORKERS + 1),
                       "pthread_barrier_init start");

    printf("Demo concurrency throttling\n");
    printf("Program: demo_conc\n");
    printf("Workers: %d\n", NUM_WORKERS);
    printf("Syscall: getpid\n\n");

    for (int i = 0; i < NUM_WORKERS; i++) {
        ids[i] = i;

        demo_check_pthread(pthread_create(&threads[i], NULL, worker, &ids[i]),
                           "pthread_create");
    }

    /*
     * Aspettiamo che tutti i worker siano pronti.
     */
    demo_wait_barrier(&ready_barrier);

    global_start = demo_now();

    /*
     * Partenza contemporanea.
     */
    demo_wait_barrier(&start_barrier);

    for (int i = 0; i < NUM_WORKERS; i++)
        demo_check_pthread(pthread_join(threads[i], NULL), "pthread_join");

    qsort(results,
          NUM_WORKERS,
          sizeof(results[0]),
          compare_results);

    printf("Ordine di completamento:\n");

    for (int i = 0; i < NUM_WORKERS; i++) {
        printf("  Worker %d -> pid=%ld | completato a +%ld ms | attesa=%ld ms\n",
               results[i].worker,
               results[i].pid,
               results[i].completion_ms,
               results[i].wait_ms);
    }

    demo_check_pthread(pthread_barrier_destroy(&ready_barrier),
                       "pthread_barrier_destroy ready");
    demo_check_pthread(pthread_barrier_destroy(&start_barrier),
                       "pthread_barrier_destroy start");

    return 0;
}