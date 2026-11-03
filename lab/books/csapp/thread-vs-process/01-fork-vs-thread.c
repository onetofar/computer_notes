// @book: 深入理解计算机系统
// @topic: thread-vs-process
// @source: 第12章 并发编程（讲义）.md §12.1.4
// @expect: Linux 上创建 20 个子进程的成本比创建 20 个 pthread 高一个数量级; Windows 上本文件只作提示
#define _DEFAULT_SOURCE 1        /* POSIX 扩展: clock_gettime 等, 必须在第一个系统头之前定义 */
#include <stdio.h>

#if defined(_WIN32)

int main(void) {
    printf("本文件演示 POSIX fork 与 pthread_create 的创建开销对比, 请在 Linux 上运行。\n");
    return 0;
}

#else

#include <pthread.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define N 20

static void *noop(void *arg) {
    (void)arg;
    return NULL;
}

static double ms_since(struct timespec t0) {
    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (double)(t1.tv_sec - t0.tv_sec) * 1e3 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6;
}

int main(void) {
    struct timespec t0;

    /* 线程: 创建 + 回收 */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < N; i++) {
        pthread_t tid;
        pthread_create(&tid, NULL, noop, NULL);
        pthread_join(tid, NULL);
    }
    double ms_thread = ms_since(t0);

    /* 进程: fork + waitpid */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < N; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            _exit(0);                     /* 子进程立刻退出 */
        }
        int status = 0;
        waitpid(pid, &status, 0);        /* 收尸, 不留僵尸 */
    }
    double ms_fork = ms_since(t0);

    printf("创建 %d 个 pthread : %8.3f ms (每次 %.3f ms)\n", N, ms_thread, ms_thread / N);
    printf("创建 %d 个 子进程 : %8.3f ms (每次 %.3f ms)\n", N, ms_fork, ms_fork / N);
    printf("fork / pthread 倍数 = %.1fx\n", ms_fork / ms_thread);
    printf("\n子进程全部被 waitpid 回收, 没有僵尸残留\n");
    return 0;
}

#endif
