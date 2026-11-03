// @book: C++ Concurrency in Action
// @topic: condition-variable-family
// @source: C++ Concurrency in Action/Chapter 4. Synchronizing concurrent operations（讲义）.md §4.3.4
// @expect: 循环里用 wait_for 每次假醒都重新计时, 总等待被拉长; 用一次算好的绝对 time_point 才有界
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

int main() {
    std::mutex m;
    std::condition_variable cv;
    bool done = false;                            // 条件始终不成立

    // 假醒风暴: 只 notify, 从不改条件
    std::thread notifier([&] {
        auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
        while (std::chrono::steady_clock::now() < until) {
            cv.notify_all();
            std::this_thread::yield();
        }
    });

    // A: 循环里用 duration —— 每次假醒都把 20ms 重新计一遍
    auto t0 = std::chrono::steady_clock::now();
    {
        std::unique_lock<std::mutex> lk(m);
        while (!done) {
            if (cv.wait_for(lk, std::chrono::milliseconds(20)) == std::cv_status::timeout) break;
        }
    }
    auto ms_a = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - t0).count();

    // B: 一次算好的绝对时间点 —— 总时长有界
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(20);
    t0 = std::chrono::steady_clock::now();
    {
        std::unique_lock<std::mutex> lk(m);
        while (!done) {
            if (cv.wait_until(lk, deadline) == std::cv_status::timeout) break;
        }
    }
    auto ms_b = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - t0).count();

    notifier.join();
    std::printf("cv: wait_for 循环共等了 %lld ms, wait_until 绝对时间点共等了 %lld ms\n",
                (long long)ms_a, (long long)ms_b);
    return 0;
}
