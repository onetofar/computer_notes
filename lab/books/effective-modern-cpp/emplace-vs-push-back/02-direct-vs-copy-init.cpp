// @book: Effective Modern C++
// @topic: emplace-vs-push-back
// @source: Effective Modern C++/8. Tweaks（讲义）.md §2.4 Item 42（explicit 构造函数：emplacement 会"多能编译"）
// @expect: explicit 构造函数: is_constructible_v<Widget,int>=true 而 is_convertible_v<Widget,int>=false, 所以 emplace_back(1) 编译得过, push_back(1) 编译不过
#include <cstdio>
#include <type_traits>
#include <vector>

struct Widget {
    int id;
    explicit Widget(int i) : id(i) { std::printf("Widget(%d) 直接构造\n", id); }
};

static_assert(std::is_constructible_v<Widget, int>);     // 直接初始化: 允许 explicit 构造函数
static_assert(!std::is_convertible_v<Widget, int>);      // 拷贝初始化: 不允许 -> push_back(1) 编译不过

int main() {
    std::vector<Widget> v;
    v.emplace_back(1);                   // 直接初始化 -> 编译通过
    std::printf("emplace_back(1) 成功, 容器里有 %zu 个元素\n", v.size());

    // v.push_back(1);                   // 拷贝初始化 -> 编译不过, 这里只能注释掉
    return 0;
}
