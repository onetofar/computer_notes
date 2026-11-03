// @book: Effective C++
// @topic: return-object-vs-reference
// @source: Item 28 避免返回对象内部构件的“句柄”（讲义）.md Item 28
// @expect: 返回内部构件的句柄 -> const 对象被改; 取临时对象内部构件的地址 -> 悬垂指针
#include <algorithm>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

class Point;                               // 前向声明, 与下面的定义是同一个类
int g_live_points = 0;
std::vector<const Point*> g_destroyed_addrs;   // 已析构 Point 的地址名单

class Point {
public:
    Point(int x, int y) : x_(x), y_(y) { ++g_live_points; }
    Point(const Point& o) : x_(o.x_), y_(o.y_) { ++g_live_points; }
    ~Point() {
        --g_live_points;
        g_destroyed_addrs.push_back(this);
    }
    int x() const { return x_; }
    int y() const { return y_; }
    void setX(int v) { x_ = v; }

private:
    int x_ = 0;
    int y_ = 0;
};

struct RectData {
    Point ulhc;   // upper left-hand corner
    Point lrhc;   // lower right-hand corner
};

class Rectangle {
public:
    Rectangle(Point a, Point b) : pData_(std::make_shared<RectData>(RectData{a, b})) {}

    // 教材原版: const 成员函数却交出一把可变的钥匙
    // (pData 是 shared_ptr, operator-> 返回非 const 的 RectData*, const 在这里断掉了)
    Point& upperLeft() const { return pData_->ulhc; }

    // 返回内部构件的副本: 调用者改不到内部
    Point upperLeftCopy() const { return pData_->ulhc; }

    int upperLeftX() const { return pData_->ulhc.x(); }

private:
    std::shared_ptr<RectData> pData_;
};

}  // namespace

int main() {
    // 场景 1: const 对象被改
    {
        Rectangle rec(Point(0, 0), Point(100, 100));
        std::printf("场景 1: rec 初始 upperLeftX = %d\n", rec.upperLeftX());
        const Rectangle crec(Point(0, 0), Point(100, 100));
        crec.upperLeft().setX(50);     // rec 是 const 的, 却还是被改了
        std::printf("  const 对象的 upperLeftX = %d  <- 句柄绕过了 const\n",
                    crec.upperLeftX());
    }

    // 场景 2: 返回副本就改不到
    {
        Rectangle rec(Point(0, 0), Point(100, 100));
        rec.upperLeftCopy().setX(50);
        std::printf("\n场景 2: 返回副本时 upperLeftX = %d  <- 改的是副本\n",
                    rec.upperLeftX());
    }

    // 场景 3: 取临时对象内部构件的地址
    {
        const Point* pUpperLeft = &(Rectangle(Point(0, 0), Point(100, 100)).upperLeft());
        const bool dangling =
            std::find(g_destroyed_addrs.begin(), g_destroyed_addrs.end(), pUpperLeft) !=
            g_destroyed_addrs.end();
        std::printf("\n场景 3: 临时 Rectangle 在语句结束时就已析构\n");
        if (dangling) {
            std::printf("  悬垂指针: pUpperLeft 指向临时对象内部的 Point, "
                        "语句结束时它已被析构 —— 一旦解引用就是未定义行为\n");
        } else {
            std::printf("  pUpperLeft 仍然有效\n");
        }
    }
    return 0;
}
