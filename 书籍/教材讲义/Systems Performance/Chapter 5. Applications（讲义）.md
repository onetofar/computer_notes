---
title: Chapter 5. Applications（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - 应用层性能
  - 火焰图
  - 线程状态分析
  - 非阻塞IO
  - 观测工具
source: "《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg，Addison-Wesley 2020，英文原版）第 5 章"
companion: "[[05-Chapter 5. Applications]]"
---

# 第 5 章 讲义：性能调优离工作发生的地方最近

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[05-Chapter 5. Applications|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：`/home/xier/dev/game-project/otf-server/` —— 一个 9692 行、零三方依赖的 C++ 游戏服务器；本章逐条对照它「做到了什么、没做到什么」。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原章。

---

## 0. 开局：CPU 才跑了 3%，服务器却说「慢」

线上告警：游戏服务器延迟飙高。登上去一看——

- `top`：CPU 用了 **3%**
- 磁盘是 SSD，IO 队列是空的
- 内存还空着一大半
- 网卡流量正常，没有打满

「**那它到底慢在哪？**」

这是性能工程师最常见的开场。本章不给你新的测量工具——第 4 章已经给过了。它给的是**一套「先问什么」的顺序**。因为在这种场景里，工具只是把你已有的证据变精细；**真正稀缺的是「先看哪儿」**。

原文开门第一句就定了调：

> Performance is best tuned closest to where the work is performed: in the applications.

注意这里的 **closest**（最近）。不是「先看应用、再看系统」这种顺序上的偏好，而是**因果上的距离**：工作真正被做出来的地方，就是能改掉它的地方。

本章讲的就是「怎么在那一步把问题抓住」。

---

## 1. §5.1 Application Basics：先把应用认清楚

原文列了一份「回答这些问题，你就理解了这个应用」的清单：Function（它是数据库？Web 服务器？）、Operation（它服务什么请求？）、Performance requirements（有没有 SLO？）、CPU mode（用户态还是内核态？）、Configuration、Host、Metrics、Logs、Version、Bugs、Source code、Community、Books、Experts。

### §5.1.1 Objectives：没有目标，分析就是钓鱼

原文的第一条硬规矩：

> A performance goal provides direction for your performance analysis work and helps you select which activities to perform.

没有目标的性能分析，原文给了一个很刻薄的比喻：

> performance analysis risks turning into a random “fishing expedition.”

目标有四类：**Latency**（响应时间低或稳定）、**Throughput**（操作率或数据率）、**Resource utilization**（给定负载下的效率）、**Price**（性能/价格比）。原文强调的是**能量化**：

- 平均请求延迟 5 ms
- 95% 的请求在 100 ms 以内
- 消除延迟离群值：零个请求超过 1000 ms
- 每台指定规格的服务器至少 10000 请求/秒

**离群值那条最值得抄。** 延迟分布的长尾往往比均值更能毁掉体验，而「零个请求超过 1000 ms」是一个**绝对、可测、不容打折**的目标。

> Apdex = (satisfactory + 0.5 × tolerable + 0 × frustrating) / total events

Apdex 把客户体验分成「满意 / 可忍 / 恼火」三档。**它是个不完美的指标**（把 50 ms 和 500 ms 都算「可忍」），但它的价值在于**给非技术方一个数**。

### §5.1.2 Optimize the Common Case

原文说得很直白：应用动辄几万行，内核几十万行，**随机挑地方优化是白干**。正确做法是：

> One way to efficiently improve application performance is to find the most common code path for the production workload and begin by improving that.

CPU-bound 就找在 CPU 上待得最久的路径；I/O-bound 就找最常触发 I/O 的路径。

### §5.1.3 Observability

这一小节只有两段，但可能是全章最值钱的两段。

> the biggest performance wins can come from eliminating unnecessary work

注意「**unnecessary** work」。不是「让必要的工作更快」，是**把不必要的工作整个删掉**。

然后是选型建议——它举了个很具体的例子：如果 A 比 B 快 10%，你会选 A；但如果 A 是个黑盒、B 提供了丰富的观测工具，

> The performance wins gained through enhanced observability may dwarf the initial 10% performance difference.

**可观测性带来的收益，会把那 10% 甩得看不见。**

### §5.1.4 Big O Notation

Big O 是 CS 课内容，这里出现是因为**它是性能分析的第一层筛选器**：O(n²) 的算法在数据量小的时候毫无问题，等你「开始服务比以往更多的用户或数据对象」时，它就变成病理性的了。

原文给了一个很好用的量级例子：

> For example, for searching a sorted array of 100 items, the difference between linear search and binary search is a factor of 21 (100/log(100)).

**21 倍**。这告诉你两件事：一是排序的代价在 n=100 时就已经值 21 倍；二是在做容量规划时，「n 翻倍」对 O(n) 是 2 倍、对 O(n²) 是 4 倍、对 O(2^n) 是无穷大。

原文也提醒了 Big O 的盲区：

> Big O notation does ignore some constant computation costs incurred for each algorithm. For cases where n (the input data size) is small, these costs may dominate.

---

## 2. §5.2 Application Performance Techniques

这一节是一张「应用层技术清单」，原文列了七类：选 I/O 大小、缓存、缓冲、轮询、并发与并行、非阻塞 I/O、处理器绑定。

### §5.2.1–5.2.3 Selecting an I/O Size / Caching / Buffering

**I/O 大小**的核心是一个固定成本：

> “Initialization tax” is paid for small and large I/O alike. For efficiency, the more data transferred by each I/O, the better.

于是：

> It’s usually much more efficient to transfer 128 Kbytes as a single I/O than as 128 × 1 Kbyte I/O

**但**大 I/O 不是免费午餐：数据库做 8 KB 随机读，用 128 KB 的 I/O 尺寸会有 120 KB 是白搬的 —— **浪费带宽 + 引入延迟**。原文的结论是「选一个更贴近应用实际请求的尺寸」。

**缓存**（Caching）原文说得很简洁：把常用操作的结果存在本地缓存里，替代每次都做那个昂贵操作。部署时该做的事是「搞清楚有哪些缓存、能不能开、怎么配大小」。

**缓冲**（Buffering）是写侧的对应物：数据先在缓冲区里合并再发出去，**I/O 变大了、效率上去了，代价是写延迟**（第一次写要等后面的写跟上来）。

> A ring buffer (or circular buffer ) is a type of fixed buffer that can be used for continuous transfer between components, which act upon the buffer asynchronously.

**环形缓冲区**是缓冲的特例：固定大小、start/end 两个指针由两个组件各自推进。**它天生适合「一端写、一端读、异步」的场景**——这正是网络收发的形状。

### §5.2.4 Polling

轮询 = 在循环里反复检查状态。原文点出两个性能问题：**反复检查的 CPU 开销**、**事件发生到下次检查之间的延迟**。

解决方向是「改成等事件发生时通知我」。Linux 上的 `poll(2)` 就是这种**事件驱动**接口，但它仍然要扫数组：

> The poll(2) interface supports multiple file descriptors as an array, which requires the application to scan the array when events occur to find the related file descriptors.

> An alternative on Linux is epoll(2), which can avoid the scan and therefore be O(1). On BSD, the equivalent is kqueue(2).

**O(n) → O(1) 的差别，就是「连接数一涨就是性能悬崖」和「连接数一涨只是多花点常数」的区别。**

### §5.2.5 Concurrency and Parallelism

原文先把两个词分清：

- **concurrency（并发）**：多个可运行的程序**轮流**执行（时间共享），它们**不同时刻**在 CPU 上。
- **parallelism（并行）**：多个 CPU **同一时刻**真的在跑。

三种用户态并发机制：**Fibers**（用户态轻量线程）、**Co-routines**（协程，比 fiber 更轻）、**Event-based concurrency**（事件处理器跑在队列上，Node.js 就是单事件 worker 线程）。

原文提醒了一个硬约束：

> With all of these mechanisms, I/O must still be handled by the kernel, so OS thread switching is typically inevitable.

**用户态的并发省不掉内核态的线程切换**——除非你用 `sendfile(2)` 这种内核内拷贝或 Linux `io_uring`。

三种常见多线程模型：**Service thread pool**（一线程一连接）、**CPU thread pool**（一 CPU 一线程，长批处理用）、**SEDA**（Staged Event-Driven Architecture，请求拆成阶段，每阶段一个或多个线程池）。

> Since multithreaded programming shares the same address space as the process, threads can read and write the same memory directly, without the need for more expensive interfaces (such as inter-process communication [IPC] for multiprocess programming).

**多线程的真正好处不只是并行 CPU，还有「别的线程去干活时，这个线程能阻塞在 I/O 上」**（另一种是异步 I/O）。

#### Synchronization Primitives

原文把同步原语比作**红绿灯**：它保证数据完整性，但它也会**卡住车流**，产生等待时间（延迟）。四类：

- **Mutex 锁**：只有持有者能操作，其他人阻塞、**off-CPU** 等。
- **自旋锁**：其他人**在 CPU 上空转**轮询锁是否释放。低延迟（不用离开 CPU），但**烧 CPU**。
- **读写锁**：多个读者或一个写者。
- **信号量**：计数型（允许 N 个并行）或二值型（等价于互斥锁）。

Linux 的 mutex 是**自旋 + 阻塞的混合体**，有三条路径：**fastpath**（cmpxchg 一把成）、**midpath**（乐观自旋，赌持有者马上放）、**slowpath**（阻塞并被调度出去）。

内核里还常用 **RCU**（读-拷贝-更新）：读操作**不用拿锁**，写的时候复制一份替换掉，在途的读者继续读旧的那份。

#### Hash Tables

这一小节讲的是**锁的哈希表**：一个大全局锁 vs 每个数据结构一把锁 vs 哈希表一把（固定数量）锁。中间那个是折中。

关键的一句：

> Ideally, the number of hash table buckets should be equal to or greater than the CPU count, for the potential of maximum parallelism.

以及**假共享**的引子：

> With an array of adjacent locks in memory, a performance problem can arise when locks fall within the same cache line.

两个 CPU 改同一 cache line 里的**不同**锁，也会互相 invalidate。**解法是给锁加填充字节，一个 cache line 只放一把锁。**（第 6 章会把这事讲透。）

### §5.2.6–5.2.8 Non-Blocking I/O / Processor Binding / Performance Mantras

**非阻塞 I/O**要解决的是阻塞模型的两个代价：**每个 I/O 占住一个线程**（于是要开很多线程，每个都有创建/销毁和栈空间的开销）、**频繁的短命 I/O 让上下文切换吃掉 CPU 并增加延迟**。

原文列了四种异步机制：`open(2)` 加 `O_ASYNC`、`io_submit(2)`（Linux AIO）、`sendfile(2)`（内核内拷贝，Netflix CDN 用它发视频）、`io_uring_enter(2)`（**用户态与内核态共享的环形缓冲区**）。

**处理器绑定**（Processor Binding）：NUMA 环境下让线程留在同一个 CPU 上、I/O 之后还回原来的 CPU，能改善内存局部性。但原文警告了一段**共享环境下的陷阱**：

> Be especially careful about the risks of CPU binding when there are other tenants or applications running on the same system.

容器里应用能看到全部 CPU 就去绑，**多个租户可能绑到同一批 CPU 上**，于是 CPU 争抢 + 调度延迟，而其它 CPU 空着。

**Performance Mantras** 是一份七条的口诀清单，原文在本章只列不改：

> Do it less.

第一条「Don’t do it」就是 §5.1.3 说的**消除不必要的工作**。

---

## 3. §5.3 Programming Languages

原文的核心判断：

> Many languages list “performance optimizations” as features, but, strictly speaking, these are usually features of the software that executes the language, not the language itself.

**「Java 比 C 快」这种话是错的**——快的是 HotSpot 的 JIT，不是 Java 这个语言。

### §5.3.1 Compiled Languages

编译型语言生成机器指令，存在 ELF / PE 二进制里。原文说性能分析「通常很直截了当」，因为：

> During compilation, a symbol table can be generated that maps addresses to program functions and object names.

**符号表就是「地址 → 函数名」的映射**，有了它，profile 里的十六进制地址才能翻译成函数名。

`gcc(1)` 有七档优化（0/1/2/3/s/fast/g），`-O3` 与 `-O2` 的完整选项列表有约 230 项。原文举的第一个例子是 `-fomit-frame-pointer`：

> This is an example of a trade-off: omitting the frame pointer typically breaks the operation of analyzers that profile stack traces.

原文的结论很值得记：**这个选项带来的初步收益，可能远远抵不上「后来再也找不到性能问题」的代价。** 建议是「用 `-fno-omit-frame-pointer` 编」+「用 `-g` 带 debuginfo」（debuginfo 以后再 strip 不迟）。

还有一个常见的坑：

> Should performance issues arise, it may be tempting to simply recompile the application with a reduced optimization level (from –O3 to –O2 , for example) in the hope that any debugging needs could then be met. This turns out not to be simple: the changes to the compiler output can be massive and important, and may affect the behavior of the issue you were originally trying to analyze.

**降一级优化可能让 bug 消失或变形**——那样你追的就不是原来那个问题了。

### §5.3.2–5.3.4 Interpreted Languages / Virtual Machines / Garbage Collection

**解释型语言**：原文说 CPU profile 只能看到解释器本身（解析、翻译、执行），看不到原程序的函数名；「除非有专门的观测工具，否则分析解释型语言会很难」。常见的土办法是「加打印语句和时间戳」。

**虚拟机**：

> Virtual machines are typically the most difficult of the language types to observe.

因为程序真正在 CPU 上跑时，已经过了好几层编译/解释，原始程序的信息早没了。JIT 的符号表还在**运行时变化**，这是 §5.6.1 的伏笔。

**GC 的三个代价**，原文列得很清楚：

- **Memory growth**：内存涨上去之后，要么撞自己的上限，要么被系统换页。
- **CPU cost**：GC 是间歇性的，扫描内存里的对象；**内存越大，GC 吃的 CPU 越多**，极端情况能吃满一整个 CPU。
- **Latency outliers**：应用执行可能被 GC 暂停打断。

---

## 4. §5.4 Methodology

原文给了一张方法表（CPU profiling、Off-CPU profiling、Syscall analysis、USE method、Thread state analysis、Lock analysis、Static performance tuning、Distributed tracing），并说：

> These methodologies may be followed individually or used in combination. My suggestion is to try them in the order listed in the table.

### §5.4.1–5.4.3 CPU Profiling / Off-CPU Analysis / Syscall Analysis

**CPU profiling** 原文推荐从内核态 profiler 起步：

> I always start with kernel-based profilers (perf(1) and profile(8)) and use user-based ones as a last resort.

因为用户态 profiler **看不到内核 CPU 时间**，甚至会把「内核把应用调度走了」这段时间算错。

**火焰图**（flame graph）原文讲得最透的是读法：每个矩形是一帧栈，y 轴是调用流，**宽度正比于它在 profile 里出现的比例，x 轴的顺序没有意义**（是字母序）。要找的是那些「大平台」和「高塔」。

> In Figure 5.3 , crc32_z() is the function that is on-CPU the most, spanning about 40% of this excerpt (the center plateau).

然后是全文最妙的一个比喻——**CPU profile 里也能看出 off-CPU 的痕迹**：

> This is like finding the footprints of a bear: you don’t see the bear, but you’ve discovered that one exists.

CPU profile 能看到文件系统 I/O、块 I/O、网络 I/O、锁竞争的**存在**，但看不到它们的**量级**。原文给了几个 Linux 上的搜索关键词：`ext4` / `btrfs`（文件系统）、`blk`（块 I/O）、`tcp`（网络）、`utex`（锁竞争）、`alloc`（内存分配路径）。

**Off-CPU analysis** 的定义：

> Off-CPU analysis is the study of threads that are not currently running on a CPU

三种做法：采样、**调度器追踪**（在调度器里给 off-CPU 时长打时间戳）、应用自带埋点。原文推荐前两种，因为它们「对所有应用都适用、能看到所有 off-CPU 事件」，但**开销大**：CPU 采样在 49 Hz 下开销可忽略，off-CPU 采样却要采样**线程池**而不是 CPU 池——同样一台机器上 10000 个线程，采样开销就是 1000 倍。

原文的优化技巧：**只记录超过某个极短时长的 off-CPU 事件**（减少样本数），并**在内核态用 BPF 聚合栈**（不把样本全部送到用户态）。

> Off-CPU profiles can be visualized as an off-CPU time flame graph.

读法与 CPU 火焰图一样：找最宽的塔。但有个陷阱：

> Apart from the overheads of collecting off-CPU profiles, another issue is interpreting them: they can be dominated by wait time.

**等活干的时间**（`pthread_cond_wait()` / `futex()`）会淹没整个图。原文给两个解法：**缩进到应用请求处理函数**（MySQL 是 `do_command()`）、**采集时用内核过滤**（Linux 上匹配 `TASK_UNINTERRUPTIBLE`）。

**Syscall analysis** 原文给了三个目标：追 `execve(2)` 找短命进程问题、追 `read/write/send/recv` 找 I/O 尺寸问题、以及——

> Syscalls explain most but not all of kernel CPU time; exceptions include page faults, asynchronous kernel threads, and interrupts.

**「大部分但不是全部」**——这三个例外分别是 page fault、内核线程、中断。

### §5.4.4–5.4.6 USE Method / Thread State Analysis / Lock Analysis

**USE 方法**应用到软件资源上。原文举了个线程池 + 请求队列的例子：

- **Utilization**：区间内平均多少线程在忙（占线程总数的百分比）。
- **Saturation**：区间内请求队列的平均长度。
- **Errors**：被拒或失败的请求。

> Your task is then to find how these metrics can be measured.

**最后半句才是重点**：定义指标容易，**找到能测它的工具**才是工作。

**线程状态分析**是原文的**第一方法**：

> This is the first methodology I use for every performance issue

最少两态：on-CPU / off-CPU。原文给了**九态模型**（这一节的骨架）：

| 状态 | 含义 |
|---|---|
| User | 用户态 on-CPU |
| Kernel | 内核态 on-CPU |
| Runnable | off-CPU，等着上 CPU |
| Swapping | Runnable，但卡在匿名页换入 |
| Disk I/O | 等块设备 I/O |
| Net I/O | 等网络设备 I/O |
| Sleeping | 自愿睡眠 |
| Lock | 等别人的锁 |
| Idle | 等活干 |

> Performance for an application request is improved by reducing the time in every state except idle.

**除了 idle，每一个状态的时间都该被压。** 这句话可以直接当优化目标用。

Linux 上的实现基于 `task_struct` 的 state 字段：`R`（Runnable）、`D`（不可中断 = Disk）、`S`（可中断 = Sleep）。原文给了三种打法：**线索法**（用 `pidstat`/`vmstat`/`sar` 的现成列去猜）、**off-CPU 分析法**、**直接测量**（`/proc/PID/schedstat` 里的 runnable 纳秒数、delay accounting 等）。

原文还有一条很实用的警告：

> Because of how applications typically wait for work, you will often find that time in the network I/O and lock states is actually idle time.

**网络 I/O 状态和 Lock 状态很多时候其实是 Idle**——worker 线程在 HTTP keep-alive 上等下一个请求、或在条件变量上等着被唤醒。

**锁分析**分两件事：**查争用**（现在有没有问题）和**查过长的持锁时间**（现在可能没事，但负载上去就不一定）。原文说自旋锁的争用可以从 CPU profile 里直接看出来（它表现为 CPU 占用），而自适应互斥锁只露出一部分。

### §5.4.7–5.4.8 Static Performance Tuning / Distributed Tracing

**静态性能调优**聚焦「**配置的环境**」，原文列了一串该问的问题：版本与依赖、有没有已知性能问题（bug 数据库）、配置怎么来的（是基于测量还是拍脑袋？）、对象缓存多大、并发怎么配的、有没有开 debug 模式、用什么内存分配器、有没有用大页、编译选项是什么、有没有降级模式、有没有资源限额。

> Answering these questions may reveal configuration choices that have been overlooked.

**分布式追踪**：跨服务的请求被拆成依赖链，找出「谁贡献了延迟/错误」。原文指出一个实际难题：**日志量太大**。解法是 **head-based sampling**（在请求开头就决定采不采）或者 **tail-based sampling**（全采了再按延迟/错误筛）。

---

## 5. §5.5 Observability Tools

原文这一节按工具列了七件：**perf**（CPU profile、火焰图、syscall 追踪、内核时间分析、I/O profiling）、**profile**、**offcputime**、**strace**、**execsnoop**、**syscount**、**bpftrace**。

几个值得注意的点：

- `perf trace -p $(pgrep mysqld)` 做 syscall 追踪；`perf trace -s -p ...` 做**内核时间分析**；`perf trace -e sendto -p ...` 做 **I/O profiling**。
- `offcputime` 出的 **off-CPU 时间火焰图**，配色与 CPU 火焰图相反（蓝底）。
- `strace` 原文专门有一节讲 **Overhead**——它是**逐系统调用**的，代价最大。
- `execsnoop` 追 `execve(2)`，找短命进程。
- `syscount` 按系统调用计数（还有 `-P` 按进程）。

`bpftrace` 那一节原文分成四类用途：**Signal Tracing**、**I/O Profiling**（比如 `t:syscalls:sys_enter_recvfrom { @bytes = hist(args->size); }` 看收包大小的分布）、**Lock Tracing**、**Application Internals**。

---

## 6. §5.6 Gotchas

原文说这两个问题「你在看 CPU profile（比如火焰图）的时候就会先撞上」。

### §5.6.1 Missing Symbols

**符号缺失**：profiler 解析不出函数名，就打印十六进制或 `[unknown]`。

- **ELF 二进制**：发行版打包时被 `strip(1)` 剥过。解法是改构建流程不 strip，或者用别处的符号信息（debuginfo / BTF）。
- **JIT 运行时**（Java、Node.js）：JIT 有自己的符号表，运行时还在变，不在二进制的符号表里。解法是用运行时生成的**补充符号表**，放在 `/tmp/perf-<PID>.map`，`perf(1)` 和 BCC 都读。

原文提了一个真实的坑叫 **symbol churn**（符号漂移）：profile 采样和符号表 dump 之间符号映射变了，profile 里就是无效的函数名。**解法是 `perf record` 之后立刻跑 `jmaps`**。

### §5.6.2 Missing Stacks

栈缺失或不完整，有时只剩一两帧。原文给了一个 MySQL 的例子——`pthread_cond_timedwait()` 后面只剩一个 `[unknown]`，而**恰恰是那下面的 MySQL 函数才是应用上下文**。

原因原文写得很清楚：**两个因素叠加**——① 观测工具用**基于帧指针**的方式读栈；② 目标二进制**没给 RBP 留帧指针的位**，而是把它当通用寄存器用了（一个编译期优化）。

> The observability tool tries to resolve this number in the symbol table and, if it is lucky, it doesn’t find it and can print “[unknown]”. If it is unlucky, that random number resolves to an unrelated symbol, and now the printed stack trace has a function name that is wrong, confusing you, the end user.

**运气好是 `[unknown]`，运气差是一个看起来很合理但完全错误的函数名。** 后者更糟——你会照着错误的名字去查。

> Since the libc library is typically compiled without frame pointers, broken stacks are common in any path through libc

**任何经过 libc 的路径都可能断栈**——`pthread_cond_timedwait()` 和 `send()` 就是原文举的两个例子。

解法是修帧指针寄存器：C/C++ 用 `-fno-omit-frame-pointer`，Java 用 `-XX:+PreserveFramePointer`。

> This may come with a performance cost, but it has often been measured at less than 1%

**不到 1% 的代价，换「能用栈找到性能问题」的能力。**

另一条路是换栈遍历技术：`perf(1)` 支持 DWARF、ORC、LBR。

---

## 7. 实战锚点：`otf-server` 怎么面对这一章

仓库实况（2026-11-01）：`/home/xier/dev/game-project/otf-server/`，`src/` 下 9692 行，零三方依赖，C++20，单线程 reactor。

### 7.1 §5.1 的对照：这个「应用」是什么

原文的清单逐条对到仓库：

| 原文的问题 | otf-server 的答案 |
|---|---|
| Function | 权威游戏服务器（4 人共斗狩猎） |
| Operation | 处理 `PlayerInput` / `HitReport`，产出 `WorldSnapshot` / `BossEvent` |
| Performance requirements | ⚠️ **没有 SLO**。`otf_tick_drift_max_ms` 是**逻辑漂移**，不是请求延迟 |
| CPU mode | 用户态，Linux，C++20 |
| Configuration | `src/config/game_config.h`（玩法数值唯一产地）+ `src/config/server_config.h`（运行期参数） |
| Host | Linux，epoll |
| Metrics | ✅ `src/net/metrics.h` 的 `Metrics`，Prometheus 文本格式，独立端口 :7778 |
| Logs | ✅ `src/common/logger.h`，带结构化字段 |
| Version | ⚠️ 无版本号，`kProtocolHash` 只覆盖协议 |
| Bugs | 无 |
| Source code | ✅ 有，且有 22 个测试 target |
| Community / Books / Experts | — |

**这一行最刺眼：「没有 SLO」。** 原文说

> A performance goal provides direction for your performance analysis work

仓库有一个 `otf_tick_drift_max_ms`（tick 逻辑时间与墙钟的偏差峰值），但它**不是端到端延迟**——玩家感受到的是「我的操作到屏幕上画面动起来」的时间，那条链是：客户端输入 → 网络 → 服务器 tick → 快照 → 网络 → 客户端插值。仓库**只量了中间那一小段**。

### 7.2 §5.1.2–5.1.3 的对照：常见路径与可观测性

**常见路径**：仓库的常见路径显然是「tick 推进」——20Hz（`kTickRateHz = 20`），每 tick 跑一遍 `World::Tick`，10Hz 广播快照（`kSnapshotRateHz = 10`）。

**但仓库证明不了这一点。** 原文说常见路径「可以通过分析和 profiling 应用来确定，包括研究栈轨迹和火焰图」——**仓库零 profiling 设施**，所以没人能说出「CPU 到底花在哪儿」。这是本章最实在的一条缺口。

**可观测性**：仓库的 `Metrics` 是**选型上的正确决定**——`metrics.h` 的文件头写着「商业化会先撞到『线上出问题查不了』」。而且它做了一个很多项目不做的事：

> ⚠️ 本结构**不做任何钳位**：on_connect(-1) 减过头会照实渲染成负数。

以及「没测过 ≠ 测出来是 0」：`MetricsSnapshot::tick_drift_max_ms` 是 `std::optional<std::int64_t>`，一次都没记录过时**整行缺席**而不是输出 0。

**这正是原文 §5.1.3 的精神：可观测性的价值不在于数值好看，在于它敢说出「我不知道」。**

不过指标覆盖面很窄：连接数、房间数、收发帧数、心跳超时、超长帧拒绝、tick 漂移峰值、快照发送耗时峰值、epoll_ctl 计数。**没有延迟直方图，没有队列长度，没有 per-room 状态。**

### 7.3 §5.1.4 的对照：Big O 在哪儿

- `src/room/room_mgr.h:119`：`std::map<std::uint32_t, Room> rooms_;` —— **红黑树**，查找 O(log n)。房间数上限 32（`kMaxRooms`），所以 log₂32 = 5，**现在无所谓**。但第 6 章讲义已经指出这是一处「连续数组更合适」的候选。
- `src/net/frame_codec.cpp` 的 `FrameDecoder::Feed`：字节流逐帧扫，O(n) in 字节数，**必然**。
- `src/sim/boss_brain.cpp` 的轮廓判定：`kBodyPointCount = 69` 个点，**每 tick × 每玩家 × 每命中报告**。69 是常数，但乘上「4 玩家 × 20Hz × 每次命中」就是一个可观的常数项。

**这一条是本章 §5.1.4 那句「Big O 忽略常数开销，n 小的时候常数可能占主导」的直接应用**：仓库的性能问题**不会**来自复杂度，只会来自**常数**（69 个点、每帧几次 `resize`、每 tick 一次全房快照拷贝）。

### 7.4 §5.2 的对照：I/O 大小、缓冲、轮询

**I/O 大小**：读侧 `src/net/tcp_transport.cpp` 的 `kReadChunk = 4096`；协议侧 `kMaxPayloadSize = 1400`（一帧不触发 IP 分片）；快照 payload `kWorldSnapshotPayloadMax4 = 98` 字节。

⚠️ 这里有个**量级不匹配**：读缓冲 4096 字节，但**快照只有 98 字节**。也就是说一次 `read()` 通常能捞回 40 帧左右——这正是 `FrameDecoder` 必须处理粘包的原因（它确实处理了，见 `frame_codec.h` 的「半包 / 粘包 / 超长」三种形态）。

**缓冲**——这是仓库最实在的一处缺口：

> ⚠️ **无环形缓冲区**（`c.out.resize(old + n)`）

`TcpTransport::Send`（`src/net/tcp_transport.cpp`）里的发送缓冲是 `std::vector<uint8_t>`：

```cpp
const std::size_t old = c.out.size();
c.out.resize(old + n);
std::memcpy(c.out.data() + old, buf, n);
```

`resize` **保留已有 capacity**，所以稳态下零分配——这一点仓库做对了。但它仍是**线性缓冲**：写指针永远在尾部，读指针 `out_sent` 在前面，两端之间那段已经发出去的字节**永远不会被回收**。长时间高吞吐的连接上，`c.out` 会**单调增长直到 `kSendBufCap = 128 KB`**，然后被背压逻辑断开。

**这就是原文 §5.2.3 说的环形缓冲存在的全部理由**：固定大小、start/end 指针各自推进、内存恒定。**仓库的接收侧已经做了**（`FrameDecoder` 是真环形缓冲，`RingPush`/`RingPopTo`/`RingSkip`/`RingPeekU16`），**发送侧没有**。

**轮询 vs 事件驱动**：仓库用的是 `epoll`。而且 `src/net/transport.h` 的文件头把「不要退化成轮询」写成了硬约束：

> ★ 主循环**不走「Poll 之后遍历全表逐个 Recv」** —— 那是退回轮询，正是 §6.4 整套 epoll 纪律要避免的东西

**这是 §5.2.4 那句「poll 是 O(n)、epoll 是 O(1)」在本仓库的落点**——文件头已经知道这个坑，并且用「回调式 API」把它堵死了（epoll 的全部意义就是只碰就绪的 fd）。

### 7.5 §5.2.5 的对照：并发模型

仓库是**单线程 reactor**。按原文 §5.2.5 的分类，这最接近 **SEDA**（事件驱动的分阶段架构），但连阶段拆分都还没做——IO 与逻辑在**同一个线程**里跑：

```
Poll(timeout) → OnReadable → on_bytes_ 回调 → sessions.OnBytes → 解帧
             → TickScheduler::Advance → sessions.Tick / TickWorld → 快照
```

**IO 与逻辑未拆**是当前架构最大的天花板：一次 20ms 的 `TickWorld`（4 个玩家 × 69 个轮廓点 + 全房状态）会**阻塞所有连接的收发**。原文 §5.2.5 说多线程的一个作用是「别的线程去干活时，这个线程能阻塞在 I/O 上」——**仓库没有这个能力**。

**同步原语**：仓库是单线程的，所以**业务代码里一把锁都不需要**。唯一的原子量在 `src/net/metrics.h`，而它的文件头说得很清楚：

> ★ 线程模型：单线程 reactor。原子量是为了让 snapshot() 与 on_*() 的调用点不必严格同线程（指标打点常发生在回调里），**不代表** Metrics 是多线程安全的计数方案

**这是正确且诚实的注释。**

**假共享**：现在没有——`sessions_` 是 `std::vector<Session>`，元素之间是 `sizeof(Session)`（几百字节），不会挤在一个 cache line 里。**但地雷已经埋好**：如果哪天有人为了省内存把 `Session` 瘦身成几十字节、或者在 `std::vector<Session>` 里塞一堆小结构体并用 `std::atomic` 打点，**假共享就会来**。原文 §5.2.5 那句「用填充字节让一个 cache line 只有一把锁」在 C++ 里对应的就是 `alignas(64)`。

### 7.6 §5.2.6–5.2.8 的对照：非阻塞 I/O、绑核、口号

**非阻塞 I/O**：仓库**整个就是**非阻塞模型——`SetNonBlocking` 在监听 socket 和每个 accept 出来的连接上都设了 `O_NONBLOCK`。原文列的四种异步机制里，仓库用的是最经典的一种（`O_NONBLOCK` + `epoll`），**没有**用 `io_uring`（原文特别指出 io_uring 是「用户态与内核态共享的环形缓冲区」，正好是仓库发送侧最缺的那个形状）。

**处理器绑定**：仓库**没有**绑核，也没设 `SO_INTRINSICS`。原文警告的场景对仓库**不适用**（单机单进程独占），但一旦上容器、多副本共机，就得重新审视。

**Performance Mantras**：仓库最该套用的是第一条「**Don’t do it**」。当前最该「不做」的工作是：
1. **发送侧环形缓冲**（不做的收益 = 不做那几次 `resize` 的拷贝）
2. **重复发送整份世界快照**：10Hz × 每房 × 98 字节 × 4 玩家 = 每秒约 4 KB/房。4 人房不多，但如果以后房间数上去、或者玩家数上去，这个「整份快照」就会变成「每玩家一份整份快照」。

### 7.7 §5.3 的对照：编译型 + 一个真实的符号坑

仓库是 C++20 编译型语言。原文 §5.3.1 的「符号表让 profile 能翻译成函数名」在这里是**成立的**——但**只成立一半**。

`build/dev/compile_commands.json` 里的真实编译命令是：

```
g++ -O2 -g -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
    -Wsign-conversion -Wold-style-cast -Wnon-virtual-dtor -Werror ...
```

对照原文 §5.6.2 的判决：

- ✅ **`-g` 在** → debuginfo 有了 → **符号不会缺**（原文 §5.6.1 的 ELF 那条，仓库不会中招）。
- ❌ **`-fno-omit-frame-pointer` 不在** → GCC 的 `-O2` 在 x86-64 上**隐含** `-fomit-frame-pointer` → **§5.6.2 的「Missing Stacks」会中招**。

**这条不是理论风险。** 仓库现在的编译命令下，`perf record -g` 采到的用户栈在经过 `libc` 的路径上会断（`read()`、`write()`、`epoll_wait()` 全在 libc 里，全都没有帧指针），出现原文描述的现象：栈只剩一帧、或者多一个 `[unknown]`、**或者更糟——一个看起来合理但完全错误的函数名**。

**修复是一行**（加 `-fno-omit-frame-pointer`），代价原文说了：

> This may come with a performance cost, but it has often been measured at less than 1%

**这一行是本章对仓库最有直接价值的一条建议**：仓库将来想上 `perf` / `bpftrace` 做 CPU profile（它现在完全没有），**这个编译选项必须先加上**，否则第一次 profile 就会撞上「栈是断的」，而症状（几个 `[unknown]`）很容易被误读成「我的代码没有符号」。

另外，仓库在**编译期纪律**上做得很到位，值得表扬：`CMakeLists.txt` 给每个 target 单独设了 `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast -Wnon-virtual-dtor -Werror`，并且注释说明了**为什么不用全局 `CMAKE_CXX_FLAGS`**（会被第三方代码继承，然后在无关警告上炸）。这比原文 §5.4.7 问的「编译选项是什么」高了一个层级——**它把「配置对不对」变成了构建期的事实，而不是靠人记得**。

### 7.8 §5.4 的对照：方法论缺哪些

| 原文方法 | 仓库现状 |
|---|---|
| §5.4.1 CPU Profiling | ❌ 无。零 perf 资产、零火焰图 |
| §5.4.2 Off-CPU Analysis | ❌ 无。**单线程**下尤其需要——它能直接告诉你「主循环睡在 `epoll_wait` 上的时间 vs 干活的时间」 |
| §5.4.3 Syscall Analysis | ❌ 无。`epoll_ctl` 计数有（`otf_epoll_ctl_add/del/mod`），但**没有 `read`/`write` 次数计数** |
| §5.4.4 USE Method | ⚠️ 部分。有 utilization（`otf_frames_sent/recv` 速率）、有 errors（`otf_heartbeat_timeouts` / `otf_oversize_rejects`），**缺 saturation**（发送队列长度 `queued_bytes` 有 API 但没进指标） |
| §5.4.5 Thread State Analysis | ⚠️ 退化。单线程 → 只有 on-CPU / off-CPU 两态；`otf_tick_drift_max_ms` 是 runnable 的**代理** |
| §5.4.6 Lock Analysis | ✅ 不需要（单线程无锁） |
| §5.4.7 Static Performance Tuning | ✅ **做得好**（见下） |
| §5.4.8 Distributed Tracing | ❌ 不适用（单进程） |

**§5.4.7 这条是仓库的强项**，值得单独说。原文问「配置是怎么来的？基于测量还是拍脑袋？」——仓库的做法是**把配置变成可 grep 的事实**：

- `CMakeLists.txt` 第 16–17 行：`if(NOT CMAKE_BUILD_TYPE ...) set(CMAKE_BUILD_TYPE RelWithDebInfo ... FORCE)` —— **堵死了「忘了加 `-DCMAKE_BUILD_TYPE=Release` → 静默无优化」这个经典坑**。
- `CMakeLists.txt` 末尾 `message(STATUS "  构建类型  : ${CMAKE_BUILD_TYPE}")` —— 每次构建都打印，**配置指纹**。
- `src/main.cpp` 的 `PrintProtocolSummary()` 启动时打印 `tick_rate_hz=20 snapshot_rate_hz=10 max_players=4`、帧头大小、消息号、`kProtocolHash`。
- `src/proto/protocol.h` 的 `kProtocolHash = 3334591311` —— **改协议忘了重生成，全员握手被拒**；`main.cpp` 启动时把真值打出来。

**这就是原文那句「Answering these questions may reveal configuration choices that have been overlooked」的仓库版**——仓库把它做成了**机器可核对的事实**，而不是一份要人去读的文档。

### 7.9 §5.4.5 的对照：线程状态模型退化成两态

原文的九态模型在仓库里可以这样映射：

| 原文状态 | otf-server 对应物 | 在哪看 |
|---|---|---|
| User / Kernel | ❌ 无拆分 | 只能靠外部 `pidstat` |
| Runnable | ⚠️ `otf_tick_drift_max_ms`（**代理**，不是真值） | Prometheus :7778 |
| Swapping | ❌ 无（纯内存、无 swap 需求） | — |
| Disk I/O | ❌ 无（**零磁盘 IO**） | — |
| Net I/O | ❌ 无（无 per-connection 阻塞时间） | — |
| Sleeping | ⚠️ `epoll_wait` 的阻塞时间（**没量**） | — |
| Lock | ✅ 不存在 | — |
| Idle | ⚠️ `epoll_wait` 空转（**没量**） | — |

**最要紧的缺口是 Net I/O 与 Idle 分不开。** 仓库的 `TcpTransport::Poll(timeout_ms)` 在没有连接活动时**整段阻塞在 `epoll_wait` 上**——那是 Idle；连接活跃时它返回得快。**这两者现在完全无法区分**，因为没有任何指标记录「Poll 阻塞了多久 / 立即返回了几次」。

**加两个计数器就够**：`poll_returns`（每轮计数）和 `poll_blocked_ms`（累计阻塞时长）。**这两个数一减，Idle 与 Net I/O 的比例就出来了**——这是本章 §5.4.5「最少的两个状态」在单线程服务器上的最小可用形态。

### 7.10 §5.6 的对照：符号与栈

已在 7.7 说过：**符号没问题（`-g` 在），栈有问题（`-O2` 隐含 omit-frame-pointer）**。

补一条：**`CMakeLists.txt` 的 `-Werror` 与 `-fno-omit-frame-pointer` 一起加，不会打架**——那个选项在 `-O2` 下是合法的，只是会让 `RBP` 不再被当通用寄存器。

### 7.11 本章给仓库的行动清单

按「改动小、收益大」排序：

1. **编译加 `-fno-omit-frame-pointer`** —— 一行，让未来的 perf/bpftrace profile 不缺栈（原文说代价 <1%）。
2. **加两个指标：`poll_returns` / `poll_blocked_ms`** —— 让 Idle 与 Net I/O 第一次可分（§5.4.5 的最小形态）。
3. **把 `queued_bytes` 变成指标** —— 补齐 USE 的 saturation 那一维；顺带它就是发送侧环形缓冲的**动机度量**。
4. **发送缓冲改环形缓冲** —— 消除「发送过的字节永不回收」导致的单调增长（§5.2.3）。
5. **定义一条 SLO** —— 比如「P99 端到端延迟 < 200 ms」；哪怕现在量不出来，先写下来就定义了目标（§5.1.1）。
6. **做一次 `perf record -g` + 火焰图** —— 证明「常见路径在哪」这件事，从猜变成事实（§5.1.2）。

---

## 8. 收尾：本章金句清单

1. **Performance is best tuned closest to where the work is performed: in the applications.** —— 性能调优离工作发生的地方最近：改得动的地方才是能改的地方。
2. **A performance goal provides direction for your performance analysis work and helps you select which activities to perform.** —— 没有目标，就不是分析，是闲逛。
3. **performance analysis risks turning into a random “fishing expedition.”** —— 「随便钓几条鱼看看」不是方法论。
4. **the biggest performance wins can come from eliminating unnecessary work** —— 最大的收益来自**不做**，不是做得更快。
5. **The performance wins gained through enhanced observability may dwarf the initial 10% performance difference.** —— 选型时，「可观测性」比那 10% 的性能差值钱得多。
6. **For example, for searching a sorted array of 100 items, the difference between linear search and binary search is a factor of 21 (100/log(100)).** —— 21 倍：在 n 还不算大的时候，算法选择就已经值 21 倍。
7. **Big O notation does ignore some constant computation costs incurred for each algorithm. For cases where n (the input data size) is small, these costs may dominate.** —— 小数据量时，常数开销才是主角——**这正是「复杂度没问题但程序很慢」的来源**。
8. **“Initialization tax” is paid for small and large I/O alike. For efficiency, the more data transferred by each I/O, the better.** —— 每次 I/O 都有固定成本；大 I/O 是吞吐的杠杆。
9. **A ring buffer (or circular buffer ) is a type of fixed buffer that can be used for continuous transfer between components, which act upon the buffer asynchronously.** —— 环形缓冲的适用场景是「一端写一端读、异步、固定内存」。
10. **An alternative on Linux is epoll(2), which can avoid the scan and therefore be O(1). On BSD, the equivalent is kqueue(2).** —— poll 是 O(n)，epoll 是 O(1)：连接数一涨，这就是悬崖和缓坡的区别。
11. **Time-sharing systems (including all derived from Unix) provide program concurrency** —— 并发 ≠ 并行：前者是轮流，后者是同时。
12. **With all of these mechanisms, I/O must still be handled by the kernel, so OS thread switching is typically inevitable.** —— 用户态并发省不掉内核态的线程切换。
13. **Mutex (MUTually EXclusive) locks: Only the holder of the lock can operate. Others block and wait off-CPU.** —— 互斥锁的等待者**离开 CPU**；自旋锁的等待者**烧 CPU**。
14. **Mutex locks may be implemented by the library or kernel as a hybrid of spin and mutex locks** —— 真实世界的锁是三路的混合体：快路径、乐观自旋、慢路径阻塞。
15. **Ideally, the number of hash table buckets should be equal to or greater than the CPU count, for the potential of maximum parallelism.** —— 锁的粒度按 CPU 数来定，不是拍脑袋。
16. **With an array of adjacent locks in memory, a performance problem can arise when locks fall within the same cache line.** —— 假共享：改不同的东西，也一样互相 invalidate。
17. **Be especially careful about the risks of CPU binding when there are other tenants or applications running on the same system.** —— 绑核在共享环境里是**把租户绑到同一批 CPU 上**。
18. **Do it less.** —— 七条口诀里最短的一条，也是最容易做到的一条。
19. **Many languages list “performance optimizations” as features, but, strictly speaking, these are usually features of the software that executes the language, not the language itself.** —— 「Java 快」说的是 HotSpot，不是 Java。
20. **This is an example of a trade-off: omitting the frame pointer typically breaks the operation of analyzers that profile stack traces.** —— 省一个寄存器，可能换来「再也找不到性能问题」。
21. **Should performance issues arise, it may be tempting to simply recompile the application with a reduced optimization level (from –O3 to –O2 , for example) in the hope that any debugging needs could then be met.** —— 降一级优化会让 bug 变形，那你追的就不是原来那个问题了。
22. **Virtual machines are typically the most difficult of the language types to observe.** —— 越靠近虚拟机，越难观测。
23. **I always start with kernel-based profilers (perf(1) and profile(8)) and use user-based ones as a last resort.** —— 用户态 profiler 看不见内核时间，还会算错。
24. **This is like finding the footprints of a bear: you don’t see the bear, but you’ve discovered that one exists.** —— CPU profile 能看出 off-CPU 的**存在**，看不出**量级**。
25. **Off-CPU analysis is the study of threads that are not currently running on a CPU** —— off-CPU 分析就是研究「不在 CPU 上的那些时间」。
26. **Apart from the overheads of collecting off-CPU profiles, another issue is interpreting them: they can be dominated by wait time.** —— off-CPU 图会被「等活干」的时间淹没。
27. **Syscalls explain most but not all of kernel CPU time; exceptions include page faults, asynchronous kernel threads, and interrupts.** —— 「大部分但不是全部」——那三个例外要单独查。
28. **This is the first methodology I use for every performance issue** —— 线程状态分析排在所有方法的第一位。
29. **Performance for an application request is improved by reducing the time in every state except idle.** —— 除了 idle，每一个状态的时间都该被压——**这就是一条可执行的优化目标**。
30. **Because of how applications typically wait for work, you will often find that time in the network I/O and lock states is actually idle time.** —— 「Net I/O」和「Lock」很多时候其实是 Idle。
31. **Answering these questions may reveal configuration choices that have been overlooked.** —— 配置这一层藏着最便宜的收益。
32. **Incomplete stack traces are unfortunately common, and are usually caused by a confluence of two factors** —— 断栈是两个因素叠加的结果，不是玄学。
33. **This may come with a performance cost, but it has often been measured at less than 1%** —— 修栈的代价不到 1%，而它换来的是「能用栈找到问题」的能力。
