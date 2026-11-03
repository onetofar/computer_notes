---
title: Chapter 1. Introduction（精读）
category: 性能工程
tags:
  - 性能工程
  - 可观测性
  - 延迟
  - 基准测试
  - 方法论
difficulty: 入门
source: "《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg）第1章 Introduction"
source_section: "精读 §1.1 / §1.3 / §1.4 / §1.5 / §1.6 / §1.7 / §1.8 / §1.9 / §1.10 / §1.11；§1.2 Roles 随 §1.3 一并立篇（角色与活动是同一条时间线）；§1.12 References 不立篇（纯文献列表，无机制）；§1.7.1 / §1.7.2 / §1.7.3、§1.10.1、§1.11.1–§1.11.3 是所属节的内部小节，随所属节立篇，不单独成节"
skills:
  - obsidian-markdown
---

# Chapter 1. Introduction

> **小节地图**：立篇八节——§1.1（范围：full stack 与两个目标）、§1.2–§1.4（谁做 / 什么时候做 /
> 从哪个方向看）、§1.5（性能为什么难）、§1.6（延迟：唯一能算出最大加速比的指标）、
> §1.7（可观测性：计数器 / 指标 / profiling / tracing）、§1.8（实验与基准测试）、
> §1.9–§1.10（云的新问题 + 方法论 + 60 秒清单）、§1.11（两个假想案例）。
> **不立篇**：§1.12 References（纯文献列表）。
> **底层依赖**：方法论见 [[Chapter 2. Methodologies]]；工具见 [[Chapter 4. Observability Tools]]；
> 动态插桩见 [[Chapter 15. BPF]] / [[Chapter 13. perf]]；各资源的「可观测 vs 实验」两节见
> [[Chapter 6. CPUs]] / [[Chapter 7. Memory]] / [[Chapter 8. File Systems]]。
> **同层对照**：[[Chapter 1. Introduction（讲义）]]（解释版）/ [[01-Chapter 1. Introduction]]（原文）。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §1.1 full stack 的定义 | 01-Chapter 1. Introduction.md L43 | When speaking of systems performance, however, we use full stack to mean the entire software stack from the application down to metal (the hardware), including system libraries, the kernel, and the hardware itself. Systems performance studies the full stack. |
| §1.3 性能工程要从第一步开始 | 01-Chapter 1. Introduction.md L87 | Performance engineering should ideally begin before any hardware is chosen or software is written: the first step should be to set objectives and create a performance model. |
| §1.4 两种分析视角 | 01-Chapter 1. Introduction.md L101 | Two perspectives for performance analysis are labeled in Figure 1.2 : workload analysis and resource analysis , which approach the software stack from different directions. |
| §1.5.1 性能是主观的 | 01-Chapter 1. Introduction.md L117 | Performance, on the other hand, is often subjective . With performance issues, it can be unclear whether there is an issue to begin with, and if so, when it has been fixed. |
| §1.5.1 用目标把主观变客观 | 01-Chapter 1. Introduction.md L125 | Subjective performance can be made objective by defining clear goals, such as having a target average response time, or requiring a percentage of requests to fall within a certain latency range. |
| §1.5.2 瓶颈会搬家 | 01-Chapter 1. Introduction.md L133 | Bottlenecks can also be complex and related in unexpected ways; fixing one may simply move the bottleneck elsewhere in the system, with overall performance not improving as much as hoped. |
| §1.6 延迟能算出最大加速比 | 01-Chapter 1. Introduction.md L161 | As a metric, latency can allow maximum speedup to be estimated. For example, Figure 1.3 depicts a database query that takes 100 ms (which is the latency) during which it spends 80 ms blocked waiting for disk reads. The maximum performance improvement by eliminating disk reads (e.g., by caching) can be calculated: from 100 ms to 20 ms (100 – 80) is five times (5x) faster. This is the estimated speedup , and the calculation has also quantified the performance issue: disk reads are causing the query to run up to 5x more slowly. |
| §1.6 IOPS 算不出加速比 | 01-Chapter 1. Introduction.md L167 | Such a calculation is not possible when using other metrics. I/O operations per second (IOPS), for example, depend on the type of I/O and are often not directly comparable. If a change were to reduce the IOPS rate by 80%, it is difficult to know what the performance impact would be. There might be 5x fewer IOPS, but what if each of these I/O increased in size (bytes) by 10x? |
| §1.7 生产环境先上可观测性 | 01-Chapter 1. Introduction.md L177 | For production environments, observability tools should be tried first wherever possible, as experimental tools may perturb production workloads through resource contention. |
| §1.7.1 计数器 → 统计量 | 01-Chapter 1. Introduction.md L183 | They are typically implemented as integer variables called counters that are hard-coded in the software, some of which are cumulative and always increment. These cumulative counters can be read at different times by performance tools for calculating statistics : the rate of change over time, the average, percentiles, etc. |
| §1.7.2 profiling 就是采样 | 01-Chapter 1. Introduction.md L223 | In systems performance, the term profiling usually refers to the use of tools that perform sampling: taking a subset (a sample) of measurements to paint a coarse picture of the target. |
| §1.7.3 tracing 是事件记录 | 01-Chapter 1. Introduction.md L239 | Tracing is event-based recording, where event data is captured and saved for later analysis or consumed on-the-fly for custom summaries and other actions. |
| §1.7.3 动态插桩 | 01-Chapter 1. Introduction.md L277 | Dynamic instrumentation creates instrumentation points after the software is running, by modifying in-memory instructions to insert instrumentation routines. This is similar to how debuggers can insert a breakpoint on any function in running software. |
| §1.8 宏基准 vs 微基准 | 01-Chapter 1. Introduction.md L299 | There are macro-benchmark tools that simulate a real-world workload such as clients making application requests; and there are micro-benchmark tools that test a specific component, such as CPUs, disks, or networks. |
| §1.8 观测与实验是两只手 | 01-Chapter 1. Introduction.md L331 | An analogy taught to me by a senior performance engineer (Roch Bourbonnais) many years ago was this: you have two hands, observability and experimentation. Only using one type of tool is like trying to solve a problem one-handed. |
| §1.10 没有方法论就是钓鱼 | 01-Chapter 1. Introduction.md L345 | Without a methodology, a performance investigation can turn into a fishing expedition: trying random things in the hope of catching a win. This can be time-consuming and ineffective, while allowing important areas to be overlooked. |
| §1.10.1 Linux 60 秒清单 | 01-Chapter 1. Introduction.md L349 | This is a Linux tool-based checklist that can be executed in the first 60 seconds of a performance issue investigation, using traditional tools that should be available for most Linux distributions [Gregg 15a] . |
| §1.11.1 采样粒度决定结论 | 01-Chapter 1. Introduction.md L475 | AcmeMon reported 80% utilization but uses a one-minute interval. At one-second granularity, Sumit can see that disk utilization fluctuates, often hitting 100% and causing levels of saturation and increased disk I/O latency. |
| §1.11.1 页面缓存被别的进程吃掉 | 01-Chapter 1. Introduction.md L495 | This memory is taken from what is available for the file system cache, reducing its hit rate and causing more file system reads to become disk reads. |
| §1.11.2 瓶颈可能在客户端 | 01-Chapter 1. Introduction.md L523 | To investigate the client software she performs thread state analysis and finds that it is single-threaded! That one thread is spending 100% of its time executing on-CPU. This convinces her that this is the limiter of the test. |

> **书里没说的**（本篇的推论，原文没有，别当原文观点）：
> 1. 原文说 counters / statistics / metrics 三词「常被混用」、告警可由任何一层产生，
>    **没有**把它们画成严格层级；本篇 §1.7.1 的三层表格是为检索方便而做的整理。
> 2. 原文把 tracing 的实现分成静态插桩 / 动态插桩 / BPF 三条路线，但**没有**说
>    「BPF 是动态插桩的一种实现」；本篇把三者并列成三栏是本篇的表述方式。
> 3. §1.11 是原文明说的**假想例子**（hypothetical examples）。本篇把「一条命令就能定位」
>    这类做法当判据用；案例里的数值（80% 利用率等）是示例数值，**不是通用门限**。
> 4. 原文说延迟「能算出最大加速比」，**没有**说延迟永远优先于其它指标；
>    本篇章末把「能换算成加速比」当成选延迟的**条件**，这个条件化是本篇的归纳。

## §1.1 性能工程的边界：full stack 与两个目标

**范围 = 数据路径上的一切。** 从存储设备到应用软件，任何一环都可能影响性能；
分布式系统还意味着多台服务器与多个应用。原文要求：**手上没有数据路径图就先画一张**，
否则整片区域会被整体忽略。

**术语澄清：`full stack` 在本书里指「从应用到金属」的全栈**，而不是业界常说的
「应用 + 数据库 + Web 服务器」那一小撮：

> When speaking of systems performance, however, we use full stack to mean the entire software stack from the application down to metal (the hardware), including system libraries, the kernel, and the hardware itself. Systems performance studies the full stack.

这是全书后面所有章节的**授权边界**：内核、库、硬件都归性能工程师管。

**两个目标**（原文措辞很克制）：**降延迟**（终端用户体验）与**降成本**
（省算力 = 省机器 = 省电费）。这是**两个独立目标**——为降延迟加机器，有时反而更贵。

## §1.2–§1.4 谁做、什么时候做、从哪个方向看

### §1.2 角色

性能工作由多个岗位分担：系统管理员、SRE、应用开发、网络工程师、DBA、Web 管理员。
多数人只负责**自己那部分**（网络团队查网络、数据库团队查数据库）；有些问题的根因横跨
多个团队，就必须协作。另有一类公司设专职 **performance engineer**：可跨团队做整体研究，
也可作为中心资源去**造更好的性能工具**、做全环境的容量规划；大型团队内部还会再分专长
（内核性能、客户端性能、语言性能如 Java、运行时性能如 JVM、性能工具本身）。

### §1.3 活动：性能工作是一条时间线

原文列了十项活动，并指出它们**恰好是软件产品生命周期的理想步骤**：
① 为未来产品设性能目标 + 性能建模；② 对原型软硬件做性能表征；③ 在测试环境分析在研产品；
④ 新版本的非回归测试；⑤ 对发布版本做基准测试；⑥ 在目标生产环境做概念验证测试；
⑦ 生产环境性能调优；⑧ 监控运行中的生产软件；⑨ 分析生产问题；⑩ 生产问题的事故复盘。

判据——**问题出现在第 6–9 步，说明它本该在开发阶段就被发现或修掉**：

> If an issue is encountered in the target environment (steps 6 to 9), it means that the issue was not detected or fixed during the development stages.

而**第一步就该做**，不能等到出问题再补：

> Performance engineering should ideally begin before any hardware is chosen or software is written: the first step should be to set objectives and create a performance model.

原文随即泼冷水：**云让「跳过前五步」变得太容易**。两种技术是 **canary testing**
（用生产负载的一小部分在单实例上试）与 **blue-green deployment**（逐步把流量切到新实例池，
旧池留作回退；Netflix 叫 red-black）。有了「失败可回退」这层保险，新软件常未经任何性能
分析就上生产。作者的立场是：**条件允许时，前面的步骤也该做**（虽然可能有上市时间压力）。
**容量规划（capacity planning）**横跨前面多项活动：设计期是「研究开发中软件的资源占用是否
匹配目标」，部署后是「监控资源使用以预测问题」。

### §1.4 两种视角

> Two perspectives for performance analysis are labeled in Figure 1.2 : workload analysis and resource analysis , which approach the software stack from different directions.

| 视角 | 方向 | 典型使用者 | 问的问题 |
|---|---|---|---|
| **工作负载分析**（workload analysis） | 自上而下 | 应用开发 | 「这个请求为什么慢？」 |
| **资源分析**（resource analysis） | 自下而上 | 系统管理员 | 「哪项资源用满了？」 |

两者各有强项，**难搞的问题两边都试**；详细展开见 [[Chapter 2. Methodologies]]。

## §1.5 性能为什么难

原文给了四个原因，各占一小节。

### §1.5.1 主观

技术行业习惯客观：一个 bug 要么存在要么不存在，报错一读就懂。性能不是这样：

> Performance, on the other hand, is often subjective . With performance issues, it can be unclear whether there is an issue to begin with, and if so, when it has been fixed.

原文的例子极简：平均磁盘 I/O 响应时间 1 ms —— **这算好还是坏？**
同一个人可能觉得「慢到不能用」，另一个人觉得「快得离谱」。

**解法是把主观变客观**：

> Subjective performance can be made objective by defining clear goals, such as having a target average response time, or requiring a percentage of requests to fall within a certain latency range.

两条可执行的目标形态：**平均响应时间目标**、**「落在某延迟区间内的请求占比」目标**。
后者更好，因为它自带一个分位数而不是一个均值。

### §1.5.2 复杂

难在**系统复杂且没有明显起点**——云环境里你甚至不知道该先看哪台实例；
有时从假设出发（「肯定是网络」「肯定是数据库」），分析师得先验证方向对不对。

两个具体的复杂度来源：**级联故障**（cascading failure，子系统单独看都正常，
组合起来出问题；一个组件挂了连带让别的组件出现性能问题）与**瓶颈会搬家**：

> Bottlenecks can also be complex and related in unexpected ways; fixing one may simply move the bottleneck elsewhere in the system, with overall performance not improving as much as hoped.

修好一个瓶颈、另一个顶上来、整体改善远不如预期——这是性能工作最常见的挫败来源，
也是**必须整体看**而不是逐个资源修的原因。此外，生产负载本身可能很复杂：
**这类问题在实验室里可能永远复现不了，或者只能间歇复现**。

### §1.5.3 多因

有些问题**没有单一根因**，而是多个因素叠加：三个单独看都正常的普通事件同时发生，
组合起来才出问题。原文特别强调：**每个单看都不是根因**。

### §1.5.4 多问题

> Finding a performance issue is usually not the problem; in complex software there are often many.

在复杂软件里，**已知未修的性能问题往往一大把**（原文建议去搜自己操作系统的 bug 库里
"performance" 这个词试试）。所以真正的任务不是「找到问题」，而是
**判定哪些问题最重要**。做法是**量化每个问题的量级**，并估计修掉它能带来多少加速比——
后者在向管理层要人、要机器时是硬通货。

## §1.6 延迟：唯一能算出最大加速比的指标

延迟（latency）是「等待所花的时间」，可以指任何操作的耗时：应用请求、数据库查询、
文件系统操作、网页从点击到出画面的全过程。它同时对用户和提供方重要。

**延迟的独有能力：能算出最大加速比。**

> As a metric, latency can allow maximum speedup to be estimated. For example, Figure 1.3 depicts a database query that takes 100 ms (which is the latency) during which it spends 80 ms blocked waiting for disk reads. The maximum performance improvement by eliminating disk reads (e.g., by caching) can be calculated: from 100 ms to 20 ms (100 – 80) is five times (5x) faster. This is the estimated speedup , and the calculation has also quantified the performance issue: disk reads are causing the query to run up to 5x more slowly.

这段话有两层价值：**（1）** 给出上限——缓存掉磁盘读，100 ms → 20 ms，即 5x；
**（2）** 顺带把问题**量化**了：磁盘读让查询慢了最多 5 倍。
一次测量同时给出「天花板」和「责任方」。

**别的指标做不到这件事**：

> Such a calculation is not possible when using other metrics. I/O operations per second (IOPS), for example, depend on the type of I/O and are often not directly comparable. If a change were to reduce the IOPS rate by 80%, it is difficult to know what the performance impact would be. There might be 5x fewer IOPS, but what if each of these I/O increased in size (bytes) by 10x?

IOPS 掉了 80%，性能是好是坏？**说不清**——单次 I/O 的大小可能同时涨了 10 倍。
**速率型指标与总量型指标混在一起时无法换算。**

**术语要限定**：网络里的 "latency" 可能指建连时间，也可能指含数据传输的连接总时长。
原文全书用更精确的说法：**connection latency**（建连延迟）、**request latency**（请求延迟），
每章开头都有延迟术语表。

**延迟也有拿不到的时候**：有些子系统只给平均延迟，有些根本不提供延迟测量。
**BPF 让「从任意自定义关注点测延迟」成为可能**，还能给出完整分布（而不只是均值）。

## §1.7 可观测性：观察系统的三种手段

**可观测性（observability）= 通过观察来理解系统**，工具分三类：**计数器、profiling、tracing**。
**关键分界：可观测性工具不改系统状态；实验工具会。**

> For production environments, observability tools should be tried first wherever possible, as experimental tools may perturb production workloads through resource contention.

生产环境**先上可观测性工具**，因为实验工具会通过资源争抢扰动生产负载；
反过来，**空闲的测试环境**可以先跑基准测试，先摸清硬件本身的能力。

### §1.7.1 计数器、统计量、指标

应用和内核都会提供自身状态与活动的数据：操作计数、字节计数、延迟测量、资源利用率、错误率。
它们通常是**硬编码在软件里的整型计数器**：

> They are typically implemented as integer variables called counters that are hard-coded in the software, some of which are cumulative and always increment. These cumulative counters can be read at different times by performance tools for calculating statistics : the rate of change over time, the average, percentiles, etc.

**关键机制：累加计数器本身没有信息量，信息量在「两次读数之差」里** —— 所以统计量
（变化率、均值、分位数）必须靠**多次采样**才能算出来。
**指标 = 被挑出来用来评估或监控某个目标的统计量**；多数公司用监控 agent 定期记录并画图，
支持基于指标的自定义告警。原文提醒：这层术语**在业界不严格**，三者常被混用。
**什么时候 metrics 就够了**：知道问题**从哪一刻开始**，可能正好对应一次已知的代码或配置
变更——直接回滚即可。什么时候不够：metrics 只指出方向（「像是 CPU 或磁盘的问题」）却不说
为什么——这时**必须上 profiling 或 tracing**。

### §1.7.2 Profiling：采样

> In systems performance, the term profiling usually refers to the use of tools that perform sampling: taking a subset (a sample) of measurements to paint a coarse picture of the target.

**profiling = 采样**：取测量值的一个子集，画出目标的粗略画像。
CPU 是最常见的 profiling 目标，方法是对**在 CPU 上运行的代码路径**做定时采样。

**火焰图（flame graph）是 CPU profile 最有效的可视化。** 原文的判断：
「除 metrics 之外，CPU 火焰图能帮你找到的性能收益比任何其它工具都多。」
它的厉害之处在于**能顺带发现非 CPU 类问题**：自旋路径占 CPU ⇒ 锁竞争；
`malloc()` 等内存分配函数占大量 CPU ⇒ 内存问题（顺着调用链还能看出是谁在分配）；
慢速或遗留的网络代码路径占 CPU ⇒ 网络配置不当。
火焰图本身见 [[Chapter 6. CPUs]] 的 §6.7.3，工具见 [[Chapter 4. Observability Tools]]。

### §1.7.3 Tracing：事件记录

> Tracing is event-based recording, where event data is captured and saved for later analysis or consumed on-the-fly for custom summaries and other actions.

**tracing = 基于事件的记录**：事件数据被捕获后存下来事后分析，或**实时消费**做自定义汇总。
专用 tracer（`strace(1)` 看系统调用、`tcpdump(8)` 看网络包）只覆盖某一类事件；
通用 tracer（Ftrace、BCC、bpftrace）能分析软件与硬件的**全部**事件，依赖三条技术路线：

- **静态插桩（static instrumentation）**：源码里硬编码的插桩点。Linux 内核里有几百个，
  覆盖磁盘 I/O、调度器事件、系统调用等，内核侧叫 **tracepoints**；用户态叫 **USDT**，
  被 `libc` 之类的库和大量应用用来标记服务请求。`execsnoop(8)` 就是插桩 `execve(2)` 的
  tracepoint 来打印新建进程——**这类短命进程是 `top(1)` 会漏掉的**，而它们常常就是
  性能问题的来源。
- **动态插桩（dynamic instrumentation）**：

> Dynamic instrumentation creates instrumentation points after the software is running, by modifying in-memory instructions to insert instrumentation routines. This is similar to how debuggers can insert a breakpoint on any function in running software.

  **机制：软件已经跑起来了，通过改写内存中的指令来插入插桩例程。** 与调试器的区别：
  调试器命中断点时把控制权交给交互式调试器，动态插桩则**跑完例程就继续目标程序**。
  ⇒ **可以对任意运行中的软件生成自定义性能统计**；那些「因为缺乏可观测性而根本没法查」
  的问题，现在能查了。原文的比喻：分析内核内部像**走进黑屋子**，里面摆着**蜡烛**
  （系统计数器）；动态插桩则是**一只手电筒**，你指哪打哪。
- **BPF**：原本只是加速 `tcpdump(8)` 表达式求值的内核内小虚拟机，2013 年起扩展成
  **通用的内核内执行环境**，是当下 Linux 动态追踪工具的底座。详见 [[Chapter 15. BPF]]。

`perf(1)` 与 Ftrace 也是 tracer，能力与 BPF 前端有重叠，分别见
[[Chapter 13. perf]] 与 [[Chapter 14. Ftrace]]。

## §1.8 实验与基准测试

可观测性之外是**实验工具（experimentation tools）**，绝大多数是基准测试工具：
**对系统施加合成负载，再测性能**。风险也写在这里——
**实验工具会扰动被测系统本身**。

**两类基准测试**：

> There are macro-benchmark tools that simulate a real-world workload such as clients making application requests; and there are micro-benchmark tools that test a specific component, such as CPUs, disks, or networks.

| 类型 | 测什么 | 原文的评价 |
|---|---|---|
| **宏基准**（macro） | 模拟真实负载，比如客户端发起应用请求 | 更接近真实，但难复现、难解释 |
| **微基准**（micro） | 单个部件：CPU、磁盘、网络 | **更容易调试、重复、理解，也更稳定** |

原文的类比：赛车场的单圈时间是宏基准，**极速和 0–60 mph 加速时间是微基准**。

原文用 `iperf(1)` 做了一次网络微基准，结果出现**双峰吞吐**（前 5 秒约 4.8 Gbit/s，
之后掉到约 3.2 Gbit/s）。这里有一个关键对照：
**只用可观测性**看生产上的网络吞吐，客户端负载的天然波动会把**底层双模行为一起淹没**；
**用固定负载的 `iperf(1)`** 消除了客户端方差，剩下的方差才归因于其它因素
（外部网络限速、缓冲区利用率等）。原文顺带纠正一个用词错误：`iperf` 输出的
"Bandwidth" 是误用——**bandwidth 指最大可能吞吐，iperf 测的是当前速率，即 throughput**。

**总原则（作者引用的老话）**：

> An analogy taught to me by a senior performance engineer (Roch Bourbonnais) many years ago was this: you have two hands, observability and experimentation. Only using one type of tool is like trying to solve a problem one-handed.

**两只手：可观测性与实验。** 只用一只就是单手解题。但顺序有讲究：
**生产环境先观测，实验要小心**；空闲测试环境可以先实验。

## §1.9–§1.10 云带来的新问题、方法论与 60 秒清单

### §1.9 云

云计算让应用能按需扩缩（实例），**降低了对严格容量规划的需要**；
但也**提高了对性能分析的欲望**——用更少资源 = 用更少机器，
而云通常按分钟/小时计费，**性能收益直接等于立刻省下的钱**
（对比企业数据中心：固定支持合同锁几年，成本收益要等合同结束才能兑现）。

云与虚拟化带来的新难点：

> New difficulties caused by cloud computing and virtualization include the management of performance effects from other tenants (sometimes called performance isolation ) and physical system observability from each tenant.

即 **性能隔离**（邻租户抢资源）与 **物理可观测性**（租户看不到底层磁盘的真实使用）——
后者让「被邻居拖累」这类问题**很难被发现**。详见 [[Chapter 11. Cloud Computing]]。

### §1.10 方法论

> Without a methodology, a performance investigation can turn into a fishing expedition: trying random things in the hope of catching a win. This can be time-consuming and ineffective, while allowing important areas to be overlooked.

**没有方法论的性能调查就是钓鱼**——随机试、碰运气；既费时又低效，还会让重要区域被整体漏掉。
方法论的本质是**把推荐步骤文档化**。本章给出的第一份是「工具式清单」。

### §1.10.1 Linux 60 秒分析清单

> This is a Linux tool-based checklist that can be executed in the first 60 seconds of a performance issue investigation, using traditional tools that should be available for most Linux distributions [Gregg 15a] .

清单用**传统工具**（几乎所有发行版都自带），**十步**：

| # | 命令 | 看什么 |
|---|---|---|
| 1 | `uptime` | 负载均值：对比 1 / 5 / 15 分钟，看是在涨还是跌 |
| 2 | `dmesg -T \| tail` | 内核错误，包括 OOM 事件 |
| 3 | `vmstat -SM 1` | 系统级统计：运行队列长度、换页、整体 CPU 使用 |
| 4 | `mpstat -P ALL 1` | 每 CPU 均衡度：**单个 CPU 很忙 ⇒ 线程扩展性差** |
| 5 | `pidstat 1` | 每进程 CPU：意外的 CPU 消费者，以及用户态/内核态时间 |
| 6 | `iostat -sxz 1` | 磁盘 I/O：IOPS、吞吐、平均等待、繁忙百分比 |
| 7 | `free -m` | 内存使用，含文件系统缓存 |
| 8 | `sar -n DEV 1` | 网卡 I/O：包数与吞吐 |
| 9 | `sar -n TCP,ETCP 1` | TCP 统计：连接速率、重传 |
| 10 | `top` | 总览 |

原文补充：这份清单也可以用监控 GUI 走，前提是**同样的指标都能拿到**；
但原文的设计意图是榨干现成的 CLI 工具，监控产品通常有更多更好的指标——
**要做定制看板，原文更倾向于做 USE 方法那类方法论的看板**。

更完整的方法论库（USE 方法、工作负载表征、延迟分析等）在 [[Chapter 2. Methodologies]]。

## §1.11 案例研究

原文给的是**两个假想案例**（hypothetical examples），用来说明活动在实践中怎么排。

### §1.11.1 慢磁盘

**场景**：系统管理员 Sumit 收到工单，数据库团队抱怨「磁盘慢」。

**第一步是澄清问题，不是查磁盘。** Sumit 反问四件事：现在真有数据库性能问题吗、怎么测的；
存在多久了；数据库最近有变动吗；**为什么怀疑磁盘**？回复是：有慢查询日志（> 1000 ms），
过去一周涨到每小时几十条；监控 AcmeMon 显示磁盘很忙。

**Sumit 的判断**：

> This confirms that there is a real database issue, but it also shows that the disk hypothesis is likely a guess.

**问题是真的，但「磁盘慢」很可能是猜的。** 于是两边同时查：用 **USE 方法**
（[[Chapter 2. Methodologies]] 的 §2.5.9）扫资源瓶颈，发现磁盘利用率高（约 80%），
CPU 与网络都低；且磁盘利用率一周来稳步上升，CPU 平稳。

**接着是采样粒度问题**：

> AcmeMon reported 80% utilization but uses a one-minute interval. At one-second granularity, Sumit can see that disk utilization fluctuates, often hitting 100% and causing levels of saturation and increased disk I/O latency.

监控面板用**一分钟**间隔，只看到被平均掉的 80%；用**一秒**间隔就能看到它**经常冲到 100%**
⇒ 存在饱和与 I/O 延迟上升。**采样间隔决定了你能看到什么样的结论**——本案例最可复用的机制。

**确认因果而不只是相关**：用 BPF 工具 `offcputime(8)` 抓数据库被内核调度出去的时刻的栈与
off-CPU 时间，栈显示**数据库常常阻塞在查询中的文件系统读上** ⇒ 确认是同步阻塞。

**继续找「为什么」**：工作负载表征（IOPS、吞吐、平均 I/O 延迟、读写比）后，方向指向
「磁盘负载高，但磁盘本身正常」；查询速率平稳、CPU 平稳，都排除；文件系统碎片也排除
（容量才 30%）。**根因**：他知道可以往下钻（drill-down analysis），但先试更快的解释——
想起磁盘 I/O 主要由**文件系统缓存（page cache）未命中**造成 ⇒ 查缓存命中率（91%）。
查内存占用时发现一个**被忽略的事实**：某个开发项目的原型应用在持续吃内存（虽未承生产负载）。
**这些内存是从文件系统缓存的份额里扣的**：

> This memory is taken from what is available for the file system cache, reducing its hit rate and causing more file system reads to become disk reads.

⇒ 缓存命中率下降 ⇒ 更多文件读变成磁盘读 ⇒ 磁盘忙 ⇒ 查询变慢。
**真正的链条是「内存 → 页面缓存 → 磁盘 I/O → 查询延迟」**，中间任何一环单独看都正常。
停掉那个原型应用并迁走后，缓存恢复、磁盘利用率回落、慢查询归零。

### §1.11.2 软件变更

**场景**：性能工程师 Pamela 要在部署前对新核心功能做**非回归测试**。她拿了**空闲服务器**，
配置对齐生产，从另一台机器跑客户端负载模拟器；先**表征客户端工作负载**（读访问日志），
并与生产日志对比：模拟器**能施加平均生产负载，但没有覆盖方差**——记下继续。

**压测**：从 100 每次加 100，到 700 请求/秒时吞吐**不再上升**；换新版本重测**也是 700**，
服务端资源却依然很闲。她意识到：

> She knows she checked only server resources, and the limiter could instead be an application logic issue. It could also be elsewhere: the network or the client simulator.

**改换思路**：**固定速率**跑（700/s），再表征资源消耗，换算成「每请求耗多少 CPU」。
旧版本让 32 个 CPU 平均跑到 20% 利用率，新版本同样负载下跑到 **30%** ⇒ **确实是回归**。

**接着定位 700 这个上限**：加大负载，检查**数据路径上的所有部件**——网络、客户端系统、
负载生成器都查，并对客户端做**线程状态分析**：

> To investigate the client software she performs thread state analysis and finds that it is single-threaded! That one thread is spending 100% of its time executing on-CPU. This convinces her that this is the limiter of the test.

**负载生成器是单线程的，那一个线程 100% 时间在 CPU 上 ⇒ 它就是测试的限流器。**
**实验验证**：把客户端**并行跑在多台机器上**，把服务端打到 100% CPU —— 旧版本 3500 req/s，
新版本 2300 req/s，与「每请求更费 CPU」一致。
**收尾**：用 **CPU 火焰图**分析新版本的 CPU 时间去向；记录「测的是平均负载、没测变化负载」；
并给负载生成器的单线程缺陷**开了 bug**。

### §1.11.3 更多阅读

完整的案例研究见 [[Chapter 16. Case Study]]（云上的一次真实性能问题排查）。

## 章末选型判据

| 场景 | 用什么 | 别用什么 |
|---|---|---|
| 界定排查范围 | 画一张**数据路径图**，包含应用到硬件的每一跳 | 只查自己负责的那一层（§1.1） |
| 判定「这是不是个问题」 | 先写死**目标**（平均响应时间 / 落在延迟区间内的请求占比） | 凭「我觉得慢」下结论（§1.5.1） |
| 给问题排优先级 | **量化每个问题的量级 + 估计最大加速比** | 按「先找到的先修」排队（§1.5.4） |
| 估算收益 / 向上汇报 | **延迟**：可算出最大加速比（100 ms → 20 ms 即 5x） | IOPS 等速率型指标（单独掉 80% 说明不了性能变化）（§1.6） |
| 生产环境 | **先可观测性工具**；实验要小心，它会扰动负载 | 一上来就压测（§1.7 / §1.8） |
| 空闲测试环境 | 先**基准测试**摸清硬件能力，再上真实负载 | 直接拿生产负载试（§1.7） |
| 想知道「问题从什么时候开始」 | **metrics**（时间序列），对上已知的代码/配置变更 | tracing（此时是杀鸡用牛刀）（§1.7.1） |
| 想知道「为什么会这样」 | **profiling**（采样找热点）/ **tracing**（事件找因果） | 只看 metrics（它只指方向不给原因）（§1.7.1） |
| 找 CPU 热点 | **火焰图**：一次覆盖 CPU、锁竞争、内存分配、网络路径 | 逐个函数手工计时（§1.7.2） |
| 看短命进程 / 系统调用 | **tracing**（`execsnoop`、tracepoints、USDT） | `top`（会漏掉短命进程）（§1.7.3） |
| 查「无从下手的黑盒」 | **动态插桩 / BPF**（改内存中的指令，任意插桩点） | 只能看现成计数器（§1.7.3） |
| 测单个部件能力 | **微基准**（更易调试、复现、稳定） | 宏基准（难复现、难解释）（§1.8） |
| 排查方向不明 | **观测 + 实验两只手都用** | 只用一只（§1.8） |
| 面对多瓶颈 | 预期「修一个会顶上来一个」，改完**重测全局** | 局部修完就宣布解决（§1.5.2） |
| 开始一次调查 | **60 秒清单**先跑一遍十条命令 | 随机试命令（钓鱼）（§1.10 / §1.10.1） |
| 看利用率数字 | 先确认**采样间隔**（一分钟均值会藏住冲到 100% 的尖峰） | 直接拿面板上的 80% 下结论（§1.11.1） |
| 压测到吞吐不再上升 | 检查**数据路径上的所有部件**，含客户端与负载生成器 | 只查服务端资源（限流器可能不在服务端）（§1.11.2） |
