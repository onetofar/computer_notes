// @book: C++ Concurrency in Action
// @topic: thread-vs-async-future
// @source: C++ Concurrency in Action/Chapter 2. Managing threads（讲义）.md §2.1.2-2.1.3
// @expect: 线程没有返回值通道, 异常必须自己搬运; join 之后才能安全读结果
#include <cstdio>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>

int main() {
    std::mutex m;
    std::exception_ptr worker_err;   // 线程唯一的"回传通道"
    int worker_result = 0;

    std::thread worker([&] {
        try {
            worker_result = 42;
            throw std::runtime_error("worker failed");
        } catch (...) {
            std::lock_guard<std::mutex> lk(m);
            worker_err = std::current_exception();
        }
    });

    // 不 join 就往下走: worker_result 是竞态, 且 worker 可能还在跑
    worker.join();
    std::printf("thread: result=%d\n", worker_result);

    if (worker_err) {
        try {
            std::rethrow_exception(worker_err);
        } catch (const std::exception& e) {
            std::printf("thread: 异常自己搬回主线程 -> %s\n", e.what());
        }
    }
    std::printf("thread: join 后 joinable=%s\n", worker.joinable() ? "true" : "false");
    return 0;
}
