// @book: 操作系统导论
// @topic: concurrency-anomalies
// @source: 操作系统导论/第32章 常见并发问题（讲义）.md §32.3
// @expect: 两个线程用相反顺序抢两把锁, 限时等待双双超时, 证明循环等待成立
// @ub: yes
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static pthread_mutex_t g_l1 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_l2 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_gate_cv = PTHREAD_COND_INITIALIZER;
static int g_ready;                                   /* 都拿到第一把锁的线程数 */

static void deadline_in(struct timespec *ts, long ms)
{
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += ms / 1000;
    ts->tv_nsec += (ms % 1000) * 1000000L;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000L;
    }
}

/* 两个线程各拿一把锁, 再抢对方那把 —— 顺序相反, 环就闭上了 */
static void *worker(void *arg)
{
    long id = (long)(intptr_t)arg;
    pthread_mutex_t *first = (id == 0) ? &g_l1 : &g_l2;
    pthread_mutex_t *second = (id == 0) ? &g_l2 : &g_l1;
    struct timespec ts;
    int rc;

    pthread_mutex_lock(first);
    pthread_mutex_lock(&g_gate);
    g_ready++;
    pthread_cond_broadcast(&g_gate_cv);
    while (g_ready < 2)                              /* 等对方也拿到第一把锁 */
        pthread_cond_wait(&g_gate_cv, &g_gate);
    pthread_mutex_unlock(&g_gate);

    deadline_in(&ts, 300);                            /* 不无限期等, 免得挂死 */
    rc = pthread_mutex_timedlock(second, &ts);
    printf("[thread %ld] 抢第二把锁 rc=%d%s\n", id, rc,
           rc == ETIMEDOUT ? " (ETIMEDOUT: 对方永远不会放手)" : " (拿到了)");
    pthread_mutex_unlock(first);
    return NULL;
}

int main(void)
{
    pthread_t th[2];

    pthread_create(&th[0], NULL, worker, (void *)(intptr_t)0);
    pthread_create(&th[1], NULL, worker, (void *)(intptr_t)1);
    pthread_join(th[0], NULL);
    pthread_join(th[1], NULL);

    printf("runtime error: 互斥+持有并等待+非抢占+循环等待 四条件同时成立, 死锁成立\n");
    return 1;
}
