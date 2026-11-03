// @book: 网络多人在线游戏开发实战教程
// @topic: object-pool-vs-new
// @source: 第5章 性能优化与对象池（讲义）.md §5.5
// @expect: 20 万次 new/delete 全部落到堆上, 构造与析构各 20 万次, 峰值同时在用的对象只有 1 个
#include <chrono>
#include <cstdio>
#include <cstring>

namespace {

constexpr int kIterations = 200000;
long g_ctor = 0;
long g_dtor = 0;

struct Session {
    char account[32];
    int id;
    explicit Session(int i) {
        ++g_ctor;
        std::snprintf(account, sizeof(account), "acc%06d", i);
        id = i;
    }
    ~Session() { ++g_dtor; }
};

}  // namespace

int main() {
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kIterations; ++i) {
        Session* s = new Session(i);  // 每次都去堆上要一块
        s->id = i;
        delete s;                      // 每次都还给分配器
    }
    auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    std::printf("new/delete: iterations=%d ctor=%ld dtor=%ld bytes_each=%zu peak_in_use=1\n",
                kIterations, g_ctor, g_dtor, sizeof(Session));
    std::printf("elapsed_ms=%.1f\n", ms);
    return 0;
}
