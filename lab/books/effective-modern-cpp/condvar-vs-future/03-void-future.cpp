// @book: Effective Modern C++
// @topic: condvar-vs-future
// @source: Effective Modern C++/7. The Concurrency API（讲义）.md §Item 39 · 方案四 · void future（一次性事件）
// @expect: promise<void>/future<void>: 一次性事件通知, 不用锁, 通知早于等待也不丢(先 set_value 再 wait 立刻返回), 等待方是真阻塞
#include <chrono>
#include <cstdio>
#include <future>
#include <thread>

int main() {
    // 情形 A: 先通知, 后等待
    std::promise<void> p1;
    p1.set_value();
    auto f1 = p1.get_future();
    const auto t0 = std::chrono::steady_clock::now();
    f1.wait();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0).count();
    std::printf("先通知后等待: 立刻返回, 耗时 %lld ms\n", static_cast<long long>(ms));

    // 情形 B: 等待方先阻塞, 检测方后放行
    std::promise<void> p2;
    auto f2 = p2.get_future();
    std::thread t([&] {
        f2.wait();
        std::printf("等待方被放行\n");
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    p2.set_value();
    t.join();
    return 0;
}
