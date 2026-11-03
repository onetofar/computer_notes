// @book: Effective Modern C++
// @topic: auto-deduction-boundary
// @source: Effective Modern C++/1. Deducing Types（讲义）.md §Item 3 · 3.2 括号陷阱
// @expect: decltype(x)=int 而 decltype((x))=int&; 一对括号就让 decltype(auto) 把引用带出函数 -> 悬垂引用 -> 再释放一次触发 double free
// @ub: yes
#include <cstdio>
#include <memory>
#include <type_traits>

int x = 0;

// 名字: decltype(local) 是 int -> 按值返回, 安全
decltype(auto) byName() {
    int local = 42;
    return local;
}

// 表达式: decltype(*owner) 是 int& -> 把引用带出函数, 指向 owner 管的那块堆内存
decltype(auto) byParen() {
    int* p = new int(42);
    std::unique_ptr<int> owner(p);           // 局部: 函数一返回, 这块堆内存就被释放
    return *p;                                // -> int& 悬垂在已释放的堆内存上
}

int main() {
    static_assert(std::is_same_v<decltype(x), int>);
    static_assert(std::is_same_v<decltype((x)), int&>);

    int safe = byName();
    std::printf("byName 返回 int, 值=%d (按值返回, 安全)\n", safe);

    int& dangling = byParen();
    std::printf("byParen 返回 int&, 读它: %d\n", dangling);   // UB: 读已释放的堆内存

    int* stale = &dangling;                 // 悬垂引用转成裸指针
    delete stale;                            // UB: 再释放一次 -> double free
    return 0;
}
