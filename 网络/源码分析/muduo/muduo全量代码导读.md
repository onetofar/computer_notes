---
title: Muduo 全量代码导读（索引）
category: 网络编程
tags:
  - 源码分析
  - muduo
  - Reactor
  - C++
  - 服务器架构
difficulty: 深入
source: src/muduo_source/
created: 2026-07-23
modified: 2026-07-24（拆分为多文件索引页）
maturity: 完善
skills:
  - obsidian-markdown
---

> [!info] 本文用法
> 本文是索引页。正文已按主题拆分为 4 篇独立文档，请通过下方链接跳转。每节标注了 **要打开的文件和行号**，请边开文件边读。

---

## 第〇章：出发前

### 你需要打开的东西

```bash
# 本文所有路径相对于 /workspace/src/muduo_source/
cd /workspace/src/muduo_source/
```

用 VSCode 打开整个 `muduo_source/` 目录，保持本文在侧边。

### 先看目录结构（30 秒）

☐ 打开并扫一眼：

```text
muduo/
├── base/      ← 不依赖网络的工具库（线程、日志、时间）
├── net/       ← 网络核心（EventLoop、Channel、TcpServer）
└── net/poller/ ← epoll/poll 的具体实现
```

只看目录名就够了，不用深入。

---

## 导航——从哪篇开始

```text
┌─ 你在这里 ──────────────────────────────────────────────┐
│                                                          │
│  [[muduo导读-EventLoop核心]]     ← EventLoop 循环 + 跨线程  │
│  [[muduo导读-Channel与所有权]]    ← Channel + tie/weak_ptr  │
│  [[muduo导读-连接生命周期]]       ← Acceptor→TcpServer→连接  │
│  [[muduo导读-高级组件]]           ← Buffer/定时器/多线程/日志 │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

**推荐阅读顺序**：

1. [[muduo导读-EventLoop核心]]（第 1-2 章 + 跨线程投递）
2. [[muduo导读-Channel与所有权]]（Channel 事件分发 + tie 保护）
3. [[muduo导读-连接生命周期]]（从 accept 到关闭）
4. [[muduo导读-高级组件]]（挑感兴趣的读）

---

## 核心概念速览

### EventLoop 结构

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

### 所有权链

```text
main()
  └── TcpServer（栈 / unique_ptr）
        ├── unique_ptr<Acceptor>         ← 只 accept
        ├── shared_ptr<EventLoopThreadPool>
        └── map<string, shared_ptr<TcpConnection>>  ← 掌控生死
              └── TcpConnection
                    ├── unique_ptr<Socket>     ← 独占 connfd
                    └── unique_ptr<Channel>    ← 独占事件分发
                          └── tie_: weak_ptr<void>  ← 借用担保
```

---

## 学习检核表

读完代码后，应该能用**自己的话**回答：

### 事件循环

- ☐ `EventLoop::loop()` 里做了哪三件事？（poll、dispatch、doPendingFunctors）
- ☐ 为什么 EventLoop 的构造函数要记录 `threadId_`？
- ☐ `channel_->handleEvent()` 和你的 if-else 链本质区别是什么？

### 回调链路

- ☐ 从 `epoll_wait` 到你写的 `onMessage`，完整经过哪些函数？
- ☐ `Channel` 的 `tie()` 解决什么问题？不用的后果？
- ☐ `TcpConnection` 的 `closeCallback_` 和 `connectionCallback_` 什么区别？哪条先执行？

### 线程

- ☐ `runInLoop` 和 `queueInLoop` 的区别？（提示：同线程 vs 异线程）
- ☐ 为什么 `queueInLoop` 后要 `wakeup()`？
- ☐ `doPendingFunctors` 为什么要 swap 而不是直接遍历？

### Buffer

- ☐ `prependableBytes()` 什么时候能>0？
- ☐ `makeSpace` 的两种路径分别是什么场景？
- ☐ 为什么要用 `readv` + extrabuf 而不是只用 buffer？

### 多线程

- ☐ 新连接怎么从 Accept 线程转移到 IO 线程的？
- ☐ 为什么 TcpConnection 内部不需要锁？
- ☐ 为什么 `setThreadNum(0)` 时 `getNextLoop()` 返回 baseLoop？
- ☐ Acceptor 的 `idleFd_` 是干什么的？（提示：EMFILE）

---

## 关联文档

- [[muduo导读-EventLoop核心]] — EventLoop 循环与跨线程
- [[muduo导读-Channel与所有权]] — Channel 机制与所有权链
- [[muduo导读-连接生命周期]] — TcpServer/TcpConnection 生命周期
- [[muduo导读-高级组件]] — Buffer、TimerQueue、多线程、日志

---

## ⚠️ 踩坑/待办

### 已确认的坑

> [!warning] FIXME: unsafe — TcpServer 的裸 this 指针
> `TcpServer::newConnection` 中 `conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, _1))` 的 `this` 是裸指针。如果 `TcpServer` 先于所有 `TcpConnection` 析构，`removeConnection` 回调会访问已销毁的对象。muduo 作者自己标注了 `FIXME: unsafe`。实际使用中需确保 TcpServer 生命周期长于所有连接，或用 `std::enable_shared_from_this` 改造。
>
> ↵ 参考：TcpServer.cc L78

### 【待确认】

> [!question] 【待深入】BoundedBlockingQueue 在 muduo 中的实际使用场景
> 代码里有 `BoundedBlockingQueue` 实现，但 ThreadPool 用的是无界 `deque` 加 `maxQueueSize_` 限流。有界队列版本在哪里被使用？
>
> [!question] 【待验证】conn->send() 在跨线程调用时的性能
> `send()` 跨线程时会把数据拷贝成 string 再 bind 到 `runInLoop`。如果每次发送 1MB 数据，这个拷贝开销是否可接受？对比直接加锁写 outputBuffer_ 的方案。
