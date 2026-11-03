// @book: 网络多人在线游戏开发实战教程
// @topic: thread-process-actor
// @source: 第3章 线程、进程以及Actor模型（讲义）.md §3.5
// @expect: 8 个子进程各算 5 万次再经管道回传, 结果恒为 400000, 代价是 fork 开销与 64 字节管道消息
#include <chrono>
#include <cstdio>
#include <vector>

#if defined(__linux__)
#include <sys/wait.h>
#include <unistd.h>

namespace {

constexpr int kWorkers = 8;
constexpr int kPerWorker = 50000;

}  // namespace

int main() {
    auto t0 = std::chrono::steady_clock::now();
    int fds[2];
    if (::pipe(fds) != 0) { std::printf("pipe failed\n"); return 1; }

    std::vector<pid_t> kids;
    for (int w = 0; w < kWorkers; ++w) {
        pid_t pid = ::fork();
        if (pid < 0) { std::printf("fork failed\n"); return 1; }
        if (pid == 0) {
            ::close(fds[0]);
            long long local = 0;  // 进程私有, 没有任何共享可变状态
            for (int i = 0; i < kPerWorker; ++i) ++local;
            ::write(fds[1], &local, sizeof(local));
            ::close(fds[1]);
            ::_exit(0);
        }
        kids.push_back(pid);
    }
    ::close(fds[1]);
    for (pid_t pid : kids) {
        int status = 0;
        ::waitpid(pid, &status, 0);
    }
    long long total = 0;
    long bytes = 0;
    for (int i = 0; i < kWorkers; ++i) {
        long long part = 0;
        ssize_t n = ::read(fds[0], &part, sizeof(part));
        if (n == static_cast<ssize_t>(sizeof(part))) {
            total += part;
            bytes += n;
        }
    }
    ::close(fds[0]);
    auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    std::printf("process+pipe: workers=%d per_worker=%d counter=%lld expected=%d\n",
                kWorkers, kPerWorker, total, kWorkers * kPerWorker);
    std::printf("pipe_bytes=%ld elapsed_ms=%.1f\n", bytes, ms);
    return 0;
}

#else

int main() {
    std::printf("本演示使用 POSIX fork/pipe, 请在 Linux 上运行\n");
    return 0;
}

#endif
