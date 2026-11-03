// @book: Effective Modern C++
// @topic: emplace-vs-push-back
// @source: Effective Modern C++/8. Tweaks（讲义）.md §Item 42 · 2.2 emplacement 什么时候更快：三条判据
// @expect: emplace_back 走构造路线(构造1/移动赋值0); emplace(begin) 走赋值路线(构造1/移动赋值1), emplacement 省掉临时的那点优势没了
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

struct Item {
    std::string s;
    static int ctors;
    static int moves;
    static int move_assigns;

    explicit Item(const char* p) : s(p) { ++ctors; }
    Item(const Item& o) : s(o.s) {}
    Item(Item&& o) noexcept : s(std::move(o.s)) { ++moves; }
    Item& operator=(Item&& o) noexcept { s = std::move(o.s); ++move_assigns; return *this; }
    Item& operator=(const Item&) = delete;

    static void reset() { ctors = 0; moves = 0; move_assigns = 0; }
};

int Item::ctors = 0;
int Item::moves = 0;
int Item::move_assigns = 0;

int main() {
    std::vector<Item> v;
    v.reserve(4);

    Item::reset();
    v.emplace_back("first");
    std::printf("emplace_back    : 构造=%d 移动=%d 移动赋值=%d\n",
                Item::ctors, Item::moves, Item::move_assigns);

    Item::reset();
    v.emplace(v.begin(), "second");
    std::printf("emplace(begin)  : 构造=%d 移动=%d 移动赋值=%d  <- 走的是赋值路线\n",
                Item::ctors, Item::moves, Item::move_assigns);

    std::printf("容器里现在是: %s, %s\n", v[0].s.c_str(), v[1].s.c_str());
    return 0;
}
