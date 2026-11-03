// @book: 操作系统导论
// @topic: semaphore-binary-vs-counting
// @source: 操作系统导论/第31章 信号量（讲义）.md §31.1
// @expect: 初值 3 的计数信号量: 4 个线程里临界区最多同时进 3 个, 第 4 个必须等
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static sem_t g_sem;
static atomic_int g_inside;
static atomic_int g_max_inside;

static void *worker(void *arg)
{
    (void)arg;
    sem_wait(&g_sem);                                 /* 等一个许可, 许可有 3 个 */
    int cur = atomic_fetch_add(&g_inside, 1) + 1;
    int seen = atomic_load(&g_max_inside);
    while (cur > seen && !atomic_compare_exchange_weak(&g_max_inside, &seen, cur)) {
        /* CAS 失败就重试 */
    }
    struct timespec ts = {0, 30 * 1000 * 1000L};
    nanosleep(&ts, NULL);
    atomic_fetch_sub(&g_inside, 1);
    sem_post(&g_sem);
    return NULL;
}

int main(void)
{
    enum { N = 4 };
    const int kPermits = 3;
    pthread_t th[N];

    sem_init(&g_sem, 0, kPermits);                    /* 初值 N = N 个许可 */
    for (int i = 0; i < N; i++)
        pthread_create(&th[i], NULL, worker, NULL);
    for (int i = 0; i < N; i++)
        pthread_join(th[i], NULL);
    sem_destroy(&g_sem);

    printf("计数信号量(初值 %d): 线程数=%d 临界区最大并发=%d (期望 %d)\n",
           kPermits, N, atomic_load(&g_max_inside), kPermits);
    return atomic_load(&g_max_inside) == kPermits ? 0 : 1;
}
