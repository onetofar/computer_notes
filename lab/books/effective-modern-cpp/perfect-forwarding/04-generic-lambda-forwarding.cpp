// @book: Effective Modern C++
// @topic: perfect-forwarding
// @source: Effective Modern C++/6. Lambda Expressions（讲义）.md §Item 33 · 用 decltype(auto&&) 转发
// @expect: 通用 lambda 按值收参时永远把左值传下去(命中 lvalue 重载, 改的还是副本); 改成 auto&& + forward<decltype(x)> 后按实参的值类别命中
#include <cstdio>
#include <string>
#include <utility>

struct Widget {
    std::string tag;
};

void normalize(Widget& w)  { w.tag += "-lvalue"; }
void normalize(Widget&& w) { w.tag += "-rvalue"; }

int main() {
    auto byValue    = [](auto x)  { normalize(x); std::printf("  处理后 x.tag=%s\n", x.tag.c_str()); };
    auto forwarded = [](auto&& x) { normalize(std::forward<decltype(x)>(x)); std::printf("  处理后 x.tag=%s\n", x.tag.c_str()); };

    Widget a{"a"};
    std::printf("byValue(左值):\n");
    byValue(a);

    Widget b{"b"};
    std::printf("forwarded(左值):\n");
    forwarded(b);

    Widget c{"c"};
    std::printf("byValue(右值):\n");
    byValue(std::move(c));

    Widget d{"d"};
    std::printf("forwarded(右值):\n");
    forwarded(std::move(d));

    std::printf("调用方手里的 d.tag=%s  (forwarded 把右值原样传了下去)\n", d.tag.c_str());
    return 0;
}
