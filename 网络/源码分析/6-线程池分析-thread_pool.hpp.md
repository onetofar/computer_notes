---
title: thread_poll线程池
category: 计算机网络
tags:
  - 源码分析
  - 线程池
  - cpp
  - ThreadPool
source: src/base/thread_pool.hpp
difficulty: 进阶
created: 2026-07-19
skills:
  - obsidian-markdown
---
# ThreadPool —— 线程池源码陪读 & 深度解析

> 一个仅 140 行的 `queue + mutex + condition_variable` 经典线程池。
>
> 核心亮点：`submit()` 返回 `std::future`，完美转发 + `packaged_task` 解耦 "任务提交" 和 "结果获取"。
>
> 本文定位：逐行拆解线程池的四个关键设计——生命周期、任务投递、线程安全、Worker 循环。

---

## 文件位置

| 文件 | 用途 |
|------|------|
| `src/base/thread_pool.hpp` | 线程池类（header-only，141 行） |
| `src/examples/cpp/thread_pool_test.cpp` | 单元测试（11 个用例，234 行） |
| **本文** | `src/examples/cpp/thread_pool_analysis.md` |

---

## 一、架构一览

```
                   submit(f, args...)
                        │
                        ▼
              ┌──────────────────┐
              │   任务队列         │
              │ queue<function>   │  ← mutex 保护
              └──────────────────┘
                        │
          ┌─────────────┼─────────────┐
          │             │             │
          ▼             ▼             ▼
      ┌────────┐  ┌────────┐  ┌────────┐
      │Worker 1│  │Worker 2│  │Worker N│  ← 每个 Worker 一个 thread
      │        │  │        │  │        │
      │ loop() │  │ loop() │  │ loop() │
      │取任务→执行│  │取任务→执行│  │取任务→执行│
      └────────┘  └────────┘  └────────┘
           │            │            │
           └────────────┼────────────┘
                        ▼
                  future.get()
                  (调用方收结果)
```

### 与 muduo 线程池的对比

| 特性 | 本实现 | muduo::ThreadPool |
|------|--------|-------------------|
| `submit()` 返回值 | ✔ `std::future<T>` | ❌ 无返回值 |
| 参数传递 | ✔ 完美转发 | ❌ 只接受 `std::function<void()>` |
| 异常传递 | ✔ 自动传播到 `future` | ❌ 异常在 Worker 线程中吞掉 |
| 暂停/停止 | ✔ `stop()` | ✔ `stop()` |
| 线程数 | 固定 | 固定 |
| C++ 标准 | C++17 | C++03 + boost |

---

## 二、构造函数 —— 启动 Worker 线程

**第 44–50 行**：

```cpp
explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency())
    : stop_(false) {
    if (num_threads == 0) num_threads = 1;
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] { worker_loop(); });
    }
}
```

### 几个设计决策

**为什么用 `std::thread::hardware_concurrency()` 作为默认值？**

返回 CPU 的逻辑核心数（例如 8 核 16 线程的 CPU 返回 16）。对于 CPU 密集型任务，这是最优 Worker 数。对于 IO 密集型，通常建议 2× 核心数。

**为什么 `emplace_back` 传 `[this] { worker_loop(); }` 而不是直接传 `&worker_loop`？**

`worker_loop()` 是**非静态成员函数**，有隐式的 `this` 参数。`std::thread` 不直接支持绑定成员函数。两种写法等价：

```cpp
// 写法 A：lambda 捕获 this（我们的方式）
workers_.emplace_back([this] { worker_loop(); });

// 写法 B：std::thread 的成员函数指针版本
workers_.emplace_back(&ThreadPool::worker_loop, this);
```

写法 A 更清晰，写法 B 需要确保 `this` 在线程运行期间有效——我们的 `workers_` 在析构时 `join()`，保证线程在对象销毁前结束。

**`emplace_back` vs `push_back`**

`emplace_back` 直接构造 `std::thread` 对象，`push_back` 需要移动或拷贝。`std::thread` 是 move-only 的，所以 `push_back` 也需要 `std::move`：

```cpp
workers_.push_back(std::thread(&ThreadPool::worker_loop, this));
//       ^ 需要临时对象，然后移动进去
workers_.emplace_back([this] { worker_loop(); });
//       ^ 直接在 vector 的尾部构造，少一次移动
```

---

## 三、`submit()` —— 任务投递的最关键设计

**第 68–91 行**：

```cpp
template <typename F, typename... Args>
auto submit(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>> {

    using return_type = std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> result = task->get_future();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_) {
            throw std::runtime_error("ThreadPool: submit on stopped pool");
        }
        tasks_.emplace([task]() { (*task)(); });
    }

    cv_.notify_one();
    return result;
}
```

### 拆解：函数签名

```cpp
template <typename F, typename... Args>
auto submit(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>>;
```

- `F&&` — **万能引用**（不是右值引用），既可以接受左值（具名函数）也可以接受右值（lambda）
- `Args&&...` — 参数包的万能引用
- `F, Args...` 各自独立推导，互不影响

**返回类型**：

`std::invoke_result_t<F, Args...>` 是 C++17 的 traits，推导出调用 `F(Args...)` 的返回值类型。等价于 C++11 的 `std::result_of<F(Args...)>::type`，但 `invoke_result` 没有 `decltype` 的缺陷。

例如：
```cpp
pool.submit([](int a, int b) { return a + b; }, 3, 4);
// F = lambda(int, int) → return_type = int
// 返回 std::future<int>
```

```cpp
pool.submit([]{ printf("hi\n"); });
// F = lambda() → return_type = void
// 返回 std::future<void>
```

### 拆解：packaged_task 的妙用

```cpp
auto task = std::make_shared<std::packaged_task<return_type()>>(
    std::bind(std::forward<F>(f), std::forward<Args>(args)...)
);

std::future<return_type> result = task->get_future();

tasks_.emplace([task]() { (*task)(); });
```

**`std::packaged_task` 是一个"可调用对象 + future"的组合体。**

```
std::packaged_task<int()>        ← 包装一个无参、返回 int 的可调用对象
  ├── 重载了 operator()           ← Worker 可以调用它
  └── 关联了一个 std::future<int>  ← 调用方通过它拿结果
```

**为什么用 `shared_ptr` 包一层？**

`std::packaged_task` 是 move-only（不可拷贝）。但 `std::function<void()>` （队列元素类型）要求可拷贝或可移动。

```
queue<std::function<void()>> tasks_;
                   ↑
            要求：拷贝或移动

packaged_task 是 move-only    ← 不能直接放入 std::function
      ↓
shared_ptr<packaged_task>    ← shared_ptr 可拷贝，放入 function
```

lambda `[task]() { (*task)(); }` 捕获 `shared_ptr` → 拷贝的是指针，task 本身只被移动一次。

**执行流**：

```cpp
// Worker 线程中：
auto wrapper = lambda [task];  // shared_ptr 拷贝
wrapper();  // → (*task)() → packaged_task::operator()()
            //    → 实际执行 std::bind(f, args...)
            //    → 返回值存入关联的 future
```

```
 submit() 线程              Worker 线程              调用方线程
     │                        │                        │
     │ make_shared<task>      │                        │
     │ task->get_future()     │                        │
     │ tasks_.emplace(wrapper)│                        │
     │ cv_.notify_one() ─────►│                        │
     │                        │ cv_.wait() 返回        │
     │                        │ pop wrapper            │
     │                        │ wrapper()              │
     │                        │   → (*task)()          │
     │                        │   → f(args...)         │
     │                        │   → 结果存入 future ──►│ f.get() 拿到结果
     │                        │                        │
     return result ───────────►─────────────────────────│
```

### 拆解：线程安全

```cpp
{
    std::lock_guard<std::mutex> lock(mutex_);    // ① 锁保护
    if (stop_) throw ...;                         // ② 停止检查
    tasks_.emplace([task]() { (*task)(); });      // ③ 入队
}
cv_.notify_one();                                  // ④ 通知（已解锁）
```

**为什么 `notify_one` 放在锁外面？**

伪唤醒（spurious wakeup）不需要持有锁。在锁外通知有两个好处：

1. **减少锁竞争**——Worker 被唤醒后需要锁来取任务，如果主线程在通知时还持着锁，Worker 会立刻阻塞在 `lock(mutex_)` 上，相当于是同步的。解锁后再通知，Worker 可以立刻拿到锁。

2. **性能**——锁的粒度越小越好。

但有一个争议：放到锁外可能导致 "通知丢失"（通知发生在 Worker 检查 `wait` 条件之前）。在我们的实现中，`cv_.wait(lock, predicate)` 使用的是**带谓词的 wait**，内部等价于：

```cpp
while (!predicate()) cv_.wait(lock);
```

所以即使 notify 在解锁前发出，Worker 检查 `predicate`（`!tasks_.empty()`）也会发现新任务。**不存在丢失问题。**

### 拆解：异常安全

如果 `f(args...)` 抛出异常，`packaged_task` 会捕获它并存入 `future`。调用 `future.get()` 时异常被重新抛出：

```cpp
// Worker 线程中：
try {
    f(args...);
} catch (...) {
    task->set_exception(std::current_exception());  // 存入 future
}

// 调用方线程：
try {
    int result = future.get();  // 如果 Worker 抛了异常，这里重新抛出
} catch (const std::runtime_error& e) {
    // 捕获来自 Worker 线程的异常
}
```

这是 `std::packaged_task` / `std::promise` 标准库自带的特性，我们不需要写任何异常处理代码。

---

## 四、Worker 循环 —— 线程生命线

**第 116–130 行**：

```cpp
void worker_loop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return stop_ || !tasks_.empty();
            });
            if (stop_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}
```

### 逐行陪读

```
while (true)
    │                ── Worker 线程永远循环，直到收到"停止"信号
    │
std::unique_lock<std::mutex> lock(mutex_);
    │                ── 拿锁，准备操作队列
    │                   unique_lock 比 lock_guard 灵活（可在 wait 中释放/重获）
    │
cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
    │                ── 等价于：
    │                   while (!stop_ && tasks_.empty())
    │                       cv_.wait(lock);
    │                   被唤醒后检查条件，不满足继续 sleep
    │
if (stop_ && tasks_.empty()) return;
    │                ── 线程安全的退出条件
    │                   只有"停止标志位为 true"且"队列空了"才退出
    │                   防止：主线程调 stop() 但还有任务未执行时 Worker 提前退出
    │
task = std::move(tasks_.front());
tasks_.pop();
    │                ── 取出队头任务
    │                   std::move 避免 function 内部的 shared_ptr 拷贝开销
    │
lock.unlock();       ── 解锁（unique_lock 离开作用域自动解锁）
    │
task();              ── 执行任务（在锁外执行！）
    │                   如果 task() 很耗时，其他 Worker 可以同时取任务
```

### 为什么不持锁执行 task？

这是线程池的核心设计决策：**锁只保护队列操作，不保护任务执行。**

如果持锁执行任务：
```cpp
// ❌ 错误的设计
{
    std::lock_guard<std::mutex> lock(mutex_);
    task = tasks_.front();
    tasks_.pop();
    task();  // ← 锁没放！其他 Worker 不能取任务，全部串行化
}
```

如果任务耗时 5 秒，在此期间其他 Worker 全部空转。好的设计：

```cpp
// ✔ 正确的设计
{
    std::lock_guard<std::mutex> lock(mutex_);
    task = std::move(tasks_.front());
    tasks_.pop();
}
task();  // ← 锁已释放，其他 Worker 可以并发执行
```

---

## 五、`stop()` —— 优雅关闭

**第 94–103 行**：

```cpp
void stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
}
```

### 执行顺序

```
stop()
  ├── mutex_.lock()
  │     stop_ = true;          ← 所有 Worker 的 wait predicate 变 true
  ├── mutex_.unlock()
  │
  ├── cv_.notify_all()         ← 唤醒所有 Worker
  │
  ├── for (worker : workers_)
  │     worker.join()          ← 主线程等待每个 Worker 退出
  │
  │     Worker 线程被唤醒后：
  │     while (true) {
  │         wait(lock, stop_ || !tasks_.empty())
  │         if (stop_ && tasks_.empty()) return;  ← 退出
  │         task();
  │     }
  │     ← Worker 线程结束，join() 返回
```

**关键问题**：`stop_` 和 `tasks_.empty()` 之间的关系。

假设主线程调 `stop()` 时队列还有 3 个任务：

```
stop_ = true                    ← 停止标志位
cv_.notify_all()               ← Worker 们被唤醒
                                Worker A: stop_=true, tasks_ 有 3 个
                                         → 不满足退出条件，取一个任务执行
                                Worker B: stop_=true, tasks_ 有 2 个
                                         → 同样不退出，取任务执行
                                Worker C: stop_=true, tasks_ 有 1 个
                                         → 取最后一个任务执行
                                Worker D: stop_=true, tasks_ 空
                                         → return（退出线程）
```

**已提交的任务全部执行完毕后才退出 Worker。** 这正是我们想要的优雅关闭。

### `join()` vs `detach()`

| 方式 | 行为 | 问题 |
|------|------|------|
| `join()` | 主线程阻塞等待 Worker 结束 | 如果 Worker 卡住，主线程也被卡 |
| `detach()` | 分离线程，后台执行 | Worker 可能在访问已销毁的对象 |
| **本实现** | `join()` | 安全、可预测 |

---

## 六、`pending()` —— 队列状态查看

```cpp
size_t pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}
```

**注意**：`pending()` 是 `const` 方法，但 `mutex_` 在 `const` 方法中需要可修改。所以 `mutex_` 声明为 `mutable`：

```cpp
mutable std::mutex mutex_;
```

`mutable` 是 C++ 的"const 豁免"关键字。用在 mutex 上是最常见的场景——逻辑上 `pending()` 不修改对象，但物理上需要修改 mutex 的内部状态。

---

## 七、析构函数 —— RAII

```cpp
~ThreadPool() {
    stop();
}
```

**不显式调用 `stop()` 也可以**——`ThreadPool` 作为局部变量时：

```cpp
{
    ThreadPool pool(4);
    pool.submit([] { /* ... */ });
    pool.submit([] { /* ... */ });
    // 离开作用域时 ~ThreadPool() 自动调用
    // → stop() → notify_all() → join() → 所有任务完成
}
```

这就是 RAII（Resource Acquisition Is Initialization）——资源在构造时获取（创建线程），在析构时释放（等待线程结束）。

---

## 八、测试用例解读

### 基本路径

```cpp
TEST(basic_submit) {
    ThreadPool pool(2);
    auto f = pool.submit([] { return 42; });
    EXPECT_EQ(f.get(), 42);
}
```

最小可行路径：提交一个 λ → 获取结果 → 验证。

### 异常传播

```cpp
TEST(exception_propagation) {
    ThreadPool pool(2);
    auto f = pool.submit([]() -> int {
        throw std::runtime_error("test error");
    });
    try {
        f.get();
        EXPECT_TRUE(false);  // 不应到达
    } catch (const std::runtime_error& e) {
        EXPECT_EQ(std::string(e.what()), "test error");
    }
}
```

验证 `packaged_task` 自动捕获异常并存入 `future` 的机制。

### Move-only 参数

```cpp
TEST(lambda_with_move_only) {
    ThreadPool pool(2);
    auto p = std::make_unique<int>(42);
    auto f = pool.submit([p = std::move(p)] { return *p; });
    EXPECT_EQ(f.get(), 42);
}
```

`std::unique_ptr` 不可拷贝，只能移动。Lambda 通过 `[p = std::move(p)]` 捕获，再被 `std::function<void()>` 包装。这是 C++14 的 init capture 特性。

### 并发提交

```cpp
TEST(concurrent_submit) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    constexpr int N = 10000;

    auto worker = [&]() {
        for (int i = 0; i < N; ++i)
            pool.submit([&counter] { counter++; });
    };

    std::thread t1(worker);
    std::thread t2(worker);
    t1.join();
    t2.join();
    pool.stop();

    EXPECT_EQ(counter.load(), 2 * N);
}
```

验证 `submit()` 本身是线程安全的——两个线程并发 `submit`，mutex 保护队列操作。

### 压力测试

```cpp
TEST(stress_test) {
    ThreadPool pool(std::thread::hardware_concurrency());
    constexpr int N = 100000;
    std::vector<std::future<int>> futures;
    for (int i = 0; i < N; i++)
        futures.push_back(pool.submit([i] { return i; }));
    long long sum = 0;
    for (int i = 0; i < N; i++)
        sum += futures[i].get();
    EXPECT_EQ(sum, 4999950000);  // N*(N-1)/2
}
```

10 万次 submit + get，验证无内存泄漏、无死锁、无竞态。

---

## 九、常见问题

### Q1：为什么 worker_loop 不用 `while (!stop_)`？

```cpp
// ❌ 常见错误写法
void worker_loop() {
    while (!stop_) {           // 无锁 check stop_
        std::lock_guard<std::mutex> lock(mutex_);
        if (tasks_.empty()) continue;  // 忙等！
        task = tasks_.front();
        tasks_.pop();
        task();
    }
}
```

问题：`continue` 不释放锁，立刻检查 `tasks_.empty()`（仍持有锁）→ 死锁。如果用 `cv_` 等待而不是 `continue`，则需要 `while (!stop_ || !tasks_.empty())`——就是我们的实现。

### Q2：`notify_one` 还是 `notify_all`？

| 场景 | 选择 |
|------|------|
| 一次 submit 只需唤醒一个 Worker | `notify_one()` |
| 一次 stop 需要唤醒所有 Worker | `notify_all()` |
| 队列中有大量任务，需要所有 Worker 立刻抢 | `notify_all()` |

我们的 `submit()` 用 `notify_one()`（节约一次 CPU 唤醒），`stop()` 用 `notify_all()`（必须保证所有 Worker 退出）。

### Q3：任务执行时发生异常会怎样？

`packaged_task` 会**捕获异常并存入关联的 `future`**。调用 `future.get()` 时异常被重新抛出。Worker 线程本身不受影响，继续取下一个任务。

**关键**：异常不会导致 Worker 线程退出。如果不用 `packaged_task` 而直接用裸函数：

```cpp
// ❌ 危险——异常会杀死 Worker 线程
void worker_loop() {
    while (true) {
        std::function<void()> task;
        { /* 取任务 */ }
        task();  // 如果 task() 抛出未捕获异常，整个线程挂掉！
    }
}
```

```cpp
// ✔ 安全——packaged_task 自动捕获异常
// 即使 task() 抛异常，packaged_task 存进 future，Worker 继续运行
```

### Q4：`std::function<void()>` 中对 `shared_ptr<task>` 的拷贝开销

每个任务被包装为 `[task]() { (*task)(); }`。`std::function` 存储这个 lambda 时，会拷贝 `shared_ptr`（只是引用计数+1，不是 task 的内容拷贝）。当 Worker 取出 task 时 `std::move` 出来，ref count −1。所以**实际拷贝的是 shared_ptr 的控制块指针（8 字节）**，不是任务本身。

---

## 十、局限性 & 改进方向

| 局限 | 影响 | 生产级改进 |
|------|------|-----------|
| 固定线程数 | 不能动态扩容 | 动态线程池（如 Java ThreadPoolExecutor） |
| 无界队列 | 内存爆炸 | 有界队列 + 拒绝策略 |
| 无 work-stealing | 负载不均 | 每个 Worker 独立队列 + steal（同 ForkJoinPool） |
| 无优先级 | FCFS 无法调整 | 优先队列 `std::priority_queue` |
| `std::function` 虚函数开销 | 每次任务提交有分配 | 自定义类型擦除（如 folly::Function） |
| `shared_ptr<packaged_task>` 堆分配 | 额外 16 字节 | 侵入式 linked list（BS::thread_pool 的做法） |

## 总结速查表

| 代码段 | 行号 | 重要程度 | 说明 |
|--------|------|---------|------|
| 构造函数 | 44–50 | ★★★★☆ | 创建 N 个 Worker 线程 |
| `submit()` 返回值类型 | 69–70 | ★★★★★ | 完美转发 + `invoke_result` |
| `packaged_task` + `shared_ptr` | 75–77 | ★★★★★ | 解耦提交和结果获取的桥梁 |
| `lock_guard` + 入队 | 81–87 | ★★★★★ | 临界区保护 |
| `cv_.notify_one()` 在锁外 | 88 | ★★★★☆ | 减少锁竞争 |
| `worker_loop` wait | 121–123 | ★★★★★ | 带谓词的 wait，避免伪唤醒 |
| 退出条件 | 124 | ★★★★★ | 停止 + 队列空才退出 |
| 锁外执行 `task()` | 127–128 | ★★★★★ | 线程池并发的根本保证 |
| `stop()` 通知 + join | 94–103 | ★★★★☆ | 优雅关闭的顺序 |
| `mutable mutex_` | 135 | ★★★☆☆ | const 兼容 |

## 关联笔记

- [[6-multi_reactor服务器-multi_reactor_server.cpp|6-a multi_reactor服务器实现源码解析]] — 配套网络服务器框架
- [[3-c FutureTask源码分析笔记]] — Java FutureTask 对比
- [[2-4 AQS核心理论笔记|2-4 AQS核心理论笔记]] — Java 并发同步器对比
- [[A cpp第二部分_cpp标准库|A cpp第二部分_cpp标准库]] — std::thread/future/packaged_task 基础
