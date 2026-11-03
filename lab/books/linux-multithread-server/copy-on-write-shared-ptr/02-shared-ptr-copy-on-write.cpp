// @book: Linux多线程服务端编程
// @topic: copy-on-write-shared-ptr
// @source: 第2章 线程同步精要（讲义）.md §2.8
// @expect: 读者只在临界区内拷贝一次 shared_ptr, 遍历期间不持锁, 写者几乎不被阻塞
#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using FooList = std::vector<int>;
using FooListPtr = std::shared_ptr<FooList>;

std::mutex g_mu;
FooListPtr g_foos = std::make_shared<FooList>();

int main() {
    for (int i = 0; i < 1000; ++i) g_foos->push_back(i);

    std::thread reader([&] {
        FooListPtr foos;
        {
            std::lock_guard<std::mutex> lk(g_mu);
            foos = g_foos;  // 临界区里只有这一行: 引用计数 +1
        }
        // 出了临界区, 读者不再需要锁, 写者也永远不会被读者阻塞
        long long sum = 0;
        for (int v : *foos) sum += v;
        std::printf("[reader] sum=%lld (已退出临界区, 遍历期间不持锁)\n", sum);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));  // 模拟长遍历
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto t1 = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lk(g_mu);
        // unique() 为 false 说明还有读者, 复制一份在副本上改, 再替换
        if (!g_foos.unique()) {
            g_foos.reset(new FooList(*g_foos));
            std::printf("[writer] 引用计数>1, 复制整份列表后在副本上修改\n");
        }
        g_foos->push_back(1000);
    }
    auto tw = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t1).count();

    reader.join();
    std::printf("copy-on-write: 写者等待=%lld us (读者不在临界区, 不被阻塞), size=%lu\n",
                (long long)tw, (unsigned long)g_foos->size());
    return 0;
}
