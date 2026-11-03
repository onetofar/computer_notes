// @book: C++ Concurrency in Action
// @topic: thread-vs-async-future
// @source: C++ Concurrency in Action/Chapter 4. Synchronizing concurrent operations（讲义）.md §4.2.5
// @expect: future 是一次性的; shared_future 给每个消费者一份拷贝, 都能取到同一个结果
#include <cstdio>
#include <future>
#include <thread>
#include <vector>

int main() {
    std::promise<int> p;
    std::shared_future<int> sf = p.get_future().share();

    std::thread producer([pr = std::move(p)]() mutable { pr.set_value(2026); });

    std::vector<std::thread> consumers;
    for (int i = 0; i < 3; ++i) {
        consumers.emplace_back([sf, i] {          // 每个消费者一份 shared_future 拷贝
            std::printf("consumer%d: 取到 %d\n", i, sf.get());
        });
    }
    producer.join();
    for (auto& c : consumers) c.join();
    return 0;
}
