// @book: C++ Concurrency in Action
// @topic: protecting-shared-state
// @source: C++ Concurrency in Action/Chapter 3. Sharing data between threads（讲义）.md §3.3.1
// @expect: 手工 once flag 也能保证只初始化一次, 但每次调用都要过一次 mutex
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

struct Resource {
    int id;
    explicit Resource(int i) : id(i) { std::printf("Resource(%d) 构造\n", id); }
};

std::mutex g_m;
bool g_initialized = false;
Resource* g_resource = nullptr;

void init_resource() {
    std::lock_guard<std::mutex> lk(g_m);      // 每次调用都要拿锁
    if (!g_initialized) {
        g_resource = new Resource(7);
        g_initialized = true;
    }
}

int main() {
    constexpr int kThreads = 8, kIters = 50000;
    auto t0 = std::chrono::steady_clock::now();

    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t) {
        ts.emplace_back([] {
            for (int i = 0; i < kIters; ++i) {
                init_resource();
            }
            std::printf("线程看到 id=%d\n", g_resource->id);
        });
    }
    for (auto& t : ts) t.join();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();

    std::printf("manual-once: id=%d 耗时=%lld us\n", g_resource->id, (long long)us);
    delete g_resource;
    return 0;
}
