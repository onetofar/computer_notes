// @book: Effective Modern C++
// @topic: lambda-capture-lifetime
// @source: Effective Modern C++/6. Lambda Expressions（讲义）.md §Item 31 · [=] 的坑一：它抓的根本不是你想抓的那个东西
// @expect: 成员函数里的 [=] 抓的是 this(闭包 sizeof=8), 成员改动后闭包看到新值; init capture [d=divisor] 抓值拷贝(sizeof=4), 外部改动不影响闭包
#include <cstdio>

struct Widget {
    int divisor = 3;

    auto byDefault() const {
        // C++20 起 [=] 隐式抓 this 被弃用, 这里显式写成 [=, this], 语义与 [=] 完全相同
        return [=, this](int v) { return v % divisor == 0; };  // 抓的是 this, 不是 divisor
    }

    auto byInitCapture() const {
        return [d = divisor](int v) { return v % d == 0; };      // 抓的是值的拷贝
    }
};

int main() {
    Widget w;
    auto f1 = w.byDefault();
    auto f2 = w.byInitCapture();

    std::printf("[=] 闭包 sizeof=%zu   init capture 闭包 sizeof=%zu\n", sizeof(f1), sizeof(f2));

    w.divisor = 5;                       // 改成员
    std::printf("改成员之后: [=] f1(10)=%d   init capture f2(10)=%d\n", f1(10), f2(10));
    return 0;
}
