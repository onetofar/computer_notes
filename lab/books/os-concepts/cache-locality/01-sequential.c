// @book: 操作系统导论
// @topic: cache-locality
// @source: 操作系统导论/第41章 局部性和快速文件系统（讲义）.md §41.4
// @expect: 顺序扫过 32MB 数组, 打印校验和与耗时, 作为跨步访问的对照基线
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>

#define N (8 * 1024 * 1024)            /* 8M 个 int = 32MB, 远超 L3 */

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
    for (long i = 0; i < N; i++)      /* 顺序: 每次访问都落在上一条的下一条缓存行 */
        sum += g_a[i];
    dt = now_ms() - t0;

    printf("顺序访问: 元素数=%d 校验和=%lld 耗时=%.3f ms\n", N, sum, dt);
    return 0;
}
