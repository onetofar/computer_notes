// @book: Effective Modern C++
// @topic: perfect-forwarding
// @source: Effective Modern C++/5. Rvalue References, Move Semantics, and Perfect Forwarding（讲义）.md §Item 26 · 完美转发构造函数会劫持拷贝构造
// @expect: 拷贝一个非 const Person 时走的是模板构造(模板1/拷贝0), 名字被改写; const Person 才走拷贝构造(模板0/拷贝1)
#include <cstdio>
#include <string>
#include <utility>

struct Person {
    std::string name;
    static int via_template;
    static int via_copy;

    // 完美转发构造: 对非 const 左值, 它比拷贝构造匹配得更好
    template <typename T>
    explicit Person(T&& n) : name(describe(std::forward<T>(n))) { ++via_template; }

    Person(const Person& o) : name(o.name) { ++via_copy; }

    static std::string describe(const Person& p) { return "Person(" + p.name + ")"; }
    static std::string describe(std::string s) { return s; }
};

int Person::via_template = 0;
int Person::via_copy = 0;

int main() {
    Person p1("Alice");
    std::printf("构造 p1        : 模板=%d 拷贝=%d name=\"%s\"\n",
                Person::via_template, Person::via_copy, p1.name.c_str());

    Person::via_template = 0;
    Person::via_copy = 0;
    Person p2(p1);                        // 非 const 左值 -> 模板构造赢了
    std::printf("拷贝 p2(p1)     : 模板=%d 拷贝=%d name=\"%s\"  <- 拷贝构造被劫持\n",
                Person::via_template, Person::via_copy, p2.name.c_str());

    Person::via_template = 0;
    Person::via_copy = 0;
    Person p3(std::as_const(p1));         // const 左值 -> 拷贝构造
    std::printf("拷贝 p3(const)  : 模板=%d 拷贝=%d name=\"%s\"\n",
                Person::via_template, Person::via_copy, p3.name.c_str());
    return 0;
}
