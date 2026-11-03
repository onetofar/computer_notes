// @book: Effective C++
// @topic: compiler-generated-functions
// @source: Item 6 如果你不想使用 compiler-generated functions（编译器生成函数），就明确拒绝（讲义）.md Item 6
// @expect: 继承 Uncopyable 后 HomeForSale 既不可拷贝也不可移动; 原因在基类的 private 拷贝构造
#include <cstdio>
#include <type_traits>

// Item 6 的做法: 把拷贝操作声明为 private 且不给出实现
class Uncopyable {
protected:
    Uncopyable() = default;
    ~Uncopyable() = default;

private:
    Uncopyable(const Uncopyable&);                // 只声明, 不定义
    Uncopyable& operator=(const Uncopyable&);
};

class HomeForSale : private Uncopyable {
public:
    explicit HomeForSale(int price) : price_(price) {}
    int price() const { return price_; }

private:
    int price_;
};

int main() {
    std::printf("HomeForSale : private Uncopyable\n");
    std::printf("  is_copy_constructible = %d\n", (int)std::is_copy_constructible_v<HomeForSale>);
    std::printf("  is_copy_assignable    = %d\n", (int)std::is_copy_assignable_v<HomeForSale>);
    std::printf("  is_move_constructible = %d\n", (int)std::is_move_constructible_v<HomeForSale>);
    std::printf("  is_move_assignable    = %d\n", (int)std::is_move_assignable_v<HomeForSale>);

    HomeForSale h(100);
    std::printf("  构造/析构照常工作, price = %d\n", h.price());
    std::printf("\n机制: 编译器生成的拷贝构造要调用基类的拷贝构造, 那是 private -> 访问检查失败 -> 拷贝构造被删除\n");
    return 0;
}
