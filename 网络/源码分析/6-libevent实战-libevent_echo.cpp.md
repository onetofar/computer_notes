---
title: libevent实战代码分析
category: 计算机网络
tags:
  - 源码分析
  - libevent
  - Reactor
  - epoll
source: src/examples/net/easyserver/libevent_echo.cpp src/base/reactor.hpp
difficulty: 进阶
created: 2026-07-19
skills:
  - obsidian-markdown
---
# Libevent Echo Server —— 源码陪读 & 对比分析

> 一个基于 libevent 的 Echo 服务器，55 行代码展示 libevent 的核心用法。
> 本文通过与项目中的 Reactor 类对比，揭示两种事件驱动库的设计哲学差异。

---

## 一、文件位置

| 文件                                              | 用途                 |
| ----------------------------------------------- | ------------------ |
| `src/examples/net/easyserver/libevent_echo.cpp` | Libevent echo 服务器  |
| `src/base/reactor.hpp`                          | 对比对象：项目自研的 Reactor |

---

## 二、Libevent 架构概览

```
event_base（事件引擎）
  ├── event（底层事件：fd + 回调）
  │     .events = EV_READ | EV_PERSIST
  │     .callback = on_xxx
  │
  ├── evconnlistener（accept 封装）
  │     on_accept → bufferevent_socket_new
  │
  └── bufferevent（高级 IO 封装）
        ├── evbuffer input（自动读缓冲）
        ├── evbuffer output（自动写缓冲）
        └── 回调：on_read / on_event
```

## 三、逐段陪读

### 3.1 创建事件引擎

```cpp
struct event_base* base = event_base_new();
```

`event_base` 是 libevent 最核心的对象——相当于我们的 `Reactor` + `Epoller` 的组合体。内部封装了 `epoll_create`（Linux）或 `kqueue`（macOS）或 `IOCP`（Windows）。

**对比我们的 Reactor：**
- Reactor 是 `Epoller` 的封装 + 回调分发
- `event_base` = `Epoller` + 跨平台抽象 + 内置事件管理

### 3.2 创建监听器

```cpp
struct evconnlistener* listener = evconnlistener_new_bind(
    base, on_accept, base,
    LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE,
    128, (struct sockaddr*)&addr, sizeof(addr));
```

`evconnlistener` 是 libevent 对 "accept" 的高级封装。它内部维护了一个监听 socket，自动处理：
- `socket()` + `bind()` + `listen()`
- `accept()` 循环
- 错误处理（如 EMFILE）

**对比我们的 Reactor：**
- 我们：手动 `Socket::bind()` → `listen()` → Reactor 内部 `accept`
- libevent：`evconnlistener_new_bind` 一步到位

```
Reactor 的 accept 流程：
  Socket::bind() → Socket::listen() → Reactor::listen()
  → Reactor::handle_accept() 中循环 accept

Libevent 的 accept 流程：
  evconnlistener_new_bind() → 内部自动 accept
  → on_accept 回调
```

### 3.3 Accept 回调

```cpp
static void on_accept(struct evconnlistener* /*listener*/,
                      evutil_socket_t fd,
                      struct sockaddr* addr,
                      int /*socklen*/,
                      void* ctx) {
    auto* base = static_cast<struct event_base*>(ctx);

    // TCP_NODELAY
    int yes = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));

    // 创建 bufferevent——libevent 的高级 IO 封装
    struct bufferevent* bev = bufferevent_socket_new(
        base, fd, BEV_OPT_CLOSE_ON_FREE);

    bufferevent_setcb(bev, on_read, nullptr, on_event, nullptr);
    bufferevent_enable(bev, EV_READ | EV_WRITE);
}
```

**`bufferevent` 是 libevent 的核心价值所在：**

| 特性          | libevent bufferevent | 我们的 Reactor                 |
| ----------- | -------------------- | --------------------------- |
| 自动读缓冲       | ✔ evbuffer 内部管理      | ❌ 用户需要 `char buf[4096]`     |
| 自动写缓冲       | ✔ evbuffer 排队        | ❌ 用户需要 `std::string output` |
| EPOLLOUT 开关 | ✔ 内部自动管理             | ❌ 用户手动 mod_fd               |
| 错误处理        | ✔ on_event 回调        | ❌ on_close + on_error       |

**对比：** 用 Reactor 实现 echo 的回显，需要：
```cpp
// Reactor on_read:
char buf[4096];
ssize_t n = recv(fd, buf, sizeof(buf), 0);
if (n > 0) {
    // 手动管理写缓冲
    output_buf[fd] += std::string(buf, n);
    reactor.mod_fd(fd, EPOLLIN | EPOLLOUT);
}

// Reactor on_write:
if (!output_buf[fd].empty()) {
    ssize_t n = send(fd, output_buf[fd].data(), ...);
    output_buf[fd].erase(0, n);
    if (output_buf[fd].empty())
        reactor.mod_fd(fd, EPOLLIN);
}

// Libevent on_read: 一行解决
evbuffer_add_buffer(output, input);
```

### 3.4 读取回调

```cpp
static void on_read(struct bufferevent* bev, void* /*ctx*/) {
    struct evbuffer* input = bufferevent_get_input(bev);
    struct evbuffer* output = bufferevent_get_output(bev);
    // input → output（零拷贝，不经过用户缓冲）
    evbuffer_add_buffer(output, input);
}
```

**`evbuffer_add_buffer(output, input)` 是内存零拷贝的：**
它在内部交换两个 evbuffer 的指针，而不是复制数据。对比用户态 `memcpy`，在网络 IO 中这是一个重要的性能优化。

**我们的 Reactor echo 同样需要读取→输出→开启 EPOLLOUT，三行代码。Libevent 一行解决。**

### 3.5 事件循环

```cpp
event_base_dispatch(base);  // 阻塞，等价于 Reactor::run()
```

`event_base_dispatch` 内部等价于：
```c
while (!finished) {
    epoll_wait / kevent / IOCP  ← 跨平台抽象
    for each active event:
        call callback(fd, events)
}
```

**对比 Reactor::run():**
```cpp
void Reactor::run() {
    while (running_) {
        int nfds = epoller_.wait(-1);
        for (int i = 0; i < nfds; i++) {
            // 事件分类 + 回调调用
        }
    }
}
```

**本质完全一样**——只是 libevent 把跨平台的 epoll/kqueue 抽象了。

---

## 四、Libevent vs Reactor 深度对比

### 4.1 抽象层次

```
Libevent:
  event_base              → OS 多路复用抽象（epoll/kqueue/IOCP）
  evconnlistener          → accept + listen socket 封装
  bufferevent             → fd + input_buffer + output_buffer 封装
  evbuffer                → 自动扩容的 IO 缓冲区

我们的 Reactor:
  Epoller                 → epoll 封装
  Socket                  → fd RAII 封装
  Reactor                 → Epoller + 回调分发
  用户                    → 手动管理 buffer 和 EPOLLOUT
```

### 4.2 API 风格

| 对比项 | Libevent | 我们的 Reactor |
|--------|----------|---------------|
| 语言 | C | C++ |
| 回调类型 | C 函数指针 | `std::function` |
| 上下文传递 | `void* ctx` | lambda 捕获 |
| 可读性 | 需要 cast void* | lambda 直接使用变量 |
| 类型安全 | ❌ void* | ✔ 闭包类型安全 |

**示例——context 传递：**

```cpp
// Libevent: 纯 C 风格，需要 cast void*
struct my_data data = {42};
event_new(base, fd, EV_READ, callback, &data);
// callback 中：auto* d = static_cast<my_data*>(ctx);

// Reactor: lambda 闭包，类型安全
reactor.on_read([data = 42](int fd) {
    // 直接使用 data
});
```

### 4.3 功能对比

| 功能 | Libevent | 我们的 Reactor |
|------|----------|---------------|
| **跨平台** | Linux/macOS/Windows/FreeBSD | ❌ Linux only (epoll) |
| **缓冲区管理** | ✔ evbuffer 自动扩容 | ❌ |
| **accept 封装** | ✔ evconnlistener | ❌ 手动 |
| **DNS** | ✔ 异步 DNS | ❌ |
| **HTTP** | ✔ evhttp | ❌ |
| **SSL** | ✔ 通过 OpenSSL | ❌ |
| **信号** | ✔ event 支持信号 | ❌ |
| **定时器** | ✔ event 支持超时 | ❌ 需要 timerfd |
| **线程安全** | ✔ 部分（event_base 不是） | ❌ 单线程 |
| **代码量** | ~12 万行 C | ~200 行 C++ |

### 4.4 性能

| 方面 | Libevent | 我们的 Reactor |
|------|----------|---------------|
| epoll 封装 | 直接 syscall 封装 | 通过 Epoller 类 |
| 事件分发 | 直接回调函数指针（无开销） | `std::function` 有虚函数开销 |
| 缓冲区 | evbuffer 内部优化 | 用户自定义，可能低效 |
| 结论 | 略快（纯 C + 优化多年） | 可接受（主要瓶颈在网络） |

### 4.5 设计哲学

```
Libevent："给你所有工具，一句回调搞定"
            （evconnlistener + bufferevent + evbuffer）

我们的 Reactor："只给你事件通知，你怎么读怎么写你自己决定"
            （只告诉你 fd 可读了，自己 recv/buffer/send）
```

---

## 五、用 Libevent 重写我们的 echo 服务器

这是项目中各 echo 服务器的代码量对比：

| 文件 | 行数 | 独立实现的类 |
|------|------|-------------|
| `blocking_echo_server.cpp` | ~70 | 无（阻塞 socket） |
| `callback_echo_server.cpp` | ~120 | Server 类 |
| `epoll_practice.cpp` | ~180 | Epoller + 手动 epoll |
| `epoll_ws_practice.cpp` | ~200 | ConnectionManager |
| **`libevent_echo.cpp`** | **~55** | **无（libevent 封装）** |
| **`multi_reactor_server.cpp`** | **~500** | **SPSC + Worker 等** |

Libevent 以最小的代码量实现了功能完备的 echo 服务器，代价是依赖一个 12 万行 C 代码的第三方库。

---

## 六、总结速查表

| 概念对比 | Libevent | 我们的 Reactor |
|----------|----------|---------------|
| 事件引擎 | `event_base` ≡ epoll fd | `Reactor` ≡ `Epoller` |
| 回调注册 | `event_new(fd, cb)` | `on_read(lambda)` |
| accept | `evconnlistener_new_bind` | `Reactor::listen()` + `on_accept` |
| IO 封装 | `bufferevent` | 无 |
| 缓冲区 | `evbuffer` (input/output) | 用户手动 |
| 事件循环 | `event_base_dispatch` | `Reactor::run()` |
| 清理 | `event_base_free` | `Reactor` 析构 |

## 关联笔记

- [[6-1 高性能服务器框架概述]] — Reactor/Proactor 事件处理模式
- [[6-3 高性能IO框架库 Libevent|6-3 高性能IO框架库 Libevent]]
- [[6-multi_reactor服务器-multi_reactor_server.cpp|6-a multi_reactor服务器实现源码解析]] — 与本文件的 Reactor 对比
- [[5-2-2 epoll下LT和ET的具体差别]] — epoll LT/ET 触发模式
- [[5-1 socket选项]] — TCP_NODELAY 等 socket 选项
- [[A muduo源码指南]] — Muduo 网络库对比

**一句话结论：**
- 如果要**快速开发跨平台网络应用** → Libevent（或 libuv、Boost.Asio）
- 如果要**学习 epoll 和 Reactor 模式的内部原理** → 我们的 Reactor
- 两者的事件循环核心（epoll_wait + 事件分类 + 回调调用）**完全一致**
