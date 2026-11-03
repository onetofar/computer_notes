// @book: Effective Modern C++
// @topic: lambda-capture-lifetime
// @source: Effective Modern C++/6. Lambda Expressions（讲义）.md §2 Item 32（用 init capture 把对象搬进闭包）
// @expect: init capture 把 shared_ptr 搬进闭包, 闭包成了最后一个持有者(闭包活着对象就活着); 按引用捕获则所有权留在外面, 作用域一结束对象先死, 闭包悬垂
#include <cstdio>
#include <memory>

int main() {
    std::weak_ptr<int> weakMoved;
    {
        auto shared = std::make_shared<int>(42);
        weakMoved = shared;
        auto owner = [p = std::move(shared)] { return *p; };   // init capture: 所有权搬进闭包
        std::printf("init capture : 闭包读到=%d  对象还活着? %d\n",
                    owner(), weakMoved.expired() ? 0 : 1);
    }   // 闭包析构: 它是最后一个持有者, 对象跟着析构
    std::printf("闭包销毁后  : 对象还活着? %d\n", weakMoved.expired() ? 0 : 1);

    std::weak_ptr<int> weakBorrowed;
    {
        auto borrowed = std::make_shared<int>(7);
        weakBorrowed = borrowed;
        auto borrower = [&borrowed] { return *borrowed; };   // 按引用: 所有权不转移
        std::printf("按引用捕获  : 闭包读到=%d  对象还活着? %d\n",
                    borrower(), weakBorrowed.expired() ? 0 : 1);
    }   // borrowed 析构: 对象先死, 但闭包还在 -> 闭包手里的引用悬垂
    std::printf("作用域结束  : 对象还活着? %d  (闭包还在, 但它已经悬垂)\n",
                weakBorrowed.expired() ? 0 : 1);
    return 0;
}
