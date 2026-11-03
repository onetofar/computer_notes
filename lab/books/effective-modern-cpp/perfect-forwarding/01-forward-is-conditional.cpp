// @book: Effective Modern C++
// @topic: perfect-forwarding
// @source: Effective Modern C++/5. Rvalue References, Move Semantics, and Perfect Forwarding（讲义）.md §Item 23 §Item 25
// @expect: 传左值时 std::move 仍把结果变成右值, std::forward 保持左值; 传右值时两者都是右值; 万能引用上无脑 move 会掏空调用方的对象
#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>

struct Widget {
    std::string tag;
    static int copies;
    static int moves;

    explicit Widget(std::string t) : tag(std::move(t)) {}
    Widget(const Widget& o) : tag(o.tag) { ++copies; }
    Widget(Widget&& o) noexcept : tag(std::move(o.tag)) { ++moves; }
};

int Widget::copies = 0;
int Widget::moves = 0;

template <typename T>
void show(const char* label, T&& x) {
    using Moved = decltype(std::move(x));
    using Fwd   = decltype(std::forward<T>(x));
    std::printf("%-18s move 之后是左值? %d   forward 之后是左值? %d\n", label,
                static_cast<int>(std::is_lvalue_reference_v<Moved>),
                static_cast<int>(std::is_lvalue_reference_v<Fwd>));
}

// 万能引用上无脑 move: 调用方传左值时, 它的对象被掏空
template <typename T>
void steal(T&& x) {
    Widget stolen = std::move(x);
    std::printf("  steal 之后调用方的 tag 变成 \"%s\"\n", x.tag.c_str());
}

int main() {
    Widget w{"left"};

    show("实参是左值", w);
    show("实参是右值", Widget{"right"});

    Widget::copies = 0;
    Widget::moves = 0;
    steal(w);
    std::printf("steal(左值) 之后: 拷贝=%d 移动=%d  调用方 tag=\"%s\"\n",
                Widget::copies, Widget::moves, w.tag.c_str());
    return 0;
}
