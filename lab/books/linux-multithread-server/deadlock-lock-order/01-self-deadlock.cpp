// @book: Linux多线程服务端编程
// @topic: deadlock-lock-order
// @source: 第2章 线程同步精要（讲义）.md §2.1.2 死锁
// @expect: 同一线程二次 lock 非递归 mutex, 任务超时未完成, 打印 DEADLOCK DETECTED
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <mutex>

int main() {
    std::mutex m;

    auto task = std::async(std::launch::async, [&m] {
        m.lock();
        std::printf("[T1] 已持有 mutex\n");
        std::fflush(stdout);
        m.lock();  // 同一线程再次加锁同一把非递归 mutex
        std::printf("[T1] 第二次 lock() 成功 (不应发生)\n");
        m.unlock();
        m.unlock();
    });

    if (task.wait_for(std::chrono::milliseconds(300)) == std::future_status::timeout) {
        std::printf("DEADLOCK DETECTED: 同一线程二次 lock() 非递归 mutex, 任务 300ms 未完成\n");
        std::printf("验尸要点: 线程停在 mutex 上, 而不是在等事件\n");
        std::fflush(stdout);
        std::_Exit(0);  // 直接退出, 避免 future 析构时等待那个死锁的任务
    }

    task.get();
    std::printf("NO DEADLOCK\n");
    return 0;
}
