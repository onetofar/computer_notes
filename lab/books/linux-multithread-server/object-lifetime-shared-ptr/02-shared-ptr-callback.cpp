// @book: Linux多线程服务端编程
// @topic: object-lifetime-shared-ptr
// @source: 第1章 线程安全的对象生命期管理（讲义）.md §1.5 原始指针有何不妥
// @expect: 回调里改存 shared_ptr 副本, 析构被推迟到回调销毁, 不再悬垂
#include <cstdio>
#include <functional>
#include <memory>

class Stock {
public:
    explicit Stock(int id) : id_(id) { std::printf("[Stock] 构造 id=%d\n", id_); }
    ~Stock() { std::printf("[Stock] 析构 id=%d\n", id_); }

    int id() const { return id_; }

    void register_callback(std::function<void()>& out) {
        // 所有权: 回调里存 shared_ptr 副本, 对象不会提前死
        std::shared_ptr<Stock> self = shared_holder_;
        out = [self] {
            std::printf("[callback] 触发, id=%d (对象仍然存活)\n", self->id());
        };
        std::printf("[Stock] 已注册回调 (回调里持有 shared_ptr)\n");
    }

    void bind_holder(std::shared_ptr<Stock> s) { shared_holder_ = std::move(s); }

private:
    int id_;
    std::shared_ptr<Stock> shared_holder_;
};

int main() {
    std::function<void()> cb;
    {
        auto stock = std::make_shared<Stock>(42);
        stock->bind_holder(stock);
        stock->register_callback(cb);
    }  // 外层 shared_ptr 没了, 但回调里还有一份 -> 对象还活着

    std::printf("[main] 外层 shared_ptr 已释放, 现在触发回调\n");
    cb();
    std::printf("[main] 回调销毁后对象才析构\n");
    cb = nullptr;
    return 0;
}
