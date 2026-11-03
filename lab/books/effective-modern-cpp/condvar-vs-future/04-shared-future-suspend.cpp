// @book: Effective Modern C++
// @topic: condvar-vs-future
// @source: Effective Modern C++/7. The Concurrency API（讲义）.md §Item 39 · 用 void future 挂起一个线程 / shared_future 放行多个线程
// @expect: void future 可以"先创建线程、后放行"(线程先建好, 此刻挂起); shared_future 一次放行多个线程
#include <chrono>
#include <cstdio>
#include <future>
#include <thread>
#include <vector>

int main() {
    std::promise<void> gate;
    std::thread worker([&] {
        gate.get_future().wait();        // 挂起: 等 promise 被 set
        std::printf("worker 真正开始跑\n");
    });
    std::printf("线程已创建, 此刻是挂起状态\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    gate.set_value();                    // 放行
    worker.join();

    std::promise<void> p;
    auto sf = p.get_future().share();   // 多个等待方共享同一个共享状态
    std::vector<std::thread> vt;
    for (int i = 0; i < 3; ++i) {
        vt.emplace_back([sf, i] {
            sf.wait();
            std::printf("线程 %d 被放行\n", i);
        });
    }
    p.set_value();                      // 一次放行所有线程
    for (auto& t : vt) t.join();
    return 0;
}
