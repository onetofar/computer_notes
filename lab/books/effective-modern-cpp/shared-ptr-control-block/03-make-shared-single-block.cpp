// @book: Effective Modern C++
// @topic: shared-ptr-control-block
// @source: Effective Modern C++/4. Smart Pointers（讲义）.md §Item 19 §Item 21
// @expect: make_shared 只造一块控制块, use_count=1, 无 sanitizer 报错
#include <cstdio>
#include <memory>

struct Widget {
    int id;
    explicit Widget(int i) : id(i) { std::printf("Widget(%d) ctor\n", id); }
    ~Widget() { std::printf("Widget(%d) dtor\n", id); }
};

int main() {
    auto sp = std::make_shared<Widget>(7);
    std::printf("use_count=%ld (一块控制块)\n", sp.use_count());
    {
        auto sp2 = sp;
        std::printf("拷贝后 use_count=%ld\n", sp.use_count());
    }
    std::printf("sp2 析构后 use_count=%ld\n", sp.use_count());
    return 0;
}
