---
title: Chapter 4. Observability Tools（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - Systems Performance
  - 性能工程
  - 可观测性
  - tracepoints
  - kprobes
source: "《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg，Addison-Wesley 2020，英文原版）第 4 章"
companion: "[[04-Chapter 4. Observability Tools]]"
---

# 第 4 章 讲义：没有观测就没有性能

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[04-Chapter 4. Observability Tools|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：`otf-server/src/net/metrics_server.cpp` —— 它到底暴露了什么，以及**采样开销有多大**。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原章。

---

## 0. 开局：凌晨三点，你在等一个数字

线上告警：「副本 A 的 p99 延迟涨到 800 ms。」

你打开服务器：

```
$ top
$ iostat -x 1
$ ss -s
$ cat /proc/net/dev
$ sar -n DEV 1
```

**没有一条告诉你「是谁的问题」。** 上面这堆数字全是真的，但它们只能告诉你「现在有点慢」，不能告诉你「**慢在哪个环节**」。

于是你开始犯第 1 章警告过的错误：凭印象猜。

> **没有观测就没有性能。** 不是「观测 nicer」，是「**不观测就只有猜**」。

本章要给你的是**观测的地图**：有哪些数据源、每种源能回答什么问题、每种源的**开销是多少**。最后一条最要紧——**观测本身要花钱**，而这笔账必须有人算。

---

## 1. §4.1 Tool Coverage：工具覆盖图

原文 `Figure 4.1` 是一张**标注了 Linux 观测工具的操作系统图**。作者在脚注里讲了这张图的来历：2000 年代中期教性能课时，他习惯在白板上画自己的内核图，然后标上各个性能工具及其观测对象——**把它当作一种思维地图**。后来他把数字化版本发布到网上，现在贴得到处都是。

原文的观察：这些工具大多**聚焦于某个资源**（CPU、内存、磁盘），在各资源专章里讲；也有几个**多面手**（perf、Ftrace、BCC、bpftrace）在本章后面介绍。

### 1.1 §4.1.1 Static Performance Tools：静态性能工具

有一类观测检查的是**静止状态下**系统的属性，而不是活跃负载下的行为。这对应第 2 章的 §2.5.17 静态性能调优方法论。

原文提醒：

> Remember to use the tools in Figure 4.2 to check for issues with configuration and components. Sometimes performance issues are simply due to a misconfiguration.

**「有时候性能问题就是配置错了。」** 这一条值得反复读——本仓库的 `kMaxSessions = 64`、`kMaxFd = 1024`、`kSendBufCap = 128KB` 全是配置项，任何一个被改错都会变成性能问题。

### 1.2 §4.1.2 Crisis Tools：救火工具

原文描述了一个真实的窘境：

> When you have a production performance crisis that requires various performance tools to debug it, you might find that none of them are installed. Worse, since the server is suffering a performance issue, installing the tools may take much longer than usual, prolonging the crisis.

**服务器正病着，你却要现装工具——而且装工具本身要占 CPU，反而更慢。** 原文因此给了 `Table 4.1`：Linux 上各危机工具对应的安装包（Ubuntu/Debian 的包名）。

> （推演）这条的实践含义是**镜像里就该装好工具**。生产环境的镜像不是「跑应用的最小集合」，而是「能诊断这个应用的集合」。本仓库当前只依赖系统自带的 `gdb`/`strace` 类工具与自制的 metrics 端点——**没有任何一条能回答「我的火焰图长什么样」**。

---

## 2. §4.2 Tool Types：工具的分类学

原文给了一张二维分类图（`Figure 4.3`）：横轴是**系统范围 vs 进程范围**，纵轴是**基于计数器 vs 基于事件**。

|  | 系统范围 | 进程范围 |
|---|---|---|
| **计数器** | `vmstat(8)`、`mpstat(1)`、`iostat(1)`、`sar(1)` | `ps(1)`、`top(1)`、`/proc/<pid>/*` |
| **事件** | `perf(1)`、`Ftrace`、`BCC`、`bpftrace`、`tcpdump(8)` | `strace(1)`、`gdb(1)`、`profile(8)` |

原文说有些工具跨象限（`top(1)` 也有系统范围汇总；系统范围的事件工具常能用 `-p PID` 过滤到某进程）。

> **事件型工具 = profiling + tracing**。profiling 取快照，tracing 插每一次事件。

### 2.1 §4.2.1 Fixed Counters：固定计数器 ★

内核维护各种计数器，通常是**无符号整数**，事件发生时递增（收了多少包、发了多少磁盘 I/O、发生多少次中断）。

原文指出内核的常见做法是**维护一对累加计数器**：

> A common kernel approach is to maintain a pair of cumulative counters: one to count events and the other to record the total time in the event.

**这一对计数器的妙处**：直接相除就得到**平均延迟**；因为是累加的，在两个时刻相减还能得到**每秒次数与平均延迟**。**很多系统统计就是这么算出来的。**

然后是本章最该记住的一句：

> Performance-wise, counters are considered “free” to use since they are enabled by default and maintained continually by the kernel. The only additional cost when using them is the act of reading their values from user-space (which should be negligible).

**计数器是「免费」的**——因为内核**本来就在维护**它们，你只是去读。

但注意前提：**「免费」指的是 CPU 开销可忽略，不等于「零成本」**。读 `/proc` 要系统调用，要拷贝，要解析。§4.3.1 会说 `/proc` 的读开销通常可忽略，**例外是某些与内存映射相关、会遍历页表的文件**。

### 2.2 §4.2.2 Profiling：性能剖析

> Profiling characterizes the target by collecting a set of samples or snapshots of its behavior.

CPU 是最常见的目标：**定时采样**指令指针或栈回溯，刻画消耗 CPU 的代码路径。

原文给了采样的典型参数：**固定频率（如 100 Hz）、所有 CPU、短时长（如 1 分钟）**，并给了一个细节：

> often use 99 Hz instead of 100 Hz to avoid sampling in lockstep with target activity, which could lead to over- or undercounting.

**用 99 Hz 而不是 100 Hz，是为了避免与被测对象「同频共振」**（否则采样点系统性地落在特定相位上，导致多算或少算）。这是采样方法论里非常典型的「反直觉但正确」的设计。

Profiling 也可以基于**非定时**的硬件事件（缓存未命中、总线活动），特别有助于优化内存使用。

**开销警告**（原文明确）：

> Unlike fixed counters, profiling (and tracing) are typically only enabled on an as-needed basis, since they can cost some CPU overhead to collect, and storage overhead to store.

并且原文给了选择依据：

> Timer-based profilers are generally safer: the event rate is known, so its overhead can be predicted, and the event rate can be selected to have negligible overhead.

**定时采样更安全**：事件率已知 ⇒ 开销可预测 ⇒ 可以选一个「开销可忽略」的频率。硬件事件采样则不然。

工具清单：

- 系统范围：`perf(1)`（标准 Linux profiler）、`profile(8)`（BCC 的 BPF CPU profiler，在内核上下文频次统计栈）、Intel VTune Amplifier XE
- 进程范围：`gprof(1)`（分析编译器插入的 profiling 信息，`gcc -pg`）、`cachegrind`（valgrind 套件，可分析硬件缓存使用）、Java Flight Recorder

### 2.3 §4.2.3 Tracing：追踪

> Tracing instruments every occurrence of an event, and can store event-based details for later analysis or produce a summary.

与 profiling 的区别在于**意图**：profiling 取样本，tracing **全都要**。

> Tracing can incur higher CPU and storage overheads than profiling, which can slow the target of tracing.

而且原文补了一句容易被忽略的：

> measured timestamps may also be skewed by the tracer.

**追踪器本身会扭曲它测量的时间戳。** 这是用 tracing 数据算延迟时必须记住的事。

**日志**可以看作「默认开启的低频 tracing」：错误、警告等不频繁事件写进日志文件。

工具：

- 系统范围：`tcpdump(8)`（用 libpcap）、`biosnoop(8)`（BCC/bpftrace）、`execsnoop(8)`、`perf(1)`、`perf trace`、Ftrace、BCC、bpftrace
- 进程范围：`strace(1)`、`gdb(1)`

原文对调试器的一句评价值得记住：

> The debuggers can examine per-event data, but they must do so by stopping and starting the execution of the target. This can come with an enormous overhead cost, making them unsuitable for production use.

**调试器必须停/启目标进程，开销巨大，不适合生产。** 所以：

> System-wide tracing tools such as perf(1) and bpftrace support filters for examining a single process and can operate with much lower overhead, making them preferred where available.

**能用系统级工具过滤，就别用调试器。**

### 2.4 §4.2.4 Monitoring：监控

> Unlike the tool types covered previously, monitoring records statistics continuously in case they are later needed.

` sar(1)`（System Activity Reporter，源自 AT&T Unix）是传统的单机监控工具：**基于计数器**，有一个 agent 按计划时间（用 cron）记录系统范围计数器的状态。

---

## 3. §4.3 Observability Sources：数据源地图 ★

原文 `Table 4.2` 是一张**类型 → 数据源**的映射表：

| 类型 | 数据源 |
|---|---|
| 每进程计数器 | `/proc` |
| 系统范围计数器 | `/proc`、`/sys` |
| 设备配置与计数器 | `/sys` |
| Cgroup 统计 | `/sys/fs/cgroup` |
| 每进程追踪 | `ptrace` |
| 硬件计数器（PMC） | `perf_event` |
| 网络统计 | `netlink` |
| 网络抓包 | `libpcap` |
| 每线程延迟指标 | Delay accounting |
| 系统范围追踪 | Ftrace 函数剖析、tracepoints、软件事件、kprobes、uprobes、perf_event |

原文补了一句很实用的建议：

> One way to find such sources is to read the kernel code you are interested in observing and see what statistics or tracepoints have been placed there.

（这句在 §4.3.10「And More」里。）

### 3.1 §4.3.1 /proc

`/proc` 是内核统计的文件系统接口。**每个进程 PID 一个目录**，目录里的文件是从内核数据结构映射出来的信息；另有顶层文件放系统范围统计。

关键性质：

- **动态创建，不落存储**（跑在内存里）
- **基本只读**（部分文件可写，用于控制进程与内核行为）
- 文件系统接口方便：`open()` / `read()` / `close()`，命令行里也能 `cd` / `cat` / `grep` / `awk`
- 用文件访问权限提供用户级安全
- **极端情况下**（`ps(1)`、`top(1)` 都跑不起来时），还能用 shell 内建命令读 `/proc` 做有限的进程调试

开销：

> The overhead of reading most /proc files is negligible; exceptions include some memory-map related files that walk page tables.

### 3.2 §4.3.2 /sys

`sysfs` 挂载在 `/sys`，随 2.6 内核引入，提供**基于目录**的内核统计结构（`/proc` 是历史演化出来的，统计散在顶层）。

原文举 CPU 0 的例子，展示 `/sys/devices/system/cpu/cpu0/` 下的文件（`power/`、`topology/`、`cache/` 等），并用 `grep . /sys/devices/system/cpu/cpu0/cache/index*/level` 读出缓存层次（L1/L2/L3）。

> The /sys file system typically has tens of thousands of statistics in read-only files, as well as many writeable files for changing kernel state.

**几万条只读统计**——这就是「工具覆盖」这个概念的字面含义。

### 3.3 §4.3.3 Delay Accounting ★

开了 `CONFIG_TASK_DELAY_ACCT` 的 Linux 会**按任务**追踪四种状态的时长：

| 状态 | 含义 |
|---|---|
| **Scheduler latency** | 等着轮到自己在 CPU 上跑 |
| **Block I/O** | 等块 I/O 完成 |
| **Swapping** | 等换页（内存压力） |
| **Memory reclaim** | 等内存回收例程 |

**这四种正好对应第 5 章九状态模型里的四种**——是「CPU 到底在等什么」这个问题在 Linux 上的标准答案接口。

原文给了 `getdelays.c` 的输出示例（纳秒为单位），并说明：调度器延迟统计其实来自 `/proc` 里的 `schedstats`（在 `struct sched_info` 里），但暴露时和其他延迟状态放在一起。

### 3.4 §4.3.4 netlink

`netlink` 是取内核信息的**特殊套接字族（AF_NETLINK）**：开一个 AF_NETLINK 套接字，用 `send(2)` / `recv(2)` 传请求、收二进制结构体。

> While this is a more complicated interface to use than /proc, it is more efficient, and also supports notifications.

常用组：`NETLINK_ROUTE`（路由信息）、`NETLINK_SOCK_DIAG`（socket 信息，`ss(8)` 用的就是这个）、`NETLINK_SELINUX`、`NETLINK_AUDIT`、`NETLINK_SCSITRANSPORT`、`NETLINK_CRYPTO`。

用它的命令：`ip(8)`、`ss(8)`、`routel(8)`，以及老的 `ifconfig(8)`、`netstat(8)`。

### 3.5 §4.3.5 Tracepoints ★

tracepoints 是**基于静态插桩**的内核事件源——硬编码在内核代码逻辑位置上的插桩点：系统调用的开始与结束、调度器事件、文件系统操作、磁盘 I/O 都有。

原文给了两个关键事实：

- **基础设施由 Mathieu Desnoyers 开发，Linux 2.6.32（2009）首次提供**
- **「Tracepoints are a stable API」**（原文注释里补了一句：「我会称之为『尽力而为的稳定』。很少见，但我见过 tracepoint 变更。」）

以及为什么它比 kprobes 更值钱：

> While function-based tracing can provide a similar power, only tracepoints provide a stable interface, allowing robust tools to be developed.

**稳定 API 才能长出可靠的工具。**

#### 开销

这一节是本章最有实操价值的数据：

> On typical systems of today (4 to 128 CPUs), I find that event rates of less than 10,000 per second cost negligible overhead, and only over 100,000 does the overhead begin to become measurable.

**每秒 1 万事件以内：开销可忽略。超过 10 万：开始可测量。**

原文给的例子：磁盘事件通常**低于** 1 万/秒；**调度器事件经常远超 10 万/秒**——所以追调度器事件会很贵。

具体数字：

> I’ve previously analyzed overheads for a particular system and found the minimum tracepoint overhead to be 96 nanoseconds of CPU time

**tracepoint 最低 96 ns CPU 时间**。原文还提到 **raw tracepoints**（Linux 4.7，2018）避开了创建稳定 tracepoint 参数的成本，进一步降低这个开销。

**禁用态开销**也不该忽略：

> A disabled tracepoint becomes a small number of instructions: for x86_64 it is a 5-byte no-operation (nop) instruction.

**禁用态的 tracepoint 是 5 字节 nop**——外加函数尾部挂一个 handler，让 text 段略微变大。原文说这些开销很小，但**「你在给内核加 tracepoint 时应该分析并理解它们」**。

### 3.6 §4.3.6 kprobes / §4.3.7 uprobes

**kprobes** 是动态插桩内核函数的办法。原文给的三种接入方式：通过 `/sys` 文件、通过 **`perf_event_open(2)` 系统调用（首选）**、以及 `register_kprobe()` 内核 API。

开销数据：

> The overhead is similar to that of tracepoints when the entries to functions are traced (Ftrace method, if available), and higher when function offsets are traced (breakpoint method) or when kretprobes are used (trampoline method). For a particular system I measured the minimum kprobe CPU cost to be 76 nanoseconds, and the minimum kretprobe CPU cost to be 212 nanoseconds

**kprobe 最低 76 ns；kretprobe 最低 212 ns。**

**uprobes** 是用户态版本（Linux 3.5，2012）。原文说明了它为什么贵：

> uprobes currently work by trapping into the kernel. This costs much higher CPU overheads than kprobes or tracepoints. For a particular system I measured, the minimum uprobe cost 1,287 nanoseconds, and the minimum uretprobe cost 1,931 nanoseconds

**uprobe 最低 1287 ns；uretprobe 最低 1931 ns。**

> The uretprobe overhead is higher because it is a uprobe plus a trampoline function.

**开销排序（本节最实用的一张表）**：

| 手段 | 最低开销 | 说明 |
|---|---|---|
| tracepoint | 96 ns | 静态、稳定 API |
| kprobe | 76 ns | 动态；断点法更高 |
| kretprobe | 212 ns | kprobe + trampoline |
| uprobe | 1287 ns | **要陷入内核** |
| uretprobe | 1931 ns | uprobe + trampoline |

**1287 / 76 ≈ 17 倍。** 这就是「能用 tracepoint 就别用 uprobe」的量化依据。

### 3.7 §4.3.8 USDT

USDT（user statically-defined tracing）是 tracepoints 的用户态版本。原文的类比很清晰：

> USDT is to uprobes as tracepoints is to kprobes.

Java JDK、PostgreSQL、libc 里都有 USDT 探针。原文用 `bpftrace -lv 'usdt:...libjvm.so:*'` 列出 OpenJDK 的探针——**这个 JDK 版本有 524 个 USDT 探针**。

**USDT 的独特价值**（原文举了个好例子）：

> An application-level logger may tell you that a database query was slow due to file system I/O, but a tracer can reveal more information: e.g., the query was slow due to lock contention in the file system, and not disk I/O as you might have assumed.

**日志说「慢在文件系统 I/O」；追踪器告诉你「其实慢在文件系统的锁竞争，不是磁盘 I/O」。**

两个坑：

1. **很多应用打包版本里 USDT 探针没开**（OpenJDK 就是），要用得**从源码重新编译**（配置项可能叫 `--enable-dtrace-probes`，这个命名来自 DTrace）。
2. **USDT 探针必须编进被观测的可执行文件**——这**对 JIT 语言不可能**（Java 边跑边编）。解法是 **dynamic USDT**：把探针预编译成共享库，供 JIT 语言调用；Java、Node.js 等都有这样的库。**解释型语言有同样的问题。**

实现上，USDT 探针在 Linux 里是用 **uprobes** 实现的（所以开销按 uprobe 算），另外还像 tracepoint 一样在代码里放 nop 指令。

### 3.8 §4.3.9 Hardware Counters（PMCs）

PMC 是**可编程的硬件寄存器**，在 CPU 周期级别给出底层性能信息。原文说它们是性能分析的**关键资源**：

> Only through PMCs can you measure the efficiency of CPU instructions, the hit ratios of CPU caches, the utilization of memory and device buses, interconnect utilization, stall cycles, and so on.

（指令效率、缓存命中率、内存/总线/互连利用率、停顿周期——**这些只有 PMC 能测**。）

Intel 选出了七个作为「architectural set」，可用 `cpuid` 指令检查是否存在。

### 3.9 §4.3.10 其他数据源

原文列举：MSRs（模型专用寄存器，含 CPU 时钟频率、使用率、温度、功耗；**一个用途是精确的基于周期的 CPU 利用率测量**）、`ptrace(2)`（进程追踪，`gdb(1)`/`strace(1)` 用；**基于断点，能让目标慢一百倍以上**）、函数剖析（x86 上给所有非内联内核函数入口加 `mcount()` / `__fentry__()`，**不用时转成 nop**）、网络嗅探（libpcap，`/proc/net/dev`，`tcpdump(8)` 消费）、netfilter conntrack、进程记账（Linux `atop(1)` 用它抓短命进程）、软件事件（页错误等，经 `perf_event_open(2)`）、系统调用（`getrusage(2)` 提供用户/系统时间、缺页、消息、**上下文切换次数**）。

**还有更多**：I/O accounting、blktrace、timer_stats、lockstat、debugfs，取决于内核版本与编译选项。

---

## 4. §4.5 Tracing Tools：追踪工具全景

| 工具 | 定位 |
|---|---|
| **perf(1)** | 官方 Linux profiler。CPU profiling（栈采样）与 PMC 分析一流，也能插别的事件，通常记录到文件后处理 |
| **Ftrace** | 官方 Linux tracer，多工具组合。适合内核代码路径分析与**资源受限系统**（无依赖） |
| **BPF（BCC、bpftrace）** | 扩展 BPF 的产物。BCC 功能强大，bpftrace 提供高级语言写 one-liner 与短程序 |
| **SystemTap** | 高级语言与 tracer，有大量 tapset；近期在开发 BPF 后端（作者推荐 `stapbpf(8)`） |
| **LTTng** | 专为黑盒记录优化：最优地记录大量事件供事后分析 |

原文说前三个在第 13/14/15 章细讲，**这个顺序是刻意的**：先讲用法与性能收益，再回头讲工具细节。

以及作者在 Netflix 的实际分工：

> At Netflix, I use perf(1) for CPU analysis, Ftrace for kernel code digging, and BCC/bpftrace for everything else

---

## 5. §4.6 Observing Observability：观测的观测 ★★

**这是全章最该反复读的一节。** 原文的判断很不客气：

> Observability tools and the statistics upon which they are built are implemented in software, and all software has the potential for bugs. The same is true for the documentation that describes the software.

> Regard with a healthy skepticism any statistics that are new to you, questioning what they really mean and whether they are really correct.

**对你不熟的新统计量，要带着健康的怀疑去看。**

原文列了指标可能出的问题：

- **Tools and measurements are sometimes wrong.**
- **Man pages are not always right.**
- 可用指标**不完整**。
- 可用指标**设计糟糕、令人困惑**。
- **指标采集器本身有 bug**（原文脚注里有个真实案例：某基准公司的 shell 脚本用正则把 `100KB/s` 砍成 `100`，又砍掉小数点把 `1.6` 变成 `16`——引入了 1024 倍的误差）。
- 指标处理（算法/电子表格）也会引入误差。

**交叉验证**：

> When multiple observability tools have overlapping coverage, you can use them to cross-check each other. Ideally, they will source different instrumentation frameworks to check for bugs in the frameworks as well.

另一种验证技术：**施加已知负载，然后检查工具的输出是否与你预期一致**。

原文还提醒：**有时错的不是工具或统计量，而是描述它的文档**——软件演进了，文档没跟上。

以及那句很实在的：

> Realistically, you may not have time to double-check every performance measurement you use and will do this only if you encounter unusual results or a result that is company critical.

**最后一句是对「观测的观测」最成熟的表述**：不是每条数据都要验证，但**遇到反常结果或关键结论时必须验证**；而且**即使没验证，心里要清楚自己没验证**。

还有一条关于**缺失**的洞察：

> An absence of metrics can be more difficult to identify than the presence of poor metrics.

**「没有指标」比「指标很差」更难发现**——因为差的指标至少还在那儿，缺失是沉默的。而第 2 章说：「Chapter 2, Methodologies, can help you find these missing metrics by studying the questions you need answered for performance analysis.」

---

## 6. 实战锚点：otf-server 的观测端点与它的采样开销

前面讲的是通用工具箱。现在看本仓库自制的那个观测端点。

### 6.1 它暴露了什么

`src/net/metrics_server.cpp` 在**独立端口**（`kMetricsPort = 7778`，`server_config.h`）上跑一个极简 HTTP 服务，响应是 Prometheus 文本 exposition 格式。

`MetricsServer::Poll()` 的核心：

```cpp
void MetricsServer::Poll() {
  if (listen_fd_ < 0) return;

  // ★ 每轮只 render 一次：brief 把它写在 accept 循环里，连接数 × 字符串拼接。
  const std::string body = metrics_.render();
  const std::string resp = BuildResponse(body.size()) + body;

  int served = 0;
  int intr = 0;
  for (;;) {
    if (served >= kMaxServePerPoll) return;   // 给主循环留出预算
    const int fd = ::accept(listen_fd_, nullptr, nullptr);
    ...
    ++served;
    Serve(fd, resp);
    ::close(fd);
  }
}
```

对照第 4 章的分类：

| 维度 | 本仓库 | 原文分类 |
|---|---|---|
| 数据源 | **应用自己维护的计数器** | 类似 `/proc` 的「固定计数器」 |
| 范围 | 进程级 | 无系统范围视图 |
| 类型 | 计数器（counter）+ 峰值（gauge 上的 max） | 固定计数器 |
| 形态 | **HTTP 拉取（pull）** | 类似 `sar` 的 agent 模式，但反过来 |

**它是 pull 模型**：没有 agent 定期跑、没有时序存储，只有一个「现在是多少」的快照。

### 6.2 采样开销：分三笔账算

原文说计数器「免费」，但那是**内核已经在维护**的情况。本仓库的计数器是**应用自己维护**的，所以开销要自己算。分三笔：

#### 第一笔：写入侧（每次业务事件）

`Metrics` 的所有 `on_*` 都是 `std::atomic` 的 `fetch_add` 或 CAS 循环：

```cpp
void on_connect(std::int64_t d = 1) { conns_.fetch_add(d); }
```

**单次 `fetch_add` 是纳秒级**，且 `metrics.h` 明确说明单线程 reactor 下原子量只是「不留后患」——**不需要锁，也不需要 CAS**（只有取峰值的两条走 CAS 循环）。

**这一笔几乎可以忽略。** 但要注意调用点：`on_frame_sent` / `on_frame_recv` 是**每帧**调用的——20 Hz × N 个连接 × 若干帧。

#### 第二笔：读取侧（每次渲染）

`Metrics::render()` 做的是：

1. `snapshot()`：读 12 个 atomic（`load`）
2. `RenderInto`：**一次 `snprintf`** 把 11 条指标写进**栈上固定缓冲**（`kRenderBuf`）
3. drift 那条**单独** `snprintf` 到 `kDriftBuf`
4. `std::string out` 拷贝一次

**这里的设计值得表扬**（`metrics.cpp` 的注释写得很清楚）：

```cpp
/// 数一遍格式串里有几个 `%` —— 用来把缓冲大小变成**编译期算出的事实**，
/// 而不是注释里一句「1024 应该够了」（那种话一旦加了指标就会悄悄失效）。
constexpr std::size_t CountSpecs(const char* s, std::size_t i) { ... }
static_assert(kSpecCount == 12, "指标个数变了：确认 MetricsSnapshot 字段与格式串一一对应");
```

**缓冲容量由格式串在编译期算出**，不是「留够应该够」。这消除了「加一个指标就悄悄截断」这一整类 bug——**这正是原文 §4.6「观测工具本身会有 bug」的正面例子**。

还有一个更精细的细节：

```cpp
// ⚠️ snprintf 返回的是「本应写入的长度」，**不是**「实际写入的长度」。
//   截断时 want >= kCap，此时 buf 在第 kCap 个字节处**根本没写**。
//   直接 std::string(buf, want) 会去读那 want 个字节 —— 其中
//   (want - 实际写入长度) 个是**未初始化的栈内存**，等于把进程栈残留
//   原样塞进 HTTP 响应发给外部。
```

**这是本仓库观测实现里最值得学习的一处**：它不是「相信 `snprintf` 的返回值」，而是**查了 snprintf 的契约**。原文 §4.6 说「工具和测量有时是错的」——这里的做法是**把已知的坑提前堵死**。

**开销估算**：一次 `render()` 是 12 次 atomic load + 2 次 snprintf + 1 次 ~400 字节的字符串拷贝。**量级在几百纳秒到 1 微秒。**

#### 第三笔：网络侧（每次 scrape）

`Serve()` 里每个连接要做：

- `fcntl(F_GETFL)` + `fcntl(F_SETFL, O_NONBLOCK)` —— **2 次系统调用**（而且**必须**：accept 不继承 O_NONBLOCK，注释里写了「漏掉这次 fcntl 的话，下面那次 read 会在『客户端连上但还没发数据』时永久阻塞 —— 而 `Poll()` 跑在游戏主循环线程上，一卡就是整局卡死」）
- 最多 `kReqReadRounds = 4` 次 `read`（**读不满就当空请求**，只为腾开对端发送窗口）
- 1~8 次 `write`（`kWriteAttempts = 8`），短写时 `::poll(&p, 1, kWriteWaitMs = 50)`

**一个 scrape 大约 5~15 次系统调用。** 若 Prometheus 按 15 秒抓一次，那是每秒约 0.3~1 次系统调用——**相对于主循环每秒 20 次 `epoll_wait` + 每帧的读写，可以忽略**。

### 6.3 真正的问题：端点跑在主循环线程上

这是本节最该指出的事。

`metrics_server.h` 的文件头写了一条硬约束：

> ★ 「独立 socket」本身**不等于**隔离 —— `Poll()` 仍跑在主循环线程上，所以那一侧另有一条硬约束（永不阻塞）。

`main.cpp` 的主循环里确实有：

```cpp
metrics_server.Poll();
```

**每轮主循环调一次。** 也就是说：

**`render()` 是每 50 ms 执行一次，而不是「每个 scrape 一次」。**

`Poll()` 的第一行就是 `const std::string body = metrics_.render();`——**在 accept 之前无条件渲染**。注释里解释了动机（「brief 把它写在 accept 循环里，连接数 × 字符串拼接」——那会按连接数重复渲染，浪费），但代价是：

> **哪怕一个采集器都没有，每 50 ms 也要渲染一次。**

**20 次/秒 × 约 1 微秒 = 每秒约 20 微秒。** 相对于 20 Hz 的 tick 周期（50 ms = 50,000 微秒），这是 **0.04%**。

**结论：这一笔开销小到可以接受，但结构上是可以优化的**——把 `render()` 挪进 accept 之后（渲染一次、缓存起来供本轮所有连接用），就能做到「没人 scrape 就不渲染」。

> （推演）但**别急着改**。原因有二：
> 1. §4.6 的教训是「观测工具本身可能不可靠」。把 `render()` 挪到 accept 之后，会引入一个新状态（「本轮是否已渲染」），而这个状态在 `listen_fd_ < 0` 的早退路径上要特别小心。**用 0.04% 的 CPU 换掉一个潜在的状态 bug，是划算的。**
> 2. 原文第 2 章 §2.5.20 的第一条口诀是「**Don't do it**」——**先别做不必要的改动**。

### 6.4 缺什么：对照第 4 章的能力清单

| 第 4 章提供 | 本仓库现状 |
|---|---|
| **系统范围观测**（`/proc`、`/sys`、`sar`） | ❌ 只有进程自己的指标 |
| **每进程计数器**（`/proc/<pid>/*`、`getrusage(2)`） | ❌ 没有用 `getrusage(2)`——**上下文切换次数、缺页次数、用户/系统时间全都没量** |
| **profiling**（`perf(1)`、`profile(8)`） | ❌ 没有火焰图。`src/sim/` 每帧跑多少 CPU，**完全未知** |
| **tracing**（`strace`、`bpftrace`） | ❌ 没有 `strace` 埋点报告 |
| **延迟统计**（delay accounting、PSI） | ❌ 没用 |
| **tracepoints** | ❌ 不适用（用户态程序） |
| **USDT** | ❌ 没有加任何 USDT 探针 |
| **PMC** | ❌ 不适用（进程级） |

**最有价值的一条**：`getrusage(2)`。它**几乎免费**（一次系统调用），却能给出：

- 用户态时间 / 系统态时间 ⇒ **CPU 利用率**（第 2 章 USE 的 utilization）
- **上下文切换次数** ⇒ 直接回答「我是不是在频繁切模式」（第 3 章）
- 缺页次数 ⇒ 内存压力信号

**这是本仓库从「只有结果指标」走向「有状态指标」的最短路径**，比任何自定义打点都便宜。

### 6.5 一条纪律的移植

原文 §4.6 的核心是「**对不熟的统计量保持怀疑**」。本仓库的 `metrics.h` 里已经有一条同构的纪律：

> ⚠️ 本结构**不做任何钳位**：`on_connect(-1)` 减过头会照实渲染成负数。这是刻意的 —— 钳位会把「调用方记账错了」这个真 bug 变成一个永远停在 0 的健康指标，比负数危险得多。

以及 `tick_drift_max_ms` 的「没测过 ≠ 测出来是 0」。

**这两条正是「不信任指标，但也不掩盖指标」的立场**——与原文的「healthy skepticism」是同一种态度。

---

## 7. 收尾：本章金句清单

1. **Performance-wise, counters are considered “free” to use since they are enabled by default and maintained continually by the kernel.**
   —— 计数器「免费」的前提是**内核本来就在维护**；你自己维护的计数器不免费。

2. **Regard with a healthy skepticism any statistics that are new to you**
   —— 对不熟的统计量保持健康的怀疑，问清「它到底量的是什么」。

3. **Tools and measurements are sometimes wrong.**
   —— 工具会错，测量会错。**这是常态，不是意外。**

4. **Man pages are not always right.**
   —— man page 也不总对；软件演进了文档没跟上。

5. **An absence of metrics can be more difficult to identify than the presence of poor metrics.**
   —— **缺失比错误更难发现**——因为缺失是沉默的。

6. **The overhead of reading most /proc files is negligible; exceptions include some memory-map related files that walk page tables.**
   —— 「读 /proc 很便宜」有例外：会遍历页表的那些文件不便宜。

7. **The /sys file system typically has tens of thousands of statistics in read-only files**
   —— Linux 的观测面是**几万条**；你缺的不是工具，是**选哪几条**。

8. **Linux systems with the CONFIG_TASK_DELAY_ACCT option track time per task in the following states**
   —— 内核早就能按任务告诉你「卡在调度器 / 块 I/O / 换页 / 内存回收」。

9. **Tracepoints are a stable API**
   —— **稳定 API 才能长出可靠的工具**——这是 tracepoint 相对 kprobe 的核心优势。

10. **event rates of less than 10,000 per second cost negligible overhead, and only over 100,000 does the overhead begin to become measurable.**
    —— 事件率的两个门槛：1 万/秒可忽略，10 万/秒开始可测量。

11. **I’ve previously analyzed overheads for a particular system and found the minimum tracepoint overhead to be 96 nanoseconds of CPU time**
    —— tracepoint 96 ns。

12. **For a particular system I measured the minimum kprobe CPU cost to be 76 nanoseconds, and the minimum kretprobe CPU cost to be 212 nanoseconds**
    —— kprobe 76 ns、kretprobe 212 ns。

13. **For a particular system I measured, the minimum uprobe cost 1,287 nanoseconds, and the minimum uretprobe cost 1,931 nanoseconds**
    —— uprobe 1287 ns、uretprobe 1931 ns——**比 kprobe 贵约 17 倍**。

14. **uprobes currently work by trapping into the kernel.**
    —— uprobe 贵的**根因**是它要陷入内核。

15. **USDT is to uprobes as tracepoints is to kprobes.**
    —— USDT 是「用户态的 tracepoint」；要用它，应用必须**从源码重编**。

16. **An application-level logger may tell you that a database query was slow due to file system I/O, but a tracer can reveal more information**
    —— 日志说「慢在 I/O」，追踪器说「慢在锁竞争」——**这就是追踪相对日志的价值**。

17. **PMCs are a vital resource for performance analysis.**
    —— 指令效率、缓存命中、停顿周期——**只有 PMC 能测**。

18. **Profiling tools, or profilers, often use 99 Hz instead of 100 Hz to avoid sampling in lockstep with target activity**
    —— 99 而不是 100：**避开与被测对象同频**。

19. **Unlike fixed counters, profiling (and tracing) are typically only enabled on an as-needed basis**
    —— 剖析与追踪**按需开启**，因为它们有 CPU 与存储开销。

20. **It is breakpoint-based and can slow the target over one hundred-fold.**
    —— `ptrace(2)` 基于断点，能让目标**慢一百倍**——所以生产环境别用调试器。

21. **System-wide tracing tools such as perf(1) and bpftrace support filters for examining a single process and can operate with much lower overhead**
    —— **能用系统级工具过滤就别用调试器**。

22. **measured timestamps may also be skewed by the tracer.**
    —— 追踪器会**扭曲它测量的时间戳**——用追踪数据算延迟时必须记住。

23. **Remember to use the tools in Figure 4.2 to check for issues with configuration and components. Sometimes performance issues are simply due to a misconfiguration.**
    —— 有时候性能问题就是配置错了；**静态检查比动态排查便宜**。

24. **Even if you do not double-check, it can be valuable to be aware that you didn’t and that you assumed the tools were correct.**
    —— 成熟的态度不是「我验证了每一项」，而是「**我知道我没验证**」。
