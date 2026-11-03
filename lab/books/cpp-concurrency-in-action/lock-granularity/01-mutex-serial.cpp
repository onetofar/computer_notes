// @book: C++ Concurrency in Action
// @topic: lock-granularity
// @source: C++ Concurrency in Action/Chapter 4. Synchronizing concurrent operations（讲义）.md
// @expect: 粗粒度大锁串行化, 吞吐最低, 编译运行通过
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

int main() {
    constexpr int kThreads = 4;
    constexpr int kIters = 20000;
    std::mutex m;                 // 一把大锁罩住整个累加
    long long sum = 0;

    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t) {
        ts.emplace_back([&] {
            for (int i = 0; i < kIters; ++i) {
                std::lock_guard<std::mutex> lk(m);
                sum += 1;
            }
        });
    }
    for (auto& th : ts) th.join();
    auto dt = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();

    std::printf("mutex-粗粒度: sum=%lld 耗时=%lld us\n", sum, (long long)dt);
    return 0;
}
