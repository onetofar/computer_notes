---
title: muduo源码指南
category: 常用工具
tags:
  - net/server
  - net
  - muduo
difficulty: 深入
source: 自整理
---
# muduo 源码陪读 & 深度解析

> muduo 是陈硕（Shuo Chen）基于 Reactor 模式编写的 C++ 网络库，是现代 C++ 网络编程的经典教材配套项目（《Linux 多线程服务端编程》）。
>
> 代码风格整洁，模块划分清晰，是学习 Reactor 网络库的最佳入门项目之一。
>
> **本文定位**：逐层深入，标出每个模块的学习价值、设计亮点与现代替代方案。

---

## 阅读路线总览

```
muduo/
├── base/              ← 基础组件（可独立学习）
│   ├── Thread         → 线程封装
│   ├── Mutex/Condition→ 同步原语
│   ├── ThreadPool     → 线程池
│   ├── Logging        → 日志库
│   └── Timestamp      → 时间戳
└── net/               ← 网络核心（建议按顺序读）
    ├── EventLoop      → ★★★★★ 核心
    ├── Channel        → ★★★★★ 核心
    ├── Poller         → ★★★★☆
    ├── TimerQueue     → ★★★★☆
    ├── Acceptor       → ★★★☆☆
    ├── TcpConnection  → ★★★★★ 核心
    ├── TcpServer      → ★★★★★ 入口
    ├── Buffer         → ★★★★★ 精华
    ├── EventLoopThreadPool → ★★★★☆
    └── http/          → ★★★☆☆ 应用层
```

---

# 一、base/ 基础库

## 1.1 noncopyable / copyable

**重要程度：★★☆☆☆**

```cpp
class noncopyable {
 protected:
  noncopyable() = default;
  ~noncopyable() = default;
  noncopyable(const noncopyable&) = delete;
  noncopyable& operator=(const noncopyable&) = delete;
};
```

**设计意图**：继承 `noncopyable` 的类自动禁止拷贝，防止意外复制持有资源（如 socket fd、锁）的对象。

**陪读要点：**
- 原理是 C++11 的 `= delete`，读一遍就过
- 还有一个对应的 `copyable` 空基类（标记接口，零开销）

**现代替代：**
- C++11 后直接在类里写 `X(const X&) = delete;` 更常见
- `noncopyable` 这种基类的作用更多的是**语义标记**（文档化意图）
- `copyable` 纯标记，无实用价值 → 跳过

> **结论**：看一眼知道就行，不用深究。

---

## 1.2 Types / StringPiece

**重要程度：★★☆☆☆**

```cpp
// Types.h 中：
#include <stdint.h>  // int64_t 等
#include <string>
// using 常用的标准库类型
```

`StringPiece` 是 `std::string_view` 之前时代的产物，一个指向外部字符串的非拥有视图：

```cpp
class StringPiece {
    const char* ptr_;
    int len_;
    // 大量构造函数：string, const char*, 裸指针+长度
};
```

**陪读要点：**
- `StringPiece` 不持有数据，调用者需保证生命周期
- 支持 `substr`、`as_string`、比较等操作

**现代替代：**
- C++17 标准库 `std::string_view` 功能完全覆盖，直接替换即可
- 如果项目用 C++17，无视 `StringPiece`，直接用 `std::string_view`

> **结论**：过时的工具类，了解概念即可。

---

## 1.3 Timestamp / Date / TimeZone

**重要程度：★★★☆☆**

muduo 自己实现了时间戳，基于 `struct timeval`（微秒精度）：

```cpp
class Timestamp {
    int64_t microSecondsSinceEpoch_;
    Timestamp() : microSecondsSinceEpoch_(0) {}
    static Timestamp now();
    string toFormattedString(bool showMicroseconds = false) const;
    bool operator<(const Timestamp& rhs) const;
};
```

**陪读要点：**
- 核心设计是小对象 + 值语义（可拷贝、可比较）
- `toFormattedString` 内部调 `strftime` + 微秒拼接
- `addTime()` 返回新的 `Timestamp`，非递增时间字符串

**学习价值：**
- 时间戳设计思路在几乎所有网络库中都类似（libuv、Netty、Boost.Asio 都有对应）
- 微秒精度足够应付绝大多数场景

**缺点：**
- `toFormattedString` 每次格式化都调 `localtime_r`，性能一般
- 没有单调时钟（`CLOCK_MONOTONIC`）版本用于测量间隔

> **结论**：了解时间戳在小对象和值语义上的设计思路即可，不必逐行实现。

---

## 1.4 Atomic

**重要程度：★★☆☆☆**

```cpp
template<typename T>
class AtomicIntegerT {
    volatile T value_;
public:
    T get() { return __sync_val_compare_and_swap(&value_, 0, 0); }
    T addAndGet(T x) { return __sync_add_and_fetch(&value_, x); }
    T incrementAndGet() { return addAndGet(1); }
    T decrementAndGet() { return addAndGet(-1); }
};
```

**陪读要点：**
- 使用 GCC 内置原子操作（`__sync_*` 系列），不是 C++11 `std::atomic`
- `volatile` 不是原子操作的正确工具——这里实际靠的是 `__sync_*`
- 提供了 `getAndSet(T)` 方法用于替代 CAS 自旋

**现代替代：**
- C++11 后直接使用 `std::atomic<T>`，更标准、更安全
- muduo 也意识到了这点，在 `EventLoop.h` 中混用了 `std::atomic<bool> quit_`

> **结论**：使用 `std::atomic` 直接替代即可，muduo 的实现只是为了在没有 C++11 特性的环境兼容。

---

## 1.5 Thread

**重要程度：★★★★☆**

```cpp
class Thread {
    bool started_;
    pthread_t pthreadId_;
    pid_t tid_;                  // 缓存线程真实 TID
    ThreadFunc func_;
    CountDownLatch latch_;       // 确保线程启动成功后才返回
    static AtomicInt32 numCreated_;
};
```

**陪读要点——设计亮点：**

1. **`CountDownLatch` 确保启动同步**（关键设计）
   `start()` 中调用 `latch_.wait()` 等待线程 tid 获取完毕，返回时保证线程已就绪：
   ```cpp
   void Thread::start() {
       pthread_create(&pthreadId_, NULL, &startThread, this);
       latch_.wait();  // 等待新线程记录完 tid
   }
   ```
   这是 C++ 线程封装的经典技巧——不在构造函数中启动线程，而是单独 `start()` + 同步确认。

2. **缓存 TID**（性能优化）
   muduo 使用 Linux 的 `gettid()` 获取线程真实 ID（不是 `pthread_t`），缓存在 `thread_local` 变量中：
   ```cpp
   // CurrentThread.cc
   __thread int t_cachedTid = 0;
   void cacheTid() {
       if (t_cachedTid == 0) {
           t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
       }
   }
   ```
   **一次 syscall，后续直接读 `thread_local` 变量**——这个技巧在 muduo 里被 `EventLoop` 用来检测"是否在同一个线程"。

3. **线程名**支持
   muduo 通过 `prctl(PR_SET_NAME, ...)` 设置线程名，辅助调试（`top -H` 可见）。

**现代替代：**
- C++11 `std::thread` 可以直接替代 `pthread_t`，但功能不如 muduo 的 `Thread` 丰富
- `std::thread` 没有 `gettid()` 的等价物，需要自行 syscall
- `std::thread` 没有线程启动同步机制

> **总结**：★★★★ — 值得精读。`tid` 缓存和 `CountDownLatch` 同步方案是 muduo 的核心性能基石。

---

## 1.6 Mutex / Condition

**重要程度：★★★☆☆**

```cpp
class MutexLock {
    pthread_mutex_t mutex_;
    pid_t holder_;  // 记录持有线程的 TID（调试用）
public:
    void lock();
    void unlock();
    bool isLockedByThisThread() const;
};
```

**陪读要点：**
- 封装了 POSIX mutex，提供 `MutexLockGuard` RAII 包装
- `holder_` 记录持有者 TID，`isLockedByThisThread()` 用于调试时检测死锁
- `Condition` 在 `wait()` 中调用 `pthread_cond_wait`，需要传入已加锁的 `MutexLock`

**亮点：**
- `GUARDED_BY(mutex_)` 宏（clang 的线程安全注解）——声明式标注哪些变量被哪个锁保护
- 编译期静态检查，比运行时检查更早发现并发问题

**问题：**
```cpp
class Condition {
    // wait 内部锁操作：
    int ret = 0;
    pthread_mutex_lock(&mutex_.getPthreadMutex());
    ret = pthread_cond_wait(&pcond_, &mutex_.getPthreadMutex());
    pthread_mutex_unlock(&mutex_.getPthreadMutex());
    return ret == 0;
};
```
`pthread_cond_wait` 的 spurious wakeup 处理是**正确的**（循环等待是调用者的责任，而非封装在此处）。muduo 的 API 清晰地把这个选择交给了上层。

> **总结**：★★★☆☆ — 看看封装思路即可。如果有条件，建议使用 C++20 标准库的 `std::mutex` + `std::condition_variable`。

---

## 1.7 CountDownLatch

**重要程度：★★★☆☆**

信号量同步原语：

```cpp
class CountDownLatch {
    mutable MutexLock mutex_;
    Condition condition_;
    int count_;
public:
    void wait();    // 等待 count 降到 0
    void countDown(); // count-- 并在变为 0 时 notify
    int getCount() const;
};
```

**陪读要点：**
- 本质是 Java `CountDownLatch` 的 C++ 移植
- 使用场景：一个线程等待 N 个线程全部完成（或启动）后再继续
- muduo 中用于：`Thread::start()` 等待新线程初始化完毕（N=1）和 `EventLoopThread` 中

**现代替代：**
- `std::promise` / `std::future` 可以替代一次性同步
- `std::barrier`（C++20）可以替代多线程同步

> **总结**：★★★☆☆ — 小巧精巧，但标准库已有替代。

---

## 1.8 BlockingQueue / BoundedBlockingQueue

**重要程度：★★☆☆☆**

```cpp
template<typename T>
class BlockingQueue {
    mutable MutexLock mutex_;
    Condition notEmpty_;
    std::deque<T> queue_;
public:
    void put(const T& x);        // push_back + notify
    T take();                    // wait → pop_front
};
```

**陪读要点：**
- 生产者-消费者模式的经典实现
- `put` 后通知 `notEmpty_`，`take` 等待队列非空
- `BoundedBlockingQueue` 加了上限和 `notFull_` 条件变量

**问题：**
- 直接使用 `std::queue` + `std::mutex` + `std::condition_variable` 更简洁
- muduo 内部其实也没用这个队列（它用 `std::vector<Functor>` + `swap` 技巧）

**现代替代：**
- 如果有大量生产消费场景，推荐用无锁队列（如 `moodycamel::concurrentqueue`）
- `BlockingCollection<T>`（C++ 扩展）或自己用 `std::queue` + `std::condition_variable`

> **总结**：★★☆☆☆ — 经典但过时，看看架构就可以跳过。

---

## 1.9 ThreadPool

**重要程度：★★★☆☆**

固定大小线程池，典型的生产者-消费者：

```cpp
class ThreadPool {
    MutexLock mutex_;
    Condition notEmpty_;
    Condition notFull_;
    std::vector<std::unique_ptr<muduo::Thread>> threads_;
    std::deque<Task> queue_;  // Task = std::function<void()>
    int maxQueueSize_;
};
```

**陪读要点：**
- 启动 N 个 worker 线程循环取任务执行
- `run(Task)` 投递任务，队列满时 `notFull_` 阻塞
- `stop()` 设置标志位并 `notifyAll`

**学习价值：**
- 基础线程池的完整实现，适合理解线程池原理
- 代码简洁，适合作为定制化线程池的起点

**问题：**
- 固定队列大小，满时阻塞调用方（而非拒绝或抛入独立队列）
- 没有 `std::future` 支持（不能获取任务返回值）
- 没有 work-stealing

**现代替代：**
- C++17/20 自带 `std::async` + `std::future`
- 如需真正工程级线程池，看 `BS::thread_pool` 或 OpenMP
- 游戏服务器常用定制化线程池（绑定核、优先级等）

> **总结**：★★★☆☆ — 适合学习基础原理，工程项目建议用更现代的实现。

---

## 1.10 Singleton / ThreadLocal / ThreadLocalSingleton

**重要程度：★☆☆☆☆**

**陪读要点：**
- `Singleton`：pthread_once + 静态指针，双重检查锁定（DCLP，有指令重排风险）
- `ThreadLocal<T>`：封装 `pthread_key_t`，每线程独立副本
- `ThreadLocalSingleton<T>`：每线程一个单例（线程独占）

**问题——完全不推荐：**
- `Singleton` 的实现有**内存泄漏**（没有 `delete` 逻辑）——设计如此，但现代代码不推荐
- DCLP 有指令重排风险（C++11 `std::call_once` 才是正解）
- 单例模式本身在现代 C++ 中已不推荐作为全局状态管理方式

**现代替代：**
- 全局单例 → 依赖注入
- `thread_local`（C++11 关键字），零开销，编译器保证
- `std::call_once` + `std::once_flag`

> **总结**：★☆☆☆☆ — 了解概念即可，不要在实际项目中模仿。

---

## 1.11 Logging / LogStream / LogFile

**重要程度：★★★★☆**

muduo 的日志系统是**全书最值得读的组件之一**，虽然不推荐直接用在工程项目中，但设计思路非常值得学习。

### LogStream（核心）

```cpp
class LogStream {
    static const int kSmallBuffer = 4000;
    static const int kLargeBuffer = 4000 * 1000;
    FixedBuffer<kSmallBuffer> buffer_;
public:
    LogStream& operator<<(int);
    LogStream& operator<<(double);
    LogStream& operator<<(const char*);
    LogStream& operator<<(const string&);
    // ...
};
```

**设计亮点——无需堆分配的自定义格式化：**

1. **`FixedBuffer`** — 栈上固定大小缓冲区（4KB），避免 `std::ostringstream` 的堆分配开销
2. **手写格式化** — 每个 `operator<<` 调用自定义整数/浮点数格式代码，不依赖 `sprintf`
3. **零分配**（分配在栈上）的日志行构建路径，性能远高于 `std::stringstream`

```cpp
template<int SIZE>
class FixedBuffer {
    char data_[SIZE];
    char* cur_;
public:
    void append(const char* buf, size_t len) {
        memcpy(cur_, buf, len);
        cur_ += len;
    }
    // 通过 cur_ - data_ 获取长度，不需要 strlen
};
```

### Logger

```cpp
class Logger {
    LogStream stream_;
public:
    Logger(SourceFile file, int line);
    ~Logger();  // 析构时 flush 到 g_output
    LogStream& stream() { return stream_; }
};

#define LOG_INFO Logger(__FILE__, __LINE__).stream()
```

**陪读要点——宏的关键作用：**
- `LOG_INFO << "hello"` 展开为 `Logger(__FILE__, __LINE__).stream() << "hello"`
- `Logger` 构造时写入时间戳+线程ID+日志级别前缀
- `~Logger()` 析构时调用 `g_output` 函数指针写出到目的地

### LogFile

```cpp
class LogFile {
    // 支持日志滚动（按文件大小或按日期）
    // 多线程安全写入，内部使用互斥锁
};
```

**陪读要点：**
- 滚动策略：大小阈值（如 1GB）触发滚动
- 异步日志：`AsyncLogging` 使用双缓冲（double buffering）技术，后台线程批量写入

**AsyncLogging 双缓冲机制（muduo 日志性能的关键）：**

```
前端线程 A → current buffer_
前端线程 B → current buffer_  ← 写入时不加锁（实际有锁，但极短）
           ↓ current buffer_ 满了
         换入 next buffer_ 继续写
           ↓ 通知后台线程
后台线程 swap 两个 buffer（极快的一次交换）
后台线程 将满 buffer 写到磁盘文件
```

**为什么值得读：**
- 双缓冲异步日志是高性能 C++ 服务端的标配（类似的设计在 spdlog、glog 中都有）
- 理解这个模式对设计高性能系统很有帮助
- 陈硕在书里有完整的性能测试数据

**不建议直接使用的理由：**
- muduo 日志不支持格式化占位符（只能 `<<` 拼接）
- 日志级别不够丰富（只有 TRACE/DEBUG/INFO/WARN/ERROR/FATAL）
- 配置不够灵活（不支持运行期动态调整文件路径等）

> **总结**：★★★★☆ — **强烈推荐精读 `LogStream` 的 `FixedBuffer` 和 `AsyncLogging` 的双缓冲**，但不要复制代码到工程中。生产环境推荐 spdlog、fmtlog 或 glog。

---

# 二、net/ 网络库核心

这是 muduo 的精华所在，建议按以下顺序阅读：

```
EventLoop → Channel → Poller → TimerQueue → Acceptor → TcpConnection → TcpServer → Buffer → EventLoopThreadPool
```

---

## 2.1 架构总览：Reactor 模式

muduo 采用**one loop per thread + thread pool** 架构：

```
┌──────────────────────────────────────────────┐
│               Reactor 模式                       │
│                                                  │
│  ┌──────────┐  poll/epoll                        │
│  │ EventLoop │────→ Poller ──→ active events     │
│  │           │                                    │
│  │  ┌─────────────┐                               │
│  │  │ Channel(1)  │  ← fd, events, callbacks     │
│  │  │ Channel(2)  │                               │
│  │  │ Channel(3)  │                               │
│  │  └─────────────┘                               │
│  │           │                                    │
│  │  handleEvent(fd) → read_ / write_ / close_ /  │
│  │                     error_ callback            │
│  └──────────┘                                    │
│                                                  │
│  mainLoop: 负责 accept + 分发新连接到 IO 线程    │
│  IO_loop_1: 处理一组连接的 IO 事件                │
│  IO_loop_2: 处理另一组连接                        │
│        ...                                       │
│  ThreadPool: 执行 CPU 密集型/阻塞业务             │
└──────────────────────────────────────────────────┘
```

muduo 包含两种线程池：
1. **`EventLoopThreadPool`** — 多个 IO 线程，每个运行自己的 `EventLoop::loop()`，用于分担连接 IO
2. **`ThreadPool`**（base/ 中的）— 工作线程池，用于执行回调中的耗时任务（非网络 IO）

**与 Netty 的对应关系：**

| muduo | Netty | 说明 |
|-------|-------|------|
| EventLoop | EventLoop | 事件循环 |
| Channel | Channel | IO 事件注册 |
| Poller | EventLoop 内部 | IO 多路复用 |
| Acceptor | ServerBootstrap | 接受连接 |
| TcpConnection | Channel | TCP 连接 |
| TcpServer | ServerBootstrap | 服务器 |
| EventLoopThreadPool | EventLoopGroup | IO 线程组 |
| Buffer | ByteBuf | 缓冲区 |

---

## 2.2 EventLoop — 事件循环心脏

**重要程度：★★★★★**

```cpp
// EventLoop.h 核心成员
std::unique_ptr<Poller> poller_;
std::unique_ptr<TimerQueue> timerQueue_;
int wakeupFd_;              // eventfd，用于跨线程唤醒
std::unique_ptr<Channel> wakeupChannel_;  // 监听 wakeupFd_
std::vector<Functor> pendingFunctors_;    // 跨线程投递的任务
bool looping_, quit_, eventHandling_, callingPendingFunctors_;
```

### EventLoop::loop() 事件循环

```cpp
void EventLoop::loop() {
    while (!quit_) {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        eventHandling_ = true;
        for (Channel* channel : activeChannels_) {
            channel->handleEvent(pollReturnTime_);
        }
        eventHandling_ = false;
        doPendingFunctors();  // ★ 执行跨线程投递的任务
    }
}
```

**陪读要点——关键设计：**

1. **One Loop Per Thread（核心约束）**
   ```cpp
   __thread EventLoop* t_loopInThisThread = 0;
   ```
   - 每个线程最多一个 `EventLoop`
   - 构造时检查 `t_loopInThisThread` 是否已存在，存在则 `LOG_FATAL`
   - `assertInLoopThread()` 在关键操作中检查调用线程

2. **wakeupFd_（eventfd）— 跨线程唤醒**
   ```cpp
   int createEventfd() { return ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC); }
   void wakeup() { uint64_t one = 1; write(wakeupFd_, &one, sizeof one); }
   ```
   - 其他线程调用 `runInLoop(Functor)` 时，需要唤醒 IO 线程从 `epoll_wait` 返回
   - 使用 **eventfd** 而非 pipe，因为 eventfd 更轻量（一个 fd 代替一对 pipe fd）
   - 写 8 字节到 eventfd → epoll 触发可读 → `loop()` 处理完 IO 事件后执行 `doPendingFunctors()`

3. **doPendingFunctors() — 跨线程异步执行**
   ```cpp
   void doPendingFunctors() {
       std::vector<Functor> functors;
       {
           MutexLockGuard lock(mutex_);
           functors.swap(pendingFunctors_);  // 极短的临界区
       }
       for (const Functor& functor : functors) functor();
   }
   ```
   **swap + 放锁**的技巧避免了长时间持有锁，是 muduo 线程模型的核心细节。

4. **runInLoop / queueInLoop — 跨线程任务投递**
   ```cpp
   void runInLoop(Functor cb) {
       if (isInLoopThread()) cb();           // 同线程：直接执行
       else queueInLoop(std::move(cb));       // 异线程：入队 + wakeup
   }
   void queueInLoop(Functor cb) {
       MutexLockGuard lock(mutex_);
       pendingFunctors_.push_back(std::move(cb));
       if (!isInLoopThread() || callingPendingFunctors_) wakeup();
   }
   ```
   **为什么要检查 `callingPendingFunctors_`？**
   如果 `doPendingFunctors()` 正在执行，当前线程处理完回调后又再次执行了 `doPendingFunctors()`——但没有再次 `epoll_wait`。如果期间另一个线程也 push 了一个 functor，它不会被这次执行处理。所以 `callingPendingFunctors_` 触发的 wakeup 可以确保**下一次** `loop()` 迭代会处理这个新任务。

**★★★★★ = 必须精读，逐行理解：**

- `EventLoop::loop()` 的执行流
- `eventfd` 跨线程唤醒机制
- `runInLoop/queueInLoop` 的线程安全设计
- `doPendingFunctors` 的 swap 技巧
- One loop per thread 的约束和检查

---

## 2.3 Channel — 事件分发器

**重要程度：★★★★★**

```cpp
class Channel {
    EventLoop* loop_;
    const int fd_;
    int events_;       // 感兴趣的事件（EPOLLIN/OUT 等）
    int revents_;      // poller 返回的活跃事件
    ReadEventCallback readCallback_;     // std::function<void(Timestamp)>
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
    std::weak_ptr<void> tie_;  // 保活机制
};
```

**Channel 不拥有 fd**，它只是 f的观测者和通知者。fd 的所有者（通常是 `TcpConnection` 或 `Acceptor`）负责生命周期。

### 核心方法

```cpp
void enableReading();   // 注册读事件
void enableWriting();   // 注册写事件
void disableAll();      // 关闭所有事件
void handleEvent(Timestamp receiveTime);  // 根据 revents 分发到具体回调
```

### tie() — 保活机制（重要设计）

```cpp
void Channel::handleEventWithGuard(Timestamp receiveTime) {
    eventHandling_ = true;
    // ★ 关键：通过 shared_ptr tie 防止对象在回调执行中被销毁
    if (tied_) {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard) handleEventWithGuard(receiveTime);
    } else {
        handleEventWithGuard(receiveTime);
    }
}
```

**为什么需要 `tie()`？**

假设这样一个场景：`TcpConnection` 的可读回调正在执行，执行过程中 `TcpConnection` 被其他操作销毁了（比如 `forceClose()`），那么 `this`（`TcpConnection` 对象）变成悬空指针，回调中访问成员变量会 crash。

**`tie()` 的解法：**
- `TcpConnection` 构造时将自身的 `shared_ptr` 作为 `weak_ptr` 传给 `Channel::tie()`
- `handleEvent` 中先 `tie_.lock()` 提升为 `shared_ptr`，在回调执行期间保活
- 回调完成后 `shared_ptr` 析构，`TcpConnection` 可安全释放

**陪读要点：**
- `handleEvent()` 内部根据 `revents_` 判断事件类型并调用对应回调
- `update()` 调用 `loop_->updateChannel(this)` → `poller_->updateChannel(channel)`
- 所有 `enableXXX/disableXXX` 都会触发 `update()`，确保事件注册实时生效
- Channel 没有读缓冲——可读数据由回调的调用者决定如何读取

> **★★★★★ = 必须精读**。Channel 是 Reactor 模式中组件（fd）和事件循环之间的桥梁。tie 机制是所有异步网络库都要处理的难题。

---

## 2.4 Poller / EPollPoller — IO 多路复用

**重要程度：★★★★☆**

```cpp
// Poller.h — 基类
class Poller {
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;
};
```

muduo 通过工厂方法支持多后端：
```cpp
// Poller.cc
Poller* Poller::newDefaultPoller(EventLoop* loop) {
    if (::getenv("MUDUO_USE_POLL")) return new PollPoller(loop);
    else return new EPollPoller(loop);
}
```

### EPollPoller 核心

```cpp
class EPollPoller : Poller {
    int epollfd_;
    EventList events_;  // std::vector<struct epoll_event>
    ChannelMap channels_;  // std::map<int, Channel*>  fd → Channel 映射

    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
    void update(int operation, Channel* channel);
};
```

### fillActiveChannels — 将 epoll 事件映射为 Channel 回调

```cpp
Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels) {
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(),
                                 static_cast<int>(events_.size()), timeoutMs);
    fillActiveChannels(numEvents, activeChannels);
    return Timestamp::now();
}

void EPollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const {
    for (int i = 0; i < numEvents; ++i) {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);  // ★
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
    }
}
```

**陪读要点——关键细节：**

1. **`events_[i].data.ptr` vs `data.fd`**
   muduo 使用 `data.ptr` 直接指向 `Channel*`，比用 `data.fd` 再查 map 更快

2. **`update()` 封装 epoll_ctl**
   ```cpp
   void EPollPoller::update(int operation, Channel* channel) {
       struct epoll_event event;
       event.events = channel->events();
       event.data.ptr = channel;  // 关键：存指针而非 fd
       ::epoll_ctl(epollfd_, operation, channel->fd(), &event);
   }
   ```

3. **Channel 状态追踪**
   ```cpp
   // index_ 的状态机：
   const int kNew = -1;      // 未添加到 poller
   const int kAdded = 1;     // 已添加到 poller
   const int kDeleted = 2;   // 从 poller 删除
   ```
   `kNew → updateChannel → epoll_ctl(ADD) → kAdded`
   `kAdded → removeChannel → epoll_ctl(DEL) → kDeleted`

> **★★★★☆ = 重要但不一定逐行读**。理解 epoll 封装的核心逻辑（data.ptr 技巧、状态机）即可，`epoll` API 本身是固定的。

---

## 2.5 TimerQueue — 定时器实现

**重要程度：★★★★☆**

muduo 使用 `timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK)` 将定时器集成到 epoll 事件循环中，**不依赖信号**。

```cpp
class TimerQueue {
    int timerfd_;
    Channel timerfdChannel_;
    TimerList timers_;   // std::set<std::pair<Timestamp, Timer*>>
    ActiveTimerSet activeTimers_;
};
```

### 核心机制

```cpp
void TimerQueue::handleRead() {
    // 1. 读取 timerfd 以清理事件
    read(timerfd_, &exp, sizeof exp);
    // 2. 获取到期定时器
    std::vector<Entry> expired = getExpired(now);
    // 3. 执行回调
    callingExpiredTimers_ = true;
    for (const Entry& it : expired) it.second->run();
    callingExpiredTimers_ = false;
    // 4. 重置重复定时器
    reset(expired, now);
}
```

**陪读要点：**

1. **`timerfd` 的优势（★★★★★ 强烈推荐）**
   - 无须信号处理（无 SIGALRM）
   - 可用 `read()` 消费事件（类似普通 fd）
   - 完美融入 epoll 事件循环
   - 精度到纳秒

2. **`std::set` 作为定时器容器**
   - `Entry = std::pair<Timestamp, Timer*>` → 按到期时间排序
   - 到期检查：`getExpired` 从 `begin()` 开始取出所有 `Timestamp <= now` 的条目
   - `O(log N)` 的增删和 `O(K)` 的取出到期定时器

3. **重复定时器的处理**
   - `reset()` 将过期的重复定时器重新计算下次到期时间后插入 `timers_`
   - 如果定时器在回调执行期间被取消，`cancelingTimers_` 列表会防止它被重新添加

**性能评价：**
- `std::set` 的 `O(log N)` 对于定时器数量不多的场景完全够用
- 如果定时器数量 > 10万，可以考虑时间轮（muduo 未实现）

> **★★★★☆ = 建议精读**。timerfd + set 的组合是 Linux 网络库定时器的标准实践。理解这个模式可以迁移到任何事件驱动框架。

---

## 2.6 Socket / SocketsOps — RAII 封装

**重要程度：★★★☆☆**

```cpp
class Socket {
    const int sockfd_;
public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}
    ~Socket();  // ::close(sockfd_)
    int fd() const { return sockfd_; }
    void bindAddress(const InetAddress& localaddr);
    void listen();
    int accept(InetAddress* peeraddr);
    void setReuseAddr(bool on);
    void shutdownWrite();
};
```

**陪读要点：**
- RAII 封装，析构时自动 `close(fd)`
- `Socket` 不可拷贝（`noncopyable`）
- `SocketsOps` 是一组 C 风格的工具函数（`read`, `write`, `close`, `toIpPort` 等）

**对比我们的 `src/base/socket.hpp`：**
- 我们的实现更现代（移动语义、`set_nonblocking` 等）
- muduo 的版本更基础——它的重点在封装 epoll 事件，而非 socket 本身

> **总结**：★★★☆☆ — 看 RAII 思路，不必逐行。

---

## 2.7 InetAddress — 网络地址

**重要程度：★★★☆☆**

```cpp
class InetAddress {
    union {
        struct sockaddr_in addr_;
        struct sockaddr_in6 addr6_;
    };
};
```

**陪读要点：**
- 封装 `sockaddr_in` / `sockaddr_in6`
- 支持 IPv4 和 IPv6（通过 `sockaddr_in6` 足够大来存 `sockaddr_in`）
- `toIpPort()` 返回 `"1.2.3.4:5678"` 格式字符串

> **总结**：★★★☆☆ — 标准封装，快速过。

---

## 2.8 Acceptor — 接受连接

**重要程度：★★★☆☆**

```cpp
class Acceptor {
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;  // void(int fd, InetAddress)
    int idleFd_;  // ★ 预留 fd，防止文件描述符耗尽
};
```

### 核心：handleRead

```cpp
void Acceptor::handleRead() {
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0) {
        if (newConnectionCallback_) newConnectionCallback_(connfd, peerAddr);
    } else {
        // EMFILE 处理：关闭 idleFd_，accept，再重新打开 idleFd_
        if (errno == EMFILE) {
            ::close(idleFd_);
            idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
            ::close(idleFd_);
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}
```

**陪读要点——idleFd_ 妙用（★★★★☆）：**

当进程打开的文件数达到上限，`accept()` 返回 `EMFILE`。传统做法是直接 close 新连接，但**客户端的 connect 已经成功了**，只是服务端无法确认。此时：

1. 关闭 `idleFd_`（占位 fd）释放一个 fd 位置
2. 立刻 `accept()` 拿到新连接
3. 关闭这个新连接（`close` 释放 SYN 队列）
4. 重新打开 `/dev/null` 作为占位 fd

这样客户端不会看到 connect 失败，而是连接后被服务端立刻关闭——比 connect 直接超时/拒绝更友好。

**但注意**：这种 EMFILE 处理在 `SO_REUSEPORT` 多进程模式下效果有限。

> **总结**：★★★☆☆ — 理解 `acceptChannel_` 把监听 fd 接入事件循环的思路即可，`idleFd_` 是个有趣的边缘技巧。

---

## 2.9 Connector — 发起连接

**重要程度：★★☆☆☆**

`Connector` 是对 `connect()` 的异步封装，使用非阻塞 `connect` + epoll 检测连接建立完成。

**陪读要点：**
- 非阻塞 connect 的典型流程：
  1. `socket()` + `setNonBlockAndCloseOnExec()`
  2. `connect(fd, addr)` → 返回 `EINPROGRESS`
  3. 注册 `EPOLLOUT` 事件到 epoll
  4. `EPOLLOUT` 触发 → 检查 `SO_ERROR` 确认连接成功
- muduo 的 `Connector` 支持重试、超时等

> **总结**：★★☆☆☆ — 了解非阻塞 connect 的套路即可，工程中较少自己实现（一般用库自带）。

---

## 2.10 TcpConnection — TCP 连接

**重要程度：★★★★★**

muduo 最复杂的类，管理一个已建立的 TCP 连接的整个生命周期。

```cpp
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
    EventLoop* loop_;
    std::unique_ptr<Socket> socket_;     // RAII socket，析构时 close
    std::unique_ptr<Channel> channel_;    // IO 事件通道
    Buffer inputBuffer_;                  // 读缓冲
    Buffer outputBuffer_;                 // 写缓冲
    StateE state_;                        // 连接状态机
    // 回调：
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
};
```

### 状态机

```
kDisconnected → kConnecting → kConnected → kDisconnecting → kDisconnected
                                  ↑
                             connectEstablished()
                                  ↓
                             connectDestroyed()
```

### handleRead — ET 风格读取

```cpp
void TcpConnection::handleRead(Timestamp receiveTime) {
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0) {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    } else if (n == 0) {
        handleClose();
    } else {
        errno = savedErrno;
        handleError();
    }
}
```

**陪读要点：**
- `inputBuffer_.readFd()` 内部使用 `readv` 配合额外栈上缓冲区，实现单次读取
- 数据读入 `inputBuffer_` 后，触发 `messageCallback_`，传入 `Buffer*` 让用户自行解析

### handleWrite — ET 风格写入

```cpp
void TcpConnection::handleWrite() {
    ssize_t n = ::write(channel_->fd(),
                        outputBuffer_.peek(),
                        outputBuffer_.readableBytes());
    if (n > 0) {
        outputBuffer_.retrieve(n);
        if (outputBuffer_.readableBytes() == 0) {
            channel_->disableWriting();  // 写完了，关闭 EPOLLOUT
            if (writeCompleteCallback_) writeCompleteCallback_(shared_from_this());
        }
    }
}
```

**陪读要点：**
- 只在 `outputBuffer_` 有数据时才注册 `EPOLLOUT`（**边写边注册**，不是一直开启）
- 写完自动 `disableWriting()`，避免 busy loop
- `send()` 内部：如果 outputBuffer 为空且处于 IO 线程，直接 `write()`（尝试零拷贝优化）

### send — 智能发送

```cpp
void TcpConnection::send(const StringPiece& message) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread())
            sendInLoop(message);
        else
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, message.as_string()));
    }
}
```

如果调用方在 IO 线程：直接尝试写入。
如果不在：入队到 `EventLoop::pendingFunctors_` 并由 IO 线程执行。

### 连接生命周期管理

```cpp
void TcpConnection::connectEstablished() {
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();
    connectionCallback_(shared_from_this());
}
```

**重要——`tie()` 防止回调中的析构：**
- `channel_->tie(shared_from_this())` 建立了 `weak_ptr<TcpConnection>`
- 即使外部所有 `shared_ptr` 都被释放，`handleRead` 中的提升操作也能确保在函数执行期间 `TcpConnection` 不会被销毁

> **★★★★★ = 必须精读，逐行理解**。TcpConnection 的生命周期管理（shared_from_this + tie + 状态机）是现代 C++ 网络库最精妙也最复杂的部分。

---

## 2.11 TcpServer — TCP 服务器

**重要程度：★★★★★**

```cpp
class TcpServer {
    EventLoop* loop_;                          // acceptor 所在 loop
    std::unique_ptr<Acceptor> acceptor_;       // 接受连接
    std::shared_ptr<EventLoopThreadPool> threadPool_;  // IO 线程池
    ConnectionMap connections_;                // std::map<string, TcpConnectionPtr>
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
};
```

### newConnection — 新连接创建

```cpp
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr) {
    // 选择一个 IO loop（round-robin）
    EventLoop* ioLoop = threadPool_->getNextLoop();
    // 构造连接名
    char buf[64];
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    string connName = name_ + buf;
    // 创建 TcpConnection
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn;
    // 设置回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, _1));
    // 在 IO loop 中执行 connectionEstablished
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}
```

### 多线程模型（★★★☆☆ 理解即可）

```
mainLoop (acceptor loop)
    │
    │  newConnection() 发生后：
    │  1. 从 EventLoopThreadPool 中轮询选择一个 IO loop
    │  2. 在该 IO loop 上注册并管理这个连接
    │
    ├── IO Loop 1  →  连接 A, 连接 C, ...
    ├── IO Loop 2  →  连接 B, 连接 D, ...
    └── IO Loop 3  →  连接 E, 连接 F, ...

EventLoop 之间的通信：
    runInLoop(Functor) → queueInLoop → wakeup(eventfd) → loop() 执行
```

**陪读要点——核心洞察：**
- `TcpServer` 本身不处理连接读写——它只负责 accept 和分发
- 所有 IO 事件回调都在目标 IO loop 线程中执行
- `removeConnection` 需要跨线程执行：先调用方 IO loop，再回到 main loop 从 `connections_` map 中删除

> **★★★★★ = 核心入口，理解 TcpServer 的架构就能理解 muduo 的全貌**。重点在于 `newConnection` 的多线程分发和 `removeConnection` 的线程安全处理。

---

## 2.12 TcpClient

**重要程度：★★☆☆☆**

- `TcpClient` 是对 `Connector` + `TcpConnection` 的封装
- 需要重连逻辑的场景可以使用

> **总结**：快速过，服务器端开发不太需要。

---

## 2.13 Buffer — IO 缓冲区

**重要程度：★★★★★**

muduo 的 `Buffer` 设计直接源自 **Netty 的 ChannelBuffer**，是全书工程价值最高的组件之一。

```cpp
class Buffer {
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;

    // 三块区域：
    // [0, readerIndex_)           → prependable（预留8字节，用于添加帧头）
    // [readerIndex_, writerIndex_) → readable（应用层可读数据）
    // [writerIndex_, size())       → writable（可写入的空间）
};
```

### 内存布局图

```
 +-------------------+------------------+------------------+
 | prependable bytes |  readable bytes  |  writable bytes  |
 |                   |     (CONTENT)    |                  |
 +-------------------+------------------+------------------+
 |                   |                  |                  |
 0      <=      readerIndex   <=   writerIndex    <=     size
```

### 核心操作

```cpp
// 读操作
size_t readableBytes() const;   // writerIndex_ - readerIndex_
const char* peek() const;       // begin() + readerIndex_
void retrieve(size_t len);      // readerIndex_ 前进

// 写操作
void append(const char* data, size_t len);  // ensureWritable + copy
void ensureWritableBytes(size_t len);
char* beginWrite();             // begin() + writerIndex_
void hasWritten(size_t len);    // writerIndex_ 前进

// 读写协议头
void appendInt32(int32_t x);    // 大端写入 int32
int32_t readInt32();            // 大端读取 int32
void prependInt32(int32_t x);   // 在 prependable 区写入（栈协议头）
```

### makeSpace — 巧妙的内存整理

```cpp
void Buffer::makeSpace(size_t len) {
    if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
        buffer_.resize(writerIndex_ + len);  // 直接扩容
    } else {
        // 前移数据：把 readable 区域挪到 prependable 区域
        size_t readable = readableBytes();
        std::copy(begin() + readerIndex_, begin() + writerIndex_,
                  begin() + kCheapPrepend);
        readerIndex_ = kCheapPrepend;
        writerIndex_ = readerIndex_ + readable;
    }
}
```

**陪读要点——关键洞察：**

1. **prependable 区的用途**
   - 默认 8 字节头部预留
   - 某些协议需要在 payload 前加帧头（如 length-prefix），可以直接 `prependInt32(length)` 而不需要整体 memmove

2. **readFd — 栈上辅助读**
   ```cpp
   ssize_t Buffer::readFd(int fd, int* savedErrno) {
       char extrabuf[65536];
       struct iovec vec[2];
       vec[0].iov_base = begin() + writerIndex_;
       vec[0].iov_len = writableBytes();
       vec[1].iov_base = extrabuf;
       vec[1].iov_len = sizeof(extrabuf);
       const ssize_t n = ::readv(fd, vec, 2);
       if (n < 0) { *savedErrno = errno; }
       else if (static_cast<size_t>(n) <= writableBytes()) {
           writerIndex_ += n;
       } else {
           writerIndex_ = buffer_.size();
           append(extrabuf, n - writableBytes());
       }
       return n;
   }
   ```
   **精妙之处**：
   - 使用 `readv` + 两段式 iovec：第一段直接写入 buffer 的 writable 区，第二段是栈上 64KB 额外空间
   - 如果数据量小（<= writableBytes），直接写入 buffer，零额外分配
   - 如果数据量大，除了填满 buffer，多余数据从 extrabuf 追加到 buffer（触发扩容）
   - **全程最多 2 次 readv + 1 次 memcpy，没有额外的系统调用**

3. **`std::vector` 作为底层存储**
   - 支持 `shrink()` 和 `capacity()` 管理内存
   - 扩容策略：`resize`（可能 2 倍增长）
   - 和 `std::string` 相比：`std::vector` 可以持有二进制数据（含 `\0`）

> **★★★★★ = 强烈建议完整抄一遍写在自己的代码里**。Buffer 设计是网络编程中最实用的组件之一，不依赖任何库，直接在项目中复用。

---

## 2.14 EventLoopThread — 线程绑定

**重要程度：★★★☆☆**

```cpp
class EventLoopThread {
    EventLoop* loop_;
    Thread thread_;
    MutexLock mutex_;
    Condition cond_;
    ThreadInitCallback callback_;
};
```

### startLoop — 启动线程并等待 EventLoop 就绪

```cpp
EventLoop* EventLoopThread::startLoop() {
    thread_.start();  // 创建线程，运行 threadFunc
    {
        MutexLockGuard lock(mutex_);
        while (loop_ == NULL) cond_.wait();  // 等待 EventLoop 创建完成
    }
    return loop_;
}

void EventLoopThread::threadFunc() {
    EventLoop loop;  // 在栈上创建 EventLoop（自动绑定到本线程）
    {
        MutexLockGuard lock(mutex_);
        loop_ = &loop;
        cond_.notify();
    }
    loop.loop();  // 阻塞事件循环
    loop_ = NULL;
}
```

**陪读要点：**
- `EventLoop` 在栈上创建——`threadFunc` 返回时自动析构
- `startLoop()` 会阻塞直到新线程创建好 `EventLoop`（用条件变量同步）
- 保证"一个线程一个 EventLoop"的不变量

> **总结**：★★★☆☆ — 理解"在栈上创建 EventLoop"和"条件变量同步"即可。

---

## 2.15 EventLoopThreadPool — IO 线程池

**重要程度：★★★★☆**

```cpp
class EventLoopThreadPool {
    EventLoop* baseLoop_;                         // acceptor 所在 loop
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
    int numThreads_;
    int next_;  // round-robin 计数器
};
```

### getNextLoop — 轮询分配

```cpp
EventLoop* EventLoopThreadPool::getNextLoop() {
    if (loops_.empty()) return baseLoop_;   // 单线程模式
    EventLoop* loop = loops_[next_];
    ++next_;
    if (static_cast<size_t>(next_) >= loops_.size())
        next_ = 0;
    return loop;
}
```

**陪读要点：**
- 支持 3 种模式：
  - `numThreads_ = 0`：所有 IO 在 main loop 处理（单线程 Reactor）
  - `numThreads_ = 1`：所有连接在单独的 IO 线程处理
  - `numThreads_ = N`：N 个 IO 线程，新连接轮询分配到其中一个
- `getLoopForHash(hashCode)` 支持哈希绑定（同一个客户端的连接总是分配到同一个 IO 线程）

**高性能配置建议：**
- IO 线程数 = CPU 核数（纯 IO 密集型）或 2×CPU 核数（需要处理少量业务）
- 不要创建超过 CPU 核数的 IO 线程，否则上下文切换反而降低性能

> **★★★★☆ = 理解多线程 Reactor 模型的必读组件**。

---

# 三、HTTP 模块

## 3.1 HttpRequest / HttpResponse

**重要程度：★★☆☆☆**

数据模型类，存储 HTTP 请求/响应的结构化字段：

```cpp
class HttpRequest {
    Method method_;       // kGet, kPost, kPut, kDelete, ...
    Version version_;     // kHttp10, kHttp11
    string path_;
    string query_;
    Timestamp receiveTime_;
    std::map<string, string> headers_;
};

class HttpResponse {
    HttpStatusCode statusCode_;   // k200Ok, k404NotFound 等
    string statusMessage_;
    std::map<string, string> headers_;
    string body_;
    bool closeConnection_;
};
```

> **总结**：快速过，标准数据模型。

---

## 3.2 HttpContext — 状态机解析

**重要程度：★★★☆☆**

```cpp
class HttpContext {
    enum HttpRequestParseState {
        kExpectRequestLine,   // 正在解析请求行：GET /path HTTP/1.1
        kExpectHeaders,       // 正在解析请求头
        kExpectBody,          // 正在解析请求体
        kGotAll,              // 解析完成
    };
    HttpRequestParseState state_;
    HttpRequest request_;
};
```

### parseRequest — HTTP 解析状态机

```cpp
bool HttpContext::parseRequest(Buffer* buf, Timestamp receiveTime) {
    bool ok = true;
    bool hasMore = true;
    while (hasMore) {
        if (state_ == kExpectRequestLine) {
            const char* crlf = buf->findCRLF();
            if (crlf) {
                ok = processRequestLine(buf->peek(), crlf);
                buf->retrieveUntil(crlf + 2);  // 跳过 \r\n
                state_ = kExpectHeaders;
            } else {
                hasMore = false;
            }
        } else if (state_ == kExpectHeaders) {
            const char* crlf = buf->findCRLF();
            if (crlf) {
                const char* colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf) {
                    request_.addHeader(buf->peek(), colon, crlf);
                } else {
                    state_ = kGotAll;  // 空行 → 头部结束
                }
                buf->retrieveUntil(crlf + 2);
            } else {
                hasMore = false;
            }
        }
    }
    return ok;
}
```

**陪读要点——状态机思路：**
- HTTP 解析在非阻塞场景下必须使用状态机
- `kExpectRequestLine` → 读 `\r\n` → `kExpectHeaders` → 读空行 → `kGotAll`
- 状态机天然支持**增量解析**（数据一次没读完，下次 `parseRequest` 从上次状态继续）

**局限性：**
- 不支持 chunked transfer encoding
- 不支持 HTTP keep-alive（默认短连接）
- 不支持 body 解析（Content-Length 没有处理）
- **生产环境绝不可用于真正 HTTP 服务**——只适合作教学示例

> **总结**：★★★☆☆ — 状态机解析思路值得学，但具体实现太简略。真实项目推荐用 `llhttp` 或 `nghttp2`。

---

## 3.3 HttpServer

**重要程度：★★☆☆☆**

```cpp
class HttpServer {
    TcpServer server_;
    HttpCallback httpCallback_;
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp t) {
        HttpContext context;
        if (!context.parseRequest(buf, t)) {
            conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
            conn->shutdown();
        }
        if (context.gotAll()) {
            onRequest(conn, context.request());
            context.reset();
        }
    }
};
```

**局限：**
- 每个连接只解析**一个**请求（没有 keep-alive）
- body 不解析到 HttpRequest 中
- 没有请求拼接/分块支持
- 纯粹的教学用途

> **总结**：★★☆☆☆ — 了解如何把 TcpServer 和 HttpContext 拼起来即可。

---

# 四、设计模式总结

| 模式 | muduo 中的体现 | 学习价值 |
|------|---------------|---------|
| **Reactor** | EventLoop + Channel + Poller | ★★★★★ |
| **Acceptor-Connector** | Acceptor（被动）+ Connector（主动） | ★★★☆☆ |
| **One Loop Per Thread** | EventLoopThread + EventLoopThreadPool | ★★★★★ |
| **非阻塞 IO + ET** | 全异步读写，边沿触发 | ★★★★★ |
| **shared_from_this + tie** | TcpConnection 生命周期管理 | ★★★★★ |
| **RAII** | Socket, Channel, MutexLockGuard | ★★★★☆ |
| **回调驱动** | setXxxCallback + 事件触发 | ★★★★☆ |
| **双缓冲** | AsyncLogging | ★★★★☆ |
| **状态机解析** | HttpContext (HTTP 解析) | ★★★☆☆ |
| **Observer** | Channel 观察 fd 事件 | ★★★☆☆ |

---

# 五、muduo 的局限性与现代替代

| 方面 | muduo 的问题 | 现代替代 |
|------|-------------|---------|
| **C++ 标准** | 大量 boost（shared_ptr, bind, function） | 全部可用 `std::` 替代 |
| **线程池** | 固定大小，无 work-stealing | BS::thread_pool, Intel TBB |
| **HTTP** | 仅最小教学实现 | `llhttp`（Node.js 核心解析器）, `nghttp2` |
| **日志** | 功能有限，配置不灵活 | spdlog, fmtlog, glog |
| **协程** | 没有协程支持 | C++20 `std::coroutine`, libco, ucontext |
| **SSL** | 不支持 | OpenSSL, BoringSSL 集成 |
| **DNS 解析** | 阻塞式 | c-ares（异步 DNS） |
| **TLS/加密** | 不支持 | mbedTLS, OpenSSL |
| **内存分配** | 常规 new/delete | jemalloc, tcmalloc |
| **时间轮定时器** | 只有 std::set 定时器 | 时间轮（大规模定时器适用） |
| **序列化** | 无 | protobuf, flatbuffers, msgpack |
| **信号处理** | 简单的 SIGPIPE 忽略 | signalfd |

**那为什么还要学 muduo？**

因为 muduo 的**架构**在现代网络库中仍然是最经典的——Reactor + one loop per thread。理解 muduo 之后：
- 再去看 Netty（Java）能理解它们为什么长得像
- 再去看 libevent/libuv（C）能理解它们之间的异同
- 再去实现自己的网络框架时能避开 muduo 踩过的坑

---

# 六、阅读路线推荐

根据学习目的，选择不同的阅读路径：

## 路线 A：快速掌握核心架构（2-3 小时）

```
EventLoop.(h|cc)           ← 事件循环心脏
Channel.h                   ← 事件分发
TcpConnection.h             ← 连接管理
TcpServer.h                 ← 服务器入口
Buffer.h                    ← 缓冲区设计
```

## 路线 B：全面学习（4-6 小时）

```
路线 A 全部  +
EventLoopThreadPool.(h|cc)  ← 多线程模型
Acceptor.(h|cc)             ← 连接接受
Poller.h + EPollPoller.h    ← IO 多路复用
TimerQueue.(h|cc)           ← 定时器
LogStream.h + AsyncLogging  ← 日志双缓冲
Callbacks.h                 ← 回调设计
```

## 路线 C：按实际需求选读

- **需要设计高性能 Buffer** → `Buffer.h`（★★★★★）
- **需要线程安全的异步日志** → `AsyncLogging.h` + `LogStream.h`（★★★★☆）
- **需要理解 Reactor 模式** → `EventLoop.h` + `Channel.h`（★★★★★）
- **需要多线程 IO 模型** → `EventLoopThreadPool.h` + `EventLoopThread.h`（★★★★☆）
- **需要 epoll 封装** → `EPollPoller.h`（★★★★☆）
- **需要定制网络服务器** → `TcpServer.h` + `TcpConnection.h`（★★★★★）

## 对比阅读建议

muduo 和其它网络库的异同：

| 特性 | muduo | libevent | libuv | Boost.Asio |
|------|-------|----------|-------|-----------|
| 语言 | C++11 | C | C | C++ |
| Reactor | ✔ | ✔ | ✔ | Proactor |
| 多线程 | ✔ | ✔ | ✔ | ✔ |
| HTTP | 教学级 | 部分支持 | 部分 | 无 |
| DNS | 阻塞 | 异步 | 异步 | 异步 |
| 文件 IO | 无 | 无 | ✔ | 无 |
| pipe | 无 | ✔ | ✔ | 无 |
| 信号 | 简单 | ✔ | ✔ | 无 |
| 协程 | 无 | 无 | ✔ | ✔（spawn） |

---

## 总结速查表

| 组件 | 重要程度 | 精读建议 |
|------|---------|---------|
| **EventLoop** | ★★★★★ | 全文背诵 |
| **Channel** | ★★★★★ | 理解 tie 机制 |
| **EPollPoller** | ★★★★☆ | 理解 data.ptr 技巧 |
| **TcpConnection** | ★★★★★ | 生命周期管理精读 |
| **TcpServer** | ★★★★★ | 架构入口 |
| **Buffer** | ★★★★★ | 全文复用 |
| **TimerQueue** | ★★★★☆ | timerfd 集成 |
| **EventLoopThreadPool** | ★★★★☆ | 多线程模型 |
| **LogStream** | ★★★★☆ | FixedBuffer 设计 |
| **AsyncLogging** | ★★★★☆ | 双缓冲技巧 |
| **Acceptor** | ★★★☆☆ | idleFd 技巧 |
| **Thread** | ★★★★☆ | tid 缓存 |
| **ThreadPool** | ★★★☆☆ | 基础线程池 |
| **HttpContext** | ★★★☆☆ | 状态机思想 |
| **Atomic** | ★★☆☆☆ | 用 std::atomic 替代 |
| **BlockingQueue** | ★★☆☆☆ | 用标准库替代 |
| **Singleton** | ★☆☆☆☆ | 不推荐 |
| **StringPiece** | ★☆☆☆☆ | 用 std::string_view 替代 |
