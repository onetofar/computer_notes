---
title: Muduo 导读—EventLoop 核心
category: 网络编程
tags:
  - 源码分析
  - muduo
  - EventLoop
  - Reactor
difficulty: 深入
source: src/muduo_source/muduo/net/EventLoop.cc
created: 2026-07-24
modified: 2026-07-24
maturity: 完善
skills:
  - obsidian-markdown
---

> [!info] 本文定位
> 本文是 [[muduo全量代码导读]] 的拆分文档，专注 **EventLoop 核心**：循环结构、构造组装、跨线程任务投递。建议边开源文件边读。

---

## 第一章：从一个最简单的示例跑起来

> **目标**：先跑通一个 echo server，建立对 muduo "用起来什么样"的体感。

### 1.1 打开 examples/simple/echo

> 注意：muduo 的示例将业务逻辑封装在独立的类中（`EchoServer`），`main()` 只做编排。

☐ 打开 [`examples/simple/echo/main.cc`](src/muduo_source/examples/simple/echo/main.cc)

```cpp
int main() {
    muduo::net::EventLoop loop;
    muduo::net::InetAddress listenAddr(2007);
    EchoServer server(&loop, listenAddr);
    server.start();
    loop.loop();  // 阻塞在这里
}
```

☐ 再打开 [`examples/simple/echo/echo.h`](src/muduo_source/examples/simple/echo/echo.h) 和 [`echo.cc`](src/muduo_source/examples/simple/echo/echo.cc)

回调的注册发生在 `EchoServer` 的构造函数（`echo.cc` L16-L19）：

```cpp
EchoServer::EchoServer(EventLoop* loop, const InetAddress& listenAddr)
  : server_(loop, listenAddr, "EchoServer") {
    server_.setConnectionCallback(
        std::bind(&EchoServer::onConnection, this, _1));
    server_.setMessageCallback(
        std::bind(&EchoServer::onMessage, this, _1, _2, _3));
}
```

实际的 echo 逻辑在 `onMessage`（`echo.cc` L34-L42）：

```cpp
void EchoServer::onMessage(const TcpConnectionPtr& conn,
                           Buffer* buf, Timestamp time) {
    muduo::string msg(buf->retrieveAllAsString());
    LOG_INFO << conn->name() << " echo " << msg.size() << " bytes";
    conn->send(msg);  // 收到什么，发回什么
}
```

**看完这段要回答的问题**（在代码里找到答案）：
1. ☐ `conn->send(buf)` 是在哪个线程执行的？（看 `TcpConnection::send` 的实现）
2. ☐ 如果 `loop.loop()` 阻塞了，`setMessageCallback` 怎么被调用的？

带着这两个问题往下读。

---

## 第二章：EventLoop——一切从这里开始

> **目标**：理解 `loop.loop()` 到底在干什么。

### 2.1 核心循环

☐ 打开 [`muduo/net/EventLoop.cc`](src/muduo_source/muduo/net/EventLoop.cc)，找到 `loop()` 函数（L103）

```cpp
void EventLoop::loop() {
    while (!quit_) {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        //                  ↑ 第 2 章先跳过，只看它是怎么拿到"就绪事件"的
        
        for (Channel* channel : activeChannels_) {
            currentActiveChannel_ = channel;
            currentActiveChannel_->handleEvent(pollReturnTime_);
            //               ↑ 这是关键：每个 Channel 自己处理事件
        }
        doPendingFunctors();  // 第 5 章再讲
    }
}
```

**☐ 动手**：把这个循环和你 `game_gateway_v1.cpp` L358-L387 的 while 循环并排看。结构完全一样，只是你的循环用 if-else 分流，这里用 `channel->handleEvent()` 分流。

### 2.2 构造函数里发生了什么

☐ 回到 `EventLoop.cc`，找到构造函数（L64-L91）

关键看这行：

```cpp
wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
wakeupChannel_->enableReading();
```

**EventLoop 把自己「唤醒处理函数」注册到了一个 Channel 上**。这个 Channel 的 fd 是 `eventfd`。

> [!tip] `std::bind` 的参数丢弃特性
> 注意 `EventLoop::handleRead()` 签名是 `void()`（无参），而 `ReadEventCallback` 类型是 `std::function<void(Timestamp)>`（需要 Timestamp 参数）。这个 bind 能编译是因为 `std::bind` 返回的 callable 有一个特性：调用时多余的实参会被默默丢弃。所以当 Channel 最终调 `readCallback_(receiveTime)` 时，`Timestamp` 被 bind 结果忽略，直接调无参的 `handleRead()`。
>
> 对比 `TcpConnection::handleRead(Timestamp)`——后者真正用了 `receiveTime`。两者可以通过同一个 `ReadEventCallback` 类型注册，得益于 `std::bind` 的"垫片"能力。

☐ 找到 `createEventfd()`（L33-L42）——就是 `eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)`。

### 2.3 验证你理解了这个结构

```text
EventLoop
  ├── poller_        (unique_ptr<Poller>)      ← 封装 epoll_wait
  ├── timerQueue_    (unique_ptr<TimerQueue>)   ← 封装定时器
  ├── wakeupFd_      (int)                     ← eventfd
  ├── wakeupChannel_ (unique_ptr<Channel>)      ← eventfd → handleRead
  ├── pendingFunctors_ (vector<Functor>)        ← 跨线程任务队列
  └── activeChannels_ (vector<Channel*>)        ← poll 返回的就绪列表
```

**EventLoop = epoll + timer + eventfd + 跨线程任务队列 + 循环驱动**。

### 2.4 Poller 和 Channel 的分工

> [!tip] 核心架构：Poller 查"哪个 fd 有事"，Channel 管"有事了找谁"
>
> EventLoop 中有两种角色处理 I/O 事件，职责完全正交：
>
> | | Poller | Channel |
> | --- | --- | --- |
> | **数量** | 一个 EventLoop 一个 | 每个 fd 一个 |
> | **职责** | 问内核"哪个 fd 有事？" | "我的 fd 有事了，调哪个回调？" |
> | **拥有资源** | `epollfd_`（epoll 实例） | `fd_`（被监控的 fd，不拥有） |
> | **系统调用** | `epoll_wait` / `epoll_ctl` | **无**（通过 Poller 间接调） |
> | **数据结构** | fd→Channel\* 映射表（`ChannelMap`） | 4 个 `std::function` 回调 |
>
> Poller 在 `poll()` 中的工作：
>
> ```text
> epoll_wait → 返回就绪事件列表
>   → event.data.ptr 取出 Channel*（EPollPoller::update() 中设置的）
>   → 填入 activeChannels_
> ```
>
> Channel 的工作在 `handleEvent()` 中完成。两者通过 `EventLoop` 串联：
>
> ```text
> Channel::enableReading()          ← fd 想被监听
>   → Channel::update()
>     → EventLoop::updateChannel(this)
>       → Poller::updateChannel(this)  ← 虚函数调用
>         → epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, {.ptr=channel})
> ```
>
> **Channel 不能自己调 epoll_wait**：一个 epoll 实例监控数千 fd。如果每个 Channel 自己搞一个 epoll，要调 N 次才能知道所有 fd 状态——性能灾难。所以 Poller 是共享的查询引擎，Channel 是每个 fd 专属的应答处理器。

---

## 第三章：跨线程任务投递

> **目标**：理解 `runInLoop` 和 `queueInLoop` 怎么实现"线程安全地让别的线程执行代码"。

> [!tip] EventLoop\* 就是"线程身份证"
> 你可能会注意到：muduo 中每个类都持有 `EventLoop*`：
>
> ```text
> TcpServer::loop_       → main EventLoop（主线程）
> Acceptor::loop_        → main EventLoop（主线程 accept）
> TcpConnection::loop_   → 某个 worker EventLoop（工作线程）
> Channel::loop_         → 所属的 EventLoop（epoll 在哪）
> TimerQueue::loop_      → 所属的 EventLoop（timerfd 在哪）
> ```
>
> 在 muduo 的 **one loop per thread** 模型中，`EventLoop` 就是线程的抽象。`isInLoopThread()` 检查当前线程的 `pthread_id` 是否匹配 EventLoop 的 `threadId_`。所以看到 `obj->loop_->runInLoop(cb)` 就是：**把 `cb` 交给 `obj` 所属的线程去执行**。这是 muduo 线程模型的基石——所有跨线程通信都通过这个 `EventLoop*`。

### 3.1 核心机制

☐ 回到 `EventLoop.cc`，找 `runInLoop()`（L148-L158）和 `queueInLoop()`（L160-L171）

```cpp
void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();                  // 同线程 → 直接执行
    } else {
        queueInLoop(std::move(cb));  // 异线程 → 入队
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        MutexLockGuard lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }
    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();  // 写 eventfd 唤醒 epoll_wait
    }
}
```

**☐ 为什么 `|| callingPendingFunctors_`？**

看 `doPendingFunctors()`（L254-L269）：

```cpp
void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        MutexLockGuard lock(mutex_);
        functors.swap(pendingFunctors_);  // 移出全部，短时持锁
    }
    for (const Functor& functor : functors) {
        functor();                         // 无锁执行
    }
    callingPendingFunctors_ = false;
}
```

> [!warning] 这个条件不是跨线程保护！它是同线程重入保护
>
> 跨线程时 `!isInLoopThread()` 已经为 `true`，`wakeup()` 必然触发。
> `callingPendingFunctors_` 只在**同线程内 `doPendingFunctors` 执行中又调 `queueInLoop`** 时才生效。

以下面这个场景为例：

```text
EventLoop 线程正在执行 doPendingFunctors：
                            callingPendingFunctors_ = true

  for (functor : functors) {
      functor();                               ← 比如 onMessage
          └─ conn->send(data)                   ← outputBuffer 满了
              └─ queueInLoop(writeCompleteCb)
                    ├─ pendingFunctors_.push_back(writeCompleteCb)
                    │     ↑ 已经 swap 出来了，这轮回调不会处理它
                    └─ isInLoopThread()? true
                       callingPendingFunctors_? true
                       → wakeup() ✓             ← 保证下轮 epoll_wait 立刻返回
  }

下轮 loop() 迭代：
  epoll_wait ← 刚被 wakeup 唤醒
  doPendingFunctors → 执行 writeCompleteCb ✓
```

如果去掉 `|| callingPendingFunctors_`，后果：

| 场景                            | 概率        | 影响                                 |
| ----------------------------- | --------- | ---------------------------------- |
| onMessage 中调 conn->send（写缓冲满） | **常见**    | writeComplete 回调延迟一个 epoll_wait 周期 |
| doPendingFunctors 中投递清理任务     | **常见**    | 资源延迟释放                             |
| 服务器空闲、无其他 I/O                 | **罕见但致命** | 任务无限阻塞                             |
|                               |           |                                    |

**没有这个标记不会崩溃，但会不定期地「吞」任务**——这是最难排查的那类 bug（偶发、无日志、不重复）。muduo 作者一定是在实际运行中被坑过才加了这个标记。

### 3.2 wakeup() 的实现

☐ 找 `wakeup()`（L234-L242）和 `handleRead()`（L244-L252）

```cpp
void EventLoop::wakeup() {
    uint64_t one = 1;
    sockets::write(wakeupFd_, &one, sizeof one);
}
void EventLoop::handleRead() {
    uint64_t one = 1;
    sockets::read(wakeupFd_, &one, sizeof one);  // 清空 eventfd 计数
}
```

eventfd 的特殊性：
- 写 `1` 累加计数器（不丢失——写 100 次计数器 = 100）
- 读时返回累加值并清零
- 所以一次写就能唤醒 epoll_wait，多次写也不会丢

### 3.3 回答"conn->send() 在哪个线程执行"

`TcpConnection::send()`（L87-L110）：

```cpp
void TcpConnection::send(const StringPiece& message) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(message);           // 同线程：直接写
        } else {
            loop_->runInLoop(              // 异线程：投递到 I/O 线程
                std::bind(fp, this, message.as_string()));
        }
    }
}
```

**所以 `conn->send()` 可以在任何线程调用**——如果调用者不是 I/O 线程，它自动把 `sendInLoop` 投递过去。这就是 muduo "线程安全"的核心。

---

## 关联文档

- [[muduo全量代码导读]] — 索引页
- [[muduo导读-Channel与所有权]] — Channel、tie、weak_ptr 所有权链
- [[muduo导读-连接生命周期]] — Acceptor → TcpServer → TcpConnection
- [[muduo导读-高级组件]] — Buffer、TimerQueue、多线程、日志
