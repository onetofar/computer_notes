// @book: 网络多人在线游戏开发实战教程
// @topic: object-pool-vs-new
// @source: 第5章 性能优化与对象池（讲义）.md §5.5
// @expect: 池子预造 1024 个对象, 20 万次借还只发生 1024 次构造, 归还时 BackToPool 清空残留
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr int kIterations = 200000;
constexpr int kPoolSize = 1024;
long g_ctor = 0;
long g_dtor = 0;
long g_borrow = 0;
long g_return = 0;

struct Session {
    char account[32];
    int id;
    explicit Session(int i) {
        ++g_ctor;
        std::snprintf(account, sizeof(account), "acc%06d", i);
        id = i;
    }
    ~Session() { ++g_dtor; }
    void BackToPool() {  // 谁的数据谁清楚: 对象自己清场
        account[0] = '\0';
        id = 0;
    }
};

class Pool {
public:
    explicit Pool(size_t n) {
        free_.reserve(n);
        for (size_t i = 0; i < n; ++i) free_.push_back(new Session(static_cast<int>(i)));
    }
    Session* Borrow() {
        if (free_.empty()) return nullptr;
        Session* s = free_.back();
        free_.pop_back();
        ++g_borrow;
        return s;
    }
    void Return(Session* s) {
        if (!s) return;
        s->BackToPool();
        free_.push_back(s);
        ++g_return;
    }

private:
    std::vector<Session*> free_;
};

}  // namespace

int main() {
    auto t0 = std::chrono::steady_clock::now();
    Pool pool(kPoolSize);
    for (int i = 0; i < kIterations; ++i) {
        Session* s = pool.Borrow();
        if (!s) { std::printf("pool exhausted\n"); return 1; }
        std::snprintf(s->account, sizeof(s->account), "acc%06d", i);
        s->id = i;
        pool.Return(s);
    }
    auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    std::printf("pool: iterations=%d pool_size=%d ctor=%ld dtor=%ld borrow=%ld return=%ld\n",
                kIterations, kPoolSize, g_ctor, g_dtor, g_borrow, g_return);
    std::printf("elapsed_ms=%.1f\n", ms);
    return 0;
}
