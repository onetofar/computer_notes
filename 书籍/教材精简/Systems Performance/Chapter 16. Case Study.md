---
title: Chapter 16. Case Study（精读）
category: 性能工程
tags:
  - 性能案例
  - 容器化
  - CPU饱和
  - 上下文切换
  - 缓存
  - 可观测性
difficulty: 进阶
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）第16章 Case Study"
source_section: "精读 §16.1 / §16.1.1 / §16.1.2 / §16.1.3 / §16.1.4 / §16.1.5 / §16.1.6 / §16.1.7 / §16.1.8 / §16.2；章首引言（Chapter 16 标题下的无编号段落）随 §16.1 立篇；§16.3 References 不立篇（纯文献列表，不承载机制陈述）"
skills:
  - obsidian-markdown
---

# Chapter 16. Case Study

> **小节地图**：本章是全书唯一的**完整案例研究**——不引入新技术，而是把一次真实生产性能问题从「收到报告」走到「定因」的全过程摊开。立篇：章首定位（为什么这一章用讲故事的方式）、§16.1（案例背景与分析工具谱系）、§16.1.1（问题陈述）、§16.1.2（分析策略）、§16.1.3（统计层）、§16.1.4（配置层）、§16.1.5（PMC 层）、§16.1.6（软件事件层）、§16.1.7（追踪层）、§16.1.8（结论：三个原因）、§16.2（更多案例的来源）。
> **不立篇**：§16.3 References（纯文献列表，不承载机制陈述）。
> 底层依赖：60 秒清单见 [[Chapter 1. Introduction]]，USE 方法与下钻分析见 [[Chapter 2. Methodologies]]，`uptime` 与 `pmcarch` 的指标口径见 [[Chapter 6. CPUs]]，容器内可观测性受限见 [[Chapter 11. Cloud Computing]]，对照实验与基准测试见 [[Chapter 12. Benchmarking]]。
> 本篇要记住的不是某一章的机制，而是**证据如何一层层叠加**：统计 → 配置 → PMC → 软件事件 → 追踪，每层只回答一个问题。

### 章首定位：为什么这一章用讲故事的方式

**机制**：这一章的技术含量近乎为零——它不引入任何新工具或新原理，全部内容是既有方法在真实问题上的串联。原文自己把这一点讲得很清楚：

> My intent in this chapter is not to introduce new technical content but to use storytelling to show how tools and methodologies may be applied in practice, in a real work environment.

**为什么值得单独读**：分析类知识最容易学成「工具清单」，而这一章示范的是**判断过程**——在信息不完整时先信哪个假设、拿什么证据去证伪、什么时候承认「量级对上了但原因还没找到」。

**容易错在哪**：把「专家的最终结论」当成「可以直接套用的标准流程」。原文明确说了它不是最佳路径记录，只是解释「为什么走了这条路」。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| 章首定位 | 16-Chapter 16. Case Study.md L27 | My intent in this chapter is not to introduce new technical content but to use storytelling to show how tools and methodologies may be applied in practice, in a real work environment. |
| §16.1 案例背景 | 16-Chapter 16. Case Study.md L33 | For analysis I used a variety of tools, including those based on counters, static configuration, PMCs, software events, and tracing. All of these tool types played a role and provided clues that fit together. |
| §16.1.1 问题陈述 | 16-Chapter 16. Case Study.md L37 | This component had a request latency of three to four seconds on the VM instances, which became one second on containers: three to four times faster! |
| §16.1.1 问题陈述 | 16-Chapter 16. Case Study.md L39 | The problem was to explain this performance difference. If it was simply due to the container move, the microservice can expect a permanent 3-4x win by moving. |
| §16.1.1 问题陈述 | 16-Chapter 16. Case Study.md L41 | What immediately came to mind was the benefit of running one component of a workload in isolation: it would be able to use the entire CPU caches without contention from the other component, improving cache hit ratios and thus performance. Another guess would be bursting on the container platform, where a container can use idle CPU resources from other containers. |
| §16.1.2 分析策略 | 16-Chapter 16. Case Study.md L45 | As traffic is handled by a load balancer (AWS ELB), it was possible to split the traffic between the VM and containers so that I could log in to both at the same time. This is an ideal situation for comparative analysis: I could run the same analysis command on both at the same time of day (same traffic mix and load) and compare the output immediately. |
| §16.1.2 分析策略 | 16-Chapter 16. Case Study.md L47 | In this case I had access to the container host, not just the container, which allowed me to use any analysis tool and provided the permission for those tools to make any syscall. If I had had only container access, analysis would have been much more time-consuming due to limited observability sources and kernel permissions, requiring much more inference from limited metrics rather than direct measurement. |
| §16.1.2 分析策略 | 16-Chapter 16. Case Study.md L49 | For methodologies, I planned to start with the 60-second checklist ( Chapter 1 , Introduction , Section 1.10.1 , Linux Perf Analysis in 60s ) and the USE method ( Chapter 2 , Methodologies , Section 2.5.9 , The USE Method ), and based on their clues perform drill-down analysis ( Section 2.5.12 , Drill-Down Analysis ) and other methodologies. |
| §16.1.3 统计层 | 16-Chapter 16. Case Study.md L104 | mpstat(1) prints the number of CPUs as the first line. The output showed that the virtual machine had 48 CPUs, and the container host had 64. |
| §16.1.3 统计层 | 16-Chapter 16. Case Study.md L108 | CPU utilization ( %usr + %sys + ...) showed that the VM was at 98% versus the container at 22%. These processors have two hyperthreads per CPU core, so crossing the 50% utilization mark typically means hyperthread core contention, degrading performance. |
| §16.1.3 统计层 | 16-Chapter 16. Case Study.md L116 | This meant that threads were waiting their turn roughly 77% of the time (85/48 – 1), and eliminating this time spent waiting would produce a roughly 4x speedup (1 / (1 – 0.77)). |
| §16.1.4 配置层 | 16-Chapter 16. Case Study.md L198 | The CPUs for the container host had a slightly slower base frequency (2.30 versus 2.50 GHz); however, they had a much larger last-level cache (45 versus 33 Mbytes). Depending on the workload, the larger cache size can make a significant difference to CPU performance. |
| §16.1.5 PMC 层 | 16-Chapter 16. Case Study.md L232 | This showed instructions per cycle (IPC) of around 0.57 for the VM versus around 1.52 for the container: a 2.6x difference. |
| §16.1.5 PMC 层 | 16-Chapter 16. Case Study.md L234 | One reason for the lower IPC could be hyperthread contention, as the VM host was running at over 50% CPU utilization. The last column showed an additional reason: the last-level cache (LLC) hit ratio was only 30% for the VM, versus around 90% for the container. |
| §16.1.6 软件事件层 | 16-Chapter 16. Case Study.md L269 | This output showed a rate of around two million context switches per second. |
| §16.1.7 追踪层 | 16-Chapter 16. Case Study.md L330 | This output shows that the application typically spent very little time on-CPU, often less than 7 microseconds. |
| §16.1.7 追踪层 | 16-Chapter 16. Case Study.md L366 | The involuntary context switches on the VM, and subsequent high rate of context switches seen earlier, caused performance problems. Causing the application to leave the CPU after often less than 10 microseconds also does not give the CPU caches much time to warm up to the current code paths. |
| §16.1.8 结论 | 16-Chapter 16. Case Study.md L372 | No container neighbors : The container host was idle except for the one container. This allowed the container to have the entire CPU caches all to itself, as well as to run without CPU contention. While this produced container-favorable results during the test, it is not the expected situation for long-term production use where neighboring containers will be the norm. The microservice may find that the 3-4x performance win vanishes when other tenants move in. |
| §16.1.8 结论 | 16-Chapter 16. Case Study.md L382 | CPU load difference : A higher load was directed to the VM, driving the CPUs to saturation: a CPU-based load average of 85 on a 48-CPU system. This caused a rate of around 2 million context switches per second, and run queue latency as threads waited their turn. |
| §16.2 案例来源 | 16-Chapter 16. Case Study.md L388 | Taking wrong turns, and identifying multiple contributing factors, is normal. |

> **书里没说的**（本篇有几处推论，原文没有，别当成原文观点）：
> - **「2.6x（IPC）与 4x（排队）如何合成 3-4x」原文没有做算术**：L374 说 IPC 低 2.6 倍「could explain 2.6x」，L382 说负载导致的排队延迟「showed the VM was running roughly 4x slower」，两者既不相乘也不相加，原文只以一句「These issues explain the observed performance difference.」（L384）收尾。本篇因此把它们当成**三条并行且各自可量的原因**，而不是一条可复算的因果链。
> - **「邻居容器上线后收益还剩多少」原文没有实验数据**，只有定性警告（L372）。本篇不替它补数字。
> - **PMC 数据是单次 10 秒采样**，原文没有多次采样、置信区间或负载匹配说明。
> - 「容器平台的 bursting 借邻居空闲 CPU」是原文列出的**初始假设**（L41），后文没有再去验证它。

## §16.1 案例背景与分析工具谱系

**机制**：Netflix 一个微服务（Java 写的客户推荐服务）原本跑在 AWS EC2 的虚拟机实例上；它由两个组件构成，其中一个被放到 Netflix 自研容器平台 Titus 上测试，请求延迟从 3-4 秒掉到 1 秒。这么大的收益「好得不像真的」，于是被要求解释。

原文在正式分析前先交代了这次用到的**五类工具**，这份清单就是后面 §16.1.3 到 §16.1.7 的推进顺序：

> For analysis I used a variety of tools, including those based on counters, static configuration, PMCs, software events, and tracing. All of these tool types played a role and provided clues that fit together.

**为什么重要**：这五类工具的**成本递增、解释力递减**——计数器最便宜但只能给线索，追踪最贵但能定因。原文的推进顺序就是按这个性价比排的。

**判据（推演）**：遇到「快得不真实」的收益，第一反应应该是怀疑**对照组没被压满**或**测试组占了便宜**，而不是先相信收益。

## §16.1.1 问题陈述

**机制**：组件在 VM 上请求延迟 3-4 秒，在容器上变成 1 秒：

> This component had a request latency of three to four seconds on the VM instances, which became one second on containers: three to four times faster!

原文把要回答的问题定义得很清楚——不只是「快了多少」，而是「这个差异是什么造成的、会不会永久、能不能迁移到别处并用得更多」：

> The problem was to explain this performance difference. If it was simply due to the container move, the microservice can expect a permanent 3-4x win by moving.

**这一步决定了后面所有动作**：只有把问题写成「可证伪的三个子问题」，才能知道该测什么。

原文还列了两个**初始假设**，注意它们后来都没有被直接证实，而是被拆成了可测的指标：

> What immediately came to mind was the benefit of running one component of a workload in isolation: it would be able to use the entire CPU caches without contention from the other component, improving cache hit ratios and thus performance. Another guess would be bursting on the container platform, where a container can use idle CPU resources from other containers.

**容易错在哪**：把初始假设当成结论。原文的两个假设（组件隔离独享缓存、容器平台 burst 借用邻居空闲 CPU）最后都没有直接被验证——真正被量化的是缓存命中率、CPU 饱和度、上下文切换率。

## §16.1.2 分析策略

**机制一 · 造出可比的两组**：流量由 AWS ELB 分发，因此可以拆分流量、同时登进 VM 和容器宿主，在**同一时段、相同流量构成与负载**下跑同一条分析命令，输出直接对照。

> As traffic is handled by a load balancer (AWS ELB), it was possible to split the traffic between the VM and containers so that I could log in to both at the same time. This is an ideal situation for comparative analysis: I could run the same analysis command on both at the same time of day (same traffic mix and load) and compare the output immediately.

**机制二 · 权限决定可观测性上限**：本次拿到的是**容器宿主**而不只是容器，因此任何分析工具都能用、任何系统调用都能发。若只有容器内访问，可观测数据源和内核权限都受限，只能从有限指标做推断而不是直接测量。

> In this case I had access to the container host, not just the container, which allowed me to use any analysis tool and provided the permission for those tools to make any syscall. If I had had only container access, analysis would have been much more time-consuming due to limited observability sources and kernel permissions, requiring much more inference from limited metrics rather than direct measurement.

**机制三 · 方法论有先后**：先跑 60 秒清单和 USE 方法拿到线索，再据此做下钻分析。

> For methodologies, I planned to start with the 60-second checklist ( Chapter 1 , Introduction , Section 1.10.1 , Linux Perf Analysis in 60s ) and the USE method ( Chapter 2 , Methodologies , Section 2.5.9 , The USE Method ), and based on their clues perform drill-down analysis ( Section 2.5.12 , Drill-Down Analysis ) and other methodologies.

**判据**：可比性不是自然具备的，需要**主动设计**——同一时段、同一负载、同一命令、同一层权限，缺一项，对照结论就不成立。而「能拿到宿主层权限」本身就是决定分析上限的变量。

## §16.1.3 统计层

**机制**：第一刀是 `uptime(1)` 的 load average。

```bash
serverA# uptime
 22:07:23 up 15 days,  5:01,  1 user,  load average: 85.09, 89.25, 91.26

serverB# uptime
 22:06:24 up 91 days, 23:52,  1 user,  load average: 17.94, 16.92, 16.62
```

关键不是绝对值 85.09 这个数，而是**拿它除以 CPU 数**：

> mpstat(1) prints the number of CPUs as the first line. The output showed that the virtual machine had 48 CPUs, and the container host had 64.

两台机器的 `mpstat(1)` 也印证了这个判断：

```bash
serverA# mpstat 10
Linux 4.4.0-130-generic (...) 07/18/2019        _x86_64_  (48 CPU)

10:07:55 PM  CPU   %usr  %nice  %sys %iowait  %irq %soft %steal %guest %gnice  %idle
10:08:05 PM  all  89.72   0.00  7.84    0.00  0.00  0.04   0.00   0.00   0.00   2.40
10:08:15 PM  all  88.60   0.00  9.18    0.00  0.00  0.05   0.00   0.00   0.00   2.17
10:08:25 PM  all  89.71   0.00  9.01    0.00  0.00  0.05   0.00   0.00   0.00   1.23
10:08:35 PM  all  89.55   0.00  8.11    0.00  0.00  0.06   0.00   0.00   0.00   2.28
10:08:45 PM  all  89.87   0.00  8.21    0.00  0.00  0.05   0.00   0.00   0.00   1.86
^C
Average:     all  89.49   0.00  8.47    0.00  0.00  0.05   0.00   0.00   0.00   1.99
```

```bash
serverB# mpstat 10
Linux 4.19.26 (...) 07/18/2019        _x86_64_  (64 CPU)

09:56:11 PM CPU    %usr  %nice  %sys %iowait  %irq %soft %steal %guest %gnice  %idle
09:56:21 PM  all  23.21   0.01  0.32    0.00  0.00  0.10   0.00   0.00   0.00  76.37
09:56:31 PM  all  20.21   0.00  0.38    0.00  0.00  0.08   0.00   0.00   0.00  79.33
09:56:41 PM  all  21.58   0.00  0.39    0.00  0.00  0.10   0.00   0.00   0.00  77.92
09:56:51 PM  all  21.57   0.01  0.39    0.02  0.00  0.09   0.00   0.00   0.00  77.93
09:57:01 PM  all  20.93   0.00  0.35    0.00  0.00  0.09   0.00   0.00   0.00  78.63
^C
Average:     all  21.50   0.00  0.36    0.00  0.00  0.09   0.00   0.00   0.00  78.04
```

由此得到三条线索：VM 的 CPU 利用率 98%、容器侧 22%；两台都是每核两个超线程，**越过 50% 利用率线意味着超线程核内争用**，VM 已深陷其中；VM 的 `%sys` 约 8%、容器侧 0.38%，多出来的系统时间可能就是内核里的上下文切换路径。

> CPU utilization ( %usr + %sys + ...) showed that the VM was at 98% versus the container at 22%. These processors have two hyperthreads per CPU core, so crossing the 50% utilization mark typically means hyperthread core contention, degrading performance.

**交叉验证**：随后 `vmstat(8)` 的 run queue 长度与 load average 相符（确认是 CPU 型而非 I/O 型），`iostat(1)` 与 `sar(1)` 显示磁盘和网络 I/O 都很轻。到这里可以排除 I/O 方向。

**这一层最有价值的产出是一个算术**：

> This meant that threads were waiting their turn roughly 77% of the time (85/48 – 1), and eliminating this time spent waiting would produce a roughly 4x speedup (1 / (1 – 0.77)).

**为什么重要**：这个 4x **量级**与报告的 3-4x 对上了，等于确认了「CPU 饱和 + 排队」这条主线是对的。但原文紧接着说「more analysis was necessary」——量级对上了，原因还没找到。

**判据**：load average 是**相对量**，永远要配 CPU 数读；而且它把不可中断睡眠（D 状态）也算进去，所以高 load 不必然是 CPU 忙，必须用 run queue 和 idle 时间交叉验证。

## §16.1.4 配置层

**机制**：既然问题指向 CPU，就去比配置。两边 CPU 型号不同，基频更**低**、末级缓存却更**大**：

> The CPUs for the container host had a slightly slower base frequency (2.30 versus 2.50 GHz); however, they had a much larger last-level cache (45 versus 33 Mbytes). Depending on the workload, the larger cache size can make a significant difference to CPU performance.

`/proc/cpuinfo` 的关键字段（节选）：

```bash
serverA# cat /proc/cpuinfo
processor       : 47
vendor_id       : GenuineIntel
cpu family      : 6
model           : 85
model name      : Intel(R) Xeon(R) Platinum 8175M CPU @ 2.50GHz
stepping        : 4
microcode       : 0x200005e
cpu MHz         : 2499.998
cache size      : 33792 KB
physical id     : 0
siblings        : 48
core id         : 23
cpu cores       : 24
```

```bash
serverB# cat /proc/cpuinfo
processor       : 63
vendor_id       : GenuineIntel
cpu family      : 6
model           : 79
model name      : Intel(R) Xeon(R) CPU E5-2686 v4 @ 2.30GHz
stepping        : 1
microcode       : 0xb000033
cpu MHz         : 1200.601
cache size      : 46080 KB
physical id     : 1
siblings        : 32
core id         : 15
cpu cores       : 16
```

**为什么要单独扫配置**：容器基频更低却缓存更大——这说明「换平台」同时改了不止一个变量。如果不做这一步，很容易把「换了更大的 LLC 机型」误记成「容器平台更快」。

**判据**：任何 A/B 对照，只要两边不是**同一套硬件规格**，结论就自带混淆变量；配置差异必须先被摊平，再谈平台差异。

## §16.1.5 PMC 层

**机制**：静态配置说完了还不够（基频更低反而是劣势），需要硬件计数器。`pmcarch(8)` 给出 Intel 架构基本计数器集：

```bash
serverA# ./pmcarch -p 4093 10
K_CYCLES  K_INSTR    IPC BR_RETIRED   BR_MISPRED  BMR% LLCREF      LLCMISS     LLC%
982412660 575706336 0.59 126424862460 2416880487  1.91 15724006692 10872315070 30.86
999621309 555043627 0.56 120449284756 2317302514  1.92 15378257714 11121882510 27.68
991146940 558145849 0.56 126350181501 2530383860  2.00 15965082710 11464682655 28.19
996314688 562276830 0.56 122215605985 2348638980  1.92 15558286345 10835594199 30.35
979890037 560268707 0.57 125609807909 2386085660  1.90 15828820588 11038597030 30.26
[...]
```

```bash
serverB# ./pmcarch -p 1928219 10
K_CYCLES  K_INSTR    IPC BR_RETIRED   BR_MISPRED  BMR% LLCREF      LLCMISS     LLC%
147523816 222396364 1.51 46053921119  641813770   1.39 8880477235  968809014  89.09
156634810 229801807 1.47 48236123575  653064504   1.35 9186609260  1183858023 87.11
152783226 237001219 1.55 49344315621  692819230   1.40 9314992450  879494418  90.56
140787179 213570329 1.52 44518363978  631588112   1.42 8675999448  712318917  91.79
136822760 219706637 1.61 45129020910  651436401   1.44 8689831639  617678747  92.89
[...]
```

最关键的一列是 IPC（每周期指令数）：

> This showed instructions per cycle (IPC) of around 0.57 for the VM versus around 1.52 for the container: a 2.6x difference.

原文把 2.6x 的差距拆成两个来源，其中主因是 LLC 命中率：

> One reason for the lower IPC could be hyperthread contention, as the VM host was running at over 50% CPU utilization. The last column showed an additional reason: the last-level cache (LLC) hit ratio was only 30% for the VM, versus around 90% for the container.

**再往下拆一层**，LLC 命中率低有三个候选原因（L236-L242）：

1. LLC 更小（33 vs 45 Mbytes）；
2. VM 上跑的是**完整应用**而不是单个子组件，指令和数据都更多，更难被缓存；
3. CPU 饱和导致上下文切换频繁、在代码路径间跳转（含用户态与内核态），加大缓存压力。

第三条原文明确说「could be investigated using tracing tools」（L244）——于是分析自然进入下一层。

**判据**：IPC 不是一个可直接优化的指标，它是多个上游因素的**合成量**。看到 IPC 低，要先拆成「超线程争用 / 缓存命中率 / 切换率」三项分别测，否则没有可执行动作。

## §16.1.6 软件事件层

**机制**：上下文切换率是软件事件（用软件实现的计数，而非硬件 PMC）。`perf stat -e cs` 测系统范围：

```bash
serverA# perf stat -e cs -a -I 1000
#           time             counts unit events
     1.000411740          2,063,105      cs
     2.000977435          2,065,354      cs
     3.001537756          1,527,297      cs
     4.002028407            515,509      cs
     5.002538455          2,447,126      cs
     6.003114251          2,021,182      cs
     7.003665091          2,329,157      cs
     8.004093520          1,740,898      cs
     9.004533912          1,235,641      cs
    10.005106500          2,340,443      cs
^C    10.513632795          1,496,555      cs
```

> This output showed a rate of around two million context switches per second.

容器侧按 PID 匹配（排除其它容器）后：

```bash
serverB# perf stat -e cs -p 1928219 -I 1000
#           time             counts unit events
     1.001931945              1,172      cs
     2.002664012              1,370      cs
     3.003441563              1,034      cs
     4.004140394              1,207      cs
     5.004947675              1,053      cs
     6.005605844                955      cs
     7.006311221                619      cs
     8.007082057              1,050      cs
     9.007716475              1,215      cs
    10.008415042              1,373      cs
^C    10.584617028                894      cs
```

两者相差约三个数量级。原文给出的机制是：切换率高会让 CPU 缓存承受更多压力，因为在**不同代码路径之间来回跳**，其中还包括管理切换本身的内核代码，甚至不同进程之间。

**两处值得注意的细节**：

- 脚注说，某些处理器与内核配置下，上下文切换**还会冲掉 L1 缓存**（L294）；
- 脚注说 VM 侧**没有**保留 PID 匹配后的输出，原文的原话是「Then why don't I include the PID-matching output for the VM? I don't have it.」（L271）——作者选择不掩饰这份证据缺口。

**判据**：切换率必须**按 PID 匹配后再比**，否则系统范围的数字会把邻居容器算进来，量级差会被稀释掉。

## §16.1.7 追踪层

**机制**：切换率只是「多少次」，还差「每次跑多久」。`cpudist(8)` 给出 on-CPU 时长分布。VM 侧：

```bash
serverA# cpudist -p 4093 10 1
Tracing on-CPU time... Hit Ctrl-C to end.

     usecs               : count     distribution
         0 -> 1          : 3618650  |****************************************|
         2 -> 3          : 2704935  |*****************************           |
         4 -> 7          : 421179   |****                                    |
         8 -> 15         : 99416    |*                                       |
        16 -> 31         : 16951    |                                        |
        32 -> 63         : 6355     |                                        |
        64 -> 127        : 3586     |                                        |
       128 -> 255        : 3400     |                                        |
       256 -> 511        : 4004     |                                        |
       512 -> 1023       : 4445     |                                        |
      1024 -> 2047       : 8173     |                                        |
      2048 -> 4095       : 9165     |                                        |
      4096 -> 8191       : 7194     |                                        |
      8192 -> 16383      : 11954    |                                        |
     16384 -> 32767      : 1426     |                                        |
     32768 -> 65535      : 967      |                                        |
     65536 -> 131071     : 338      |                                        |
    131072 -> 262143     : 93       |                                        |
    262144 -> 524287     : 28       |                                        |
    524288 -> 1048575    : 4        |                                        |
```

> This output shows that the application typically spent very little time on-CPU, often less than 7 microseconds.

容器侧分布完全换了个形状：

```bash
serverB# cpudist -p 1928219 10 1
Tracing on-CPU time... Hit Ctrl-C to end.

     usecs               : count     distribution
         0 -> 1          : 0        |                                        |
         2 -> 3          : 16       |                                        |
         4 -> 7          : 6        |                                        |
         8 -> 15         : 7        |                                        |
        16 -> 31         : 8        |                                        |
        32 -> 63         : 10       |                                        |
        64 -> 127        : 18       |                                        |
       128 -> 255        : 40       |                                        |
       256 -> 511        : 44       |                                        |
       512 -> 1023       : 156      |*                                       |
      1024 -> 2047       : 238      |**                                      |
      2048 -> 4095       : 4511     |****************************************|
      4096 -> 8191       : 277      |**                                      |
      8192 -> 16383      : 286      |**                                      |
     16384 -> 32767      : 77       |                                        |
     32768 -> 65535      : 63       |                                        |
     65536 -> 131071     : 44       |                                        |
    131072 -> 262143     : 9        |                                        |
    262144 -> 524287     : 14       |                                        |
    524288 -> 1048575    : 5        |                                        |
```

> The involuntary context switches on the VM, and subsequent high rate of context switches seen earlier, caused performance problems. Causing the application to leave the CPU after often less than 10 microseconds also does not give the CPU caches much time to warm up to the current code paths.

**关键补证**：VM 上是**非自愿**上下文切换（被抢占，而不是自己让出）——用 `stackcount(8)` 统计 `t:sched:sched_switch` 与读 `/proc/PID/status` 得到，后者把这些计数叫 `nonvoluntary_ctxt_switches`（脚注 L332）。

**闭环**：饱和 → 被频繁抢占 → 每段只跑不到 10 微秒 → 缓存来不及预热 → 上下文切换更多 → 更难预热。这是一个**自我强化的循环**，也是 3-4x 差距的真正来源。

**判据**：均值会骗人，分布不会。「平均每次只跑 3 微秒」和「平均每次跑 3 毫秒」在均值口径下可能被混为一谈，但它们对缓存预热的影响差三个数量级。

## §16.1.8 结论

**机制**：原文把原因拆成三条**并行**的贡献项。先把前面各层的证据汇总：

| 指标 | VM（serverA） | 容器宿主（serverB） |
|---|---|---|
| CPU 数 | 48 | 64 |
| load average（1 分钟） | 85.09 | 17.94 |
| CPU 利用率 | 98% | 22% |
| %idle | 约 2% | 约 78% |
| %sys | 约 8% | 0.38% |
| 上下文切换率 | 约 200 万/秒（系统范围） | 约 1000/秒（按 PID） |
| IPC | 约 0.57 | 约 1.52 |
| LLC 命中率 | 30% | 约 90% |
| on-CPU 时长 | 常 < 7 微秒 | 2-4 毫秒 |

**原因一 · 没有容器邻居**：

> No container neighbors : The container host was idle except for the one container. This allowed the container to have the entire CPU caches all to itself, as well as to run without CPU contention. While this produced container-favorable results during the test, it is not the expected situation for long-term production use where neighboring containers will be the norm. The microservice may find that the 3-4x performance win vanishes when other tenants move in.

**原因二 · LLC 尺寸与负载差异**：IPC 低 2.6 倍能解释其中 2.6 倍，主因是 LLC 命中率 30% vs 90%，而命中率低又有三个原因（L376-L380）：LLC 更小（33 vs 45 Mbytes）、VM 上跑的是完整应用（指令与数据更多）、以及切换率高达约 200 万/秒让线程跑不满（on-CPU 通常 < 10 μs，而容器宿主是 2-4 ms）。

**原因三 · CPU 负载差异**：

> CPU load difference : A higher load was directed to the VM, driving the CPUs to saturation: a CPU-based load average of 85 on a 48-CPU system. This caused a rate of around 2 million context switches per second, and run queue latency as threads waited their turn.

**必须一起读的两条警告**：

1. **原因一不可持续**——测试时宿主除这一个容器外空闲，长期生产里邻居容器是常态，原文明确说 3-4x 的收益可能随其它租户进场而消失。
2. **三个原因不是三选一**——它们是并行的三个量级，原文没有把 2.6x 与 4x 合成一个数（见「书里没说的」）。

**判据**：给结论时必须同时回答两件事：**各个原因分别贡献了多少量级**，以及**这个优势在生产条件下还成不成立**。只回答前者是技术报告，只回答后者是业务决策。

## §16.2 案例来源

**机制**：原文给了一个很实用的建议——**真实案例藏在 bug 数据库里**。公司的 bug/工单系统、所用应用与操作系统的公开 bug 库，往往以「问题陈述 → 最终修复」的形式保存完整过程；带时间戳的评论历史还能复原分析的推进过程，包括被试过的假设和走过的弯路。

> Taking wrong turns, and identifying multiple contributing factors, is normal.

**为什么这一节值得保留**：它把「性能分析」从个人手艺变成可检索的公共资产。原文还指出 USENIX ;login: 与 ACM Queue 这类实务期刊也常以案例作为技术方案的上下文。

**判据**：接手一个新系统前，先去它的 bug 库捞一遍带时间戳的评论——那里面常有别人已经做过的排除法，比自己从头猜便宜得多。

## 章末选型判据

| 场景 | 动作 | 别做什么 |
|---|---|---|
| 收到「新平台快 N 倍」的报告 | 先证明对照组没被压满：查 load/CPU 比值、CPU 利用率、邻居负载 | 直接归因于新平台，把收益写进容量规划 |
| 只有一组候选对照 | 用负载均衡分流，同时登两套环境跑同一条命令 | 拿两个不同时段的数据直接相减 |
| load average 很高 | 先除以 CPU 数得到饱和度，再用 vmstat run queue 与 mpstat idle 交叉验证 | 把 load average 当成 CPU 利用率百分比直接读 |
| IPC 偏低 | 拆成超线程争用 / LLC 命中率 / 切换率三项分别测 | 把 IPC 当成可直接优化的单一指标 |
| 怀疑上下文切换太多 | 先按 PID 匹配再测速率，再用 tracing 看 on-CPU 时长分布 | 只看系统范围的 cs 计数就下结论 |
| 分析权限受限 | 先确认能否拿到宿主层权限与内核权限 | 在只有容器内可见性的条件下硬猜原因 |
| 要给出结论 | 同时给出各原因的量级，以及该优势在生产里是否持续 | 只给一个根因标签 |
| 接手陌生系统 | 先查它的 bug 库与工单系统的历史评论 | 从零开始重复别人做过的排除法 |