#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

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

static long elapsed_ms(const struct timespec *start,
                       const struct timespec *end)
{
    return (end->tv_sec - start->tv_sec) * 1000L +
           (end->tv_nsec - start->tv_nsec) / 1000000L;
}

static void *worker(void *arg)
{
    int id = *(int *)arg;
    struct timespec before;
    struct timespec after;

    /*
     * Tutti i worker comunicano di essere pronti.
     */
    pthread_barrier_wait(&ready_barrier);

    /*
     * Attendono il segnale di partenza comune.
     */
    pthread_barrier_wait(&start_barrier);

    clock_gettime(CLOCK_MONOTONIC, &before);

    long pid = syscall(SYS_getpid);

    clock_gettime(CLOCK_MONOTONIC, &after);

    results[id].worker = id + 1;
    results[id].pid = pid;
    results[id].wait_ms = elapsed_ms(&before, &after);
    results[id].completion_ms = elapsed_ms(&global_start, &after);

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

    if (pthread_barrier_init(&ready_barrier, NULL, NUM_WORKERS + 1) != 0 ||
        pthread_barrier_init(&start_barrier, NULL, NUM_WORKERS + 1) != 0) {
        perror("pthread_barrier_init");
        return 1;
    }

    printf("Demo concurrency throttling\n");
    printf("Program: demo_conc\n");
    printf("Workers: %d\n", NUM_WORKERS);
    printf("Syscall: getpid\n\n");

    for (int i = 0; i < NUM_WORKERS; i++) {
        ids[i] = i;

        if (pthread_create(&threads[i], NULL, worker, &ids[i]) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    /*
     * Aspettiamo che tutti i worker siano pronti.
     */
    pthread_barrier_wait(&ready_barrier);

    clock_gettime(CLOCK_MONOTONIC, &global_start);

    /*
     * Partenza contemporanea.
     */
    pthread_barrier_wait(&start_barrier);

    for (int i = 0; i < NUM_WORKERS; i++)
        pthread_join(threads[i], NULL);

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

    pthread_barrier_destroy(&ready_barrier);
    pthread_barrier_destroy(&start_barrier);

    return 0;
}