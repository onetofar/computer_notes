// @book: Effective C++
// @topic: self-assignment
// @source: Item 11 在 operator= 中处理 assignment to self（自赋值）（讲义）.md Item 11
// @expect: copy-and-swap 不需要 identity test; 自赋值安全, 且失败时对象原样不动
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

class Bitmap;
std::vector<Bitmap*> g_alive;
bool g_throw_on_copy = false;
int g_copies = 0;

class Bitmap {
public:
    Bitmap(int w, int h) : w_(w), h_(h) { g_alive.push_back(this); }

    Bitmap(const Bitmap& other) {
        if (g_throw_on_copy) {
            std::printf("    [Bitmap] copy ctor -> throw\n");
            throw std::runtime_error("copy failed");
        }
        w_ = other.w_;
        h_ = other.h_;
        ++g_copies;
        g_alive.push_back(this);
    }

    ~Bitmap() {
        auto it = std::find(g_alive.begin(), g_alive.end(), this);
        if (it != g_alive.end()) g_alive.erase(it);
    }

private:
    int w_ = 0;
    int h_ = 0;
};

bool isAlive(const Bitmap* p) {
    return std::find(g_alive.begin(), g_alive.end(), p) != g_alive.end();
}

class Widget {
public:
    Widget() : pb_(new Bitmap(1, 1)) {}
    ~Widget() { release(); }

    Widget(const Widget& rhs) : pb_(new Bitmap(*rhs.pb_)) {}   // 可能抛

    // copy and swap: 参数按值传 -> 拷贝在进入函数体之前就完成了
    Widget& operator=(Widget rhs) {
        swap(rhs);
        return *this;
    }

    void swap(Widget& rhs) noexcept { std::swap(pb_, rhs.pb_); }

    bool bitmapAlive() const { return isAlive(pb_); }

private:
    void release() {
        if (isAlive(pb_)) delete pb_;
    }
    Bitmap* pb_;
};

}  // namespace

int main() {
    {
        Widget w;
        g_copies = 0;
        std::printf("场景 1: 自赋值 w = w\n");
        w = w;
        std::printf("  赋值后 bitmap 仍然存活 = %d, 期间拷贝次数 = %d\n",
                    (int)w.bitmapAlive(), g_copies);
    }

    {
        Widget a, b;
        g_throw_on_copy = true;
        std::printf("场景 2: a = b, 但拷贝构造抛异常\n");
        try {
            a = b;
        } catch (const std::exception& e) {
            std::printf("  捕获异常: %s\n", e.what());
        }
        std::printf("  赋值后 a 的 bitmap 仍然存活 = %d  <- 对象原样没动\n",
                    (int)a.bitmapAlive());
    }

    std::printf("\n拷贝在参数构造阶段完成, 失败时 operator= 根本没被进入 -> 强异常安全。\n");
    return 0;
}
