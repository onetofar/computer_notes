---
title: Chapter 2. Methodologies（精读）
category: 性能工程
tags:
  - 性能工程
  - 延迟
  - 排队论
  - USE 方法
  - 统计
  - 方法论
difficulty: 进阶
source: "《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg）第2章 Methodologies"
source_section: "精读 §2.1 / §2.2 / §2.3.1 / §2.3.8 / §2.3.9 / §2.3.11 / §2.3.12 / §2.3.14 / §2.4 / §2.5 / §2.5.9 / §2.5.10 / §2.5.11 / §2.5.12 / §2.5.13 / §2.5.19 / §2.5.20 / §2.6.5 / §2.8；§2.3.2 / §2.3.3 / §2.3.4 / §2.3.5 / §2.3.6 / §2.3.7 / §2.3.10 / §2.3.13 / §2.3.15 随所属机制立篇（时间尺度、权衡、调优层次、何时停止、时点性建议等为工作判断，不单独成节）；§2.5.1 / §2.5.2 / §2.5.3 / §2.5.4 / §2.5.7 / §2.5.8 / §2.5.14 / §2.5.15 / §2.5.16 / §2.5.17 / §2.5.18 归入 §2.5 方法论全景与 §2.5.9（反方法、诊断循环、Tools 方法、方法 R、事件追踪、基线、静态与缓存调优）；§2.6.1 / §2.6.2 / §2.6.3 / §2.6.4 不立篇（云建模、视觉识别、Amdahl 定律与通用可扩展性定律是扩展性专题，见第11/12章）；§2.7 不立篇（容量规划是第9章扩展与第11章云的话题，本篇只取其排队论依据 §2.6.5）；§2.9 / §2.10 不立篇（监控与可视化属第4章可观测性）；§2.11 Exercises 与 §2.12 References 不立篇（练习题与参考文献是全书共用的自测/索引材料，不承载机制陈述）"
skills:
  - obsidian-markdown
---

# Chapter 2. Methodologies

> **小节地图**：这一章是全书的**方法论底座**——第 6/7/8/9/10/11/12 章都是它的应用。
> 本篇立篇十四节：§2.1（口径）、§2.2（模型）、§2.3.1（延迟）、§2.3.8–§2.3.9（负载 vs 架构 / 扩展性 / 上下文切换）、
> §2.3.11–§2.3.12（利用率与饱和）、§2.3.14（缓存与局部性）、§2.4（两种视角）、§2.5（方法论全景）、
> §2.5.9（USE 方法，连到附录 A）、§2.5.10（RED 方法）、§2.5.11（工作负载刻画）、
> §2.5.12–§2.5.13（下钻与延迟分析）、§2.5.19–§2.5.20（微基准与性能箴言）、
> §2.6.5（排队论 / Little 定律 / M/D/1 的 60%）、§2.8（统计）。
> **不立篇**：§2.6.1–§2.6.4、§2.7、§2.9、§2.10、§2.11、§2.12（理由见 frontmatter）。
> **与其它章的分工**：本篇只讲**方法论本身**（怎么定义、怎么算、怎么推理）。
> 落到磁盘 / 网络 / 内存的具体实例，分别见 [[Chapter 9. Disks]] / [[Chapter 10. Network]] / [[Chapter 7. Memory]]；
> 基准测试见 [[Chapter 12. Benchmarking]]；USE 方法的 Linux 清单见附录 A。
> **同层对照**：[[Chapter 1. Introduction]]（为什么难 / 60 秒清单）。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §2.1 术语 | 02-Chapter 2. Methodologies.md L73 | Latency : A measure of time an operation spends waiting to be serviced. In some contexts it can refer to the entire time for an operation, equivalent to response time. See Section 2.3 , Concepts , for examples. |
| §2.3.1 延迟的歧义 | 02-Chapter 2. Methodologies.md L131 | Since the single word “latency” can be ambiguous, it is best to include qualifying terms to explain what it measures: request latency, TCP connection latency, etc. |
| §2.3.1 延迟为什么是好指标 | 02-Chapter 2. Methodologies.md L133 | As latency is a time-based metric, various calculations are possible. Performance issues can be quantified using latency and then ranked because they are expressed using the same units (time). |
| §2.3.8 负载 vs 架构 | 02-Chapter 2. Methodologies.md L409 | An issue of load may be a multithreaded application that is busy on all available CPUs, with requests still queueing. In this case, performance is limited by the available CPU capacity, or put differently, by there being more load than the CPUs can handle. |
| §2.3.9 上下文切换 | 02-Chapter 2. Methodologies.md L423 | After peak performance, at 100% utilization, throughput begins to decrease as more threads are added, causing more context switches, which consume CPU resources and cause less actual work to be completed. |
| §2.3.11 两种利用率 | 02-Chapter 2. Methodologies.md L501 | 100% busy does not mean 100% capacity. |
| §2.3.12 饱和 | 02-Chapter 2. Methodologies.md L511 | The degree to which more work is requested of a resource than it can process is saturation . Saturation begins to occur at 100% utilization (capacity-based), as extra work cannot be processed and begins to queue. |
| §2.3.14 缓存缺失率 | 02-Chapter 2. Methodologies.md L547 | Another metric for understanding cache performance is the cache miss rate , in terms of misses per second. This is proportional (linear) to the performance penalty of each miss and can be easier to interpret. |
| §2.5.5 问题陈述 | 02-Chapter 2. Methodologies.md L1025 | Just asking and answering these questions often points to an immediate cause and solution. |
| §2.5.6 科学方法 | 02-Chapter 2. Methodologies.md L1031 | The scientific method studies the unknown by making hypotheses and then testing them. It can be summarized by the following steps: |
| §2.5.9 USE 方法 | 02-Chapter 2. Methodologies.md L1123 | For every resource, check utilization, saturation, and errors. |
| §2.5.9 检查顺序 | 02-Chapter 2. Methodologies.md L1145 | Errors are checked first because they are usually quick to interpret (they are usually an objective and not subjective metric), and it can be time-efficient to rule them out before investigating the other metrics. Saturation is checked second because it is quicker to interpret than utilization: any level of saturation can be an issue. |
| §2.5.9 区间均值会藏住尖峰 | 02-Chapter 2. Methodologies.md L1165 | Consider a toll plaza on a highway. Utilization can be defined as how many tollbooths were busy servicing a car. Utilization at 100% means you can’t find an empty booth and must queue behind someone (saturation). |
| §2.5.9 利用率判据 | 02-Chapter 2. Methodologies.md L1341 | Utilization at 100% is usually a sign of a bottleneck (check saturation and its effect to confirm). Utilization beyond 60% can be a problem for a couple of reasons: depending on the interval, it can hide short bursts of 100% utilization. |
| §2.5.13 延迟分析 | 02-Chapter 2. Methodologies.md L1463 | Latency analysis examines the time taken to complete an operation and then breaks it into smaller components, continuing to subdivide the components with the highest latency so that the root cause can be identified and quantified. |
| §2.6.5 Little 定律 | 02-Chapter 2. Methodologies.md L1787 | which determines the average number of requests in a system, L , as the average arrival rate, λ, multiplied by the average request time in the system, W . This can be applied to a queue, such that L is the number of requests in the queue, and W is the average wait time on the queue. |
| §2.6.5 M/D/1 的 60% | 02-Chapter 2. Methodologies.md L1855 | Beyond 60% utilization, the average response time doubles. By 80%, it has tripled. As disk I/O latency is often the bounding resource for an application, increasing the average latency by double or higher can have a significant negative effect on application performance. |
| §2.6.5 高分位更陡 | 02-Chapter 2. Methodologies.md L1859 | There is also a distribution of response times, not pictured in Figure 2.19 , such that the 90th and 99th percentiles degrade much faster beyond 60% utilization. |
| §2.8.2 均值的局限 | 02-Chapter 2. Methodologies.md L2045 | Averages are a summary statistic that hides details. I’ve analyzed many cases of occasional disk I/O latency outliers exceeding 100 ms, while the average latency was close to 1 ms. |
| §2.8.5 双峰下的平均值 | 02-Chapter 2. Methodologies.md L2077 | The average (mean) I/O latency is 3.3 ms, which is plotted as a vertical line. This average is not the index of central tendency (as described earlier); in fact, it is almost the opposite. As a metric, the average for this distribution is seriously misleading. |

> **书里没说的**（本篇的推论，原文没有，别当原文观点）：
> - **「协调遗漏（coordination omission）」全书没有出现过**。本章讲负载测试的部分（§2.5.2 随机改动反方法、
>   §2.5.19 微基准）都只谈「怎么造负载、怎么量」，没谈**开环（到达率固定）vs 闭环（并发数固定）**的区别。
>   这不是本书的疏漏，而是它把压测方法放在第 12 章 Benchmarking（§12.3.7 Ramping load、§12.2.2 Simulation）展开。
>   本篇给一条**（推演）**的提醒：§2.6.5 的 Little 定律与 M/D/1 模型**只在开环假设下成立**；
>   若用闭环压测（并发线程固定、请求阻塞到返回），系统接近饱和时会自发降速，
>   被测者自己把到达率降了下来 ⇒ **测出的延迟分布系统性偏低**，且**饱和区根本测不到**。
> - **「局部性原理」在本章没有专节**。本章给的是局部性的**可量化形式**（§2.3.14 的命中率 / 缺失率），
>   而 "locality" 这个词在本书别处出现（第 6 章 CPU affinity 与 cache warmth、第 7 章 NUMA 内存局部性、
>   第 10 章 RFS），见 [[Chapter 6. CPUs]] / [[Chapter 7. Memory]]。
> - **「延迟是分布而不是标量」这句归纳是本篇的串法**：原文分别在三处讲（§2.8.3 百分位、
>   §2.8.5 多峰分布、§2.6.5 响应时间的分布），没有一处把它们统一成一句论断。
> - **「§2.5.9 + 附录 A 是同一条线」是本篇的读法**：原文本章只说附录 A 是一份 Linux 的 USE 清单，
>   没把它讲成「方法 → 清单」的配套关系。
> - 本篇把 **§2.6.5 的 M/D/1 曲线当作「延迟对利用率的二阶导」**来用，是本篇的用法；
>   原文只把它当一个算例（磁盘 1 ms 服务时间）。

## §2.1 术语：这一层统一的是「口径」

这一节是全章的**口径表**，作用是消灭「同一个词在不同人嘴里指不同东西」的混乱。
对性能工程最有杀伤力的是**两个 latency**：

> Latency : A measure of time an operation spends waiting to be serviced. In some contexts it can refer to the entire time for an operation, equivalent to response time.

即：**latency 的窄义 = 等待被服务的时间**；广义（很多上下文里）= **整段操作时间 = response time**。
原文 L71 顺带给 **response time** 定义：「包含等待时间 + 服务时间（service time），含结果传输时间」。

本节的九个词，各自有明确边界：

| 术语 | 原文的定义要点 | 用错会怎样 |
|---|---|---|
| **IOPS** | I/O 操作/秒（磁盘语境 = 读写次数/秒） | 脱离读写比、I/O 大小就没意义（见 [[Chapter 9. Disks]]） |
| **Throughput** | 完成工作的速率；通信里指数据率，数据库里可能指操作率 | 与「带宽」混用 |
| **Response time** | 操作完成所需时间 = 等待 + 服务 + 结果传输 | 与 latency 混用 |
| **Latency** | 等待被服务的时间（广义可等于 response time） | 同一个词三种口径（§2.3.1） |
| **Utilization** | 忙的时间占比（时间口径）／消耗的容量比例（容量口径） | 两套定义混用（§2.3.11） |
| **Saturation** | 资源**排队着**处理不了的工作有多少 | 与「利用率高」混为一谈 |
| **Bottleneck** | 限制系统性能的资源 | —— |
| **Workload** | 施加给系统的负载（数据库语境 = 客户端发来的查询与命令） | 把「负载」当成「负载率」 |
| **Cache** | 快的存储层复制/缓冲有限数据，避免直接访问慢层 | —— |

**为什么这一节重要**：原文 L29 说作者当年把 man page 从头读到尾、学到了 page fault / context switch
这些指标的定义，**却不知道拿到这些数字以后该干什么**——「how to move from signals to solutions」。
术语表本身不解决问题，但**它决定了后面所有方法的输入是否可比**。

## §2.2 模型：SUT 与排队系统

两个简化模型，是后面所有分析的心智脚手架。

### §2.2.1 System Under Test（SUT）

要测的系统是一个**黑盒 + 一组扰动源**。原文 L99 点出**扰动（perturbations）**的三类来源：
定时任务、系统的其他用户、其它负载；并且说「扰动的来源可能不明显，需要仔细研究才能确定」。

原文特别点名了云环境的困难：**物理宿主上的其它租户活动，在 guest SUT 内部可能根本观测不到**。

> **判据（推演）**：测出来的性能差异，先问「这是不是扰动造成的」——
> 尤其是共享宿主上的对比实验，两边若不在同一台物理机上，差异可能全部来自邻居而不是你的改动。

### §2.2.2 Queueing System

原文 L105：很多组件与资源可以建模成**排队系统**，从而**预测它们在不同情形下的性能**；
磁盘是最典型的例子。「排队论」这个领域本身在 §2.6 引入。

**为什么这个模型是全书的地基**：第 9 章的磁盘扩展、第 11 章的云容量，
都要用「排队系统 + 利用率」这套语言；§2.6.5 会把它写成公式。

## §2.3.1 延迟：定义、歧义，和「为什么延迟是好指标」

### 延迟到底指哪一段

原文 Figure 2.3 把一次网络传输拆成两段：**latency**（等连接建立）＋ **operation time**，
而 **response time 横跨这两段**。原文 L129 给了一个极实用的拆法：
一个网站的加载时间可以由**三个不同位置测出的时间**拼成——
**DNS latency**（整个 DNS 操作）、**TCP connection latency**（只算握手）、**TCP data transfer time**。

> Since the single word “latency” can be ambiguous, it is best to include qualifying terms to explain what it measures: request latency, TCP connection latency, etc.

**判据**：写报告时不要写「latency」，要写「**request latency**」「**TCP connection latency**」这种带限定词的写法，
否则两个人拿着同一个词在比不同的数。

### 延迟为什么是首选指标

> As latency is a time-based metric, various calculations are possible. Performance issues can be quantified using latency and then ranked because they are expressed using the same units (time).

三条推论（原文 L133 与 L181）：

1. **可排序**：所有延迟单位相同，能直接排优先级。
2. **可算加速比**：能估「这段延迟去掉后能快多少」。
3. **可跨类型比较**：原文 L181 举例——「100 次网络 I/O 还是 50 次磁盘 I/O 更好？」这问题涉及网络跳数、
   丢包重传率、I/O 大小、随机还是顺序、磁盘类型…没法直接比；
   但「**100 ms 的网络 I/O 时间 vs 50 ms 的磁盘 I/O 时间**」差异一目了然。

> **判据**：要跨组件比较性能，**先把一切换算成时间**。IOPS、字节率这类速率型指标单独出现时说明不了性能变化。

### 时间尺度直觉（§2.3.2，随节立篇）

原文 Table 2.2 给了一个很有说服力的换算技巧：**把一个 CPU 周期（真实世界 0.3 ns）当成 1 秒**，
于是 DRAM 访问 100 ns = 6 分钟，SSD I/O 10–100 μs = 9–90 小时，
机械盘 I/O 1–10 ms = 1–12 个月，旧金山→纽约 40 ms = 4 年。

**这条直觉的用处**：在设计阶段，「慢」不是形容词而是**可换算的量级差**——
一个慢 I/O 抵得上一万次快操作，这就是 §2.8.5「快端要靠数量堆」的直觉来源。

## §2.3.8 / §2.3.9 负载 vs 架构、扩展性与上下文切换

### §2.3.8 先分清「是负载太多，还是架构不行」

原文 L399 把两种坏法摆在一起：**架构问题**（软件配置与硬件实现不对）与**负载问题**（负载压过了系统能力）。
原文给了两端的判别式：

- **架构问题**：单线程应用占着一个 CPU，而其它 CPU 空闲，请求在排队（→ 加线程/加机器没用，要改架构）；
- 另一个架构问题：多线程程序抢同一把锁，只有一个线程能前进（→ 锁竞争）。

> An issue of load may be a multithreaded application that is busy on all available CPUs, with requests still queueing. In this case, performance is limited by the available CPU capacity, or put differently, by there being more load than the CPUs can handle.

> **判据**：**「所有 CPU 都忙 + 请求还在排队」= 负载问题；「CPU 明明闲着 + 请求还在排队」= 架构问题。**
> 这一条在 §2.5.10 的 RED 方法里被重新用了一次（请求率不变而耗时上升 ⇒ 架构；两者都涨 ⇒ 负载）。

### §2.3.9 扩展性曲线、拐点与饱和点

原文 Figure 2.6 的吞吐-负载曲线：**先线性，然后到达 knee point（拐点）**，越过之后吞吐曲线掉头向下。
拐点可能出现在某组件 100% 利用率处（**saturation point**），也可能出现在**接近 100%、排队开始频繁**时。

原文 L423 给出拐点之后的机制（顺带引出**上下文切换**）：

> After peak performance, at 100% utilization, throughput begins to decrease as more threads are added, causing more context switches, which consume CPU resources and cause less actual work to be completed.

**机制要点**：**上下文切换不是「开销」，而是「拐点之后自我加速的恶化机制」**——
线程越多 → 切换越多 → 真正干活的 CPU 越少 → 吞吐下降 → 用户再加线程 → 更多切换。
**加并发不一定加吞吐**，这是本节最该记住的一条。

原文还给出**退化快慢的两种形态**（Figure 2.7）：
- **「快」退化**：内存负载（开始换页到盘）、磁盘 I/O（空闲时 ~1 ms，负载上来后接近 10 ms）；
- **「慢」退化**：CPU 负载（调度器延迟）。

以及一种**线性退化**的特殊情况：应用资源不可用时**直接返回错误而不是排队**
（如 web 服务器返回 503），于是被服务的请求延迟保持一致——**延迟曲线线性，但错误率上升**。

> **判据**：看到「延迟涨得比负载快」，先问是哪种退化形态；
> 如果延迟没涨但错误涨了，那是**过载保护**在起作用，不是性能变好了。

### §2.6 建模视角与拐点（§2.6 / §2.6.3，随节引用）

原文 L1678 给出性能评估的三种活动与组合原则：

> Performance is best understood when at least two of these activities are performed: analytical modeling and simulation, or simulation and measurement.

三种活动 = **测量（生产观测）／模拟（实验测试）／分析建模**。
原文 L1682 定义 **knee point**：一个函数切换到另一个函数（从线性扩展切到资源争用）。

## §2.3.11 / §2.3.12 利用率与饱和：全书最容易被用错的一对概念

### 两套利用率定义

原文给利用率下了两个**正式定义**，而且它们不是一回事：

| 定义 | 出处 | 形式 | 含义 | 100% 时意味着 |
|---|---|---|---|---|
| **时间口径** | 排队论（引 Gunther 97） | `U = B / T` | 观察期内**忙的时间占比** | 只是**忙**，**还能接活** |
| **容量口径** | IT 实践（引 Wong 97） | 当前能力 / 最大能力 | 当前吞吐或操作量占**产能**的比例 | **接不了更多活了** |

> 100% busy does not mean 100% capacity.

原文用**电梯**做类比：电梯在楼层之间移动算「在用」，空闲等待算「不在用」——
**即使 100% 忙，它还能接更多乘客**（100% 利用率 ≠ 100% 容量）；
而 100% 容量指的是**载客已满**，再也塞不进人了。

原文 L499 的补充：容量口径「implies that a disk at 100% utilization cannot accept any more work」，
而时间口径「100% utilization only means it is busy 100% of the time」。

**书中声明的口径约定**（原文 L507）：**本书里的 utilization 通常指时间口径**（也叫 non-idle time）；
容量口径只用在少数容量型指标上（例如内存使用量）。

> **判据**：看到「利用率 100%」这四个字，**先问是哪套口径**。
> `iostat` 的 `%b`（percent busy）是时间口径；`df` 的磁盘占用、cgroup 的内存上限是容量口径。
> 两套口径**不可加、不可比**。

### 饱和：排队的事实

> The degree to which more work is requested of a resource than it can process is saturation . Saturation begins to occur at 100% utilization (capacity-based), as extra work cannot be processed and begins to queue.

三条要点：

1. **饱和从「容量口径 100%」开始**（超过 100% 的负载开始排队，Figure 2.8 里是线性增长的一段）；
2. **任何程度的饱和都是问题**——因为时间花在等待上（§2.5.9 的 USE 方法明确写「Any degree of saturation (non-zero) can be a problem」）；
3. **时间口径下饱和未必在 100% 开始**：

> For time-based utilization (percent busy), queueing and therefore saturation may not begin at the 100% utilization mark, depending on the degree to which the resource can operate on work in parallel.

**「过载（overload）」**这个说法在本章只出现一次（L1001 的诊断清单：磁盘 `r_await` 持续 > 10 ms ⇒
要么磁盘读慢、要么磁盘过载）。**机制上「过载」= 饱和的日常说法**：请求量超过资源交付能力，
超出部分进入队列。**把「过载」翻译成可测量的东西，就是「饱和度」（队列长度 / 等待时间）。**

> **判据**：**利用率是上限的代理，饱和才是排队的事实。**
> 利用率低 + 饱和高 ⇒ 突发负载把队列顶起来了；
> 利用率高 + 饱和低 ⇒ 稳定高负载，短期还撑得住。

## §2.3.14 缓存：局部性的可量化形式

原文 L529：缓存把慢层的结果存到快层供后续引用。
本章给的是**两个可算的量 + 一个可算的总时间**：

```
hit ratio = hits / (hits + misses)
```

原文 L545 给了一个反直觉但极重要的结论：

> The performance difference between 98% and 99% is much greater than that between 10% and 11%.

因为命中率与性能的关系是**非线性**的：命中与未命中的两个存储层速度差越大，曲线越陡。
**命中率从 98% 提到 99%，性能收益远大于 10% → 11%。**

### 缺失率：更适合跨系统比较的指标

> Another metric for understanding cache performance is the cache miss rate , in terms of misses per second. This is proportional (linear) to the performance penalty of each miss and can be easier to interpret.

原文 L549 的例子把两者的差别讲透了：工作负载 A 命中率 90%、B 命中率 80%，
**光看命中率 A 更好**；但如果 A 的缺失率是 200/s、B 是 20/s，
则 **B 的磁盘读少 10 倍**，B 可能快得多。

原文给的**总时间公式**：

```
runtime = (hit rate × hit latency) + (miss rate × miss latency)
```

**注意这个公式的隐含前提**（原文 L553 明说）：用的是**平均**命中/未命中延迟，且**假设工作被串行化**。

> **判据**：跨系统比缓存效率，**用「缺失/秒」而不是「命中率」**——
> 命中率要结合访问总量才有意义，缺失率直接对应「每秒要付多少代价」。

### §2.5.6 科学方法里的缓存实验（原文 L1045）

原文给的缓存实验设计很值得抄：**加内存扩大缓存 → 预测性能变好**；
或者反向做**负向实验**：用 tunable **人为缩小缓存 → 预测性能变差**。
后者往往更容易做（不改硬件），而且结论更硬。

### 缓存的冷/温/热（§2.3.14 末）

原文给了三个状态：**cold**（空或无用的数据，命中率≈0）、**warm**（装了有用数据但命中率还不够高）、
**hot**（常用数据，命中率 > 99%），以及 **warmth**（缓存的冷热程度）。

原文 L577 的真例子：某存储设备用 128 GB DRAM 做文件系统缓存 + 600 GB 闪存做二级缓存，
随机读负载下盘只给 ~2000 reads/s，缓存预热速率只有 16 MB/s ⇒
**DRAM 缓存要 2 小时以上才热，闪存缓存要 10 小时以上**。

> **判据**：**缓存的效果要按「预热时间」算，不只按「稳态命中率」算。**
> 一个命中率 99% 但两小时才热身的缓存，对短生命周期的实例可能毫无价值。

## §2.4 两种分析视角：资源分析 vs 工作负载分析

原文 L593 给出全书的两种视角，可以理解成**自顶向下 vs 自底向上**：

| | 资源分析（§2.4.1） | 工作负载分析（§2.4.2） |
|---|---|---|
| **起点** | CPU、内存、盘、网卡、总线、互连 | 应用与它施加的负载 |
| **主要指标** | IOPS、吞吐、利用率、饱和度 | 吞吐（事务/秒）、延迟 |
| **受众** | 系统管理员（负责物理资源） | 应用开发者与支持人员 |
| **擅长** | 判断某类资源是否到顶、容量规划 | 找出「是哪个请求、哪段代码」造成的延迟 |
| **盲区** | 看不出「负载本身没必要做」 | 看不出底层资源谁在顶 |

原文 L623 的提醒很实在：资源分析之所以流行，是因为 `vmstat(8)` / `iostat(1)` / `mpstat(1)`
的文档多，**但「这是一种视角，不是唯一的视角」**。

工作负载分析这边，原文 L641 给出**最佳性能收益的来源**：

> The best performance wins are the result of eliminating unnecessary work .

并且提醒：即使系统对**当前**负载表现很好（延迟低），**检查请求属性也可能发现可以减少的工作量**。

原文 L647 提醒了一个容易被忽略的路径：**请求很快完成，但完成时带着错误状态**——
错误导致重试，重试累积成延迟。

> **判据**：拿不到应用层指标时（比如纯数据库/中间件），用**资源视角**起步；
> 能拿到请求级指标时，**从工作负载视角起步**——
> 因为「不必要的负载」是唯一一类**加机器也解决不了**的问题。

## §2.5 方法论全景：从反方法到科学方法

原文 Table 2.4 把全章方法论分类。原文 L941 给了入口顺序：
**「第一个该用的方法永远是 problem statement（问题陈述），然后才轮到其它方法。」**

### §2.5.5 问题陈述（起点）

原文给客服/支持人员的一组固定提问：

1. 你凭什么认为有性能问题？
2. 这套系统**以前**表现好吗？
3. 最近**变了**什么？软件？硬件？负载？
4. 问题能**用延迟或运行时间**表达吗？
5. 影响别人/别的应用，还是只有你？
6. 环境是什么？软件与硬件的**版本与配置**？

> Just asking and answering these questions often points to an immediate cause and solution.

原文 L1027 补了一句很硬的话：**「我曾只靠问题陈述、在电话里解决过性能问题，一次都没登录过服务器、没看过任何指标。」**

### §2.5.1–§2.5.4 四种反方法（反面教材）

| 反方法 | 机制 | 为什么失败 |
|---|---|---|
| **§2.5.1 路灯效应** | 挑自己会用的 / 网上搜得到的工具看 | 「找到的问题可能是问题，但不是**那个**问题」；原文 L949 的醉汉找钥匙寓言 |
| **§2.5.2 随机改动** | 随机猜 → 乱改 → 看数字有没有变 | 一次改多个变量 ⇒ **不知道是哪个起了作用**（见 §2.5.6 的同款警告） |
| **§2.5.3 责怪他人** | 「不是我的问题」 | —— |
| **§2.5.4 临时清单** | 临时凑一份检查表 | 不系统 |

原文 L945 对路灯效应的定义：**「其实是无方法论的状态」**——用熟悉/随机/网上的工具看有没有明显的东西。

### §2.5.6 科学方法：提问 → 假设 → 预测 → 检验 → 分析

> The scientific method studies the unknown by making hypotheses and then testing them. It can be summarized by the following steps:

原文 L1043 给的映射：**问题 = 问题陈述；然后假设原因；构造一个检验（观察性或实验性）去检验基于假设的预测；
最后分析收集到的数据。**

原文给了三个完整例子：

- **观察性例**（L1049）：问题是「什么导致慢查询」；假设是「吵闹邻居在抢磁盘」；
  预测是「测文件系统 I/O 延迟会显示是它」；检验结果是「文件系统只占 < 5%」⇒
  **假设被否证，但大块环境已被排除**，回到第 2 步提新假设。
- **实验性例**（L1063）：把 host A 搬到与 B 同一机房；**若没好，**先回滚再提新假设——
  原文 L1075 强调：**一次改多个因素，就没法知道是哪个起了作用。**
- **负向实验例**（L1077）：故意把性能做坏来学习系统——把记录块调小 ⇒ 记录数变多 ⇒ 管理开销变大 ⇒ 预测性能逐步变差。

### §2.5.7 诊断循环（§2.5.8 Tools 方法为对照）

原文 L1095 用一个箭头式给出诊断循环：

```
hypothesis → instrumentation → data → hypothesis
```

与科学方法同源，但强调**数据能快速带出新假设**，像医生做一系列小检查。

原文 L1099 的操作建议：**尽快从假设走到数据**，好让坏理论尽早被否掉。

**§2.5.8 Tools 方法**：列工具 → 列每个工具的有用指标 → 列每个指标的可能解读，
产出「跑哪个命令、看哪几列、怎么解释」的清单。原文 L1111 指出它的**结构性缺陷**：

> Issues that require custom tooling (e.g., dynamic tracing) may never be identified and solved.

**而且使用者意识不到自己的视野是残缺的**——这正是 USE 方法要解决的（下一节）。

### §2.3.6 何时停止分析（随节立篇）

原文 L375–L383 给三个停止条件：

1. **已经解释了性能问题的主体**（原文例子：Java 应用 CPU 涨 3 倍，异常栈只占 12% ⇒ 不够，还得查）；
2. **潜在收益小于分析成本**（几十百万美元级的问题值得投入几个月；几百美元级的小微服务不值得花一小时）；
3. **别处有更大的收益**。

## §2.5.9 USE 方法：把「看工具」翻转成「看资源」

> For every resource, check utilization, saturation, and errors.

**USE = Utilization（利用率）、Saturation（饱和度）、Errors（错误）。**

原文 L1127–L1133 给的三条定义：

- **Resources**：所有物理服务器功能组件（CPU、总线……），某些软件资源也可查（**前提是指标讲得通**）；
- **Utilization**：一段时间内资源**忙着服务工作的百分比**；**忙着的时候它可能仍能接更多活**，
  接不了的部分由 saturation 刻画；
- **Saturation**：资源有**额外的、它服务不了的工作**（通常在排队上）的程度；另一个词是 **pressure**；
- **Errors**：错误事件的计数。

### 为什么它优于 Tools 方法

原文 L1139 的两句是**这一节的全部要害**：

> In contrast with the tools method, the USE method involves iterating over system resources instead of tools. This helps you create a complete list of questions to ask, and only then do you search for tools to answer them.

即：**先列问题，再找工具**。原文接着说，即便某个问题**找不到工具**，
「知道这个问题没被回答」本身也极有价值——它们变成了 **known-unknowns**（§2.3.15 的三分类：
已知 / 已知未知 / 未知未知）。

**这就是 USE 方法与 Tools 方法的根本差别**：Tools 方法的视野边界是**你知道的工具**；
USE 方法的视野边界是**系统的资源**。前者天然漏，后者把漏项变成显式清单。

### 检查顺序：Errors → Saturation → Utilization

> Errors are checked first because they are usually quick to interpret (they are usually an objective and not subjective metric), and it can be time-efficient to rule them out before investigating the other metrics. Saturation is checked second because it is quicker to interpret than utilization: any level of saturation can be an issue.

**排序的逻辑**：Errors 最客观、最好读；Saturation 只要**非零**就是问题，最好读；
Utilization 最需要判断（是时间口径还是容量口径？多高才算高？），放最后。

原文 L1137 补充了 Errors 值得先查的理由：**可恢复的失败模式会拖慢性能却不报警**
——操作失败后重试、冗余设备池里某块坏了但服务照常。

### 指标的表述形式

原文 L1157–L1161 给的三种表述（**直接抄去写报告**）：

| 指标 | 表述形式 | 例句 |
|---|---|---|
| Utilization | 一段时间内的百分比 | "One CPU is running at 90% utilization" |
| Saturation | **等待队列的长度** | "The CPUs have an average run-queue length of four" |
| Errors | 错误计数 | "This disk drive has had 50 errors" |

**注意 Saturation 的表述是「队列长度」而不是「百分比」**——这与 §2.3.12「饱和从容量 100% 开始、
线性增长」是配套的：饱和度**天生是一个绝对量**（队列里有几个），
所以用「非零即问题」这种判据才成立。

### 区间均值会藏住尖峰（原文 L1163–L1165）

> Though it may seem counterintuitive, a short burst of high utilization can cause saturation and performance issues, even though the overall utilization is low over a long interval.

原文用**收费站**做类比：全天 40% 利用率 ⇒ 你无法判断**有没有任何时刻排过队**——
早高峰时利用率是 100%，但这在日均里看不见。

> **判据**：**任何「利用率」数字都必须带着它的采样间隔一起出现。**
> 「5 分钟平均 CPU 80%」和「1 秒采样的 CPU 80%」是完全不同的两件事（§2.8.2 有真实案例）。

### 资源清单与功能框图

原文 L1169–L1183 的通用清单：**CPUs**（插槽/核/硬件线程）、**Main memory**、**Network interfaces**、
**Storage devices**、**Accelerators**、**Controllers**、**Interconnects**（CPU / 内存 / I/O）。

原文 L1185 补了一条关键分类：**同一个组件可能是多种资源类型**——
主存是**容量型**资源，网卡是 **I/O 型**资源（可以是 IOPS 也可以是吞吐），
而一个存储设备**既是 I/O 型也是容量型**资源。

原文 L1187 划了一条边界：**硬件缓存可以排除在清单之外**——因为缓存是在高利用率下**提升**性能的，
不满足 USE 方法「在高利用率/饱和下退化」的前提。

另一个做法（原文 L1191）：**画功能框图**——不仅列资源，还画出**数据流过哪些部件**，
这样才看得出「谁卡住了流」。

### USE 方法的指标对照表（原文 Table 2.6）

| 资源 | 类型 | 指标 |
|---|---|---|
| CPU | Utilization | CPU 利用率（每 CPU 或系统平均） |
| CPU | Saturation | 运行队列长度、调度器延迟、CPU pressure（Linux PSI） |
| Memory | Utilization | 可用空闲内存（系统级） |
| Memory | Saturation | 换页（匿名分页）、页面扫描、OOM 事件、内存 pressure（Linux PSI） |
| Network interface | Utilization | 收/发吞吐 ÷ 最大带宽 |
| Storage device I/O | Utilization | 设备 busy 百分比 |
| Storage device I/O | Saturation | 等待队列长度、I/O pressure（Linux PSI） |
| Storage device I/O | Errors | 设备错误（"soft"、"hard"） |

原文 L1263 说：全部组合做完大约 **30 条指标**，有些难测、有些根本测不了；
**但最常见的问题都藏在最容易测的那几条里**（CPU 饱和、内存容量饱和、网卡利用率、磁盘利用率）——
所以这些先查。原文 L1319 提醒：**难的那部分可能需要动态追踪或 CPU 性能计数器**（见第 13/14/15 章）。

### 解读判据（原文 L1341–L1347）

> Utilization at 100% is usually a sign of a bottleneck (check saturation and its effect to confirm). Utilization beyond 60% can be a problem for a couple of reasons: depending on the interval, it can hide short bursts of 100% utilization. Also, some resources such as hard disks (but not CPUs) usually cannot be interrupted during an operation, even for higher-priority work. As utilization increases, queueing delays become more frequent and noticeable.

> Saturation : Any degree of saturation (non-zero) can be a problem. It may be measured as the length of a wait queue, or as time spent waiting on the queue.

> Errors : Non-zero error counters are worth investigating, especially if they are increasing while performance is poor.

原文还强调**负例的价值**（L1347）：「低利用率、无饱和、无错误」是很好解释的结论——
它把调查范围缩小了，**排除了资源问题**。这就是**排除法**。

> **判据**：USE 方法跑完一圈**什么都没发现**，本身就是一个有价值的结论，
> **接下来该换方法**（工作负载刻画 / 延迟分析），而不是继续盯资源。

### 软件资源（原文 L1323–L1333）

| 软件资源 | Utilization | Saturation | Errors |
|---|---|---|---|
| 互斥锁 | 锁被持有的时间 | 排队等锁的线程数 | —— |
| 线程池 | 线程忙着处理工作的时间 | 等待被服务的请求数 | —— |
| 进程/线程容量 | 当前使用量 | 等待分配 | 分配失败（"cannot fork"） |
| 文件描述符容量 | 同上 | 同上 | 同上 |

### 资源控制与容器（原文 L1349–L1353）

云与容器环境里有**软件资源控制**（cgroups）限制租户的内存/CPU/磁盘 I/O/网络 I/O。
**这些限制本身也要按 USE 方法查**：「内存容量利用率」= 租户用量 ÷ 租户上限；
「内存容量饱和」= 触发限额导致的分配错误或换页——**即使宿主机毫无内存压力**。

### §2.5.10 RED 方法：USE 的服务侧对应物

原文 L1373 的一句式：**对每个服务，查 request rate、errors、duration。**

| 指标 | 含义 |
|---|---|
| **Request rate** | 每秒服务请求数 |
| **Errors** | 失败的请求数 |
| **Duration** | 请求完成时间（**原文特别要求：除平均外还要看分布统计量，如百分位**） |

原文 L1385 给出两者的分工：**USE 看机器健康，RED 看用户健康，两者互补。**

原文 L1387 给出 RED 里最有价值的一条判据（回到 §2.3.8）：

> If the request rate has been steady but the request duration has increased, it points to a problem with the architecture: the service itself.

**请求率不变而耗时上升 ⇒ 架构问题；请求率与耗时同时上升 ⇒ 可能是负载问题。**

### 连到附录 A

原文 L1321：**附录 A 是 Linux 的 USE 方法清单**——把硬件资源与 Linux 可观测工具的所有组合铺开，
并包括上一节说的软件资源。**本篇只讲方法，具体的「资源 × 指标 × 命令」清单在附录 A。**

## §2.5.11 工作负载刻画

原文 L1391 给这一节的定位：**它针对的是「负载造成的那一类问题」**——
系统可能没有任何架构、实现或配置问题，只是**负载超过了它能合理处理的量**。

原文 L1393–L1401 的四个问题（**这四个问题的顺序本身就是检查表**）：

| 问题 | 要拿到的东西 |
|---|---|
| 谁在施加负载？ | 进程 ID、用户 ID、远端 IP |
| 为什么被调用？ | 代码路径、栈回溯 |
| 负载特征是什么？ | IOPS、吞吐、方向（读/写）、类型；**适用时带上方差（标准差）** |
| 负载如何随时间变化？ | 有没有日内规律 |

原文 L1403 提醒：**即使你已经很确定答案，也要检查**——「因为你可能会惊讶」。

原文 L1405 的例子很扎心：数据库客户端本该是那池 web 服务器，
一查 IP 才发现**整个互联网都在往数据库上砸流量**——**那是一场 DoS**。

原文 L1407 给出这一节的**收益论**：

> The best performance wins are the result of eliminating unnecessary work .

而负载**消不掉**时（原文 L1409）：用**资源控制**把它降速
（备份任务压缩时抢 CPU、传输时抢网络 ⇒ 给它限流，让它慢慢跑而不伤数据库）。

原文 L1411 还给了工作负载刻画的**第二个用途**：**它是仿真基准测试的输入**——
而且强调「如果测到的是平均值，理想情况下还应同时收集分布与变化，
以便仿真出负载的多样性，而不是只测一个平均负载」。

## §2.5.12 / §2.5.13 下钻分析与延迟分析

### §2.5.12 三阶段下钻（Monitoring → Identification → Analysis）

原文 L1421 的三阶段方法论：

| 阶段 | 做什么 | 典型技术 |
|---|---|---|
| **Monitoring** | 持续记录高层统计，识别/告警 | SNMP、exporters、现代监控平台 |
| **Identification** | 把调查缩小到特定资源或区域，找可能的瓶颈 | `vmstat(8)` / `iostat(1)` / `mpstat(1)`、GUI 面板 |
| **Analysis** | 深挖可疑区域，定位并量化根因 | 追踪、profiling、`strace(1)`、`perf(1)`、BCC、bpftrace、Ftrace |

原文 L1435 给了 Netflix 的三件套对应：**Atlas（监控）／perfdash（定位，含 USE 指标）／FlameCommander + BCC + bpftrace（分析）**，
并给了串联例子：Atlas 定位到问题微服务 → perfdash 缩小到某个资源 → FlameCommander 指出吃资源的代码路径 → 用 BCC / 自定义 bpftrace 插桩。

原文 L1445 的 **Five Whys**：连问五次「为什么」；原文举的例子最终挖到了**内存分配库的碎片化问题**。

### §2.5.13 延迟分析：对延迟做二分

> Latency analysis examines the time taken to complete an operation and then breaks it into smaller components, continuing to subdivide the components with the highest latency so that the root cause can be identified and quantified.

原文 L1467 的 MySQL 例子，**每一步都是「把延迟劈成两半，钻更大的那一半」**：

1. 有查询延迟问题吗？（有）
2. 查询时间主要花在 on-CPU 还是 off-CPU？（off-CPU）
3. off-CPU 在等什么？（文件系统 I/O）
4. 文件系统 I/O 是磁盘 I/O 还是锁竞争？（磁盘 I/O）
5. 磁盘 I/O 时间主要在排队还是在服务？（服务）
6. 磁盘服务时间主要在初始化还是在数据转移？（数据转移）

原文 L1481 给这整套流程一个名字：

> For this example, each step of the process posed a question that divided the latency into two parts and then proceeded to analyze the larger part: a binary search of latency, if you will.

**「延迟的二分查找」**——这个说法值得记住，因为它同时说明了**为什么平均延迟没用**：
你要找的是**最大的那一块**，不是平均。

原文还说明延迟分析可以**逐层下钻软件栈**（应用 → 库 → 系统调用 → 内核 → 驱动），
§2.5.14 的 **Method R** 是 Oracle 的数据库版本。

## §2.5.19 / §2.5.20 微基准与性能箴言

### §2.5.19 微基准

原文 L1620 的定义：微基准测**简单且人为**的工作负载；
**宏基准**（行业基准）测真实自然的负载，通常靠仿真因而复杂。

原文 L1634 给出微基准的标准做法：

> Micro-benchmarking typically conducts the target operation as quickly as possible and measures the time for a large number of these operations to complete.

⚠️ 注意这条做法与 §2.8.5 的「平均值会骗人」是**同一枚硬币的两面**：
微基准测的就是**平均值**，所以它只能回答「这个操作单次有多快」，**不能回答「系统在负载下的延迟分布」**。

原文 L1624 的纪律：**用微基准工具，而不是裸负载生成器**（测量交给别的工具），
并且**用别的工具交叉验证**。

### §2.5.20 性能箴言（按收益从大到小）

原文 L1640 把它定义为「按最有效到最无效排序的可执行条目清单」：

```
Don’t do it.
Do it, but don’t do it again.
Do it less.
Do it later.
Do it when they’re not looking.
Do it concurrently.
Do it more cheaply.
```

原文 L1658–L1670 给的对应例子：

| 箴言 | 例子 |
|---|---|
| Don't do it | 消除不必要的工作 |
| Do it, but don't do it again | 缓存 |
| Do it less | 调低刷新/轮询/更新的频率 |
| Do it later | 回写缓存 |
| Do it when they're not looking | 把工作调度到非高峰时段 |
| Do it concurrently | 从单线程改多线程 |
| Do it more cheaply | 买更快的硬件 |

**这张表的价值在于它的顺序就是 ROI 顺序**：
「消除不必要的工作」永远排第一，「买更快的硬件」永远排最后。

## §2.6.5 排队论：Little 定律与 M/D/1 的 60%

**这一节是全书所有「利用率 → 延迟」判断的数学来源。**

### Little 定律

原文 L1785：

```
L = λ W
```

> which determines the average number of requests in a system, L , as the average arrival rate, λ, multiplied by the average request time in the system, W . This can be applied to a queue, such that L is the number of requests in the queue, and W is the average wait time on the queue.

**用法：三个量里知道两个，就能求第三个。**

| 你知道的 | 你能推的 | 用在哪 |
|---|---|---|
| 到达率 λ（QPS） | 平均在系统里的请求数 L = λW | 「我需要多少并发才够用？」 |
| 在系统里的请求数 L | 平均停留时间 W = L/λ | 「为什么延迟比预想的高？」 |
| 到达率 λ 与目标延迟 W | 允许的最大排队长度 L | **容量规划 / 限流阈值** |

原文 L1789–L1795 给了排队论**能回答的问题清单**，三个都直接对应生产决策：

- 负载翻倍后平均响应时间会变成多少？
- 加一个处理器之后平均响应时间会怎样？
- **负载翻倍时，系统还能提供「90 分位响应时间 < 100 ms」吗？**

第三个问题说明了为什么**排队论必须和百分位一起用**（呼应 §2.8.3）。

### 排队系统的三要素与 Kendall 记号

原文 L1807–L1813：**到达过程**（随机 / 定长 / 泊松）、
**服务时间分布**（定长 / 指数 / 其它）、
**服务台数量**（一个或多个）。写成 **Kendall 记号 `A/S/m`**：

| 记号 | 含义 |
|---|---|
| `M/M/1` | 马尔可夫到达（指数间隔）、马尔可夫服务时间（指数分布）、1 个服务台 |
| `M/M/c` | 同上，多服务台 |
| `M/G/1` | 马尔可夫到达、服务时间**任意分布**、1 个服务台 |
| `M/D/1` | 马尔可夫到达、服务时间**确定**（定长）、1 个服务台 |

原文 L1835：**M/G/1 常用于研究旋转硬盘。**

### M/D/1 与 60% 利用率

原文 L1839 的建模假设：一个**确定性**响应工作负载的磁盘 ⇒ 模型是 **M/D/1**。
原文 L1845 给的响应时间公式：

```
r = s (2 - ρ)/2(1 - ρ)
```

其中 `r` = 响应时间，`s` = 服务时间，`ρ` = 利用率。原文用 s = 1 ms 画了 Figure 2.19。

原文 L1866–L1875 给出生成这条曲线的 R 代码（照抄）：

```r
svc_ms <- 1                   # average disk I/O service time, ms
util_min <- 0                 # range to plot
util_max <- 100               # "
ms_min <- 0                   # "
ms_max <- 10                  # "
# Plot mean response time vs utilization (M/D/1)
plot(x <- c(util_min:util_max), svc_ms * (2 - x/100) / (2 * (1 - x/100)),
    type="l", lty=1, lwd=1,
    xlim=c(util_min, util_max), ylim=c(ms_min, ms_max),
    xlab="Utilization %", ylab="Mean Response Time (ms)")
```

**结论（原文 L1855）：**

> Beyond 60% utilization, the average response time doubles. By 80%, it has tripled.

**这就是「接近 100% 时延迟非线性爆炸」的精确版本**：
不是 100% 才炸，而是 **60% 就已经翻倍、80% 就已经三倍**。

原文还给出了**为什么磁盘比 CPU 更危险**的理由：

> ...this is different from CPUs, for example, where higher-priority work can preempt.

**磁盘 I/O 通常不可中断**（不能被高优先级抢占），必须排队等轮次；CPU 可以抢占。
⇒ **同一个 60% 利用率，对磁盘是「延迟已经翻倍」，对 CPU 只是「调度延迟上升」。**

原文 L1859 补了最重要的一句（**高分位比平均值更早爆炸**）：

> There is also a distribution of response times, not pictured in Figure 2.19 , such that the 90th and 99th percentiles degrade much faster beyond 60% utilization.

原文 L1837 还提醒：**这个模型是最乐观的情况**——
服务时间一变（用 M/G/1 或 M/M/1）平均响应时间就会更高。

> **判据**：**容量规划按 60% 划线**（不是 80%、不是 100%）——
> 这不是经验法则，是 M/D/1 的直接结论。磁盘/存储尤其要留到 50%，
> 因为磁盘不可抢占（§9.5.10 的扩展计算也是这么做的）。

## §2.8 统计：为什么「平均延迟」会骗人

### §2.8.1 量化收益（先有度量，再谈改进）

原文给了两条路：

- **观察式**（原文 L1979–L1991）：选一个可靠的指标 → 估出修好能提升多少 →
  原文的例子：请求 10 ms，其中 9 ms 是磁盘 I/O ⇒ 改成内存缓存（DRAM 约 10 μs）
  ⇒ 10 ms → 1.01 ms，约 **9 倍**提升。
- **实验式**（原文 L2001–L2013）：直接改 → 前后对比 ⇒ 例子：线程数从排队改成并发，
  平均事务延迟 10 ms → 2 ms ⇒ **5 倍**。
  原文 L2015 提醒：**生产环境里改不动时，用观察式。**

原文 L1995 有一个关键的限定：**延迟必须是「应用请求的同步组成部分」**——
有些事件是异步的（比如后台刷盘），**不影响应用性能**，算进去会虚高收益。

### §2.8.2 平均值的三重陷阱

原文 L2019–L2033 讲了三种平均值（算术 / 几何 / 调和）：

- **几何平均**：适合**层层相乘**的场景——内核网络栈每层的提升是乘法效应，
  总体提升该用几何平均（原文 L2023）；
- **调和平均**：适合**速率**——原文 L2027 的例子：800 MB 数据，前 100 MB 按 50 MB/s、
  其余 700 MB 被限到 10 MB/s ⇒ 调和平均 = 800/(100/50 + 700/10) = **11.1 MB/s**；
- **衰减平均**（decayed average）：`uptime(1)` 报的 load average 就是——**近期权重更大**。

**陷阱一：平均值是对某个区间的平均，区间长度会藏住尖峰。**
原文 L2033 的真实案例：**客户被 CPU 饱和（调度器延迟）折磨，但监控显示 CPU 利用率从没超过 80%**——
因为监控工具报的是 **5 分钟平均值**，把「CPU 冲到 100% 的那几秒」给抹平了。

**陷阱二：平均值是摘要统计，细节被藏起来了。**

> Averages are a summary statistic that hides details. I’ve analyzed many cases of occasional disk I/O latency outliers exceeding 100 ms, while the average latency was close to 1 ms.

**陷阱三：多峰分布下，平均值不是「中心趋势」，而是「反中心趋势」**（见 §2.8.5）。

> **判据**：看到「平均延迟 1 ms」这类结论，**第一反应应该是问「区间多长、分布什么样」**，
> 而不是接受这个数。

### §2.8.3 标准差、百分位、中位数

原文 L2049：标准差是**方差**的度量；99 分位是「包含 99% 数据的那个点」。
原文 L2055：

> Percentiles such as 90th, 95th, 99th, and 99.9th are used in performance monitoring of request latency to quantify the slowest in the population. These may also be specified in service-level agreements (SLAs) as a way to measure that performance is acceptable for most users.

**50 分位 = 中位数（median）**，看「大部分数据在哪」。

### §2.8.4 变异系数（CoV）

原文 L2061：

> Since standard deviation is relative to the mean, variance can be understood only when considering both standard deviation and mean. A standard deviation of 50 alone tells us little. That plus a mean of 200 tells us a lot.

原文 L2063 给出把变异压成**一个数**的办法：

```
CoV (CV) = 标准差 / 均值
```

**CoV 越小 ⇒ 变异越小。**
另一个单值表达是 **z 值**（某个值离均值几个标准差）。

> **判据**：要**跨系统比较延迟的稳定性**（而不只是快慢），用 **CoV**——
> 「延迟 10 ms ± 2 ms」与「延迟 100 ms ± 50 ms」相比，后者更不稳定（CoV 50% vs 20%）。

### §2.8.5 多峰分布：平均值的最强反例

原文 L2069：

> There is a problem with means, standard deviations, and percentiles, which may be obvious from the previous chart: they are intended for normal -like or unimodal distributions. System performance is often bimodal , returning low latencies for a fast code path and high latencies for a slow one, or low latencies for cache hits and high latencies for cache misses. There may also be more than two modes.

**注意这句话的杀伤力**：**连标准差和百分位也被点名了**——
它们是为**单峰**分布设计的。

原文 L2071–L2077 的例子：混合读写 + 随机顺序的磁盘 I/O 延迟直方图，
左边一个峰是 **< 1 ms 的盘上缓存命中**，右边一个峰在 **~7 ms（随机读、缓存未命中）**，
而平均 I/O 延迟是 **3.3 ms**：

> The average (mean) I/O latency is 3.3 ms, which is plotted as a vertical line. This average is not the index of central tendency (as described earlier); in fact, it is almost the opposite. As a metric, the average for this distribution is seriously misleading.

**3.3 ms 既不代表命中（< 1 ms）也不代表未命中（~7 ms）——它两边都不是。**

原文 L2081 的告诫：

> Every time you see an average used as a performance metric, especially an average latency, ask: What is the distribution?

### §2.8.6 离群点

原文 L2085：离群点是**极少量**、**极高或极低**、不符合预期分布的值。

原文 L2087 给了两类现实例子：

- **磁盘 I/O 离群点**：极偶尔的 I/O 超过 1000 ms，而绝大多数在 0–10 ms；
- **网络 I/O 离群点**：由 **TCP 基于定时器的重传**造成。

原文 L2089 给了统计上的处理建议：正态分布下，离群点**会拉偏均值，但不会影响中位数**；
标准差和 99 分位识别离群点的机会更大，但**仍取决于离群点的频率**。

原文 L2091 的最终处方：**看完整分布**（例如直方图）。

### §2.10 可视化（不立篇，只记一条）

原文 §2.10 的五种可视化里，本篇只取两条与统计直接相关的：

- **Figure 2.28（中位数、均值、标准差、百分位画在同一条图上）**：原文 L2169 说明
  **平均高于预期的原因**是分布里含高延迟 I/O——**1% 的 I/O 超过 20 ms**（99 分位），
  而**中位数显示的是「预期值」**——~1 ms。
  ⇒ **同一个图上，均值与中位数之间的缝隙，就是离群与多峰的量度。**
- **Figure 2.29（延迟热力图）**：原文 L2201 说，即使量程差了九倍，
  图上仍然能清楚看到**双峰**（近零 = 缓存命中，略小于 1 ms = 未命中）。

## 章末选型判据

| 场景 | 用什么 | 别用什么 |
|---|---|---|
| 报告里要写「延迟」 | 写带限定词的口径（request latency / TCP connection latency），并给**分布** | 只写「latency」三个字（§2.3.1） |
| 要跨组件比性能 | 先把一切换算成**时间**（100 ms 网络 I/O vs 50 ms 磁盘 I/O） | 比 IOPS、比字节率（§2.3.1） |
| 看到「利用率 100%」 | 先问**时间口径还是容量口径**（`%b` vs 容量占比） | 直接下「这资源满了」的结论（§2.3.11） |
| 报告一个利用率数字 | **同时给出采样间隔**（5 分钟均值 ≠ 1 秒采样） | 只报「80%」一个数（§2.5.9 / §2.8.2） |
| 怀疑资源到顶 | 查**饱和度**（队列长度 / 等待时间）：**非零即问题** | 只看利用率（§2.3.12 / §2.5.9） |
| 延迟涨、错误也涨 | 先看是不是**过载保护**（返回错误而不是排队） | 误判为「性能变好了」（§2.3.9） |
| 加并发后吞吐反而降 | 查**上下文切换**与资源争用（拐点已过） | 继续加线程（§2.3.9） |
| CPU 忙但请求还在排队 | 判为**架构问题**（单线程 / 锁竞争） | 判为负载问题去加机器（§2.3.8） |
| 请求率不变但耗时上升 | 判为**架构问题**（RED 方法的用法） | 判为负载问题（§2.5.10） |
| 存储/磁盘的容量规划 | 按 **60% 利用率**划线（M/D/1：60% 时延迟已翻倍），磁盘取更低（不可抢占） | 按峰值或 100% 配（§2.6.5） |
| 要估并发数 / 队列长度 | 用 **Little 定律** `L = λW`，三个量知二求一 | 拍脑袋估（§2.6.5） |
| 看到「平均延迟 1 ms」 | 追问**区间长度 + 分布形状**，看直方图与百分位 | 接受这个平均值（§2.8.2 / §2.8.5） |
| 要跨系统比延迟稳定性 | 用 **CoV**（标准差 ÷ 均值）与中位数 | 只比均值（§2.8.4） |
| 缓存效率要对比 | 用**缺失率（misses/秒）**并代入 `runtime = hit×hit_latency + miss×miss_latency` | 只比命中率（§2.3.14） |
| 缓存收益要算清 | 把**预热时间**算进去（冷缓存会拖垮短生命周期实例） | 只算稳态命中率（§2.3.14） |
| 缓存实验做不出来 | 用**负向实验**（用 tunable 人为缩小缓存，预测性能变差） | 一定要加硬件（§2.5.6） |
| 一次改多个东西想知道哪个起作用 | 一次只改一个，**不奏效就回滚** | 同时改多个（§2.5.6 / §2.5.2） |
| 刚接手一个新问题 | 先做**问题陈述**（六个固定问题） | 直接登录上去看指标（§2.5.5） |
| 不知道从哪下手 | 跑一遍 **USE**（先 Errors，再 Saturation，再 Utilization），**列资源清单** | 挑熟悉的工具挨个试（§2.5.9 / §2.5.8） |
| USE 跑完没发现 | 记为**排除法**的结论，换**工作负载刻画 / 延迟分析** | 继续盯着资源指标（§2.5.9 / §2.4） |
| 容器 / 云里查不到瓶颈 | 把 **cgroup 限额本身**当资源查（租户口径的利用率与饱和） | 只看宿主机指标（§2.5.9） |
| 要找「慢请求是谁发的」 | **工作负载刻画**四问（谁 / 为什么 / 特征 / 随时间变化） | 凭猜（§2.5.11） |
| 怀疑是「负载本身没必要」 | 查**不必要的工作**（备份撞高峰、线程空转、DoS） | 加机器（§2.5.11 / §2.5.20） |
| 有一段说不清的延迟 | **延迟二分**：每一问把延迟劈两半，钻更大的那半 | 看平均延迟（§2.5.13） |
| 要决定还查不查 | 三个停止条件：已解释主体 / ROI < 分析成本 / 别处 ROI 更大 | 无限深挖（§2.3.6） |
| 要改性能 | 按**性能箴言**的顺序来（消除不必要工作 → 缓存 → 少做 → 晚做 → 闲时做 → 并发 → 买硬件） | 一上来买硬件（§2.5.20） |
| 压测要测什么 | 微基准测**单次操作的平均耗时**；系统级性能分布另有其法 | 用微基准的平均值代表系统的延迟分布（§2.5.19 / §2.8.5） |
| 用别人给的调优参数 | 只把它当**线索**（有哪些参数、历史上调过什么），在自己的系统与负载上验证 | 直接照抄（§2.3.7） |
| 改可调参数 | 存进**版本控制**并记下时间与原因 | 改完不留痕（§2.3.7） |
