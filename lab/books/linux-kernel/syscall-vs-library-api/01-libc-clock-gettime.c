// @book: Linux Kernel Programming
// @topic: syscall-vs-library-api
// @source: Chapter 1. Kernel Workspace Setup（讲义）.md §3（3.1 man page 的分区约定：man 2 = 系统调用，man 3 = 库 API）
// @expect: glibc 的 clock_gettime 走 vDSO, 50 万次调用只花几十毫秒, 每次开销是纳秒级
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>

#define ITERS 500000

static long long now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
}

int main(void)
{
    struct timespec ts;
    long long t0, dt, checksum = 0;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        return 1;
    }

    t0 = now_ns();
    for (int i = 0; i < ITERS; i++) {
        if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
            perror("clock_gettime");
            return 1;
        }
        checksum += (ts.tv_sec > 0 && ts.tv_nsec >= 0) ? 1 : 0;
    }
    dt = now_ns() - t0;

    printf("路径      : glibc clock_gettime (man 3 库 API -> vDSO, 不进内核)\n");
    printf("调用次数  : %d\n", ITERS);
    printf("总耗时    : %.3f ms\n", (double)dt / 1e6);
    printf("每次开销  : %.1f ns\n", (double)dt / (double)ITERS);
    printf("有效读数  : %lld / %d\n", checksum, ITERS);
    return 0;
}
