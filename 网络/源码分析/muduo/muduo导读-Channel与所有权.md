---
title: Muduo 导读—Channel 与所有权链
category: 计算机网络
tags:
  - 源码分析
  - muduo
  - Channel
  - weak_ptr
  - tie
difficulty: 深入
source: src/muduo_source/muduo/net/Channel.h
created: 2026-07-24
modified: 2026-07-24
maturity: 完善
skills:
  - obsidian-markdown
---

> [!info] 本文定位
> 本文是 [[muduo全量代码导读]] 的拆分文档，专注 **Channel 机制**：回调分发、tie/weak_ptr 所有权链。建议边开源文件边读。

---

## 第一章：Channel——把 fd 和回调绑在一起

> **目标**：理解 Channel 是"从 epoll_wait 到你代码"的胶水层。

### 1.1 看头文件

☐ 打开 [`muduo/net/Channel.h`](src/muduo_source/muduo/net/Channel.h)

Channel 的成员变量揭示了它的全部职责：

```cpp
class Channel : noncopyable {
    EventLoop* loop_;        // 所属 EventLoop
    const int  fd_;          // 监听的 fd
    int        events_;      // 关心的 epoll 事件（EPOLLIN/OUT 等）
    int        revents_;     // epoll_wait 返回的事件
    // ... index_, tied_, eventHandling_ ...

    ReadEventCallback readCallback_;   // std::function<void(Timestamp)>
    EventCallback writeCallback_;      // std::function<void()>
    EventCallback closeCallback_;      // std::function<void()>
    EventCallback errorCallback_;      // std::function<void()>
};
```

**☐ 停留 10 秒**：Channel 不拥有 fd，它只负责"当 fd 上有事件发生时，调用对应的回调"。就像一个插座——谁插上电（设置回调），电来了就通到谁那里。

### 1.2 看事件分发的实现

☐ 打开 [`muduo/net/Channel.cc`](src/muduo_source/muduo/net/Channel.cc)，找到 `handleEventWithGuard()`（L83-L113）

```cpp
void Channel::handleEventWithGuard(Timestamp receiveTime) {
    // POLLHUP（对端挂断）
    if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) {
        if (closeCallback_) closeCallback_();
    }
    // POLLERR（错误）
    if (revents_ & (POLLERR | POLLNVAL)) {
        if (errorCallback_) errorCallback_();
    }
    // POLLIN（数据可读）——最常见
    if (revents_ & (POLLIN | POLLPRI | POLLRDHUP)) {
        if (readCallback_) readCallback_(receiveTime);
    }
    // POLLOUT（可写）
    if (revents_ & POLLOUT) {
        if (writeCallback_) writeCallback_();
    }
}
```

**这就是你网关中 if-else 链的泛化版本**。你的代码：

```cpp
if (ev & EPOLLERR) close_conn(fd);
else if (ev & EPOLLIN) handle_read(fd);
else if (ev & EPOLLOUT) handle_write(fd);
```

区别在于：你的回调是固定的 `handle_read(fd)`，muduo 的 `readCallback_` 可以指向**任何函数**。是谁设置的？往上层的 [[muduo导读-连接生命周期]] 看。

---

## 第二章：tie()——所有权链与 weak_ptr 的深层逻辑

> **目标**：理解 muduo 中谁拥有谁、为什么 tie 用 weak_ptr 而不用 shared_ptr。

### 2.1 所有权链全景

先理解 muduo 中谁拥有谁，才能理解为什么 tie 必须用 `weak_ptr`：

```text
main()
  └── TcpServer（栈 / unique_ptr）
        ├── unique_ptr<Acceptor>
        │     └── Socket acceptSocket_（by value，不调 tie）
        │     └── Channel acceptChannel_（by value，不调 tie）
        ├── shared_ptr<EventLoopThreadPool>
        │     └── vector<EventLoopThread>
        └── map<string, shared_ptr<TcpConnection>>   ← 掌控生死
              └── TcpConnection（shared_ptr 管理）
                    ├── unique_ptr<Socket>            ← 独占 connfd
                    └── unique_ptr<Channel>           ← 独占事件分发
                          └── tie_: weak_ptr<void>    ← 借用担保（不贡献 refcount）
```

核心原则：
- **TcpServer → TcpConnection：`shared_ptr`**（连接表掌控生死）
- **TcpConnection → Channel：`unique_ptr`**（独占工具，TcpConnection 析构 → Channel 自动析构）
- **Channel → TcpConnection：`weak_ptr`**（借来用用，不影响引用计数）

### 2.2 为什么不直接存 shared_ptr？

如果 `tie_` 存的是 `shared_ptr<TcpConnection>`：

```text
TcpServer map ──shared_ptr──→ TcpConnection ←── shared_ptr 如果存在这里
                                      │               ↑
                                      │ unique_ptr    │
                                      ↓               │
                                  Channel ────────────┘
```

当 TcpServer `connections_.erase(name)` 后：

1. TcpServer 释放自己的 `shared_ptr`，refcount 从 2 → 1
2. Channel 还持有一份 `shared_ptr`，refcount 为 1，TcpConnection 不会析构
3. 但 Channel 是 TcpConnection 的成员（`unique_ptr`），Channel 不死因为 TcpConnection 还活着
4. **循环引用 → 内存泄漏**

`weak_ptr` 打破此环：`tie_.lock()` 只"借用"，不贡献 refcount。TcpServer 释放后 refcount 归零 → TcpConnection 析构 → `unique_ptr<Channel>` 析构 → `weak_ptr` 自动变空。

### 2.3 谁调 tie，谁不调？

| Channel | 调 tie？ | 原因 |
| --- | --- | --- |
| TcpConnection 的 `channel_` | **✅** | 可能被其他线程 `forceClose()` |
| Acceptor 的 `acceptChannel_` | ❌ | 主线程内，不移交 |
| EventLoop 的 `wakeupChannel_` | ❌ | EventLoop 线程内，不移交 |
| TimerQueue 的 timerfd Channel | ❌ | EventLoop 线程内，不移交 |

**边界**：谁暴露给跨线程谁就需要 `tie()`。

### 2.4 逐行解析 handleEvent

☐ 回来看 `Channel::handleEvent()`（Channel.cc L60-70）：

```cpp
void Channel::handleEvent(Timestamp receiveTime) {
    // L61: 局部 shared_ptr guard。不初始化为任何对象，等待 lock 结果
    std::shared_ptr<void> guard;

    // L62: tied_ 为 true 表示调过 tie()，需要保护
    if (tied_) {
        // L63: weak_ptr::lock()——尝试提升为 shared_ptr
        //      TcpConnection 还活着 → 返回有效 shared_ptr（refcount+1）
        //      TcpConnection 已销毁 → 返回空
        guard = tie_.lock();

        // L64: 检查 lock 是否成功。成功就安全处理事件
        if (guard) {
            handleEventWithGuard(receiveTime);
        }
        // guard 为空时：对象已销毁，什么都不做，安全跳过
    } else {
        // L68: 内部 Channel 不需要保护，直接处理
        //      避免了 weak_ptr::lock() 的开销
        handleEventWithGuard(receiveTime);
    }
}   // L70: guard 离开作用域释放 shared_ptr，refcount 恢复原值
```

每一行的设计意图：

| 行 | 意图 |
| --- | --- |
| `shared_ptr<void> guard` | RAII 保活——局部变量确保事件处理期间对象不析构 |
| `if (tied_)` | 区分"需跨线程保护"和"不需"的 Channel |
| `guard = tie_.lock()` | 裸指针查岗——你还活着吗？ |
| `if (guard)` | 安全检查：活着就处理，死了就跳过 |
| `else` | 内部 Channel 的性能优化（跳过 lock 开销） |

---

## 关联文档

- [[muduo全量代码导读]] — 索引页
- [[muduo导读-EventLoop核心]] — EventLoop 循环与跨线程投递
- [[muduo导读-连接生命周期]] — TcpConnection 注册回调到 channel 的实际场景
