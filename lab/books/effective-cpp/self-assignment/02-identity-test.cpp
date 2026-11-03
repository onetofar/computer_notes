// @book: Effective C++
// @topic: self-assignment
// @source: Item 11 在 operator= 中处理 assignment to self（自赋值）（讲义）.md Item 11
// @expect: identity test 让自赋值变安全, 但赋值中途抛异常时成员指针仍然悬垂
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {

class Bitmap;
std::vector<Bitmap*> g_alive;      // 活着的 Bitmap 名单
bool g_throw_on_copy = false;

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

    Widget& operator=(const Widget& rhs) {
        if (this == &rhs) return *this;    // identity test: 自赋值直接返回
        delete pb_;
        pb_ = new Bitmap(*rhs.pb_);        // 这里抛异常的话, pb_ 就悬垂了
        return *this;
    }

    bool bitmapAlive() const { return isAlive(pb_); }

private:
    void release() {
        if (isAlive(pb_)) delete pb_;        // 已被 delete 过就不要再删
    }
    Bitmap* pb_;
};

}  // namespace

int main() {
    {
        Widget w;
        std::printf("场景 1: 自赋值 w = w\n");
        w = w;
        std::printf("  赋值后 bitmap 仍然存活 = %d\n", (int)w.bitmapAlive());
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
        std::printf("  赋值后 a 的 bitmap 仍然存活 = %d  <- 悬垂指针\n",
                    (int)a.bitmapAlive());
    }

    std::printf("\nidentity test 只解决了自赋值, 没有解决异常安全。\n");
    return 0;
}
