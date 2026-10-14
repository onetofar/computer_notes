---
title: Muduo 导读—高级组件
category: 计算机网络
tags:
  - 源码分析
  - muduo
  - Buffer
  - TimerQueue
  - EventLoopThread
  - AsyncLogging
difficulty: 深入
source: src/muduo_source/
created: 2026-07-24
modified: 2026-07-24
maturity: 初稿
skills:
  - obsidian-markdown
---

> [!info] 本文定位
> 本文是 [[muduo全量代码导读]] 的拆分文档，收录**非核心但重要的独立模块**：Buffer、TimerQueue、多线程、日志。各章之间没有强依赖，可以挑着读。

---

## 第一章：Buffer——网络缓冲区的设计

> **目标**：理解 muduo 的 Buffer 怎么解决"一次 recv 读不完/一次写不完"的问题。

### 1.1 Buffer 布局

☐ 打开 [`muduo/net/Buffer.h`](src/muduo_source/muduo/net/Buffer.h)，看类注释（L32-L41）

```text
+-------------------+------------------+------------------+
| prependable bytes |  readable bytes  |  writable bytes  |
|                   |     (CONTENT)    |                  |
+-------------------+------------------+------------------+
|                   |                  |                  |
0      <=      readerIndex   <=   writerIndex    <=     size
```

☐ 打开 `Buffer` 的成员变量（L411-L414）：

```cpp
std::vector<char> buffer_;
size_t readerIndex_;
size_t writerIndex_;
```

三个核心方法：

```cpp
size_t readableBytes() const { return writerIndex_ - readerIndex_; }  // 可读数据量
size_t writableBytes() const { return buffer_.size() - writerIndex_; }  // 剩余可用空间
size_t prependableBytes() const { return readerIndex_; }  // 可回退空间
```

### 1.2 关键设计：makeSpace

☐ 找 `makeSpace()`（L390-L409）：

```cpp
void makeSpace(size_t len) {
    if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
        buffer_.resize(writerIndex_ + len);               // A. 真的不够→扩容
    } else {
        // B. 空间够但碎片化→前移数据
        std::copy(begin()+readerIndex_, begin()+writerIndex_,
                  begin()+kCheapPrepend);
        readerIndex_ = kCheapPrepend;
        writerIndex_ = readerIndex_ + readable;
    }
}
```

**场景**：读了 100 字节，处理了 50 字节。Buffer 内部：

```text
prepend(8) | 未读(50) | 已读(50) | 可写(0)
```

此时要 append 30 字节——`writableBytes() == 0` 但 `prependableBytes() == 58`（8+50）。`makeSpace` 把未读的 50 字节前移到 prepend 之后，空出写空间。**避免了频繁的 resize**。

### 1.3 readFd——零拷贝读

☐ 打开 [`muduo/net/Buffer.cc`](src/muduo_source/muduo/net/Buffer.cc) 看 `readFd()`（L25-L57）

```cpp
ssize_t Buffer::readFd(int fd, int* savedErrno) {
    char extrabuf[65536];
    struct iovec vec[2];
    const size_t writable = writableBytes();
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    //   ↑ 如果 buffer 尾部空间 >= 64KB，只用一段，避免栈上拷贝
    const ssize_t n = sockets::readv(fd, vec, iovcnt);
    if (n < 0) {
        *savedErrno = errno;                 // 保存 errno
    } else if (implicit_cast<size_t>(n) <= writable) {
        writerIndex_ += n;                   // 全部写入 buffer
    } else {
        writerIndex_ = buffer_.size();       // buffer 尾部填满
        append(extrabuf, n - writable);      // 溢出部分从 extrabuf 追加
    }
    return n;
}
```

**注意**：`readv` + `extrabuf` 的组合是为了避免"先查可读字节数再分配"的额外系统调用。一次 `readv` 搞定，数据要么进 buffer 尾部，要么进 extrabuf 再 append。

---

## 第二章：TimerQueue——定时器

> **目标**：理解 muduo 怎么用 timerfd 把定时器也变成 fd，统一进 epoll。

### 2.1 数据结构

☐ 打开 [`muduo/net/TimerQueue.h`](src/muduo_source/muduo/net/TimerQueue.h)：

```cpp
class TimerQueue {
    const int timerfd_;                    // timerfd_create 的 fd
    Channel timerfdChannel_;               // 封装成 Channel
    std::set<Entry> timers_;               // 按到期时间排序
    // Entry 是 pair<Timestamp, Timer*>
};
```

**关键洞察**：`timerfd_` 也是一个 fd，像 socket fd 一样被 Channel 包装，注册到 EventLoop 的 epoll 中。当定时器到期时，timerfd 变为可读，`timerfdChannel_` 的读回调被触发。

### 2.2 工作机制

☐ 打开 [`muduo/net/TimerQueue.cc`](src/muduo_source/muduo/net/TimerQueue.cc)

```cpp
// addTimer 是线程安全的——通过 runInLoop 投递到 EventLoop 线程
TimerId TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval) {
    Timer* timer = new Timer(std::move(cb), when, interval);
    loop_->runInLoop(std::bind(&TimerQueue::addTimerInLoop, this, timer));
    return TimerId(timer, timer->sequence());
}

void TimerQueue::addTimerInLoop(Timer* timer) {
    loop_->assertInLoopThread();
    bool earliestChanged = insert(timer);   // 插入到 set
    if (earliestChanged) {
        resetTimerfd(timerfd_, timer->expiration());  // 更新 timerfd
    }
}
```

工作流程：

```text
addTimer(cb, when, interval)
  → runInLoop 切到 EventLoop 线程
    → insert(timer)                  // 插入 std::set，按到期时间排序
    → 如果最早到期时间变了
      → timerfd_settime(timerfd_, ...)  // 更新 timerfd

epoll_wait 返回（timerfd 可读）
  → handleRead()
    → readTimerfd(timerfd_, now)     // 读走 timerfd 计数
    → getExpired(now)                // 取出所有到期的 Timer
    → 逐个执行 callback
    → reset(expired, now)            // 重复的 timer 重新插入
```

**验证**：`TimerQueue` 不需要单独的线程或信号——它和 I/O fd 在同一个 `epoll_wait` 里处理。这就是"一切皆 fd"的体现。

---

## 第三章：多线程——One Loop Per Thread

> **目标**：理解 Muduo 怎么把 Reactor 扩展到多线程。

### 3.1 EventLoopThread——一个线程一个 EventLoop

☐ 打开 [`muduo/net/EventLoopThread.h`](src/muduo_source/muduo/net/EventLoopThread.h) 和 `.cc`：

核心逻辑：

```cpp
EventLoop* EventLoopThread::startLoop() {
    thread_.start();   // 启动线程
    {
        MutexLockGuard lock(mutex_);
        while (loop_ == NULL)
            cond_.wait();    // 等线程内的 EventLoop 创建完
    }
    return loop_;
}

void EventLoopThread::threadFunc() {
    EventLoop loop;           // 在这个线程创建 EventLoop
    {
        MutexLockGuard lock(mutex_);
        loop_ = &loop;
        cond_.notify();       // 通知 startLoop() 可以返回了
    }
    loop.loop();              // 阻塞在这个线程
}
```

**关键**：EventLoop 必须在其所属线程创建和运行——构造函数里记录 `threadId_`，`loop()` 里全程 `assertInLoopThread()`。

### 3.2 EventLoopThreadPool——连接分发

☐ 打开 [`muduo/net/EventLoopThreadPool.cc`](src/muduo_source/muduo/net/EventLoopThreadPool.cc)：

```cpp
EventLoop* EventLoopThreadPool::getNextLoop() {
    EventLoop* loop = baseLoop_;
    if (!loops_.empty()) {
        loop = loops_[next_];
        ++next_;
        if (next_ >= loops_.size()) next_ = 0;  // round-robin
    }
    return loop;
}
```

**☐ 思考**：当 `setThreadNum(0)` 时，`loops_` 为空，`getNextLoop()` 返回 `baseLoop_`（主线程的 EventLoop）。这就是**单线程模式**——所有连接在主线程处理。

当 `setThreadNum(4)` 时，有 4 个工作 EventLoop，accept 到的连接 round-robin 分配给它们。

---

## 第四章：AsyncLogging——日志后端

> **目标**：理解 muduo 的日志怎么做到"不阻塞业务线程"。

### 4.1 双缓冲交换

☐ 打开 [`muduo/base/AsyncLogging.h`](src/muduo_source/muduo/base/AsyncLogging.h)：

```cpp
class AsyncLogging {
    MutexLock mutex_;
    Condition cond_;
    BufferPtr currentBuffer_;     // 当前正在写的 buffer
    BufferPtr nextBuffer_;        // 备用 buffer
    BufferVector buffers_;        // 已满、待写入的 buffer 列表
};
```

☐ 打开 [`muduo/base/AsyncLogging.cc`](src/muduo_source/muduo/base/AsyncLogging.cc)

前端（任何线程写日志）：

```cpp
void AsyncLogging::append(const char* logline, int len) {
    MutexLockGuard lock(mutex_);
    if (currentBuffer_->avail() > len) {
        currentBuffer_->append(logline, len);   // 直接写入当前 buffer
    } else {
        buffers_.push_back(std::move(currentBuffer_));  // 满了→移到队列
        if (nextBuffer_) {
            currentBuffer_ = std::move(nextBuffer_);     // 用备用 buffer
        } else {
            currentBuffer_.reset(new Buffer);            // 备用也没了→新建
        }
        currentBuffer_->append(logline, len);
        cond_.notify();  // 通知后端线程
    }
}
```

后端（日志线程）：

```cpp
void AsyncLogging::threadFunc() {
    while (running_) {
        {
            MutexLockGuard lock(mutex_);
            // 等待条件或超时（默认 3 秒）
            if (buffers_.empty()) cond_.waitForSeconds(flushInterval_);
            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_ = std::move(newBuffer1);
            buffersToWrite.swap(buffers_);  // 移出所有已满 buffer
        }
        // 无锁写入文件
        for (const auto& buffer : buffersToWrite)
            output.append(buffer->data(), buffer->length());
        output.flush();
        // 归还空 buffer 给前端
    }
}
```

**效率来自**：
- 前端写日志：`memcpy` 到 4KB buffer（几微秒），无文件 I/O
- 后端批量写：一次写入多个 buffer，分摊系统调用开销
- 两个备用 buffer 避免多数情况下的内存分配

---

## 第五章：综合练习——从 code review 角度

打开以下文件，不读细节，只看**函数签名和注释**，判断它们是干什么的：

☐ [`muduo/net/Connector.h`](src/muduo_source/muduo/net/Connector.h)
☐ [`muduo/net/TcpClient.h`](src/muduo_source/muduo/net/TcpClient.h)
☐ [`muduo/net/http/HttpServer.h`](src/muduo_source/muduo/net/http/HttpServer.h)
☐ [`muduo/net/protorpc/RpcServer.h`](src/muduo_source/muduo/net/protorpc/RpcServer.h)
☐ [`muduo/base/ThreadLocal.h`](src/muduo_source/muduo/base/ThreadLocal.h)
☐ [`muduo/base/ThreadLocalSingleton.h`](src/muduo_source/muduo/base/ThreadLocalSingleton.h)
☐ [`muduo/base/CountDownLatch.h`](src/muduo_source/muduo/base/CountDownLatch.h)
☐ [`muduo/base/Singleton.h`](src/muduo_source/muduo/base/Singleton.h)

每个文件 30 秒：看类名、继承关系、公有方法列表。理解"这个类解决什么问题"就够了。

---

## 关联文档

- [[muduo全量代码导读]] — 索引页
- [[muduo导读-EventLoop核心]] — EventLoop 循环与跨线程投递
