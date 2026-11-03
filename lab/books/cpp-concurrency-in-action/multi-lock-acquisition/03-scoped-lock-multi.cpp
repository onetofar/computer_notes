// @book: C++ Concurrency in Action
// @topic: multi-lock-acquisition
// @source: C++ Concurrency in Action/Chapter 3. Sharing data between threads（讲义）.md §3.2.4
// @expect: 与 02 同一段逻辑, 换成 C++17 的 scoped_lock, 一次锁住两把且析构时全部解锁
#include <cstdio>
#include <mutex>
#include <thread>

struct Account {
    std::mutex m;
    int balance = 0;
};

void transfer(Account& a, Account& b, int amount) {
    if (&a == &b) return;
    std::scoped_lock lk(a.m, b.m);               // 变参模板: 一次锁住两把, 析构全部解锁
    a.balance -= amount;
    b.balance += amount;
}

int main() {
    Account x, y;
    std::thread t1([&] { for (int i = 0; i < 1000; ++i) transfer(x, y, 1); });
    std::thread t2([&] { for (int i = 0; i < 1000; ++i) transfer(y, x, 1); });  // 参数顺序反转
    t1.join();
    t2.join();
    std::printf("scoped_lock: x=%d y=%d (两者之和应为 0)\n", x.balance, y.balance);
    return 0;
}
