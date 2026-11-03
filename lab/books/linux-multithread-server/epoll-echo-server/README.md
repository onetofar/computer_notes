# 手写 echo server：阻塞 + 每连接一线程 vs 单线程 epoll reactor

> 来源讲义：`书籍/教材讲义/Linux多线程服务端编程/第6章 muduo网络库简介（讲义）.md` §6.1、
> `第8章 muduo网络库设计与实现（讲义）.md` §8.13

> **为什么是手写而不是 muduo**：本机没有 muduo 库，也不引入任何第三方依赖。
> 这里用两个**手写**的最小实现，把 muduo 封装掉的那一层（线程模型 + IO 多路复用）直接摊开对比。

## 对照表

两个文件做的是**同一件事**：在 `127.0.0.1` 上起一个 echo server，20 个客户端并发连接、
各发一条 `ping`、各收回一条 `ping`，然后自动退出。

| 文件 | IO 模型 | 服务端线程数 | 预期输出 |
| --- | --- | --- | --- |
| `01-blocking-thread-per-conn.cpp` | 阻塞 IO，每连接一线程 | **21**（1 accept + 20 worker） | `回显成功=20/20` |
| `02-epoll-reactor.cpp` | 非阻塞 IO + epoll | **1** | `回显成功=20/20` |

关键差异不在输出，而在**线程数那一行**：同样是 20 个连接，一个用 21 个线程，
一个用 1 个线程。这就是讲义 §6.1 讲的两种并发连接处理方式。

## 教材原话

讲义 §3.5 讲两种并发连接处理方式：

> - **当「线程」很廉价时**，一台机器上可以创建远高于 CPU 数目的「线程」。这时一个线程只处理一个 TCP 连接（甚至半个），通常使用阻塞 IO（至少看起来如此）。例如，Python gevent、Go goroutine、Erlang actor。这里的「线程」由语言的 runtime 自行调度，与操作系统线程不是一回事。
> - **当线程很宝贵时**，一台机器上只能创建与 CPU 数目相当的线程。这时一个线程要处理多个 TCP 连接上的 IO，通常使用非阻塞 IO 和 IO multiplexing。例如，libevent、muduo、Netty。这是原生线程，能被操作系统的任务调度器看见。

讲义 §6.6 讲「每连接一线程」这个方案（方案 2）：

> **连接独立** → 每个连接一个线程 → 无锁、无协调、顺序天然保证。

讲义 §6.6 拿它和 one loop per thread（方案 9）对比：

> 对比「每连接一线程」（方案 2）：连接数从 100 涨到 10000，线程数也跟着涨 100 倍，**每个线程都要栈、都要调度实体**，处理能力会**下降**。

讲义 §6.6 讲 one loop per thread（方案 9，muduo 的默认模型）：

> muduo采用的是固定大小的Reactor pool，池子的大小通常根据CPU数目确定，也就是说线程数是固定的，**这样程序的总体处理能力不会随连接数增加而下降**。

讲义 §8.13 讲「epoll」：

> epoll(4)是Linux独有的高效的IO multiplexing机制，它与poll(2)的不同之处主要在于poll(2)每次返回整个文件描述符数组，用户代码需要遍历数组以找到哪些文件描述符上有IO事件（见 Poller::fillActiveChannels()），而epoll_wait(2)返回的是活动fd的列表，需要遍历的数组通常会小得多。在并发连接数较大而活动连接比例不高时，epoll(4)比poll(2)更高效。

讲义 §8.13 讲 EPoller 的关键数据结构：

> EPoller的关键数据结构如下，其中events_不是保存所有关注的fd列表，而是一次epoll_wait(2)调用返回的活动fd列表，它的大小是自适应的。

讲义 §8.13 讲「用 epoll_data.ptr 存 Channel*」：

> struct epoll_event的定义如下，注意epoll_data是个union，muduo使用的是其ptr成员，用于存放Channel*，这样可以减少一步look up。

讲义 §7 章讲「socket 可读事件必须一次读完」：

> 网络库在处理「socket 可读」事件的时候，必须一次性把 socket 里的数据读完（从操作系统 buffer 搬到应用层 buffer），否则会反复触发 POLLIN 事件，造成 busy-loop。

## 讨论点

1. **两个文件都不依赖 muduo**，因为本机没有 muduo 库。要对比的其实是
   **「阻塞 + 线程换并发」** 和 **「非阻塞 + IO 多路复用换并发」** 这两条路线，
   muduo 只是把后者做成了库。
2. **`02` 里 `accept` 写成 `for(;;)` 循环读到 EAGAIN**，对应讲义 §7 那句
   「必须一次性把 socket 里的数据读完，否则会反复触发 POLLIN 事件，造成 busy-loop」。
   level trigger 下这是正确写法。
3. **`02` 的 `events` 数组是「活动 fd 列表」**，不是「全部关注的 fd 列表」——
   这正是讲义 §8.13 强调的那个反转，也是它相对 `poll` 的优势来源。
4. **两个服务器都在超时内自动退出**（10 秒上限），不会留下挂起进程。
   端口用 `bind(port=0)` 让内核分配，避免与本机已有服务撞端口。

## 跑法

> 需要 Linux。其它平台编译出的分支只打印一行提示，不会报错。

```bash
bash tools/run_all.sh
```

单独跑（Linux）：

```bash
g++ -std=c++20 -O2 -Wall -Wextra -pthread \
  books/linux-multithread-server/epoll-echo-server/02-epoll-reactor.cpp -o /tmp/epoll_echo
/tmp/epoll_echo
```

输出形如：

```
[server] listen 127.0.0.1:41273 (单线程 epoll reactor)
[server] 回显成功=20/20, 服务端线程数=1 (epoll_wait 返回活动 fd 列表)
```
