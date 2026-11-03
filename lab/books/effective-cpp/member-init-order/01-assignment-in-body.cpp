// @book: Effective C++
// @topic: member-init-order
// @source: Item 4 确保 objects（对象）在使用前被初始化（讲义）.md Item 4
// @expect: 构造函数体内赋值 -> 成员先被默认构造再被赋值, 多出一次无用的构造
#include <cstdio>
#include <string>

namespace {

struct Name {
    std::string text;

    Name() { std::printf("    [Name] default ctor\n"); }
    Name(const std::string& s) : text(s) { std::printf("    [Name] copy ctor\n"); }
    Name& operator=(const std::string& s) {
        std::printf("    [Name] copy assign\n");
        text = s;
        return *this;
    }
};

}  // namespace

// ABEntry 的 assignment-based 写法: 结果对, 过程浪费
class AssignmentStyle {
public:
    explicit AssignmentStyle(const std::string& name) {
        std::printf("  [ctor body] enter\n");
        name_ = name;   // assignment, not initialization
        std::printf("  [ctor body] leave\n");
    }

private:
    Name name_;
};

int main() {
    std::printf("AssignmentStyle w(\"Bob\");\n");
    AssignmentStyle w("Bob");
    std::printf("done\n");
    return 0;
}
