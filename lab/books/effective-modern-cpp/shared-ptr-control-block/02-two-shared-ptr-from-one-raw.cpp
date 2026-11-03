// @book: Effective Modern C++
// @topic: shared-ptr-control-block
// @source: Effective Modern C++/4. Smart Pointers（讲义）.md §Item 19
// @expect: 同一裸指针造两个 shared_ptr -> 两块控制块 -> double free, ASan 报错
// @ub: yes
#include <cstdio>
#include <memory>

struct Widget {
    int id;
    explicit Widget(int i) : id(i) { std::printf("Widget(%d) ctor\n", id); }
    ~Widget() { std::printf("Widget(%d) dtor\n", id); }
};

int main() {
    Widget* raw = new Widget(7);
    std::shared_ptr<Widget> a(raw);   // 第一块控制块
    std::shared_ptr<Widget> b(raw);   // 第二块控制块 —— 教材说的 UB
    std::printf("use_count(a)=%ld use_count(b)=%ld\n", a.use_count(), b.use_count());
    std::printf("两块控制块各计 1 次, 谁都不知道对方存在 -> 析构两次\n");
    return 0;
}
