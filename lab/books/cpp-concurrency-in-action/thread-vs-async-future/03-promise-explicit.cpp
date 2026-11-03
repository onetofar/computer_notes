// @book: C++ Concurrency in Action
// @topic: thread-vs-async-future
// @source: C++ Concurrency in Action/Chapter 4. Synchronizing concurrent operations（讲义）.md §4.2.3-4.2.4
// @expect: promise 由持有者亲手兑现; 未兑现就析构 => future 拿到 broken_promise 异常
#include <cstdio>
#include <exception>
#include <future>
#include <stdexcept>
#include <thread>

int main() {
    std::promise<int> p;
    std::future<int> f = p.get_future();
    std::thread worker([pr = std::move(p)]() mutable { pr.set_value(99); });
    worker.join();
    std::printf("promise: set_value 后 get() = %d\n", f.get());

    std::promise<int> p2;
    std::future<int> f2 = p2.get_future();
    p2.set_exception(std::make_exception_ptr(std::runtime_error("explicit failure")));
    try {
        (void)f2.get();
    } catch (const std::exception& e) {
        std::printf("promise: set_exception -> %s\n", e.what());
    }

    std::promise<int> p3;
    std::future<int> f3 = p3.get_future();
    { std::promise<int> dying = std::move(p3); }   // 析构时一个值也没兑现
    try {
        (void)f3.get();
    } catch (const std::exception& e) {
        std::printf("promise: 未兑现就析构 -> %s\n", e.what());
    }
    return 0;
}
