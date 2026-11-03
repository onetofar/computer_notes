// @book: Effective C++
// @topic: polymorphic-base-dtor
// @source: Item 7 在 polymorphic base classes（多态基类）中将 destructors（析构函数）声明为 virtual（虚拟）（讲义）.md Item 7
// @expect: 经基类指针 delete -> 派生部分根本没被析构 -> 派生持有的堆内存泄漏
// @ub: yes
#include <cstdio>

// GCC 的 -Wdelete-non-virtual-dtor 属于 -Wall, 这里整篇局部关闭,
// 保证整个 lab 零警告构建; 警告文本正是本文件要演示的东西。
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"

namespace {

int g_derived_destroyed = 0;

// 派生类持有的资源: 只有派生析构跑起来, 它的析构才会跑
class Resource {
public:
    Resource() : p_(new int[64]) { std::printf("    [Resource] ctor\n"); }
    ~Resource() { std::printf("    [Resource] dtor\n"); delete[] p_; }

private:
    int* p_;
};

// 多态基类 (有虚函数), 却没有虚析构 —— 这正是 Item 7 要治的病
class TimeKeeper {
public:
    ~TimeKeeper() { std::printf("    [TimeKeeper] dtor\n"); }   // 非虚析构
    virtual void tick() {}
};

class AtomicClock : public TimeKeeper {
public:
    AtomicClock() { std::printf("    [AtomicClock] ctor\n"); }
    ~AtomicClock() {
        std::printf("    [AtomicClock] dtor\n");
        ++g_derived_destroyed;
    }

private:
    Resource res_;
};

}  // namespace

int main() {
    TimeKeeper* ptk = new AtomicClock();     // 工厂函数返回基类指针
    std::printf("delete via TimeKeeper* ...\n");
    delete ptk;                               // 只跑了基类析构

    if (g_derived_destroyed == 0) {
        std::printf("runtime error: 派生部分根本没有被析构, "
                    "它持有的堆内存泄漏 (undefined behavior)\n");
        return 1;
    }
    std::printf("派生析构被调用了\n");
    return 0;
}
