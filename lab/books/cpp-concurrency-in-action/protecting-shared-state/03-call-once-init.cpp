// @book: C++ Concurrency in Action
// @topic: protecting-shared-state
// @source: C++ Concurrency in Action/Chapter 3. Sharing data between threads（讲义）.md §3.3.1
// @expect: call_once 保证初始化恰好一次, 且返回时资源一定已被正确同步地初始化
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

struct Resource {
    int id;
    explicit Resource(int i) : id(i) { std::printf("Resource(%d) 构造\n", id); }
};

std::once_flag g_flag;
Resource* g_resource = nullptr;

void init_resource() {
    g_resource = new Resource(7);      // 只会被执行一次
}

int main() {
    constexpr int kThreads = 8, kIters = 50000;
    auto t0 = std::chrono::steady_clock::now();

    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t) {
        ts.emplace_back([] {
            for (int i = 0; i < kIters; ++i) {
                std::call_once(g_flag, init_resource);   // 第一次真跑, 之后只查标志
            }
            std::printf("线程看到 id=%d\n", g_resource->id);
        });
    }
    for (auto& t : ts) t.join();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();

    std::printf("call_once: id=%d 耗时=%lld us\n", g_resource->id, (long long)us);
    delete g_resource;
    return 0;
}
