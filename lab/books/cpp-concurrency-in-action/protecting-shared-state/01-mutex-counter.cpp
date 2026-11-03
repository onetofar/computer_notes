// @book: C++ Concurrency in Action
// @topic: protecting-shared-state
// @source: C++ Concurrency in Action/Chapter 3. Sharing data between threads（讲义）.md §3.2.8
// @expect: mutex 保护计数器, 结果恒定, 耗时可与 02-atomic-counter 直接对比
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

int main() {
    constexpr int kThreads = 4, kIters = 50000;
    std::mutex m;
    long long counter = 0;

    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t) {
        ts.emplace_back([&] {
            for (int i = 0; i < kIters; ++i) {
                std::lock_guard<std::mutex> lk(m);   // 每次加法都要过一遍锁
                ++counter;
            }
        });
    }
    for (auto& t : ts) t.join();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();

    std::printf("mutex-counter: counter=%lld (期望 %d) 耗时=%lld us\n",
                counter, kThreads * kIters, (long long)us);
    return 0;
}
