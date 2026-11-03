# 服务器编程模型：单线程串行 / 多线程加锁队列 / one loop per thread

> 来源讲义：`书籍/教材讲义/Linux多线程服务端编程/第3章 多线程服务器的适用场合与常用编程模型（讲义）.md` §3.2、§3.3.1、§3.3.2

## 对照表

三个文件处理**同一批任务**（一个 50ms 的慢任务 + 三个 1ms 的快任务）：

| 文件 | 模型 | 共享状态 | 预期输出 |
| --- | --- | --- | --- |
| `01-single-thread-serial.cpp` | 单线程 event loop 串行 | 无 | 总耗时 ≈ 53ms，慢任务拖住后面全部 |
| `02-multi-thread-locked-queue.cpp` | 多线程抢一个带锁队列 | 队列 + mutex + cv | 总耗时 ≈ 50ms，快任务先完成 |
| `03-one-loop-per-thread.cpp` | 每线程一个 loop，任务按归属分派 | 无（不需要锁） | 总耗时 ≈ 50ms，同一连接固定在同一线程 |

## 教材原话

讲义 §3.2 讲「单线程服务器的常用编程模型」：

> 作者判断：在高性能的网络程序中，使用得最为广泛的恐怕要数「**non-blocking IO＋IO multiplexing**」这种模型，即 Reactor 模式。

讲义 §3.2 讲 event loop 骨架（翻译成人话是四句话）：

> 1. **算一个超时**：距离下一个定时器还有多久，钳到至少 1000 毫秒；
> 2. **等**：把「要监听的文件描述符集合」交给 `poll()`，最多等这么久；
> 3. **出错就走错误回调**；
> 4. **醒了就分两拨处理**：到期的定时器走 timer handler，有事件就绪的 fd 走 IO event handler。

讲义 §3.2 讲 Reactor 的「本质缺点」：

> 基于事件驱动的编程模型也有其**本质缺点**：它要求**事件回调函数必须是非阻塞的**。

讲义 §3.3.1 讲「one loop per thread」（引 libev 作者 Marc Lehmann 原话）：

> One loop per thread is usually a good model. Doing this is almost never wrong, sometimes a better-performance model exists, but it is always a good start.

讲义 §3.3.1 讲 one loop per thread 的好处：

> - **线程数目基本固定**，可以在程序启动的时候设置，不会频繁创建与销毁。
> - **可以很方便地在线程间调配负载。**
> - **IO 事件发生的线程是固定的**，同一个 TCP 连接不必考虑事件并发。

讲义 §3.3.2 讲「线程池」：

> 不过，对于**没有 IO 而光有计算任务**的线程，使用 event loop 有点浪费。作者用一种补充方案，即用blocking queue实现的任务队列（TaskQueue）。

讲义 §3.3.3 讲「推荐模式」：

> 总结起来，作者推荐的 C++ 多线程服务端编程模式为：**one (event) loop per thread + thread pool**。

讲义 §3.5.1 讲「最简单的工具」原则（引 Paul E. McKenney）：

> "As a rough rule of thumb, use the simplest tool that will get the job done."

## 讨论点

1. **三个文件跑的是同一批任务，差别只在并发结构。** `01` 的总耗时约 53ms（50+1+1+1 串行），
   `02`/`03` 约 50ms（慢任务与快任务并行）。这就是「多线程买的是延迟，不是吞吐」。
2. **`01` 是最诚实的对照。** 它暴露了讲义说的 Reactor 本质缺点——
   **回调不许阻塞**。一个 50ms 的回调会把整个 loop 卡住 50ms。
3. **`02` 与 `03` 的差别不是性能，是结构。** `02` 所有任务挤在一个共享队列上，
   需要 mutex + condition_variable；`03` 每个 loop 只碰自己那份任务，**一把锁都不需要**。
   这正是讲义 §3.3.1 第三条好处：**IO 事件发生的线程是固定的，同一个 TCP 连接不必考虑事件并发。**
4. **`03` 是讲义推荐的最终形态**（one loop per thread + thread pool）的上半部分；
   `02` 对应下半部分（thread pool 里的任务队列）。

## 跑法

```bash
bash tools/run_all.sh
```

三个文件都不含数据竞争，ASan/UBSan 干净退出是预期结果。
