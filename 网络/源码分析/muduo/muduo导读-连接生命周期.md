---
title: Muduo 导读—连接生命周期
category: 网络编程
tags:
  - 源码分析
  - muduo
  - TcpServer
  - TcpConnection
  - Acceptor
difficulty: 深入
source: src/muduo_source/muduo/net/TcpServer.cc
created: 2026-07-24
modified: 2026-07-24
maturity: 完善
skills:
  - obsidian-markdown
---

> [!info] 本文定位
> 本文是 [[muduo全量代码导读]] 的拆分文档，专注**一条连接从 accept 到关闭的完整生命周期**。建议边开源文件边读。

---

## 第一章：Acceptor 负责 accept

☐ 打开 [`muduo/net/Acceptor.cc`](src/muduo_source/muduo/net/Acceptor.cc)

```cpp
void Acceptor::handleRead() {         // L55
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0) {
        if (newConnectionCallback_) {
            newConnectionCallback_(connfd, peerAddr);
            //     ↑ 这个回调是 TcpServer 设置的
        }
    }
}
```

这个 `newConnectionCallback_` 绑到了哪里？

☐ 打开 [`muduo/net/TcpServer.cc`](src/muduo_source/muduo/net/TcpServer.cc)，看构造函数（L22-L37）：

```cpp
acceptor_->setNewConnectionCallback(
    std::bind(&TcpServer::newConnection, this, _1, _2));
```

**☐ 关键**：Acceptor 不关心 accept 以后的事——它只负责"接进来，然后喊一声"。

---

## 第二章：TcpServer 决定连接归谁管

☐ 在 `TcpServer.cc` 里找 `newConnection()`（L71-L98）

```cpp
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr) {
    EventLoop* ioLoop = threadPool_->getNextLoop();  // 选一个工作线程
    //                                    ↑ round-robin

    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn;

    conn->setConnectionCallback(connectionCallback_);      // ← 你的 onConnection
    conn->setMessageCallback(messageCallback_);             // ← 你的 onMessage
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, _1));

    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
    // ↑ 把连接"移交"给工作线程——关键！
}
```

### 理解最后两行

```cpp
// 第 1 行：注册"善后"——只是存函数，不执行
conn->setCloseCallback(
    std::bind(&TcpServer::removeConnection, this, _1));
//                  签名: void(TcpConnectionPtr)      ↑ _1 = 调用时再传参
//
// _1 是占位符——removeConnection 需要一个参数（删哪个 conn），
// 这个参数等 handleClose 调 closeCallback_ 时才传过来。

// 第 2 行：投递"激活"——把连接放到工作线程去启动
ioLoop->runInLoop(
    std::bind(&TcpConnection::connectEstablished, conn));
//               签名: void()    ↑ conn 已提前绑死
// runInLoop 接受 void()，无参，完全匹配。
```

| | `_1` | conn |
| --- | --- | --- |
| **参数何时确定** | 调用时（handleClose 传 `shared_from_this()`） | bind 时已确定 |
| **产生的 callable 签名** | `void(TcpConnectionPtr)` | `void()` |
| **用途** | 注册一个"将来要别人告诉我删谁"的回调 | 把任务投递到工作线程 |

**顺序不能反**：先注册善后（`setCloseCallback`），再激活（`runInLoop`）。如果先激活再注册，断线发生在这个间隙里，`closeCallback_` 还是空的，连接永远从 `connections_` 表里移除不掉。

> [!warning] FIXME: unsafe
> `bind(&TcpServer::removeConnection, this, _1)` 中的 `this` 是**裸指针**。
> 如果 `TcpServer` 对象在连接之前析构，这个指针就 dangling 了。
> muduo 作者自己标注了这个坑但没修——实际使用中确保 TcpServer 生命周期长于所有连接。

**☐ 在这行停下来想一下**：`conn` 是 `shared_ptr`。`runInLoop` 把它 capture 进去。工作线程执行 `connectEstablished` 时，`conn` 还活着。

---

## 第三章：TcpConnection 初始化——绑定回调

☐ 打开 [`muduo/net/TcpConnection.cc`](src/muduo_source/muduo/net/TcpConnection.cc)，看构造函数（L38-L64）

```cpp
TcpConnection::TcpConnection(EventLoop* loop, ...)
    : loop_(loop),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd))    // ← 为这个 fd 创建 Channel
{
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, _1));
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError, this));
}
```

**这是整个 muduo 最关键的函数**。它把 `Channel` 的四个回调槽填上了 `TcpConnection` 的成员函数。

> [!tip] Socket vs Channel：一个管"寿命"，一个管"活动"
>
> 构造函数同时创建了 `Socket` 和 `Channel`，它们持有**同一个 sockfd** 但职责完全分离：
>
> | | Socket | Channel |
> | --- | --- | --- |
> | **拥有 fd？** | ✅ 是，析构时 `::close(fd)` | ❌ 只记数字，不拥有 |
> | **认识 epoll？** | ❌ 不认识 | ✅ 通过 EventLoop → Poller |
> | **认识业务对象？** | ❌ 不认识 | ✅ 通过回调绑定 TcpConnection |
> | **TCP 操作？** | ✅ `shutdownWrite` / `setTcpNoDelay` | ❌ 不管 |
> | **事件分发？** | ❌ 不管 | ✅ `handleEvent` / `revents_` 位掩码 |
>
> 销毁时序最能体现两者区别：
>
> ```cpp
> ~TcpConnection() {
>     // unique_ptr<Channel> 析构 → 从 epoll 移除（disableAll），但 fd 仍存活
>     // unique_ptr<Socket> 析构 → ::close(fd)  ← 只有 Socket 关 fd
> }
> ```
>
> **Socket 决定 fd 什么时候死，Channel 决定 fd 活着的时候有事找谁**。

> [!tip] `std::move` 不动所有权
> `setReadCallback(std::move(cb))` 移动的是 `std::function` 对象（省一次拷贝），**不是 TcpConnection**。
> Channel 内部存的是 `this` 裸指针（`TcpConnection*`），不持有 TcpConnection 的任何引用计数。
> TcpConnection 的生死由 `TcpServer::connections_`（`shared_ptr` map）决定，**不归 Channel 管**。

> [!tip] 回调分层：Channel 负责"原始 I/O"，TcpConnection 负责"业务通知"
> 这里注册的四个回调是**第 1 层**——Channel → TcpConnection：
>
> ```text
> Channel 的四个回调 → TcpConnection 的内部 I/O 函数
>   readCallback_   → handleRead:   readFd 读数据到 inputBuffer_
>   writeCallback_  → handleWrite:  把 outputBuffer_ 写到 fd
>   closeCallback_  → handleClose:  对端关闭，做清理
>   errorCallback_  → handleError:  打印错误日志
> ```
>
> TcpConnection 拿到数据后，再调**第 2 层**——TcpConnection → 用户业务：
>
> ```text
> TcpConnection 的业务回调 → 用户注册的函数
>   connectionCallback_  → onConnection(conn)
>   messageCallback_     → onMessage(conn, buf, time)
>   writeCompleteCallback_ → onWriteComplete(conn)
> ```
>
> Channel 不认识 Buffer、TcpConnectionPtr、Timestamp 这些业务概念。它只关心一件事：**fd 上有事件 → 调对应的回调**。

现在回想 [[muduo导读-Channel与所有权]] 中的 `handleEventWithGuard`——当 epoll 说"这个 fd 可读"时：

```text
epoll_wait → Channel::handleEventWithGuard()
    → if (revents & POLLIN) → readCallback_(time)
        → 这个 readCallback_ 绑的是 TcpConnection::handleRead
            → 读数据到 inputBuffer_
                → 调用 messageCallback_(conn, buf, time)
                    → 这就是你的 onMessage！
```

---

## 第四章：connectEstablished——把 Channel 注册到 epoll

☐ 找 `connectEstablished()`（L323-L332）

```cpp
void TcpConnection::connectEstablished() {
    loop_->assertInLoopThread();      // ← 确保在工作线程
    assert(state_ == kConnecting);    // ← 初始状态必须是 kConnecting
    setState(kConnected);
    channel_->tie(shared_from_this()); // ← tie 保护（详见 Channel 文档）
    channel_->enableReading();          // ← EPOLL_CTL_ADD 到工作线程的 epoll
    
    connectionCallback_(shared_from_this());  // ← 回调你的 onConnection
}
```

**`channel_->enableReading()`** 这行调用链：

```text
enableReading()
→ events_ |= kReadEvent     // 设置关心 EPOLLIN
→ update()                  // Channel::update()
  → loop_->updateChannel(this)  // EventLoop::updateChannel()
    → poller_->updateChannel(channel)  // 虚函数调用！实际调用哪个？
      → epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, {.events=EPOLLIN, .ptr=channel})
```

> [!tip] 虚函数派发
> `poller_` 的静态类型是 `Poller*`，但 `Poller::updateChannel` 是**纯虚函数**（`= 0`）。
> 实际调用哪个版本取决于运行时 `poller_` 指向的对象类型。
>
> `Poller::newDefaultPoller` 的创建逻辑（`DefaultPoller.cc`）：
>
> ```cpp
> if (::getenv("MUDUO_USE_POLL"))
>   return new PollPoller(loop);      // 设了环境变量 → 用 poll
> else
>   return new EPollPoller(loop);     // 默认 → 用 epoll
> ```
>
> 所以 Linux 默认走 `EPollPoller::updateChannel()`，但准确说这是**运行时多态决定**的。

☐ 打开 [`muduo/net/poller/EPollPoller.cc`](src/muduo_source/muduo/net/poller/EPollPoller.cc) 验证（L172-L192），`updateChannel` 内部调 `update`：

```cpp
void EPollPoller::update(int operation, Channel* channel) {
    struct epoll_event event;
    event.events = channel->events();
    event.data.ptr = channel;              // ← ptr 指向 Channel
    ::epoll_ctl(epollfd_, operation, fd, &event);
}
```

**重要**：`epollfd_` 是**每个 EventLoop 私有**的。所以这一步把 fd 注册到了**工作线程**的 epoll 实例上。

---

## 第五章：handleRead——数据来了

☐ 找 `handleRead()`（L347-L366）

```cpp
void TcpConnection::handleRead(Timestamp receiveTime) {
    loop_->assertInLoopThread();          // ← 必须在所属 EventLoop 线程
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0) {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
        //           ↑ 用户注册的 onMessage
    } else if (n == 0) {
        handleClose();  // 对端关闭
    } else {
        errno = savedErrno;
        LOG_SYSERR << "TcpConnection::handleRead";
        handleError();  // 异常
    }
}
```

**这是完整的链路**：epoll_wait → EventLoop → Channel → TcpConnection → 你的代码。

---

## 第六章：handleWrite——数据写回

☐ 找 `handleWrite()`（L368-L406）

```cpp
void TcpConnection::handleWrite() {
    loop_->assertInLoopThread();
    if (channel_->isWriting()) {
        ssize_t n = sockets::write(channel_->fd(),
                                   outputBuffer_.peek(),
                                   outputBuffer_.readableBytes());
        if (n > 0) {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0) {
                channel_->disableWriting();       // 写完了，关 EPOLLOUT
                if (writeCompleteCallback_)
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                if (state_ == kDisconnecting)
                    shutdownInLoop();              // 优雅关闭
            }
        } else {
            LOG_SYSERR << "TcpConnection::handleWrite";
        }
    }
}
```

---

## 第七章：handleClose——连接关闭

☐ 找 `handleClose()`（L408-L421）

```cpp
void TcpConnection::handleClose() {
    loop_->assertInLoopThread();
    assert(state_ == kConnected || state_ == kDisconnecting);
    setState(kDisconnected);
    channel_->disableAll();                     // 从 epoll 移除

    TcpConnectionPtr guardThis(shared_from_this());  // 防止析构
    connectionCallback_(guardThis);              // 通知用户"连接断了"
    closeCallback_(guardThis);                   // 通知 TcpServer 清理
    // ↑ 这两行顺序不能反——用户可能要在 onConnection 里读最后的数据
}
```

> [!tip] 两个不同的 `closeCallback_`
> `TcpConnection` 中有两个名字相似但**不同对象**的 `closeCallback_`：
>
> 1. **`Channel::closeCallback_`** —— 绑的是 `TcpConnection::handleClose`，在 `TcpConnection` 构造函数中注册。当 epoll 检测到 POLLHUP 时由 `Channel::handleEventWithGuard` 触发。
> 2. **`TcpConnection::closeCallback_`** —— 绑的是 `TcpServer::removeConnection`，在 `TcpServer::newConnection` 中通过 `conn->setCloseCallback(...)` 注册。由上面 `handleClose()` 的最后一行 `closeCallback_(guardThis)` 触发。
>
> 串起来：Channel 测到底层关闭 → Channel 回调触发 `TcpConnection::handleClose` → 先调 `connectionCallback_`（用户的 onConnection）→ 再调 `TcpConnection::closeCallback_` → `TcpServer::removeConnection` 做收尾清理。

**☐ 验证**：找 `TcpServer::removeConnectionInLoop()`（TcpServer.cc L106-L117），看清理时怎么再次跨线程：

```cpp
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn) {
    connections_.erase(conn->name());
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn));
    // ↑ 再把销毁任务投回工作线程
}
```

**二阶段析构**：主线程从 map 移除 → 工作线程真正销毁。

---

## 关联文档

- [[muduo全量代码导读]] — 索引页
- [[muduo导读-EventLoop核心]] — EventLoop 循环与跨线程投递
- [[muduo导读-Channel与所有权]] — Channel 事件分发、tie/weak_ptr 保护机制
