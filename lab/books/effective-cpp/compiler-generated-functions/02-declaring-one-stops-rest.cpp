// @book: Effective C++
// @topic: compiler-generated-functions
// @source: Item 5 了解 C++ 为你偷偷地加上和调用了什么函数（讲义）.md Item 5
// @expect: 声明拷贝构造 -> 移动语义不再隐式生成 (移动一个对象其实在拷贝); 声明移动构造 -> 拷贝语义不再隐式生成
#include <cstdio>
#include <type_traits>
#include <utility>

namespace {

int g_copies = 0;
int g_moves = 0;

// Probe 自己是一个「完整」的普通类型: 拷贝/移动四件套齐全
struct Probe {
    Probe() = default;
    Probe(const Probe&) { ++g_copies; }
    Probe(Probe&&) noexcept { ++g_moves; }
    Probe& operator=(const Probe&) = default;
    Probe& operator=(Probe&&) noexcept = default;
};

// 一个用户声明的成员函数都没有
struct Bare {
    Probe p;
    int v = 0;
};

// 用户声明了拷贝构造 -> 隐式的移动构造/移动赋值不再生成
struct HasCopyCtor {
    HasCopyCtor() = default;
    HasCopyCtor(const HasCopyCtor&) = default;
    Probe p;
    int v = 0;
};

// 用户声明了移动构造 -> 隐式的拷贝构造/拷贝赋值不再生成
struct HasMoveCtor {
    HasMoveCtor() = default;
    HasMoveCtor(HasMoveCtor&&) = default;
    Probe p;
    int v = 0;
};

}  // namespace

template <typename T>
void row(const char* name) {
    std::printf("  %-14s default_ctor=%d copy_ctor=%d copy_assign=%d move_ctor=%d move_assign=%d\n",
                name,
                (int)std::is_default_constructible_v<T>,
                (int)std::is_copy_constructible_v<T>,
                (int)std::is_copy_assignable_v<T>,
                (int)std::is_move_constructible_v<T>,
                (int)std::is_move_assignable_v<T>);
}

template <typename T>
void countMove(const char* name) {
    g_copies = 0;
    g_moves = 0;
    {
        T a;
        T b(std::move(a));
        (void)b;
    }
    std::printf("  %-14s copies=%d moves=%d\n", name, g_copies, g_moves);
}

int main() {
    std::printf("traits 对照:\n");
    row<Bare>("Bare");
    row<HasCopyCtor>("HasCopyCtor");
    row<HasMoveCtor>("HasMoveCtor");

    std::printf("\n移动一个对象时实际发生了什么 (代价):\n");
    countMove<Bare>("Bare");
    countMove<HasCopyCtor>("HasCopyCtor");
    countMove<HasMoveCtor>("HasMoveCtor");

    std::printf("\nHasCopyCtor 的 is_move_constructible 仍报 1:\n");
    std::printf("  拷贝构造 const T& 同样接受右值, 所以这个 trait 只回答「能不能」,\n");
    std::printf("  回答不了「代价是什么」; 要看代价只能数拷贝次数。\n");
    return 0;
}
