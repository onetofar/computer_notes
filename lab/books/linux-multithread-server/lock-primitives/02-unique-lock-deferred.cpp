// @book: Linux多线程服务端编程
// @topic: lock-primitives
// @source: 第2章 线程同步精要（讲义）.md §2.4 封装MutexLock、MutexLockGuard、Condition
// @expect: unique_lock 支持 defer_lock 与中途 unlock, 打印 提前解锁后锁已释放
#include <chrono>
#include <cstdio>
#include <mutex>

std::mutex g_mu;
int g_counter = 0;

int main() {
    // unique_lock 的三种构造: 直接加锁 / defer_lock / try_lock
    {
        std::unique_lock<std::mutex> lk(g_mu);
        ++g_counter;
        std::printf("unique_lock(直接加锁): owns=%d counter=%d\n", (int)lk.owns_lock(),
                    g_counter);
    }

    {
        std::unique_lock<std::mutex> lk(g_mu, std::defer_lock);
        std::printf("unique_lock(defer_lock): 刚构造 owns=%d (还没加锁)\n",
                    (int)lk.owns_lock());
        lk.lock();
        std::printf("  显式 lock() 后 owns=%d\n", (int)lk.owns_lock());
        ++g_counter;

        lk.unlock();  // lock_guard 做不到这件事: 中途放弃锁
        std::printf("  中途 unlock() 后 owns=%d (临界区提前结束)\n", (int)lk.owns_lock());
    }

    {
        std::unique_lock<std::mutex> lk(g_mu, std::try_to_lock);
        std::printf("unique_lock(try_to_lock): owns=%d\n", (int)lk.owns_lock());
    }

    std::printf("counter=%d\n", g_counter);
    return 0;
}
