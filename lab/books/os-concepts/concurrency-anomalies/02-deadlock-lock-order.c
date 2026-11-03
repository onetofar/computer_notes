// @book: 操作系统导论
// @topic: concurrency-anomalies
// @source: 操作系统导论/第32章 常见并发问题（讲义）.md §32.3
// @expect: 同样两把两线程, 改成全局锁序(按地址排序)后两个线程都跑完, 不再死锁
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static pthread_mutex_t g_l1 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_l2 = PTHREAD_MUTEX_INITIALIZER;
static int g_done;

/* 按地址排序: 无论调用方以什么顺序传进来, 内部都按同一顺序加锁 */
static void lock_pair(pthread_mutex_t *a, pthread_mutex_t *b)
{
    if ((uintptr_t)a < (uintptr_t)b) {
        pthread_mutex_lock(a);
        pthread_mutex_lock(b);
    } else {
        pthread_mutex_lock(b);
        pthread_mutex_lock(a);
    }
}

static void unlock_pair(pthread_mutex_t *a, pthread_mutex_t *b)
{
    if ((uintptr_t)a < (uintptr_t)b) {
        pthread_mutex_unlock(b);
        pthread_mutex_unlock(a);
    } else {
        pthread_mutex_unlock(a);
        pthread_mutex_unlock(b);
    }
}

static void *worker(void *arg)
{
    long id = (long)(intptr_t)arg;
    struct timespec ts = {0, 50 * 1000 * 1000L};

    lock_pair(&g_l1, &g_l2);                          /* 顺序由地址决定 */
    printf("[thread %ld] 同时拿到两把锁, 进入临界区\n", id);
    nanosleep(&ts, NULL);
    unlock_pair(&g_l1, &g_l2);
    g_done++;
    return NULL;
}

int main(void)
{
    pthread_t th[2];

    pthread_create(&th[0], NULL, worker, (void *)(intptr_t)0);
    pthread_create(&th[1], NULL, worker, (void *)(intptr_t)1);
    pthread_join(th[0], NULL);
    pthread_join(th[1], NULL);

    printf("全局锁序: 完成线程数=%d (期望 2), 没有死锁\n", g_done);
    return g_done == 2 ? 0 : 1;
}
