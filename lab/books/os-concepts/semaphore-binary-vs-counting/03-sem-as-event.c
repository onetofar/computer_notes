// @book: 操作系统导论
// @topic: semaphore-binary-vs-counting
// @source: 操作系统导论/第31章 信号量（讲义）.md §31.3
// @expect: 初值 0 的信号量当事件标志: 事件先发生或后发生, 等待方都能正确返回
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static sem_t g_evt;

static void *child(void *arg)
{
    (void)arg;
    struct timespec ts = {0, 50 * 1000 * 1000L};
    nanosleep(&ts, NULL);
    printf("[child] 干活完毕, 发一次许可\n");
    sem_post(&g_evt);                                 /* 事件发生 */
    return NULL;
}

int main(void)
{
    pthread_t c;

    /* 场景一: 父线程先等, 子线程后发信号 */
    sem_init(&g_evt, 0, 0);
    pthread_create(&c, NULL, child, NULL);
    printf("[parent] 开始等待事件\n");
    sem_wait(&g_evt);
    printf("[parent] 被唤醒, 拿到许可(队列里存下了这次许可)\n");
    pthread_join(c, NULL);
    sem_destroy(&g_evt);

    /* 场景二: 事件先发生, 等待方后到 —— 状态和计数融进了同一个数 */
    sem_init(&g_evt, 0, 0);
    sem_post(&g_evt);
    printf("[parent] 事件已经发生过, 现在才来等\n");
    sem_wait(&g_evt);
    printf("[parent] sem_wait 立刻返回, 没有白等\n");
    sem_destroy(&g_evt);
    return 0;
}
