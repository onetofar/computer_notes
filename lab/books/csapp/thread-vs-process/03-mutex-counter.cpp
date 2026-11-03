// @book: 深入理解计算机系统
// @topic: thread-vs-process
// @source: 第12章 并发编程（讲义）.md §12.5 §12.6
// @expect: 互斥锁计数同样正确, 但每次加法都要抢一次锁, 耗时明显高于原子计数
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

int main() {
    constexpr int kThreads = 4;
    constexpr long kIters = 1000000;
    long cnt = 0;                 /* 讲义 psum-mutex 的做法: 一个全局变量 + 一把锁 */
    std::mutex mtx;

    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t) {
        ts.emplace_back([&] {
            for (long i = 0; i < kIters; ++i) {
                std::lock_guard<std::mutex> lk(mtx);
                cnt += 1;
            }
        });
    }
    for (auto &th : ts) th.join();
    auto dt = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();

    long expect = static_cast<long>(kThreads) * kIters;
    std::printf("互斥锁计数: cnt=%ld (理论值 %ld)  耗时=%lld us\n",
                cnt, expect, static_cast<long long>(dt));
    return 0;
}
