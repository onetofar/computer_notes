// @book: Effective C++
// @topic: resource-management-raii
// @source: Item 13 使用对象管理资源（讲义）.md Item 13
// @expect: 用对象管理资源: 异常穿过作用域时析构照样跑, created == destroyed
#include <cstdio>
#include <memory>
#include <stdexcept>

namespace {

int g_created = 0;
int g_destroyed = 0;

}  // namespace

class Investment {
public:
    Investment() { ++g_created; std::printf("    [Investment] ctor\n"); }
    ~Investment() { ++g_destroyed; std::printf("    [Investment] dtor\n"); }
};

Investment* createInvestment() {
    return new Investment();
}

void useRaii() {
    std::unique_ptr<Investment> pInv(createInvestment());   // 立刻接管所有权
    std::printf("    使用中...\n");
    throw std::runtime_error("中途出错了");
    // 栈展开 -> pInv 析构 -> Investment 被 delete
}

int main() {
    std::printf("场景: 同样的异常, 但资源由对象管理\n");
    try {
        useRaii();
    } catch (const std::exception& e) {
        std::printf("  捕获异常: %s\n", e.what());
    }
    std::printf("  created=%d destroyed=%d  <- 没有泄漏\n", g_created, g_destroyed);

    std::printf("\n离开作用域的方式 (正常返回 / 异常展开) 不再影响资源是否被释放。\n");
    return 0;
}
