---
title: Chapter 5. Applications（精读）
category: 性能工程
tags:
  - 应用性能
  - 火焰图
  - 线程状态分析
  - 可观测性工具
  - 性能方法论
difficulty: 进阶
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）第5章 Applications"
source_section: "精读 §5.1.1 / §5.1.2 / §5.1.3 / §5.1.4 / §5.2.1 / §5.2.2–5.2.3 / §5.2.4 / §5.2.5 / §5.2.6 / §5.2.7 / §5.2.8 / §5.3.1 / §5.3.2–5.3.3 / §5.3.4 / §5.4.1 / §5.4.2 / §5.4.3 / §5.4.4 / §5.4.5 / §5.4.6 / §5.4.7 / §5.4.8 / §5.5.1 / §5.5.2–5.5.6 / §5.5.7 / §5.6.1 / §5.6.2；§5.7 Exercises 与 §5.8 References 不立篇（练习题与参考文献是全书共用的自测/索引材料，不承载机制陈述）；§5.1 / §5.2 / §5.3 / §5.4 / §5.5 / §5.6 下的无编号子标题按原文文字引用，不编 § 号"
skills:
  - obsidian-markdown
---

# Chapter 5. Applications

> **小节地图**：本章是全书的**应用层**——前面几章讲「资源」（CPU、内存、文件系统、磁盘、网络），这一章讲「被测对象本身」。性能最好在**离工作最近的地方**调，也就是应用里。
> 立篇：§5.1.1（目标与限制器）、§5.1.2（优化常见路径）、§5.1.3（可观测性优先于基准分）、§5.1.4（Big O 与它的盲区）、§5.2.1（I/O 尺寸）、§5.2.2–5.2.3（缓存与缓冲）、§5.2.4（轮询与 poll/epoll）、§5.2.5（并发与并行、同步原语、哈希表锁、伪共享）、§5.2.6（非阻塞 I/O）、§5.2.7（处理器绑定）、§5.2.8（Performance Mantras）、§5.3.1（编译型语言与编译器优化）、§5.3.2–5.3.3（解释型语言与虚拟机）、§5.3.4（垃圾回收）、§5.4.1（CPU Profiling 与火焰图）、§5.4.2（Off-CPU 分析）、§5.4.3（系统调用分析）、§5.4.4（USE 方法用在软件资源上）、§5.4.5（线程状态分析九状态）、§5.4.6（锁分析）、§5.4.7（静态性能调优）、§5.4.8（分布式追踪）、§5.5.1–§5.5.7（perf / profile / offcputime / strace / execsnoop / syscount / bpftrace）、§5.6.1–§5.6.2（符号与栈的坑）。
> **不立篇**：§5.7 Exercises、§5.8 References（自测题与文献索引，不承载机制陈述）。
> 底层依赖：内核态机制（模式切换 / 上下文切换 / 系统调用 / 调度器）见 [[Chapter 3. Operating Systems]]；方法论骨架（Performance Mantras、USE、drill-down、延迟指标）见 [[Chapter 2. Methodologies]]；磁盘 I/O 的量级见 [[Chapter 9. Disks]]；全书方法论起点见 [[Chapter 1. Introduction]]。
> ⚠️ 本章只给**应用层的机制与工具入口**：perf 的完整用法在第 13 章，BPF 与 bpftrace 编程在第 15 章，USDT / uprobes 在第 4 章。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §5.1 应用基础 | 05-Chapter 5. Applications.md L87 | A performance goal provides direction for your performance analysis work and helps you select which activities to perform. Without a clear goal, performance analysis risks turning into a random “fishing expedition.” |
| §5.1 应用基础 | 05-Chapter 5. Applications.md L113 | Once a goal has been chosen, you can work on the limiters for that goal. For latency, the limiter may be disk or network I/O; for throughput, it may be CPU usage. The strategies in this and other chapters will help you identify them. |
| §5.1 应用基础 | 05-Chapter 5. Applications.md L131 | One way to efficiently improve application performance is to find the most common code path for the production workload and begin by improving that. If the application is CPU-bound, that may mean the code paths that are frequently on-CPU. If the application is I/O-bound, you should be looking at the code paths that frequently lead to I/O. These can be determined by analysis and profiling of the application, including studying stack traces and flame graphs, as covered in later chapters. A higher level of context for understanding the common case may also be provided by application observability tools. |
| §5.2.1 I/O 尺寸 | 05-Chapter 5. Applications.md L199 | Increasing the I/O size is a common strategy used by applications to improve throughput. It’s usually much more efficient to transfer 128 Kbytes as a single I/O than as 128 × 1 Kbyte I/O, considering any fixed per-I/O costs. Rotational disk I/O, in particular, has historically had a high per-I/O cost due to seek time. |
| §5.2.1 I/O 尺寸 | 05-Chapter 5. Applications.md L201 | There’s a downside when the application doesn’t need larger I/O sizes. A database performing 8 Kbyte random reads may run more slowly with a 128 Kbyte disk I/O size, as 120 Kbytes of data transfer is wasted. This introduces I/O latency, which can be lowered by selecting a smaller I/O size that more closely matches what the application is requesting. Unnecessarily larger I/O sizes can also waste cache space. |
| §5.2.4 轮询与 poll/epoll | 05-Chapter 5. Applications.md L231 | The poll(2) interface supports multiple file descriptors as an array, which requires the application to scan the array when events occur to find the related file descriptors. This scanning is O(n) (see Section 5.1.4 , Big O Notation ), whose overhead can become a performance problem at scale. An alternative on Linux is epoll(2), which can avoid the scan and therefore be O(1). On BSD, the equivalent is kqueue(2). |
| §5.2.5 并发与并行 | 05-Chapter 5. Applications.md L237 | To take advantage of a multiprocessor system, an application must execute on multiple CPUs at the same time. This is parallelism , which an application may accomplish by using multiple processes ( multiprocess ) or multiple threads ( multithreaded ), each performing its own task. For reasons explained in Chapter 6 , CPUs , Section 6.3.13 , Multiprocess, Multithreading , multiple threads (or the equivalent tasks) are more efficient and are therefore the preferred approach. |
| §5.2.5 并发与并行（伪共享） | 05-Chapter 5. Applications.md L315 | With an array of adjacent locks in memory, a performance problem can arise when locks fall within the same cache line. Two CPUs updating different locks in the same cache line will encounter cache coherency overhead, with each CPU invalidating the cache line in the other’s cache. This situation is called false sharing and is commonly solved by padding locks with unused bytes so that only one lock exists in each cache line in memory. |
| §5.2.6 非阻塞 I/O | 05-Chapter 5. Applications.md L321 | Each I/O operation consumes a thread (or process) while it is blocked. In order to support many concurrent I/O, the application must create many threads (typically one for each client), which have a cost associated with thread creation and destruction, as well as stack space required to keep them. |
| §5.2.7 处理器绑定 | 05-Chapter 5. Applications.md L347 | Be especially careful about the risks of CPU binding when there are other tenants or applications running on the same system. This is a problem I’ve encountered in OS virtualization (container) environments, where an application can see all the CPUs and then bind to some, on the assumption that it is the only application on the server. When a server is shared by other tenant applications that are also binding, multiple tenants may unknowingly bind to the same CPUs, causing CPU contention and scheduler latency even though other CPUs are idle. |
| §5.3.1 编译型语言与编译器优化 | 05-Chapter 5. Applications.md L421 | This is an example of a trade-off: omitting the frame pointer typically breaks the operation of analyzers that profile stack traces. |
| §5.3.4 垃圾回收 | 05-Chapter 5. Applications.md L455 | CPU cost : GC will typically run intermittently and involves searching or scanning objects in memory. This consumes CPU resources, reducing what is available to the application for short periods. As the memory of the application grows, CPU consumption by GC may also grow. In some cases this can reach the point where GC continually consumes an entire CPU. |
| §5.4.1 CPU Profiling | 05-Chapter 5. Applications.md L537 | Applications and runtimes sometimes provide their own profiler that runs in user mode, which cannot show kernel CPU usage. These user-based profilers may have a skewed notion of CPU time as they may be unaware of when the kernel has descheduled the application, and do not account for it. I always start with kernel-based profilers (perf(1) and profile(8)) and use user-based ones as a last resort. |
| §5.4.1 CPU Profiling（off-CPU 足迹） | 05-Chapter 5. Applications.md L575 | This method only identifies the presence of these activities, not their magnitude. The CPU flame graph shows the magnitude of CPU usage, not the time spent blocked off-CPU. To measure off-CPU time directly, you can use off-CPU analysis, covered next, although it typically costs greater overhead to measure. |
| §5.4.2 Off-CPU 分析 | 05-Chapter 5. Applications.md L585 | Scheduler tracing : Instrumenting the kernel CPU scheduler to time the duration that threads are off-CPU, and recording these times with the off-CPU stack trace. Stack traces do not change when a thread is off-CPU (because it is not running to change it), so the stack trace only needs to be read once for each blocking event. |
| §5.4.5 线程状态分析 | 05-Chapter 5. Applications.md L705 | Performance for an application request is improved by reducing the time in every state except idle. Other things being equal, this would mean that application requests have lower latency, and the application can handle more load. |
| §5.4.6 锁分析 | 05-Chapter 5. Applications.md L797 | For multithreaded applications, locks can become a bottleneck, inhibiting parallelism and scalability. Single-threaded applications can be inhibited by kernel locks (e.g., file system locks). Locks can be analyzed by: |
| §5.5.1 perf | 05-Chapter 5. Applications.md L993 | The advantage of perf(1) is that it uses per-CPU buffers to reduce the overhead, making it much safer to use than the current implementation of strace(1). It can also trace system-wide, whereas strace(1) is limited to a set of processes (typically a single process), and it can trace events other than syscalls. perf(1), however, does not have as many syscall argument translations as strace(1); here is a single line from strace(1) for comparison: |
| §5.5.2–5.5.6 strace 等工具 | 05-Chapter 5. Applications.md L1215 | WARNING: The current version of strace(1) employs breakpoint-based tracing via the Linux ptrace(2) interface. This sets breakpoints for the entry and return of all syscalls (even if the -e option is used to select only some). This is invasive, and applications with high syscall rates may find their performance worsened by an order of magnitude. To illustrate this, here is the same dd(1) command without strace(1): |
| §5.6.2 栈缺失 | 05-Chapter 5. Applications.md L1552 | Incomplete stack traces are unfortunately common, and are usually caused by a confluence of two factors: 1) the observability tool using a frame pointer-based approach for reading the stack trace, and 2) the target binary not reserving a register (RBP on x86_64) for the frame pointer, instead reusing it as a general-purpose register as a compiler performance optimization. The observability tool reads this register expecting it to be a frame pointer, but in fact it could now contain anything: numbers, object address, pointers to strings, etc. The observability tool tries to resolve this number in the symbol table and, if it is lucky, it doesn’t find it and can print “[unknown]”. If it is unlucky, that random number resolves to an unrelated symbol, and now the printed stack trace has a function name that is wrong, confusing you, the end user. |

> **书里没说的**（本篇的推论 / 归类，原文没有，别当原文观点）：
> - **「每次 I/O 的固定成本」是本篇的说法**：原文把这串成本称为 “initialization tax”（§5.2.1 开头），并说它对小 I/O 和大 I/O 一视同仁。
> - **「缓存与缓冲占同一块内存、互相挤预算」是本篇的推论**：原文只说缓存的存储空间常被当作缓冲使用（§5.2.2 末句），没有讨论两者共享内存预算。
> - **「fiber / 协程 / 事件驱动归为『用户态自调度』一类」是本篇的归类**：原文是三条并列陈述，没有把它们收成一个类别。
> - **「§5.1.4 的 Big O 在本篇被降格为『读 §5.2.4 复杂度结论的前提』」是本篇的读法**：原文把 Big O 当作独立小节讲算法复杂度与病理条件。
> - **「perf trace 可能取代 strace」是原文给出的未来方向**（§5.5.4 末段：a future version of strace(1) may solve its overhead problem by becoming an alias to the perf(1) trace subcommand），**不是现状**——本篇不把它当既成事实。

## §5.1 应用基础：先把问题定义清楚

### §5.1.1 目标与限制器

**机制**：性能分析的第一步不是动手测，而是**写下目标**。原文说得很直接：

> A performance goal provides direction for your performance analysis work and helps you select which activities to perform. Without a clear goal, performance analysis risks turning into a random “fishing expedition.”

四类目标：**延迟**（低或稳定的响应时间）、**吞吐**（高操作速率或数据吞吐率）、**资源利用率**（给定负载下的效率）、**价格**（性能/价格比、降低算力成本）。

目标必须**可量化**，原文给的例子是这一串（注意后几条是「负载 + 约束」的组合，不是单值）：

- 平均请求延迟 5 ms
- 95% 的请求在 100 ms 以内完成
- 消除延迟离群点：没有任何请求超过 1,000 ms
- 每台指定规格的服务器至少 10,000 请求/秒
- 每 10,000 请求/秒下磁盘平均利用率低于 50%

**目标选定之后才去找限制器**：

> Once a goal has been chosen, you can work on the limiters for that goal. For latency, the limiter may be disk or network I/O; for throughput, it may be CPU usage. The strategies in this and other chapters will help you identify them.

**吞吐目标必须额外限定「是哪类操作」**——原文明确说不是所有操作在性能与成本上等价；如果目标是某个操作速率，就需要说明操作的类型，可以**用期望负载或实测负载的分布**来表达。

**§5.1.1 的 Apdex**：把客户事件分成 satisfactory / tolerable / frustrating 三档，公式是

```
Apdex = (satisfactory + 0.5 × tolerable + 0 × frustrating) / total events
```

取值 0（没人满意）到 1（全部满意）。

**§5.1 的应用自查清单**（原文以提问形式列出 14 项，本篇按用途归类）：**角色**（它是数据库服务器 / 网页服务器 / 负载均衡器 / 文件服务器 / 对象存储？）→ **操作**（它服务什么请求：数据库服务查询，网页服务器服务 HTTP 请求；**这个可以量化成速率**，用来评估负载和做容量规划）→ **性能要求**（有没有 SLO，比如 99.9% 的请求延迟 < 100 ms）→ **CPU 模式**（用户态还是内核态：多数应用是用户态进程，少数是内核服务如 NFS，BPF 程序也是内核态）→ **配置**（怎么配的、为什么这么配；性能相关参数被改过吗：缓冲区大小、缓存大小、并行度等）→ **宿主**（跑在哪台机器/云实例上，CPU、内存拓扑、存储设备及其限制）→ **指标**（有没有应用指标）→ **日志**（有没有操作日志、能否开启、里面有没有延迟；例：MySQL 的**慢查询日志**给出每个超过阈值的查询的性能细节）→ **版本**（是不是最新版，release notes 有没有性能修复）→ **Bug**（有没有 bug 库，当前版本有哪些「性能」bug）→ **源码**（是否开源；profiler 找到的代码路径可以研究，甚至可以改完提上游）→ **社区**（论坛 / 博客 / IRC / Slack / Meetup / 会议，会议的 slides 和视频往往多年后仍有用）→ **书**（有没有讲这个应用或其性能的书，质量如何）→ **专家**（公认的性能专家是谁，学他们的名字能找到他们写的材料）。

原文的收尾建议：不管信息来自哪里，目标都是**在高层理解这个应用**——它做什么、怎么运转、性能如何；而**一份说明应用内部结构的功能图**是极其有用的资源。

**判据（推演）**：目标不只是形式，它是**决定用哪条方法论的开关**——延迟目标先查 off-CPU，吞吐目标先查 CPU。没有目标就只能「钓鱼」。

### §5.1.2 优化常见路径

**机制**：应用内部动辄几万行代码、内核几十万行，随手挑地方优化是「大量工作、很少收益」。有效做法是**找到生产负载中最常见的代码路径并从它开始**：

> One way to efficiently improve application performance is to find the most common code path for the production workload and begin by improving that. If the application is CPU-bound, that may mean the code paths that are frequently on-CPU. If the application is I/O-bound, you should be looking at the code paths that frequently lead to I/O. These can be determined by analysis and profiling of the application, including studying stack traces and flame graphs, as covered in later chapters. A higher level of context for understanding the common case may also be provided by application observability tools.

**判据（推演）**：CPU-bound 就优化「常驻 CPU 的路径」，I/O-bound 就优化「常引发 I/O 的路径」——两条路的工具完全不同（前者 CPU 火焰图，后者 off-CPU / I/O 剖析）。先判断是哪种 bound，再决定采哪种 profile。

### §5.1.3 可观测性优先于基准分

原文给了一个反直觉的选型判断（原文 L137）：如果基准测试显示**应用 A 比应用 B 快 10%**，人很容易选 A；但**如果 A 是个黑盒，而 B 提供了丰富的可观测性工具**，那么长期看 B 才是更好的选择——因为这些工具让你能**看见并消除无用功**，也能更好地理解并调优真正在跑的工作；**可观测性带来的收益可能远远盖过那 10% 的差距**。

同一条逻辑也适用于**语言与运行时的选型**（原文举例 Java 与 C——成熟、工具多，对比一门新语言）。

**这条与 §5.1.2 和 §5.2.8 的 “Don't do it” 是同一主张的三种说法**：先问「要不要做」，再问「怎么做」。

### §5.1.4 Big O 与它的盲区

Big O 描述**函数增长的阶**，用来分析算法复杂度、预测输入规模变大时的表现。原文给的对照表（Table 5.1）是 O(1) 布尔测试、O(log n) 有序数组二分查找、O(n) 链表线性查找、O(n log n) 快排平均、O(n²) 冒泡排序平均、O(2ⁿ) 质因数分解与指数增长、O(n!) 旅行商问题暴力枚举。

原文用一个**具体量级**说明它能干什么：在 100 个元素的**有序数组**里搜索，线性搜索与二分搜索的差距是 **21 倍（100/log(100)）**。

原文也说清了它的**病理条件**：性能问题常常在「服务对象或数据对象比以往多得多」时才暴露，此时 O(n²) 之类的算法开始病态；修法可能是**换更高效的算法**，也可能是**换一种划分数据的方式**。

**但 Big O 忽略每个算法固有的常数计算成本——n 很小时这些常数项可能占主导**（原文 L189，未列入本篇锚点表）。这一句是**下面 §5.2.4 那句「poll 是 O(n)、epoll 是 O(1)」的前提**：O(1) 说的是「每次事件的处理成本不随 fd 数增长」，不是「永远更快」。

## §5.2.1 I/O 尺寸：吞吐与延迟的同一个旋钮

**机制**：一次 I/O 的成本里有一长串**每次都要付的固定项**——初始化缓冲区、发系统调用、模式/上下文切换、分配内核元数据、检查进程权限与限额、把地址映射到设备、执行内核与驱动代码把 I/O 送出去，最后释放元数据与缓冲区。原文把这串称为 **“initialization tax”**，并强调**它对小 I/O 和大 I/O 一视同仁**。

于是每单位数据摊到的固定成本随 I/O 尺寸增大而下降：

> Increasing the I/O size is a common strategy used by applications to improve throughput. It’s usually much more efficient to transfer 128 Kbytes as a single I/O than as 128 × 1 Kbyte I/O, considering any fixed per-I/O costs. Rotational disk I/O, in particular, has historically had a high per-I/O cost due to seek time.

**但反面同样是机制性的**：

> There’s a downside when the application doesn’t need larger I/O sizes. A database performing 8 Kbyte random reads may run more slowly with a 128 Kbyte disk I/O size, as 120 Kbytes of data transfer is wasted. This introduces I/O latency, which can be lowered by selecting a smaller I/O size that more closely matches what the application is requesting. Unnecessarily larger I/O sizes can also waste cache space.

**判据**：I/O 尺寸**不是越大越好**，它由**目标**决定——吞吐目标调大、延迟目标调小到与请求匹配。原文在 §5.1.1 结尾就把这句话说死了：选更大的 I/O 尺寸可能改善吞吐、**代价是延迟**。另外，别忘了**过大的 I/O 尺寸会白占缓存空间**。

## §5.2.2–5.2.3 缓存与缓冲

**缓存（cache）**：操作系统用缓存改善文件系统读性能与内存分配性能；应用同理——**把常用操作的结果存在本地缓存**里，避免反复做昂贵操作。原文的例子是**数据库 buffer cache**，存常用查询的数据。

部署应用时的常见动作：**搞清楚这个应用提供了哪些缓存（或哪些可以开启），然后把它们的尺寸调成适合本系统**。

**缓存的副作用**（原文 §5.2.2 末句）：**缓存的存储空间经常被当作缓冲使用来改善写性能**——这是缓存与缓冲的接口。

**缓冲（buffering）**：为了改善写性能，**先把数据合并（coalesce）在缓冲区里**再交给下一层，这既**提高 I/O 尺寸**也**提高效率**；代价是**可能增加写延迟**——因为缓冲区的第一次写要**等后续写**才能发出去。

**环形缓冲（ring buffer / circular buffer）**是定长缓冲，供两个组件之间**连续、异步**地传输数据，用**起始指针与结束指针**实现，各组件在追加或移除数据时移动指针。

**判据（推演）**：读路径用缓存降延迟，写路径用缓冲降系统调用次数与 I/O 次数；但两者**占的是同一块内存**——所以把缓存调大之前，先算清楚还剩多少留给缓冲。

## §5.2.4 轮询与 poll()/epoll()

**机制**：**轮询（polling）**是用一个循环反复检查事件状态、中间加暂停，来等事件发生。**工作很少的时候**，轮询有两个性能问题：**反复检查的 CPU 开销**很高；**事件发生到下一次检查之间的延迟**很高。

**改法**：让应用**改为监听事件**——事件一到就立刻通知应用并执行对应例程。

**poll(2) 是事件驱动的**，因此不吃轮询的 CPU 开销，但它是**数组接口**，事件到来时必须**扫描数组**才能找到相关的文件描述符：

> The poll(2) interface supports multiple file descriptors as an array, which requires the application to scan the array when events occur to find the related file descriptors. This scanning is O(n) (see Section 5.1.4 , Big O Notation ), whose overhead can become a performance problem at scale. An alternative on Linux is epoll(2), which can avoid the scan and therefore be O(1). On BSD, the equivalent is kqueue(2).

**判据**：fd 数量少 → poll 够用；fd 数量大或持续增长 → 上 `epoll(2)` / `kqueue(2)`。**但这是 O(n) vs O(1)，不是「有 vs 无」**——见 §5.1.4 的常数成本限定：n 小的时候，poll 的扫描可能根本不是瓶颈。

## §5.2.5 并发与并行：线程、纤程、协程与锁

### 并发 ≠ 并行

**并发（concurrency）**：Unix 及其衍生的分时系统提供的能力——能**加载并开始执行多个可运行的程序**。它们的运行时段**可以重叠，但不保证同一时刻真的在 CPU 上执行**。每个程序可以是一个应用进程。

**并行（parallelism）**：要吃满多处理器系统，应用必须**同一时刻在多个 CPU 上执行**：

> To take advantage of a multiprocessor system, an application must execute on multiple CPUs at the same time. This is parallelism , which an application may accomplish by using multiple processes ( multiprocess ) or multiple threads ( multithreaded ), each performing its own task. For reasons explained in Chapter 6 , CPUs , Section 6.3.13 , Multiprocess, Multithreading , multiple threads (or the equivalent tasks) are more efficient and are therefore the preferred approach.

**原文的偏好**：**线程（或等价的任务）比进程更高效**，理由在第 6 章 §6.3.13。

多线程除提高 CPU 吞吐外还有一个作用：**让 I/O 并发进行**——一个线程阻塞在 I/O 上时，其他线程可以继续跑（另一种做法是异步 I/O）。

### 三种用户态自调度机制

用多进程/多线程意味着**把「谁来跑」的决定权交给内核**（CPU 调度器），代价是**上下文切换开销**。另一条路是**应用自己实现调度与编程模型**，在同一个 OS 线程里服务不同请求。原文列了三种：

| 机制 | 机制说明 | 原文指出的注意点 |
|---|---|---|
| **Fibers（轻量线程）** | 用户态的线程，每个 fiber 是一个**可调度的程序**，应用用自己的调度逻辑决定跑哪个 | 微软文档警告：线程局部存储在 fiber 间**共享**，得改用 fiber-local storage；退出线程会退出该线程上**所有** fiber。原文引微软文档原话：“In general, fibers do not provide advantages over a well-designed multithreaded application” |
| **Co-routines（协程）** | 比 fiber 更轻，由**用户态应用调度**的子程序，提供并发机制 | — |
| **Event-based concurrency（事件驱动）** | 程序拆成一串**事件处理器**，可运行的事件**排队后依次执行**；可为每个请求分配元数据，供事件处理器引用 | Node.js 用的是**单个事件 worker 线程**——**它可能成为瓶颈，因为它只能在一个 CPU 上执行** |

**原文给的硬约束**（这一段是本节最该记住的）：**I/O 终究要由内核处理，所以 OS 线程切换通常无法避免**；而且**要并行就必须用多个 OS 线程**（一个线程只能被调度到一个 CPU 上）。原文给的例外是：用 `sendfile(2)` 避开 I/O 系统调用，以及 Linux **io_uring**（允许用户态通过读写共享的 ring buffer 来调度 I/O，见 §5.2.6）。

**混合模型**：有些运行时**同时用协程做轻量并发、用多 OS 线程做并行**。原文的例子是 **Golang**：goroutine（协程）跑在一池 OS 线程上；为了性能，当一个 goroutine 发起阻塞调用时，**Golang 的调度器会自动把阻塞线程上的其他 goroutine 迁到别的线程去跑**。

### 三种多线程编程模型

- **Service thread pool**：一池线程服务网络请求，**每个线程一次服务一个客户端连接**。
- **CPU thread pool**：**每个 CPU 一个线程**，常用于长时批处理（如视频编码）。
- **SEDA（Staged event-driven architecture）**：请求被**分解成若干 stage**，每个 stage 由一或多线程池处理。

**为什么用线程**：线程与进程**共享同一地址空间**，可以直接读写同一块内存，不必用更贵的接口（比如多进程的进程间通信 IPC）。代价是**必须用同步原语**保证数据不会被并发读写搞坏。

### 同步原语：互斥、自旋、读写锁、信号量

原文的类比是**红绿灯**：同步原语管理对内存的访问以保证完整性，就像红绿灯调节十字路口的通行——**但它们也会挡住车流，造成等待（延迟）**。

| 原语 | 行为 | 代价 |
|---|---|---|
| **Mutex（互斥锁）** | 只有持锁者能操作，其他人**阻塞并离开 CPU** 等 | 上下文切换成本 |
| **Spin lock（自旋锁）** | 持锁者操作，其他人**在 CPU 上紧循环**检查锁是否释放 | **延迟低**（阻塞的线程从没离开 CPU，锁一释放几个周期内就能跑），但**等待期间白烧 CPU** |
| **RW lock（读写锁）** | 保证完整性：**多个读者** 或 **一个写者且无读者**，二选一 | 读多写少时最划算 |
| **Semaphore（信号量）** | 变量类型：可以是**计数型**（允许给定数量的并行操作），也可以是**二值型**（只允许一个，等价于互斥锁） | — |

**Linux 的互斥锁是「自旋 + 阻塞」的混合体**：锁在**别的 CPU 上运行时自旋**，不在时就阻塞（或达到自旋阈值后阻塞）。这套实现 2009 年进 Linux（[Zijlstra 09]），现在有**三条路径**（见内核 `Documentation/locking/mutex-design.rst`）：

- **fastpath**：用 **cmpxchg 指令**设 owner 抢锁，**只有锁未被持有时才成功**；
- **midpath（也叫 optimistic spinning）**：持锁者也在别的 CPU 上跑时就**在 CPU 上自旋**，赌它很快释放、这样就能免阻塞地拿到；
- **slowpath**：**阻塞并把线程下线（deschedule）**，等锁可用时再唤醒。

**RCU（read-copy-update）**：Linux 内核里重度使用的另一种同步机制，**读操作完全不需要加锁**，因此比其他锁类型更快。机制是：**写操作复制一份被保护的数据并更新副本**，此时**在途的读仍然访问原件**；RCU 能**检测到已经没有读者了**（依据若干 per-CPU 条件），然后再把原件换成更新后的副本。

原文给的现实提醒：**排查锁的性能问题很耗时，而且通常需要熟悉应用源码**——**这一般是开发者的活**。

### 哈希表锁与伪共享

两种极端都不理想：**一把全局互斥锁**（简单，但所有并发访问都撞锁、排队等待，需要锁的多个线程**串行执行**而不是并发）；**每个数据结构一把锁**（竞争只在真需要时发生，但每个结构都要付锁的存储开销，以及**创建与销毁这把锁的 CPU 开销**）。

**哈希表锁是折中**（原文 L301）：

> A hash table of locks is an in-between solution and is suitable when lock contention is expected to be light. A fixed number of locks is created, and a hashing algorithm is used to select which lock is used for which data structure. This avoids the creation and destruction cost with the data structure and also avoids the problem of having only a single lock.

原文给的实现细节：**桶（bucket）里的每一项各带一把锁**（图 5.2 的例子里有四个桶）；**哈希冲突**（两个以上结构散到同一个桶）用**哈希链**解决——同一桶下挂一条链，之后靠哈希函数再找到它们。⚠️ **哈希链过长会成为性能问题**：链被串行遍历、且**只有一把锁保护**，锁的持有时间会变长。原文建议**按生产负载检查哈希链长度**，以防哈希算法没按预期工作。

原文给的调参建议：**桶数理想上应 ≥ CPU 数**（取并行度上限）；哈希算法可以很简单——**取数据结构地址的低位（或中位）当索引，数组长度取 2 的幂**；低位的坏处是「指向数组元素的结构的低位地址可能碰撞太多」（原文脚注 4）。

**伪共享（false sharing）**——两个毫不相干的锁挤在同一条 cache line：

> With an array of adjacent locks in memory, a performance problem can arise when locks fall within the same cache line. Two CPUs updating different locks in the same cache line will encounter cache coherency overhead, with each CPU invalidating the cache line in the other’s cache. This situation is called false sharing and is commonly solved by padding locks with unused bytes so that only one lock exists in each cache line in memory.

**判据**：锁本身没问题、竞争也不高，但多核下就是慢——先查**有没有两把锁在同一条 cache line**。解法是**用填充字节把锁隔开**，让每条 cache line 只装一把锁。（这条的底层机制在第 6 章 CPU 与第 7 章内存。）

## §5.2.6 非阻塞 I/O

**机制**：Unix 的进程生命周期里，进程在 I/O 期间**阻塞并进入睡眠态**。这个模型有两个性能问题。第一个是线程成本：

> Each I/O operation consumes a thread (or process) while it is blocked. In order to support many concurrent I/O, the application must create many threads (typically one for each client), which have a cost associated with thread creation and destruction, as well as stack space required to keep them.

第二个是**频繁的短命 I/O** → **频繁的上下文切换** → **吃掉 CPU 资源并增加应用延迟**。

**非阻塞 I/O 模型异步发起 I/O，不阻塞当前线程**，线程可以去做别的事。Node.js 是这一模型的关键代表（它把「代码要按非阻塞方式写」当成设计前提）。

原文列的机制：

| 机制 | 做法 |
|---|---|
| `open(2)` + **`O_ASYNC` 标志** | I/O 变得可行时，用**信号**通知进程 |
| `io_submit(2)` | **Linux 异步 I/O（AIO）** |
| `sendfile(2)` | 在两个 fd 之间复制数据，**把 I/O 交给内核**而不是在用户态做 I/O（原文脚注：Netflix CDN 用它给客户发视频资产，避开用户态 I/O 开销） |
| `io_uring_enter(2)` | **Linux io_uring**：用**用户态与内核态共享的 ring buffer** 提交异步 I/O |

**判据**：连接数上千、每个连接一次短读 → 线程-per-connection 的成本（创建销毁 + 栈空间 + 上下文切换）会成为主因。这时要么换非阻塞 I/O，要么把线程池上限与并发模型对齐——**但注意非阻塞 I/O 并不能凭空消灭线程切换**（§5.2.5 的硬约束）。

## §5.2.7 处理器绑定：NUMA 亲和与绑核风险

**机制**：在 **NUMA** 环境下，让一个进程或线程**留在同一个 CPU 上**、并且**做完 I/O 之后仍然回到同一个 CPU**，可以改善应用的**内存局部性**，减少内存 I/O 的周期数。操作系统其实已经知道并这么设计了——这就是 **CPU affinity**（主题在第 7 章内存）。

有些应用**自己强制**绑到某些 CPU 上。这**可能显著提升性能**，但**也可能降低性能**——比如绑核与其他绑核**冲突**时（典型：**设备中断到 CPU 的映射**）。

⚠️ **最大的风险在共享环境**：

> Be especially careful about the risks of CPU binding when there are other tenants or applications running on the same system. This is a problem I’ve encountered in OS virtualization (container) environments, where an application can see all the CPUs and then bind to some, on the assumption that it is the only application on the server. When a server is shared by other tenant applications that are also binding, multiple tenants may unknowingly bind to the same CPUs, causing CPU contention and scheduler latency even though other CPUs are idle.

即：**容器里的应用能看到全部 CPU，于是以为自己独占服务器而绑了一部分；多租户各自绑定，结果多个租户绑到同一批 CPU**——于是产生 CPU 争用与调度延迟，**而其他 CPU 明明是空闲的**。

**另一个坑**：应用的**宿主规格会变**，**没跟着更新的绑定会从帮忙变成拖累**——比如无谓地跨多个 socket 去绑。

**判据**：绑核是**最后手段**，不是默认动作。动手前先确认三件事：① 是不是真的 NUMA 布局；② 是不是多租户 / 容器环境；③ 绑的 CPU 是不是设备中断所在的那个。

## §5.2.8 Performance Mantras

原文在这一节只做了一件事：把第 2 章的 **Performance Mantras** 方法论复述成七条，并点明**第一条 “Don't do it” 就是消除不必要的劳动**：

1. **Don't do it.**（消除无用功）
2. **Do it, but don't do it again.**（缓存）
3. **Do it less.**
4. **Do it later.**（延迟 / 批处理）
5. **Do it when they're not looking.**（离线 / 错峰）
6. **Do it concurrently.**（并发）
7. **Do it cheaper.**（换更省的实现）

**（推演）**：这七条与 §5.1.2 的「优化常见路径」、§5.1.3 的「消除无用功」是**同一条主张的三种说法**——**先问要不要做，再问怎么做**。

## §5.3.1 编译型语言与编译器优化

**机制**：编译把程序**提前**翻译成机器指令，存进二进制可执行文件（Linux 及其他 Unix 衍生用 **ELF**，Windows 用 **PE**），之后**随时可以运行、无需再编译**。编译型语言包括 C、C++ 和汇编；有些语言既有解释器也有编译器。

**性能分析为什么在编译型语言上相对直接**：执行的机器码**通常与原程序紧密对应**（虽然取决于编译优化）；编译期可以生成**符号表**，把地址映射到程序函数与对象名；之后对 CPU 执行的 profiling 与 tracing 就能**直接映射到程序名**，**栈回溯及其中的数值地址也能翻译成函数名**，从而给出代码路径的祖先链。

**编译器优化（compiler optimizations）**就是「优化 CPU 指令的选择与摆放」的例程。`gcc(1)` 提供**七档优化**：`0 1 2 3 s fast g`——数字里 0 最少、3 最多；`s` 为体积优化、`g` 为调试、`fast` 打开全部优化并**放弃标准合规**。原文给的查询方法是 `gcc -Q -O3 --help=optimizers`。⚠️ **gcc 7.4.0 的完整列表约 230 个选项，其中一些在 `-O0` 下也是启用的。**

**最关键的一个选项是 `-fomit-frame-pointer`**（列表里显示为 enabled）：不为不需要的函数在寄存器里保留帧指针，省掉保存/设置/恢复帧指针的指令，并在很多函数里**多出一个可用寄存器**。代价按 gcc 手册是「在某些机器上无法调试」。

原文对这个取舍的判断：

> This is an example of a trade-off: omitting the frame pointer typically breaks the operation of analyzers that profile stack traces.

原文的结论很直接：栈剖析工具很有用，这个选项**牺牲掉的「以后还能找到的性能问题」可能远远大于它当下带来的性能收益**；解法是**用 `-fno-omit-frame-pointer` 编译**。另外建议用 **`-g` 保留 debuginfo** 方便日后调试（debuginfo 事后可以 strip 掉；如果要分发 stripped 二进制，可以做 debuginfo 包）。

**判据**：要上生产、又打算被剖析的应用，**帧指针是必须付的保险费**——§5.6.2 给的实测代价是「通常低于 1%」。

⚠️ **另一个坑**：遇到性能问题就**降优化档位**（比如 `-O3` → `-O2`）来「方便调试」——原文说这**并不简单**：编译器输出的变化**可能又大又重要**，甚至**改变你原本要分析的那个问题的行为**。

## §5.3.2–5.3.3 解释型语言与虚拟机

**解释型语言**：运行时把程序翻译成动作执行，**这会增加执行开销**。原文的定位很中性：解释型语言**本就不期待高性能**，它们用在**其他因素更重要**的场景，比如易写易调试——shell 脚本就是例子。

**分析难点**：**除非提供了可观测性工具**，否则分析解释型语言很困难。CPU profiling 能看到的是**解释器自身**在干什么（解析、翻译、执行动作），**可能看不到原程序的函数名**，关键上下文成了谜。不过这**也不完全白费**——**解释器本身**可能就有性能问题，即使它执行的代码看起来设计得很好。

原文给的替代手段：**程序上下文可能作为解释器函数的参数出现**（用**动态插桩**能看到）；或者**去读进程内存**（前提是了解程序布局，例如 Linux 的 `process_vm_readv(2)`）；最朴素的办法是**加打印语句和时间戳**。原文的评价：**更严谨的性能分析很少见**，因为解释型语言本来就不常被选来做高性能应用。

**虚拟机（VM，也叫 process virtual machine）**是模拟计算机的软件。Java、Erlang 等语言常跑在 VM 上：应用被编译成 **字节码（bytecode）**，由 VM 执行——只要目标平台上有可用的 VM，编译产物就**可移植**。

**HotSpot** 同时支持**解释执行**与 **JIT 编译**（把字节码编译成机器码交给处理器直接执行），**兼得编译代码的性能与虚拟机的可移植性**。

**虚拟机是最难观测的一类**：

> Virtual machines are typically the most difficult of the language types to observe. By the time the program is executing on-CPU, multiple stages of compilation or interpretation may have passed, and information about the original program may not be readily available. Performance analysis usually focuses on the toolset provided with the language virtual machine, many of which provide USDT probes, and on third-party tools.

**判据**：分析 JVM / Node.js，**先找运行时自带的探针（USDT）**，而不是先上通用 profiler；§5.6.1 给出的 JIT 符号方案（`/tmp/perf-<PID>.map` + `jmaps`）是这条线的续集。

## §5.3.4 垃圾回收：三个代价

有些语言用**自动内存管理**——分配的内存不必显式释放，交给**异步的垃圾回收（GC）进程**。这让程序好写，但有三个代价：

1. **内存增长**：对内存用量的**控制变少**——对象没被自动识别为可回收时内存就会涨；涨得太大，**要么撞上自己的上限，要么遇到系统换页（Linux swapping），性能严重受损**。
2. **CPU 成本**：

> CPU cost : GC will typically run intermittently and involves searching or scanning objects in memory. This consumes CPU resources, reducing what is available to the application for short periods. As the memory of the application grows, CPU consumption by GC may also grow. In some cases this can reach the point where GC continually consumes an entire CPU.

3. **延迟离群**：GC 执行期间应用可能被**暂停**，造成**偶发的高延迟响应**。这**取决于 GC 类型**：stop-the-world、incremental、concurrent。

**GC 是性能调优的常见目标**——为的是**降低 CPU 成本**和**减少延迟离群的出现**。原文举例：**Java VM 提供了大量可调参数**：GC 类型、GC 线程数、最大堆大小、目标堆空闲比等。

**如果调优无效**，问题可能在于**应用制造了太多垃圾**，或者**泄漏了引用**——这两件都是**应用开发者**要解决的。可行的方向是**尽量少分配对象**；用能**显示对象分配及其代码路径**的工具找出可消除的分配点。

**判据**：CPU 占用高时，先用 profiler 分清「是 GC 还是业务代码」——原文给的典型路径是先看出是 GC，再用可调参数压下去；压不动就去查分配量与引用泄漏。

## §5.4.1 CPU Profiling 与火焰图

**机制**：CPU profiling 是应用性能分析的**基本活动**（第 6 章 §6.5.4 起展开）。Linux 上有很多 profiler，`perf(1)` 与 `profile(8)`（见 §5.5）都用**定时采样**，而且它们**跑在内核态**，能同时抓到**内核栈与用户栈**，产出**混合模式（mixed-mode）profile**，**几乎完整地覆盖了 CPU 占用**。

⚠️ **用户态 profiler 的陷阱**：

> Applications and runtimes sometimes provide their own profiler that runs in user mode, which cannot show kernel CPU usage. These user-based profilers may have a skewed notion of CPU time as they may be unaware of when the kernel has descheduled the application, and do not account for it. I always start with kernel-based profilers (perf(1) and profile(8)) and use user-based ones as a last resort.

**采样量级**：Netflix 的一次典型 CPU profile 是**在约 32 个 CPU 上以 49 Hz 采 30 秒，共 47,040 个样本**。

**火焰图（flame graph）读法**：

> In a flame graph, each rectangle is a frame from a stack trace, and the y-axis shows the code flow: top-down shows the current function and then its ancestry. The frame width is proportional to its presence in the profile, and the x-axis ordering has no meaning (it is an alphabetical sort). You look for the big “plateaus” or “towers”—that’s where the bulk of the CPU time is spent. See Chapter 6 , CPUs , Section 6.7.3 , Flame Graphs , for more detail about flame graphs. 

即：**每个矩形是一层栈帧**；**y 轴自上而下是调用链**（当前函数在上，祖先在下）；**宽度正比于它在 profile 中的占比**；**x 轴的排列没有含义**（只是字母序）。**要找的是宽的平台和塔尖**。

**原文的实操例子**（图 5.3）：`crc32_z()` 是**在 CPU 上时间最长**的函数，占这段摘录的约 **40%**（中间那个平台）；左边一座塔显示一条 `write(2)` 进内核的路径，占约 **30%** 的 CPU 时间。**往下（沿祖先链）看**能找出高层入口：这个例子里**全部** CPU 使用都来自 `MYSQL_BIN_LOG::commit()`。

⚠️ **一个心态上的关键提醒**：原文明说 **他不知道 `crc32_z()` 或 `MYSQL_BIN_LOG::commit()` 是干什么的**——CPU profile 会暴露应用的内部运转，**除非你就是这个应用的开发者，否则你不该指望认识其中任何一个函数**；你得**去研究它们**才能得出可执行的优化。原文的实操是：上网搜 `MYSQL_BIN_LOG::commit()`，很快找到讲 MySQL binlog（用于数据库恢复与复制）以及如何调优甚至**完全关闭**它的文章；搜 `crc32_z()` 发现是 zlib 的**校验和**函数——于是接着问：有没有更新更快的 zlib？处理器有没有**优化过的 CRC 指令**、zlib 用上了吗？MySQL 到底需不需要算这个 CRC，能不能关掉？

### CPU 火焰图里的 off-CPU 足迹

CPU profile 不只显示 CPU 占用，还能**看出别的 off-CPU 问题的痕迹**——磁盘 I/O（通过文件系统访问与块 I/O 初始化的 CPU 用量）、网络 I/O、锁竞争等等。原文的比喻是**「熊的脚印：你没看见熊，但发现它存在」**。

⚠️ **但它只证明「有」，不证明「多」**：

> This method only identifies the presence of these activities, not their magnitude. The CPU flame graph shows the magnitude of CPU usage, not the time spent blocked off-CPU. To measure off-CPU time directly, you can use off-CPU analysis, covered next, although it typically costs greater overhead to measure.

原文给的**搜索线索**（Linux 函数名）：**`ext4`**（或 `btrfs` / `xfs` / `zfs`）找文件系统操作、**`blk`** 找块 I/O、**`tcp`** 找网络 I/O、**`utex`** 显示锁竞争（mutex / futex）、**`alloc`** 或 **`object`** 显示做内存分配的代码路径。

**判据**：**CPU 火焰图不是延迟图**。它显示的是 CPU 用量的量级，不是**阻塞在 off-CPU 的时间**。要量阻塞时间就得做 §5.4.2 的 off-CPU 分析（开销更大）。

## §5.4.2 Off-CPU 分析

**机制**：**off-CPU = 线程当前不在 CPU 上运行的状态**。它涵盖**所有阻塞原因**：磁盘 I/O、网络 I/O、锁竞争、显式 sleep、调度器抢占等。分析这些原因及其性能影响通常要动用一堆工具，而 **off-CPU 分析是一套能统一分析它们的方法，且可以只靠一个 off-CPU profiling 工具支撑**。

三种采集方式：

1. **采样**：定时采集 off-CPU 线程的样本，或者干脆采所有线程（wallclock sampling）。
2. **调度器追踪**：给内核 CPU 调度器打桩，测出线程 off-CPU 的时长，并连同 off-CPU 栈一起记录。**为什么它比采样便宜**：

> Scheduler tracing : Instrumenting the kernel CPU scheduler to time the duration that threads are off-CPU, and recording these times with the off-CPU stack trace. Stack traces do not change when a thread is off-CPU (because it is not running to change it), so the stack trace only needs to be read once for each blocking event.

3. **应用内建插桩**：有些应用对常用阻塞代码路径（典型是磁盘 I/O）有内建插桩，**可以带上应用自己的上下文**。方便好用，但**通常是看不见 off-CPU 事件的**（调度器抢占、缺页等）。

**前两种更可取**，因为它们**对所有应用都有效、能看见所有 off-CPU 事件**；**但开销很大**。原文给了量级：49 Hz 采样在 8 CPU 系统上开销可以忽略，**但 off-CPU 采样要采的是线程池而不是 CPU 池**——同一台机器可能有 **10,000 个线程**且大多空闲，于是采样开销涨 **1000 倍**（原文脚注还补了一句：这可能还更高，因为要给 off-CPU 线程采栈，而它们的栈不太可能还在 CPU 缓存里；限制到单个应用能减少线程数，但 profile 会不完整）。**调度器追踪**同样可能很贵——同一台机器每秒可能有 **100,000 个以上调度事件**。

原文的优化手段：**只记录超过极短时长的 off-CPU 事件**（减少样本量）；**用 BPF 在内核上下文里聚合栈**，只把样本送到用户态，进一步降低开销。⚠️ 原文的告诫：**在生产环境用 off-CPU profiling 要小心，先在测试环境评估开销。**

### Off-CPU 时间火焰图

解读方式与 CPU 火焰图一致：**先找最宽的塔**。原文的例子（图 5.4）：30 秒全系统 off-CPU profile 里，**绝大部分 off-CPU 时间在 `fsync()` 代码路径与 ext4 文件系统上**；缩放到处理命令（查询）的 MySQL 线程后，指针停在 `Prepared_statement::execute()` 上——底部信息行显示该函数 off-CPU 时间合计 **3.97 秒**。

⚠️ **hot/cold 合并图行不通**：原文说合并成一张图效果不好——**CPU 时间会被挤成一根细柱**，因为**off-CPU 的线程数可能比正在 CPU 上跑的线程数多两个数量级**，合并图里 99% 是等待时间。所以原文的做法是**把两张图分开出**。

### 等待时间是主要的噪声源

不缩放时，整张图大多是 `pthread_cond_wait()` 与 `futex()` 路径——**这些是线程在等活干**（图 5.5 的完整视图里能看到 `srv_worker_thread()`、`srv_purge_coordinator_thread()`、`srv_monitor_thread()` 等线程函数）。

**两个把信号找出来的办法**：

1. **缩放（或过滤）到应用请求处理函数**——我们最关心的是「处理一个应用请求期间」的 off-CPU 时间；MySQL 的对应函数是 `do_command()`。**有效，但你得知道该搜哪个函数。**
2. **采集时用内核过滤**排除无关的线程状态。原文提醒：有效性**取决于内核**；Linux 上匹配 **`TASK_UNINTERRUPTIBLE`** 能聚焦很多有意思的 off-CPU 事件，**但也会排除掉一些**。

**再往下的通用技术**：你会发现有些应用阻塞路径其实在等**别的东西**（比如一把锁）。要继续深挖，就得知道**持锁者为什么那么久才释放**。除了 §5.4.6 的锁分析，通用做法是**给唤醒（waker）事件插桩**——原文称之为高级活动，指向《BPF Performance Tools》第 14 章以及 BCC 的 `wakeuptime(8)`、`offwaketime(8)`。

## §5.4.3 系统调用分析

**机制**：给系统调用打桩，用来研究**基于资源的性能问题**。目的是找出**系统调用的时间花在哪**、**是哪种类型的调用**、以及**为什么被调用**。

三个目标：

| 目标 | 做法 | 对应工具 |
|---|---|---|
| **新进程追踪** | 追 `execve(2)`，记录新进程执行，**分析短命进程问题** | `execsnoop`（§5.5.5） |
| **I/O 剖析** | 追 `read(2)/write(2)/send(2)/recv(2)` 及其变体，研究它们的 **I/O 尺寸、标志位、代码路径**——用来发现**大量小 I/O** 这类次优问题 | `bpftrace`（§5.5.7） |
| **内核时间分析** | 系统显示很高的 `%sys` 时，用系统调用定位原因 | `syscount`（§5.5.6） |

⚠️ 原文明确：**系统调用解释了大部分但不全部内核 CPU 时间**——**例外包括缺页（page faults）、异步内核线程、中断**。

**为什么系统调用是个好事件源**：它们是**文档齐全的 API**（man page），而且**与应用同步调用**——所以从系统调用上采集的**栈会显示对应的应用代码路径**，而且**这样的栈可以画成火焰图**。

## §5.4.4 USE 方法用在软件资源上

**USE = 利用率（Utilization）/ 饱和度（Saturation）/ 错误（Errors）**，第 2 章引入，后续各章反复使用。

**原文的关键扩展**：USE 方法**也可以用在软件资源上**——如果你能拿到一份说明应用内部组件的**功能图**，就为每个软件资源考虑这三个指标。

**原文的例子一——线程池 + 请求队列**：

| 指标 | 定义 |
|---|---|
| Utilization | 区间内**忙着的处理线程数**占线程总数的百分比（例：50% 表示平均有一半线程在处理请求） |
| Saturation | 区间内**请求队列的平均长度**——显示有多少请求积压着等一个工作线程 |
| Errors | **因任何原因被拒绝或失败的请求** |

**原文的例子二——文件描述符**（系统可能对它设了上限，所以它是有限资源）：

| 指标 | 定义 |
|---|---|
| Utilization | **在用 fd 数**占上限的百分比 |
| Saturation | **取决于 OS 行为**：如果线程会阻塞等 fd，那就是**等这个资源的阻塞线程数** |
| Errors | 分配错误，例如 `EFILE`、"Too many open files" |

**流程**：对应用的各个组件**重复这个练习，不成立的指标直接跳过**。原文说这个过程本身就能帮你**沉淀出一份应用健康检查清单**，然后再进入其他方法论。**像上面这个队列系统还可以用排队论来研究**（见第 2 章）。

**判据**：USE 方法用在软件上时，**饱和度是三项里最容易被忽略、也最有价值的一项**——它对应「排队」这个在硬件视角里没有对应物的现象。

## §5.4.5 线程状态分析：九状态模型

**机制**：把每个应用的**线程时间**划分成若干有意义的**状态**，就能**在高层定位线程时间花在哪**——一部分问题当场解决，另一部分被指到具体方向。原文说这是他**处理每一个性能问题时用的第一套方法**，但它**在 Linux 上是高级活动**。

**最低要求是两种状态**：on-CPU 与 off-CPU。on-CPU 用标准指标与工具（如 `top(1)`）识别，然后按情况接 CPU profiling 或 off-CPU 分析（§5.4.1 / §5.4.2）。**状态越多，这套方法越有效。**

**原文给的九状态**：

| 状态 | 含义 |
|---|---|
| User | 用户态在 CPU 上 |
| Kernel | 内核态在 CPU 上 |
| Runnable | 已离开 CPU，**等着轮到上 CPU** |
| Swapping（匿名分页） | 可运行，但**被匿名页换入阻塞** |
| Disk I/O | 等块设备 I/O：读写、数据/文本页换入 |
| Net I/O | 等网络设备 I/O：socket 读写 |
| Sleeping | **主动**睡眠 |
| Lock | 等着获取某把同步锁（**在等别人**） |
| Idle | 等活干 |

**核心判据**：

> Performance for an application request is improved by reducing the time in every state except idle. Other things being equal, this would mean that application requests have lower latency, and the application can handle more load.

**各状态往下怎么查**（原文逐条给）：

- **User / Kernel** → 做 profiling 确定是哪些代码路径在吃 CPU，**包括花在自旋锁上的时间**（§5.4.1）。
- **Runnable** → 时间停在这里说明**应用想要更多 CPU**：查全系统 CPU 负载，以及应用自身的 CPU 限额（资源控制）。
- **Swapping** → 主存不够导致换页延迟：查全系统内存用量与限额（第 7 章）。
- **Disk** → 含直接磁盘 I/O 与缺页：查 §5.4.3、第 8 章文件系统、第 9 章磁盘；**工作负载表征**能解决很多磁盘问题——看**文件名、I/O 尺寸、I/O 类型**。
- **Network** → 网络 I/O（send/receive）阻塞的时间，**但不含等待新连接**（那是 idle）：查 §5.4.3、§5.5.7 的 I/O Profiling、第 10 章；同样可以用工作负载表征——**主机名、协议、吞吐**。
- **Sleeping** → 查**睡眠的原因（代码路径）与时长**。
- **Lock** → 找出**锁、当前持锁的线程、以及持锁者为什么持了这么久**——持锁者**可能又卡在另一把锁上**，需要**继续向上拆**。原文说这是**高级活动，通常由熟悉应用及其锁层次结构的开发者做**；他为此开发了 BCC 的 `offwaketime(8)`，**同时给出阻塞栈与唤醒者**。

⚠️ **一个必须知道的陷阱**：因为应用**通常在等活干**，所以**「网络 I/O」和「锁」两个状态里的时间往往其实是 idle**——一个 worker 线程可能用「等下一个请求（HTTP keep-alive）」或「等条件变量唤醒」来实现空闲。

### Linux 上怎么量

**内核线程状态 → 九状态的映射**（图 5.7）：内核的 `task_struct` 状态成员里，**Runnable = `TASK_RUNNING`**、**Disk = `TASK_UNINTERRUPTIBLE`**、**Sleep = `TASK_INTERRUPTIBLE`**；`ps(1)` 与 `top(1)` 用**单字母** **R / D / S** 显示。（还有更多状态，比如被调试器暂停，原文没列。）

**但这远不等于九状态**：还需要更多信息——比如 Runnable 要用 `/proc` 或 `getrusage(2)` 拆成 user 与 kernel 时间。

**原文说其他内核通常提供更多状态，这套方法更好用**；他最初是在 **Solaris** 上发展并使用这套方法的，灵感来自 Solaris 的 **microstate accounting**——它把线程时间记成八个状态：user、system、trap、text fault、data fault、lock、sleep、run queue（调度延迟）。原文说这些状态**与他的理想状态并不吻合，但是更好的起点**。

Linux 上原文用三种做法：

**一、线索式（clue-based）**——先用现成 OS 工具提示线程时间可能花在哪：

| 状态 | 工具与列 |
|---|---|
| User | `pidstat(1)` 的 `%usr`（**直接测量**） |
| Kernel | `pidstat(1)` 的 `%system`（**直接测量**） |
| Runnable | `vmstat(8)` 的 `r`（系统级） |
| Swapping | `vmstat(8)` 的 `si` 与 `so`（系统级） |
| Disk I/O | `pidstat(1) -d` 的 `iodelay`（**含换页状态**） |
| Network I/O | `sar(1) -n DEV` 的 `rxkB/s` 与 `txkB/s`（系统级） |
| Sleeping | **不好拿到** |
| Lock | `perf(1) top`（**可能直接看出自旋锁时间**） |
| Idle | **不好拿到** |

⚠️ 原文提醒：**这些统计里有的是系统级的**——比如 `vmstat(8)` 显示系统级有换页，**不代表你的应用正受影响**，要用更深的工具再确认一遍。

**二、Off-CPU 分析**：九状态里除 User 与 Kernel 外**全是 off-CPU**，直接套 §5.4.2。

**三、直接测量**：

| 状态 | 直接测量的方法 |
|---|---|
| User | 用户态 CPU 时间在多个工具里都有，也在 `/proc/PID/stat` 与 `getrusage(2)` 里；`pidstat(1)` 报为 `%usr` |
| Kernel | 内核态 CPU 时间同样在 `/proc/PID/stat` 与 `getrusage(2)` 里；`pidstat(1)` 报为 `%system` |
| Runnable | 内核 **schedstats** 特性以纳秒记录，经 `/proc/PID/schedstat` 暴露；也可以用 tracing 工具测（有开销），如 `perf(1) sched` 子命令与 BCC 的 `runqlat(8)` |
| Swapping | **delay accounting**（第 4 章 §4.3.3，含示例工具 `getdelays.c`）以纳秒测匿名分页换入时间；tracing 工具也能插桩 |
| Disk | `pidstat(1) -d` 的 `iodelay`（进程被块 I/O 与换页延迟的时钟节拍数）——**若系统级没有换页（由 `vmstat(8)` 判定），那么这些 iodelay 可归为 I/O 状态**；delay accounting 等会计特性也提供块 I/O 时间（`iotop(8)` 用的就是这个）；还可以用 BCC 的 `biotop(8)` |
| Network | 用 BCC 与 bpftrace 等 tracing 工具，如 TCP 的 `tcptop(8)`；应用也可能自带 I/O 计时插桩 |
| Sleeping | 查**进入主动睡眠**的时间，用 tracer 与事件，比如 `syscalls:sys_enter_nanosleep` tracepoint；原文的 `naptime.bt` 会打印 PID 与时长 |
| Lock | 用 tracing 工具：BCC 的 `klockstat(8)`；以及 bpf-perf-tools-book 的 `pmlock.bt` / `pmheld.bt`（pthread 互斥锁）、`mlock.bt` / `mheld.bt`（内核互斥锁） |
| Idle | 用 tracing 工具给「等活干」的应用代码路径插桩 |

**应用看起来「完全睡着」时**（阻塞 off-CPU 且没有 I/O 速率或其他事件）：可能得用 `pstack(1)` 或 `gdb(1)` 看线程栈，或直接读 `/proc/PID/stack`。⚠️ 原文警告：**这类调试器会暂停目标应用，本身可能造成性能问题——上生产前先搞清楚用法与风险。**

## §5.4.6 锁分析

**机制**：多线程应用的锁会成为瓶颈，**抑制并行性与可扩展性**；**单线程应用也会被内核锁拖住**（例如文件系统锁）。锁分析分两件事：

1. **检查竞争（contention）**——判断**现在**有没有问题；
2. **检查过长的持有时间（hold time）**——现在未必是问题，但**并行负载上来之后可能会变成问题**。

每一项都要尽量找出**锁的名字**（如果它有名字）和**走到这把锁的代码路径**。

**一个实用捷径**：虽然有专门的锁分析工具，但**有时只靠 CPU profiling 就能解决**——**自旋锁**的竞争表现为 CPU 占用，用栈回溯的 CPU profile 很容易认出来；**自适应互斥锁**的竞争通常也伴随一些自旋，同样能从 CPU profile 的栈里认出来。但这时要清醒：**CPU profile 只给了一半的故事**，因为线程可能**已经阻塞睡去**而不是在自旋（§5.4.1）。

**判据**：先分型——**自旋型竞争**用 CPU profile 就够；**阻塞型竞争**必须上 §5.4.5 的 Lock 状态与 §5.5.7 的锁追踪。Linux 上的具体锁分析工具见 §5.5.7。

## §5.4.7 静态性能调优清单

静态调优看的是**被配置好的环境**。原文给的应用层清单（逐条问）：

- 运行的**应用版本**是什么，**依赖**是什么？有新版吗？**release notes 里提到性能改进吗？**
- 有**已知的性能问题**吗？有没有列出它们的 **bug 库**？
- 应用**是怎么配的**？
- 如果它**偏离了默认值**，**为什么**？——**是基于测量与分析，还是拍脑袋猜的？**
- 应用**用了对象缓存吗**？**缓存多大**？
- 应用**跑并发吗**？**并发怎么配的**（比如**线程池大小**）？
- 应用**跑在特殊模式里吗**？（比如**开了 debug 模式正在拖性能**；或者跑的是 **debug build 而不是 release build**）
- 应用**用了哪些系统库**？**什么版本**？
- 应用**用什么内存分配器**？
- 应用**配了用大页（large pages）来管理堆吗**？
- 应用**编译过吗**？**编译器什么版本**？**什么编译选项与优化**？**64 位吗**？
- 原生代码**用了高级指令吗**？（应该用吗？）——比如 **SIMD/向量指令，含 Intel SSE**
- 应用**是不是遇到了错误、现在处于降级模式**？或者**配置错了、一直跑在降级模式**？
- 有没有**系统施加的限额或资源控制**（CPU、内存、文件系统、磁盘、网络）？——**云计算里这很常见**。

原文的结论：**回答这些问题本身就可能暴露出被忽略的配置选择。**

**判据**：这张清单回答的是「**还没测量就已经错了什么**」。清单里每一条都是**可以立刻核对**的（配置文件、版本号、启动参数、cgroup 设置），比 profiling 便宜得多——**先跑清单，再跑火焰图**。

## §5.4.8 分布式追踪

**机制**：分布式环境下，应用可能由**跑在不同系统上的多个服务**组成。每个服务都可以**当作自己的小应用**来研究，但**整体也必须研究**——这需要新的方法与工具，通常就是**分布式追踪**。

**做法**：在**每个服务请求**上记录信息，事后**合并**这些信息来研究。跨多个服务的请求可以被**拆成它的依赖请求**，从而**定位到哪个服务**造成了高应用延迟或错误。

**记录的信息**包括：请求的**唯一标识**（external request ID）、它在**依赖层次中的位置**、**起止时间**、**错误状态**。

**挑战是日志量**：每个应用请求会产生多条记录。原文给的解法：

- **head-based sampling（头采样）**：在请求的**开头**就决定要不要采（例：**每 10000 个请求采 1 个**）。原文的评价：**这足够分析大部分请求的性能**，但**因为数据有限，分析间歇性错误或离群点会很困难**。
- **tail-based sampling（尾采样）**：**所有事件先全部捕获**，再决定保留哪些（可能依据**延迟与错误**）。

**判据**：要查**偶发**问题 → tail-based（数据全，但成本高）；只关心**整体分布** → head-based（便宜）。

**一旦定位到问题服务**，就可以用其他方法与工具（§5.4.1–§5.4.7）深入分析它。

## §5.5.1 perf：Linux 的标准多面手

`perf(1)` 是 Linux 的标准 profiler，一个工具干很多事（完整用法在第 13 章）。本章用它做三件事。

### CPU profiling

```bash
# perf record -F 49 -a -g -- sleep 30
# perf script
```

`-F 49` = 每秒 49 个样本，`-a` = 所有 CPU，`-g` = 采调用栈。`perf script` 打印之前录制的 `perf.data` 里的每个栈样本；`perf report` 按**代码路径层次**汇总；也可以直接画成 CPU 火焰图：

```bash
# perf record -F 49 -a -g -- sleep 10; perf script --header > out.stacks
# git clone https://github.com/brendangregg/FlameGraph; cd FlameGraph
# ./stackcollapse-perf.pl < ../out.stacks | ./flamegraph.pl --hash > out.svg
```

`out.svg` 可以在浏览器里打开。`flamegraph.pl` 为不同语言提供**配色方案**（如 Java 用 `--color=java`）。

### 系统调用追踪

`perf trace` **默认就追系统调用**，是 `perf(1)` 版的 `strace(1)`：

```bash
# perf trace -p $(pgrep mysqld)
```

**与 strace 的对比**：

> The advantage of perf(1) is that it uses per-CPU buffers to reduce the overhead, making it much safer to use than the current implementation of strace(1). It can also trace system-wide, whereas strace(1) is limited to a set of processes (typically a single process), and it can trace events other than syscalls. perf(1), however, does not have as many syscall argument translations as strace(1); here is a single line from strace(1) for comparison:

即：**perf 用 per-CPU 缓冲，开销低得多**；**能全系统追踪**（strace 只能追一组进程，通常是一个）；**能追非系统调用的事件**。**代价是参数翻译不如 strace 丰富**——strace 会把 `utime` 这种结构体展开。原文说 `perf trace` 的 BPF 版参数「美化」正在做，目标终局是**让 `perf trace` 成为 strace 的可替换实现**。

### 内核时间分析：先看汇总，再看细节

```bash
# perf trace -s -p $(pgrep mysqld)
```

输出每个系统调用的 **calls / total / min / avg / max / stddev**。原文的顺序建议很重要：**直接跑 `perf trace` 在繁忙应用上会「输出雪崩」**，所以**先看汇总**，再用过滤器只看关心的系统调用类型。

### I/O 剖析

```bash
# perf trace -e sendto -p $(pgrep mysqld)
```

原文读到的输出是：**两次 12664 字节的 send，接着两次 11 字节的 send**，都带 `DONTWAIT` 标志。原文的读法：**如果看到大量小 send**，就该怀疑**能不能合并**，或者**能不能去掉 `DONTWAIT` 标志**。

⚠️ **原文的抱怨**：`perf trace` 只显示**文件描述符号（37）和 socket 号（833323）**，**看不到 socket 类型、IP 地址、端口**——要这些就得换 bpftrace（§5.5.7）。

## §5.5.2–5.5.6 profile / offcputime / strace / execsnoop / syscount

### profile(8)：内核态聚合的采样 profiler

BCC 工具（源自 2016-07-15，基于 Sasha Goldshtein、Andrew Birchall、Evgeny Vereshchagin、Teng Qin 的代码）。**用 BPF 在内核上下文里聚合栈**，**只把唯一栈及其计数传给用户态**，因此开销低：

```bash
# profile -F 49 10
```

输出是**唯一栈 + 采样次数**（如 `SELECT_LEX::prepare()` 被采到 13 次）。

### offcputime(8)：off-CPU 分析的主力工具

BCC 与 bpftrace 工具，统计**线程阻塞在 off-CPU 的时间**并给出解释原因的栈；**它是 profile(8) 的对偶——两者合起来覆盖线程在系统上的全部时间**。

```bash
# offcputime 5
```

输出是**唯一栈 + off-CPU 时间（微秒）**。原文的例子：那一条栈显示经 `MYSQL_BIN_LOG::sync_binlog_file()` 路径的 ext4 文件系统同步操作，**本次追踪合计 352 毫秒**。

**两个降开销的机制**：

> For efficiency, offcputime(8) aggregates these stacks in kernel context, and emits only unique stacks to user space. It also only records stack traces for off-CPU durations that exceed a threshold, one microsecond by default, which can be tuned using the -m option.

**`-M` 选项设最长记录时长**——用来**滤掉没意思的长栈**（原文的例子：等活干、每秒循环阻塞好几秒的线程），例如 `offcputime -M 900000` 排除超过 900 ms 的。

画成 off-CPU 时间火焰图：

```bash
# git clone https://github.com/brendangregg/FlameGraph; cd FlameGraph
# offcputime -f 5 | ./flamegraph.pl --bgcolors=blue \
    --title="Off-CPU Time Flame Graph"> out.svg
```

原文用**蓝色背景**作为「这是 off-CPU 火焰图、不是常见的 CPU 火焰图」的视觉提醒。

### strace(1)：经典，但昂贵

```bash
$ strace -ttt -T -p 1884
$ strace -c dd if=/dev/zero of=/dev/null bs=1k count=5000k
```

常用选项：`-ttt` 打印**以 epoch 为起点、微秒精度**的时间戳列；`-T` 打印每次系统调用的**耗时**（最后一列 `<time>`）；`-p PID` 追这个进程（也可以给一个命令，让 strace 启动并追它）；另有 `-f` 跟随子线程、`-o filename` 输出到文件。**把系统调用参数翻译成人可读的形式**是它最有用的特性——尤其是理解 `ioctl(2)` 调用时。`-c` 选项给出汇总表（time 百分比、seconds 总系统 CPU 时间、usecs/call 平均每次、calls 次数、syscall 名称）。

⚠️ **开销警告**：

> WARNING: The current version of strace(1) employs breakpoint-based tracing via the Linux ptrace(2) interface. This sets breakpoints for the entry and return of all syscalls (even if the -e option is used to select only some). This is invasive, and applications with high syscall rates may find their performance worsened by an order of magnitude. To illustrate this, here is the same dd(1) command without strace(1):

注意 **`-e` 只选部分系统调用也不减少断点**——**所有系统调用的入口与返回**都被设了断点，所以是**侵入式**的。

原文用同一个 `dd` 命令做对照：**带 strace 是 `140.722 s, 37.3 MB/s`，不带 strace 是 `1.91247 s, 2.7 GB/s`**，据此得出 **strace 让 dd 慢了 73 倍**——原文说这是一个**特别严重**的例子，因为 dd 的系统调用率极高。

**其他 tracer 的低开销做法**：`perf(1)`、Ftrace、BCC、bpftrace 都用**缓冲追踪**——事件写进**共享内核 ring buffer**，用户态 tracer 周期性读走缓冲区，**减少了用户态与内核态之间的上下文切换**，从而降低开销。

**判据**：按应用要求，这种追踪**短时间用**来搞清「在调哪些系统调用」也许可以接受；如果开销不可接受，就换 `perf trace` 或 BPF 系工具。

### execsnoop(8)：新进程追踪

BCC 与 bpftrace 工具，**全系统追踪新进程的执行**。它能找出**消耗 CPU 的短命进程**问题，也能用来调试软件执行（**包括应用启动脚本**）。

```bash
# execsnoop
```

输出列是 **PCOMM / PID / PPID / RET / ARGS**。原文讲了一个真实案例：他在自己的数据库系统上跑 execsnoop，**发现一个读写压测还在循环跑**——**已经跑了几天**；而**其他系统指标（CPU 与磁盘负载）看不出异常**，因为数据库同时在跑另一个负载。后面几行则是 `sar(1)` 在采集系统指标。

**机制**：execsnoop 靠**追 `execve(2)`** 工作，每条打一行摘要；支持 `-t` 时间戳等选项。原文还提到 `threadsnoop(8)`（bpftrace 版），用于追 `pthread_create()` 创建线程。

### syscount(8)：系统调用计数

```bash
# syscount
# syscount -P
```

前者按**系统调用名**统计（原文示例：`recvfrom` 114746、`sendto` 57395、`ppoll` 28654、`futex` 953、`io_getevents` 55、`epoll_wait` 11……）；后者（`-P`）按**进程**统计（原文示例：`mysqld` 155463、`oltp_read_only.` 61779、`sshd` 36……）。

**用法**：syscount 告诉你**哪类调用最多**；再往下要挖**参数、延迟、调用栈**，就用 `perf(1) trace` 配 `-e recvfrom` 过滤，或用 bpftrace 插桩 `syscalls:sys_enter_recvfrom`。

**判据**：**syscount 是最便宜的第一眼**——它能在一秒内告诉你「这机器上最热的是哪类系统调用」，然后再决定用哪个工具深挖。

## §5.5.7 bpftrace：按线索定制

bpftrace 是基于 BPF 的追踪器，提供**高层编程语言**，能写出强大的**一行命令**与短脚本，**特别适合根据其他工具给出的线索做定制分析**。

### 信号追踪

```bash
# bpftrace -e 't:syscalls:sys_enter_kill { time("%H:%M:%S ");
    printf("%s (PID %d) send a SIG %d to PID %d\n",
    comm, pid, args->sig, args->pid); }'
```

输出显示信号 2（Ctrl-C）由 bash 发给自己，后面是 systemd-journal 发信号 0 给其他 PID。原文指出：**信号 0 什么都不做**——它通常用来**通过系统调用的返回值检查某个进程是否还存在**。这个一行命令对调试**异常提前退出**很有用；时间戳可以与监控软件里的性能问题交叉核对。独立工具是 BCC / bpftrace 的 `killsnoop(8)`。

### I/O 剖析

三个递进的一行命令（原文 §5.5.7）：

```bash
# bpftrace -e 't:syscalls:sys_enter_recvfrom { @bytes = hist(args->size); }'
# bpftrace -e 't:syscalls:sys_exit_recvfrom { @bytes = hist(args->ret); }'
# bpftrace -e 't:syscalls:sys_enter_recvfrom { @ts[tid] = nsecs; }
    t:syscalls:sys_exit_recvfrom /@ts[tid]/ {
    @usecs = hist((nsecs - @ts[tid]) / 1000); delete(@ts[tid]); }'
```

- **第一行**：`recvfrom(2)` **请求的缓冲区尺寸**直方图。原文的读数：约一半的尺寸**极小（4–7 字节）**，最大的一批在 **16–32 Kbyte** 区间。
- **第二行**：**实际收到的字节数**直方图（退出 tracepoint）。⚠️ 原文提醒：退出这一行**会把系统调用错误也算进直方图，表现为尺寸 -1**。**两个直方图对不上**，就说明**应用在申请比需要更大的缓冲**。
- **第三行**：同时追 enter 与 exit，得到**延迟直方图**。原文的读数：`recvfrom(2)` **多数时候不到 8 微秒**，另有 **32–256 微秒**的较慢模式，并**存在延迟离群点**（最慢到 **8–16 毫秒**量级）。

**再往下钻**：把输出映射声明改成 `@usecs[args->ret]`（按返回值分组——返回值是收到的字节数或 -1，这样能**确认是不是更大的 I/O 导致了更高延迟**）；改成 `@usecs[ustack]`（按用户栈分组，每条代码路径一张延迟直方图）；还可以加 `/comm == "mysqld"/` 之类的**过滤**（只看我关心的进程），或加**只挑延迟离群点与慢模式的过滤**。

### 锁追踪

对典型的 pthread 互斥锁：**用 uprobes 追 pthread 库函数**（`pthread_mutex_lock()` 等），**用 tracepoints 追管理阻塞的 `futex(2)`**。

```bash
# pmlock.bt $(pgrep mysqld)
# pmheld.bt $(pgrep mysqld)
```

`pmlock.bt` 追**获取锁的时延**；`pmheld.bt` 追**持有锁的时长**（从加锁到解锁）——**两个合起来回答「这把锁为什么这么久」**。

原文的例子：锁地址 `0x7f37280019f0` 经 `THD::set_query()` 代码路径获取，**获取时延常在 8–16 微秒区间**；而 `pmheld.bt` 显示的**持有路径不同**（`__pthread_mutex_unlock` ← `THD::set_query()`）。**锁有符号名时打印符号名，没有时从栈里认锁**——例子里的结论是「这是 `THD::set_query()` 里偏移 147 处的一把锁」，查源码发现该函数**只获取一把锁：`LOCK_thd_query`**。

⚠️ **开销与替代方案**：原文明确**锁追踪会增加开销**，而且**锁事件可能非常频繁**（见第 4 章 §4.3.7 uprobes）。可以用内核 `futex` 函数的 kprobes 写类似工具，**开销能低一些**。**开销几乎为零的替代方案是改用 CPU profiling**——CPU profiling 的开销**由采样率限定**，而**重度锁竞争会烧掉足够多的 CPU 周期，在 CPU profile 里直接显形**。

### 应用内部

需要时可以**自己开发工具来汇总应用内部状态**。原文给的顺序：**先检查有没有 USDT 探针可用（或能否通过重新编译打开）**；不行或不够，再考虑 **uprobes**；**JIT 编译的软件可能需要 dynamic USDT**（原文第 4 章 §4.3.8 有述），因为 **uprobes 可能插不进去**。

**Java 的分层例子**（原文给的三层）：**uprobes 插 JVM 运行时（C++ 代码）与 OS 库**；**USDT 插高层 JVM 事件**；**dynamic USDT 插进 Java 代码**，以提供**方法执行**层面的洞察。

## §5.6.1 符号缺失：ELF 与 JIT

**症状**：profiler 或 tracer **无法把应用指令地址解析成函数名（符号）**时，会打印十六进制数或字符串 `[unknown]`。修法取决于**应用的编译器、运行时、调优方式，以及 profiler 本身**。

**ELF 二进制（C/C++ 等）**：符号可能缺失，**尤其是打包分发的那些**——它们被 `strip(1)` 处理过以减小体积。两种修法：**调整构建流程以避免 strip**；或**换用别的符号来源**，比如 **debuginfo** 或 **BPF Type Format（BTF）**。Linux 上通过 `perf(1)`、BCC、bpftrace 的 profiling **都支持 debuginfo 符号**。

**JIT 运行时（Java、Node.js 等）**：符号缺失**很常见**——**JIT 编译器有自己的符号表，它在运行时不断变化，不属于二进制里预编译的符号表**。通用修法是**使用运行时生成的补充符号表**，它们放在 **`/tmp/perf-<PID>.map`** 文件里，**`perf(1)` 与 BCC 都会读**。

原文给的组合拳：**Netflix 用 perf-map-agent**（可以附加到一个活的 Java 进程上导出补充符号文件），Gregg 用 **jmaps** 把这一步自动化，并强调**必须在 profile 之后、符号翻译之前立刻运行**：

```bash
# perf record -F 49 -a -g -- sleep 10; jmaps
# perf script --header > out.stacks
```

```bash
# bpftrace --unsafe -e 'profile:hz:49 { @[ustack] = count(); }
    interval:s:10 { exit(); } END { system("jmaps"); }'
```

**符号漂移（symbol churn）**：

> A symbol mapping may change between the profile sample and the symbol table dump, producing invalid function names in the profile. This is called symbol churn , and running jmaps immediately after perf record reduces it. It has so far not been a serious problem; if it was, a symbol dump could be taken before and after the profile to look for changes.

**其他解法**：**符号时间戳日志**（`perf(1)` 支持，**能解决 churn**，代价是**开启时开销更高**）；或让 **`perf(1)` 调用运行时自己的栈回溯器**（通常是为异常栈存在的）——这被称为 **stack helpers**，Java 上的实现是 **async-profiler**。

⚠️ **JIT 运行时也有预编译组件**：JVM 还会用 `libjvm` 与 libc——这部分回到 ELF 那一节处理。

## §5.6.2 栈缺失与帧指针

**症状**：栈回溯**缺失或不完整**，有时短到只有一两帧。原文给的例子（MySQL 的 off-CPU profile）：

```
    finish_task_switch
    schedule
    futex_wait_queue_me
    futex_wait
    do_futex
    __x64_sys_futex
    do_syscall_64
    entry_SYSCALL_64_after_hwframe
    pthread_cond_timedwait@@GLIBC_2.3.2
    [unknown]
```

这个栈是不完整的：**`pthread_cond_timedwait()` 之后只剩一个 `[unknown]`**——**缺的正是最需要理解的 MySQL 函数**。有时栈**只剩一帧**：

```
    send
```

在火焰图里这会表现为**「草」**：profile 底部一堆细的单帧。

**根因是两个因素的叠加**：

> Incomplete stack traces are unfortunately common, and are usually caused by a confluence of two factors: 1) the observability tool using a frame pointer-based approach for reading the stack trace, and 2) the target binary not reserving a register (RBP on x86_64) for the frame pointer, instead reusing it as a general-purpose register as a compiler performance optimization. The observability tool reads this register expecting it to be a frame pointer, but in fact it could now contain anything: numbers, object address, pointers to strings, etc. The observability tool tries to resolve this number in the symbol table and, if it is lucky, it doesn’t find it and can print “[unknown]”. If it is unlucky, that random number resolves to an unrelated symbol, and now the printed stack trace has a function name that is wrong, confusing you, the end user.

**关键在最后那句**：**运气好 → 打印 `[unknown]`（你知道有问题）；运气不好 → 解析到一个无关符号，于是栈里出现一个错误的函数名，把分析者带沟里。**

**libc 库通常是**不带帧指针编译的，所以**任何经过 libc 的路径都会断栈**——原文举的两个例子（`pthread_cond_timedwait()` 与 `send()`）都在 libc 里。

**修法一：把帧指针寄存器修回来**

- **C/C++ 及其它用 gcc(1) 或 LLVM 编译的软件**：`Recompile the software with -fno-omit-frame-pointer .`
- **Java**：用 `java -XX:+PreserveFramePointer` 启动。

原文的代价评估：**这可能带来性能成本，但实测通常低于 1%**；而**能用栈回溯找到性能问题的收益，通常远大于这个成本**。

**修法二：换成不依赖帧指针的栈回溯技术**——`perf(1)` 支持 **DWARF-based stack walking、ORC、last branch record（LBR）**。原文给的现状约束：**DWARF 与 LBR 尚不能从 BPF 使用；ORC 还不能用于用户态软件。**

**判据**：**栈里的函数名可能是错的**——看到 `[unknown]` 不要只当「符号丢了」，要知道它同时意味着「**这个栈不可信**」，必须先修帧指针或换栈回溯技术，再看分析结论。

## 章末选型判据

| 场景 | 用什么 | 别用什么 |
|---|---|---|
| 接手一个没分析过的应用 | 先写下**目标**（延迟 / 吞吐 / 利用率 / 价格）+ 找**限制器**，再跑 §5.4.7 **静态清单** | 直接抓 CPU 火焰图（没有目标就没有「该看哪里」） |
| 选应用 / 选语言 / 选运行时 | 按**可观测性**选（工具丰富、能定位到函数） | 只看基准分高 10% 的黑盒 |
| 报告「应用很慢」 | 拆成两张图：CPU 火焰图 + off-CPU 火焰图，**分开出** | 合成 hot/cold 一张图（CPU 时间会被挤成细柱，99% 是等待） |
| 火焰图上看到不认识的函数名 | 火焰图负责**发现**；去**查**这个函数是什么（原文现场搜 `MYSQL_BIN_LOG::commit()`） | 把火焰图当源码（分析师不必认识应用内部） |
| 采 CPU profile | `perf(1)` / `profile(8)` 这类**内核态** profiler | 用户态 profiler（看不到内核 CPU，且对「被内核换下」无感） |
| 线程等不到 CPU | §5.4.5 的 **Runnable** 状态 → 查 CPU 负载与限额 | 直接加线程（会让 Runnable 更糟） |
| 锁竞争 | 先分型：**自旋型**用 CPU profile 就够；**阻塞型**上 Lock 状态 + `pmlock.bt`/`pmheld.bt` | 只看 CPU profile 就下结论（阻塞等待的那部分它看不到） |
| 锁没竞争但多核下就是慢 | 查**伪共享**：两把锁是不是同一条 cache line | 盲目换锁类型 / 加锁 |
| fd 数量大 | `epoll(2)` / `kqueue(2)`（O(1)） | `poll(2)` 的 O(n) 数组扫描 |
| 连接数上千、每次短读 | 非阻塞 I/O（`O_ASYNC` / `io_submit` / `sendfile` / `io_uring`） | 线程-per-connection（创建销毁 + 栈空间 + 上下文切换） |
| 调 I/O 尺寸 | 吞吐目标 → 调大；延迟目标 → 调到**贴近请求** | 无脑调到最大（浪费带宽、伤延迟、白占缓存） |
| 追系统调用 | 先 `syscount` 看**哪类最多**，再 `perf trace -e` 或 bpftrace 挖细节 | 长时间在生产上挂 `strace`（ptrace 断点，侵入式，可慢 73 倍） |
| 需要调用栈 | `perf(1)` / `profile(8)` / `offcputime(8)`（内核态聚合，开销低） | `strace(1)` |
| 栈里出现 `[unknown]` 或栈断掉 | ① 查是不是 `-fomit-frame-pointer`（ELF）或 JIT 符号缺失（`/tmp/perf-<PID>.map` + `jmaps`）② 修帧指针（`-fno-omit-frame-pointer` / `-XX:+PreserveFramePointer`） | 相信栈里的函数名——**帧指针缺失时那个名字可能是错的** |
| 分布式服务定位慢在哪 | 分布式追踪；查**偶发**问题用 **tail-based** 采样 | 只用 head-based 采样去查偶发错误（数据太少） |
| 生产上做 off-CPU profiling | 先在**测试环境评估开销**；用 BPF 内核聚合 + 过滤极短事件 | 直接在生产上长时间开着（线程池采样可能贵 1000 倍） |
| 用调试器看线程栈 | 先评估风险（**会暂停目标应用**），限制在测试环境 | 直接上生产抓「完全睡着」的应用 |


