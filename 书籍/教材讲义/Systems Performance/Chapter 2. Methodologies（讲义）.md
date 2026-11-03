---
title: Chapter 2. Methodologies（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - Systems Performance
  - 性能工程
  - USE方法
  - RED方法
  - 排队论
source: "《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg，Addison-Wesley 2020，英文原版）第 2 章"
companion: "[[02-Chapter 2. Methodologies]]"
---

# 第 2 章 讲义：先问「要多快」，再问「慢在哪」

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[02-Chapter 2. Methodologies|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：`otf-server` 目前**没有**成体系的 USE 指标；本篇末尾给出一份针对它的具体补测清单。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原章。

---

## 0. 开局：性能投诉的第一句话应该是问句

工单进来：「服务器慢。」

老王看了一眼，第一句话不是打开 `top`，而是回电话：

> 「慢，是**谁**在慢？你说的是哪个操作？从什么时候开始？之前快吗？最近改过什么？」

六个问题问完，工单的描述从「服务器慢」变成：「**下午 3 点开始，JoinRoom 操作平均要 800 ms，之前是 20 ms；只有从某个 IP 段进来的客户端会这样**。」

到这一步，方向已经很清楚了。这就是第 2 章的全部内容：**性能分析不是「找慢的地方」，是「一套把模糊抱怨变成可测假设的流程」**。

本章的骨架可以先记住四句话：

1. **先定义目标**（要多快），再定位问题（慢在哪）。
2. **两种视角**：资源分析 vs 负载分析。
3. **USE / RED**：把「该问哪些问题」变成清单，而不是凭灵感。
4. **什么时候停**：找到能解释大部分现象的原因就该停，剩下的 ROI 不划算。

---

## 1. §2.1 Terminology / §2.3 Concepts：先把词定死

### 1.1 §2.3.1 Latency：延迟是「等待」

> The latency is the time spent waiting before an operation is performed.

原文用 `Figure 2.3` 拆解一次 HTTP GET：

```
点击 ──DNS 查询──┐
                 ├─TCP 握手─┐
                            ├─传输数据──┐
                                       └─响应
   └─── 这几段合起来叫「响应时间」──┘
   └──── 只有前面的等待段才是「延迟」───┘
```

**关键点**：延迟**可以从不同位置测量**，所以原文要求**带上测量目标的限定词**：

> it is best to include qualifying terms to explain what it measures: request latency, TCP connection latency, etc.

（DNS latency、TCP connection latency、TCP data transfer time；再往上，「从点链接到页面渲染完」又可以整体叫 latency。）

延迟是**时间**单位，因此能做两件别的事做不到的事：**量化问题规模** + **算潜在加速比**（第 1 章已述）。

### 1.2 §2.3.2 Time Scales：对时间量级建立直觉

原文给了 `Table 2.2`，把一切延迟按「CPU 周期算 1 秒」重标定：

| 事件 | 真实延迟 | 缩放后 |
|---|---|---|
| 1 个 CPU 周期 | 0.3 ns | 1 秒 |
| L1 cache 访问 | 0.9 ns | 3 秒 |
| 主内存访问（DRAM） | 100 ns | 6 分钟 |
| 固态盘 I/O | 10–100 μs | 9–90 小时 |
| 机械盘 I/O | 1–10 ms | 1–12 个月 |
| 旧金山 → 纽约 | 40 ms | 4 年 |
| 旧金山 → 澳大利亚 | 183 ms | 19 年 |
| 物理重启 | 5 分钟 | 32000 年 |

这张表的用处不是炫技，是**让人对「什么算快」有直觉**。原文说：「光走 0.5 米（大约你眼睛到这本书的距离）约 1.7 ns；在这段时间里，现代 CPU 能执行 5 个周期、处理若干指令。」

### 1.3 §2.3.3 Trade-Offs：好、快、便宜只能选两个

`Figure 2.4` 是经典的「pick two」，原文配上了 IT 项目的翻译：**很多项目选了「按时 + 便宜」，把性能留给以后** —— 而这个选择往往在早期就锁死了：

> This choice can become problematic when earlier decisions inhibit improving performance, such as choosing and populating a suboptimal storage architecture, using a programming language or operating system that is implemented inefficiently, or selecting a component that lacks comprehensive performance analysis tools.

还有一类更常见的权衡：**CPU ↔ 内存**。内存可以拿来缓存结果以省 CPU；现代系统 CPU 富余时，方向可以反过来（用 CPU 去压缩数据省内存）。可调参数几乎都带权衡——原文举了文件系统记录大小：小的适合随机 I/O，大的适合流式。

### 1.4 §2.3.6 When to Stop Analysis：什么时候停

这是本章最少被讲、却最实用的一节。原文说他在教学时给学生一个**有三个成因**的性能问题，结果有人找到一个就停，有人找两个，有人三个，还有人继续找第四个——**谁是对的？**

原文给的三个停止条件：

1. **你已经解释了性能问题的主体。** 原文的例子：一个 Java 应用 CPU 是原来的 3 倍，第一个找到的原因是异常栈在烧 CPU；量化后发现它只占整体的 **12%**——离「能解释 3 倍」的 66% 还差得远，所以必须继续查。
2. **潜在 ROI 低于分析成本。** 有些问题一年能省几千万美元，值得花几个月；有些小微服务一年只值几百美元，一小时都不值得。
3. **别处有更大的 ROI。**

### 1.5 §2.3.7 Point-in-Time Recommendations：调优建议有时效

> Performance recommendations, especially the values of tunable parameters, are valid only at a specific point in time

原文的比喻很传神：网上抄来的调优参数像是**去翻别人家的药柜抓药**——可能不适合你、可能过期了、可能只该短期吃。

原文给的补救办法：**把调过的参数存进版本控制，带详细历史**，这样以后能查清「什么时候、为什么改的」。

### 1.6 §2.3.8 Load vs. Architecture：负载问题还是架构问题

原文用 `Figure 2.5` 区分两种病：

| | 架构问题 | 负载问题 |
|---|---|---|
| 单线程应用，CPU 忙，请求排队，其他 CPU 空闲 | ✓ | |
| 多线程但抢一把锁，只有一个能推进 | ✓ | |
| 多线程，所有 CPU 都忙，请求还在排队 | | ✓（CPU 容量不够） |

**这两种病的药方完全相反**：架构问题要改代码/加实例，负载问题要扩容或削负载。分错了方向，越努力越糟。

### 1.7 §2.3.9 Scalability：拐点

> This point can be described as a knee point, as it is the boundary between two functions.

`Figure 2.6` 是标准的吞吐-负载曲线：先线性上升，到**膝点**后偏离线性，最终吞吐**下降**（因为争抢与一致性开销吃掉了有效产出）。

膝点可能出现在「某资源 100% 利用率」（**饱和点**），也可能出现在「接近 100% 且开始频繁排队」时。

`Figure 2.7` 给了两种**退化曲线**：

- **快退化**（内存负载：开始把页换到磁盘；磁盘 I/O：空闲时约 1 ms，负载起来接近 10 ms）
- **慢退化**（CPU 负载：调度器延迟上升）

还有第三种：**直接返回错误**。Web 服务器资源不足时返回 503 而不是排队，于是「被服务的请求」响应时间保持一致——延迟曲线看起来是平的，但吞吐已经封顶。

### 1.8 §2.3.10 Metrics / §2.3.11 Utilization / §2.3.12 Saturation

**指标类型**（原文列举的常见四类）：

| 指标 | 含义 | 依赖上下文吗 |
|---|---|---|
| Throughput | 每秒操作数或数据量 | 是（数据库按操作，网络按字节） |
| IOPS | 每秒 I/O 操作数 | 是 |
| Utilization | 资源有多忙（百分比） | 否 |
| Latency | 操作时间（均值或百分位） | 否 |

原文特别提醒**指标不是免费的**：

> Performance metrics are not free; at some point, CPU cycles must be spent to gather and store them.

并且厂商给的指标常常并不可靠：

> metrics can be confusing, complicated, unreliable, inaccurate, and even plain wrong (due to bugs). Sometimes a metric was correct in one software version but did not get updated to reflect the addition of new code and code paths.

**利用率有两种定义**，原文特意分开：

- **时间型（time-based）**：一段时间里资源「忙」的时间百分比。**忙的时候它仍可能接受更多工作**，不能接受的程度要看**饱和**。
- **容量型（capacity-based）**：主内存用容量百分比。**到 100% 就不能再接活了**，要么排队（饱和）要么报错。

**饱和**：

> Saturation begins to occur at 100% utilization (capacity-based), as extra work cannot be processed and begins to queue.

> Any degree of saturation is a performance issue, as time is spent waiting (latency).

注意最后半句：**饱和不是「有点饱和没关系」，饱和就是延迟。** 时间型利用率下，排队未必等到 100% 才开始——取决于该资源能否并行处理工作。

---

## 2. §2.4 Perspectives：资源分析 vs 负载分析

原文给了一张对照（`Figure 2.10`），可以背：

|  | **Resource Analysis（资源分析）** | **Workload Analysis（负载分析）** |
|---|---|---|
| 谁在做 | 系统管理员 | 应用开发者 / 支持人员 |
| 视角 | 自底向上 | 自顶向下 |
| 目标 | 某类资源是否为瓶颈 | 应用对所受负载的响应 |
| 典型活动 | 性能问题排查、容量规划 | 性能问题排查、容量规划 |
| 关注指标 | IOPS、Throughput、Utilization、Saturation | Throughput、Latency |
| 起点 | CPU、内存、磁盘、网卡、总线 | 请求 |

资源分析这一侧原文提醒：不要把 `vmstat(8)` / `iostat(1)` / `mpstat(1)` 的手册读成「这就是性能分析的全部」——**这只是视角之一**。

负载分析这一侧，原文给出**最实用的两个动作**：

> Latency (response time) is the most important metric for expressing application performance.

以及那句最该贴在墙上的话：

> Keep in mind that the fastest query is the one you don’t do at all.

（先做 **workload characterization**，找有没有**不必要的活**。）

原文还强调负载分析要**从应用出发往深挖**：应用 → 库 → 系统调用 → 内核。

---

## 3. §2.5 Methodology：方法论总览

原文说：面对复杂的性能环境，第一难是**知道从哪开始、从哪往下走**。

这一节的骨架是：

```
§2.5.1  Streetlight Anti-Method      （只查手边有的）
§2.5.2  Random Change Anti-Method    （瞎改）
§2.5.3  Blame-Someone-Else Anti-Method（甩锅）
§2.5.4  Ad Hoc Checklist Method      （临时清单）
§2.5.5  Problem Statement            ★ 起点
§2.5.6  Scientific Method            ★ 假设-预测-实验
§2.5.7  Diagnosis Cycle              ★ 假设→埋点→数据→假设
§2.5.8  Tools Method                 （以工具为中心）
§2.5.9  The USE Method               ★ 资源视角
§2.5.10 The RED Method               ★ 服务视角
§2.5.11 Workload Characterization     ★ 负载视角
§2.5.12 Drill-Down Analysis         ★ 逐层下钻
§2.5.13 Latency Analysis             ★ 延迟二分
§2.5.14 Method R
§2.5.15 Event Tracing
§2.5.16 Baseline Statistics
§2.5.17 Static Performance Tuning
§2.5.18 Cache Tuning
§2.5.19 Micro-Benchmarking
§2.5.20 Performance Mantras           ★ 调优口诀
```

### 3.1 §2.5.5 Problem Statement：起点

原文列了 6 个问题（对客户/报告人问）：

1. 你**凭什么**觉得有性能问题？
2. 这系统**以前**表现好吗？
3. **最近改了什么**？软件、硬件、负载？
4. 能不能用**延迟或运行时间**来表述？
5. 影响的是**别人**还是只有你？
6. **环境**是什么？软硬件版本、配置？

原文的结论相当激进：

> Just asking and answering these questions often points to an immediate cause and solution.

> I have solved performance issues over the phone by using the problem statement method alone, and without needing to log in to any server or look at any metrics.

### 3.2 §2.5.6 Scientific Method / §2.5.7 Diagnosis Cycle

科学方法五步：**Question → Hypothesis → Prediction → Test → Analysis**。

原文给的第一个例子（**观测型**）：

```
问题：    什么导致数据库查询慢？
假设：    吵闹的邻居租户在做磁盘 I/O，与数据库的 I/O 争抢
预测：    如果在查询期间测文件系统 I/O 延迟，会显示文件系统是慢的原因
测试：    追踪文件系统 I/O 延迟与查询延迟之比
分析：    只有不到 5% 的时间在等文件系统
结论：    文件系统与磁盘**不是**原因（问题仍未解决，但排除了大块）
```

第二个例子（**实验型**）：

```
问题：    为什么从主机 A 发 HTTP 请求到主机 C 比从主机 B 发更慢？
假设：    A 和 B 在不同数据中心
预测：    把 A 挪到和 B 同一个数据中心就能修好
测试：    挪 A，测性能
分析：    修好了 —— 与假设一致
```

原文特别提醒：**如果没修好，先把实验改动回滚**，再去提新假设——**一次改多个因素，就分不清是哪个起了作用**。

第三个例子是**故意做坏**的负向实验（减少缓存大小让性能变差），用来验证「更大的缓存需要更多计算来管理」这个假设。

**诊断循环**：

> hypothesis → instrumentation → data → hypothesis

原文说它和科学方法一样是「理论与数据的平衡」，并且强调**尽快从假设走到数据**，好让错的理论早点被淘汰。

### 3.3 §2.5.8 Tools Method 及其反面

工具法：列工具 → 列每个工具能给的指标 → 列每个指标怎么解释 → 得到一份「跑哪个命令、看哪个数、怎么判断」的处方。

原文批评它：**完全依赖已有工具**，视野可能不完整，而且**使用者意识不到自己不完整**——需要动态追踪才能抓的问题可能永远不会被发现。

### 3.4 §2.5.9 The USE Method ★

这是本章最该带走的方法。全文就一句话：

> For every resource, check utilization, saturation, and errors.

三个词的定义（原文原话）：

- **Resources**：服务器的所有物理功能组件（CPU、总线……），有些软件资源也可以查，**前提是指标讲得通**。
- **Utilization**：一段时间内资源**忙于服务工作的百分比**。**忙的时候它可能仍能接活**；接不了的程度由饱和度刻画。
- **Saturation**：资源被要求处理超出其能力的程度，通常表现为**排队等待**。另一个说法叫 **pressure**。
- **Errors**：错误事件的计数。

原文还专门解释了**为什么先查错误**（快、客观）和**为什么饱和排第二**（任何程度的饱和都算问题，比利用率更容易解释）。

**指标怎么写**：

```
Utilization : 某时间区间内的百分比（"One CPU is running at 90% utilization"）
Saturation  : 等待队列长度（"The CPUs have an average run-queue length of four"）
Errors     : 错误数量（"This disk drive has had 50 errors"）
```

**关键警告**：

> Though it may seem counterintuitive, a short burst of high utilization can cause saturation and performance issues, even though the overall utilization is low over a long interval.

原文的收费站比喻：一排收费亭，100% 利用率 = 找不到空亭子只能排队（饱和）；「全天平均 40% 利用率」能告诉你白天有没有车排过队吗？**不能**——早高峰 100% 的那两小时被平均掉了。

**资源清单**（原文给的通用清单）：CPU（插槽/核/超线程）、主内存、网络接口、存储设备、加速器、控制器、互连。

**软件资源**（原文单列了一节，很实用）：

| 软件资源 | 利用率 | 饱和 | 错误 |
|---|---|---|---|
| 互斥锁 | 锁被持有的时间 | 排队等锁的线程数 | — |
| 线程池 | 线程忙于处理工作的时间 | 等待被服务的请求数 | — |
| 进程/线程容量 | 当前使用量 | 等待分配 | 分配失败（"cannot fork"） |
| 文件描述符容量 | 同上 | 同上 | 同上 |

**解释建议**（原文）：

- **Utilization 100% 通常是瓶颈的信号**（还要看饱和来确认）；**超过 60% 就可能有问题**。
- **Saturation：任何非零都是问题**。
- **Errors：非零就值得查**，尤其在性能差的同时还在涨。

以及那个被低估的收益：

> narrowing down the scope of an investigation can help you focus quickly on the problem area, having identified that it is likely not a resource problem. This is the process of elimination.

**资源控制**（云与容器）：cgroups 限的资源也可以套 USE——「内存容量利用率」= 租户用量 / 上限；「内存容量饱和」= 分配报错或该租户在换页。

**微服务**：Netflix 的 USE 指标是——利用率 = 整个实例集群的平均 CPU；饱和 = **99 分位延迟 − 平均延迟**（假设 99 分位是饱和驱动的）；错误 = 请求错误数。

### 3.5 §2.5.10 The RED Method ★

RED 面向**服务**（尤其微服务）：

> For every service, check the request rate, errors, and duration.

- **Request rate**：每秒服务请求数
- **Errors**：失败请求数
- **Duration**：请求完成时间（**要分布统计/百分位，不只是平均**）

原文给的判断表：

| 现象 | 指向 |
|---|---|
| 请求率**稳定**，请求时长变长 | **架构**问题（服务自身） |
| 请求率**和**时长都涨 | **负载**问题 |

以及两句总结：

> These methodologies are complementary: the USE method for machine health, and the RED method for user health.

### 3.6 §2.5.11 Workload Characterization ★

**关注输入而非结果**。系统可能没有架构/实现/配置问题，只是**负载超过了它能承受的量**。

四组问题：

1. **谁**造成的负载？PID、UID、远端 IP？
2. **为什么**被调用？代码路径、堆栈？
3. 负载的**特征**？IOPS、吞吐、方向（读/写）、类型？**包括方差（标准差）**。
4. 负载**随时间怎么变**？有日周期吗？

原文举了个让人后背发凉的例子：你知道数据库的客户端是一批 web 服务器，还是去查了 IP —— 结果发现**整个互联网都在往数据库灌负载**，你正遭受 DoS。

原文的结论句：

> The best performance wins are the result of eliminating unnecessary work.

**负载消不掉怎么办**？用**资源控制**把它压下去（备份任务压缩 + 传输吃 CPU 和网络，可以限速）。

### 3.7 §2.5.12 Drill-Down Analysis：三层下钻

原文引用 McDougall 的三阶段方法：

| 阶段 | 做什么 | Netflix 的对应 |
|---|---|---|
| **Monitoring** | 持续记录高层统计，发现/预警问题 | Atlas |
| **Identification** | 把范围缩小到特定资源或区域，找可能的瓶颈 | perfdash（Vector） |
| **Analysis** | 深入检查可疑区域，找根因并量化 | FlameCommander + Ftrace/BCC/bpftrace |

> Atlas may identify a problem microservice, perfdash may then narrow the problem to a resource, and then FlameCommander shows the code paths consuming that resource,

**Five Whys** 也在这一节：连问五次「为什么」。

原文的例子（真实案例）：数据库查询慢 → 被磁盘 I/O 拖慢（换页）→ 内存占用涨太大 → 分配器吃内存过多 → **分配器有内存碎片问题** —— 最后修的是**系统内存分配库**。

### 3.8 §2.5.13 Latency Analysis：延迟二分法

思路：把一次操作的耗时**拆成两半**，盯住大的那半，继续拆，直到找到根因并量化。

原文给的 MySQL 查询示例（每一问都把延迟二分）：

```
1. 有查询延迟问题吗？                       → 有
2. 查询时间主要花在 on-CPU 还是 off-CPU？   → off-CPU
3. off-CPU 在等什么？                        → 文件系统 I/O
4. 文件系统 I/O 的时间是磁盘 I/O 还是锁竞争？ → 磁盘 I/O
5. 磁盘 I/O 时间主要花在排队还是服务？       → 服务
6. 磁盘服务时间主要花在 I/O 初始化还是数据传输？→ 数据传输
```

> each step of the process posed a question that divided the latency into two parts and then proceeded to analyze the larger part: a binary search of latency, if you will.

### 3.9 §2.5.15 Event Tracing / §2.5.16 Baseline Statistics

**事件追踪**处理的是离散事件（CPU 指令、磁盘命令、网络包、系统调用、库调用、应用事务、数据库查询）。摘要会丢信息，**逐事件看**才看得见。

追踪时要找三类信息：**Input**（请求的全部属性：类型、方向、大小）、**Times**（开始、结束、延迟）、**Result**（错误状态、实际传输量）。

原文还提醒一个反直觉点：

> An uncommonly high latency event, known as a latency outlier, may be caused by previous events rather than the event itself.

（队尾那个高延迟事件，可能是被前面排队的那些害的，不是它自己的属性。）

**Baseline statistics**（基线统计）是监控面板之前就有的老办法：在「正常」负载下把**所有**系统指标采一份存起来；改动前后各采一份，用来对比。原文提醒 profiler/追踪器也能进基线，但要**注意开销**，别扰动生产。

### 3.10 §2.5.20 Performance Mantras ★

原文给的调优口诀，**从最有效到最不有效**排序：

```
1. Don’t do it.                      （别干这事）
2. Do it, but don’t do it again.     （干，但别再干 —— 缓存）
3. Do it less.                       （少干 —— 调低刷新/轮询频率）
4. Do it later.                      （晚干 —— 写回缓存）
5. Do it when they’re not looking.   （没人看时干 —— 挪到低峰）
6. Do it concurrently.               （并发干 —— 单线程改多线程）
7. Do it more cheaply.               （便宜地干 —— 买更快的硬件）
```

原文说这是他最喜欢的几个方法论之一，学自 Netflix 的 Scott Emmons（他归功于 Craig Hanson 和 Pat Crain）。

> （推演）这张表对本仓库的启示顺序很明确：本仓库第 1 条能做的事是「**别发不该发的包**」和「**别做不必要的模拟计算**」；第 2 条是缓存；第 3 条是「快照别发那么频」；第 6 条（并发）恰恰是本仓库**明确不打算做**的（单线程 reactor 是刻意的架构选择，见下文锚点）。

---

## 4. §2.6 Modeling：模型

原文把性能评估分成三种活动：

- **measurement（测量）**：观测生产系统。
- **simulation（仿真）**：做实验。
- **analytical modeling（解析建模）**：建模。

> Performance is best understood when at least two of these activities are performed: analytical modeling and simulation, or simulation and measurement.

### 4.1 §2.6.3 Amdahl's Law of Scalability

```
C(N) = N / (1 + α(N – 1))
```

`N` 是扩展维度（CPU 数、用户数），`α ∈ [0,1]` 是**串行度**。三步走：收集一系列 N 的数据 → 回归出 α → 把数据与模型一起画出来看差异。

原文给了 gnuplot 的拟合代码（`fit amdahl(x) filename every ::1::inputN using 1:2 via alpha`）。

### 4.2 §2.6.4 Universal Scalability Law

```
C(N) = N / (1 + α(N – 1) + βN(N – 1))
```

多了一个 **β（一致性延迟参数）**；β = 0 时退化为 Amdahl。

### 4.3 §2.6.5 Queueing Theory ★

**Little's Law**：

> Little’s Law can be expressed as

```
L = λW
```

系统里的平均请求数 `L` = 平均到达率 `λ` × 平均停留时间 `W`。

排队系统用 **Kendall 记号** `A/S/m` 分类：

| 记号 | 含义 |
|---|---|
| M/M/1 | 泊松到达 / 指数服务 / 单服务台 |
| M/M/c | 同上，多服务台 |
| M/G/1 | 泊松到达 / 一般服务时间 / 单服务台（**常用于研究机械硬盘**） |
| **M/D/1** | 泊松到达 / **确定性**服务时间 / 单服务台 |

**M/D/1 与 60% 利用率**（`Figure 2.19`）：

```
r = s(2 – ρ) / 2(1 – ρ)
```

服务时间 `s` = 1 ms 时：

> Beyond 60% utilization, the average response time doubles. By 80%, it has tripled.

这就是「**磁盘利用率在远未到 100% 时就成了问题**」的数学原因：磁盘请求**不可中断**、必须排队，而 CPU 可以被高优先级抢占。

原文还提醒：这是**最好情况**——服务时间一变（M/G/1、M/M/1）平均响应时间更高；而且 **90 分位、99 分位在 60% 之后退化得比平均快得多**。

原文附了 R 的绘图代码（`svc_ms * (2 - x/100) / (2 * (1 - x/100))`）。

---

## 5. §2.8 Statistics：统计

### 5.1 §2.8.1 Quantifying Performance Gains

原文区分**基于观测**（改动前后对比）与**基于实验**（受控 A/B）的收益量化，并提醒：两种方法各有偏差来源。

### 5.2 §2.8.2 Averages

平均值是**会藏细节的汇总统计**：

> Averages are a summary statistic that hides details. I’ve analyzed many cases of occasional disk I/O latency outliers exceeding 100 ms, while the average latency was close to 1 ms.

原文还给了几何平均、调和平均、时间平均、**衰减平均**（decayed average）等变体。

### 5.3 §2.8.3 Standard Deviation, Percentiles, Median

- **标准差**：方差度量，越大越分散。
- **百分位**：99 分位 = 分布里包含 99% 数据的那个点。90/95/99/99.9 分位常用于监控请求延迟、也常写进 SLA。
- **中位数（50 分位）**：显示**数据主体在哪**。

### 5.4 §2.8.4 Coefficient of Variation

> A standard deviation of 50 alone tells us little. That plus a mean of 200 tells us a lot.

**CoV = 标准差 / 均值**，例子中是 25%。**CoV 越低，波动越小**。另一个表达是 z 值（离均值几个标准差）。

### 5.5 §2.8.5 Multimodal Distributions ★

> System performance is often bimodal

—— 快代码路径返回低延迟、慢路径返回高延迟；缓存命中低延迟、未命中高延迟。

`Figure 2.23` 的磁盘 I/O 分布有两个峰：左边 < 1 ms 是**磁盘缓存命中**，右边峰值约 7 ms 是**随机读未命中**。平均是 3.3 ms。

> This average is not the index of central tendency... in fact, it is almost the opposite. As a metric, the average for this distribution is seriously misleading.

原文引用了比尔·盖茨的那句名言（关于「平均水深六英寸」淹死人的轶事），然后给出全章最实用的一句：

> Every time you see an average used as a performance metric, especially an average latency, ask: What is the distribution?

### 5.6 §2.8.6 Outliers

> Their presence can be difficult to identify from most metric types, other than as a maximum.

（少数极慢的 I/O 能造成严重问题，但从多数指标类型里看不出来——**只有最大值能看见**。）

原文的结论：**要看完整分布**（比如直方图）。

### 5.7 §2.9 Monitoring / §2.10 Visualizations

**监控**记录时间序列，用于容量规划、量化增长、展示峰值；历史值提供「正常范围」的上下文。原文提到 **summary-since-boot** 类工具（`sar` 里的一个选项）。

**可视化**的价值：

> Visualizations allow more data to be examined than can be easily understood (or sometimes even displayed) as text. They also enable pattern recognition and pattern matching.

| 图 | 用途 | 陷阱 |
|---|---|---|
| **折线图** | 时间序列；可叠多条线（每盘一条、或中位数/标准差/百分位各一条） | 平均值会骗人（`Figure 2.28` 里 99 分位 > 20 ms，而中位数约 1 ms） |
| **散点图** | 每个 I/O 一个点（x=完成时刻，y=延迟），**能看出离群点** | 点重叠成「一墙油漆」；数据量与处理成本高 |
| **热力图（column quantization）** | 把 x/y 分桶着色，解决散点图的可扩展性问题 | — |
| **时间线图 / 瀑布图** | 前端分析；后端用来画线程/CPU 时间线 | — |
| **曲面图** | 三维（`Figure 2.33` 是 300+ 服务器、5312 CPU 的每 CPU 利用率曲面） | — |

原文最后提醒：**Unix 性能分析历来靠文本工具**，因为图形访问慢、往往要「追踪—出报告」一轮。**处理紧急问题时，能多快拿到指标本身就很关键。**

---

## 6. 实战锚点：给 otf-server 补一套 USE 指标

前面讲的是方法。现在落到具体仓库。

### 6.1 现状盘点：这个仓库「没有 USE 方法」

先把话说明白：**`otf-server` 目前既没有 USE 方法，也没有 RED 方法**。它有的是 `src/net/metrics.h` 里那 12 个指标（6 个累加 + 2 个峰值 + 4 个 epoll 计数），以及 `src/net/metrics_server.cpp` 提供的 Prometheus 文本端点（端口 `kMetricsPort = 7778`，`src/config/server_config.h`）。

对照 USE 的三问：

| USE 维度 | 本仓库现状 | 判定 |
|---|---|---|
| **Utilization** | 无。没有任何「资源有多忙」的指标 | **缺** |
| **Saturation** | 无。没有排队长度、没有「等待」度量 | **缺** |
| **Errors** | 有：`otf_heartbeat_timeouts`、`otf_oversize_rejects` | **有** |

也就是说：**三问只答了一问**，而且答的那一问（错误）恰好是最容易答的一问。

对照 RED 的三问：

| RED 维度 | 本仓库现状 | 判定 |
|---|---|---|
| **Request rate** | `otf_frames_recv` / `otf_frames_sent` 是累加值，**相减**才能得到速率 | **半有**（缺时序） |
| **Errors** | 同上，有 | **有** |
| **Duration** | 只有 `otf_snapshot_send_ms_max`（**峰值**，且是「快照发送耗时」，不是玩家可见延迟） | **几乎缺** |

**最关键的一条**：本仓库**没有玩家可见的延迟指标**。玩家抱怨的是「操作到生效多久」，本仓库量的是「服务器广播一个快照花了多久」——这是**服务器内部开销**，不是**用户体验**。

### 6.2 为什么这个缺口在本仓库尤其要紧

`otf-server` 是**单线程 reactor**。原文说：

> Apart from increased throughput of CPU work, multiple threads (or processes) is one way to allow I/O to be performed concurrently... (The other way is asynchronous I/O.)

本仓库走的是**另一条路**：asynchronous I/O（epoll），且**逻辑与 IO 不拆**——`main.cpp` 的主循环里，`transport.Poll(...)` 之后紧接 `sched.Advance(...)` 跑模拟。这带来一个直接后果：

> **主循环是单线程串行的，所以「CPU 忙」和「等 IO」无法并行。**

一旦某一帧的模拟计算变重，**整个游戏的所有玩家一起卡**。用 USE 的语言说：本仓库的 CPU 是**单资源、100% 利用率即饱和**（容量型）；而且因为没有并行余量，**「等 epoll 事件」的时间也无法用来做别的事**。

这正是 §2.3.9 说的**膝点问题**：负载上来时，`sched.Advance` 的耗时逼近 50 ms tick 周期，主循环就开始丢帧。本仓库其实**已经埋了一个这个问题的探针**——`TickScheduler::Result::dropped`（补帧上限 5 帧生效时丢弃的帧数），`main.cpp` 里会打 warn：

```cpp
if (r.dropped > 0) {
  log::warn_f("补帧上限生效：丢弃 " + std::to_string(r.dropped) + " 帧（本次跑了 " + ... + " 帧）", ...);
}
```

**但它只出现在日志里，不在指标端点上。** 这是本仓库最该补的一个指标。

### 6.3 具体补测清单（按优先级）

下面是给本仓库的 USE/RED 补测清单。**每一项都写明该加在哪、量什么、单位是什么。**

#### 优先级 A：饱和度（最缺，也最致命）

| # | 指标 | 单位 | 从哪来 | 为什么 |
|---|---|---|---|---|
| A1 | `otf_tick_dropped_total` | 帧（counter） | `main.cpp` 里 `r.dropped > 0` 那处调 `metrics.on_...` | **饱和的直接证据**：补帧上限触发 = 主循环单线程资源已饱和 |
| A2 | `otf_tick_lag_ms` | ms（gauge，每轮取最新） | `sched.DriftMs(post_poll)` | 逻辑帧落后墙钟多少。这是本仓库版的「排队长度」 |
| A3 | `otf_main_loop_cost_ms_max` | ms（gauge 上的 max） | 在 `main.cpp` 里量 `transport.Poll()` 与 `sched.Advance()` 各自的耗时 | 定位是 **Poll 慢**（IO 侧）还是 **Advance 慢**（CPU 侧）。现在这两个耗时**完全不可见** |

A3 是整套清单里最有价值的一条：它把「CPU 忙」和「等 IO」**劈成两半**，而这正是 §2.5.13 延迟二分法在本仓库的形态。

> （推演）注意 A2 与现有的 `otf_tick_drift_max_ms` 语义重叠：后者是**带符号的峰值漂移**，A2 是**当前漂移**（gauge）。两者不冲突（峰值 vs 当前值），但命名要能让人一眼分清，否则采集端会拿错。

#### 优先级 B：利用率

| # | 指标 | 单位 | 从哪来 |
|---|---|---|---|
| B1 | `otf_cpu_usage_ratio` | 0–1（gauge） | `getrusage(RUSAGE_SELF)` 的 utime+stime ÷ 墙钟 |
| B2 | `otf_connections` | 个（gauge） | 已有（`metrics.h` 里的 `connections`） |
| B3 | `otf_sessions` / `otf_sessions_max` | 个 | `server_config.h` 的 `kMaxSessions = 64` 是**容量**，`otf_sessions / 64` 就是**容量型利用率** |

B3 值得多说一句：`kMaxSessions = 64` 与 `sync::kMaxRooms = 32`、`sync::kMaxPlayers = 4` 一起构成了**三层容量闸门**。按 USE 的语言，这三个都是**容量型资源**，利用率 = 当前值 / 上限，**到 100% 就是饱和**（多出来的请求被拒，而不是排队）。**这正是 §2.3.12 说的「容量型饱和从 100% 开始」**。

#### 优先级 C：延迟分布（RED 的 duration）

| # | 指标 | 单位 | 从哪来 |
|---|---|---|---|
| C1 | `otf_frame_recv_to_broadcast_ms` | ms | 从收到玩家输入帧 → 该帧影响进入下一次快照广播 |
| C2 | `otf_snapshot_send_ms` 的**分位数** | ms | 目前只有 `otf_snapshot_send_ms_max`（峰值） |

C1 是「玩家可见延迟」的近似。原文说：

> Latency (response time) is the most important metric for expressing application performance.

以及 §2.8.5 的警告：只报平均值会掩盖双峰。当前仓库的 `snapshot_send_ms_max` 只留峰值，**丢掉了分布**——但至少没犯「用平均掩盖尾巴」的错。这个取舍是**对的**，值得保留。

> （推演）如果要补分布，注意别走「每帧算一遍分位数」的路子——那是**观察者效应**（§2.3.10）。本仓库的 `metrics.cpp` 已经用了**固定大小缓冲 + 编译期算出的容量**这种零分配做法，分位数需要的是一个固定窗口的环形缓冲（容量在编译期定死），而不是动态容器。

#### 优先级 D：文件描述符（原文点名的软件资源）

`src/proto/protocol.h` 里有：

```cpp
inline constexpr std::size_t kMaxFd = 1024;
static_assert(kMaxFd <= 655350, "kMaxFd 必须 <= nofile 上限 655350（《技术参考.md》§6.4.5）；...");
```

**原文在 §2.5.9 的软件资源表里明确列了「File descriptor capacity」**：利用率 = 已用 fd 数、饱和 = 等待分配的数量、错误 = 分配失败。

本仓库的 fd 直接作数组下标（`conns_` 数组大小 = `kMaxFd`），所以：

| # | 指标 | 单位 |
|---|---|---|
| D1 | `otf_fds_used` | 个（gauge） |
| D2 | `otf_fds_utilization` | 0–1（gauge）= `otf_fds_used / 1024` |

D2 一旦逼近 1，就是**容量型饱和**——而且本仓库的 fd 分配失败会怎样？值得单独查一下（`AcceptNew` 路径）。这是**排查 fd 泄漏的最直接信号**。

### 6.4 一句话总结这个仓库的「方法论缺口」

本仓库其实**很懂得用「不变量指标」防退化**——`otf_epoll_ctl_add_with_out` 应恒为 0、`epoll_ctl_del` 远少于 `add` 是泄漏早期信号。这套思路**正是 USE 方法的精神**：把「该问的问题」变成「可观测的量」。

缺的是**另一半**：USE 要求的 utilization 与 saturation，仓库一个都没有；而 RED 要求的 duration，仓库量的是内部开销而非用户体验。

**优先级建议**：先做 A3（主循环两段耗时分开量）+ A1（丢帧计数）。这两条加起来不到 100 行代码，就能让本仓库第一次具备「知道自己饱和了没有」的能力——而这是**所有后续调优的前提**。

---

## 7. 收尾：本章金句清单

1. **For every resource, check utilization, saturation, and errors.**
   —— USE 方法的全部内容，六个词；照着问就不会漏。

2. **For every service, check the request rate, errors, and duration.**
   —— RED 方法；与 USE 互补，一个管机器健康，一个管用户健康。

3. **Keep in mind that the fastest query is the one you don’t do at all.**
   —— 最便宜的优化是**不做事**；先查有没有不必要的活。

4. **100% busy does not mean 100% capacity.**
   —— 「忙」和「满」是两回事；空闲的资源也可能已经接不了活了（那是饱和度的事）。

5. **Performance recommendations, especially the values of tunable parameters, are valid only at a specific point in time**
   —— 网上抄来的调优参数有时效；今天最好的建议下周可能有害。

6. **The latency is the time spent waiting before an operation is performed.**
   —— 延迟是**等待**，不是总时长；不含等待的那部分不叫延迟。

7. **it is best to include qualifying terms to explain what it measures: request latency, TCP connection latency, etc.**
   —— 「延迟」不加限定词就是歧义源；说「请求延迟」。

8. **Performance metrics are not free; at some point, CPU cycles must be spent to gather and store them.**
   —— 观测者效应：指标本身要花 CPU；观测是有代价的。

9. **Any degree of saturation is a performance issue, as time is spent waiting (latency).**
   —— 饱和不是「有点饱和没关系」；饱和本身就是延迟。

10. **The best performance wins are the result of eliminating unnecessary work**
    —— 最大的性能收益来自**不做**那件多余的事。

11. **The problem statement has therefore been included here as its own methodology and should be the first approach you use when tackling a new issue.**
    —— 先问清问题，再动手看指标。

12. **hypothesis → instrumentation → data → hypothesis**
    —— 诊断循环：每一次循环都要产生**数据**，好让错的假设早点死。

13. **The USE method also directs analysis to a limited number of key metrics, so that all system resources are checked as quickly as possible.**
    —— USE 的价值在于**有限**：先快速扫完所有资源，再深入。

14. **Even when tools cannot be found to answer some questions, the knowledge that these questions are unanswered can be extremely useful for the performance analyst: they are now “known-unknowns.”**
    —— 「知道这个问题我们答不上来」本身就是有价值的产出。

15. **narrowing down the scope of an investigation can help you focus quickly on the problem area, having identified that it is likely not a resource problem. This is the process of elimination.**
    —— 「资源都没问题」是一个**结论**，不是「白查了」。

16. **Every time you see an average used as a performance metric, especially an average latency, ask: What is the distribution?**
    —— 看到平均值就问分布；这是本领域最值得内化的一句。

17. **As a metric, the average for this distribution is seriously misleading.**
    —— 双峰分布里，平均值几乎与集中趋势**相反**。

18. **Beyond 60% utilization, the average response time doubles. By 80%, it has tripled.**
    —— 排队论给出的硬数字：不可中断的资源（磁盘），60% 就是危险线。

19. **Latency (response time) is the most important metric for expressing application performance.**
    —— 表达应用性能，延迟是第一指标。

20. **Workload characterization is a simple and effective method for identifying a class of issues: those due to the load applied.**
    —— 负载刻画专治「系统没毛病，只是负载太大」这一类问题。

21. **Don’t do it.**
    —— 性能口诀的第一条，也是最有效的一条。

22. **Little’s Law can be expressed as**
    —— L = λW；一把尺子量「系统里堆了多少东西」。
