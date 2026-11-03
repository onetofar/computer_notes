// @book: C++ Concurrency in Action
// @topic: protecting-shared-state
// @source: C++ Concurrency in Action/Chapter 5. The C++ memory model and operations on atomic types（讲义）.md §5.2.1
// @expect: 同一份计数器换成 atomic, 结果同样恒定, 但不再有锁的开销
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

int main() {
    constexpr int kThreads = 4, kIters = 50000;
    std::atomic<long long> counter{0};

    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t) {
        ts.emplace_back([&] {
            for (int i = 0; i < kIters; ++i) {
                counter.fetch_add(1, std::memory_order_relaxed);   // 只要计数, 不需要同步别的数据
            }
        });
    }
    for (auto& t : ts) t.join();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();

    std::printf("atomic-counter: counter=%lld (期望 %d) 耗时=%lld us\n",
                counter.load(), kThreads * kIters, (long long)us);
    return 0;
}
