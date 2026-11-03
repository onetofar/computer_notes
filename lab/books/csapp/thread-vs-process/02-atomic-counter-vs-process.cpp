// @book: 深入理解计算机系统
// @topic: thread-vs-process
// @source: 第12章 并发编程（讲义）.md §12.5 §12.6
// @expect: 原子计数永远等于理论值 4000000; 无锁, 但每次加法仍是一条原子指令
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

int main() {
    constexpr int kThreads = 4;
    constexpr long kIters = 1000000;
    std::atomic<long> cnt{0};

    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t) {
        ts.emplace_back([&] {
            for (long i = 0; i < kIters; ++i) {
                cnt.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto &th : ts) th.join();
    auto dt = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();

    long expect = static_cast<long>(kThreads) * kIters;
    std::printf("原子计数  : cnt=%ld (理论值 %ld)  耗时=%lld us\n",
                cnt.load(), expect, static_cast<long long>(dt));
    return 0;
}
