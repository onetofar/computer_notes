// @book: C++ Concurrency in Action
// @topic: mutex-family
// @source: C++ Concurrency in Action/Chapter 3. Sharing data between threads（讲义）.md §3.2.1-3.2.2
// @expect: 手工 lock/unlock 在异常路径上会漏解锁; lock_guard 靠析构兜住
#include <cstdio>
#include <mutex>
#include <stdexcept>

int main() {
    std::mutex m;

    try {
        m.lock();
        throw std::runtime_error("boom");
    } catch (const std::exception& e) {
        m.unlock();                       // 手工写法: 解锁得自己记得写
        std::printf("mutex: 手工 unlock 兜住异常 (%s)\n", e.what());
    }
    bool got = m.try_lock();
    std::printf("mutex: 手工路径之后 try_lock=%s\n", got ? "true" : "false");
    if (got) m.unlock();

    try {
        std::lock_guard<std::mutex> lk(m);   // 出作用域自动解锁
        throw std::runtime_error("boom2");
    } catch (const std::exception& e) {
        std::printf("mutex: lock_guard 自动解锁, 异常照常抛出 (%s)\n", e.what());
    }
    got = m.try_lock();
    std::printf("mutex: RAII 路径之后 try_lock=%s\n", got ? "true" : "false");
    if (got) m.unlock();
    return 0;
}
