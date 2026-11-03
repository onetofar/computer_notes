// @book: Effective C++
// @topic: polymorphic-base-dtor
// @source: Item 7 在 polymorphic base classes（多态基类）中将 destructors（析构函数）声明为 virtual（虚拟）（讲义）.md Item 7
// @expect: 无关地给非多态类加虚析构 -> 每个对象多出一个 vptr, 尺寸从 8 字节涨到 16 字节
#include <cstdio>

struct Point {
    int x = 0;
    int y = 0;
};

// 没有虚函数的类, 虚析构是纯开销
struct PointWithVirtualDtor {
    int x = 0;
    int y = 0;
    virtual ~PointWithVirtualDtor() = default;
};

// 有其它虚函数的类, 虚析构是必须的
struct PointWithVirtualFn {
    int x = 0;
    int y = 0;
    virtual void draw() const {}
};

int main() {
    std::printf("指针宽度                  = %zu 字节\n", sizeof(void*));
    std::printf("sizeof(Point)             = %zu 字节\n", sizeof(Point));
    std::printf("sizeof(PointWithVirtualDtor) = %zu 字节\n", sizeof(PointWithVirtualDtor));
    std::printf("sizeof(PointWithVirtualFn)  = %zu 字节\n", sizeof(PointWithVirtualFn));

    std::printf("\nPoint 装得进一个 64-bit 寄存器; 加上 vptr 之后就不行了,\n");
    std::printf("而且它在其它语言写的函数眼里也不再是同样的内存布局。\n");
    std::printf("\n结论: 不是多态基类的类, 不该声明虚析构 —— 无故加和从不加一样错。\n");
    return 0;
}
