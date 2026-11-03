// @book: Effective C++
// @topic: member-init-order
// @source: Item 4 确保 objects（对象）在使用前被初始化（讲义）.md Item 4
// @expect: 初始化列表写成与声明顺序相反 -> 实际仍按声明顺序初始化 (编译器会报 -Wreorder)
#include <cstdio>

namespace {

struct Slot {
    int id;
    explicit Slot(int i) : id(i) { std::printf("    [Slot %d] ctor\n", id); }
};

}  // namespace

// GCC 的 -Wreorder 属于 -Wall。这里局部关闭它, 保证整个 lab 零警告构建;
// 想看编译器自己的诊断, 用 -Wreorder 单独编译本文件即可复现:
//   warning: 'Widget::Widget' initializes member members in the wrong order
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreorder"

class Widget {
public:
    Widget() : second_(2), first_(1) {   // 故意与声明顺序相反
        std::printf("  [ctor body]\n");
    }

private:
    Slot first_;    // 声明在前 -> 先被初始化
    Slot second_;   // 声明在后 -> 后被初始化
};

#pragma GCC diagnostic pop

int main() {
    std::printf("Widget w;\n");
    Widget w;
    std::printf("done\n");
    return 0;
}
