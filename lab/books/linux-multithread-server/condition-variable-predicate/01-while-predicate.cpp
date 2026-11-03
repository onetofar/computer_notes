// @book: Linux多线程服务端编程
// @topic: condition-variable-predicate
// @source: 第2章 线程同步精要（讲义）.md §2.2 条件变量
// @expect: 生产者发 3 条, 消费者用 while + 谓词等待, 全部收到且顺序正确
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>

std::mutex g_mu;
std::condition_variable g_cv;
std::deque<int> g_queue;
bool g_done = false;

int main() {
    std::thread producer([&] {
        for (int i = 1; i <= 3; ++i) {
            {
                std::lock_guard<std::mutex> lk(g_mu);
                g_queue.push_back(i);
            }
            g_cv.notify_one();
        }
        {
            std::lock_guard<std::mutex> lk(g_mu);
            g_done = true;
        }
        g_cv.notify_all();
    });

    std::thread consumer([&] {
        int got = 0;
        for (;;) {
            std::unique_lock<std::mutex> lk(g_mu);
            // 必须用 while: 虚假唤醒时条件可能已不成立
            while (g_queue.empty() && !g_done) {
                g_cv.wait(lk);
            }
            if (g_queue.empty() && g_done) break;
            int v = g_queue.front();
            g_queue.pop_front();
            lk.unlock();
            ++got;
            std::printf("[consumer] 收到 %d\n", v);
        }
        std::printf("[consumer] 共收到 %d 条\n", got);
    });

    producer.join();
    consumer.join();
    return 0;
}
