// @book: 操作系统导论
// @topic: condvar-while-vs-if
// @source: 操作系统导论/第30章 条件变量（讲义）.md §30.1-30.2
// @expect: 同一个假唤醒, while 版重新检查条件后继续睡, 最终正好消费 3 个元素, 退出码 0
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum { CAP = 4, ITEMS = 3 };
static int g_buf[CAP];
static int g_count;                       /* 队列里现有几个元素 */
static pthread_mutex_t g_m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_c = PTHREAD_COND_INITIALIZER;
static atomic_int g_consumer_waiting;     /* 消费者是否已入睡 */
static atomic_int g_noise_sent;           /* 噪声线程是否已发过假信号 */
static int g_wakeups;                     /* 消费者被唤醒的次数 */
static int g_bug;                         /* 复现出 bug 则置 1 */

static void nap(long ns)
{
    struct timespec ts = {0, ns};
    nanosleep(&ts, NULL);
}

/* 噪声线程: 模拟假唤醒(某些实现里一次 signal 会唤醒两个线程) */
static void *noise(void *arg)
{
    (void)arg;
    while (!atomic_load(&g_consumer_waiting))
        nap(1000000L);
    pthread_mutex_lock(&g_m);
    pthread_cond_signal(&g_c);            /* 此时队列是空的: 这是一次假唤醒 */
    atomic_store(&g_noise_sent, 1);
    pthread_mutex_unlock(&g_m);
    return NULL;
}

static void *producer(void *arg)
{
    (void)arg;
    while (!atomic_load(&g_noise_sent))    /* 保证假信号先于真实数据 */
        nap(1000000L);
    for (int i = 0; i < ITEMS; i++) {
        pthread_mutex_lock(&g_m);
        g_buf[g_count++] = i;
        printf("[producer] 放入 %d, 队列长度=%d\n", i, g_count);
        pthread_cond_signal(&g_c);
        pthread_mutex_unlock(&g_m);
        nap(2000000L);
    }
    return NULL;
}

static void *consumer(void *arg)
{
    (void)arg;
    int got = 0;
    pthread_mutex_lock(&g_m);
    while (got < ITEMS) {
        while (g_count == 0) {              /* 醒来必须重新检查条件 */
            atomic_store(&g_consumer_waiting, 1);
            pthread_cond_wait(&g_c, &g_m);
            atomic_store(&g_consumer_waiting, 0);
            g_wakeups++;
        }
        int v = g_buf[--g_count];           /* BUG 版: 队列为空时 --g_count 变 -1, g_buf[-1] 越界 */
        if (g_count < 0) {
            printf("runtime error: 空队列被取走一次(if 版漏了重新检查), g_count=%d\n", g_count);
            fflush(stdout);
            g_bug = 1;
            break;
        }
        printf("[consumer] 取到 %d\n", v);
        got++;
        pthread_cond_signal(&g_c);
    }
    pthread_mutex_unlock(&g_m);
    return NULL;
}

int main(void)
{
    pthread_t tc, tp, tn;

    pthread_create(&tc, NULL, consumer, NULL);
    pthread_create(&tn, NULL, noise, NULL);
    pthread_create(&tp, NULL, producer, NULL);
    pthread_join(tc, NULL);
    pthread_join(tp, NULL);
    pthread_join(tn, NULL);

    if (g_bug) {
        printf("if 版结局: 消费中途踩空, 退出码=1\n");
        return 1;
    }
    printf("while 版结局: 消费 %d 个元素, 被唤醒 %d 次(其中 1 次是假唤醒), 退出码=0\n",
           ITEMS, g_wakeups);
    return 0;
}
