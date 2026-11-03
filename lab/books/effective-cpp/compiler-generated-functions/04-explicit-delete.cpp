// @book: Effective C++
// @topic: compiler-generated-functions
// @source: Item 6 如果你不想使用 compiler-generated functions（编译器生成函数），就明确拒绝（讲义）.md Item 6
// @expect: = delete 后不可拷贝但仍可移动; 与 Uncopyable 基类的效果不同 (那边移动也被掐死)
#include <cstdio>
#include <type_traits>
#include <utility>

// C++11 之后的写法: 拒绝发生在编译期, 且可以只拒拷贝、保留移动
class HomeForSale {
public:
    explicit HomeForSale(int price) : price_(price) {}

    HomeForSale(const HomeForSale&) = delete;         // 明确拒绝
    HomeForSale& operator=(const HomeForSale&) = delete;

    HomeForSale(HomeForSale&&) = default;             // 但移动仍然可用
    HomeForSale& operator=(HomeForSale&&) = default;

    int price() const { return price_; }

private:
    int price_;
};

int main() {
    std::printf("HomeForSale : = delete\n");
    std::printf("  is_copy_constructible = %d\n", (int)std::is_copy_constructible_v<HomeForSale>);
    std::printf("  is_copy_assignable    = %d\n", (int)std::is_copy_assignable_v<HomeForSale>);
    std::printf("  is_move_constructible = %d\n", (int)std::is_move_constructible_v<HomeForSale>);
    std::printf("  is_move_assignable    = %d\n", (int)std::is_move_assignable_v<HomeForSale>);

    HomeForSale a(100);
    HomeForSale b(std::move(a));                     // 移动合法, 资源可以转移
    std::printf("  移动之后 b.price = %d\n", b.price());
    std::printf("\n对比 03: Uncopyable 基类连移动一起掐死, = delete 可以只禁拷贝\n");
    return 0;
}
