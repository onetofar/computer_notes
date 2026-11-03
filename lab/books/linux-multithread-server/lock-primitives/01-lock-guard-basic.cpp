// @book: Linux多线程服务端编程
// @topic: lock-primitives
// @source: 第2章 线程同步精要（讲义）.md §2.4
// @expect: lock_guard 作用域结束即解锁, 打印 临界区耗时 与 解锁发生的位置
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>

std::mutex g_mu;
int g_counter = 0;

void bump_with_guard() {
    std::lock_guard<std::mutex> lk(g_mu);
    ++g_counter;
    std::printf("[worker] 临界区内, counter=%d\n", g_counter);
    // 离开作用域自动解锁, 不需要手写 unlock
}

int main() {
    auto t0 = std::chrono::steady_clock::now();
    std::thread t(bump_with_guard);
    t.join();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();
    std::printf("lock_guard: counter=%d, 临界区随作用域结束, 总耗时=%lld us\n",
                g_counter, (long long)us);
    return 0;
}
