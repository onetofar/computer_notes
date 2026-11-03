// @book: Linux多线程服务端编程
// @topic: condition-variable-predicate
// @source: 第2章 线程同步精要（讲义）.md §2.2 条件变量
// @expect: 3 个消费者在等资源, notify_one 只唤醒 1 个, notify_all 唤醒全部 3 个
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

std::mutex g_mu;
std::condition_variable g_cv;
int g_resources = 0;
int g_waiting = 0;
std::atomic<int> g_woken{0};
std::atomic<int> g_taken{0};

void consumer(int id) {
    std::unique_lock<std::mutex> lk(g_mu);
    ++g_waiting;
    g_cv.wait(lk, [&] { return g_resources > 0; });  // 资源为 0 时全部阻塞
    g_woken.fetch_add(1);
    --g_resources;
    g_taken.fetch_add(1);
    std::printf("[consumer-%d] 拿到一个资源, 剩余=%d\n", id, g_resources);
}

int main() {
    std::vector<std::thread> ts;
    for (int i = 0; i < 3; ++i) ts.emplace_back(consumer, i);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_resources = 1;             // 一个资源可用
        g_cv.notify_one();           // 资源可用 -> signal 一个
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::printf("notify_one 之后: 被唤醒=%d 取走=%d (仍在等待的=%d)\n", g_woken.load(),
                g_taken.load(), g_waiting - g_taken.load());

    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_resources = 3;             // 状态整体变化
        g_cv.notify_all();           // 状态变化 -> broadcast 全部
    }
    for (auto& t : ts) t.join();
    std::printf("notify_all 之后: 被唤醒=%d 取走=%d (剩余资源=%d)\n", g_woken.load(),
                g_taken.load(), g_resources);
    return 0;
}
