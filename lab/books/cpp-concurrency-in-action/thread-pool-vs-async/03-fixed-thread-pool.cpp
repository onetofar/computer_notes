// @book: C++ Concurrency in Action
// @topic: thread-pool-vs-async
// @source: C++ Concurrency in Action/Chapter 9. Advanced thread management（讲义）.md §9.1.1
// @expect: 固定线程池: 线程数固定, 任务排队执行, 峰值在线线程不随任务数增长
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

int main() {
    constexpr int kWorkers = 8;
    constexpr int kTasks = 64;

    std::mutex m;
    std::condition_variable cv;
    std::queue<std::function<void()>> q;
    std::atomic<int> live{0}, peak{0}, done{0};

    std::vector<std::thread> workers;
    for (int w = 0; w < kWorkers; ++w) {
        workers.emplace_back([&] {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lk(m);
                    cv.wait(lk, [&] { return !q.empty() || done.load() != 0; });
                    if (q.empty()) {
                        if (done.load() != 0) return;
                        continue;
                    }
                    task = std::move(q.front());
                    q.pop();
                }
                int now = ++live;
                int prev = peak.load();
                while (now > prev && !peak.compare_exchange_weak(prev, now)) {}
                task();
                --live;
            }
        });
    }

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kTasks; ++i) {
        {
            std::lock_guard<std::mutex> lk(m);
            q.emplace([i] { std::this_thread::sleep_for(std::chrono::milliseconds(5)); });
        }
        cv.notify_one();
    }
    {
        std::lock_guard<std::mutex> lk(m);
        done.store(1);
    }
    cv.notify_all();
    for (auto& w : workers) w.join();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();

    std::printf("thread-pool: 任务=%d 固定线程=%d 同时在线线程峰值=%d 耗时=%lld us\n",
                kTasks, kWorkers, peak.load(), (long long)us);
    return 0;
}
