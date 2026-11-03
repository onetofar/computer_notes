// @book: Linux多线程服务端编程
// @topic: copy-on-write-shared-ptr
// @source: 第2章 线程同步精要（讲义）.md §2.8
// @expect: 复制—修改—替换拆到两个临界区, 两个写者都基于同一份旧值, 丢失更新, 最终 size=1
#include <barrier>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using FooList = std::vector<int>;
using FooListPtr = std::shared_ptr<FooList>;

std::mutex g_mu;
FooListPtr g_foos = std::make_shared<FooList>();

// 教材里的"错误三": 把复制放到临界区之外
void post_broken(int v, std::barrier<>& gate) {
    FooListPtr oldFoos;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        oldFoos = g_foos;
    }
    // 两个写者都走到这里, 各自读到的是同一份旧值 -> 后写的覆盖先写的
    gate.arrive_and_wait();
    FooListPtr newFoos(new FooList(*oldFoos));
    newFoos->push_back(v);
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_foos = newFoos;
    }
}

int main() {
    std::barrier<> gate(2);
    std::thread a(post_broken, 1, std::ref(gate));
    std::thread b(post_broken, 2, std::ref(gate));
    a.join();
    b.join();

    std::printf("最终 size=%lu (正确应为 2; 丢失更新说明复制—修改—替换不原子)\n",
                (unsigned long)g_foos->size());
    return 0;
}
