// @book: 网络多人在线游戏开发实战教程
// @topic: object-pool-vs-new
// @source: 第5章 性能优化与对象池（讲义）.md §5.5
// @expect: 归还时不清场(缺 BackToPool), 下一次借到同一个对象会看到上一个用户的账号名残留
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

struct Session {
    char account[32];
    int id;
    Session() {
        std::memset(account, 0, sizeof(account));
        id = 0;
    }
    void BackToPool() {  // 规范做法: 由对象自己清空数据
        std::memset(account, 0, sizeof(account));
        id = 0;
    }
};

class Pool {
public:
    explicit Pool(size_t n) {
        for (size_t i = 0; i < n; ++i) free_.push_back(new Session());
    }
    Session* Borrow() {
        if (free_.empty()) return nullptr;
        Session* s = free_.back();
        free_.pop_back();
        return s;
    }
    void ReturnClean(Session* s) {  // 带回: 清场
        if (!s) return;
        s->BackToPool();
        free_.push_back(s);
    }
    void ReturnDirty(Session* s) {  // 漏掉 BackToPool: 数据原样留在对象里
        if (!s) return;
        free_.push_back(s);
    }

private:
    std::vector<Session*> free_;
};

}  // namespace

int main() {
    Pool pool(4);

    Session* a = pool.Borrow();
    if (!a) { std::printf("pool empty\n"); return 1; }
    std::snprintf(a->account, sizeof(a->account), "player-A");
    a->id = 1001;
    pool.ReturnDirty(a);  // 归还但没清场

    Session* b = pool.Borrow();  // 借到的很可能就是刚才那个对象
    if (!b) { std::printf("pool empty\n"); return 1; }
    std::printf("第二个借用者读到的残留: account=\"%s\" id=%d\n", b->account, b->id);
    std::printf("残留字节数=%zu\n", std::strlen(b->account));

    pool.ReturnClean(b);
    Session* c = pool.Borrow();
    if (!c) { std::printf("pool empty\n"); return 1; }
    std::printf("带回时清场后残留: account=\"%s\" id=%d\n", c->account, c->id);
    pool.ReturnClean(c);
    return 0;
}
