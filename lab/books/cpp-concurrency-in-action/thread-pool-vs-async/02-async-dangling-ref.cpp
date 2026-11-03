// @book: C++ Concurrency in Action
// @topic: thread-pool-vs-async
// @source: C++ Concurrency in Action/Chapter 2. Managing threads（讲义）.md §2.2
// @expect: 线程捕获的是裸指针, 指向的对象先死 => 悬垂访问 (UB); 程序检测到生命周期违规后非零退出
// @ub: yes
#include <chrono>
#include <cstdio>
#include <future>
#include <thread>

int main() {
    int* p = new int(7);
    std::future<int> f = std::async(std::launch::async, [p] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));   // 确保主线程先走
        return *p;                                                   // 悬垂: p 已被 delete
    });
    delete p;                                                        // 指针被复制了, 对象没有

    int v = f.get();   // 这一行本身就是 UB: 读的是已释放的对象
    std::printf("runtime error: 悬垂读 —— 读的是已 delete 的对象 (期望 7, 实际 %d)\n", v);
    return 1;
}
