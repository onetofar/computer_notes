// @book: Effective C++
// @topic: self-assignment
// @source: Item 10 让 assignment operators（赋值运算符）返回一个 reference to this（引向 this 的引用）（讲义）.md Item 10
// @expect: 返回引用时赋值可串联且 (a=b)=c 改的是 a; 返回对象时多一次拷贝, (a=b)=c 改的是临时对象
#include <cstdio>

namespace {

int g_copies = 0;

// 惯例: 返回左侧对象的引用
class WidgetByRef {
public:
    WidgetByRef() = default;
    WidgetByRef(const WidgetByRef& o) : v_(o.v_) { ++g_copies; }
    WidgetByRef& operator=(const WidgetByRef& rhs) { v_ = rhs.v_; return *this; }
    int v() const { return v_; }
    void set(int x) { v_ = x; }

private:
    int v_ = 0;
};

// 反例: 返回对象(值)
class WidgetByValue {
public:
    WidgetByValue() = default;
    WidgetByValue(const WidgetByValue& o) : v_(o.v_) { ++g_copies; }
    WidgetByValue operator=(const WidgetByValue& rhs) { v_ = rhs.v_; return *this; }
    int v() const { return v_; }
    void set(int x) { v_ = x; }

private:
    int v_ = 0;
};

}  // namespace

int main() {
    {
        WidgetByRef a, b, c;
        c.set(7);
        g_copies = 0;
        a = b = c;                       // 赋值可以串成一串
        std::printf("返回 Widget& : a = %d, 期间拷贝次数 = %d\n", a.v(), g_copies);
    }
    {
        WidgetByValue a, b, c;
        c.set(7);
        g_copies = 0;
        a = b = c;
        std::printf("返回 Widget  : a = %d, 期间拷贝次数 = %d\n", a.v(), g_copies);
    }
    {
        WidgetByValue a, b, c;
        c.set(7);
        g_copies = 0;
        b = c;                       // 只做一次赋值
        std::printf("返回 Widget  : b = c (单次) 拷贝次数 = %d  <- 来自 return *this\n", g_copies);
    }

    {
        WidgetByRef a, b, c;
        c.set(7);
        (a = b) = c;                     // 内层赋值的返回值又被赋值
        std::printf("返回 Widget& : (a=b)=c -> a = %d  <- 改的是 a\n", a.v());
    }
    {
        WidgetByValue a, b, c;
        c.set(7);
        (a = b) = c;
        std::printf("返回 Widget  : (a=b)=c -> a = %d  <- 改的是临时对象, a 没变\n", a.v());
    }

    std::printf("\n惯例: return *this;  // return the left-hand object\n");
    return 0;
}
