---
title: Chapter 1. Introduction（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - Systems Performance
  - 性能工程
  - 延迟
  - 可观测性
  - 计数器
source: "《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg，Addison-Wesley 2020，英文原版）第 1 章"
companion: "[[01-Chapter 1. Introduction]]"
---

# 第 1 章 讲义：性能不是「跑得快」，是「延迟可预测」

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[01-Chapter 1. Introduction|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：`otf-server/src/net/metrics.h` —— 这个仓库到底在量什么、什么单位、怎么暴露出来的。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原章。

---

## 0. 开局：一个「快了 10%」和「慢了 30%」的争论

运维小李把监控面板甩到群里：「昨晚优化完，帧率从 58 提到 61，快了 5%。」

服务端老王回了一句：「玩家投诉的不是帧率，是**进副本后要等 4 秒才看到 Boss**。我昨晚修的是心跳超时的判定逻辑，跟那个没关系。」

小李不服：「我贴的图能说明问题啊。」

老王：「你的图是**平均**。那个 4 秒是第 99 百分位，5% 的玩家在经历它。平均值把这条尾巴藏起来了。」

这就是第 1 章要解决的事：**性能不是一个可以靠「跑得快」概括的东西**。它是一组指标、一套工具、一条诊断路径；而这一章给你的是「地图」——哪些指标、哪些工具、按什么顺序用。

本章的核心主张只有一句：

> 性能分析的第一步不是找问题，而是**先把「什么叫好」定义清楚**，然后用**延迟**去衡量，最后才动用工具。

---

## 1. §1.1 Systems Performance：性能分析管的是整条数据路径

原文一上来就把边界划得很宽：

> Systems performance studies the performance of an entire computer system, including all major software and hardware components.

要点有三个：

1. **「系统性能」不是「应用性能」**。存储设备到应用软件，**数据路径上的任何一环**都算在内。
2. **分布式系统意味着多台服务器 + 多个应用**，不是「一台机器上的一个进程」。
3. **先画数据路径图**：

> If you don’t have a diagram of your environment showing the data path, find one or draw it yourself; this will help you understand the relationships between components and ensure that you don’t overlook entire areas.

这一条对 `otf-server` 尤其重要——因为它的数据路径极短（客户端 TCP → epoll → 帧解码 → sim → 快照广播），短路径意味着**每一段都可能是瓶颈**，也意味着画图成本几乎为零。

原文接着给了 `Figure 1.1`（通用系统软件栈：从应用到硬件，含编译器），并定义本书的「full stack」是**从应用一直到金属片**，包含系统库、内核与硬件本身——比业界常说的「应用层」宽得多。

而性能工程的两个目标是：

> The typical goals of systems performance are to improve the end-user experience by reducing latency and to reduce computing cost.

注意是 **reduce latency**（降低延迟），不是「提高吞吐」。降低延迟改善体验，降低成本靠消除低效、提吞吐、调优。

---

## 2. §1.2 Roles / §1.3 Activities：谁做、什么时候做

§1.2 列出做性能分析的角色：系统管理员、SRE、应用开发、网络工程师、DBA、Web 管理员等。大多数人**只负责自己那一块**（网络团队查网络、数据库团队查数据库），但有些问题必须跨团队协作。

原文举 Netflix 的例子：他们有专门的**云性能团队**，既帮微服务与 SRE 团队做分析，也为所有人**造工具**（性能工具是团队的产出物，不只是副产品）。

§1.3 给出的活动清单，其实就是**软件生命周期里的十个步骤**：

1. 为未来产品设定性能目标 + 性能建模
2. 原型软硬件的性能特征刻画
3. 开发中产品在测试环境的性能分析
4. 新版本的非回归测试
5. 发布版本的基准测试
6. 在目标生产环境的概念验证测试
7. 生产环境的性能调优
8. 生产软件的监控
9. 生产问题的性能分析
10. 生产问题的复盘（incident review）

原文的判断值得抄下来：

> Performance engineering should ideally begin before any hardware is chosen or software is written: the first step should be to set objectives and create a performance model. However, products are often developed without this step, deferring performance engineering work to a later time, after a problem arises.

而且越往后越难修——早期架构决策造成的问题，后面每一步都在放大修复成本。

云时代给了「跳过前五步」的两个技术：**canary testing**（单实例 + 一部分生产流量）与 **blue-green deployment**（新旧实例池并存、流量逐步切换）。Netflix 用的是 red-black deployments 的说法。原文的态度是：**能用但别滥用**，能补做前面的活动还是补做。

> （推演）`otf-server` 的现状大致停在第 8–9 步之间：有 `src/net/metrics_server.cpp` 做的监控端点，也有 `metrics.on_tick_drift()` / `on_snapshot_send()` 这样的线上打点，但**没有第 1 步的「性能目标 + 模型」**。也就是说：能观测，但没有基准线可比 —— 这正是「退到第 9 步才发现问题」的典型形态。

---

## 3. §1.4 Perspectives：两种视角

原文把性能分析分成两种视角（`Figure 1.2`）：

| 视角 | 谁在做 | 关心什么 |
|---|---|---|
| **resource analysis**（资源分析） | 系统管理员 | CPU、内存、磁盘、网络接口的利用率与饱和度 |
| **workload analysis**（负载分析） | 应用开发者 | 请求到达了什么、应用响应如何 |

原文明确说：**对困难的问题，两种视角都试**。这句话在第 2 章还会展开成完整的对照表。

---

## 4. §1.5 Performance Is Challenging：性能为什么难

这一节是本书少见地直说「这事很难」的地方，四个理由分四小节。

### 4.1 §1.5.1 Subjectivity：性能是主观的

原文的对比：软件 bug 是**客观**的——要么在要么不在，错误信息一看就懂。而性能：

> Performance, on the other hand, is often subjective.

原文给的例子：

> The average disk I/O response time is 1 ms.

「这算好还是坏？」原文的答案是：取决于应用开发者与终端用户的性能预期。

**破解办法是把它变客观**：

> Subjective performance can be made objective by defining clear goals, such as having a target average response time, or requiring a percentage of requests to fall within a certain latency range.

——这就是「定 SLO」。开篇那个小李与老王的争吵，本质上就是「1 ms 磁盘延迟是好是坏」这场争论的现代版。

### 4.2 §1.5.2 Complexity：没有明显起点

- 云环境下你甚至**不知道该先看哪台机器**。
- 有时你会先立一个假设（怪网络、怪数据库），分析师的工作是判断这个方向对不对。
- **级联故障**：一个组件挂了，导致其他组件出现性能问题；单独看每个组件都正常。
- **瓶颈会搬家**：修好一个，瓶颈可能挪到别处，总体改善不如预期。
- 生产负载本身可能有**复杂的、非实验室可复现的特征**。

原文的结论：解决复杂性能问题往往需要**全局视角**，需要跨领域的技能。

### 4.3 §1.5.3 Multiple Causes：多个原因叠加

> Some performance issues do not have a single root cause, but instead have multiple contributing factors.

想象三个正常事件同时发生，**每一个单独看都不是根因**，合起来才是。

### 4.4 §1.5.4 Multiple Performance Issues：问题不止一个

这一小节给了本章最有实操价值的一段：

> Finding a performance issue is usually not the problem; in complex software there are often many.

作者让读者去搜自己操作系统的 bug 库里 "performance" 这个词——你会被吓一跳：成熟软件里有一堆已知但未修的性能问题。

于是核心命题出现：

> the real task isn’t finding an issue; it’s identifying which issue or issues matter the most

**不是「找到问题」，是「排序问题」。**

怎么排？原文给了答案：

> To do this, the performance analyst must quantify the magnitude of issues. ... Ideally, you will not just quantify the issues but also estimate the potential speedup to be gained for each one.

——**量化问题规模 + 估算潜在加速比**。这正是下一节「延迟」存在的理由：延迟是唯一能同时完成这两件事的指标。

---

## 5. §1.6 Latency：为什么延迟是首选指标

> Latency is a measure of time spent waiting, and is an essential performance metric.

原文解释了延迟为什么比其他指标好用：**它能算出最大加速比**。

`Figure 1.3` 的例子：一个数据库查询耗时 **100 ms**，其中 **80 ms 在阻塞等磁盘读**。

```
查询总耗时 100 ms
├─ 阻塞等磁盘读  80 ms   ← 消掉它，查询变成 20 ms
└─ 真正在算       20 ms
                    ↓
              加速比 = 100 / 20 = 5x
```

于是「把磁盘读缓存掉」这个动作的收益被量化为「最多快 5 倍」，同时问题本身也被量化了：**磁盘读让查询慢了 5 倍**。

原文随即说明**别的指标做不到这件事**：

> Such a calculation is not possible when using other metrics. I/O operations per second (IOPS), for example, depend on the type of I/O and are often not directly comparable.

如果 IOPS 降了 80%，你不知道影响多大——可能是 IOPS 少 5 倍，但每次 I/O 大小涨了 10 倍。

原文还提醒**延迟这个词本身有歧义**，必须加限定词：

> Throughout this book I will use clarifying terms where possible: those examples would be better described as connection latency and request latency.

（连接延迟 vs 请求延迟；DNS 延迟通常指整个 DNS 操作。）

最后一句是给 BPF 的伏笔：过去很多子系统**只有平均延迟，甚至完全没有延迟指标**；现在基于 BPF 的工具可以从**任意自定义的感兴趣点**测延迟，并且能拿到**完整分布**而不只是平均。

> （推演）这一节可以当成一条判据表：任何时候你要比较两个方案（「A 方案每秒 5000 次 IOPS，B 方案每秒 3000 次但平均 0.4 ms」），先把它**折算成延迟**，再比。折算不了说明你还没搞清 IO 大小、方向、随机/顺序这些属性 —— 而这些恰恰是 workload characterization 的产物。

---

## 6. §1.7 Observability：观测的三种工具

> Observability refers to understanding a system through observation, and classifies the tools that accomplish this. This includes tools that use counters, profiling, and tracing.

关键定义：**观测工具不包括基准测试工具**（后者会改变系统状态）。原文的顺序建议：

> For production environments, observability tools should be tried first wherever possible, as experimental tools may perturb production workloads through resource contention.

生产环境先观测（不改状态），空闲的测试环境可以先跑基准。

### 6.1 §1.7.1 Counters, Statistics, and Metrics：三个词的层级

原文把这一串词讲得很清楚：

- **counter（计数器）**：软件里硬编码的整型变量，累积递增。内核与应用都提供。
- **statistics（统计量）**：在**不同时刻读同一个 counter**，算出来的变化率、平均值、百分位。
  原文的例子里，`vmstat(8)` 输出的是一台 48 核生产 API 服务器的数据，`cpu us + sy` 加起来约 57% 即系统级 CPU 利用率。
- **metric（指标）**：**被挑选出来用于评估或监控某个目标的统计量**。公司用监控 agent 定期记录选定的 metrics，画成图，并据此配告警。

`Figure 1.4` 画出了 counter → statistic → metric → alert 的层级（原文也承认**行业里这几个词常被混用**）。

`Figure 1.5` 是同一台服务器的 Grafana 面板。这类折线图对**容量规划**有用：预测资源何时耗尽。

原文还给了一个很实用的判断：

> Sometimes, time-series metrics are all that is needed to resolve a performance issue. Knowing the exact time a problem began may correlate with a known software or configuration change, which can be reverted. Other times, metrics only point in a direction... but without explaining why.

**时序图能定位「什么时候开始坏的」，但解释不了「为什么坏」。** 后者要靠 profiling / tracing。

### 6.2 §1.7.2 Profiling：采样

> In systems performance, the term profiling usually refers to the use of tools that perform sampling: taking a subset (a sample) of measurements to paint a coarse picture of the target.

原文对火焰图评价极高：

> CPU flame graphs can help you find more performance wins than any other tool, after metrics.

理由是火焰图**不只暴露 CPU 问题**——锁竞争（自旋路径里的 CPU 时间）、内存问题（`malloc()` 里过多的 CPU 时间，以及通往那里的调用路径）、网络配置错误（老旧慢路径里的 CPU 时间）都能从火焰图上看出来。

`Figure 1.6` 是 `iperf(1)` 的火焰图：左侧高塔是 `tcp_write_xmit()`（TCP 发送），右侧宽路径终止于 `copy_user_enhanced_fast_string()`（字节拷贝）。**宽度正比于 CPU 时间**，纵轴是代码路径。

### 6.3 §1.7.3 Tracing：事件记录

> Tracing is event-based recording, where event data is captured and saved for later analysis or consumed on-the-fly for custom summaries and other actions.

有专用 tracer（`strace(1)` 看系统调用、`tcpdump(8)` 抓包），也有通用 tracer（Ftrace、BCC、bpftrace）。

**静态插桩（Static Instrumentation）**：源码里硬编码的插桩点。Linux 内核里的这套技术叫 **tracepoints**，用户态的对应物是 **USDT**。原文用 `execsnoop(8)` 举例——它靠 tracepoint 跟踪 `execve(2)`，打印新创建的进程：

```
PCOMM            PID    PPID   RET ARGS
ssh              30656  20063    0 /usr/bin/ssh 0
sh               30660  30657    0
```

原文指出它的价值：**发现短命进程**，这些进程可能被 `top(1)` 漏掉，而它们可能是性能问题的来源。

**动态插桩（Dynamic Instrumentation）**：运行中修改内存指令插入插桩例程，像调试器下断点，但不停下来而是跑完例程继续。

原文用了全文最形象的一个比喻：

> Consider an operating system kernel: analyzing kernel internals can be like venturing into a dark room, with candles (system counters) placed where the kernel engineers thought they were needed. Dynamic instrumentation is like having a flashlight that you can point anywhere.

**BPF**（原名 Berkeley Packet Filter，现在已只是个名字）原本是内核里的迷你虚拟机，用来加速 `tcpdump` 的表达式匹配；2013 年起扩展成通用的内核执行环境，给 BCC 和 bpftrace 提供可编程性。

### 6.4 §1.8 Experimentation：另一只手

> Apart from observability tools there are also experimentation tools, most of which are benchmarking tools.

- **macro-benchmark**：模拟真实负载（如客户端发应用请求）。
- **micro-benchmark**：测单个组件（CPU、磁盘、网络）。

原文的赛车比喻：绕 Laguna Seca 赛道的一圈是 macro；极速与 0–60 mph 是 micro。两者都重要，但 **micro 更容易调试、复现、理解，也更稳定**。

原文紧接着给了本章最实用的一句「两只手」比喻：

> Only using one type of tool is like trying to solve a problem one-handed.

（出处：作者说这是 Netflix 一位资深性能工程师 Roch Bourbonnais 教他的。观测是一只手，实验是另一只手。）

---

## 7. §1.10.1 Linux Perf Analysis in 60 Seconds

原文给了一张**表 1.1** 的 60 秒检查清单，十条命令：

| # | 工具 | 看什么 |
|---|---|---|
| 1 | `uptime` | 负载均值，判断在涨还是在落（比 1/5/15 分钟） |
| 2 | `dmesg -T \| tail` | 内核错误，包括 OOM |
| 3 | `vmstat -SM 1` | 运行队列长度、交换、整体 CPU |
| 4 | `mpstat -P ALL 1` | 每 CPU 均衡；**单个 CPU 很忙 = 线程扩展性差** |
| 5 | `pidstat 1` | 每进程 CPU，找意外的消费者，以及用户态/内核态时间 |
| 6 | `iostat -sxz 1` | 磁盘 IOPS、吞吐、平均等待、busy 百分比 |
| 7 | `free -m` | 内存使用，含文件系统缓存 |
| 8 | `sar -n DEV 1` | 网络设备收发包与吞吐 |
| 9 | `sar -n TCP,ETCP 1` | TCP 连接速率、重传 |
| 10 | `top` | 总览 |

原文提醒：这张清单是按「现成的 CLI 工具」设计的，监控面板可能有更多更好的指标；但如果要自建面板，原文更倾向于**为 USE 方法等方法论**做定制面板，而不是照搬这张清单。

---

## 8. §1.11 Case Studies：两个假想的调查

原文给了两个案例，用来演示「什么时候做、为什么做」。

**§1.11.1 Slow Disks**：数据库团队报「磁盘慢」。Sumit（系统管理员）的第一件事是**不查磁盘**，而是**回去问清楚问题**：

- 现在真的有数据库性能问题吗？是怎么测量的？
- 是哪个数据库、哪些操作？
- 问题是刚出现的还是一直如此？
- 最近服务器/应用有什么变更？

（原文这里的逻辑是：**先把问题定义成可测量的形式**，再动手。）

**§1.11.2 Software Change**：一个软件变更的性能测试，展示怎么设计实验与判读结果。

原文明确声明这两个案例**不是「正确做法」或「唯一做法」**，只是「一种做法」，供批判性参考。

---

## 9. 实战锚点：otf-server 的 `src/net/metrics.h` 到底在量什么

现在把第 1 章的概念落到本仓库的真实代码上。先看它**实际量了什么**。

### 9.1 指标的完整清单

`src/net/metrics.h` 定义了 `struct MetricsSnapshot`，字段名即 Prometheus 指标名去掉 `otf_` 前缀的部分：

| 指标 | 类型 | 语义 |
|---|---|---|
| `otf_connections` | counter（可增可减） | 当前连接数 |
| `otf_rooms` | counter | 活动房间数 |
| `otf_frames_sent` | counter | 累计发帧 |
| `otf_frames_recv` | counter | 累计收帧 |
| `otf_heartbeat_timeouts` | counter | 心跳超时次数（**攻击面指标**） |
| `otf_oversize_rejects` | counter | 超长帧拒绝数（**攻击面指标**） |
| `otf_tick_drift_max_ms` | **gauge 上的 max**，可整行缺席 | 最大 tick 漂移（带符号） |
| `otf_snapshot_send_ms_max` | gauge 上的 max | 快照发送延迟峰值 |
| `otf_epoll_ctl_add` / `_del` / `_mod` | counter | epoll_ctl 调用累计 |
| `otf_epoll_ctl_add_with_out` | counter，**应恒为 0** | 注册时就带 EPOLLOUT 的次数 |

`src/net/metrics.cpp` 里 `kRenderFmt` 是 11 条固定格式串，`kDriftFmt` 是那 1 条可能缺席的漂移行，`static_assert(kSpecCount == 12, ...)` 把「指标个数」钉成编译期事实。

**对照第 1 章的层级看**：这些全是 **counter**（硬编码整型、累积递增），不是 metric。真正的 metric 需要「选出来、记录时序、配告警」——这一层在本仓库**不存在**（`metrics_server.cpp` 只提供了一次 `render()` 快照，没有时序存储）。

### 9.2 累加类 vs 峰值类：本仓库做对的一件事

`metrics.h` 的类注释把这件事写死了：

> 累加类 vs 峰值类：前六个是**累加**（counter），后两个是**峰值**（gauge 上的 max）。峰值指标累加起来没有运维意义 ——「本小时共漂移 480ms」说明不了任何事，「本小时最大一次漂移 37ms」才能定告警。

这正是第 1 章 §1.7.1 的「counter → statistics」层：**累加的 counter 要在两个时刻相减，才变成「每秒多少」这种 statistic**。本仓库的 `render()` 只给累加值，相减得由采集端做。

### 9.3 「不钳位」：一个反直觉但正确的决定

`metrics.h` 里有一段：

> ⚠️ 本结构**不做任何钳位**：`on_connect(-1)` 减过头会照实渲染成负数。这是刻意的 —— 钳位会把「调用方记账错了」这个真 bug 变成一个永远停在 0 的健康指标，比负数危险得多。

**（推演）** 这就是 §1.5.4「排序问题」的最小版本：宁可给一个刺眼的怪值，也不要给一个永远健康因而永远不会被看的指标。计数器一旦被钳位，「问题很多」这件事就被藏起来了 —— 正好是第 1 章说的「找到问题不是重点，排序才是」的反面。

### 9.4 `nullopt`：「没测过」不等于「测出来是 0」

`tick_drift_max_ms` 是 `std::optional<std::int64_t>`。`metrics.h` 的注释记录了这个坑的完整演化：

> 这里曾经是 `std::int64_t = 0`，而取峰值的比较是 `v > cur`、初值 0 —— 于是**所有负漂移被静默吞掉**：实测 `on_tick_drift(INT64_MIN)` 之后渲染出来的仍然是 0。而「sim 跑得比墙钟快」正是会导致双端不同步的故障，它恰好只在负半轴出现 —— 被吞掉的就是最该报警的那一半。

于是改成 `MaxMagnitudeInto`：**按绝对值取峰、保留符号**，用**独立的 bool 标志位**而不是哨兵值表示「是否记过」：

> 一度用 `INT64_MIN` 当哨兵，但 `INT64_MIN` 本身是**合法漂移值**（评审给的漏报用例正是它），于是那种漂移会被读成「从未记录」—— 换了个姿势继续静默丢弃。

`render()` 里，`otf_tick_drift_max_ms` 一次都没记录过时**整行缺席**，而不是输出 0。

> （推演）这是本仓库与第 1 章「主观性能变客观」最同构的一处：把「好/坏」这个主观判断，落到「有阈值、有方向、有缺失语义」的客观信号上。缺失用缺失表示，而不是用 0 假装。

### 9.5 缺什么：对照第 1 章的清单

| 第 1 章的要求 | 本仓库现状 |
|---|---|
| 画数据路径图 | 没有文档化的数据路径图（`main.cpp` 主循环 → `tcp_transport` → `session_mgr` → `room_mgr` → `world` → `sim`） |
| 定性能目标（SLO） | 没有。第 6.3 节的 20 Hz tick / 10 Hz 快照是**节奏**，不是**目标** |
| 时序 metric + 告警 | 没有。只有 Prometheus 文本 exposition 的**瞬时快照** |
| 延迟百分位 | 只有一个 `snapshot_send_ms_max`（峰值），**没有分位数**，而且是快照发送耗时，不是玩家可见延迟 |
| profiling / tracing | 无。整个仓库没有一个 profiler 接入点；`src/sim/` 是纯逻辑，但**没有火焰图**验证过它的 CPU 占比 |

最后一条尤其值得展开：第 1 章说火焰图「能找出除 CPU 之外的任何其他性能问题」。对本仓库来说，**最该先做的事不是加新指标，而是给现有逻辑打一次火焰图** —— 9700 行、零三方依赖、单线程 reactor，如果 `sim/` 每 tick 的开销占了主循环的大头，那所有网络层的优化都是白费力气。

---

## 10. 收尾：本章金句清单

1. **The typical goals of systems performance are to improve the end-user experience by reducing latency and to reduce computing cost.**
   —— 两个目标，一个改善体验（延迟），一个降低成本（吞吐与效率）；别把它们混成一个「快」。

2. **Latency is a measure of time spent waiting, and is an essential performance metric.**
   —— 延迟是最基础的性能指标；一切争议最后都要落回「等了多久」。

3. **the real task isn’t finding an issue; it’s identifying which issue or issues matter the most**
   —— 复杂系统里性能问题一大把，分析师的真本事是**排序**。

4. **To do this, the performance analyst must quantify the magnitude of issues.**
   —— 排序靠量化；而量化最好用的度量是延迟。

5. **Performance, on the other hand, is often subjective**
   —— 性能问题不像 bug 那样「一看就懂」，所以要先定义目标把它变客观。

6. **Subjective performance can be made objective by defining clear goals, such as having a target average response time, or requiring a percentage of requests to fall within a certain latency range.**
   —— 「99% 的请求 < 100 ms」这种句子就是定 SLO 的口语版。

7. **Performance issues may also originate from complex interactions between subsystems that perform well when analyzed in isolation.**
   —— 单测全绿不等于系统健康；级联故障是性能问题的经典形态。

8. **Both benchmark types are important, although micro-benchmarks are typically easier to debug, repeat, and understand, and are more stable.**
   —— 微观基准更稳更好调，宏观基准更接近真实负载；两者都要。

9. **A metric is a statistic that has been selected to evaluate or monitor a target.**
   —— counter 是事实，metric 是**选择**；选择意味着有取舍。

10. **Only using one type of tool is like trying to solve a problem one-handed.**
    —— 观测与实验是两只手；只用一只等于单手解题。

11. **Observability refers to understanding a system through observation, and classifies the tools that accomplish this.**
    —— 可观测性是「通过观察理解系统」的能力，工具只是它的实现。

12. **For production environments, observability tools should be tried first wherever possible, as experimental tools may perturb production workloads through resource contention.**
    —— 生产环境先观测，因为实验会扰动负载。

13. **CPU flame graphs can help you find more performance wins than any other tool, after metrics.**
    —— 优先级排序：指标 → 火焰图 → 其他。

14. **Dynamic instrumentation is like having a flashlight that you can point anywhere.**
    —— 计数器是别人摆好的蜡烛，动态插桩是你自己的手电筒。

15. **Sometimes, time-series metrics are all that is needed to resolve a performance issue.**
    —— 时序图的价值在于定位「从什么时候开始坏」，而不是解释原因。

16. **Performance engineering should ideally begin before any hardware is chosen or software is written**
    —— 性能工程应该从选硬件之前就开始；越晚做越贵。

17. **While latency is a useful metric, it hasn’t always been available when and where needed.**
    —— 「延迟是最好的指标」和「我这儿没有延迟指标」可以同时为真；这正是 BPF 存在的理由。

18. **If you don’t have a diagram of your environment showing the data path, find one or draw it yourself**
    —— 没有数据路径图，就先画一张；这是第 1 章最便宜的建议。
