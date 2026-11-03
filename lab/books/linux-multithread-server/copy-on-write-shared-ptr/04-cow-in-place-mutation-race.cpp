// @book: Linux多线程服务端编程
// @topic: copy-on-write-shared-ptr
// @source: 第2章 线程同步精要（讲义）.md §2.8
// @expect: 写端无视 unique() 原地修改, 读者持有的迭代器因 vector 扩容而失效 -> 悬垂读
// @ub: yes
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using FooList = std::vector<int>;
using FooListPtr = std::shared_ptr<FooList>;

std::mutex g_mu;
std::condition_variable g_cv;
FooListPtr g_foos = std::make_shared<FooList>();
bool g_reader_ready = false;
bool g_writer_done = false;

int main() {
    g_foos->push_back(1);  // capacity=1, 下面的 push_back 必然触发扩容

    std::thread reader([&] {
        FooListPtr foos;
        FooList::iterator it;
        {
            std::unique_lock<std::mutex> lk(g_mu);
            foos = g_foos;
            it = foos->begin();  // 迭代器指向 vector 的缓冲区
            g_reader_ready = true;
            g_cv.notify_all();
            g_cv.wait(lk, [&] { return g_writer_done; });
        }
        // 写端已经原地 push_back, vector 重新分配了缓冲区, it 指向已释放的内存
        std::fprintf(stderr, "runtime error: 读者即将解引用一个已失效的迭代器\n");
        std::printf("[reader] 读到 %d (这块内存已经被写端释放)\n", *it);
    });

    std::thread writer([&] {
        {
            std::unique_lock<std::mutex> lk(g_mu);
            g_cv.wait(lk, [&] { return g_reader_ready; });
            // 教材的"错误一": 无视 unique(), 直接原地修改读者正在读的那份数据
            g_foos->push_back(2);
            g_writer_done = true;
        }
        g_cv.notify_all();
    });

    reader.join();
    writer.join();
    return 0;
}
