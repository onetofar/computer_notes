// @book: 操作系统导论
// @topic: concurrency-anomalies
// @source: 操作系统导论/第31章 信号量（讲义）.md §31.5
// @expect: 读者源源不断进来, 写者一直拿不到写锁, 判定为写者饥饿
// @ub: yes
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    sem_t lock;                                      /* 保护 readers 计数 */
    sem_t writelock;                                 /* 一个写者 或 多个读者 */
    int readers;
} rwlock_t;

static rwlock_t g_rw;
static atomic_int g_read_entries;                   /* 读者成功进入的次数 */
static atomic_int g_reader_ready;                   /* 长驻读者是否已占住读锁 */

static void nap(long ns)
{
    struct timespec ts = {0, ns};
    nanosleep(&ts, NULL);
}

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static void acquire_read(rwlock_t *rw)
{
    sem_wait(&rw->lock);
    rw->readers++;
    if (rw->readers == 1)
        sem_wait(&rw->writelock);                    /* 第一个读者代持写锁 */
    sem_post(&rw->lock);
}

static void release_read(rwlock_t *rw)
{
    sem_wait(&rw->lock);
    rw->readers--;
    if (rw->readers == 0)
        sem_post(&rw->writelock);
    sem_post(&rw->lock);
}

/* 长驻读者: 一直占着读锁, 让读者计数永远不归零 */
static void *reader_hold(void *arg)
{
    (void)arg;
    acquire_read(&g_rw);
    atomic_fetch_add(&g_read_entries, 1);
    atomic_store(&g_reader_ready, 1);
    nap(400 * 1000 * 1000L);
    release_read(&g_rw);
    return NULL;
}

/* 来回切换的读者: 制造持续不断的读流量 */
static void *reader_churn(void *arg)
{
    (void)arg;
    for (int i = 0; i < 60; i++) {
        acquire_read(&g_rw);
        atomic_fetch_add(&g_read_entries, 1);
        nap(2 * 1000 * 1000L);
        release_read(&g_rw);
        nap(2 * 1000 * 1000L);
    }
    return NULL;
}

int main(void)
{
    pthread_t hold, churn;
    int got = 0;
    double deadline;

    sem_init(&g_rw.lock, 0, 1);
    sem_init(&g_rw.writelock, 0, 1);
    pthread_create(&hold, NULL, reader_hold, NULL);
    while (!atomic_load(&g_reader_ready))            /* 等长驻读者占住读锁 */
        nap(1000 * 1000L);
    pthread_create(&churn, NULL, reader_churn, NULL);

    deadline = now_ms() + 250.0;                     /* 写者只肯等 250ms */
    while (now_ms() < deadline) {
        if (sem_trywait(&g_rw.writelock) == 0) {
            got = 1;
            sem_post(&g_rw.writelock);
            break;
        }
        nap(500 * 1000 * 1000L);
    }
    pthread_join(hold, NULL);
    pthread_join(churn, NULL);

    if (!got) {
        printf("runtime error: 写者饥饿 —— 读者进入了 %d 次, 写者等了 250ms 仍未拿到写锁\n",
               atomic_load(&g_read_entries));
        sem_destroy(&g_rw.lock);
        sem_destroy(&g_rw.writelock);
        return 1;
    }
    printf("写者成功拿到写锁\n");
    return 0;
}
