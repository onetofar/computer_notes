// @book: 操作系统导论
// @topic: thread-api-create
// @source: 操作系统导论/第27章 插叙：线程API（讲义）.md §27.1-27.2
// @expect: 4 个线程各自收到打包好的参数并把结果写回调用者的结构体, join 后总和正确
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
    int id;       /* 线程编号 */
    int base;     /* 计算起点 */
    int iters;    /* 迭代次数 */
    int result;   /* 结果写回这里: 调用者的结构体, 不是线程栈 */
} task_t;

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

/* 线程体只有一个 void* 参数: 进来强转回 task_t* 拆包 */
static void *worker(void *arg)
{
    task_t *t = (task_t *)arg;
    int acc = 0;
    for (int i = 0; i < t->iters; i++)
        acc += t->base;
    t->result = acc;              /* 写回调用者的内存, 线程栈一退就没了 */
    printf("[thread %d] base=%d iters=%d result=%d\n", t->id, t->base, t->iters, acc);
    return NULL;
}

int main(void)
{
    enum { N = 4 };
    task_t *tasks = calloc(N, sizeof(task_t));
    pthread_t th[N];
    double t0, dt;
    long long total = 0;

    if (tasks == NULL)
        return 1;

    t0 = now_ms();
    for (int i = 0; i < N; i++) {
        tasks[i].id = i;
        tasks[i].base = i + 1;
        tasks[i].iters = 1000;
        if (pthread_create(&th[i], NULL, worker, &tasks[i]) != 0) {
            fprintf(stderr, "pthread_create failed\n");
            return 1;
        }
    }
    for (int i = 0; i < N; i++)
        pthread_join(th[i], NULL);
    dt = now_ms() - t0;

    for (int i = 0; i < N; i++)
        total += tasks[i].result;
    printf("join 完成: 线程数=4 总和=%lld 耗时=%.3f ms\n", total, dt);
    free(tasks);
    return 0;
}
