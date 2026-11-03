// @book: C++ Concurrency in Action
// @topic: condition-variable-family
// @source: C++ Concurrency in Action/Chapter 4. Synchronizing concurrent operations（讲义）.md §4.1.1-4.1.2
// @expect: 谓词式 wait 把循环收进标准库, 假醒也不会出错; 生产者 notify_one
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <queue>
#include <thread>

int main() {
    std::mutex m;
    std::condition_variable cv;
    std::queue<int> q;
    bool done = false;

    std::thread producer([&] {
        for (int i = 1; i <= 5; ++i) {
            {
                std::lock_guard<std::mutex> lk(m);
                q.push(i);
            }
            cv.notify_one();
        }
        {
            std::lock_guard<std::mutex> lk(m);
            done = true;
        }
        cv.notify_all();
    });

    int sum = 0, got = 0;
    std::thread consumer([&] {
        for (;;) {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [&] { return done || !q.empty(); });   // 谓词式: 假醒也安全
            if (!q.empty()) { sum += q.front(); q.pop(); ++got; continue; }
            if (done) break;
        }
    });

    producer.join();
    consumer.join();
    std::printf("cv: 收到 %d 个, 和=%d\n", got, sum);
    return 0;
}
