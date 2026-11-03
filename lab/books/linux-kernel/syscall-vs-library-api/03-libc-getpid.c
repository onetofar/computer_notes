// @book: Linux Kernel Programming
// @topic: syscall-vs-library-api
// @source: Chapter 1. Kernel Workspace Setup（讲义）.md §3（3.1 man page 的分区约定：man 2 = 系统调用，man 3 = 库 API）
// @expect: getpid 没有 vDSO 快路径, 虽是 man 3 库 API 却每次都要进内核, 开销与 02 同量级
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <sys/types.h>
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
    pid_t pid = 0;
    long long t0, dt, checksum = 0;

    t0 = now_ns();
    for (int i = 0; i < ITERS; i++) {
        pid = getpid();
        checksum += (pid > 0) ? 1 : 0;
    }
    dt = now_ns() - t0;

    printf("路径      : glibc getpid (man 3 库 API, 但无 vDSO 快路径, 转发进内核)\n");
    printf("调用次数  : %d\n", ITERS);
    printf("总耗时    : %.3f ms\n", (double)dt / 1e6);
    printf("每次开销  : %.1f ns\n", (double)dt / (double)ITERS);
    printf("有效读数  : %lld / %d  (pid=%d)\n", checksum, ITERS, (int)pid);
    return 0;
}
