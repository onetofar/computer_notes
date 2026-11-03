// @book: Effective C++
// @topic: polymorphic-base-dtor
// @source: Item 7 在 polymorphic base classes（多态基类）中将 destructors（析构函数）声明为 virtual（虚拟）（讲义）.md Item 7
// @expect: 给基类加 virtual 析构 -> delete 基类指针时整个对象被析构, 派生资源不泄漏
#include <cstdio>

namespace {

int g_derived_destroyed = 0;

class Resource {
public:
    Resource() : p_(new int[64]) { std::printf("    [Resource] ctor\n"); }
    ~Resource() { std::printf("    [Resource] dtor\n"); delete[] p_; }

private:
    int* p_;
};

class TimeKeeper {
public:
    virtual ~TimeKeeper() { std::printf("    [TimeKeeper] dtor\n"); }   // 虚析构
    virtual void tick() {}
};

class AtomicClock : public TimeKeeper {
public:
    AtomicClock() { std::printf("    [AtomicClock] ctor\n"); }
    ~AtomicClock() override {
        std::printf("    [AtomicClock] dtor\n");
        ++g_derived_destroyed;
    }

private:
    Resource res_;
};

}  // namespace

int main() {
    TimeKeeper* ptk = new AtomicClock();
    std::printf("delete via TimeKeeper* ...\n");
    delete ptk;

    if (g_derived_destroyed == 1) {
        std::printf("派生析构被调用了, 派生持有的资源也一并释放\n");
        return 0;
    }
    std::printf("runtime error: 派生析构没有被调用\n");
    return 1;
}
