// @book: 操作系统导论
// @topic: semaphore-binary-vs-counting
// @source: 操作系统导论/第31章 信号量（讲义）.md §31.2
// @expect: 初值 1 的信号量当锁用: 4 个线程里临界区同时最多只有 1 个
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static sem_t g_sem;
static atomic_int g_inside;                          /* 临界区里的线程数 */
static atomic_int g_max_inside;                      /* 观察到的最大值 */

static void *worker(void *arg)
{
    (void)arg;
    sem_wait(&g_sem);                                 /* 初值 1: 等一个许可 */
    int cur = atomic_fetch_add(&g_inside, 1) + 1;
    int seen = atomic_load(&g_max_inside);
    while (cur > seen && !atomic_compare_exchange_weak(&g_max_inside, &seen, cur)) {
        /* CAS 失败就重试 */
    }
    struct timespec ts = {0, 30 * 1000 * 1000L};      /* 临界区里待 30ms */
    nanosleep(&ts, NULL);
    atomic_fetch_sub(&g_inside, 1);
    sem_post(&g_sem);                                 /* 归还许可 */
    return NULL;
}

int main(void)
{
    enum { N = 4 };
    pthread_t th[N];

    sem_init(&g_sem, 0, 1);                           /* 初值 1 = 一把钥匙 */
    for (int i = 0; i < N; i++)
        pthread_create(&th[i], NULL, worker, NULL);
    for (int i = 0; i < N; i++)
        pthread_join(th[i], NULL);
    sem_destroy(&g_sem);

    printf("二值信号量(初值 1): 线程数=%d 临界区最大并发=%d (期望 1)\n",
           N, atomic_load(&g_max_inside));
    return atomic_load(&g_max_inside) == 1 ? 0 : 1;
}
