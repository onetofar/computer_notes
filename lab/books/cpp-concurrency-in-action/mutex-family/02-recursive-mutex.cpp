// @book: C++ Concurrency in Action
// @topic: mutex-family
// @source: C++ Concurrency in Action/Chapter 3. Sharing data between threads（讲义）.md §3.3.3
// @expect: recursive_mutex 允许同线程重复加锁, 但必须加几次解几次; 普通 mutex 上重复加锁是 UB
#include <cstdio>
#include <mutex>

int main() {
    std::recursive_mutex rm;
    rm.lock();                                        // 第 1 次
    {
        std::lock_guard<std::recursive_mutex> inner(rm);   // 第 2 次, 同一个线程
        bool again = rm.try_lock();                   // 第 3 次, 递归锁允许
        std::printf("recursive_mutex: 同线程第 3 次 try_lock=%s\n", again ? "true" : "false");
        if (again) rm.unlock();                       // 加了几次就解几次
    }                                                 // inner 析构: 解掉第 2 次
    rm.unlock();                                     // 解掉第 1 次

    bool got = rm.try_lock();
    std::printf("recursive_mutex: 全部解锁后 try_lock=%s\n", got ? "true" : "false");
    if (got) rm.unlock();
    return 0;
}
