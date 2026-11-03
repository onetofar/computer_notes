// @book: 操作系统导论
// @topic: thread-api-create
// @source: 操作系统导论/第27章 插叙：线程API（讲义）.md §27.1-27.2
// @expect: 同一份计算用 std::thread 重写, 结果与 01-pthread-create.c 完全一致
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

struct Task {
    int id;
    int base;
    int iters;
    int result{0};
};

int main()
{
    constexpr int kThreads = 4;
    constexpr int kIters = 1000;

    // 任务放在堆上的 vector 里, 线程通过引用捕获拿到自己的任务
    std::vector<Task> tasks(kThreads);
    std::vector<std::thread> ths;
    auto t0 = std::chrono::steady_clock::now();

    for (int i = 0; i < kThreads; ++i) {
        tasks[i].id = i;
        tasks[i].base = i + 1;
        tasks[i].iters = kIters;
        ths.emplace_back([&tasks, i] {
            int acc = 0;
            for (int k = 0; k < tasks[i].iters; ++k)
                acc += tasks[i].base;
            tasks[i].result = acc;      // 结果写在堆上, 不是线程栈
            std::printf("[thread %d] base=%d iters=%d result=%d\n",
                        tasks[i].id, tasks[i].base, tasks[i].iters, acc);
        });
    }
    for (auto &th : ths)
        th.join();
    auto dt = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();

    long long total = 0;
    for (const auto &t : tasks)
        total += t.result;
    std::printf("join 完成: 线程数=%d 总和=%lld 耗时=%.3f ms\n", kThreads, total, (long long)dt / 1000.0);
    return 0;
}
