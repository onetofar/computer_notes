---
title: Chapter 6. CPUs（精读）
category: 性能工程
tags:
  - CPU
  - IPC
  - 调度器
  - 性能剖析
  - 火焰图
  - 性能调优
difficulty: 高阶
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）第6章 CPUs"
source_section: "精读 §6.1 / §6.2 / §6.3 / §6.4.1 / §6.4.2 / §6.5 / §6.6 / §6.7 / §6.8 / §6.9；§6.10 Exercises 与 §6.11 References 不立篇（练习题与参考文献是全书共用的自测与索引材料，不承载机制陈述）；§6.2.1–§6.2.3、§6.3.1–§6.3.15、§6.4.1 下的无编号子标题、§6.5.1–§6.5.11、§6.6.1–§6.6.21、§6.7.1–§6.7.4、§6.9.1–§6.9.10 按原文文字引用，不编 § 号"
skills:
  - obsidian-markdown
---

# Chapter 6. CPUs

> **小节地图**：本章是全书的 **CPU 主体章**——从「CPU 是什么」一路走到「在 Linux 上怎么观测、怎么调」。
> 立篇：§6.1（术语口径）、§6.2（三个模型：架构 / 缓存 / 运行队列）、§6.3（15 个性能概念，从时钟频率到 IPC、利用率、饱和、优先级反转）、
> §6.4.1（硬件：处理器、P/C 状态、缓存层级与延迟、相联性、缓存一致性、MMU、互连、硬件计数器 PMC、GPU）、
> §6.4.2（软件：内核调度器、调度类、空闲线程、NUMA 分组）、§6.5（11 种分析方法）、
> §6.6（21 个 Linux 观测工具）、§6.7（4 类可视化）、§6.8（实验）、§6.9（调优）。
> **不立篇**：§6.10 Exercises、§6.11 References（全书共用的自测与文献材料，不承载机制陈述）。
> 底层依赖：内存侧在第 7 章、磁盘在第 9 章、网络在第 10 章、云在第 11 章、基准测试在第 12 章；
> 调度器与上下文切换的操作系统侧铺底在第 3 章。
> 同层对照：[[Chapter 1. Introduction]]（方法论起点）、[[Chapter 2. Methodologies]]（通用方法论框架）、
> [[Chapter 3. Operating Systems]]（内核侧前置）、[[Chapter 9. Disks]]（I/O 侧的同一套方法论）。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §6.1 术语：硬件线程 | 06-Chapter 6. CPUs.md L79 | Hardware thread : A CPU architecture that supports executing multiple threads in parallel on a single core (including Intel’s Hyper-Threading Technology), where each thread is an independent CPU instance. This scaling approach is called simultaneous multithreading (SMT). |
| §6.1 术语：运行队列 | 06-Chapter 6. CPUs.md L89 | Run queue : A queue of runnable threads that are waiting to be serviced by CPUs. Modern kernels may use some other data structure (e.g., a red-black tree) to store runnable threads, but we still often use the term run queue. |
| §6.2.1 CPU 架构模型 | 06-Chapter 6. CPUs.md L107 | Each hardware thread is addressable as a logical CPU , so this processor appears as eight CPUs. The operating system may have some additional knowledge of topology to improve its scheduling decisions, such as which CPUs are on the same core and how CPU caches are shared. |
| §6.3.1 时钟频率 | 06-Chapter 6. CPUs.md L143 | Clock rate is often marketed as the primary feature of a processor, but this can be a little misleading. Even if the CPU in your system appears to be fully utilized (a bottleneck), a faster clock rate may not speed up performance—it depends on what those fast CPU cycles are actually doing. |
| §6.3.2 指令与停顿周期 | 06-Chapter 6. CPUs.md L161 | Memory access is often the slowest, as it may take dozens of clock cycles to read or write to main memory, during which instruction execution has stalled (and these cycles while stalled are called stall cycles ). |
| §6.3.3 分支预测 | 06-Chapter 6. CPUs.md L173 | As an optimization, processors often implement branch prediction , where they will guess the outcome of the test and begin processing the outcome instructions. If the guess later proves to be wrong, the progress in the instruction pipeline must be discarded, hurting performance. |
| §6.3.7 IPC、CPI | 06-Chapter 6. CPUs.md L199 | A low IPC indicates that CPUs are often stalled, typically for memory access. A high IPC indicates that CPUs are often not stalled and have a high instruction throughput. These metrics suggest where performance tuning efforts may be best spent. |
| §6.3.8 利用率 | 06-Chapter 6. CPUs.md L213 | The measure of CPU utilization spans all clock cycles for eligible activities, including memory stall cycles. This can be misleading: a CPU may be highly utilized because it is often stalled waiting for memory I/O, not just executing instructions, as described in the previous section. |
| §6.3.10 饱和 | 06-Chapter 6. CPUs.md L229 | A CPU at 100% utilization is saturated , and threads will encounter scheduler latency as they wait to run on-CPU, decreasing overall performance. This latency is the time spent waiting on the CPU run queue or other structure used to manage threads. |
| §6.3.12 优先级反转 | 06-Chapter 6. CPUs.md L241 | Priority inversion occurs when a lower-priority thread holds a resource and blocks a higher-priority thread from running. This reduces the performance of the higher-priority work, as it is blocked waiting. |
| §6.4.1 缓存层级与延迟 | 06-Chapter 6. CPUs.md L819 | The access time for the Level 1 cache is typically a few CPU clock cycles, and for the larger Level 2 cache around a dozen clock cycles. Main memory access can take around 60 ns (around 240 cycles for a 4 GHz processor), and address translation by the MMU also adds latency. |
| §6.4.1 缓存一致性：LLC 惩罚 | 06-Chapter 6. CPUs.md L855 | LLC hit, line modified in another core: ~75 CPU cycles |
| §6.4.1 硬件计数器 PMC | 06-Chapter 6. CPUs.md L953 | Each CPU has a small number of registers, usually between two and eight, that can be programmed to record events like these. Those available depend on the processor type and model and are documented in the processor manual. |
| §6.4.2 调度类 CFS | 06-Chapter 6. CPUs.md L1197 | The scheduler manages tasks on a red-black tree keyed from the task CPU time, instead of traditional run queues. |
| §6.5 方法论顺序 | 06-Chapter 6. CPUs.md L1319 | My suggestion is to use the following, in this order: performance monitoring, the USE method, profiling, micro-benchmarking, and static performance tuning. |
| §6.5.2 USE 方法 | 06-Chapter 6. CPUs.md L1365 | CPU utilization should be measured in terms of the imposed limit, in addition to the physical limit. |
| §6.5.5 周期分析 | 06-Chapter 6. CPUs.md L1487 | Begin cycle analysis by measuring IPC (inverse of CPI). If IPC is low, continue to investigate types of stall cycles. If IPC is high, look for ways in the code to reduce instructions performed. |
| §6.6.1 负载均值 | 06-Chapter 6. CPUs.md L1737 | But in 1993, Linux changed load averages to show system-wide demand: CPUs, disks, and other resources. |
| §6.7.3 火焰图 | 06-Chapter 6. CPUs.md L3321 | The x-axis spans the sample population. It does not show the passing of time from left to right, as most graphs do. The left-to-right ordering has no meaning (it’s sorted alphabetically). |
| §6.9 调优 | 06-Chapter 6. CPUs.md L3434 | For CPUs, the biggest performance wins are typically those that eliminate unnecessary work, which is an effective form of tuning. |

> **书里没说的**（本篇的推论，原文没有，别当原文观点）：
> - **「四把尺子」对照表**（时钟频率 / IPC / 利用率 / 饱和）是本篇为检索方便整理的；原文 §6.3 是逐个小节平铺，
>   没有把它们并成一张表。
> - **「先看利用率 → 再看饱和 → 最后才动 IPC 与周期分析」这条流程**是本篇把 §6.5 的建议顺序（L1319）与
>   §6.5.5 的「先测 IPC」（L1487）**组合**出来的；原文没有把两处合成一条流程。
> - **「缓存热度」作为统一线索**（贯穿 §6.2.3 的 run queue、§6.5.10 的 CPU 绑定、§6.9.6–6.9.7 的独占 CPU 集）
>   是本篇的连线视角；原文把三处分开陈述，未合并成一个概念。
> - **CPU 与 GPU 的性能对比**本篇只保留原文 Table 6.6 的要点，不做量化推演；GPU 与其他加速器的可观测性
>   在原文 §6.4.1 与 §6.6.21 只是点到为止。

## §6.1 术语：先把 CPU 的层次分清

**机制**：这一节是全章的**口径表**。CPU 性能分析里最贵的错误往往不是「算错了」，
而是**把不同层次的 CPU 当成同一个东西**——于是「这台机器有多少核」这个问题能被答出四个不同的数字。

原文的词条分两组：

| 组 | 词 | 原文定义要点 |
|---|---|---|
| 硬件侧 | **Processor** | 插在系统或处理器板 socket 上的物理芯片，内含一个或多个 CPU（实现为 core 或硬件线程） |
| 硬件侧 | **Core** | 多核处理器上的一个独立 CPU 实例；用 core 扩展处理器的做法叫 **chip-level multiprocessing (CMP)** |
| 硬件侧 | **Hardware thread** | 单个 core 上并行执行多个线程的 CPU 架构（Intel 的 Hyper-Threading 技术属此类），**每个线程都是一个独立 CPU 实例**；这种扩展方式叫 **SMT** |
| 硬件侧 | **CPU instruction** | 指令集中的一条 CPU 操作，分为算术运算、内存 I/O、控制逻辑三类 |
| 软件侧 | **Logical CPU** | 操作系统看到的 CPU 实例（一个可调度的实体）；可能由硬件线程（也叫 virtual core）、core 或单核处理器实现 |
| 软件侧 | **Scheduler** | 把线程指派到 CPU 上的内核子系统 |
| 软件侧 | **Run queue** | 等待 CPU 服务的可运行线程队列 |

**最该钉死的一条**：硬件线程是「独立 CPU 实例」，但**不等于独立 core**。

> Hardware thread : A CPU architecture that supports executing multiple threads in parallel on a single core (including Intel’s Hyper-Threading Technology), where each thread is an independent CPU instance. This scaling approach is called simultaneous multithreading (SMT).

**容易错在哪**：把「8 个硬件线程」当成「8 个核」去买机器、算容量、或解释「哪颗 CPU 满了」。
硬件线程共享同一个 core 的执行单元，**共享 L1/L2 缓存与主频预算**——它的性能取决于负载类型（§6.3.6）。

**第二个术语陷阱**：现代内核里 run queue 已经不是队列了。

> Run queue : A queue of runnable threads that are waiting to be serviced by CPUs. Modern kernels may use some other data structure (e.g., a red-black tree) to store runnable threads, but we still often use the term run queue.

**机制含义**：**术语保留，数据结构已换代**。CFS 用的是红黑树（§6.4.2），但所有人仍在说「run queue」。
所以看到「run queue length」这个指标时，要先确认它到底数的是「队列长度」还是「红黑树里的 runnable 任务数」——
后者的统计方式与前者不等价。

**判据**：拿到一台机器，先问三个问题并写下答案——**物理 socket 数 / core 数 / 逻辑 CPU 数**。
这三者对不上，后面的「per-CPU 利用率」「亲和性」「扩缩容」判断全都会错位。

## §6.2 模型：三个简化模型先建立直觉

原文用三个模型（架构、缓存、运行队列）铺底，§6.4 再深入实现细节。

### §6.2.1 CPU 架构模型

以「单处理器、4 个 core、共 8 个硬件线程」为例，图 6.1 同时画出**物理架构**与**操作系统看到的视图**。

> Each hardware thread is addressable as a logical CPU , so this processor appears as eight CPUs. The operating system may have some additional knowledge of topology to improve its scheduling decisions, such as which CPUs are on the same core and how CPU caches are shared.

**机制含义**：操作系统眼里的「CPU」是**逻辑 CPU**，数量 = 硬件线程数（本例 8），不是 core 数（4）。
而内核**额外掌握拓扑知识**（哪些逻辑 CPU 在同一个 core、缓存怎么共享）——这些知识正是后面 §6.4.2
亲和性与 §6.9.6 绑定的物理基础。

**易错点**：`nproc` 给你的是逻辑 CPU 数；`lscpu` 的 `Core(s) per socket` 才是 core 数。

### §6.2.2 CPU 缓存模型

缓存的**规模越小、速度越快**，越靠近 CPU——这是一个权衡。缓存在片上（integrated）还是片外（external）
取决于处理器类型；早期处理器集成更少的缓存层级。

### §6.2.3 CPU 运行队列模型

图 6.3 是内核调度器管理的运行队列。队列里的线程状态（ready to run / on-CPU）在第 3 章图 3.8 讲过。

**两个本节特有的机制点**：

1. **queued 且 ready to run 的软件线程数**是重要的性能指标，指示 CPU 饱和度。
   图里这一瞬间有 4 个，另加 1 个正在 on-CPU 运行。
2. **等待 run queue 的时间**叫 **run-queue latency**，也叫 **dispatcher-queue latency**；
   本书统一用 **scheduler latency**，因为它对**不用队列的调度器**（如 CFS）也成立。

**多处理器系统的两条策略**（本节的真正机制）：

| 策略 | 做法 | 收益 |
|---|---|---|
| **每 CPU 一个 run queue** | 内核尽量让线程留在原来的 run queue 上 | 线程更可能继续跑在**数据已被缓存热**的 CPU 上（**cache warmth**） |
| **CPU 亲和（CPU affinity）** | 把忙的线程留在同一个 CPU 上 | 保持缓存热度，避免迁移成本 |

在 **NUMA** 系统上，per-CPU run queue 还额外改善**内存局部性**（让线程留在同一个内存节点，见第 7 章），
并**避免全局 run queue 的锁竞争**——全局队列在多 CPU 下会因为 mutex 争抢而损害扩展性。

**判据**：排查「线程迁移导致性能抖动」时，先确认调度器有没有在做**迁移成本 > 收益**的判断；
Linux 里对应 `task_hot()` 与 `idle_balance()` 这两个函数——它们的存在本身就是「迁移不是免费的」的证据。

## §6.3 概念：CPU 性能的核心度量

原文把 15 个概念平铺，本篇按「执行模型 → 度量 → 调度」三组重排（**重排是本篇的整理方式，原文顺序不同**）。

### §6.3.1 时钟频率：不是性能的代理指标

时钟是驱动处理器全部逻辑的数字信号；每条指令可能要一个或多个时钟周期。4 GHz = 每秒 40 亿个周期。
部分处理器可变速率：由操作系统请求，或由处理器自己动态调整（内核空闲线程会请求降频省电）。

**但主频是最容易被误用的指标**：

> Clock rate is often marketed as the primary feature of a processor, but this can be a little misleading. Even if the CPU in your system appears to be fully utilized (a bottleneck), a faster clock rate may not speed up performance—it depends on what those fast CPU cycles are actually doing.

**机制**：如果这些「快周期」大部分是**等内存的停顿周期**，把它们跑得更快**不会**提高指令吞吐。
换更快的 CPU 可能只是得到**更多停顿周期、相同的每秒完成指令数**。

**判据**：任何以「主频更高」为理由的采购或换型，先要求对方给出**该负载的 IPC 与 stall cycle 分布**。
主频只有在「CPU 主要在算、不在等」时才等价于性能。

### §6.3.2 指令五步与停顿周期

一条指令经由 CPU 里的**功能单元（functional unit）**处理，步骤为：

1. 指令取指（instruction fetch）
2. 指令解码（instruction decode）
3. 执行（execute）
4. 内存访问（memory access）
5. 寄存器写回（register write-back）

**后两步是可选的**——很多指令只操作寄存器，不需要内存访问步骤。每一步至少占一个时钟周期。

> Memory access is often the slowest, as it may take dozens of clock cycles to read or write to main memory, during which instruction execution has stalled (and these cycles while stalled are called stall cycles ).

**机制**：内存访问要几十个周期，主存期间指令执行处于**停滞**状态——这些周期叫 **stall cycles（停顿周期）**。
这正是缓存存在的理由（§6.4.1）：缓存能大幅削减内存访问所需的周期数。

**判据**：**停顿周期是 CPU 上最大的单项开销来源**。任何「CPU 忙」的报告都应能回答：
这 100% 里有多少是算指令、多少是停顿（等内存 / 等资源 / 等总线）？

### §6.3.3 指令流水线与分支预测

流水线让不同指令的不同阶段并行执行，像工厂流水线。理想情况下**每个时钟周期完成一条指令**
（不流水的话单条指令要 5 个周期，且每步只有一个功能单元在动、四个闲着）。

现代处理器还可以**乱序执行**（后来的指令可以先完成），但**条件分支**带来难题：处理器事先不知道分支走向，
于是做**分支预测**，先按猜的结果执行。

> As an optimization, processors often implement branch prediction , where they will guess the outcome of the test and begin processing the outcome instructions. If the guess later proves to be wrong, the progress in the instruction pipeline must be discarded, hurting performance.

**机制**：猜错 ⇒ **已推进的流水线进度必须被丢弃** ⇒ 性能受损。这就是分支预测错误（misprediction）昂贵的原因。
程序员可以在代码里放提示（如 Linux 内核源码里的 `likely()` / `unlikely()` 宏）来提高猜中率。

**§6.3.4 指令宽度（superscalar）**：同类功能单元可以有多份。现代处理器是 **3-wide 或 4-wide**，
即每周期最多完成 3~4 条指令。

### §6.3.6 SMT：硬件线程不等于独立核心

SMT 结合**超标量架构**与**硬件多线程支持**，让一个 core 同时跑多个线程，在指令间切换
（例如一个线程因内存 I/O 停顿时切到另一个）。内核把这些硬件线程呈现为**虚拟 CPU**，照常调度。
Intel 的 Hyper-Threading 通常每 core 2 个硬件线程；POWER8 是每 core 8 个。

**关键限制**（原文自己点明）：**硬件线程的性能不等于独立 core**，取决于负载。
内核为了避免问题，可能把负载**摊到不同 core** 上，使每个 core 只有一个硬件线程在忙，避免 **hardware thread contention**。
另外，**停顿周期多（低 IPC）的负载**比**指令密集（高 IPC）的负载**受益更多——因为停顿会减少 core 内争抢。

**判据**：SMT 的收益**不能用「核数翻倍」来估算**。判断依据是**负载的停顿特征**：
内存受限的负载开 SMT 收益大，纯计算受限的负载开 SMT 收益小甚至为负。

### §6.3.7 IPC、CPI：最重要的单一指标

**Instructions per cycle（IPC）**是描述 CPU 如何花掉时钟周期的高层指标；它的倒数是
**cycles per instruction（CPI）**。Linux 社区与 Linux `perf(1)` 常用 **IPC**，Intel 那边常用 **CPI**。

> A low IPC indicates that CPUs are often stalled, typically for memory access. A high IPC indicates that CPUs are often not stalled and have a high instruction throughput. These metrics suggest where performance tuning efforts may be best spent.

**绝对值是相对的**：依赖处理器与处理器特性，要用已知负载实测。原文给的量级是
低 IPC 工作负载可到 **0.2 或更低**，高 IPC 可**超过 1.0**（得益于流水线与宽度）；
Netflix 云上负载的 IPC 从 **0.2（算慢）到 1.5（算好）**，换算成 CPI 是 **5.0 到 0.66**。

**判据**：**IPC 是本篇推荐的第一把尺子**——它一测出来就分好了岔路：
低 IPC ⇒ 往停顿方向查（内存、缓存、NUMA）；高 IPC ⇒ 往「指令太多」方向查（算法与代码量）。

### §6.3.8 利用率：分母里含停顿周期

CPU 利用率 = 一个区间内 CPU 实例**忙于工作的时间占比**。它可以按「不是跑内核空闲线程」来度量，
即跑用户态线程、跑其他内核线程、或处理中断。

> The measure of CPU utilization spans all clock cycles for eligible activities, including memory stall cycles. This can be misleading: a CPU may be highly utilized because it is often stalled waiting for memory I/O, not just executing instructions, as described in the previous section.

**两个机制要点**：

1. **高利用率本身不一定是问题**，它可能只说明系统**在干活**。有人甚至把它当 ROI 指标——
   高利用率的系统算「投资回报好」，空闲的系统算浪费。
2. 与磁盘等资源不同，**CPU 在高利用率下性能不会陡降**，因为内核有优先级、抢占与时间共享，
   能保证高优先级的工作先跑。

**但第 2 点是本篇必须点破的陷阱**（原文自己给了证据）：Netflix 云上的 CPU 利用率**大部分是内存停顿周期**。
换句话说，**利用率这个指标无法区分「在算」和「在等」**。

**判据**：把「CPU 利用率 90%」写进报告之前，必须补一句：**这 90% 里有多少是停顿周期**（IPC 或 stall 计数）。
不补这句的报告是误导。

### §6.3.9 用户态/内核态时间：负载类型的第一判据

**user time** = 执行用户态软件的时间；**kernel time** = 执行内核态软件的时间，
**包含系统调用、内核线程与中断的时间**。

**全系统口径下，user/kernel 比值指示所执行的负载类型**：

| 负载类型 | user/kernel 比 | 原文例子 |
|---|---|---|
| 计算密集 | 接近 **99/1** | 图像处理、机器学习、基因组学、数据分析 |
| I/O 密集 | 约 **70/30** | 做网络 I/O 的 web server（系统调用率高） |

原文明确说这些数字**依赖很多因素**，列出来只是表达「预期的比值长什么样」。

**判据**：**这是 CPU 分析里性价比最高的一次测量**——一次 `top` / `mpstat` 就能量出负载类型，
而它直接决定后面该往用户态查还是往系统调用 / 中断查。

### §6.3.10 饱和：100% 利用率即饱和

> A CPU at 100% utilization is saturated , and threads will encounter scheduler latency as they wait to run on-CPU, decreasing overall performance. This latency is the time spent waiting on the CPU run queue or other structure used to manage threads.

**两种饱和形态**：

| 形态 | 现象 | 注意 |
|---|---|---|
| **物理饱和** | CPU 100% 利用率 | 线程等调度延迟，整体性能下降 |
| **配额饱和** | CPU 物理利用率没满，但**施加的限制**（资源控制）已到 | 可运行线程仍要排队；**用户是否感知取决于虚拟化类型**（第 11 章） |

**饱和比磁盘那类资源的问题轻**——因为高优先级工作可以**抢占**当前线程。

### §6.3.11–6.3.12 抢占与优先级反转

**抢占（preemption）**：让高优先级线程抢走当前线程的 CPU，**消除高优先级工作的 run-queue 延迟**。

**优先级反转（priority inversion）**：

> Priority inversion occurs when a lower-priority thread holds a resource and blocks a higher-priority thread from running. This reduces the performance of the higher-priority work, as it is blocked waiting.

**原文给的真实案例（本篇照搬其结构）**：A（低优先级监控）持有生产库的地址空间锁 →
B（压缩日志的普通任务）抢占 A → C（生产库，高优先级）I/O 完成变可运行 → C 抢占 B，
但 C 阻塞在 A 持有的锁上 → 调度器回头选 B → **高优先级线程 C 实际被低优先级的 B 挡住**。

**解法是优先级继承（priority inheritance）**：把 C 的高优先级借给 A，A 抢占 B 直到释放锁，C 才能跑。
Linux 从 **2.6.18** 起提供支持优先级继承的用户态 mutex，为实时负载设计。

**判据**：**「高优先级请求变慢了」但 CPU 并不忙** ⇒ 查优先级反转，别去加 CPU。
加 CPU 不会让被锁挡住的高优先级线程跑得更快。

### §6.3.13 多进程 vs 多线程

两种把应用扩展到多 CPU 的技术（原文图 6.4，注意这是**软件多线程**，与前面 SMT 无关）。
Linux 上两者都由 task 实现。

| 属性 | 多进程 | 多线程 |
|---|---|---|
| 开发 | 较容易，用 `fork(2)` / `clone(2)` | 用线程 API（pthreads） |
| 内存开销 | 每进程独立地址空间，消耗内存（页级写时复制可缓解） | 小，只需额外栈、寄存器空间与线程局部数据空间 |
| CPU 开销 | `fork(2)` / `clone(2)` / `exit(2)` 的成本，含 MMU 管理地址空间的工作 | 小，只是 API 调用 |
| 通信 | 走 IPC，**有 CPU 成本**（跨地址空间搬数据要上下文切换），除非用共享内存 | 最快，直接访问共享内存；靠同步原语保证完整性 |
| 崩溃隔离 | 高，进程彼此独立 | 低，任何一个 bug 都能带崩整个应用 |
| 内存使用 | 部分内存可能重复，但进程可以 `exit(2)` 把全部内存还给系统 | 走系统分配器，可能因多线程产生 CPU 争抢，内存复用前有碎片 |

原文结论：多线程通常被认为更优，但**对开发者更复杂**。

**最容易被忽略的一句**：

> Whichever technique is used, it is important that enough processes or threads be created to span the desired number of CPUs—which, for maximum performance, may be all of the CPUs available. Some applications may perform better when running on fewer CPUs, when the cost of thread synchronization and reduced memory locality (NUMA) outweighs the benefit of running across more CPUs.

**机制**：线程数要**够**（覆盖目标 CPU 数），但**不是越多越好**——
同步成本与 NUMA 局部性下降可能超过并行收益。

**判据**：加线程前先问「这个负载的同步粒度有多细」；细粒度锁竞争型负载，
加线程会**同时**增加争抢与跨 NUMA 流量，得不偿失。

### §6.3.14–6.3.15 字长与编译优化

**字长** = 处理器设计的最大整数/寄存器宽度（32 位或 64 位），也常用于地址空间大小与数据通路宽度。

**64 位不一定更快**：更大的尺寸会给某些数据类型的未用位带来内存开销；
**指针变大会抬高数据足迹**，可能需要更多内存 I/O。x86-64 上这些开销被**更多寄存器**与**更高效的寄存器传参约定**补偿，
所以 64 位应用通常快于其 32 位版本。处理器与操作系统可以同时支持多种字长、同时跑不同字长编译的程序——
**按小字长编译的程序可能跑成功，但性能相对较差**。

编译器选项（含字长设置）与优化能显著改善应用性能；编译器也常更新以利用最新指令集——
**有时仅仅是换个更新的编译器就有显著提升**。

## §6.4.1 硬件：缓存、互连、硬件计数器

### 处理器组件与 P/C 状态

通用双核处理器的组件（图 6.5）：**控制单元**（取指、解码、管理执行、存结果）、
共享浮点单元、（可选的）共享 L3 缓存。还可能有：**P-cache**（预取，每 core）、
**W-cache**（写回，每 core）、**Clock**（时钟信号源）、**Timestamp counter**（高精度时间）、
**Microcode ROM**（把指令快速转成电路信号）、**温度传感器**、**片上网络接口**。

部分处理器用温度传感器作为**动态超频**的输入（Intel Turbo Boost）——**前提是核心仍在温度包络内**，
可能的频率档位由 **P-states** 定义。

**ACPI 标准**定义了两类状态：

| 状态族 | 语义 | 关键细节 |
|---|---|---|
| **P-states** | 正常运行时的性能档位，**靠改频率**实现 | P0 最高频（对部分 Intel CPU 是最高 turbo 档），P1…N 更低频；可由硬件（按温度）或软件（内核省电模式）控制；当前频率与可用档位可用 **MSR** 观察（见 §6.6.10 `showboost`） |
| **C-states** | 执行停止时的**空闲**状态，**省电** | C0 是正常运行；C1 及以上是空闲态，**编号越大睡得越深** |

**C-states 明细（原文 Table 6.2）**：

| C-state | 描述 |
|---|---|
| C0 | 执行中。CPU 全开，处理指令 |
| C1 | 停止执行。由 `hlt` 指令进入。**缓存保持**。唤醒延迟在此状态最低 |
| C1E | 增强停机，功耗更低（部分处理器支持） |
| C2 | 停止执行。由**硬件信号**进入。更深的睡眠，唤醒延迟更高 |
| C3 | 更深的睡眠，比 C1/C2 更省电。**缓存可能保持状态，但停止 snooping（缓存一致性），一致性维护交给 OS** |

厂商可以定义 C3 之外的状态；部分 Intel 处理器定义到 **C10**，关掉更多处理器功能（**含缓存内容**）。

### 缓存层级与延迟

处理器内（或旁）的缓存包含：**L1 指令缓存 (I$)**、**L1 数据缓存 (D$)**、**TLB**、
**L2 缓存 (E$)**、**L3 缓存（可选）**。

**术语陷阱**：`E$` 原本指 *external cache*，L2 集成之后改称 *embedded cache*；
现在通用说法是 **Level 1 / Level 2 / Level 3**，以避免这种混淆。
Intel 用 **last-level cache (LLC)** 指「主存前的最后一级缓存」，也叫 **longest-latency cache**。

**共享关系**：多核多线程处理器上，**部分缓存在 core 之间与线程之间共享**。
原文 Table 6.3 的 Intel 处理器演进表显示：**2008 年 Xeon 7460 之后都是多份 L1 与 L2（通常每 core 一份）**，
**表中的尺寸是每 core 的，不是总量**。

**延迟（这是本节最该记住的数字）**：

> The access time for the Level 1 cache is typically a few CPU clock cycles, and for the larger Level 2 cache around a dozen clock cycles. Main memory access can take around 60 ns (around 240 cycles for a 4 GHz processor), and address translation by the MMU also adds latency.

即：**L1 几个周期、L2 约十几个周期、主存约 60 ns（4 GHz 下约 240 周期）**，再加 MMU 地址转换的延迟。
**主存比 L1 慢两个数量级**——这就是「缓存命中率」在 CPU 分析里权重极高的原因。

**相联性（associativity）**：缓存定位新条目的约束方式，三种：

| 类型 | 机制 | 代价 |
|---|---|---|
| **Fully associative** | 条目可放在任何位置（如全缓存 LRU 淘汰） | 昂贵 |
| **Direct mapped** | 每条目只有一个合法位置（如用地址位做哈希） | 命中率差 |
| **Set associative** | 先映射出一个子集，再在子集内用另一算法（如 LRU）挑选；四路组相联 ⇒ 一个地址映射到 4 个候选位置 | **CPU 缓存常用** |

原文明确：CPU 缓存常用**组相联**，因为它是全相联（太贵）与直接映射（命中率差）之间的平衡。
**缓存行（cache line）** 典型是 **64 字节**（x86），编译器会据此优化。

### 缓存一致性

内存可能同时缓存在不同处理器的多个缓存里。某个 CPU 修改内存时，所有缓存都必须知道自己的副本已过期并丢弃——
这个过程叫 **cache coherency**，保证 CPU 总是读到正确的内存状态。

**它带来 LLC 访问惩罚**（原文给的粗略参考值）：

| 场景 | 代价 |
|---|---|
| LLC 命中，行**未被共享** | ~40 CPU 周期 |
| LLC 命中，行**被另一 core 共享** | ~65 CPU 周期 |
| LLC 命中，行**被另一 core 修改** | ~75 CPU 周期 |

**机制**：共享/被修改的行会触发跨核通信，代价接近翻倍。原文还定性说：
**缓存一致性是设计可扩展多处理器系统最大的挑战之一**，因为内存可以被快速修改。

**判据**：多核负载里出现「单核跑得好、多核反而更慢」时，**先怀疑伪共享与一致性流量**，
而不是直接上「减少线程」。

### MMU 与地址转换

**MMU** 负责虚拟地址到物理地址的转换。片上 **TLB** 缓存地址转换；
**TLB 未命中**由主存中的**页表**满足——页表由 **MMU（硬件）直接读取，由内核维护**。

**代际差异**：老处理器用**内核软件**走页表并填充 TLB，软件还可以维护自己的、更大的内存转换缓存
（**TSB，translation storage buffer**）；**新处理器用硬件处理 TLB 未命中**，成本大幅下降。

**判据**：TLB 未命中率高的负载（大量随机地址访问、页表项多）值得单独看；
**大页**是最常见的缓解手段。

### 互连（Interconnects）

多处理器系统用**共享系统总线**或**专用互连**连接，与内存架构（UMA / NUMA，第 7 章）直接相关。

- **前端总线（FSB）**：早期 Intel 处理器用的共享总线，**处理器数量一多就有扩展性问题**——共享资源争抢。
- **现代服务器**通常是多处理器 + NUMA + **CPU 互连**（Intel QPI / UPI、AMD HyperTransport、ARM CoreLink、IBM CAPI）。
  **私有连接**允许无争抢访问，带宽也高于共享总线。

**带宽怎么算（原文用 QPI 举例，照抄其口径）**：QPI 是 **double-pumped**（在时钟的上升沿与下降沿都传数据），
3.2 GHz × 2 = **6.4 GT/s**；再乘以 2 字节宽度与 2 个方向 ⇒ **25.6 GB/s**。
（对照表：FSB 12.8 GB/s、QPI 25.6 GB/s、UPI 41.6 GB/s。）

**互连的失败信号**：

> Interconnects are typically designed for high bandwidth, so that they do not become a systemic bottleneck. If they do, performance will degrade as CPU instructions encounter stall cycles for operations that involve the interconnect, such as remote memory I/O. A key indicator for this is a drop in IPC.

**机制**：互连一旦成为系统瓶颈，涉及互连的操作（如**远程内存 I/O**）会产生停顿周期；
**指标是 IPC 下降**——而且此时 CPU 利用率往往还很高。

**判据**：**「利用率高 + IPC 下降 + 停顿周期集中在内存/总线」⇒ 互连或 NUMA 放置问题**，
不是算力不足。典型动作是绑核（taskset）或改 NUMA 策略，而不是买更快的 CPU。

### 硬件计数器（PMCs）

**PMCs** 是**硬件实现的处理器寄存器**，可编程以统计底层 CPU 活动，通常包括：

- **CPU 周期**（含停顿周期与各类停顿类型的细分）
- **CPU 指令**（retired / 已执行）
- **L1 / L2 / L3 缓存访问**（命中、未命中）
- **浮点单元**（操作数）
- **内存 I/O**（读、写、停顿周期）
- **资源 I/O**（读、写、停顿周期）

**关键约束（这条决定了很多分析能不能做）**：

> Each CPU has a small number of registers, usually between two and eight, that can be programmed to record events like these. Those available depend on the processor type and model and are documented in the processor manual.

**每颗 CPU 只有 2~8 个可编程计数器寄存器**，具体能测什么**取决于处理器型号**，必须查该型号的处理器手册。
Intel P6 家族用 4 个 **MSR** 实现：2 个是只读计数器（40-bit），2 个是读写的事件选择 MSR（32-bit）。
计数器由 **event select + UMASK** 共同标识；**OS / USR 位**可限定只在内核态 / 用户态计数；
**CMASK** 可设定「累计到阈值才计数」。Skylake 微架构则是**每硬件线程 3 个固定计数器 + 每 core 8 个可编程计数器**，读取时是 48-bit。

**判据**：**先查手册再设计测量方案**。计数器数量是硬约束，测不了的事件就得换指标
（例如用 `perf` 的软件事件或时间戳倒推），而不是反复重跑。

### GPU 与其他加速器

GPU 现在也跑 AI / 机器学习 / 分析 / 图像处理 / 挖矿。一个 GPU 可能含**数百上千个更小的 core**
（NVIDIA 叫 **streaming processor, SP**），每个 SP 执行一个线程；可并行的线程被分成 **thread blocks**
（块内可协作），thread blocks 由 **streaming multiprocessor (SM)** 这样的 SP 分组执行，SM 还提供缓存等资源。

原文 Table 6.6 的要点对比：CPU 封装插在主板 socket 上、典型 2~64 核、每核可执行 2 个硬件线程、
时钟高（如 3.4 GHz）；GPU 通常是插在扩展总线（如 PCIe）上的扩展卡、可有多卡互连（如 NVLink）、
时钟相对低（如 1.0 GHz）、每个 SP 只能执行一个线程。

**观测 GPU 必须用专门的工具**（可用的指标如 instructions per cycle、cache hit ratio、内存总线利用率）。
另外还有 **FPGA、TPU** 等加速器，它们也需要专门工具，通常要与 CPU 一起分析。

## §6.4.2 软件：内核调度器与调度类

### 调度器的三个职能

| 职能 | 机制 |
|---|---|
| **Time sharing** | 在可运行线程间分时，**优先跑优先级最高的** |
| **Preemption** | 高优先级线程变可运行时，抢占当前线程，让高优先级工作立即开始 |
| **Load balancing** | 把可运行线程**迁移到空闲或不忙的 CPU 的 run queue** |

**Linux 的对应实现（原文点名）**：时间共享由**系统时钟中断**驱动 → 调用 `scheduler_tick()` →
调用调度类函数管理优先级与**时间片**（CPU 时间单位）的到期；抢占在线程变可运行时触发，
调用 `check_preempt_curr()`；线程切换由 `__schedule()` 管理，经 `pick_next_task()` 选出最高优先级线程；
负载均衡由 `load_balance()` 完成。

**注意 run queue 已不是队列**：

> Figure 6.12 shows run queues, which is how scheduling was originally implemented. The term and mental model are still used to describe waiting tasks. However, the Linux CFS scheduler actually uses a red/black tree of future task execution.

（原文也提醒：这些函数名可能变化，以 Linux 源码与 `Documentation` 目录为准。）

### 调度类与调度策略

**调度类**管理可运行线程的行为：优先级、是否被时间切片、时间片时长（即 time quantum）。
**调度策略**是在调度类内选择的额外控制，可以管同优先级线程之间的调度。

Linux 的调度类（原文列举）：

| 调度类 | 说明 |
|---|---|
| **RT** | 实时负载的固定高优先级；内核支持用户态与内核态抢占，RT 任务低延迟分发；优先级范围 **0–99**（MAX_RT_PRIO-1） |
| **O(1)** | Linux 2.6 引入的默认时间共享调度器；名字来自算法复杂度 O(1)（此前调度器遍历所有任务，是 O(n)，成为扩展性问题）；动态提升 I/O 密集相对 CPU 密集的优先级，降低交互式与 I/O 负载的延迟 |
| **CFS** | Linux **2.6.23** 加入的默认时间共享调度器 |
| **Idle** | 跑优先级最低的线程 |
| **Deadline** | Linux **3.14** 加入，用最早截止时间优先（EDF），三个参数：**runtime**、**period**、**deadline**——任务应在每个 period 内获得 runtime 微秒的 CPU 时间，并在 deadline 内完成 |

**CFS 的机制（原文最关键的机制句）**：

> The scheduler manages tasks on a red-black tree keyed from the task CPU time, instead of traditional run queues.

即 **CFS 不用传统 run queue，而是用「按任务 CPU 时间排序的红黑树」**——
这样**低 CPU 消费者容易被找到并优先执行**，从而改善交互式与 I/O 密集负载的性能。

**Linux 调度策略**：

| 策略 | 语义 | 所属类 |
|---|---|---|
| **RR**（SCHED_RR） | 轮转；用完时间片后移到该优先级队列末尾 | RT |
| **FIFO**（SCHED_FIFO） | 先进先出；队首线程一直跑到主动让出或更高优先级线程到来，**同优先级线程在队列里也不打断它** | RT |
| **NORMAL**（SCHED_NORMAL，原 SCHED_OTHER） | 时间共享，用户进程默认；O(1) 的时间片按静态优先级设定（优先级高则片长），CFS 的时间片是动态的 | CFS |
| **BATCH**（SCHED_BATCH） | 类似 NORMAL，但预期是 CPU 密集，不该打断 I/O 密集的交互式工作 | CFS |
| **IDLE**（SCHED_IDLE） | 最低优先级 | Idle |
| **DEADLINE**（SCHED_DEADLINE） | EDF | Deadline |

选类的方式：用户态进程选一个**映射到调度类的策略**，用 `sched_setscheduler(2)` 或 `chrt(1)`。

**nice 与静态/动态优先级**：用户态线程的优先级受 **nice 值**影响。Linux 里 nice 值设定的是线程的
**静态优先级**，与调度器计算的**动态优先级**是**分开的**。

### 空闲线程、NUMA 分组与处理器资源感知

**空闲线程（idle thread / idle task）**：没有其他可运行线程时在 CPU 上跑，优先级最低；
通常被编程成通知处理器可以**停机（halt 指令）**或**降频**以省电；CPU 会被**下一次硬件中断**唤醒。

**NUMA 分组**：让内核 **NUMA-aware** 可以显著改善 NUMA 系统性能——内核自动探测并创建**本地化的 CPU 与内存资源组**，
按反映 NUMA 架构的拓扑组织，从而**能估算任意内存访问的成本**。Linux 上这些叫 **scheduling domains**，
拓扑的顶端是 **root domain**。管理员也可以手动分组：把进程绑到特定 CPU，或为进程创建**独占 CPU 集**（§6.5.10）。

**处理器资源感知**：内核也可以理解 CPU 资源拓扑，从而在**电源管理、硬件缓存使用、负载均衡**上做更好的调度决策。

## §6.5 方法论：CPU 分析的顺序

### 建议顺序

> My suggestion is to use the following, in this order: performance monitoring, the USE method, profiling, micro-benchmarking, and static performance tuning.

原文把这称为「cookbook of recipes」，可单独用也可组合用，并明确说**不需要全用**。

### §6.5.1 Tools Method：逐工具过一遍

流程是「迭代可用工具，检查它们提供的关键指标」。对 CPU 而言，原文列的检查清单（Linux）：

| 工具 | 看什么 |
|---|---|
| `uptime` / `top` | 负载均值在涨还是在落（**分析期间负载可能在变**） |
| `vmstat` | 跑 1 秒间隔，看系统级 CPU 利用率（`us` + `sy`）；接近 100% 则更可能出现调度延迟 |
| `mpstat` | per-CPU 统计，找**热 CPU**，识别可能的**线程扩展性问题** |
| `top` | 哪些进程、哪些用户是 CPU 大户 |
| `pidstat` | 把 CPU 大户拆成用户态与系统态 |
| `perf` / `profile` | 剖析用户态与内核态的 CPU 栈，解释 CPU 为什么被用 |
| `perf` | 测 **IPC** 作为周期级低效的指标 |
| `showboost` / `turbostat` | 检查当前 CPU 时钟频率是否异常低 |
| `dmesg` | 查 CPU 温度节流消息（`cpu clock throttled`） |

**这个方法的两个缺陷**（原文自己说）：**对工具可见性差或完全没有可见性的问题会漏掉**，而且**很费时间**。

### §6.5.2 USE Method：先广后深

USE = **Utilization / Saturation / Errors**，用于在深入之前**尽早**找出瓶颈与错误。

**先查 Errors**——因为它通常最快查、最容易解释。处理器与操作系统会感知 **ECC 可纠正错误**的增加，
并**预防性地把 CPU 下线（offline）**，以免可纠正错误演变成 CPU 失效。
检查错误有时只需**确认所有 CPU 仍在线**。

**Utilization** 通常直接来自操作系统的 percent busy，但**必须按 CPU 分别看**，以发现扩展性问题。

**云上的关键限定**：

> CPU utilization should be measured in terms of the imposed limit, in addition to the physical limit.

**配额环境（Linux taskset、cgroups）里，利用率要同时按「施加的限制」和「物理限制」两个口径看**——
**你的系统可能在物理 CPU 远未到 100% 时就耗尽了配额**，比预期更早遇到饱和。

**Saturation** 指标通常系统级提供，也包含在负载均值里；它量化的是「CPU 超载程度」或「配额被用光的程度」。
GPU 与其他加速器可以按同样的 USE 流程检查（取决于可用指标）。

### §6.5.3 负载刻画（Workload Characterization）

**刻画负载**对容量规划、基准测试与负载模拟都很重要，还可能因为**识别出可以消除的多余工作**而带来最大的性能收益。

**基本属性**：

| 属性 | 含义 |
|---|---|
| CPU 负载均值 | 利用率 + 饱和度 |
| 用户态/系统态时间比 | 负载类型 |
| 系统调用率 | 内核活动强度 |
| 主动上下文切换率 | 阻塞/让出频率 |
| 中断率 | 硬件事件强度 |

**最重要的一句限定**：

> The intent is to characterize the applied load, not the delivered performance.

**要刻画的是「施加的负载」，不是「交付的性能」。** 原因原文给在脚注里：
速率类指标**同时反映施加的负载与一部分交付性能**——交付得快，速率自然被节流
（例：同一批处理负载在更快的 CPU 上系统调用率更高，因为它完成得更早）。

**原文给的示例描述**（本篇照搬其口径）：一台平均 48-CPU 的应用服务器，白天负载均值在 **30 到 40** 之间；
用户/系统比 **95/5**（CPU 密集型负载）；约 **325 K 次系统调用/秒**；约 **80 K 次主动上下文切换/秒**。

**进阶清单**（原文列为「思考问题」，也可当检查表）：
系统级 / 每 CPU / 每 core 的利用率是多少？负载有多并行（单线程？多少线程？）？
哪些应用或用户在用 CPU、用了多少？哪些内核线程在用、用了多少？中断占多少 CPU？
CPU 互连利用率多少？CPU 为什么被用（用户态与内核态调用路径）？遇到了哪些类型的停顿周期？

### §6.5.4 剖析（Profiling）

两种 CPU 剖析方式：

| 方式 | 机制 | 粒度 | 代价 |
|---|---|---|---|
| **定时采样** | 按固定速率采集当前函数或栈回溯。**每 CPU 典型 99 Hz** | 粗，但大、小问题都能看出来 | 低；可用更低频率 + 更大时间窗，直到开销可忽略、可用于生产 |
| **函数追踪** | 给全部或部分函数调用插桩计时 | 细 | **常在 10% 或以上**，生产环境往往不可接受 |

**为什么是 99 而不是 100**：原文明确说 **100 Hz 可能产生 lock-step 采样，导致 profile 偏斜**，所以用 99 Hz。

**采样的三个先天局限**（原文图 6.14 说明）：

1. **只在进程 on-CPU 时采样**——syscall 期间的 off-CPU 时间**不会被采到**。
2. **短命函数会被完全漏掉**（图中函数 C 一次都没被采到）。
3. 只采**指令指针**也可以（显示 on-CPU 函数与指令偏移），有时这就够了，能省掉采栈的开销。

**完整 CPU profile 必须同时记录用户栈与内核栈**（内核通常维护两份栈回溯）。

**两个现实挑战**（原文以 Netflix 的规模举例：**32 CPU、49 Hz、30 秒 ⇒ 47,040 个样本**）：

- **存储 I/O**：采样写进 profile 文件会产生**扰动生产应用**的存储 I/O。
  **BPF 版的 `profile(8)` 在内核内存里汇总样本、只输出汇总，不产生中间文件**。
- **理解成本**：逐条读 47,040 个多行栈回溯不现实，必须用汇总与可视化（**火焰图**）。

**两者的取舍**：BPF 方案开销更低；**`perf(1)` 方案保存原始样本（含时间戳），可以用不同工具重新处理**（包括 FlameScope）。

**火焰图解读三步**（原文作者自己的方法）：

1. **自上而下（叶到根）找大的「平台」**——单个函数在大量样本里 on-CPU，往往是快赢。
   原文例：图 6.16 右侧有两个平台在 `unmap_page_range()` 与 `page_remove_rmap()`，都与内存页有关 →
   **可能的快赢是改用大页**。
2. **自下而上理解代码层次**——原文例：`bash(1)` 调 `execve(2)` 最终调到页函数 →
   **更大的赢是避免 execve(2)**（比如用 bash 内建命令，或换语言）。
3. **再自上而下看散落但常见的 CPU 占用**——可能是同一个问题的许多小帧（如锁争用）。
   **把火焰图的合并顺序反转成 icicle graph**（从叶到根合并）有助于暴露这类情况。

### §6.5.5 周期分析（Cycle Analysis）

用 **PMCs** 在周期级理解 CPU 利用率：能看出周期是花在 **L1/L2/L3 缓存未命中、内存 I/O、资源 I/O** 上，
还是花在浮点运算或其他活动上——这些信息能指引**编译器选项**与**代码改动**。

**起点只有一个指标**：

> Begin cycle analysis by measuring IPC (inverse of CPI). If IPC is low, continue to investigate types of stall cycles. If IPC is high, look for ways in the code to reduce instructions performed.

**IPC 低 ⇒ 查停顿周期的类型；IPC 高 ⇒ 查代码里能减少的指令数量。**
（原文给的量级参考：低可低于 0.2，高可高于 1；具体数值靠已知负载实测。）

**溢出采样（overflow sampling）**：PMC 可以配置成**溢出时中断内核**——
例如每 10,000 次 L3 缓存未命中就中断一次、取一次栈回溯。
内核随时间积累出「哪些代码路径导致 L3 未命中」的 profile，**避免了逐次测量的高昂开销**。
这主要用于 IDE 给代码标注「造成内存 I/O 与停顿周期的位置」。

**但溢出采样有已知缺陷**：会因 **skid（滑移）** 与**乱序执行**而**记错指令**；
Intel 上的解法是 **PEBS**（Linux `perf(1)` 支持）。

**成本预期**：周期分析是**高级活动**，用命令行工具可能**要花几天**，还得配时间读处理器手册；
**Intel vTune / AMD uprof** 这类分析器能替你找出关心的 PMC。

### §6.5.6 性能监控

关键指标就两个：**Utilization**（percent busy）与 **Saturation**（run-queue 长度或调度延迟）。
利用率要**按 CPU 监控**以发现线程扩展性问题；配额环境还要记录**相对配额的用量**。

**采样间隔的选择是本章最实用的一条经验**：

- 有些工具用 **5 分钟间隔**，这会**掩盖更短的 CPU 利用率突发**。
- **每秒测量更好**，但**一秒之内也可能有突发**——这些要靠**饱和度**识别，再用 **FlameScope** 细查（§6.7.4）。

### §6.5.7–6.5.11 静态调优、优先级、资源控制、绑核、微基准

**静态性能调优**看的是「配置环境」：有多少 CPU 可用（core 还是硬件线程）？有没有 GPU/加速器？
单处理器还是多处理器？缓存多大、是否共享？时钟频率多少、是否动态（Turbo Boost / SpeedStep）、
这些动态特性**在 BIOS 里是否启用**？其他 BIOS 里的 CPU 相关特性开关？该型号有没有性能 bug（在 errata 里）？
microcode 版本、有没有含影响性能的安全缓解措施（Spectre/Meltdown）？BIOS 固件有没有性能 bug？
**有没有软件施加的 CPU 使用限制？**（云环境里这条尤其常中。）

**优先级调优**：`nice(2)` 设 nice 值（正数降优先级，负数升优先级，只有 root 能设负数；
Linux 2.6.12 起可用 nice ceiling 让非 root 进程获得较低的 nice 值）。
**最有价值的用法不是「提升业务进程」，而是「降低低价值工作的优先级」**——
监控 agent、定时备份这类东西降权后，高优先级工作的调度延迟就降下来了。
调完还要**验证**：检查高优先级工作的调度延迟是否确实保持低位。

**实时调度类的代价**：Linux 的实时调度类能让进程抢占所有其他工作，
**消除调度延迟**（除其他实时进程与中断外）——但**必须理解后果**：
若实时应用出 bug、多个线程陷入死循环，**所有 CPU 会变得对其他所有工作不可用**，
**连用来手动修复问题的管理 shell 都跑不了**。Linux 从 2.6.25 起的解法是 **RLIMIT_RTTIME**，
限制实时线程在做出阻塞系统调用前可消耗的 CPU 时间（微秒）。

**资源控制**：操作系统可对**进程或进程组**做细粒度 CPU 分配，包括**固定上限**与**份额**
（份额方式更灵活——空闲的 CPU 周期可以按份额被消费）。

**CPU 绑定（§6.5.10）**：把进程/线程绑到单个 CPU 或一组 CPU，可以**提升缓存热度**从而改善内存 I/O 性能；
在 NUMA 系统上还改善内存局部性。两种做法：

| 做法 | 机制 | 效果 |
|---|---|---|
| **CPU 绑定** | 进程只跑在某个 CPU，或某组定义的 CPU 上 | 提升缓存热度 |
| **独占 CPU 集** | 划出一组 CPU **只给指定进程用** | 缓存热度更高——**进程空闲时别的进程也用不了这些 CPU** |

Linux 上独占 CPU 集用 **cpusets** 实现（配置示例在 §6.9.7）。

**微基准（§6.5.11）**：测「某个简单操作重复很多次」的时间。操作对象可以是 **CPU 指令**
（整数、浮点、内存读写、分支）、**内存访问**（查各级缓存延迟与主存吞吐）、
**高层语言**（解释型或编译型）、**操作系统操作**（如 `getpid(2)`、进程创建这类 CPU 密集的系统调用）。

**历史**：Whetstone（1972，Algol 60，模拟科学计算）、Dhrystone（1984，模拟整数负载）、
UnixBench（Monash 大学，出自 BYTE 杂志）、以及测压缩、质数、加密、编码的现代基准。

**最重要的陷阱**：

> Whichever benchmark you use, when comparing results between systems, it’s important that you understand what is really being tested. Benchmarks like those listed earlier often end up testing differences in compiler optimizations between compiler versions, rather than the benchmark code or CPU speed.

**这类基准最后测的往往是「编译器优化版本之间的差异」，而不是基准代码或 CPU 速度。**
另外：**很多基准是单线程的，在多 CPU 系统上其结果失去意义**——
四 CPU 系统可能比八 CPU 系统测得略快，但后者在有足够并行可运行线程时吞吐会大得多。

## §6.6 可观测工具（Linux）

原文 §6.6 逐个讲了 21 个工具。本篇按「**系统级 → per-CPU → 剖析 → 专项**」重排。

### §6.6.1 uptime 与负载均值

`uptime` 打印三个负载均值（1、5、15 分钟）。**比较这三个数**能判断过去约 15 分钟负载在涨、在落还是持平：
**如果正在处理生产性能问题且发现负载在下降，你可能已经错过了问题；负载在涨，则问题可能正在恶化。**
原文提醒：负载均值只是起点，**不该花超过五分钟在它上面**。

**负载均值的语义（本章最容易被误用的一条）**：

> But in 1993, Linux changed load averages to show system-wide demand: CPUs, disks, and other resources.

**1993 年 Linux 把负载均值从「只显示 CPU 需求」改成「显示系统级需求」**——
CPU、磁盘和其他资源。实现方式是把处于 **TASK_UNINTERRUPTIBLE** 状态的线程也计入
（某些工具显示为状态 **D**）。

**负载 = 当前资源使用（利用率）+ 排队请求（饱和度）**——原文用**收费站**打比方：
数正在被服务的车（利用率）加上排队的车（饱和度）。

**平均算法是指数阻尼移动平均**，它反映的是 1/5/15 分钟**之外**的负载
（这三个时间其实是指数移动和里的常数）。原文的实验：启动一个 CPU 密集线程后，
**到 1/5/15 分钟刻度时，三个负载均值达到已知负载 1.0 的约 61%**。

**现代解读示例**：64 CPU 系统负载均值 128 —— 若负载全是 CPU，意味着**平均每个 CPU 上都有一个线程在跑、
还有一个在等**；同系统负载均值 20 则说明**还有 44 个 CPU 密集线程的余量**。
（有些公司监控**归一化负载均值**，自动除以 CPU 数。）

### PSI：负载均值的现代替代

**PSI（Pressure Stall Information）** 是 Linux **4.20** 新增的接口，
提供 **CPU、内存、I/O 各自的平均值**；平均值表示**因某资源而停顿的时间百分比**（**只有饱和度**）。

| 属性 | 负载均值 | PSI |
|---|---|---|
| 资源 | 系统级整体 | cpu、memory、io **各自独立** |
| 指标 | 忙着的任务数 + 排队的任务数 | **停顿（等待）时间百分比** |
| 时间粒度 | 1 min、5 min、15 min | 10 s、60 s、300 s |
| 平均方式 | 指数阻尼移动和 | 指数阻尼移动和 |

**对照场景（这张表是 PSI 最有说服力的地方）**：

| 场景 | 负载均值 | PSI |
|---|---|---|
| 2 CPU，1 个忙线程 | 1.0 | 0.0 |
| 2 CPU，2 个忙线程 | 2.0 | 0.0 |
| 2 CPU，3 个忙线程 | 3.0 | **50.0** |
| 2 CPU，4 个忙线程 | 4.0 | **100.0** |
| 2 CPU，5 个忙线程 | 5.0 | 100.0 |

**负载均值看不出「排队」与「饱和」的区别，PSI 能**：2 CPU 上跑 2 个线程时负载均值已经是 2.0，
但 PSI 是 0.0——**没有任何停顿发生**；到 3 个线程时 PSI 跳到 50%。
原文的措辞：PSI 最能回答「**一个任务有多大概率不得不等待资源**」这个问题。

**读法**：`cat /proc/pressure/cpu` 输出的 `some avg10=50.00 ...` 里的 50.0
表示某个线程（`some`）**有 50% 的时间处于停顿**。io 与 memory 指标还有第二行 `full`，
表示**所有非 idle 线程都停顿**的时间占比。

### §6.6.2 vmstat

`vmstat` 的 CPU 相关列：`r`（**run-queue 长度 = 可运行线程总数**）、`us`（用户态 %）、
`sy`（系统态 %）、`id`（空闲 %）、`wa`（**I/O 等待 %** —— 线程阻塞在磁盘 I/O 上的 CPU 空闲时间）、
`st`（**被窃 %** —— 虚拟化环境里为其他租户服务的时间）。

**注意口径**：除 `r` 是总数外，其余都是**全系统跨所有 CPU 的平均值**。
**Linux 上 `r` = 等待的 + 运行的**；其他操作系统（如 Solaris）的 `r` **只算等待的**。
（1979 年 Bill Joy 与 Ozalp Babaoglu 的 3BSD 原版 `vmstat` 开头的 `RQ` 列就是「可运行 + 运行」。）

### §6.6.3 mpstat / sar / ps / top / pidstat

**`mpstat -P ALL`** 报告 per-CPU 统计（默认只打印系统级 `all` 汇总行）。列的含义：
`%usr`（用户态，不含 %nice）、`%nice`（被 nice 过的进程的用户态）、`%sys`（系统态）、
`%iowait`（I/O 等待）、`%irq`（硬件中断）、`%soft`（软件中断）、`%steal`（为其他租户服务的时间）、
`%guest`（客户虚拟机里的 CPU 时间）、`%gnice`（被 nice 过的客户虚拟机时间）、`%idle`（空闲）。

**关键列是 `%usr` / `%sys` / `%idle`**：它们既给出每 CPU 的用量，也给出用户态/内核态比（§6.3.9）。
**它们还能识别「热 CPU」**——某几个 CPU 100% 占用而其他不满，
成因通常是**单线程应用负载**或**设备中断映射**。

**`sar`** 的 CPU 相关选项：`-P ALL`（同 mpstat）、`-u`（系统级平均）、`-q`（含 run-queue 大小 `runq-sz` 与负载均值）。
**`ps` / `top` / `pidstat` / `time` / `ptim`** 覆盖进程级视角；
**`turbostat`** 读 MSR 给频率与 C-state 停留时间；**`showboost`** 给当前频率与可用 P-state。

### §6.6.13–6.6.21 剖析与专项工具

| 工具 | 定位 |
|---|---|
| **`perf`** | 一站式：one-liners、system-wide CPU profiling、CPU 火焰图、进程级 profiling、**调度延迟（`perf sched`）**、**硬件事件（`perf stat` / `perf list`）**、软件追踪、硬件追踪 |
| **`profile`** | BPF 版 CPU profiler：**内核内存里汇总样本、不落中间文件**（§6.5.4）；支持生成 CPU 火焰图 |
| **`cpudist`** | 按 **on-CPU 时间**分布统计（直方图） |
| **`runqlat`** | **run-queue 延迟**：任务在 run queue 上等了多久才被调度 |
| **`runqlen`** | run queue 长度随时间的变化 |
| **`softirqs` / `hardirqs`** | **软件中断 / 硬件中断**各自的 CPU 占用（对应 §6.5.3 清单里的「中断占多少 CPU」） |
| **`pmcarch`** | 原始 PMC 事件计数 |
| **`tlbstat`** | TLB 相关统计 |
| **`bpftrace`** | 通用 BPF 追踪；可直接做 profile、sched tracepoint、kprobe |
| **`cpupower`** | C-state / 频率等电源相关（`cpupower idle-info`） |

## §6.7 可视化

### §6.7 为什么不用线图

CPU 用量的经典画法是利用率或负载均值的**线图**（如 X11 的 `xload`），
它擅长展示**变化幅度**与**时间上的模式**。

**但它撑不起今天的 CPU 规模**：每 CPU 一条线，在动辄上万个 CPU 的云环境里，**10000 条线的图就是一团油漆**。
平均值、标准差、最大值、百分位数这些统计量确实能缩放，**但 CPU 利用率常是双峰的**
——一部分 CPU 空闲或近空闲、一部分 100%——**统计量表达不了这种分布**，必须看完整分布。

### §6.7.1 利用率热力图

把「利用率 × 时间」画成热力图，**每个像素的饱和度表示该利用率、该时间范围内有多少个 CPU**。
原文例：整个数据中心、300 多台物理服务器、**5312 个 CPU** 的热力图——
底部较浅说明**大部分 CPU 在 0%~30%**；顶部的实线说明**始终有一些 CPU 在 100%**，
而那条线是**深色**说明**100% 的不止一个 CPU**。

### §6.7.2 子秒偏移热力图

CPU 活动通常以微秒或毫秒计量，**报成整秒平均会把有用信息抹掉**。
子秒偏移热力图把**秒内偏移放在 y 轴**，用饱和度表示每个偏移上的非 idle CPU 数量，
**把每一秒画成一列，自下而上「涂」出来**。

原文的 Riak 云数据库案例：有意思的不是 CPU 忙的那些时段，而是**没忙的那些时段——白色柱**；
**白色柱的时长（几百毫秒内没有任何数据库线程在 CPU 上）**才是线索，
最终查出**整个数据库被一把锁阻塞了几百毫秒**。

**原文的结论值得单独记住**：如果用线图看这份数据，**每秒利用率的凹陷会被当成负载波动而略过**。

### §6.7.3 火焰图

火焰图把 profile 的栈帧可视化，让「哪些内核态或用户态代码路径在消耗 CPU」一眼可见
（原始 profile 可能有好几千页）。可由 **`perf(1)` / `profile(8)` / `bpftrace`** 等任何带栈回溯的 CPU profile 生成。

**坐标轴的语义（最常见的误解点）**：

> The x-axis spans the sample population. It does not show the passing of time from left to right, as most graphs do. The left-to-right ordering has no meaning (it’s sorted alphabetically).

**x 轴是样本总体，不是时间**；**左右顺序没有含义（按字母序排）**。
**y 轴是栈深度**（栈上帧数），**最上面的框是当时 on-CPU 的函数**，其下是祖先。

**箱宽 = 该函数 on-CPU 的时间占比**（含「作为祖先在栈上时」的时间，按样本数计）。
宽的函数可能更慢，**也可能只是被调用得更多**——**调用次数在采样里是看不到的**。
另外，**样本数可以超过实际流逝时间**，因为多线程可能被并行采样。

**颜色语义**（作者给出的实用方案）：**hue 表示代码类型**（红=原生用户态、橙=原生内核态、黄=C++、
绿=解释型函数、青=内联函数等；洋红用于高亮搜索命中）；
**saturation 由函数名哈希得到**，给相邻塔一些颜色差异，同时**同名函数保持同色**以便跨图比较；
**背景色用来提示图的类型**（黄=CPU 火焰图、蓝=off-CPU 或 I/O 火焰图、绿=内存火焰图）。
另有 **IPC 火焰图**：用蓝→白→红的渐变给每帧上色，把 IPC 作为额外维度。

**交互**：原版 `flamegraph.pl` 生成带内嵌 JavaScript 的 SVG，浏览器里可悬停看细节、点击缩放、
**Ctrl-F 搜索**并显示**累计百分比**（含搜索词的栈回溯占比）——例如搜 `tcp_` 就能知道内核 TCP 代码占多少。

**解读方法**：**先看最宽的塔**，先理解它们。
原文例：路径 `func_a() -> func_b() -> func_c()`。
**对递归函数，每一层单独显示一个框**。
（大规模 profile 里很多路径只被采到几次，塔窄到放不下函数名——原文说这反而是好事：
**注意力自然被引向宽塔**，而宽塔才是 profile 的主体。）

### §6.7.4 FlameScope

**FlameScope**（Netflix 开源）把前面两种可视化合起来：**子秒偏移热力图 + 火焰图**。
在热力图上**选择包含子秒范围的时间段**，就只显示那段时间的火焰图。

**它解决的是「扰动太小看不见」的问题**：

> FlameScope is suited for studying issues or perturbations and variance. These can be too small to see in a CPU profile, which shows the full profile at once: a 100 ms CPU perturbation during a 30-second profile will only span 0.3% of the width of a flame graph.

**30 秒 profile 里的 100 ms 扰动，在火焰图上只占 0.3% 的宽度**（几乎不可见）；
在 FlameScope 里它是一条**热力图 1/10 高的竖条**，选中后就能看到对应的代码路径。

**判据**：**「某个时间点上好像有点不对」但火焰图上什么都看不到** ⇒ 用 FlameScope 按秒内偏移切，
不要试图在整幅火焰图里找。

## §6.8 实验

用这些工具时，原文建议**一直开着 `mpstat`**，以确认 CPU 使用与并行度。

**§6.8.1 Ad Hoc**：一个**不测量任何东西**但有用的已知负载，用来**确认观测工具确实显示了它声称显示的东西**——
单线程、CPU 密集（「一个 CPU 是热的」）：

```bash
# while :; do :; done &
```

**§6.8.2 SysBench**：简单的 CPU 基准工具，算质数：

```bash
# sysbench --num-threads=8 --test=cpu --cpu-max-prime=100000 run
```

**判据**：**在相信任何观测结论之前，先跑一次已知负载**——
如果工具报不出「一个 CPU 100%」，那它报的其它数字也不可信。

## §6.9 调优

> For CPUs, the biggest performance wins are typically those that eliminate unnecessary work, which is an effective form of tuning.

**CPU 上最大的性能收益通常来自消除多余的工作**，这本身就是一种调优。
原文把优先级调优与 CPU 绑定也算作调优方法。

**前提**：具体有哪些选项、设成什么，取决于**处理器类型、操作系统版本与预期负载**。

| 调优面 | 内容 |
|---|---|
| **§6.9.1 编译选项** | 64 位 vs 32 位、优化级别等 |
| **§6.9.2 调度优先级与类** | `nice -n 19 command`；`chrt -b command`（SCHED_BATCH）；`renice(1)` 改运行中进程；`setpriority(2)` / `sched_setscheduler(2)` 直接设 |
| **§6.9.3 调度器选项** | 编译期 CONFIG 选项（`CONFIG_PREEMPT` / `CONFIG_PREEMPT_VOLUNTARY` / `CONFIG_PREEMPT_NONE` / `CONFIG_HZ` / `CONFIG_NO_HZ` / `CONFIG_SCHED_SMT` / `CONFIG_SCHED_MC` 等）+ 运行期 sysctl（`kernel.sched_cfs_bandwidth_slice_us=5000`、`kernel.sched_latency_ns=12000000`）。原文提醒：**内核可调项通常并不需要调** |
| **§6.9.4 Scaling Governors** | 见下 |
| **§6.9.5 Power States** | `cpupower(1)` 禁用深睡眠态（`-d` 禁单个；`-D latency` 禁所有退出延迟大于给定微秒数的状态） |
| **§6.9.6 CPU 绑定** | `taskset -pc 7-10 10790`；`numactl(8)` 还能绑内存节点 |
| **§6.9.7 独占 CPU 集** | cpuset（`mount -t cgroup -ocpuset cpuset /sys/fs/cgroup/cpuset`）；还要看**中断会落在哪些 CPU**（`irqbalance` 与 `/proc/irq/*/smp_affinity`） |
| **§6.9.8 资源控制** | cgroups 的 **shares**，以及 CFS 的 **CPU bandwidth**（按间隔分配微秒级固定上限） |
| **§6.9.9 安全启动选项** | `nospectre_v1` / `nospectre_v2` 等 grub 参数。原文**明确说因为有安全风险而不推荐**，只提示它们存在 |
| **§6.9.10 BIOS 处理器选项** | 默认就是最高性能，通常不需要改；作者现在唯一常改的是**关闭 Intel Turbo Boost**，让 CPU 基准跑在**一致的时钟频率**上（生产环境应开启 Turbo Boost，性能略好） |

**Scaling Governors 的要点**（Linux 通过内核软件控制 CPU 频率）：

```bash
# cat /sys/devices/system/cpu/cpufreq/policy0/scaling_available_governors
performance powersave
# cat /sys/devices/system/cpu/cpufreq/policy0/scaling_governor
powersave
```

这是**未调优系统**的典型样子：当前 governor 是 `powersave`，会用更低频率省电。
改成 `performance`：

```bash
# echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor
```

**必须对所有 CPU 都做**（policy0..N）；同一目录里还有直接设频率（`scaling_setspeed`）与
频率范围（`scaling_min_freq` / `scaling_max_freq`）的文件。

**原文的劝阻（这条比命令本身重要）**：让 CPU 恒定跑最高频率**可能带来巨大的环境代价**；
**如果这个设置没有带来显著的性能提升，就为了地球继续用 powersave**。
有权限读 power MSR 的主机（云 guest 可能被过滤）可以用 MSR 测量两种设置下的功耗，把环境代价量化。

## 章末选型判据

| 场景 | 怎么做 | 别怎么做 |
|---|---|---|
| 拿到一台机器，先说清「有多少 CPU」 | 分开报 **socket 数 / core 数 / 逻辑 CPU 数** | 把硬件线程当 core |
| 判断「CPU 是不是瓶颈」 | 先 **per-CPU 利用率** → 再**饱和度**（load / PSI / 调度延迟）→ 最后才看 IPC | 只看系统级平均利用率 |
| 报告「CPU 用满了」 | 拆成「在算指令」还是「在等内存」——**利用率的分母含 memory stall cycle** | 把 100% 利用率直接等同于「CPU 不够」 |
| 判断负载类型 | 看 **user/kernel 比**：计算密集 ~99/1，I/O 密集 ~70/30 | 只看总 CPU 百分比 |
| 决定要不要加 CPU / 换更快的 CPU | 先测 **IPC**：低 IPC（≈0.2 或更低）多半是内存问题，加 CPU 与提主频都无效 | 看到饱和就加核；用主频当性能指标 |
| 云上判断 CPU 是否够 | 利用率**同时按配额与物理上限两个口径**看 | 等物理 CPU 到 100% 才说饱和 |
| 负载均值突然变大 | 先分清是 **CPU 需求**还是 **D 状态**（Linux 1993 起负载均值 = 系统级需求，含 CPU/磁盘/其他） | 把 load average 当作 CPU 利用率 |
| 分清「在干活」与「在等」 | 用 **PSI**（按 cpu/memory/io 分别看停顿时间占比）；2 CPU 跑 2 线程时 load=2.0 但 PSI=0.0 | 只看 load average |
| 找热点函数 | 火焰图**先看最宽的塔**；**x 轴不是时间**、左右顺序无含义 | 按火焰图左右顺序推断时间先后 |
| 采样看不见的短命开销 | 降采样频率、放大时间窗，直到开销可忽略；**用 99 Hz 避开 100 Hz 的 lock-step 偏斜** | 直接上函数级 tracing 到生产（常 10%+ 开销） |
| 要做完整 CPU profile | **用户栈 + 内核栈都记录** | 只记用户栈 |
| 查调度延迟 | `perf sched` / `runqlat` / `runqlen` | 用 `top` 的 %CPU 猜 |
| 定位中断开销 | `softirqs` / `hardirqs`；并检查中断在哪些 CPU 上（`/proc/irq/*/smp_affinity`） | 只看总中断率 |
| 怀疑互连 / NUMA 放置是瓶颈 | 看 **IPC 下降** + 停顿周期集中在内存/总线；用 `taskset` / cpuset 验证 | 用加 CPU 或提主频来解 |
| 多核跑得比单核还慢 | 先查**缓存一致性流量与伪共享**、同步粒度、NUMA 局部性 | 直接减少线程或加节点 |
| 「高优先级请求变慢但 CPU 不忙」 | 查**优先级反转**与锁；Linux 2.6.18 起的用户态 mutex 支持优先级继承 | 去加 CPU（锁不会因此解开） |
| 调优先级 | 先把**低价值工作**（监控 agent、备份）降 nice，再验证高优先级工作的调度延迟 | 盲目把业务进程提到实时类 |
| 用实时调度类 | 先确认 **RLIMIT_RTTIME** 等兜底；死循环会让**所有 CPU 对其他工作不可用** | 以为实时类只影响自己 |
| 跑基准对比 | 先确认两边**编译器选项一致、线程数匹配**；基准常测的是编译器优化差异 | 拿单线程基准比多核吞吐 |
| 改 scaling governor | 设 `performance` 需对 **所有 policy0..N** 都设；且要量化功耗代价，提升不明显就保留 `powersave` | 只设 policy0；无脑锁最高频 |
| 关 Turbo Boost | **只在需要稳定时钟的基准里做**；生产环境应开 | 为跑分好看改 BIOS |
| 相信观测数据之前 | 先跑一次已知负载（`while :; do :; done &`）验证工具确实报了「一个 CPU 100%」 | 直接采信工具输出 |



