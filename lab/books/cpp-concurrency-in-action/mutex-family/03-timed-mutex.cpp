// @book: C++ Concurrency in Action
// @topic: mutex-family
// @source: C++ Concurrency in Action/Chapter 4. Synchronizing concurrent operations（讲义）.md §4.3.4
// @expect: timed_mutex 的 try_lock_for 拿不到锁时按超时返回 false, 而不是一直阻塞
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>

int main() {
    std::timed_mutex tm;
    std::printf("timed_mutex: 空锁 try_lock=%s\n", tm.try_lock() ? "true" : "false");
    tm.unlock();

    std::thread holder([&] {
        tm.lock();
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        tm.unlock();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));   // 确保 holder 已持锁

    auto t0 = std::chrono::steady_clock::now();
    bool got = tm.try_lock_for(std::chrono::milliseconds(30));
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - t0).count();

    std::printf("timed_mutex: try_lock_for(30ms)=%s, 实际等了 %lld ms\n",
                got ? "true" : "false", (long long)ms);
    if (got) tm.unlock();
    holder.join();
    return 0;
}
