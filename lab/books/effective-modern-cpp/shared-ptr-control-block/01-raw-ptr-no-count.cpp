// @book: Effective Modern C++
// @topic: shared-ptr-control-block
// @source: Effective Modern C++/4. Smart Pointers（讲义）.md §Item 19
// @expect: 裸指针没人负责 delete -> 泄漏; 程序正常退出
#include <cstdio>

struct Widget {
    int id;
    explicit Widget(int i) : id(i) { std::printf("Widget(%d) ctor\n", id); }
    ~Widget() { std::printf("Widget(%d) dtor\n", id); }
};

int main() {
    Widget* raw = new Widget(1);
    std::printf("裸指针: 谁负责 delete? 这里故意漏掉 -> 泄漏\n");
    (void)raw;
    return 0;
}
