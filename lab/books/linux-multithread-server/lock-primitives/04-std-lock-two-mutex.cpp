// @book: Linux多线程服务端编程
// @topic: lock-primitives
// @source: 第2章 线程同步精要（讲义）.md §2.1.2
// @expect: std::lock 一次获取两把锁后仍由调用方负责 unlock, 对照 scoped_lock 的自动释放
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <mutex>
#include <thread>

int main() {
    std::mutex m1;
    std::mutex m2;

    // std::lock 是函数: 锁成功后两把锁都由调用方持有, 必须自己 unlock
    auto body = [&](const char* who) {
        std::lock(m1, m2);
        std::printf("[%s] std::lock 成功, 两把锁都归我管\n", who);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        m2.unlock();  // std::lock 不管释放, 得自己写
        m1.unlock();
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

    std::printf("NO DEADLOCK: std::lock 避免死锁, 但释放仍需手工\n");
    return 0;
}
