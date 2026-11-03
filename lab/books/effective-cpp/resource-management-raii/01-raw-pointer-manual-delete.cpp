// @book: Effective C++
// @topic: resource-management-raii
// @source: Item 13 使用对象管理资源（讲义）.md Item 13
// @expect: 裸指针 + 手动 delete: acquire 与 delete 之间一旦抛出异常, delete 被跳过, 对象泄漏
#include <cstdio>
#include <stdexcept>

class Investment;

namespace {

int g_created = 0;
int g_destroyed = 0;
Investment* g_last = nullptr;

}  // namespace

class Investment {
public:
    Investment() { ++g_created; std::printf("    [Investment] ctor\n"); }
    ~Investment() { ++g_destroyed; std::printf("    [Investment] dtor\n"); }
};

Investment* createInvestment() {
    g_last = new Investment();     // 工厂函数返回裸指针: 调用者必须记得 delete
    return g_last;
}

void useRawPointer() {
    Investment* pInv = createInvestment();   // 拿到裸指针
    std::printf("    使用中...\n");
    throw std::runtime_error("中途出错了");
    delete pInv;                             // 永远到不了
}

int main() {
    std::printf("场景: 在 acquire 与 delete 之间抛出异常\n");
    try {
        useRawPointer();
    } catch (const std::exception& e) {
        std::printf("  捕获异常: %s\n", e.what());
    }
    std::printf("  created=%d destroyed=%d  <- 差值就是泄漏量\n", g_created, g_destroyed);

    std::printf("\n实验收尾: 手动补上那次 delete, 免得污染进程;\n");
    std::printf("泄漏本身已经由 created/destroyed 的差值记录下来了。\n");
    delete g_last;
    return 0;
}
