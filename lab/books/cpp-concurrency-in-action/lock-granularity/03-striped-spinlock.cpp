// @book: C++ Concurrency in Action
// @topic: lock-granularity
// @source: C++ Concurrency in Action/Chapter 4. Synchronizing concurrent operations（讲义）.md
// @expect: 自旋锁分片, 无数据竞争, 程序正常退出
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

struct SpinLock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
    void lock() {
        while (flag.test_and_set(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }
    void unlock() { flag.clear(std::memory_order_release); }
};

int main() {
    constexpr int kThreads = 4;
    constexpr int kIters = 20000;
    constexpr int kShards = 4;
    SpinLock locks[kShards];
    long long shards[kShards] = {0, 0, 0, 0};

    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t) {
        ts.emplace_back([&, t] {
            int idx = t % kShards;
            for (int i = 0; i < kIters; ++i) {
                locks[idx].lock();
                shards[idx] += 1;
                locks[idx].unlock();
            }
        });
    }
    for (auto& th : ts) th.join();
    auto dt = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();

    long long sum = 0;
    for (int i = 0; i < kShards; ++i) sum += shards[i];
    std::printf("spinlock-分片: sum=%lld 耗时=%lld us\n", sum, (long long)dt);
    return 0;
}
