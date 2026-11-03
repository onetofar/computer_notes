// @book: 操作系统导论
// @topic: lock-granularity
// @source: 操作系统导论/第29章 基于锁的并发数据结构（讲义）.md §29.1
// @expect: 同样 4 线程 20 万次, 最终值同样是 800000, 但吞吐高于一把大锁
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

enum { N = 4 };
#define S 1024              /* 懒惰阈值: 局部攒够 S 才转账给全局 */

typedef struct {
    long local;            /* 本地计数器: 只有本线程写 */
    pthread_mutex_t lock;  /* 本地锁: 几乎不发生争用 */
} shard_t;

static shard_t g_shards[N];
static long g_global = 0;                            /* 全局计数器 */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static long g_transfers = 0;                         /* 转账次数 */

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static void *bump(void *arg)
{
    long idx = (long)(intptr_t)arg;
    shard_t *sh = &g_shards[idx];

    for (long i = 0; i < 200000; i++) {
        pthread_mutex_lock(&sh->lock);
        sh->local++;
        long v = sh->local;
        pthread_mutex_unlock(&sh->lock);
        if (v >= S) {                                  /* 攒够阈值才转账 */
            pthread_mutex_lock(&sh->lock);
            long give = sh->local;
            sh->local = 0;
            pthread_mutex_unlock(&sh->lock);
            pthread_mutex_lock(&g_lock);
            g_global += give;
            g_transfers++;
            pthread_mutex_unlock(&g_lock);
        }
    }
    /* 收尾: 把没攒够 S 的零头也转进全局, 保证最终值精确 */
    pthread_mutex_lock(&sh->lock);
    long give = sh->local;
    sh->local = 0;
    pthread_mutex_unlock(&sh->lock);
    pthread_mutex_lock(&g_lock);
    g_global += give;
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

int main(void)
{
    pthread_t th[N];
    double t0, dt;
    const long kExpect = (long)N * 200000;

    for (int i = 0; i < N; i++)
        pthread_mutex_init(&g_shards[i].lock, NULL);

    t0 = now_ms();
    for (int i = 0; i < N; i++)
        pthread_create(&th[i], NULL, bump, (void *)(intptr_t)i);
    for (int i = 0; i < N; i++)
        pthread_join(th[i], NULL);
    dt = now_ms() - t0;

    printf("分片+懒惰转账(S=%d): value=%ld 期望=%ld 耗时=%.3f ms 吞吐=%.0f ops/s 转账次数=%ld\n",
           S, g_global, kExpect, dt, (double)kExpect / (dt / 1000.0), g_transfers);
    for (int i = 0; i < N; i++)
        pthread_mutex_destroy(&g_shards[i].lock);
    return 0;
}
