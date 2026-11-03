// @book: Systems Performance
// @topic: memory-and-network-latency
// @source: Chapter 7. Memory（讲义）.md §7.3.1
// @expect: 同一个 64MB 数组, 顺序访问每次约 1ns(带宽极高), 随机访问每次约 80ns; 差别就是 cache 与预取

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N (64 * 1024 * 1024 / (long)sizeof(long))
#define RANDOM_ACCESSES (8 * 1024 * 1024)

static long long now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000000000LL + (long long)t.tv_nsec;
}

static unsigned long long rng_state = 88172645463325252ULL;

static unsigned long long xorshift(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

int main(void)
{
    long *a = malloc(sizeof(long) * (size_t)N);
    if (!a)
        return 1;
    for (long i = 0; i < N; i++)
        a[i] = i;

    long long t0 = now_ns();
    long seq_sum = 0;
    for (long i = 0; i < N; i++)
        seq_sum += a[i];
    long long t1 = now_ns();

    long long t2 = now_ns();
    long rnd_sum = 0;
    for (long i = 0; i < RANDOM_ACCESSES; i++)
        rnd_sum += a[xorshift() % (unsigned long long)N];
    long long t3 = now_ns();

    printf("--- sequential ---\n");
    printf("accesses        : %ld\n", N);
    printf("elapsed_ns      : %lld\n", t1 - t0);
    printf("ns_per_access   : %.2f\n", (double)(t1 - t0) / (double)N);
    printf("bandwidth_GBs   : %.2f\n",
           (double)(N * (long)sizeof(long)) / ((double)(t1 - t0) / 1e9) / 1e9);

    printf("\n--- random ---\n");
    printf("accesses        : %d\n", RANDOM_ACCESSES);
    printf("elapsed_ns      : %lld\n", t3 - t2);
    printf("ns_per_access   : %.2f\n", (double)(t3 - t2) / (double)RANDOM_ACCESSES);
    printf("bandwidth_GBs   : %.2f\n",
           (double)(RANDOM_ACCESSES * (long)sizeof(long)) / ((double)(t3 - t2) / 1e9) / 1e9);

    printf("\nchecksum        : %ld %ld\n", seq_sum, rnd_sum);
    free(a);
    return 0;
}
