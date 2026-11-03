// @book: Effective C++
// @topic: return-object-vs-reference
// @source: Item 21 当你必须返回一个对象时不要试图返回一个引用（讲义）.md Item 21
// @expect: 返回局部对象的引用 -> 调用者拿到悬垂引用, 读它就是未定义行为
// @ub: yes
#include <cstdio>

// GCC 的 -Wreturn-local-addr / -Wdangling-reference (都属于 -Wall) 会指出「返回局部对象的引用」。
// 本文件故意这么写, 故局部关闭这些警告, 保证 lab 零警告构建。
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-local-addr"
#pragma GCC diagnostic ignored "-Wdangling-reference"

namespace {

int g_destroyed = 0;

class Rational {
public:
    Rational(int n, int d) : n_(n), d_(d) {}
    Rational(const Rational&) = default;
    ~Rational() { ++g_destroyed; std::printf("    [Rational] dtor\n"); }
    int n() const { return n_; }
    int d() const { return d_; }

private:
    int n_ = 0;
    int d_ = 1;
};

// 危险版本: 返回指向局部对象的引用
const Rational& operator*(const Rational& a, const Rational& b) {
    Rational result(a.n() * b.n(), a.d() * b.d());
    std::printf("    [Rational] ctor (局部对象)\n");
    return result;      // result 在函数返回时就被销毁了
}

}  // namespace

#pragma GCC diagnostic pop

int main() {
    Rational a(1, 2), b(3, 4);

    const int destroyed_before = g_destroyed;
    std::printf("const Rational& r = a * b;\n");
    const Rational& r = a * b;

    if (g_destroyed > destroyed_before) {
        std::printf("runtime error: operator* 返回的引用指向一个已经被销毁的局部对象 "
                    "(dangling reference); 读它就是未定义行为\n");
        return 1;
    }

    std::printf("r = %d/%d\n", r.n(), r.d());
    return 0;
}
