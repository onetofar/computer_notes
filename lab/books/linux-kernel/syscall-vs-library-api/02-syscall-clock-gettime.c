// @book: Linux Kernel Programming
// @topic: syscall-vs-library-api
// @source: Chapter 1. Kernel Workspace Setup（讲义）.md §3.1
// @expect: 用 syscall() 绕过 glibc 封装后每次都进内核, 每次开销比 01 高一个数量级以上
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

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

    if (syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts) != 0) {
        perror("syscall(SYS_clock_gettime)");
        return 1;
    }

    t0 = now_ns();
    for (int i = 0; i < ITERS; i++) {
        if (syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts) != 0) {
            perror("syscall(SYS_clock_gettime)");
            return 1;
        }
        checksum += (ts.tv_sec > 0 && ts.tv_nsec >= 0) ? 1 : 0;
    }
    dt = now_ns() - t0;

    printf("路径      : syscall(SYS_clock_gettime) (man 2 系统调用, 每次进内核)\n");
    printf("调用次数  : %d\n", ITERS);
    printf("总耗时    : %.3f ms\n", (double)dt / 1e6);
    printf("每次开销  : %.1f ns\n", (double)dt / (double)ITERS);
    printf("有效读数  : %lld / %d\n", checksum, ITERS);
    return 0;
}
