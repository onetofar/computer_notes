// @book: C++ Concurrency in Action
// @topic: memory-ordering
// @source: C++ Concurrency in Action/Chapter 5. The C++ memory model and operations on atomic types（讲义）.md §5.3.3
// @expect: relaxed 不产生 synchronizes-with, 允许出现两个线程都读到对方还没写的值
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

constexpr std::memory_order kStore = std::memory_order_relaxed;
constexpr std::memory_order kLoad  = std::memory_order_relaxed;
constexpr int kRounds = 50000;

int main() {
    std::atomic<int> x0{0}, x1{0}, x2{0}, x3{0};
    std::atomic<int> y0{0}, y1{0}, y2{0}, y3{0};
    std::atomic<int> aGo{0}, bGo{0};
    std::vector<unsigned char> staleA(kRounds + 1, 0), staleB(kRounds + 1, 0);

    auto t0 = std::chrono::steady_clock::now();
    std::thread ta([&] {
        for (int r = 1; r <= kRounds; ++r) {
            aGo.store(r, std::memory_order_relaxed);        // 只用来对齐, 不参与被测关系
            while (bGo.load(std::memory_order_relaxed) < r) {}
            x1.store(r, kStore); x2.store(r, kStore); x3.store(r, kStore);
            x0.store(r, kStore);                            // 关键 store 排最后, 留在 store buffer 里
            if (y0.load(kLoad) < r) staleA[r] = 1;           // 对方还没写 => 陈旧读
        }
    });
    std::thread tb([&] {
        for (int r = 1; r <= kRounds; ++r) {
            bGo.store(r, std::memory_order_relaxed);
            while (aGo.load(std::memory_order_relaxed) < r) {}
            y1.store(r, kStore); y2.store(r, kStore); y3.store(r, kStore);
            y0.store(r, kStore);
            if (x0.load(kLoad) < r) staleB[r] = 1;
        }
    });
    ta.join();
    tb.join();

    int both = 0, one = 0;
    for (int r = 1; r <= kRounds; ++r) {
        if (staleA[r] && staleB[r]) ++both;
        else if (staleA[r] || staleB[r]) ++one;
    }
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();
    std::printf("%s: 双方都读到陈旧值=%d 轮, 单边陈旧=%d 轮 / 共 %d 轮, 耗时=%lld us\n",
                "relaxed", both, one, kRounds, (long long)us);
    return 0;
}
