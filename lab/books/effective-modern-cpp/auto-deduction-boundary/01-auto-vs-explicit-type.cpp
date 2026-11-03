// @book: Effective Modern C++
// @topic: auto-deduction-boundary
// @source: Effective Modern C++/1. Deducing Types（讲义）.md §Item 1 §Item 2 §Item 3
// @expect: 同一个左值四种写法推出四种类型: auto=Widget(拷贝), const auto&=const Widget&, decltype(auto)=Widget&(保留引用), 显式类型=Widget
#include <cstdio>
#include <type_traits>

struct Widget {
    int id;
    explicit Widget(int i) : id(i) {}
};

template <typename T>
void show(const char* label) {
    using U = std::remove_reference_t<T>;
    std::printf("%-24s 是引用=%d 顶层const=%d sizeof=%zu\n", label,
                static_cast<int>(std::is_reference_v<T>),
                static_cast<int>(std::is_const_v<U>),
                sizeof(U));
}

int main() {
    Widget w{7};
    Widget& rw = w;                 // 非 const 左值引用
    const Widget& cw = w;           // const 左值引用

    auto            a1 = rw;        // Case 3: 掉引用、掉 const -> Widget(拷贝)
    const auto&     a3 = rw;        // Case 1: 引用 + const 都保留
    decltype(auto)  a2 = rw;        // decltype 规则: 照抄 rw 的声明类型 -> Widget&
    Widget          a4 = w;         // 显式类型: 自己说了算

    static_assert(std::is_same_v<decltype(a1), Widget>);
    static_assert(std::is_same_v<decltype(a3), const Widget&>);
    static_assert(std::is_same_v<decltype(a2), Widget&>);
    static_assert(std::is_same_v<decltype(a4), Widget>);

    show<decltype(a1)>("auto");
    show<decltype(a3)>("const auto&");
    show<decltype(a2)>("decltype(auto)");
    show<decltype(a4)>("显式 Widget");

    // decltype(auto) 保留了引用: 改 a2 就是改 w; a1 是拷贝, 不受影响
    a2.id = 99;
    std::printf("改 a2 之后: w.id=%d  a1.id=%d\n", w.id, a1.id);

    // 源是 const 时, auto 会把 const 吃掉, decltype(auto) 不会
    auto           b1 = cw;
    decltype(auto) b2 = cw;
    static_assert(std::is_same_v<decltype(b1), Widget>);
    static_assert(std::is_same_v<decltype(b2), const Widget&>);
    show<decltype(b1)>("auto(const源)");
    show<decltype(b2)>("decltype(auto,const源)");
    return 0;
}
