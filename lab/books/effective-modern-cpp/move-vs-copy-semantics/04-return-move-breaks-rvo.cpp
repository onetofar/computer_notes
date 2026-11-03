// @book: Effective Modern C++
// @topic: move-vs-copy-semantics
// @source: Effective Modern C++/5. Rvalue References, Move Semantics, and Perfect Forwarding（讲义）.md §Item 25 · return std::move(w) 是反优化
// @expect: return w 走 NRVO(拷贝0/移动0); return std::move(w) 挡掉 NRVO, 多一次移动构造(移动1)
#include <cstdio>
#include <utility>

struct Widget {
    int id;
    static int copies;
    static int moves;

    explicit Widget(int i) : id(i) {}
    Widget(const Widget& o) : id(o.id) { ++copies; }
    Widget(Widget&& o) noexcept : id(o.id) { ++moves; }
};

int Widget::copies = 0;
int Widget::moves = 0;

Widget makeByRvo() {
    Widget w{1};
    return w;                  // NRVO: 返回对象就是 w, 一次构造都不需要
}

// 编译器自己会警告这件事(-Wpessimizing-move), 这里局部静音好让整个目录零警告
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wpessimizing-move"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wpessimizing-move"
#endif
Widget makeByMove() {
    Widget w{2};
    return std::move(w);       // 挡掉 NRVO: 返回的是 Widget&&, 必须真的移动一次
}
#if defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

int main() {
    Widget a = makeByRvo();
    std::printf("return w           : 拷贝=%d 移动=%d id=%d\n", Widget::copies, Widget::moves, a.id);

    Widget::copies = 0;
    Widget::moves = 0;
    Widget b = makeByMove();
    std::printf("return std::move(w): 拷贝=%d 移动=%d id=%d\n", Widget::copies, Widget::moves, b.id);
    return 0;
}
