// @book: Effective Modern C++
// @topic: move-vs-copy-semantics
// @source: Effective Modern C++/5. Rvalue References, Move Semantics, and Perfect Forwarding（讲义）.md §1 Item 23（const 吃掉了移动）
// @expect: 参数非 const 时 std::move 触发移动(拷贝0/移动1); 参数是 const 时静默退回拷贝(拷贝1/移动0)
#include <cstdio>
#include <utility>
#include <vector>

struct Payload {
    std::vector<int> data;
    static int copies;
    static int moves;

    explicit Payload(std::vector<int> v) : data(std::move(v)) {}    // 转换构造, 不计数
    Payload(const Payload& o) : data(o.data) { ++copies; }
    Payload(Payload&& o) noexcept : data(std::move(o.data)) { ++moves; }

    static void reset() { copies = 0; moves = 0; }
};

int Payload::copies = 0;
int Payload::moves = 0;

struct MutableArg {
    Payload p;
    explicit MutableArg(Payload v) : p(std::move(v)) {}     // v 非 const -> 移动构造
};

struct ConstArg {
    Payload p;
    explicit ConstArg(const Payload v) : p(std::move(v)) {}   // v 是 const -> 只能拷贝
};

int main() {
    Payload::reset();
    MutableArg m{Payload{std::vector<int>{1, 2, 3}}};
    std::printf("参数非 const: 拷贝=%d 移动=%d\n", Payload::copies, Payload::moves);

    Payload::reset();
    ConstArg c{Payload{std::vector<int>{1, 2, 3}}};
    std::printf("参数是 const: 拷贝=%d 移动=%d   <- std::move 被 const 吃掉, 静默退回拷贝\n",
                Payload::copies, Payload::moves);
    std::printf("两边数据一样: m=%zu c=%zu\n", m.p.data.size(), c.p.data.size());
    return 0;
}
