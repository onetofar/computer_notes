// @book: Effective Modern C++
// @topic: perfect-forwarding
// @source: Effective Modern C++/5. Rvalue References, Move Semantics, and Perfect Forwarding（讲义）.md §Item 28 · 引用折叠
// @expect: 折叠规则: int& & 与 int& && 与 int&& & 都折成 int&, 只有 int&& && 折成 int&&; 实参是左值时 T 推成引用, 是右值时 T 推成非引用
#include <cstdio>
#include <type_traits>
#include <utility>

struct Widget { int id; };

template <typename T> struct AddLValueRef { using type = T&; };
template <typename T> struct AddRValueRef { using type = T&&; };

template <typename T>
void deduced(const char* label, T&& x) {
    (void)x;
    std::printf("%-16s T 是引用? %d  T 是左值引用? %d\n", label,
                static_cast<int>(std::is_reference_v<T>),
                static_cast<int>(std::is_lvalue_reference_v<T>));
}

int main() {
    using A = typename AddLValueRef<int&>::type;     // int& &   -> int&
    using B = typename AddRValueRef<int&>::type;     // int& &&  -> int&
    using C = typename AddLValueRef<int&&>::type;    // int&& &  -> int&
    using D = typename AddRValueRef<int&&>::type;    // int&& && -> int&&

    static_assert(std::is_same_v<A, int&>);
    static_assert(std::is_same_v<B, int&>);
    static_assert(std::is_same_v<C, int&>);
    static_assert(std::is_same_v<D, int&&>);

    std::printf("int&  &   -> int&  ? %d\n", static_cast<int>(std::is_same_v<A, int&>));
    std::printf("int&  &&  -> int&  ? %d\n", static_cast<int>(std::is_same_v<B, int&>));
    std::printf("int&& &   -> int&  ? %d\n", static_cast<int>(std::is_same_v<C, int&>));
    std::printf("int&& &&  -> int&& ? %d\n", static_cast<int>(std::is_same_v<D, int&&>));

    using FwdL = decltype(std::forward<int&>(std::declval<int&>()));
    using FwdR = decltype(std::forward<int>(std::declval<int>()));
    static_assert(std::is_same_v<FwdL, int&>);
    static_assert(std::is_same_v<FwdR, int&&>);
    std::printf("forward<int&> -> int&  ? %d\n", static_cast<int>(std::is_same_v<FwdL, int&>));
    std::printf("forward<int>  -> int&& ? %d\n", static_cast<int>(std::is_same_v<FwdR, int&&>));

    Widget w{1};
    deduced("实参是左值", w);          // T 推成 Widget&
    deduced("实参是右值", Widget{});    // T 推成 Widget
    return 0;
}
