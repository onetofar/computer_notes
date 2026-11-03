// @book: C++ Concurrency in Action
// @topic: multi-lock-acquisition
// @source: C++ Concurrency in Action/Chapter 3. Sharing data between threads（讲义）.md §3.2.1
// @expect: lock_guard 的作用域就是临界区, 出了作用域自动解锁, 不需要手工 unlock
#include <cstdio>
#include <mutex>
#include <thread>

struct Counter {
    std::mutex m;
    int value = 0;
    void add(int n) {
        std::lock_guard<std::mutex> lk(m);   // 临界区 = 从这一行到函数返回
        value += n;
    }
    int get() {
        std::lock_guard<std::mutex> lk(m);
        return value;
    }
};

int main() {
    Counter c;
    std::thread t([&] { for (int i = 0; i < 1000; ++i) c.add(1); });
    t.join();
    std::printf("lock_guard: value=%d\n", c.get());
    return 0;
}
