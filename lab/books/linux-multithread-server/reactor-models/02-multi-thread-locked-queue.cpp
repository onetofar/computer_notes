// @book: Linux多线程服务端编程
// @topic: reactor-models
// @source: 第3章 多线程服务器的适用场合与常用编程模型（讲义）.md §3.3.2 线程池
// @expect: 多线程抢同一个带锁队列, 快任务先完成, 慢任务不阻塞别人
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

std::mutex g_mu;
std::condition_variable g_cv;
std::deque<std::pair<int, int>> g_tasks{{1, 50}, {2, 1}, {3, 1}, {4, 1}};
bool g_done = false;
std::mutex g_print_mu;

void worker(int id) {
    for (;;) {
        std::pair<int, int> task;
        {
            std::unique_lock<std::mutex> lk(g_mu);
            g_cv.wait(lk, [] { return !g_tasks.empty() || g_done; });
            if (g_tasks.empty()) return;
            task = g_tasks.front();
            g_tasks.pop_front();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(task.second));
        std::lock_guard<std::mutex> lk(g_print_mu);
        std::printf("[worker-%d] 任务%d 完成\n", id, task.first);
    }
}

int main() {
    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> ts;
    for (int i = 0; i < 4; ++i) ts.emplace_back(worker, i);
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_done = true;
    }
    g_cv.notify_all();
    for (auto& t : ts) t.join();

    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();
    std::printf("多线程加锁队列: 总耗时=%lld us (慢任务不再拖住快任务)\n", (long long)us);
    return 0;
}
