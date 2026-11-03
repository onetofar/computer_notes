// @book: Linux多线程服务端编程
// @topic: deadlock-lock-order
// @source: 第2章 线程同步精要（讲义）.md §2.1.2
// @expect: 两线程改为同一加锁顺序, 正常完成, 打印耗时
#include <chrono>
#include <cstdio>
#include <future>
#include <mutex>
#include <thread>

int main() {
    std::mutex inventory_mu;
    std::mutex request_mu;

    auto t0 = std::chrono::steady_clock::now();

    // 全局约定: inventory_mu 永远先于 request_mu
    auto body = [&] {
        std::lock_guard<std::mutex> g1(inventory_mu);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::lock_guard<std::mutex> g2(request_mu);
        std::printf("[T] 两把锁按同一顺序拿到\n");
    };

    auto t1 = std::async(std::launch::async, body);
    auto t2 = std::async(std::launch::async, body);

    t1.get();
    t2.get();

    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();
    std::printf("NO DEADLOCK: 全局加锁顺序一致, 耗时=%lld us\n", (long long)us);
    return 0;
}
