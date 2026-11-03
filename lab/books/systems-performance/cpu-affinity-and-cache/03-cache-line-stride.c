// @book: Systems Performance
// @topic: cpu-affinity-and-cache
// @source: Chapter 6. CPUs（讲义）.md §6.4.1 Cache Line
// @expect: 步长 64B 时预取器跟得上, 带宽约 300MB/s; 步长 4096B 时每次访问跨新页(TLB miss)且预取失效, 带宽掉到约 70MB/s

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SIZE (64 * 1024 * 1024)

static long long now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000000000LL + (long long)t.tv_nsec;
}

static void scan(const volatile char *buf, long stride)
{
    long acc = 0;
    for (long i = 0; i < SIZE; i += stride)
        acc += buf[i];
    if (acc == -1)
        printf("unreachable %ld\n", acc);
}

static void run(const volatile char *buf, long stride)
{
    long long t0 = now_ns();
    scan(buf, stride);
    long long t1 = now_ns();

    long accesses = SIZE / stride;
    printf("stride_bytes    : %ld\n", stride);
    printf("accesses        : %ld\n", accesses);
    printf("elapsed_ns      : %lld\n", t1 - t0);
    printf("ns_per_access   : %.2f\n", (double)(t1 - t0) / (double)accesses);
    printf("throughput_MBs  : %.1f\n",
           (double)accesses / ((double)(t1 - t0) / 1e9) / 1e6);
}

int main(void)
{
    char *raw = malloc(SIZE);
    if (!raw)
        return 1;
    for (long i = 0; i < SIZE; i += 4096)
        raw[i] = (char)i;

    volatile char *buf = raw;
    run(buf, 64);
    printf("\n");
    run(buf, 4096);
    printf("\nnote: a cache line is 64 bytes. A 64B stride streams well; a 4096B stride\n");
    printf("      lands on a new page every access (TLB miss) and defeats the prefetcher.\n");
    free(raw);
    return 0;
}
