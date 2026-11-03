// @book: 深入理解计算机系统
// @topic: thread-vs-process
// @source: 第12章 并发编程（讲义）.md §12.5.2
// @expect: 裸 volatile 计数器会被线程交错破坏, 结果小于理论值且每次运行都不一样
// @note: 故意保留的数据竞争(C++ 里是未定义行为)。valgrind/ASan 查不出竞态(要 TSan),
//        所以本文件不标 @ub, 否则 run_all.sh --ub 会判成"没触发检测".
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

/* 讲义 Figure 12.16 的 badcnt.c: volatile 只保证"不被优化掉", 不保证原子性 */
static volatile long cnt = 0;

int main() {
    constexpr int kThreads = 2;
    constexpr long kIters = 1000000;

    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t) {
        ts.emplace_back([] {
            for (long i = 0; i < kIters; ++i) {
                /* 讲义 §12.5.2 把 cnt++ 拆成三步: L 读 / U 改 / S 写.
                   三步各自都是原子的, 合起来不是 —— 两个线程会互相覆盖. */
                long local = cnt;      /* L: Load  */
                local += 1;            /* U: Update */
                cnt = local;           /* S: Store */
            }
        });
    }
    for (auto &th : ts) th.join();
    auto dt = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();

    long expect = static_cast<long>(kThreads) * kIters;
    std::printf("裸 volatile: cnt=%ld (理论值 %ld)  丢了 %ld 次  耗时=%lld us\n",
                cnt, expect, expect - cnt, static_cast<long long>(dt));
    return 0;
}
