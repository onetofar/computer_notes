// @book: C++ Concurrency in Action
// @topic: multi-lock-acquisition
// @source: C++ Concurrency in Action/Chapter 3. Sharing data between threads（讲义）.md §3.2.6
// @expect: unique_lock 可以延迟加锁、提前解锁, 代价是比 lock_guard 更大更慢
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>

int main() {
    std::mutex m;
    long long counter = 0;

    std::unique_lock<std::mutex> lk(m, std::defer_lock);   // 构造时不加锁
    std::printf("unique_lock: 构造后 owns_lock=%s\n", lk.owns_lock() ? "true" : "false");

    lk.lock();
    ++counter;
    std::printf("unique_lock: lock() 后 owns_lock=%s\n", lk.owns_lock() ? "true" : "false");

    lk.unlock();                                 // 临界区之外的活儿不占着锁
    std::printf("unique_lock: unlock() 后 owns_lock=%s\n", lk.owns_lock() ? "true" : "false");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    lk.lock();
    ++counter;
    lk.unlock();

    std::printf("unique_lock: counter=%lld\n", counter);
    return 0;
}
