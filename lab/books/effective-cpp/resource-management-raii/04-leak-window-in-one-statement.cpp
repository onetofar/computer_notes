// @book: Effective C++
// @topic: resource-management-raii
// @source: Item 17 在一个独立的语句中将 new 出来的对象存入智能指针（讲义）.md Item 17
// @expect: 获取资源与「可能失败的操作」之间隔着一段代码 = 泄漏窗口; 教材原句因 shared_ptr 构造函数 explicit 已不能编译
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <type_traits>

class Widget;

namespace {

int g_created = 0;
int g_destroyed = 0;
Widget* g_last = nullptr;

}  // namespace

class Widget {
public:
    Widget() { ++g_created; std::printf("    [Widget] ctor\n"); }
    ~Widget() { ++g_destroyed; std::printf("    [Widget] dtor\n"); }
};

// 可能失败的操作
int priority() {
    std::printf("    priority() 求值 -> throw\n");
    throw std::runtime_error("priority() failed");
}

void processWidget(std::shared_ptr<Widget> pw, int p) {
    std::printf("    processWidget(priority=%d, pw=%s)\n", p, pw ? "有" : "无");
}

int main() {
    std::printf("场景 A: 获取资源之后, 隔着一段可能失败的代码, 才交给智能指针\n");
    try {
        Widget* raw = new Widget();       // 获取资源
        g_last = raw;
        priority();                       // 可能失败的操作 -> 抛出
        std::unique_ptr<Widget> p(raw);   // 永远到不了 -> raw 泄漏
    } catch (const std::exception& e) {
        std::printf("  捕获异常: %s\n", e.what());
    }
    std::printf("  created=%d destroyed=%d  <- 泄漏窗口\n", g_created, g_destroyed);
    delete g_last;                        // 实验收尾, 免得污染进程
    g_last = nullptr;
    g_created = 0;
    g_destroyed = 0;

    std::printf("\n场景 B: 在一条独立的语句里就把资源交给智能指针\n");
    try {
        std::unique_ptr<Widget> p(new Widget());   // 获取与接管在同一条语句
        priority();                                // 抛出 -> 栈展开时 p 析构
    } catch (const std::exception& e) {
        std::printf("  捕获异常: %s\n", e.what());
    }
    std::printf("  created=%d destroyed=%d  <- 没有泄漏\n", g_created, g_destroyed);
    g_created = 0;
    g_destroyed = 0;

    std::printf("\n场景 C: 教材原版的一条语句写法\n");
    constexpr bool convertible =
        std::is_convertible_v<Widget*, std::shared_ptr<Widget>>;
    std::printf("  is_convertible_v<Widget*, shared_ptr<Widget>> = %d\n", (int)convertible);
    if (!convertible) {
        std::printf("  -> 教材原句 processWidget(new Widget, priority()) 已不能编译:\n");
        std::printf("     shared_ptr 的指针构造函数是 explicit, 不再允许隐式转换。\n");
    }

    std::printf("  现代能编译的等价写法:\n");
    try {
        processWidget(std::shared_ptr<Widget>(new Widget), priority());
    } catch (const std::exception& e) {
        std::printf("  捕获异常: %s\n", e.what());
    }
    std::printf("  created=%d destroyed=%d\n", g_created, g_destroyed);
    std::printf("  上面几行 printf 的先后顺序 = 本机实际的实参求值顺序\n");
    return 0;
}
