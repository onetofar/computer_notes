// @book: Effective Modern C++
// @topic: move-vs-copy-semantics
// @source: Effective Modern C++/5. Rvalue References, Move Semantics, and Perfect Forwarding（讲义）.md §1 Item 23
// @expect: 只调用 std::move 对象一点没变; 真正搬东西的是被选中的移动构造(拷贝0/移动1), 源被掏空但仍是合法对象
#include <cstdio>
#include <cstddef>
#include <type_traits>
#include <utility>

struct Buffer {
    int* data = nullptr;
    std::size_t size = 0;
    static int copies;
    static int moves;

    explicit Buffer(std::size_t n) : data(new int[n]), size(n) {}
    Buffer(const Buffer& o) : data(new int[o.size]), size(o.size) { ++copies; }
    Buffer(Buffer&& o) noexcept : data(o.data), size(o.size) { ++moves; o.data = nullptr; o.size = 0; }
    ~Buffer() { delete[] data; }
};

int Buffer::copies = 0;
int Buffer::moves = 0;

int main() {
    Buffer b(4);

    // std::move 只是转换: 对象本身一点没动
    (void)std::move(b);
    std::printf("只调用 std::move 之后: data=%p size=%zu (什么都没发生)\n",
                static_cast<void*>(b.data), b.size);

    static_assert(std::is_same_v<decltype(std::move(b)), Buffer&&>);

    Buffer c = std::move(b);       // 真正干活的是这一行的移动构造
    std::printf("移动构造之后: 拷贝=%d 移动=%d\n", Buffer::copies, Buffer::moves);
    std::printf("源 b : data=%p size=%zu   (合法但未指定)\n", static_cast<void*>(b.data), b.size);
    std::printf("目标 c: data=%p size=%zu   (接管了源的堆指针)\n", static_cast<void*>(c.data), c.size);
    return 0;
}
