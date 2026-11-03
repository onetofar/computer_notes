// @book: 操作系统导论
// @topic: cache-locality
// @source: 操作系统导论/第41章 局部性和快速文件系统（讲义）.md §41.4
// @expect: 同样的元素数, 改成 4KB 跨步访问, 缓存与 TLB 全部落空, 耗时显著变长
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>

#define N (8 * 1024 * 1024)            /* 8M 个 int = 32MB */
#define STRIDE 1024                   /* 跨步 1024 个 int = 4KB, 每次都换一页 */

static int g_a[N];

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

int main(void)
{
    long long sum = 0;
    double t0, dt;

    for (long i = 0; i < N; i++)
        g_a[i] = (int)((i * 2654435761u) >> 8);

    t0 = now_ms();
    for (long s = 0; s < STRIDE; s++) /* 1024 条流交错, 相邻两次访问相隔 4KB */
        for (long i = s; i < N; i += STRIDE)
            sum += g_a[i];
    dt = now_ms() - t0;

    printf("跨步访问: 元素数=%d 跨步=%d int 校验和=%lld 耗时=%.3f ms\n",
           N, STRIDE, sum, dt);
    return 0;
}
