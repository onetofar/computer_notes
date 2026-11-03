// @book: Linux多线程服务端编程
// @topic: deadlock-lock-order
// @source: 第2章 线程同步精要（讲义）.md §2.1.1
// @expect: 递归 mutex 下不死锁, 但遍历期间 post 触发 vector 扩容, 迭代器失效造成双重释放
// @ub: yes
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

struct Foo {
    int id;
    char* name;  // 故意用裸指针: vector 扩容时会拷贝一份所有权
    explicit Foo(int i) : id(i), name(new char[16]) {
        std::snprintf(name, 16, "foo-%d", i);
    }
    Foo(const Foo& o) : id(o.id), name(o.name) {}  // 拷贝了所有权
    Foo& operator=(const Foo&) = delete;
    ~Foo() { delete[] name; }
    void doit();
};

std::vector<Foo> g_foos;
std::recursive_mutex g_mu;

void post(Foo f) {
    std::lock_guard<std::recursive_mutex> lk(g_mu);
    g_foos.push_back(f);
}

void traverse() {
    std::lock_guard<std::recursive_mutex> lk(g_mu);
    std::vector<Foo>::iterator it = g_foos.begin();
    it->doit();  // 递归加锁不死锁, 但 push_back 让 vector 扩容, it 随即失效
    std::printf("[traverse] 扩容后继续用旧迭代器, id=%d\n", it->id);
}

void Foo::doit() {
    post(Foo{1});  // 模拟"doit() 间接调用了 post()"
}

int main() {
    g_foos.reserve(1);
    g_foos.push_back(Foo{0});
    std::printf("start: size=%lu cap=%lu\n", (unsigned long)g_foos.size(),
                (unsigned long)g_foos.capacity());
    std::fflush(stdout);

    traverse();

    std::printf("end: size=%lu (退出时 vector 析构会第二次释放同一块内存)\n",
                (unsigned long)g_foos.size());
    return 0;
}
