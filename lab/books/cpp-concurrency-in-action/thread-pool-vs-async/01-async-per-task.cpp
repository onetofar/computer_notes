// @book: C++ Concurrency in Action
// @topic: thread-pool-vs-async
// @source: C++ Concurrency in Action/Chapter 4. Synchronizing concurrent operations（讲义）.md §4.2.1
// @expect: 每个 async 任务各起一个线程, 同时在线线程数随任务数增长
#include <atomic>
#include <chrono>
#include <cstdio>
#include <future>
#include <thread>
#include <vector>

int main() {
    constexpr int kTasks = 64;
    std::atomic<int> live{0};
    std::atomic<int> peak{0};

    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::future<int>> fs;
    for (int i = 0; i < kTasks; ++i) {
        fs.emplace_back(std::async(std::launch::async, [&, i] {
            int now = ++live;
            int prev = peak.load();
            while (now > prev && !peak.compare_exchange_weak(prev, now)) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            --live;
            return i * i;
        }));
    }
    long long sum = 0;
    for (auto& f : fs) sum += f.get();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();

    std::printf("async-per-task: 任务=%d 同时在线线程峰值=%d 和=%lld 耗时=%lld us\n",
                kTasks, peak.load(), sum, (long long)us);
    return 0;
}
