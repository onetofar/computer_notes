// @book: 操作系统导论
// @topic: concurrency-anomalies
// @source: 操作系统导论/第32章 常见并发问题（讲义）.md §32.3
// @expect: 两个线程用 trylock+回退互相谦让, 一直都在运行却一个也没进临界区
// @ub: yes
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

enum { ROUNDS = 20 };

static pthread_mutex_t g_l1 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_l2 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_gate_cv = PTHREAD_COND_INITIALIZER;
static int g_ticket = -1;                            /* 当前关卡票号 */
static int g_arrivals;                               /* 本关卡到达人数 */
static int g_entered;                                /* 真正进过临界区的次数 */

static void nap(long ns)
{
    struct timespec ts = {0, ns};
    nanosleep(&ts, NULL);
}

/* 关卡同步: 同一票号的两个线程互相等齐, 保证动作发生在同一瞬间 */
static void gate_sync(int ticket)
{
    pthread_mutex_lock(&g_gate);
    if (g_ticket != ticket) {
        g_ticket = ticket;
        g_arrivals = 0;
    }
    g_arrivals++;
    if (g_arrivals >= 2) {
        g_arrivals = 0;
        g_ticket = ticket + 1;                       /* 放行, 换下一张票 */
        pthread_cond_broadcast(&g_gate_cv);
    } else {
        while (g_ticket == ticket)
            pthread_cond_wait(&g_gate_cv, &g_gate);
    }
    pthread_mutex_unlock(&g_gate);
}

/* trylock 失败就放手重来: 死锁变成了活锁 */
static void *worker(void *arg)
{
    long id = (long)(intptr_t)arg;
    pthread_mutex_t *own = (id == 0) ? &g_l1 : &g_l2;
    pthread_mutex_t *other = (id == 0) ? &g_l2 : &g_l1;

    for (int round = 0; round < ROUNDS; round++) {
        pthread_mutex_lock(own);
        gate_sync(round * 2);                        /* 两边都握着各自的锁 */
        if (pthread_mutex_trylock(other) == 0) {      /* 对方还握着, 必然失败 */
            g_entered++;
            pthread_mutex_unlock(other);
        }
        gate_sync(round * 2 + 1);                    /* 两边都尝试过再放手 */
        pthread_mutex_unlock(own);
        nap(200000L);                                 /* 礼貌退避 */
    }
    return NULL;
}

int main(void)
{
    pthread_t th[2];

    pthread_create(&th[0], NULL, worker, (void *)(intptr_t)0);
    pthread_create(&th[1], NULL, worker, (void *)(intptr_t)1);
    pthread_join(th[0], NULL);
    pthread_join(th[1], NULL);

    if (g_entered == 0) {
        printf("runtime error: 活锁 —— 两个线程都在运行(轮数=%d), 但一次也没进临界区\n", ROUNDS);
        return 1;
    }
    printf("没有活锁: 成功进入临界区 %d 次\n", g_entered);
    return 0;
}
