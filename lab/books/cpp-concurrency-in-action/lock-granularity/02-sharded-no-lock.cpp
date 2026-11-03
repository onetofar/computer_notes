// @book: C++ Concurrency in Action
// @topic: lock-granularity
// @source: C++ Concurrency in Action/Chapter 4. Synchronizing concurrent operations（讲义）.md
// @expect: 分片无锁原子累加, 无数据竞争, 程序正常退出
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

int main() {
    constexpr int kThreads = 4;
    constexpr int kIters = 20000;
    constexpr int kShards = 4;
    std::atomic<long long> shards[kShards];   // 每个线程独占一片, 无需加锁

    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t) {
        ts.emplace_back([&, t] {
            auto& shard = shards[t % kShards];
            for (int i = 0; i < kIters; ++i) shard.fetch_add(1, std::memory_order_relaxed);
        });
    }
    for (auto& th : ts) th.join();
    auto dt = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();

    long long sum = 0;
    for (int i = 0; i < kShards; ++i) sum += shards[i].load();
    std::printf("sharded-无锁: sum=%lld 耗时=%lld us\n", sum, (long long)dt);
    return 0;
}
