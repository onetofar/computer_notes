// @book: C++ Concurrency in Action
// @topic: condition-variable-family
// @source: C++ Concurrency in Action/附录 D. C++ Thread Library reference（讲义）.md §D.2
// @expect: condition_variable_any 接受任意 BasicLockable, 这里配 recursive_mutex; condition_variable 不行
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

int main() {
    std::recursive_mutex m;                      // 不是 std::mutex
    std::condition_variable_any cv;              // any 版才吃得下这把锁
    bool ready = false;

    std::thread worker([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        {
            std::unique_lock<std::recursive_mutex> lk(m);
            ready = true;
        }
        cv.notify_all();
    });

    {
        std::unique_lock<std::recursive_mutex> lk(m);
        bool ok = cv.wait_for(lk, std::chrono::milliseconds(500), [&] { return ready; });
        std::printf("cv_any: wait_for 的谓词结果=%s\n", ok ? "true" : "false");
    }
    worker.join();
    std::printf("cv_any: ready=%s (用 recursive_mutex 也能等条件)\n", ready ? "true" : "false");
    return 0;
}
