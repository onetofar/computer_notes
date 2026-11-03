// @book: Effective Modern C++
// @topic: auto-deduction-boundary
// @source: Effective Modern C++/1. Deducing Types（讲义）.md §2.1 Item 2（唯一的例外：花括号）
// @expect: auto x={27} 推出 std::initializer_list<int>(元素1个); int x={27} 仍是 int; auto x={1,2,3} 是 initializer_list<int>(元素3个)
#include <cstdio>
#include <initializer_list>
#include <type_traits>
#include <vector>

int main() {
    auto x1 = 27;        // int
    auto x3 = { 27 };    // std::initializer_list<int>  <- auto 的专属规则
    int  x4 = { 27 };    // int

    static_assert(std::is_same_v<decltype(x1), int>);
    static_assert(std::is_same_v<decltype(x3), std::initializer_list<int>>);
    static_assert(std::is_same_v<decltype(x4), int>);

    std::printf("auto x1 = 27    -> size=%zu\n", sizeof(x1));
    std::printf("auto x3 = {27}  -> 元素个数=%zu 首个=%d\n", x3.size(), x3.begin()[0]);
    std::printf("int  x4 = {27}  -> size=%zu\n", sizeof(x4));

    auto many = {1, 2, 3};                   // initializer_list<int>
    std::vector<int> v = {1, 2, 3};          // 显式类型: 容器直接构造
    std::printf("auto many={1,2,3} -> 元素个数=%zu  vector.size=%zu\n", many.size(), v.size());

    // 模板推导没有这条规则: f({1,2,3}) 推不出 T, 编译不过
    // 函数返回类型写 auto 走的是模板推导, 所以 auto makeList() { return {1,2,3}; } 同样编译不过
    return 0;
}
