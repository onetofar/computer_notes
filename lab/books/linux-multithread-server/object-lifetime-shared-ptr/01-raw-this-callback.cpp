// @book: Linux多线程服务端编程
// @topic: object-lifetime-shared-ptr
// @source: 第1章 线程安全的对象生命期管理（讲义）.md §1.1 当析构函数遇到多线程
// @expect: 回调里保存裸 this 并再次释放它, 对象已析构 -> 双重释放
// @ub: yes
#include <cstdio>
#include <functional>
#include <memory>

class Stock {
public:
    explicit Stock(int id) : id_(id) { std::printf("[Stock] 构造 id=%d\n", id_); }
    ~Stock() { std::printf("[Stock] 析构 id=%d\n", id_); }

    int id() const { return id_; }

    void register_callback(std::function<void()>& out) {
        // 借用: 回调里存的是裸 this, 既不持有所有权, 也不知道对方还在不在
        Stock* p = this;
        out = [p] {
            std::printf("[callback] 触发, 对象其实已经析构了, 这里却还想再释放一次\n");
            delete p;  // 对已经析构的对象再 delete 一次 -> 双重释放
        };
        std::printf("[Stock] 已注册回调 (回调里持有裸 this)\n");
    }

private:
    int id_;
};

int main() {
    std::function<void()> cb;
    {
        auto stock = std::make_unique<Stock>(42);
        stock->register_callback(cb);
    }  // Stock 在这里析构, 但回调还活着, 里面的裸 this 已经悬垂

    std::printf("[main] 对象已析构, 现在触发回调\n");
    std::fflush(stdout);
    cb();
    std::printf("[main] 回调返回\n");
    return 0;
}
