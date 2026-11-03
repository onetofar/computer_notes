// @book: Linux多线程服务端编程
// @topic: reactor-models
// @source: 第3章 多线程服务器的适用场合与常用编程模型（讲义）.md §3.2 单线程服务器的常用编程模型
// @expect: 单线程串行处理队列, 慢任务拖住后续所有任务, 打印每任务延迟
#include <chrono>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>

int main() {
    // 模拟一个单线程 event loop: 队列里一个慢任务 + 若干快任务
    std::deque<std::pair<int, int>> tasks{{1, 50}, {2, 1}, {3, 1}, {4, 1}};
    long long total_us = 0;

    for (auto& [id, cost_ms] : tasks) {
        auto t0 = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(cost_ms));  // 回调不许阻塞
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::steady_clock::now() - t0).count();
        total_us += us;
        std::printf("[loop] 任务%d 完成, 耗时=%lld us\n", id, (long long)us);
    }

    std::printf("单线程串行: 总耗时=%lld us (一个慢任务拖住后面所有任务)\n", (long long)total_us);
    return 0;
}
