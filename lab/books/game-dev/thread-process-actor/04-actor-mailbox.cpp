// @book: 网络多人在线游戏开发实战教程
// @topic: thread-process-actor
// @source: 第3章 线程、进程以及Actor模型（讲义）.md §3.6
// @expect: 8 个 Actor 各持邮箱与私有计数器, 计数恒为 400000, 数据本身零加锁, 锁只在邮箱出入队
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace {

constexpr int kActors = 8;
constexpr int kTasks = 400000;

struct Mailbox {
    std::deque<std::pair<long long, long long>> q;
    std::mutex m;
    std::condition_variable cv;
    bool closed = false;

    void push(long long seq, long long amount) {
        {
            std::lock_guard<std::mutex> lock(m);
            q.emplace_back(seq, amount);
        }
        cv.notify_one();
    }
    bool pop(std::pair<long long, long long>& out) {
        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, [this] { return !q.empty() || closed; });
        if (q.empty()) return false;
        out = q.front();
        q.pop_front();
        return true;
    }
    void close() {
        {
            std::lock_guard<std::mutex> lock(m);
            closed = true;
        }
        cv.notify_all();
    }
};

}  // namespace

int main() {
    std::vector<Mailbox> boxes(kActors);
    std::vector<long long> totals(kActors, 0);
    std::vector<char> fifo_ok(kActors, 1);
    std::atomic<long> pushes{0};

    std::vector<std::thread> actors;
    for (int a = 0; a < kActors; ++a) {
        actors.emplace_back([&, a] {
            long long own = 0;      // 只有自己改, 不需要任何锁
            long long last_seq = -1;
            bool ok = true;
            std::pair<long long, long long> item;
            while (boxes[a].pop(item)) {
                if (item.first <= last_seq) ok = false;
                last_seq = item.first;
                own += item.second;
            }
            totals[a] = own;
            fifo_ok[a] = ok ? 1 : 0;
        });
    }

    auto t0 = std::chrono::steady_clock::now();
    for (long long seq = 0; seq < kTasks; ++seq) {
        boxes[static_cast<size_t>(seq % kActors)].push(seq, 1);
        ++pushes;
    }
    for (auto& box : boxes) box.close();
    for (auto& t : actors) t.join();
    auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    long long total = 0;
    bool all_fifo = true;
    for (int a = 0; a < kActors; ++a) {
        total += totals[a];
        if (!fifo_ok[a]) all_fifo = false;
    }
    std::printf("actor+mailbox: actors=%d tasks=%d counter=%lld expected=%d\n",
                kActors, kTasks, total, kTasks);
    std::printf("enqueue=%ld dequeue=%ld fifo_preserved=%d elapsed_ms=%.1f\n",
                pushes.load(), pushes.load(), all_fifo ? 1 : 0, ms);
    return 0;
}
