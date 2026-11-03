// @book: 网络多人在线游戏开发实战教程
// @topic: thread-process-actor
// @source: 第3章 线程、进程以及Actor模型（讲义）.md §3.5
// @expect: 8 线程各加 5 万次, 加锁后计数恒为 400000, 锁获取次数 = 40 万, 锁把并行退化成串行
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

namespace {

constexpr int kWorkers = 8;
constexpr int kPerWorker = 50000;

long long g_counter = 0;
std::mutex g_mutex;
std::atomic<long> g_lock_ops{0};

}  // namespace

int main() {
    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> pool;
    for (int w = 0; w < kWorkers; ++w) {
        pool.emplace_back([] {
            for (int i = 0; i < kPerWorker; ++i) {
                std::lock_guard<std::mutex> guard(g_mutex);  // 临界区: 只有一个线程能进
                ++g_counter;
                ++g_lock_ops;
            }
        });
    }
    for (auto& t : pool) t.join();
    auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    std::printf("thread+mutex: workers=%d per_worker=%d counter=%lld expected=%d\n",
                kWorkers, kPerWorker, g_counter, kWorkers * kPerWorker);
    std::printf("lock_ops=%ld elapsed_ms=%.1f\n", g_lock_ops.load(), ms);
    return 0;
}
