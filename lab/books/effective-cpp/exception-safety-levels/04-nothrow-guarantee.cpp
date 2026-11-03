// @book: Effective C++
// @topic: exception-safety-levels
// @source: Item 29 争取异常安全（exception-safe）的代码（讲义）.md Item 29
// @expect: 不抛保证: 只做内建类型操作的函数是 noexcept 的; 可能分配内存的函数不是
#include <cstdio>
#include <new>

namespace {

class PrettyMenu {
public:
    // 只做内建类型的操作 -> 不抛
    bool isValid(int width, int height) const noexcept { return width > 0 && height > 0; }
    void setWidth(int w) noexcept { width_ = w; }
    int width() const noexcept { return width_; }

    ~PrettyMenu() noexcept {
        // 析构函数必须不抛: 否则程序直接 terminate, 没有栈展开可言
        std::printf("  [dtor] width=%d\n", width_);
    }

private:
    int width_ = 0;
};

}  // namespace

// 可能分配内存 -> 可能抛 bad_alloc -> 不是不抛保证
void* allocateMayThrow() { return ::operator new(1024); }

int main() {
    PrettyMenu menu;
    menu.setWidth(640);
    std::printf("PrettyMenu::setWidth 是 noexcept 吗? %d\n",
                (int)noexcept(menu.setWidth(800)));
    std::printf("PrettyMenu::isValid 是 noexcept 吗? %d\n",
                (int)noexcept(menu.isValid(1, 1)));
    std::printf("allocateMayThrow() 是 noexcept 吗? %d\n",
                (int)noexcept(allocateMayThrow()));

    std::printf("\n不抛保证: 决不抛出异常, 因为它们只做它们答应要做的。\n");
    std::printf("所有对内建类型的操作都是不抛出的。\n");

    void* p = allocateMayThrow();
    std::printf("\n分配成功: %s\n", p ? "非空" : "空");
    ::operator delete(p);
    return 0;
}
