// @book: C++ Concurrency in Action
// @topic: thread-vs-async-future
// @source: C++ Concurrency in Action/Chapter 4. Synchronizing concurrent operations（讲义）.md §4.2.1
// @expect: async 抛出的异常经 future.get() 重抛; launch::deferred 的任务在 get() 时才在当前线程跑
#include <chrono>
#include <cstdio>
#include <future>
#include <stdexcept>
#include <thread>

int main() {
    std::future<int> failing = std::async(std::launch::async, []() -> int {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        throw std::runtime_error("async task failed");
    });
    try {
        (void)failing.get();
        std::printf("async: 不该走到这里\n");
    } catch (const std::exception& e) {
        std::printf("async: get() 重抛异常 -> %s\n", e.what());
    }

    std::future<int> deferred = std::async(std::launch::deferred, [] { return 7; });
    auto st = deferred.wait_for(std::chrono::milliseconds(0));
    std::printf("async-deferred: get 之前 status=%s\n",
                st == std::future_status::deferred ? "deferred" : "other");
    std::printf("async-deferred: get() 返回 %d (在调用 get 的线程上执行)\n", deferred.get());
    return 0;
}
