// @book: C++ Concurrency in Action
// @topic: multi-lock-acquisition
// @source: C++ Concurrency in Action/Chapter 3. Sharing data between threads（讲义）.md §3.2.4
// @expect: std::lock 一次拿全两把锁, 参数顺序反转也不死锁; 用 adopt_lock 接管所有权
#include <cstdio>
#include <mutex>
#include <thread>

struct Account {
    std::mutex m;
    int balance = 0;
};

void transfer(Account& a, Account& b, int amount) {
    if (&a == &b) return;                        // 对同一把锁重复加锁是 UB
    std::lock(a.m, b.m);                         // 一次拿全, 无死锁
    std::lock_guard<std::mutex> la(a.m, std::adopt_lock);   // 锁已经在我手上, 只接管
    std::lock_guard<std::mutex> lb(b.m, std::adopt_lock);
    a.balance -= amount;
    b.balance += amount;
}

int main() {
    Account x, y;
    std::thread t1([&] { for (int i = 0; i < 1000; ++i) transfer(x, y, 1); });
    std::thread t2([&] { for (int i = 0; i < 1000; ++i) transfer(y, x, 1); });  // 参数顺序反转
    t1.join();
    t2.join();
    std::printf("std::lock: x=%d y=%d (两者之和应为 0)\n", x.balance, y.balance);
    return 0;
}
