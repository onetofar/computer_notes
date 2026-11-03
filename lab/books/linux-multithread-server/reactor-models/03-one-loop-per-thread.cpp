// @book: Linux多线程服务端编程
// @topic: reactor-models
// @source: 第3章 多线程服务器的适用场合与常用编程模型（讲义）.md §3.3.1
// @expect: 每线程一个 loop, 任务按归属分派, 同一连接的事件固定在同一线程
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

std::mutex g_print_mu;

void event_loop(int loop_id, int conn_id, int cost_ms) {
    // 每个 loop 只处理分配给自己的连接, 不需要任何锁
    std::this_thread::sleep_for(std::chrono::milliseconds(cost_ms));
    std::lock_guard<std::mutex> lk(g_print_mu);
    std::printf("[loop-%d] 连接%d 的事件处理完毕 (耗时 %d ms)\n", loop_id, conn_id, cost_ms);
}

int main() {
    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> ts;
    // 连接1 归 loop0 (慢), 连接2/3/4 各归一个 loop
    ts.emplace_back(event_loop, 0, 1, 50);
    ts.emplace_back(event_loop, 1, 2, 1);
    ts.emplace_back(event_loop, 2, 3, 1);
    ts.emplace_back(event_loop, 3, 4, 1);
    for (auto& t : ts) t.join();

    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();
    std::printf("one loop per thread: 总耗时=%lld us (同一连接的事件永远在同一线程)\n",
                (long long)us);
    return 0;
}
