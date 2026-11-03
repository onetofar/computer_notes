// @book: Effective C++
// @topic: return-object-vs-reference
// @source: Item 21 当你必须返回一个对象时不要试图返回一个引用（讲义）.md Item 21
// @expect: 返回静态对象的引用 -> 所有调用共用同一个对象, ((a*b) == (c*d)) 恒为 true
#include <cstdio>

namespace {

class Rational {
public:
    Rational(int n = 0, int d = 1) : n_(n), d_(d) {}
    Rational(const Rational&) = default;
    int n() const { return n_; }
    int d() const { return d_; }
    bool operator==(const Rational& o) const { return n_ == o.n_ && d_ == o.d_; }
    void assign(int n, int d) { n_ = n; d_ = d; }

private:
    int n_ = 0;
    int d_ = 1;
};

// 单静态对象版本: 所有调用返回的是同一个对象的引用
const Rational& operator*(const Rational& a, const Rational& b) {
    static Rational result;
    result.assign(a.n() * b.n(), a.d() * b.d());
    return result;
}

}  // namespace

int main() {
    Rational a(1, 2), b(3, 4), c(5, 6), d(7, 8);

    const Rational& prod1 = a * b;
    std::printf("a*b = %d/%d\n", prod1.n(), prod1.d());

    const Rational& prod2 = c * d;
    std::printf("c*d = %d/%d  <- 与上面是同一个对象\n", prod2.n(), prod2.d());

    std::printf("&prod1 == &prod2 ? %d\n", (int)(&prod1 == &prod2));

    const bool equal = (a * b) == (c * d);
    std::printf("(a*b) == (c*d) -> %d\n", (int)equal);

    std::printf("\n不管 a, b, c, d 的值是什么, 这个表达式总是等于 true。\n");
    std::printf("因为比较的是同一个静态对象和它自己。\n");
    return 0;
}
