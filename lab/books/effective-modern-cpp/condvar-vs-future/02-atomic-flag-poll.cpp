// @book: Effective Modern C++
// @topic: condvar-vs-future
// @source: Effective Modern C++/7. The Concurrency API（讲义）.md §Item 39 · 方案二 · 原子 flag（轮询）
// @expect: 原子 flag 轮询: 不用锁/不怕通知早到/不怕伪唤醒, 但等待期间线程仍在运行(统计轮询次数与耗时)
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

int main() {
    std::atomic<bool> flag{false};
    long long polls = 0;

    std::thread setter([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        flag.store(true, std::memory_order_release);
    });

    const auto t0 = std::chrono::steady_clock::now();
    while (!flag.load(std::memory_order_acquire)) {
        ++polls;
        std::this_thread::yield();      // 只让出时间片, 线程并没有真的阻塞
    }
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0).count();

    setter.join();
    std::printf("轮询次数=%lld  等待约 %lld ms  (线程一直在跑, 只是没拿到数据)\n",
                polls, static_cast<long long>(ms));
    return 0;
}
