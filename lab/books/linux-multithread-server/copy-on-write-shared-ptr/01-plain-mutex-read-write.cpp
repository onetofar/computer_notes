// @book: Linux多线程服务端编程
// @topic: copy-on-write-shared-ptr
// @source: 第2章 线程同步精要（讲义）.md §2.8 借shared_ptr实现copy-on-write
// @expect: 读者整个遍历期间都持锁, 写者必须等读者放锁才能写入, 写者等待时间约为读者遍历耗时
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

std::mutex g_mu;
std::vector<int> g_foos;

int main() {
    for (int i = 0; i < 1000; ++i) g_foos.push_back(i);

    std::thread reader([&] {
        std::lock_guard<std::mutex> lk(g_mu);  // 整个遍历期间都持锁
        long long sum = 0;
        for (int v : g_foos) sum += v;
        std::printf("[reader] sum=%lld (持锁期间遍历完毕)\n", sum);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));  // 模拟长遍历
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto t1 = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lk(g_mu);  // 写者必须等读者放锁
        g_foos.push_back(1000);
    }
    auto tw = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t1).count();

    reader.join();
    std::printf("普通 mutex: 写者等待=%lld us (读者持锁, 写者被阻塞), size=%lu\n", (long long)tw,
                (unsigned long)g_foos.size());
    return 0;
}
