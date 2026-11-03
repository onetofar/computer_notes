// @book: Effective C++
// @topic: compiler-generated-functions
// @source: Item 5 了解 C++ 为你偷偷地加上和调用了什么函数（讲义）.md Item 5
// @expect: 空类的缺省构造/拷贝构造/拷贝赋值/析构全部隐式存在且为 public -> traits 全 true
#include <cstdio>
#include <type_traits>

class Empty {};

int main() {
    std::printf("class Empty {};  --  没有任何用户声明的成员函数\n");
    std::printf("  is_default_constructible = %d\n", (int)std::is_default_constructible_v<Empty>);
    std::printf("  is_copy_constructible    = %d\n", (int)std::is_copy_constructible_v<Empty>);
    std::printf("  is_copy_assignable       = %d\n", (int)std::is_copy_assignable_v<Empty>);
    std::printf("  is_destructible           = %d\n", (int)std::is_destructible_v<Empty>);
    std::printf("  is_trivially_copyable    = %d\n", (int)std::is_trivially_copyable_v<Empty>);
    std::printf("  sizeof(Empty)             = %zu\n", sizeof(Empty));

    Empty e1;                 // 隐式缺省构造函数
    Empty e2(e1);             // 隐式拷贝构造函数
    e2 = e1;                  // 隐式拷贝赋值运算符
    (void)e1;
    (void)e2;
    std::printf("上面三行分别用掉了三个你从没写过的函数, 而且它们都是 public\n");
    std::printf("e1/e2 离开作用域时, 又用掉了第四个: 隐式析构函数\n");
    return 0;
}
