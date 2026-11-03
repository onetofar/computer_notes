// @book: Effective Modern C++
// @topic: emplace-vs-push-back
// @source: Effective Modern C++/8. Tweaks（讲义）.md §2.1 Item 42（push_back("xyzzy") 到底发生了什么）
// @expect: push_back("xyzzy") 先造临时再移动进去(构造1/移动1/析构1), emplace_back("xyzzy") 直接原地构造(构造1/移动0/析构0)
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

struct Item {
    std::string s;
    static int ctors;
    static int copies;
    static int moves;
    static int dtors;

    Item(const char* p) : s(p) { ++ctors; }   // 非 explicit: push_back 才能靠隐式转换拿到临时对象
    Item(const Item& o) : s(o.s) { ++copies; }
    Item(Item&& o) noexcept : s(std::move(o.s)) { ++moves; }
    ~Item() { ++dtors; }

    static void reset() { ctors = 0; copies = 0; moves = 0; dtors = 0; }
};

int Item::ctors = 0;
int Item::copies = 0;
int Item::moves = 0;
int Item::dtors = 0;

int main() {
    std::vector<Item> v1;
    v1.reserve(4);
    Item::reset();
    v1.push_back("xyzzy");
    std::printf("push_back(\"xyzzy\")    : 构造=%d 拷贝=%d 移动=%d 析构=%d\n",
                Item::ctors, Item::copies, Item::moves, Item::dtors);

    std::vector<Item> v2;
    v2.reserve(4);
    Item::reset();
    v2.emplace_back("xyzzy");
    std::printf("emplace_back(\"xyzzy\"): 构造=%d 拷贝=%d 移动=%d 析构=%d\n",
                Item::ctors, Item::copies, Item::moves, Item::dtors);

    std::printf("两个容器各有 %zu / %zu 个元素\n", v1.size(), v2.size());
    return 0;
}
