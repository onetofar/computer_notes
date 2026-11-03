// @book: Effective Modern C++
// @topic: condvar-vs-future
// @source: Effective Modern C++/7. The Concurrency API（讲义）.md §Item 39 · 方案一 · 条件变量
// @expect: 条件变量必须配谓词: 带谓词立刻返回 true, 裸 wait 因通知早于等待而错过(只能超时返回 false)
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>

int main() {
    std::mutex m;
    std::condition_variable cv;
    bool ready = false;

    {                                   // 检测方: 先置位, 再通知
        std::lock_guard<std::mutex> lk(m);
        ready = true;
    }
    cv.notify_one();                    // 通知发生在等待之前

    std::unique_lock<std::mutex> lk(m);
    const bool withPredicate = cv.wait_for(lk, std::chrono::milliseconds(200),
                                            [&] { return ready; });
    std::printf("带谓词等待: %d (谓词已满足, 立刻返回)\n", withPredicate ? 1 : 0);

    const bool withoutPredicate =
        cv.wait_for(lk, std::chrono::milliseconds(200)) != std::cv_status::timeout;
    std::printf("裸等待    : %d (通知早于等待 -> 错过, 只能超时)\n", withoutPredicate ? 1 : 0);
    return 0;
}
