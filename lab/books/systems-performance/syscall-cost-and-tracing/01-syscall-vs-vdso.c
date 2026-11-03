// @book: Systems Performance
// @topic: syscall-cost-and-tracing
// @source: Chapter 3. Operating Systems（讲义）.md §3.2.2
// @expect: getpid() 每次约几百 ns(真系统调用, 有模式切换), clock_gettime() 每次约几十 ns(vDSO, 不进内核); 两者差一个量级

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define ITERS 200000

static long long now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000000000LL + (long long)t.tv_nsec;
}

int main(void)
{
    struct timespec t;
    volatile int sink = 0;

    /* 预热, 避开首次调用的惰性绑定开销 */
    for (int i = 0; i < 1000; i++) {
        sink += getpid();
        clock_gettime(CLOCK_MONOTONIC, &t);
    }

    long long t0 = now_ns();
    for (int i = 0; i < ITERS; i++)
        sink += getpid();
    long long t1 = now_ns();

    long long t2 = now_ns();
    for (int i = 0; i < ITERS; i++)
        clock_gettime(CLOCK_MONOTONIC, &t);
    long long t3 = now_ns();

    /* 让 sink 的值真的被用到, 免得被优化掉 */
    if (sink == 0x7fffffff)
        printf("unreachable\n");

    double syscall_ns = (double)(t1 - t0) / ITERS;
    double vdso_ns = (double)(t3 - t2) / ITERS;

    printf("getpid()        : %.1f ns/call  (real syscall: mode switch)\n", syscall_ns);
    printf("clock_gettime() : %.1f ns/call  (vDSO: stays in user space)\n", vdso_ns);
    printf("ratio           : %.1fx\n", syscall_ns / vdso_ns);
    printf("note: a mode switch is not a context switch; the thread stays on its CPU.\n");
    return 0;
}
