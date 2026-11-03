// @book: Linux多线程服务端编程
// @topic: deadlock-lock-order
// @source: 第2章 线程同步精要（讲义）.md §2.1.2 死锁
// @expect: 两线程按相反顺序获取两把 mutex, 超时判定后打印 DEADLOCK DETECTED 与双方持锁/等待关系
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <mutex>
#include <thread>

int main() {
    std::mutex inventory_mu;  // 保护清单
    std::mutex request_mu;    // 保护请求对象

    auto t1 = std::async(std::launch::async, [&] {
        std::lock_guard<std::mutex> g1(inventory_mu);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::lock_guard<std::mutex> g2(request_mu);  // T1 在这里等 request_mu
        std::printf("[T1] 两把锁都拿到, 不该走到这里\n");
    });

    auto t2 = std::async(std::launch::async, [&] {
        std::lock_guard<std::mutex> g2(request_mu);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::lock_guard<std::mutex> g1(inventory_mu);  // T2 在这里等 inventory_mu
        std::printf("[T2] 两把锁都拿到, 不该走到这里\n");
    });

    if (t1.wait_for(std::chrono::milliseconds(500)) == std::future_status::timeout) {
        std::printf("DEADLOCK DETECTED: 两个线程的加锁顺序正好相反\n");
        std::printf("T1: 持有 inventory_mu -> 等待 request_mu\n");
        std::printf("T2: 持有 request_mu   -> 等待 inventory_mu\n");
        std::fflush(stdout);
        std::_Exit(0);
    }

    t1.get();
    t2.get();
    std::printf("NO DEADLOCK\n");
    return 0;
}
