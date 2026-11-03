// @book: Effective Modern C++
// @topic: lambda-capture-lifetime
// @source: Effective Modern C++/6. Lambda Expressions（讲义）.md §0 开局 §1 Item 31（[&] 的坑）
// @expect: 闭包逃出创建它的作用域后被调用: 读到已死的局部变量(悬垂引用), 并把已被释放的资源再释放一次 -> double free
// @ub: yes
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

std::vector<std::function<bool(const std::string&)>> g_filters;

void addDivisorFilter() {
    int divisor = 7;                            // 局部: 作用域一结束就没了
    auto owner = std::make_unique<int>(42);     // 堆上的资源: 局部持有它
    int* volatile raw = owner.get();            // 留一份指针值(volatile: 保证删除前真的从内存里读它)

    g_filters.emplace_back([&, raw](const std::string& value) {
        const int d = divisor;                  // 悬垂引用: 读的是已经死掉的局部变量, 值是垃圾
        delete raw;                             // 第二次释放 -> double free
        return d > 0 && !value.empty();
    });
    // 局部 owner 在这里析构: 那块堆内存已经被释放过一次
}

int main() {
    addDivisorFilter();                         // divisor 在这里就没了
    const bool hit = g_filters[0]("xx a-divisor-long-enough-to-live-on-the-heap xx");
    std::printf("结果=%d (读到的是已死的局部变量, 并且发生了 double free)\n", hit ? 1 : 0);
    return 0;
}
