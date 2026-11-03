---
title: Chapter 6. CPUs（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - CPU缓存
  - 缓存未命中
  - IPC与停滞周期
  - 假共享
  - 性能工程
source: "《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg，Addison-Wesley 2020，英文原版）第 6 章"
companion: "[[06-Chapter 6. CPUs]]"
---

# 第 6 章 讲义：缓存未命中是现代 CPU 的头号开销

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[06-Chapter 6. CPUs|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：`src/room/room_mgr.h` 的 `std::map<std::uint32_t, Room> rooms_` 是**红黑树**（离散链表结构，每个房间一次堆分配、节点分散）；而 `src/net/tcp_transport.h` 的 `conns_` 与 `src/session/session_mgr.h` 的 `sessions_` 已经是**连续数组**。这一章讲的就是这两者为什么差这么多。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原章。

---

## 0. 开局：CPU 满了，但你在等内存

先看一个数字。某台 4 GHz 的机器上，一次主存访问要花掉大约 **60 ns**：

```
60 ns × 4 GHz = 240 个时钟周期
```

而 L1 缓存命中只要**几个**周期，L2 也就**十几个**周期。也就是说：

| 事件 | 大致代价 |
|---|---|
| L1 缓存命中 | 几个周期 |
| L2 缓存命中 | 十几个周期 |
| **主存访问** | **约 240 个周期（4 GHz 下）** |

现在想象一个循环：它自己一个字都不改，只是每次去读一个 `Player` 结构的字段。如果这个 `Player` 结构在缓存里，一次 240 周期变成 5 周期，**这循环快了近 50 倍**。可如果这个结构大到**怎么放都放不进 L1**，那每次读都是主存访问 —— 于是你会看到一件很反直觉的事：

> CPU 使用率 95%，机器"满负荷"跑着，但 IPC 只有 0.2。

这就是本章的主题。**现代 CPU 的头号开销不是"算得慢"，是"等内存"。** 而"等"这件事在操作系统层面几乎看不见 —— 你在 `top` 里看到的是 CPU 被占满了，可这 95% 的时间里它其实**什么都没干**，只是在等。

---

## 1. §6.1 Terminology：先把名词钉死

这一节全是定义，但有几个词**必须精确**，因为后面全靠它们区分：

- **Processor（处理器）**：插在 socket 上的那块物理芯片。里面含一个或多个 **core**（核）或 **hardware thread**（硬件线程）。
- **Core（核）**：多核处理器上的一个独立 CPU 实例。靠核来横向扩展，叫 chip-level multiprocessing（CMP）。
- **Hardware thread（硬件线程）**：单个核上并行跑多个线程的架构（比如 Intel 的 Hyper-Threading）。每个硬件线程是一个独立的 CPU 实例。这种扩展叫 **SMT**（同时多线程）。
- **Logical CPU（逻辑 CPU）**：操作系统眼里的一个可调度 CPU 实体。它可能是一个硬件线程、一个核，或一颗单核处理器。
- **Scheduler（调度器）**：内核里负责把线程分配到 CPU 上的子系统。
- **Run queue（运行队列）**：等待被 CPU 服务的可运行线程队列。现代内核内部可能用别的结构（比如红黑树），但**术语仍叫运行队列**。

原文的一个例子：单颗四核八线程的处理器，**在操作系统看来就是 8 个 CPU**。

> Each hardware thread is addressable as a logical CPU , so this processor appears as eight CPUs.

（注意 epub 抽取保留了原书的空格排版 —— "CPU , so" 这种逗号前的空格是抽取产物，不是原文排版错误。）

最后补一句书末的提醒：术语表里还有 `CPU`、`CPU cycle`、`stack` 等基础词，本章也在各章之间互相引用（第 2、3 章的术语节）。

---

## 2. §6.2.1–6.2.3 Models：三个示意图

### §6.2.1 CPU Architecture

一张图交代物理架构（4 核 × 2 硬件线程 = 8 个逻辑 CPU），以及**操作系统怎么看它**。操作系统可能还知道拓扑信息 —— 哪些 CPU 在同一个核上、缓存是怎么共享的 —— 这些信息是为了**让调度器做出更好的决定**。

Linux 上有 `lstopo(1)` 能生成类似的图（见 §6.6.21）。

### §6.2.2 CPU Memory Caches

缓存的**尺寸越小、速度越快**，越靠近 CPU —— 这是个取舍。哪些缓存在片上（integrated）、哪些在片外，取决于处理器类型；早期处理器提供的片上缓存级数更少。

这一节只有三行字，但它是全章的**地图**：把"缓存"想象成从 CPU 往外的一层层越来越大的储藏室。

### §6.2.3 CPU Run Queues

运行队列由内核调度器管理。关键的一句在这儿：

> The number of software threads that are queued and ready to run is an important performance metric indicating CPU saturation.

——**队列里排队的线程数，就是饱和度指标**。空排队不等于饱和，队列长度本身就能说明问题。

多处理器系统上，内核通常给**每个 CPU 一条运行队列**，并尽量让线程留在自己那条队列上。原因很实在：

> These caches are described as having cache warmth , and this strategy to keep threads running on the same CPUs is called CPU affinity .

——**"缓存温度"（cache warmth）**：线程上次跑过的数据还热乎着。换核 = 缓存变冷 = 缓存未命中变多。**这条策略叫 CPU 亲和性（affinity）**。

NUMA 系统上，每 CPU 一条队列还额外改善**内存局部性**（第 7 章），并且避免了全局队列上 mutex 锁的争用 —— 全局队列会让锁成为扩展性瓶颈。

---

## 3. §6.3.1–6.3.3 / §6.3.7 Concepts：指令是怎么执行的，以及怎么量它

### §6.3.1 Clock Rate

时钟是驱动全部处理器逻辑的数字信号。每条指令可能要花**一个或多个时钟周期**。有些处理器能变频：操作系统可以要求它降频省电（内核 idle 线程就会这么干），处理器自己也能动态调整。

但时钟频率作为营销指标**具有误导性**：

> Even if the CPU in your system appears to be fully utilized (a bottleneck), a faster clock rate may not speed up performance—it depends on what those fast CPU cycles are actually doing. If they are mostly stall cycles while waiting on memory access, executing them more quickly doesn’t actually increase the CPU instruction rate or workload throughput.

——**如果这些快周期大部分是"停滞周期"（stall cycle，在等内存），把它们跑得更快并不会多完成一条指令。** 这就是本章开头那个 95% CPU / 0.2 IPC 的解释。

### §6.3.2 Instructions

一条指令要走五步，每一步由一个**功能单元**（functional unit）处理：

```
指令取指 → 指令解码 → 执行 → 内存访问 → 寄存器写回
```

后两步可选（很多指令只碰寄存器）。每步至少一个时钟周期，而**内存访问通常是最慢的一步**：

> Memory access is often the slowest, as it may take dozens of clock cycles to read or write to main memory, during which instruction execution has stalled (and these cycles while stalled are called stall cycles ).

——于是有了缓存。

### §6.3.3 Instruction Pipeline

流水线让不同指令的不同步骤**并行**执行，像工厂流水线。理想情况下每个周期完成一条指令。

分支预测是流水线的麻烦所在：

> With conditional branches, the processor does not know what the later instructions will be.

处理器会**猜**分支结果并开始执行猜的那条路：

> If the guess later proves to be wrong, the progress in the instruction pipeline must be discarded, hurting performance.

猜错 ⇒ 流水线里的进展全部作废。Linux 内核源码里的 `likely()` / `unlikely()` 宏就是给预测器塞的提示。

### §6.3.7 IPC, CPI

**每周期指令数（IPC）** 是理解 CPU 利用率性质的头号指标。它的倒数是 **CPI**（每指令周期数）。Linux 社区和 `perf(1)` 用 IPC，Intel 那边用 CPI。

> A low IPC indicates that CPUs are often stalled, typically for memory access. A high IPC indicates that CPUs are often not stalled and have a high instruction throughput.

这两句话直接给出了**排查方向**：低 IPC → 装更快的 DRAM、改善内存局部性、或**干脆减少内存 I/O 量**。而：

> Installing CPUs with a higher clock rate may not improve performance to the degree expected, as the CPUs may need to wait the same amount of time for memory I/O to complete. Put differently, a faster CPU may mean more stall cycles but the same rate of completed instructions per second.

——**换更快的 CPU 会给你更多停滞周期，但每秒完成的指令数一样。**

具体数值要靠实测。书里给了两个锚点：

> you may find that low-IPC workloads run with an IPC at 0.2 or lower, and high IPC workloads run with an IPC of over 1.0

> At Netflix, cloud workloads range from an IPC of 0.2 (considered slow) to 1.5 (considered good).

**一个必须记住的陷阱**：

> It should be noted that IPC shows the efficiency of instruction processing , but not of the instructions themselves.

——IPC 只衡量"指令处理得效率高不高"，**不衡量"这些指令值不值得做"**。你加了一个纯寄存器空转循环，IPC 会**变高**，CPU 使用率也会变高 —— 两个指标同时"变好"，但程序更慢了。

（推演）把 IPC 想成"汽车里程/升油"：里程高说明发动机效率好，但如果你只是为了让仪表好看而空踩油门，那这个指标就骗了你。

---

## 4. §6.3.8–6.3.13：利用率、饱和度，和它们为什么不一样

### §6.3.8 Utilization

CPU 利用率 = 这段时间里 CPU 实例**忙于干活**的百分比。忙的定义是"不是在跑内核 idle 线程"。

书里有一条很反直觉的提醒：

> The measure of CPU utilization spans all clock cycles for eligible activities, including memory stall cycles. This can be misleading: a CPU may be highly utilized because it is often stalled waiting for memory I/O, not just executing instructions, as described in the previous section. This is the case for the Netflix cloud, where the CPU utilization is mostly memory stall cycles [Gregg 17b] .

——**利用率把停滞周期也算进去了。** Netflix 云上的 CPU 利用率，绝大部分是**内存停滞周期**。

顺带：CPU 利用率不像磁盘那样"越高越糟"。内核有优先级、抢占、时间片，所以 100% 的 CPU 仍然是可用的 —— 高优先级工作能插队。

### §6.3.9 User Time / Kernel Time

用户态时间 / 内核态时间的比值，直接告诉你这是**计算密集**还是 **I/O 密集**：

- 计算密集：比值接近 **99/1**（图像处理、机器学习、基因组学）。
- I/O 密集：比值大约 **70/30**（做网络 I/O 的 Web 服务器）—— 因为大量系统调用在跑内核代码。

这两个数字是**示例**，不是标准。

### §6.3.10 Saturation

CPU 100% 利用率 = **饱和**。线程会遇到**调度延迟**（scheduler latency），即在运行队列上排队等的时间。

还有一种更隐蔽的饱和：即使 CPU 没到 100%，**资源配额**（cgroup / 限额）也可能先被耗尽。这时可运行线程照样要排队。云环境里很常见。

> A CPU running at saturation is less of a problem than other resource types, as higher-priority work can preempt the current thread.

——CPU 饱和**比磁盘饱和温和**，因为抢占机制存在。

### §6.3.11 / §6.3.12 Preemption 与 Priority Inversion

抢占让高优先级线程直接中断当前线程，消除它的排队延迟。

**优先级反转**是它的反面：

> Priority inversion occurs when a lower-priority thread holds a resource and blocks a higher-priority thread from running.

书里给了一个真实案例（A / B / C 三个线程，A 持锁去查内存用量，B 抢占 A 去压缩日志，C 醒来要跑却卡在 A 的锁上 —— 于是 C 实际被低优先级的 B 卡住了）。解法是**优先级继承**：A 临时获得 C 的优先级，直到它释放锁。

Linux 2.6.18 起提供了支持优先级继承的用户态互斥锁，面向实时负载。

### §6.3.13 Multiprocess, Multithreading

两种横向扩展方式：**多进程**（fork/clone）和**多线程**（pthreads）。

| 属性 | 多进程 | 多线程 |
|---|---|---|
| 开发 | 更容易，用 fork(2)/clone(2) | 用 threads API（pthreads） |
| 内存开销 | 每个进程独立地址空间（页级写时复制可缓解） | 小，只需额外栈、寄存器和 TLS |
| CPU 开销 | fork/clone/exit 的成本，含 MMU 地址空间管理 | 小，就是 API 调用 |
| 通信 | 走 IPC，有 CPU 成本和上下文切换；共享内存可缓解 | **最快**，直接访问共享内存，靠同步原语保证完整性 |
| 崩溃韧性 | **高**，进程彼此独立 | **低**，一个 bug 崩整个应用 |
| 内存回收 | 进程退出可把内存全还给系统 | 走系统分配器，可能有 CPU 争用，**复用前有碎片** |

最后一行是本章和第 7 章的接口 —— **线程的内存回收比进程麻烦**，这正是对象池存在的理由。

> Whichever technique is used, it is important that enough processes or threads be created to span the desired number of CPUs—which, for maximum performance, may be all of the CPUs available.

---

## 5. §6.4.1 Hardware：缓存层级的物理事实

### §6.4.1 CPU Caches

处理器上（或旁）的硬件缓存，包含：

```
L1 指令缓存 (I$) / L1 数据缓存 (D$) / TLB / L2 (E$) / L3（可选）
```

（`E$` 原本是 external cache 的 E，L2 集成进来以后被"机智地"改称 embedded cache。现在大家都用 "Level N" 的说法。）

**最后一级缓存（LLC）**：Intel 叫 last-level cache，也就是最长的那级缓存。

表 6.3 列了 1978–2019 年 Intel 各代处理器的缓存尺寸 —— 趋势很清楚：**缓存越来越大、越来越往片上走**（片上才能把访问延迟压到最低）。

### §6.4.1 Latency

> The access time for the Level 1 cache is typically a few CPU clock cycles, and for the larger Level 2 cache around a dozen clock cycles. Main memory access can take around 60 ns (around 240 cycles for a 4 GHz processor), and address translation by the MMU also adds latency.

作者用 LMbench 在 Intel Xeon E5620 2.4 GHz 上实测了访问延迟曲线（图 6.7，两轴都是对数）。**曲线上的台阶就是各级缓存的边界** —— 越过一级，延迟就跳到下一级。这张图是本章最重要的一张：它把"缓存"从抽象名词变成了可见的台阶。

### §6.4.1 Associativity

缓存放新条目的约束方式，三种：

- **全相联（fully associative）**：新条目可以放任何位置，淘汰用 LRU。命中率高，但实现昂贵。
- **直接映射（direct mapped）**：每个条目**只有一个**合法位置（用地址的一部分位做哈希索引）。便宜，但冲突时会互相踢。
- **组相联（set associative）**：先用地址映射出一个子集，再在子集里挑。**四路组相联 = 一个地址有四个候选位置**，取其中最好的（比如最久未用的）。

> CPU caches often use set associativity as a balance between fully associative (which is expensive to perform) and direct mapped (which has poor hit rates).

### §6.4.1 Cache Line

> A typical cache line size for x86 processors is 64 bytes.

**缓存行（cache line）是缓存的传输单位**。编译器会考虑它，程序员也该考虑 —— 原文直接指向第 5 章 §5.2.5 讲哈希表的地方。

（推演）64 字节 = 一个 cache line ≈ 8 个 `double` 或 16 个 `float` 或 64 个 `char`。所以"遍历一个数组"和"在一个结构体数组里跳着访问"性能差距巨大 —— 后者每次跳都可能是**另一个** cache line。

### §6.4.1 Cache Coherency

多处理器下，同一份内存可能同时缓存在多个 CPU 的缓存里。一个 CPU 改了，其他缓存里的副本就**过期**了 —— 缓存一致性协议负责让它们失效。

**代价是可观的**（LLC 命中的三种情形）：

```
LLC 命中，行未共享：      ~40 CPU 周期
LLC 命中，行被别的核共享： ~65 CPU 周期
LLC 命中，行被别的核改过： ~75 CPU 周期
```

同一份数据，**从 40 周期变成 75 周期，接近两倍** —— 而你只是多了一个线程在动它。

> Cache coherency is one of the greatest challenges in designing scalable multiprocessor systems, as memory can be modified rapidly.

### §6.4.1 MMU 与 Interconnects

MMU 负责虚拟地址到物理地址的翻译。片上的 **TLB** 缓存地址翻译；**TLB 未命中**时由 MMU 去主存里的**页表**（page table）查 —— 页表由内核维护。新一些的处理器能在硬件里处理 TLB 未命中，成本大幅下降。

（推演）这就是为什么"内存占用大"会连带拖慢 CPU：页表本身要占内存，页表项变多 → TLB 装不下 → 更多次翻译慢。

互连部分（QPI / UPI / PCIe）的一句话总结：

> Interconnects are typically designed for high bandwidth, so that they do not become a systemic bottleneck. If they do, performance will degrade as CPU instructions encounter stall cycles for operations that involve the interconnect, such as remote memory I/O. A key indicator for this is a drop in IPC.

——**互连成为瓶颈的信号，是 IPC 掉下来。**

---

## 6. §6.4.2 Software：调度器与亲和性

内核调度器的三大职责（图 6.12）：

- **Time sharing（时间共享）**：可运行线程之间轮流跑，优先级高的先跑。
- **Preemption（抢占）**：高优先级线程变为可运行时抢占当前线程。
- **Load balancing（负载均衡）**：把可运行线程挪到空闲或较闲的 CPU 的运行队列上。

书里明确指出：**术语上叫"运行队列"，实现上 Linux CFS 用的是红黑树**（按任务 CPU 时间做键）。这个细节很关键 —— 后面第 7 章的 `std::map` 对比就靠它。

调度器类（Linux）：**RT**（实时，优先级 0–99）、**O(1)**（2.6 时代的默认时间共享调度器）、**CFS**（2.6.23 起的默认，按 CPU 时间建红黑树）、**Idle**、**Deadline**（3.14 起，EDF）。

> The Linux scheduler also uses logic to avoid migrations when the cost is expected to exceed the benefit, preferring to leave busy threads running on the same CPU where the CPU caches should still be warm (CPU affinity).

——**迁移（migration）是有代价的**：迁到别的核 = 缓存变冷 + 缓存一致性流量。调度器会权衡。

---

## 7. §6.5 Methodology：怎么查

作者给的顺序是：**性能监控 → USE 方法 → profiling → 微基准 → 静态调优**。

### §6.5.2 USE Method

对每个 CPU 查三项：**Utilization**（忙不忙）、**Saturation**（可运行线程排队等 CPU 的程度）、**Errors**（包括可纠正错误）。

一个细节值得记：可以**先查 Errors**，因为它最快也最容易解释。有些处理器在可纠正错误（ECC）上升时会**主动下线一个 CPU**，以防不可纠正错误导致 CPU 失效。

在有 CPU 配额的环境里（cgroup），利用率要**按配额算**，不只是按物理 CPU 数算：

> Your system may exhaust its CPU quota well before the physical CPUs reach 100% utilization, encountering saturation earlier than expected.

### §6.5.4 Profiling

两种 CPU profiling 方式：

- **定时采样（timer-based sampling）**：典型速率 **99 Hz**。书里解释了为什么不是 100：

> 99 is used to avoid lock-step sampling that may occur at 100 Hertz, which would produce a skewed profile.

- **函数跟踪（function tracing）**：给函数调用插桩，精度高，但：

> Function tracing : Instrumenting all or some function calls to measure their duration. This provides a fine-level view, but the overhead can be prohibitive for production use, often 10% or more

采样有**盲区**：图 6.14 明确指出 —— 只在进程**在 CPU 上时**才采到样；系统调用期间 off-CPU 的时间不会被采到；**短命的函数会被整个漏掉**。

读火焰图的方法（§6.5.4）：

> Look top-down (leaf to root) for large “plateaus.” These show that a single function is on-CPU during many samples, and can lead to some quick wins.

然后**自底向上**理解代码层次，再**自顶向下**找"散落但常见"的热点（比如锁争用）。

### §6.5.5 Cycle Analysis

用 PMC 做到**周期级**分析。起点永远是 **IPC**：

> Begin cycle analysis by measuring IPC (inverse of CPI). If IPC is low, continue to investigate types of stall cycles. If IPC is high, look for ways in the code to reduce instructions performed.

还可以让 PMC **溢出时中断内核**收集栈回溯 —— 每 10000 次 L3 未命中中断一次，内核就能慢慢攒出"哪些代码路径在制造 L3 未命中"的画像，而不必每次都测（开销可接受）。这个特性的名字叫 overflow sampling，注意 **skid**（ skid：采样点漂移）。

---

## 8. §6.6 Observability Tools：几个够用的

- **`uptime`** —— 负载均值。1993 年 Linux 把负载均值从"CPU 需求"改成"**全系统需求**"（CPU + 磁盘 + 其他），因为需求从 CPU 挪到磁盘时，负载应该保持不变。负载 = 利用率 + 排队量（想象收费站：正在被服务的车 + 排队的车）。
- **`vmstat`** —— `r` 列是运行队列长度，`us`/`sy` 是用户/内核占比，`wa` 是等 I/O 的空闲时间。
- **`mpstat`** —— 每 CPU 统计，找"热核"。
- **`perf stat`** —— 直接给你 IPC：

```
$ perf stat gzip ubuntu-19.10-live-server-amd64.iso

    94,817,146,941      cycles                    #    3.757 GHz
   152,114,038,783      instructions              #    1.60  insn per cycle
    28,974,755,679      branches                  # 1148.167 M/sec
     1,020,287,443      branch-misses             #    3.52% of all branches
```

书里还有一个漂亮的案例：某 Shopify 基准做 NUMA 调优前后，IPC 从 **0.72 提到 0.89**（+24%），与应用吞吐提升 20–30% 相符。

**关键工具选择**：`perf list` 可以列出所有硬件事件；`perf c2c` 能做 cache-to-cache 和 **cache line 假共享（false sharing）** 分析（Linux 4.10+）。

---

## 9. 实战锚点：otf-server 面对缓存这件事

先说结论：**这个仓库已经在一个关键点上做对了，但另一个关键点做错了，而且做错的那个正好是"离散链表结构"。**

### 9.1 做对的：连接表和会话表都是连续数组

```cpp
// src/net/tcp_transport.h:133
std::vector<Conn> conns_;          ///< fd 直接作下标（§6.4.1，无分配无哈希）

// src/session/session_mgr.h:206
std::vector<Session> sessions_;    ///< 下标 = conn（fd 数组下标，§6.4.1）
```

注释里那句"**fd 直接作下标（无分配无哈希）**"是这一章的直接应用：用 `unordered_map<int, Conn>` 会多一层哈希桶跳转 + 节点堆分配；用数组则第 N 个连接的 `Conn` 就躺在**连续的第 N 个 cache line** 里。遍历所有连接时，前者的每次访问是一次潜在的主存访问，后者的访问几乎是顺序的。

同样，`src/world/world.h` 里的玩家状态是：

```cpp
std::array<Player, static_cast<std::size_t>(sync::kMaxPlayers)> players_{};
```

4 个玩家**内联**在 World 里，遍历它们就是一段连续内存 —— 每次 tick 都要遍历全部玩家做模拟和裁决，这是**最热的循环**，用 `std::array` 而不是 `std::vector<Player*>` 是对的。

`Player` 内部的输入缓冲同样是连续的：

```cpp
std::array<sim::PlayerInput, static_cast<std::size_t>(sync::kInputBufferSize)> inputs{};
std::size_t input_count = 0;
std::size_t input_head = 0;   // 下一个写入位（环形）
```

**定长数组 + head 指针 = 一个手写的环形缓冲区**（深度 8，满了丢最旧）。这就是本章说的"连续内存容器"在真实代码里的样子。

### 9.2 做错的：房间表是一棵红黑树

```cpp
// src/room/room_mgr.h:119
std::map<std::uint32_t, Room> rooms_;
```

**这是 `std::map`，不是 `std::unordered_map`** —— 也就是书里说的"**红黑树**"（§6.4.2 讲 CFS 时用的就是同一个结构）。它有三个叠加的坏处：

1. **每个节点一次堆分配。** `std::map` 的节点不是数组里的元素，是 malloc 出来的独立块 —— 32 个房间节点散落在堆的各处，**彼此大概率不在同一个 cache line，甚至不在同一个 page**。
2. **每个节点很重。** `Room` 内联持有一整个 `world::World`（`world::World world_;`），里面有 4 个 `Player`、输入缓冲、命中队列等。节点几十上百字节 —— 一个节点横跨好几个 cache line。
3. **遍历是指针跳跃。** `room_mgr.cpp` 里 `for (const auto& [id, room] : rooms_)` 每次 `++` 迭代器都要**解引用一个新节点**，而这些节点在内存里是**随机排布**的。

**这意味着每 tick 遍历房间时，几乎每次访问都是一次缓存未命中** —— 而每个房间里还要跑完整的 Boss 模拟与命中裁决。

（推演）一个 32 房间的服务器，房间表按 cache line 顺序**连续**排布 vs 按红黑树节点散落，差别不是几个百分点：前者每次遍历是顺序预取（硬件预取器能提前把下一条 cache line 拉进来），后者每次遍历都是 N 次独立的主存访问。

**可能的改法**（本讲义只做诊断，不动代码）：

- 房间数量有**硬上限 32**（`kMaxRooms = sync::kMaxRooms`），且 `room_id` **单调递增且永不复用**（`room_mgr.cpp` 的注释明确说了不复用的理由：避免客户端攥着旧 room_id 进错局）。既然 id 单调递增，**完全可以用 `std::vector<Room>` + id 直接作下标**，把红黑树换成数组 —— 和 `conns_` / `sessions_` 完全同构。
- 但要注意：`Room` 内联一整个 `World`，32 份可能不小。换成数组后应当考虑 **`World` 改为指针 + 对象池**（这正是 `技术参考.md` §6.4.5 里"对象池"那条备选优化）。

### 9.3 发送缓冲：环形缓冲区还没做

```cpp
// src/net/tcp_transport.cpp:468
const std::size_t old = c.out.size();
c.out.resize(old + n);
std::memcpy(c.out.data() + old, buf, n);
```

代码注释自己写清楚了：

> ★ 追加用 resize + memcpy，不是 insert：insert 要走泛型区间构造，
>   而这里要的就是一次 memmove/memcpy。resize **保留已有 capacity**，
>   所以稳态下（缓冲已撑到过峰值）这里**零分配**。

这一点比"每次都重新分配"要好得多 —— 稳态零分配。**但它仍然不是环形缓冲区**：`c.out` 用 `out_sent` 指针标记已发送位置，`c.out.size()` 会随着发送**单调增长**（`FlushWrite` 里 `c.out.clear()` 才收回去）。

**问题**：一段连接上只要发送过较大的一批数据，`c.out` 的 capacity 就永久停在峰值，不会缩回。每条连接一个 `std::vector`，峰值多大就常驻多大 —— 这既是**内存占用**问题，也是**缓存问题**：`c.out` 的数据从头排到 `out_sent`，前 `out_sent` 字节是**已经发出去但还没被清掉的垃圾**，遍历时它们把有用的数据挤出 cache line。

（推演）正确的形态是**环形缓冲区**：固定大小的 `std::vector<uint8_t>` + `head_`/`tail_` 两个下标，满了就覆盖最旧的或者触发背压。这个仓库在**接收**侧已经这么做了 —— `src/net/frame_codec.h` 的 `FrameDecoder` 里有 `std::vector<uint8_t> ring_` 配 `head_`/`tail_`，是标准的环形缓冲。**发送侧没有做，是一处明显的不对称。**

### 9.4 假共享：现在没有，但地雷已经埋好

本仓库是**单线程 reactor**（`src/net/metrics.h` 文件头写得很清楚："线程模型：单线程 reactor"），所以**目前不存在假共享**。

但只要拆线程，它就会冒出来。危险点在 `src/net/metrics.h`：

```cpp
std::atomic<std::int64_t> conns_{0}, rooms_{0}, sent_{0}, recv_{0};
std::atomic<std::int64_t> hb_timeout_{0}, oversize_{0};
std::atomic<std::int64_t> ctl_add_{0}, ctl_del_{0}, ctl_mod_{0}, ctl_add_out_{0};
std::atomic<std::int64_t> tick_drift_{0}, snap_ms_{0};
```

**12 个 `std::atomic<int64_t>` 紧挨着排布，总共 96 字节 —— 正好压在两条 cache line 上。** 如果游戏线程和 metrics 线程分别写 `sent_` 和 `recv_`，而这两个字段恰好落在同一条 cache line，那么：

> 两个 CPU 更新同一 cache line 里的不同变量，每个都要把对方缓存里的那条行作废（invalidate）—— 这就是**假共享（false sharing）**。

症状非常有代表性：**吞吐上不去，但两个线程各自的 CPU 使用率都不高，perf 里看到的是 cache-to-cache 流量异常高。** 排查工具是 `perf c2c`（第 13 章会展开）。

**解法**（书里第 5 章 §5.2.5 讲哈希表时给过）：**用无用的字节填充，让每个变量独占一条 cache line**（对齐到 64 字节边界）。C++20 直接用 `alignas(64)`。

**拆线程后真正会热的另一个点是 `src/world/`**：World 状态（4 个 `Player` + Boss 状态 + 命中队列）会被多个线程读。如果 sim 线程写、IO 线程读，它们之间要通过队列传递 —— 而**队列本身就是 cache 边界**。这正是"环形缓冲区 + 连续内存容器"这套做法的价值：跨线程传递的是**一块连续内存**，而不是一堆离散对象。

### 9.5 本章的一句话行动清单

1. `rooms_` 从 `std::map` 换成 `std::vector`（id 单调递增，正好能作下标）—— 消除 32 次堆分配与指针跳跃。
2. 发送缓冲 `c.out` 改成环形缓冲区 —— 与 `FrameDecoder::ring_` 对称。
3. `Metrics` 的原子计数器按 64 字节对齐 —— 为拆线程预埋。
4. 会话/房间对象上对象池 —— 配合 §6.3.13 那张表最后一行（"线程的内存复用前有碎片"）。

---

## 10. 收尾：本章金句清单

1. **Memory access is often the slowest** —— 内存访问是流水线里最慢的一步，其余一切优化都要绕开它。
2. **A low IPC indicates that CPUs are often stalled, typically for memory access.** —— 低 IPC = CPU 在等内存，这是本章的诊断起点。
3. **Even if the CPU in your system appears to be fully utilized (a bottleneck), a faster clock rate may not speed up performance—it depends on what those fast CPU cycles are actually doing.** —— CPU 满了不等于该换更快的 CPU。
4. **Installing CPUs with a higher clock rate may not improve performance to the degree expected** —— 更快的 CPU 可能只是给你更多停滞周期。
5. **It should be noted that IPC shows the efficiency of instruction processing , but not of the instructions themselves.** —— IPC 变好不等于程序变快，它只衡量"指令处理得效率高不高"。
6. **The measure of CPU utilization spans all clock cycles for eligible activities, including memory stall cycles.** —— 利用率把停滞周期算进去了，所以"CPU 95%"可能意味着"CPU 在发呆 95%"。
7. **The number of software threads that are queued and ready to run is an important performance metric indicating CPU saturation.** —— 运行队列长度就是饱和度。
8. **These caches are described as having cache warmth , and this strategy to keep threads running on the same CPUs is called CPU affinity .** —— "缓存温度"是调度器不肯轻易迁移线程的理由。
9. **The Linux scheduler also uses logic to avoid migrations when the cost is expected to exceed the benefit** —— 迁移有代价：缓存变冷 + 一致性流量。
10. **Cache coherency is one of the greatest challenges in designing scalable multiprocessor systems, as memory can be modified rapidly.** —— 一致性协议让同一份数据的访问代价从 40 周期变成 75 周期。
11. **A typical cache line size for x86 processors is 64 bytes.** —— 64 字节是缓存的搬运单位，你的数据结构尺寸要按它算。
12. **CPU caches often use set associativity as a balance between fully associative (which is expensive to perform) and direct mapped (which has poor hit rates).** —— 组相联是成本与命中率的折中。
13. **Interconnects are typically designed for high bandwidth, so that they do not become a systemic bottleneck.** —— 互连是按"不成为系统瓶颈"来设计的；一旦它成了瓶颈，信号是 IPC 掉下来。
14. **Look top-down (leaf to root) for large “plateaus.”** —— 火焰图先看大平台，那是最快的赢。
15. **99 is used to avoid lock-step sampling that may occur at 100 Hertz, which would produce a skewed profile.** —— 连采样频率都有讲究。
16. **For CPUs, the biggest performance wins are typically those that eliminate unnecessary work, which is an effective form of tuning.** —— CPU 调优的第一收益永远来自"少做事"。
17. **A process may be bound to one or more CPUs, which may increase its performance by improving cache warmth and memory locality.** —— 绑核能提性能，靠的是缓存温度。
18. **The performance of each hardware thread is not the same as a separate CPU core, and depends on the workload.** —— 硬件线程 ≠ 核心，别拿 SMT 当核心数卖。
