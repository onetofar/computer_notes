// @book: Linux多线程服务端编程
// @topic: object-lifetime-shared-ptr
// @source: 第1章 线程安全的对象生命期管理（讲义）.md §1.11.1 enable_shared_from_this
// @expect: enable_shared_from_this 让 this 变身为 shared_ptr, 同一个控制块, use_count 正确
#include <cstdio>
#include <functional>
#include <memory>

class Stock : public std::enable_shared_from_this<Stock> {
public:
    explicit Stock(int id) : id_(id) { std::printf("[Stock] 构造 id=%d\n", id_); }
    ~Stock() { std::printf("[Stock] 析构 id=%d\n", id_); }

    int id() const { return id_; }

    void register_callback(std::function<void()>& out) {
        // 同一个控制块: 引用计数正确, 不会 double free
        std::shared_ptr<Stock> self = shared_from_this();
        std::printf("[Stock] shared_from_this() 的 use_count=%ld\n", (long)self.use_count());
        out = [self] {
            std::printf("[callback] 触发, id=%d (对象仍存活)\n", self->id());
        };
    }

private:
    int id_;
};

int main() {
    std::function<void()> cb;
    {
        auto stock = std::make_shared<Stock>(42);
        stock->register_callback(cb);
    }  // 外层 shared_ptr 没了, 回调里那份还持有对象

    std::printf("[main] 外层 shared_ptr 已释放, 触发回调\n");
    cb();
    std::printf("[main] 回调销毁后对象才析构\n");
    cb = nullptr;
    return 0;
}
