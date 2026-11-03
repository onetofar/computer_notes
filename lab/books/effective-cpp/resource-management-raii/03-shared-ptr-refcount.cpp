// @book: Effective C++
// @topic: resource-management-raii
// @source: Item 13 使用对象管理资源（讲义）.md Item 13
// @expect: shared_ptr 引用计数: 最后一个指针离开作用域时才 delete; get() 把裸资源交出去
#include <cstdio>
#include <memory>

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

void scenario() {
    std::shared_ptr<Investment> pInv1(createInvestment());
    std::printf("  use_count, 只有 pInv1              = %ld\n", pInv1.use_count());

    {
        std::shared_ptr<Investment> pInv2(pInv1);      // 拷贝: 计数 +1
        std::printf("  use_count, 拷贝出 pInv2           = %ld\n", pInv1.use_count());

        pInv1 = pInv2;                                 // 自赋值: 计数不变
        std::printf("  use_count, pInv1 = pInv2          = %ld\n", pInv1.use_count());

        // Item 15: 显式转换, 把 RAII 对象里的裸资源取出来
        Investment* raw = pInv1.get();
        std::printf("  get() 交出裸资源                  = %p\n", (void*)raw);
    }

    std::printf("  use_count, pInv2 离开作用域        = %ld\n", pInv1.use_count());
}

int main() {
    std::printf("场景: 共享所有权 (RCSP)\n");
    scenario();
    std::printf("  created=%d destroyed=%d  <- 最后一个 shared_ptr 离开时才 delete\n",
                g_created, g_destroyed);

    std::printf("\nRCSP 提供的行为类似于垃圾收集, 但它不能打破循环引用:\n");
    std::printf("  两个对象互相持有对方的 shared_ptr, 计数永远降不到 0。\n");
    return 0;
}
