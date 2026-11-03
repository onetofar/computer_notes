// @book: Linux多线程服务端编程
// @topic: condition-variable-predicate
// @source: 第2章 线程同步精要（讲义）.md §2.2
// @expect: 消费者误用 if 而非 while, 虚假唤醒后读到已被取空的槽位 -> 重复释放同一块内存
// @ub: yes
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

std::mutex g_mu;
std::condition_variable g_cv;
std::vector<char*> g_queue;  // 槽位里放裸指针, 所有权跟着元素走
bool g_done = false;

int main() {
    std::thread producer([&] {
        {
            std::lock_guard<std::mutex> lk(g_mu);
            g_queue.push_back(new char[64]);
        }
        g_cv.notify_all();

        // 故意制造的虚假唤醒: 条件此时已经不成立, 但 notify 依然发出
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        g_cv.notify_all();
    });

    std::thread consumer([&] {
        for (int round = 0; round < 2; ++round) {
            std::unique_lock<std::mutex> lk(g_mu);
            // 错误写法: 用了 if, 醒来后不再检查条件
            if (g_queue.empty()) {
                g_cv.wait(lk);
            }
            // 第二次醒来时队列已经被取空, front() 读到的是已失效的槽位
            char* p = g_queue.front();
            g_queue.pop_back();
            lk.unlock();
            std::printf("[consumer] 第%d轮拿到指针 %p\n", round, static_cast<void*>(p));
            delete[] p;  // 第二次醒来时释放的是同一块内存 -> 双重释放
        }
    });

    producer.join();
    consumer.join();
    return 0;
}
