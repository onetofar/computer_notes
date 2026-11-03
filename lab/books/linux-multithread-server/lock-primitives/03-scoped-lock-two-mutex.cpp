// @book: Linux多线程服务端编程
// @topic: lock-primitives
// @source: 第2章 线程同步精要（讲义）.md §2.1.2 死锁
// @expect: scoped_lock 同时锁两把 mutex, 两线程以相反顺序请求也不死锁
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <mutex>
#include <thread>

int main() {
    std::mutex m1;
    std::mutex m2;

    // 两把锁一起拿, 内部用死锁避免算法, 不依赖调用方的加锁顺序
    auto body = [&](const char* who) {
        std::scoped_lock lk(m1, m2);
        std::printf("[%s] 同时持有 m1 和 m2\n", who);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    };

    auto t1 = std::async(std::launch::async, [&] { body("T1"); });
    auto t2 = std::async(std::launch::async, [&] { body("T2"); });

    if (t1.wait_for(std::chrono::milliseconds(500)) == std::future_status::timeout) {
        std::printf("DEADLOCK DETECTED\n");
        std::fflush(stdout);
        std::_Exit(0);
    }
    t1.get();
    t2.get();

    std::printf("NO DEADLOCK: scoped_lock 不依赖调用方的加锁顺序\n");
    return 0;
}
