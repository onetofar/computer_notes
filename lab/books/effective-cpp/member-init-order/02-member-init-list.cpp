// @book: Effective C++
// @topic: member-init-order
// @source: Item 4 确保 objects（对象）在使用前被初始化（讲义）.md Item 4
// @expect: 初始化列表 -> 直接拷贝构造, 没有多余的默认构造; 顺序是 基类 -> 成员(声明序) -> 构造函数体
#include <cstdio>
#include <string>

namespace {

struct Name {
    std::string text;

    Name() { std::printf("    [Name] default ctor\n"); }
    Name(const std::string& s) : text(s) { std::printf("    [Name] copy ctor\n"); }
};

struct Slot {
    int id;
    explicit Slot(int i) : id(i) { std::printf("    [Slot %d] ctor\n", id); }
};

}  // namespace

class Base {
public:
    Base() { std::printf("  [Base] ctor\n"); }
};

// 成员初始化列表版本: 少一次默认构造, 顺序一目了然
class InitListStyle : public Base {
public:
    explicit InitListStyle(const std::string& name) : name_(name) {
        std::printf("  [ctor body] enter\n");
        std::printf("  [ctor body] leave\n");
    }

private:
    Name name_;          // 声明顺序 1
    Slot first_{1};      // 声明顺序 2
    Slot second_{2};     // 声明顺序 3
};

int main() {
    std::printf("InitListStyle w(\"Bob\");\n");
    InitListStyle w("Bob");
    std::printf("done\n");
    return 0;
}
