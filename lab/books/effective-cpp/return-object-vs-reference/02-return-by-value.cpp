// @book: Effective C++
// @topic: return-object-vs-reference
// @source: Item 21 当你必须返回一个对象时不要试图返回一个引用（讲义）.md Item 21
// @expect: 按值返回: 结果是调用者自己的对象, 与操作数互不影响; C++17 起返回值的拷贝可以被省略
#include <cstdio>

namespace {

int g_copies = 0;
int g_constructed = 0;

class Rational {
public:
    Rational(int n, int d) : n_(n), d_(d) { ++g_constructed; }
    Rational(const Rational& o) : n_(o.n_), d_(o.d_) { ++g_copies; }
    int n() const { return n_; }
    int d() const { return d_; }
    void scale(int k) { n_ *= k; d_ *= k; }

private:
    int n_ = 0;
    int d_ = 1;
};

// 正确版本: 返回对象, 而不是引用
Rational operator*(const Rational& a, const Rational& b) {
    return Rational(a.n() * b.n(), a.d() * b.d());
}

}  // namespace

int main() {
    Rational a(1, 2), b(3, 4);

    g_copies = 0;
    g_constructed = 0;
    Rational product = a * b;        // C++17: 返回值的拷贝被省略
    std::printf("product = %d/%d\n", product.n(), product.d());
    std::printf("  构造次数 = %d, 拷贝次数 = %d\n", g_constructed, g_copies);

    g_copies = 0;
    g_constructed = 0;
    Rational product2(a * b);        // 同样不拷贝
    std::printf("product2 = %d/%d\n", product2.n(), product2.d());
    std::printf("  构造次数 = %d, 拷贝次数 = %d\n", g_constructed, g_copies);

    product.scale(2);
    std::printf("\n改 product 不影响操作数: product = %d/%d, a = %d/%d, b = %d/%d\n",
                product.n(), product.d(), a.n(), a.d(), b.n(), b.d());
    std::printf("结果是一个独立的对象, 不是任何内部构件的别名。\n");
    return 0;
}
