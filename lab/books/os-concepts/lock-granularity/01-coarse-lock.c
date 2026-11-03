// @book: 操作系统导论
// @topic: lock-granularity
// @source: 操作系统导论/第29章 基于锁的并发数据结构（讲义）.md §29.1
// @expect: 4 线程各加 20 万次, 最终值=800000, 但一把大锁把 4 个线程压成串行
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static long g_value = 0;                             /* 全体线程抢同一把锁 */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static void *bump(void *arg)
{
    long n = (long)(intptr_t)arg;
    for (long i = 0; i < n; i++) {
        pthread_mutex_lock(&g_lock);                   /* 临界区只有一行 */
        g_value++;
        pthread_mutex_unlock(&g_lock);
    }
    return NULL;
}

int main(void)
{
    enum { N = 4 };
    const long kIters = 200000;
    const long kExpect = (long)N * kIters;
    pthread_t th[N];
    double t0, dt;

    t0 = now_ms();
    for (int i = 0; i < N; i++)
        pthread_create(&th[i], NULL, bump, (void *)(intptr_t)kIters);
    for (int i = 0; i < N; i++)
        pthread_join(th[i], NULL);
    dt = now_ms() - t0;

    printf("一把大锁: value=%ld 期望=%ld 耗时=%.3f ms 吞吐=%.0f ops/s\n",
           g_value, kExpect, dt, (double)kExpect / (dt / 1000.0));
    return 0;
}
