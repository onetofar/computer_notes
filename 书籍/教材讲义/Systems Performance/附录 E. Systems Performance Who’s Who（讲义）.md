---
title: 附录 E. Systems Performance Who’s Who（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - 工具史
  - 人物
  - DTrace
  - BPF
  - 机械同理心
source: "《Systems Performance》（Brendan Gregg，2018，英文原版）附录 E：Systems Performance Who’s Who"
companion: "[[附录E-Appendix E： Systems Performance Who’s Who]]"
---

# 附录 E 讲义：一份工具家谱 —— 每个工具背后站着谁

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[附录E-Appendix E： Systems Performance Who’s Who|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：`src/net/metrics.h`（本仓的指标集合**事实上已经是 USE 方法** —— 只是没人这么叫它）、`src/net/byte_codec.h:14`（「定长写入器」而不是「往 vector 里 push_back」—— **机械同理心**的实例）、`src/net/frame_codec.h:53`（`BeginFrame` —— 自造，不属于任何一脉）。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原附录。

> ⚠️ **这一篇是名单，不是知识。** 原文稿 201 行，
> 八十多个人名 + 各自的东西。
> **它的价值不在「记住人名」，在「看出工具之间的血缘」。**

---

## 0. 开局：`iostat` 是谁写的？

你用 `iostat -x 1`，看到一个 `r_await` 列，有点困惑：为什么磁盘 I/O 的延迟
要拆成「读延迟」「写延迟」「合并延迟」三个数？

man page 会告诉你每个数是什么意思，**但不会告诉你这个划分是从哪来的**。

翻到这份名单，你会看到一行：

> Sebastian Godard : sysstat package for Linux, which contains numerous performance tools including iostat(1), mpstat(1), pidstat(1), nfsiostat(1), cifsiostat(1), and an enhanced version of sar(1), sadc(8), sadf(1)

⇒ `iostat` 和 `sar` **是同一个包里的兄弟**。这解释了一个你可能没注意的事实：
**这两个工具的输出格式是同源的**（百分比列的排版方式、`%` 后缀的用法都一致）。

⇒ 但真正让你受益的是**更大的那张图**：这份名单把八十多个人按**技术血缘**排开之后，
你会发现 **DTrace 和 eBPF 是同一场运动的两个阶段**，
而 `iostat`/`sar`/`vmstat`/`top` 这些「老工具」其实是**不同的血脉**，
它们在不同的年代、不同的公司、为了不同的问题被造出来。

---

## 1. §附录 E 这份名单的读法

### 1.1 作者自己先道了歉

> It can be useful to know who created the technologies that we use.

> This was inspired by the Unix who's who list in [Libes 89] .

> Apologies to those who are missing or misappropriated.

**「missing or misappropriated」—— 漏掉的，和归错人的。**
作者明说这份名单**可能有错**。

⇒ 所以正确的用法是：**把它当索引，不当权威。**
要查准确归属，去下一个地方：

> If you wish to dig further into the people and history, see the chapter references and the names listed in the Linux source, both the Linux repository history and the MAINTAINERS file in the Linux source code.

⇒ **Linux 源码里的 `MAINTAINERS` 文件** —— 这才是名单的「活版本」。
作者指了一个**可持续维护的机制**，而不是一份会腐烂的静态清单。
这是这份附录最实用的一句话。

### 1.2 把八十多个人压成七个集群

按人读是名单，按集群读才是知识：

| 集群 | 代表人物 | 东西 |
|---|---|---|
| **Unix 奠基** | Ken Thompson, Dennis Ritchie, Bill Joy, Marshall Kirk McKusick | Unix, FFS, `vmstat(1)`, swapping, buffer cache, 进程优先级 |
| **Sun / Solaris 观测** | Bryan Cantrill, Michael Shapiro, Richard McDougall, Jim Mauro, Jerry Jelinek, Roger Faulkner | **DTrace**, Solaris Performance and Tools, `truss(1)`, Zones |
| **Linux 内核调度与内存** | Ingo Molnar, Peter Zijlstra, Robert Love, Christoph Lameter, Andrew Morton, Arjan van de Ven | O(1) 调度器, CFS, 自适应自旋互斥量, SLUB, `fadvise`, irqbalance, futex |
| **BPF 家族** | Daniel Borkmann, Alexei Starovoitov, Yonghong Song, Sasha Goldshtein, Brenden Blanco | extended BPF, BTF, BCC, bpftrace |
| **动态插桩** | Frank Ch. Eigler, Jeffrey Hollingsworth, Barton Miller, Richard J. Moore | SystemTap, DProbes, kprobes |
| **测量方法论** | Mike Loukides, Adrian Cockcroft, Dr. Neil Gunther, John Allspaw, Gian-Paolo D. Musumeci, Cary Millsap | 资源分析传统, Method R, Universal Scalability Law, 容量规划 |
| **网络与工具杂技** | Van Jacobson, Mike Muuss, William LeFebvre, Dag Wieers, Arjan van de Ven | `traceroute(8)`, `ping(8)`, `top(1)`, `dstat`, latencytop |

> **推演（讲解性类比，非原文）**：
> 这七个集群像**七个学派**。你学的每个工具都有出身 ——
> 知道出身，你才知道**该拿它去回答哪一类问题**，
> 以及**它的盲区在哪**（DTrace 出身于 Solaris，所以它天生是「全系统追踪」；
> eBPF 出身于内核网络栈，所以它天生擅长「内核事件」；
> `iostat`/`sar` 出身于 sysstat，所以它们天生是「采样计数器」）。

### 1.3 一条作者自己的线索：BPF 工具的谱系

> The Acknowledgments section of my BPF book [Gregg 19] also lists various technologies, in particular extended BPF, BCC, bpftrace, kprobes, and uprobes, and the people behind them.

作者还给了两个入口：**他自己那本 BPF 书的致谢**，以及**Linux 源码**。

⇒ 名单本身是「索引」，作者反复在指路：真正的细节在**源码的 `MAINTAINERS`**、
在**技术的作者自己的书**里。

---

## 2. 几条值得记的血缘线索

### 2.1 `sar` / `iostat` / `mpstat` / `pidstat` 是一家人

> Sebastian Godard : sysstat package for Linux, which contains numerous performance tools including iostat(1), mpstat(1), pidstat(1), nfsiostat(1), cifsiostat(1), and an enhanced version of sar(1), sadc(8), sadf(1)

⇒ 附录 B 那张 `sar` 速查表里的**每一个选项**，背后是同一个包。
**`sar -q`（运行队列）和 `iostat -x`（磁盘）能拼在一起看，是因为它们本来就是一个工具箱。**

### 2.2 DTrace 与 eBPF 是「同一场运动的两个阶段」

名单里两个人写着一模一样的前缀：

> Bryan Cantrill : Co-created DTrace; Oracle ZFS Storage Appliance Analytics.

> Michael Shapiro : Co-created DTrace.

而 BPF 那一簇是：

> Daniel Borkmann : Co-creator and maintainer of extended BPF.

> Alexei Starovoitov : Co-creator and maintainer of extended BPF.

> Alastair Robertson : Created bpftrace.

⇒ **推演（讲解性类比，非原文）**：
DTrace 是「**用一种安全的脚本语言做动态追踪**」，
eBPF 是「**用受验证的字节码在内核里做同样的事**」。
DTrace 的路子被内核出于安全考虑挡住了（脚本能做的事太多），
eBPF 是**绕开那个限制的续集**。
⇒ 所以现代 Linux 上 `bpftrace` 做的事，
**在概念上是 DTrace 的位置**，只是换了实现约束。

### 2.3 「机械同理心」这四个字，是这一整份名单的**价值观**

名单里最短的一条：

> Martin Thompson : Mechanical sympathy.

**整条只有四个词 + 一个人名。**
机械同理心 = **顺着机器的工作方式来设计软件**，而不是强迫机器适应你的抽象。

名单里还有一条同源的：

> Ingo Molnar : O(1) scheduler, completely fair scheduler, voluntary kernel preemption, ftrace, perf, and work on real-time preemption, mutexes, futexes, scheduler profiling, work queues.

> Peter Zijlstra : Adaptive spinning mutex implementation, hardirq callbacks framework, other Linux performance work.

⇒ 调度器、自旋互斥量、futex —— **这一簇的共同点是「顺着 CPU 的执行模型来设计」**。

### 2.4 方法论那一簇

> Mike Loukides : First book on Unix systems performance [Loukides 90] , which either began or encouraged the tradition of resource-based analysis: CPU, memory, disk, network.

> Cary Millsap : Method R.

> Dr. Neil Gunther : Universal Scalability Law, ternary plots for CPU utilization, performance books [Gunther 97].

> John Allspaw : Capacity planning [Allspaw 08] .

> Gian-Paolo D. Musumeci : System Performance Tuning , 2nd Ed. [Musumeci 02] .

⇒ **资源分析（CPU / 内存 / 磁盘 / 网络）这套提问方式**，
在名单里有一个明确的起点（Loukides 的书），此后分出好几条支线：
Method R（把工作量当自变量）、Universal Scalability Law（可扩展性定律）、
容量规划、SP 自己的 Tuning 一脉。

⇒ **USE 方法**则是作者本人的：

> Brendan Gregg : nicstat(1), DTraceToolkit, ZFS L2ARC, BPF tools (execsnoop, biosnoop, ext4slower, tcptop, etc.), BCC/bpftrace contributions, USE method, heat maps (latency, utilization, subsecond-offset), flame graphs, flame scope, this book and previous ones [Gregg 11a] [Gregg 19] , other perf work.

⇒ **热力图（latency / utilization / subsecond-offset）和火焰图**也在这里 ——
这两个是「**把一批散点聚成一张图**」的思路，和名单里其它工具的「**做计数器**」是不同的物种。

### 2.5 网络那一支

> Van Jacobson : traceroute(8), pathchar, TCP/IP performance.

> Bill Joy : vmstat(1), BSD virtual memory work, TCP/IP performance, FFS.

> Mike Loukides : ...

⇒ Van Jacobson 一个人贡献了 `traceroute`、路径 MTU 发现（`pathchar`）和 TCP/IP 性能工作。
**「网络」这一整章的技术源头，基本可以追到 BSD 和 Jacobson 那一脉。**

---

## 3. 实战锚点：这份名单和 `otf-server` 有什么关系？

### 3.1 短答：**零交集** —— 而这是**对的**

本仓的观测面是**应用内埋点**：

| 工具 | 本仓用吗 |
|---|---|
| `sar` / `iostat` / `vmstat` / `top` / `perf` / `bpftrace` / `ftrace` / `eBPF` | ❌ **一个都没有** |
| 自造固定计数器端点 | ✅ `src/net/metrics.cpp` + `metrics_server.cpp` |

⇒ **它不属于名单上任何一脉**，因为它是**应用自己写的**。

⇒ **但这恰恰是本篇要讲的第一件事**：
**「本仓库不用 sar」不是缺陷，是结论。**
这份名单告诉你 `sar` / `iostat` 那一族是为**系统级**问题准备的；
本仓的问题域是「一个进程里的连接与 tick」，
**系统级工具在这里能测的东西，本仓自己埋点测得更准**
（比如 `tick_drift_max_ms` 这种量，**`sar` 根本没有对应的项**）。

⇒ 名单的真正用途是**反向的**：它列出「有哪些现成的工具和它们各自管什么」，
于是你能判断**「我这个问题的现成工具是什么」** —— 答案是「没有，那就自己埋」。

### 3.2 但有一处交集，而且挺有意思：本仓的指标集合**事实上已经是 USE 方法**

SP 附录 A（USE Method: Linux）在库里已经有一篇讲义。
USE 方法问三件事：**Utilization（利用率）· Saturation（饱和）· Errors（错误）**。

把本仓 `src/net/metrics.h` 的 `MetricsSnapshot` 字段按这三栏摆一摆：

| USE 的三问 | 本仓的对应字段 | 是不是真的对应？ |
|---|---|---|
| **Utilization** | `connections`、`rooms` | ✅ 有多少连接、多少房间 |
| **Saturation** | `tick_drift_max_ms`、`snapshot_send_ms_max` | ✅ 排队/漂移 = 饱和的直接度量 |
| **Errors** | `heartbeat_timeouts`、`oversize_rejects` | ✅ **这两个字段的注释里明写「攻击面指标」** |

⇒ **本仓的指标集合事实上已经是一份 USE 三件套，只是没人这么叫它。**

⇒ 这个发现的意义在于：
**「要不要引入一套观测框架」这个问题，在本仓已经有答案了 —— 已经有一套，
只是没有名字。** 有了名字，它就有了**可扩展的检查清单**：
下一个要加指标时，先问「这是 U、S 还是 E？」，而不是随手加一个计数器。

（`MetricsSnapshot` 里还有三个 `epoll_ctl_*` 字段，它们**不属于 USE 三件套** ——
那是**不变量计数器**（`epoll_ctl_add_with_out` 应恒为 0），
属于另一个类别：**「正确性断言的证据」**。）

### 3.3 「机械同理心」在本仓的一个实例

`src/net/byte_codec.h:14` 有一条设计说明：

> ★ 为什么 ByteWriter 是「定长写入器」而不是「往 vector 里 push_back」：

理由写在 `src/net/frame_codec.h:44-47`：

> payload 从空 vector 开始逐字节 push_back，容量按 1→2→4→…→128 指数增长，
> 每一档都是一次 realloc + 一次拷贝；而「拷进 out」又是一轮全量搬运。

⇒ **这就是 Martin Thompson 那四个字。**
协议规定了一帧的**长度是确定的**（变长消息的长度也能在编码前算出来），
那就不该让内存去适应一个「不知道会有多长」的容器 ——
**应该让内存的形状去适应协议。**
所以有 `BeginFrame`（先把 `out` 撑到最终大小、返回写入起点），而不是「先造 payload 再拷进去」。

⇒ 同一思路在 `src/net/frame_codec.cpp:173` 也出现了一次：

```cpp
queue_.push_back(std::move(f));
```

`Frame` 里带着 `std::vector<uint8_t> payload`。
**这里用 `std::move` 而不是拷贝，是同一个道理**：
帧已经解码好了，它的 payload 是一块**现成的内存**，
再拷贝一份就是「强迫内存做一次它不需要做的搬运」。

⇒ **推演（讲解性类比，非原文）**：
机械同理心不是一条「性能优化」的规矩，
它是一条「**不要向抽象要它不该要的东西**」的规矩 ——
抽象层越薄，越贴着底下那台机器的真实形状。

### 3.4 一处「名单上没有、但本仓欠着」的对照

名单里有：

> John Levon : OProfile.

> Arnaldo Carvalho de Melo : Linux perf(1) maintainer.

> Steven Rostedt : Ftrace, KernelShark, real-time Linux, adaptive spinning mutexes, Linux tracing support.

⇒ **动态追踪这一整族（OProfile / perf / ftrace / SystemTap / DTrace）本仓一个都没有。**

⇒ 而这在本仓**是有代价的**，而且代价可以具体说：

**当一个 bug 只在「特定时序」出现时（比如「偶发的 0.5 秒卡顿」），
应用内埋点帮不上忙** —— 因为你不知道「那 0.5 秒里，进程在干什么」。

本仓现在的对策是**故障注入**：`src/main.cpp` 里有一个一次性的
`debug_stall_ms` 注入（`nanosleep`），用来**主动制造**那种时序。
⇒ **这是「没有追踪工具」时的替代方案：用可复现性换可观测性。**
但它只能覆盖「你已经想到的那种故障」。

⇒ 这一条正是 SP 第 4 章和第 13–15 章（perf / Ftrace / BPF）存在的理由，
也正是**为什么本篇要给你这份名单**：
**名单上每一行，都是一个你将来可能会需要的能力。**

---

## 4. 收尾：本章金句清单

1. **It can be useful to know who created the technologies that we use.** —— 名单的正当性：**知道工具是谁造的，才知道该拿它干什么**。
2. **This was inspired by the Unix who's who list in [Libes 89] .** —— 这份名单有个**前世**（Unix 世界的同类名单）。
3. **Apologies to those who are missing or misappropriated.** —— ⚠️ **作者自己承认名单可能有错** ⇒ 它是索引，不是权威。
4. **If you wish to dig further into the people and history, see the chapter references and the names listed in the Linux source, both the Linux repository history and the MAINTAINERS file in the Linux source code.** —— **名单的「活版本」在 Linux 源码的 `MAINTAINERS` 里** —— 这才是可持续维护的做法。
5. **The Acknowledgments section of my BPF book [Gregg 19] also lists various technologies, in particular extended BPF, BCC, bpftrace, kprobes, and uprobes, and the people behind them.** —— 名单的另一个入口：**技术的作者自己的书**。
6. **Jeff Bonwick : Invented kernel slab allocation, co-invented user-level slab allocation, co-invented ZFS, kstat, first developed mpstat.** —— 一个人横跨**分配器、文件系统、统计接口、性能工具**四层。
7. **Sebastian Godard : sysstat package for Linux, which contains numerous performance tools including iostat(1), mpstat(1), pidstat(1), nfsiostat(1), cifsiostat(1), and an enhanced version of sar(1), sadc(8), sadf(1)** ⇒ **`sar` 与 `iostat` 是一家人**，输出格式同源。
8. **Bryan Cantrill : Co-created DTrace; Oracle ZFS Storage Appliance Analytics.** —— DTrace 的作者之一，后来做 ZFS 存储设备分析。
9. **Michael Shapiro : Co-created DTrace.** —— DTrace 的另一位作者。
10. **Daniel Borkmann : Co-creator and maintainer of extended BPF.** —— extended BPF 的起点。
11. **Alastair Robertson : Created bpftrace.** —— bpftrace 的作者。
12. **Brendan Gregg : nicstat(1), DTraceToolkit, ZFS L2ARC, BPF tools (execsnoop, biosnoop, ext4slower, tcptop, etc.), BCC/bpftrace contributions, USE method, heat maps (latency, utilization, subsecond-offset), flame graphs, flame scope** ⇒ 本书作者自己的**工具清单**。
13. **Martin Thompson : Mechanical sympathy.** —— **整条名单最短的一条，也是整套价值观**：顺着机器的工作方式来设计软件。
14. **Ingo Molnar : O(1) scheduler, completely fair scheduler, voluntary kernel preemption, ftrace, perf, and work on real-time preemption, mutexes, futexes, scheduler profiling, work queues.** —— 调度器 + 抢占 + 追踪 + futex，一整条 Linux 性能主线。
15. **Peter Zijlstra : Adaptive spinning mutex implementation, hardirq callbacks framework, other Linux performance work.** —— **自适应自旋互斥量**：旋一会儿发现拿不到锁就睡觉，是「机械同理心」的教科书样本。
16. **Mike Loukides : First book on Unix systems performance [Loukides 90] , which either began or encouraged the tradition of resource-based analysis: CPU, memory, disk, network.** —— **「CPU / 内存 / 磁盘 / 网络」这套提问方式的起点**。
17. **Cary Millsap : Method R.** —— Method R：把**工作量**当自变量，而不是把资源当自变量。
18. **Dr. Neil Gunther : Universal Scalability Law, ternary plots for CPU utilization, performance books [Gunther 97].** —— 可扩展性定律与**三元图**。
19. **Van Jacobson : traceroute(8), pathchar, TCP/IP performance.** —— 网络这一整章的技术源头。
20. **Bill Joy : vmstat(1), BSD virtual memory work, TCP/IP performance, FFS.** —— **`vmstat` 的作者**，也是 FFS 的人。
