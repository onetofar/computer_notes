// @book: Effective Modern C++
// @topic: lambda-capture-lifetime
// @source: Effective Modern C++/6. Lambda Expressions（讲义）.md §Item 31 · [=] 抓到的是 this 的一份拷贝
// @expect: [=] 抓到的 this 在对象被销毁后仍被调用: 读到已释放的对象(悬垂的 this), 并把已被释放的资源再释放一次 -> double free
// @ub: yes
#include <cstdio>
#include <memory>

struct Widget {
    int divisor = 2;
    std::unique_ptr<int> payload = std::make_unique<int>(7);

    auto makeFilter() {
        int* volatile raw = payload.get();      // 留一份指针值(volatile: 保证删除前真的从内存里读它)
        return [=, this](int v) {
            const int d = divisor;             // 透过悬垂的 this 读已释放的对象
            delete raw;                        // 第二次释放 -> double free
            return d > 0 && v > 0;
        };
    }
};

int main() {
    auto pw = std::make_unique<Widget>();
    auto filter = pw->makeFilter();
    pw.reset();                                 // Widget 被销毁, payload 已经被释放

    const bool hit = filter(1);
    std::printf("结果=%d (this 已悬垂, 行为未定义)\n", hit ? 1 : 0);
    return 0;
}
