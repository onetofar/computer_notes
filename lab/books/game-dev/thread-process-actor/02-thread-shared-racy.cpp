// @book: 网络多人在线游戏开发实战教程
// @topic: thread-process-actor
// @source: 第3章 线程、进程以及Actor模型（讲义）.md §3.5
// @expect: 不加锁时计数小于 400000, 丢失量随每次运行变化, 每次结果都不一样
// @note: 这里是数据竞争(未定义行为), 但 valgrind/ASan 查不出(需 ThreadSanitizer), 故不标 @ub: yes
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace {

constexpr int kWorkers = 8;
constexpr int kPerWorker = 50000;

volatile long long g_counter = 0;  // volatile 只保证不被优化掉, 不保证原子性

}  // namespace

int main() {
    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> pool;
    for (int w = 0; w < kWorkers; ++w) {
        pool.emplace_back([] {
            for (int i = 0; i < kPerWorker; ++i) {
                long long v = g_counter;   // 读
                std::this_thread::yield(); // 让别的线程插进来
                g_counter = v + 1;        // 写: 读-改-写三步被交错
            }
        });
    }
    for (auto& t : pool) t.join();
    auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    long long expected = static_cast<long long>(kWorkers) * kPerWorker;
    std::printf("thread+racy: workers=%d per_worker=%d counter=%lld expected=%lld lost=%lld\n",
                kWorkers, kPerWorker, g_counter, expected, expected - g_counter);
    std::printf("elapsed_ms=%.1f\n", ms);
    return 0;
}
