// @book: Effective C++
// @topic: self-assignment
// @source: Item 11 在 operator= 中处理 assignment to self（自赋值）（讲义）.md Item 11
// @expect: 自赋值时先 delete 再拷贝 -> 拷贝构造读到已被 delete 的对象 (use-after-free)
// @ub: yes
#include <cstdio>
#include <cstdlib>

namespace {

int g_live_bitmaps = 0;

class Bitmap {
public:
    Bitmap(int w, int h) : w_(w), h_(h) {
        ++g_live_bitmaps;
        std::printf("    [Bitmap] ctor\n");
    }

    Bitmap(const Bitmap& other) {
        // 先查存活再读成员: 检测本身不依赖未定义行为, 结果是确定的
        if (g_live_bitmaps == 0) {
            std::printf("runtime error: Bitmap 的拷贝构造读取了已被 delete 的对象 "
                        "(use-after-free)\n");
            std::exit(1);
        }
        w_ = other.w_;
        h_ = other.h_;
        ++g_live_bitmaps;
        std::printf("    [Bitmap] copy ctor\n");
    }

    ~Bitmap() {
        --g_live_bitmaps;
        std::printf("    [Bitmap] dtor\n");
    }

private:
    int w_ = 0;
    int h_ = 0;
};

class Widget {
public:
    Widget() : pb_(new Bitmap(1, 1)) {}
    ~Widget() { delete pb_; }

    // Item 11 的原始写法: 既不自赋值安全, 也不异常安全
    Widget& operator=(const Widget& rhs) {
        delete pb_;                            // 先扔掉当前的
        pb_ = new Bitmap(*rhs.pb_);           // 再拷贝 rhs 的
        return *this;
    }

private:
    Bitmap* pb_;
};

}  // namespace

int main() {
    Widget w;
    std::printf("w = w;   (自赋值)\n");
    w = w;
    std::printf("done\n");
    return 0;
}
