---
title: Chapter 4. Observability Tools（精读）
category: 性能工程
tags:
  - 可观测性
  - 性能工具
  - 系统追踪
  - 计数器
  - sar
  - eBPF
difficulty: 进阶
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）第4章 Observability Tools"
source_section: "精读 §4.1 / §4.1.1 / §4.1.2 / §4.2 / §4.2.1 / §4.2.2 / §4.2.3 / §4.2.4 / §4.3 / §4.3.1 / §4.3.2 / §4.3.3 / §4.3.4 / §4.3.5 / §4.3.6 / §4.3.7 / §4.3.8 / §4.3.9 / §4.3.10 / §4.4 / §4.4.1 / §4.4.2 / §4.4.3 / §4.4.4 / §4.5 / §4.6；§4.7 Exercises 与 §4.8 References 不立篇（自测题与文献清单是全书共用的索引性材料，不承载机制陈述）；§4.2 与 §4.3 与 §4.4 下的无编号子标题（System-Wide、Per-Process、Tracepoints Overhead、And More、Solaris Kstat 等）按原文文字引用，不编 § 号"
skills:
  - obsidian-markdown
---

# Chapter 4. Observability Tools

> **小节地图**：本章是全书的**工具箱**——前面讲方法论（[[Chapter 2. Methodologies]]），这里讲「拿什么去量」。
> 立篇：§4.1（工具覆盖图 + 危机工具箱）、§4.2（工具的两种分类轴：系统级/进程级 × 计数器/事件）、
> §4.3（十一个数据源：/proc、/sys、delay accounting、netlink、tracepoints、kprobes、uprobes、USDT、PMC 及其它）、
> §4.4（sar(1) 的采集与归档）、§4.5（五个追踪器的分工）、§4.6（对可观测性本身保持怀疑）。
> **不立篇**：§4.7 Exercises、§4.8 References（自测与文献，不承载机制）。
> 底层依赖：内核与系统调用机制在 [[Chapter 3. Operating Systems]]；PMC 与 IPC 在 [[Chapter 6. CPUs]]；
> 块 I/O 与 tracepoint 的落地在 [[Chapter 9. Disks]]；基准测试方法在 [[Chapter 12. Benchmarking]]；
> 云上 PMC 不可用的后果在 [[Chapter 11. Cloud Computing]]。
> 本篇的实践面在第 5–12 章（各资源章的工具用法）与第 13–15 章（perf / Ftrace / BPF 的深入）里展开。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §4.1.2 危机工具：装包之外还要配内核 | 04-Chapter 4. Observability Tools.md L147 | Adding tool packages is often not enough: kernel and user-space software may also need to be configured to support these tools. Tracing tools typically require certain kernel CONFIG options to be enabled, such as CONFIG_FTRACE and CONFIG_BPF. |
| §4.2 工具类型的两条分类轴 | 04-Chapter 4. Observability Tools.md L153 | A useful categorization for observability tools is whether they provide system-wide or per-process observability, and whether they are based on counters or events |
| §4.2.1 固定计数器：成对累计计数器 | 04-Chapter 4. Observability Tools.md L169 | A common kernel approach is to maintain a pair of cumulative counters: one to count events and the other to record the total time in the event. These provide the count of events directly and the average time (or latency) in the event, by dividing the total time by the count. |
| §4.2.2 Profiling：99 Hz 而非 100 Hz | 04-Chapter 4. Observability Tools.md L217 | Profiling tools, or profilers , often use 99 Hz instead of 100 Hz to avoid sampling in lockstep with target activity, which could lead to over- or undercounting. |
| §4.2.3 Tracing：开销高于 Profiling | 04-Chapter 4. Observability Tools.md L249 | Tracing instruments every occurrence of an event, and can store event-based details for later analysis or produce a summary. This is similar to profiling, but the intent is to collect or inspect all events, not just a sample. Tracing can incur higher CPU and storage overheads than profiling, which can slow the target of tracing. This should be taken into consideration, as it may negatively affect the production workload, and measured timestamps may also be skewed by the tracer. |
| §4.2.4 Monitoring：sar 的采集模型 | 04-Chapter 4. Observability Tools.md L295 | A traditional tool for monitoring a single operating system host is the System Activity Reporter, sar(1), originating from AT&T Unix. sar(1) is counter-based and has an agent that executes at scheduled times (via cron) to record the state of system-wide counters. |
| §4.3.1 /proc：观测者效应 | 04-Chapter 4. Observability Tools.md L475 | top(1) repeats this for all active processes on the system. On some systems, especially those with many processes, the overhead from performing these can become noticeable, especially for versions of top(1) that repeat this sequence for every process on every screen update. This can lead to situations where top(1) reports that top itself is the highest CPU consumer! |
| §4.3.1 /proc：CPU 统计精度取决于内核配置 | 04-Chapter 4. Observability Tools.md L544 | The accuracy of these statistics depends on the kernel configuration. The default configuration (CONFIG_TICK_CPU_ACCOUNTING) measures CPU utilization with a granularity of clock ticks |
| §4.3.3 Delay Accounting：四类等待 | 04-Chapter 4. Observability Tools.md L622 | Linux systems with the CONFIG_TASK_DELAY_ACCT option track time per task in the following states: |
| §4.3.4 netlink：二进制内核接口 | 04-Chapter 4. Observability Tools.md L662 | netlink is a special socket address family (AF_NETLINK) for fetching kernel information. Use of netlink involves opening a networking socket with the AF_NETLINK address family and then using a series of send(2) and recv(2) calls to pass requests and receiving information in binary structs. While this is a more complicated interface to use than /proc, it is more efficient, and also supports notifications. |
| §4.3.5 Tracepoints：硬编码的静态插桩点 | 04-Chapter 4. Observability Tools.md L692 | Tracepoints are hard-coded instrumentation points placed at logical locations in kernel code. |
| §4.3.5 Tracepoints：事件率与开销 | 04-Chapter 4. Observability Tools.md L851 | On typical systems of today (4 to 128 CPUs), I find that event rates of less than 10,000 per second cost negligible overhead, and only over 100,000 does the overhead begin to become measurable. |
| §4.3.6 kprobes：改写指令文本 | 04-Chapter 4. Observability Tools.md L867 | The standard method is to modify the instruction text of running kernel code to insert instrumentation where needed. |
| §4.3.6 kprobes：三种实现的开销量级 | 04-Chapter 4. Observability Tools.md L985 | The kprobes interface is similar to tracepoints. There is a way to instrument them via /sys files, via the perf_event_open(2) syscall (which is preferred), and also via the register_kprobe() kernel API. The overhead is similar to that of tracepoints when the entries to functions are traced (Ftrace method, if available), and higher when function offsets are traced (breakpoint method) or when kretprobes are used (trampoline method). For a particular system I measured the minimum kprobe CPU cost to be 76 nanoseconds, and the minimum kretprobe CPU cost to be 212 nanoseconds |
| §4.3.7 uprobes：陷入内核的开销 | 04-Chapter 4. Observability Tools.md L1038 | uprobes currently work by trapping into the kernel. This costs much higher CPU overheads than kprobes or tracepoints. For a particular system I measured, the minimum uprobe cost 1,287 nanoseconds, and the minimum uretprobe cost 1,931 nanoseconds |
| §4.3.8 USDT：必须编进可执行文件 | 04-Chapter 4. Observability Tools.md L1066 | USDT probes must be compiled into the executable they instrument. This is not possible for JIT-compiled languages such as Java, which usually compiles on the fly. |
| §4.3.9 PMCs：硬件寄存器数量受限 | 04-Chapter 4. Observability Tools.md L1178 | While there are hundreds of PMCs available, there is only a fixed number of registers available in the CPUs to measure them at the same time, perhaps as few as six. |
| §4.3.10 其他数据源：ptrace 的代价 | 04-Chapter 4. Observability Tools.md L1220 | ptrace(2) : This syscall controls process tracing, which is used by gdb(1) for process debugging and strace(1) for tracing syscalls. It is breakpoint-based and can slow the target over one hundred-fold. |
| §4.4 sar：长间隔采集与实时报告 | 04-Chapter 4. Observability Tools.md L1464 | Data collecting is intended for long intervals, such as five or ten minutes, whereas live reporting allows you to look at per-second variation. |
| §4.6 观察可观测性：保持怀疑 | 04-Chapter 4. Observability Tools.md L1516 | Regard with a healthy skepticism any statistics that are new to you, questioning what they really mean and whether they are really correct. |

> **书里没说的**（本篇有几处推论，原文没有，别当成原文观点）：
> - **「开销阶梯」是本篇拼出来的**：原文在 §4.3.5、§4.3.6、§4.3.7 各自报了开销数字（tracepoint 96ns、
>   kprobe 76ns / kretprobe 212ns、uprobe 1287ns / uretprobe 1931ns），但**从未把四者排在一张表里**；
>   本篇按「每次事件的 CPU 成本」把它们并列，是本篇的整理，不是原文的结论。
> - **「先看 §4.2 决定用哪类工具，再看 §4.3 决定从哪个源取数」这个两段式工作流是本篇的读法**：
>   原文 §4.2 与 §4.3 是平铺的两节，没有把它们串成流程。
> - **「观测者效应」这个术语是本篇起的**：原文只描述了 top(1) 自己变成最高 CPU 消费者这个现象，没有命名它。
> - **§4.3.10 里的 Solaris Kstat 是原文用来说明「另一种统计提供方式」的对照**，不是本 Linux 流程里的一环；
>   本篇把它放在「其它源」末尾，不算进主线。
> - **各源的开销数字是「某台特定机器」的实测值**（原文注明 For a particular system），换机器会变；
>   原文只给了数量级，没有给跨机器的保证。

## §4.1 工具覆盖：一张图与一个工具箱

**机制**：作者把 Linux 工作负载的观测工具标注在一张操作系统示意图（原文 Figure 4.1）上，
每个部件旁边写上管它的工具。**多数工具只盯一个资源**（CPU、内存、磁盘），
那些资源在后续各章展开；**能横跨多个区域的多面手工具**是 perf、Ftrace、BCC、bpftrace，
本章后半部分介绍它们。本章用 Ubuntu 做例子，工具大多跨发行版通用。

### §4.1.1 静态性能工具

**机制**：有一类观测**看的是系统「静止时」的属性，而不是负载运行中的行为**——
原文称之为 static performance tuning 方法（见第 2 章 §2.5.17 Static Performance Tuning），
对应 Figure 4.2 的一组工具。

**容易错在哪**：把静态检查当成「不紧急」。原文的判断很直接：

> Remember to use the tools in Figure 4.2 to check for issues with configuration and components. Sometimes performance issues are simply due to a misconfiguration.

**判据**：任何性能问题，**先查配置与组件，再查运行时**。静态检查几乎零开销，且命中率高。

### §4.1.2 危机工具：预装、预配置、预验证

**机制**：生产环境出性能危机时，最糟的情况是**一个工具都没装**；更糟的是**机器正卡着时装包本身就很慢**，
危机被拉长。原文 Table 4.1 列出推荐的安装包/源码仓库（包名按 Ubuntu/Debian，别的发行版会变）：

| 包 | 提供 |
|---|---|
| procps | ps(1), vmstat(8), uptime(1), top(1) |
| util-linux | dmesg(1), lsblk(1), lscpu(1) |
| sysstat | iostat(1), mpstat(1), pidstat(1), sar(1) |
| iproute2 | ip(8), ss(8), nstat(8), tc(8) |
| numactl | numastat(8) |
| linux-tools-common / linux-tools-$(uname -r) | perf(1), turbostat(8) |
| bcc-tools（aka bpfcc-tools） | opensnoop(8), execsnoop(8), runqlat(8), biosnoop(8), biolatency(8), biotop(8), trace(8) 等一百多个 |
| bpftrace | bpftrace，加上 opensnoop/execsnoop/runqlat/biosnoop/biolatency 的基础版 |
| perf-tools-unstable | Ftrace 版的 opensnoop/execsnoop/iolatency/iosnoop/funccount/kprobe |
| trace-cmd | trace-cmd(1) |
| nicstat / ethtool / tiptop / msr-tools | 网络与硬件计数器的补充 |
| msr-cloud-tools / pmc-cloud-tools | showboost(8), cpuhot(8), pmcarch(8), cpucache(8), tlbstat(8), resstalls(8) |

大公司（Netflix）有专门的 OS 与性能团队，保证生产机上这些包**全都装好**；
默认 Linux 发行版通常只有 procps 和 util-linux，其余都要自己补。
容器环境的做法是：**做一个特权调试容器**，拥有系统的完整访问权和全部工具，
镜像装在容器宿主机上，需要时投放。

**但装包不等于能用**——这是本节最承重的一句：

> Adding tool packages is often not enough: kernel and user-space software may also need to be configured to support these tools. Tracing tools typically require certain kernel CONFIG options to be enabled, such as CONFIG_FTRACE and CONFIG_BPF.

配套的第二层依赖原文也点明了：**profiling 工具需要软件支持栈回溯**——
要么全部软件（含 libc、libpthread 等系统库）都用 **frame-pointer 方式编译**，
要么装 **debuginfo 包**以支持 dwarf 栈回溯。

**判据**：危机工具箱要满足三件事，缺一不可——**① 包装齐；② 内核 CONFIG 开着；③ 栈回溯可行**。
原文的建议很直接：**在危机之前把每个工具都跑一遍并修好**，
「等到急需时才发现不能用」是最贵的失败模式。

## §4.2 工具类型：两条分类轴

**机制**：原文给了一条很实用的二维分类法：

> A useful categorization for observability tools is whether they provide system-wide or per-process observability, and whether they are based on counters or events

即 **系统级 vs 进程级** × **计数器 vs 事件**（原文 Figure 4.3）。两条轴是正交的。

**边界情况（原文明确说了）**：有些工具跨象限——
**top(1) 也有系统级汇总**；**系统级的事件工具往往可以按 `-p PID` 过滤到某个进程**。

**事件类工具又分两种**（原文措辞）：
**profiler** 在事件上取一系列快照，画出目标的**粗画像**；
**tracer** 则**对每个关心的事件都插桩**，并可对事件做处理（例如生成定制的计数器）。

原文另有两句限定本篇口径的话：counters / tracing / profiling 三者已在第 1 章讲过；
本章覆盖的工具与数据源，是理解 Linux 可观测性的 essentials，
第 6 到 11 章会用它们解决具体问题，第 13 到 15 章再深入讲追踪器本身。

## §4.2.1 固定计数器

**机制**：内核维护各种计数器，通常是**事件发生时自增的无符号整数**
（收到的网络包数、发出的磁盘 I/O 数、中断次数等），对外暴露成监控指标（§4.2.4）。

原文给出的**通用模式**是**成对的累计计数器**：

> A common kernel approach is to maintain a pair of cumulative counters: one to count events and the other to record the total time in the event. These provide the count of events directly and the average time (or latency) in the event, by dividing the total time by the count.

**这条机制决定了后面所有读数的方式**：两个计数器都是**累计值**，
所以必须**按间隔读两次求差值**，才能得到每秒次数与平均延迟——原文原话是
"by reading the pair at a time interval (e.g., one second) the delta can be calculated"。

**开销**：计数器算「免费」——默认启用、内核持续维护，
唯一的额外成本是从用户态读值（应当可以忽略）。

**工具分布**：
系统级有 vmstat(8)（内存）、mpstat(1)（每 CPU）、iostat(1)（每磁盘，来自块设备接口）、
nstat(8)（TCP/IP 栈统计）、sar(1)（各种统计，还能归档）；
进程级有 ps(1)、top(1)、pmap(1)（进程内存段与用量），它们**通常从 /proc 读**。

**使用惯例**：这类工具大多接受**可选的 interval 和 count** 参数，例如 `vmstat 1 3`：

```bash
$ vmstat 1 3
procs -----------memory---------- ---swap-- -----io---- -system-- ------cpu-----
 r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st
 4  0 1446428 662012 142100 5644676    1    4    28   152   33    1 29  8 63  0  0
 4  0 1446428 665988 142116 5642272    0    0     0   284 4957 4969 51  0 48  0  0
 4  0 1446428 685116 142116 5623676    0    0     0     0 4488 5507 52  0 48  0  0
```

**容易错在哪**：**第一行是自开机以来的汇总**，后面几行才是每秒的区间值。
但原文紧接着提醒：这个 Linux 版本**把两种口径混在了第一行**——
内存列是**当前值**，不是开机以来的平均。所以「首行 = 开机平均」这个惯例**在这台机器上不成立**。

## §4.2.2 Profiling

**机制**：profiling **靠采样/快照刻画目标**。CPU 是最常见的目标：
按固定速率对指令指针或栈采样，刻画吃 CPU 的代码路径。
原文给的参数惯例是 **100 Hz（每秒周期数）跨所有 CPU**，持续**短时间（如一分钟）**。

**为什么是 99 Hz 而不是 100 Hz**——这是本章最容易被忽略的工程细节：

> Profiling tools, or profilers , often use 99 Hz instead of 100 Hz to avoid sampling in lockstep with target activity, which could lead to over- or undercounting.

**理由是「不要与目标同步」**：采样周期和目标自身的活动周期撞上，就会**多算或少算**。

**第二种形态**：也可以基于**无定时器的硬件事件**（CPU 缓存未命中、总线活动）做采样，
这类能指出**是哪条代码路径**造成的，对优化内存使用尤其有用。

**开销与按需开启**：profiling（和 tracing）**通常按需开启**，因为采集有 CPU 开销和存储开销；
量级取决于工具和它插桩的事件率。原文给的判断是：
**定时器型 profiler 更安全**——事件率已知，所以开销可预测，且采样率可选到开销可忽略。

**工具分布**：系统级有 perf(1)（标准）、profile(8)（BCC 的 CPU profiler，在内核上下文里对栈做频率计数）、
Intel VTune Amplifier XE；进程级有 gprof(1)（分析编译器加的 profiling 信息，如 `gcc -pg`）、
cachegrind（分析硬件缓存使用，kcachegrind 可视化）、Java Flight Recorder（语言自带的专用 profiler，能看语言上下文）。

**判据**：profiling 回答「**哪条路径在消耗 CPU/内存**」，
不回答「**这一次请求为什么慢**」——后者要靠 tracing（§4.2.3）。

## §4.2.3 Tracing

**机制**：tracing **对每次事件都插桩**，可以存下事件细节供事后分析，也可以只出汇总：

> Tracing instruments every occurrence of an event, and can store event-based details for later analysis or produce a summary. This is similar to profiling, but the intent is to collect or inspect all events, not just a sample. Tracing can incur higher CPU and storage overheads than profiling, which can slow the target of tracing. This should be taken into consideration, as it may negatively affect the production workload, and measured timestamps may also be skewed by the tracer.

**这段话里有三个独立的机制要点**：
① **「全量 vs 采样」是本质区别**（intent 不一样，不是精度不一样）；
② **开销高于 profiling**，CPU 与存储双开；
③ **tracer 会扭曲自己的测量**——被测目标变慢，且**时间戳本身可能被 tracer 偏移**。
所以 tracing **按需使用**。

**一个有用的等价**：原文指出**日志可以看作「默认开启的低频 tracing」**——
错误和警告这类不频繁的事件写进日志文件供事后读。

**工具分布**：
系统级有 tcpdump(8)（抓包，libpcap）、biosnoop(8)（块 I/O 追踪）、execsnoop(8)（新进程追踪）、
perf(1) 与 perf trace（perf trace 是专门做系统调用级追踪的子命令）、Ftrace（内核自带追踪器）、
BCC（BPF 追踪库与工具集）、bpftrace（BPF 追踪器与工具集）——BCC/bpftrace 生态有**一百多个**追踪工具。
进程级有 strace(1)（系统调用）与 gdb(1)（源码级调试器）。

**生产环境的分水岭**（原文的判词）：**调试器能看逐事件数据，但必须停下再启动目标**，
开销可能极大，**不适合生产**；而**系统级追踪工具支持按进程过滤、开销低得多**，
所以「能用 perf(1)/bpftrace 就别用 gdb」。

**判据**：要「每一个事件」的细节 → tracing；要「热点的分布」→ profiling；
要「多少次、总共多久」→ 计数器。**三者不要混着说**。

## §4.2.4 Monitoring

**机制**：monitoring 与前三类不同——它**持续记录统计量，以防后用**。

**sar(1) 的模型**（本节核心）：

> A traditional tool for monitoring a single operating system host is the System Activity Reporter, sar(1), originating from AT&T Unix. sar(1) is counter-based and has an agent that executes at scheduled times (via cron) to record the state of system-wide counters.

拆开看三个要素：**① 基于计数器**（所以开销低）；**② 有一个 agent**；
**③ agent 按计划（cron）定时执行**，把系统级计数器的状态**落盘归档**。
这就是 sar 能回答「昨天上午十点的 CPU 怎么样」的原因。

sar 能记录**几十种**统计：CPU、内存、磁盘、网络、中断、功耗等（§4.4 详述）。
**第三方监控产品常建在 sar(1) 或它用的同一批统计量之上**，再通过网络暴露出去。

**SNMP**：传统网络监控技术。设备和操作系统可以支持 SNMP，有些默认就提供，省去装 agent。
但原文指出其局限：**SNMP 只含基础 OS 指标，没有扩展到现代应用**，因此大多数环境转向**自定义 agent 式监控**。

**Agents**：现代监控软件在每台系统上跑 agent（也叫 exporter 或 plugin），
记录内核指标**和应用指标**。可以有针对具体应用/目标的 agent（MySQL、Apache、Memcached），
它们能提供**系统计数器里拿不到的应用请求指标**。代表：PCP（PMDA 机制）、
Prometheus（各种 exporter）、collectd（各种 plugin）。

**典型架构**（原文 Figure 4.4）：agent 把指标送到**监控数据库服务器**（如 Graphite Carbon），
再由**监控 web 服务器/仪表盘**（如 Grafana）提供给客户端 UI。

**本节最该记住的一句工程判断**：
有些监控产品是**跑一遍系统工具、解析它的文本输出**来取指标的——**这很没效率**；
**更好的做法是直接用库接口和内核接口读指标**，也就是命令行工具用的同一批接口。
**这正好引出 §4.3**——那一节讲的就是这些内核接口本身。

## §4.3 可观测性数据源

原文 Table 4.2 把「要看什么」映射到「从哪取」（本篇整理成表）：

| 要看的东西 | 数据源 |
|---|---|
| 每进程计数器 | /proc |
| 系统级计数器 | /proc、/sys |
| 设备配置与计数器 | /sys |
| Cgroup 统计 | /sys/fs/cgroup |
| 每进程追踪 | ptrace |
| 硬件计数器（PMC） | perf_event |
| 网络统计 | netlink |
| 网络抓包 | libpcap |
| 每线程延迟指标 | Delay Accounting |
| 系统级追踪 | Ftrace（函数 profiling）、tracepoints、软件事件、kprobes、uprobes、perf_event |

原文 Figure 4.5 画的是这些追踪源及它们的 event/group 名（例如 `block:` 覆盖所有块 I/O tracepoint，
包括 `block:block_rq_issue`）；图中还画了少量 USDT 源：PostgreSQL（`postgres:`）、
JVM hotspot 编译器（`hotspot:`）、libc（`libc:`）——**你自己有什么应用，取决于你的用户态软件**。
tracepoints / kprobes / uprobes 的内部机制，原书《BPF Performance Tools》第 2 章有详细说明。

### §4.3.1 /proc

**机制**：/proc 是**内核统计的文件系统接口**。每个进程 PID 一个目录，
目录里的文件是该进程的信息与统计，**从内核数据结构映射而来**；另有系统级统计文件。
/proc **由内核动态创建、不依托存储设备（跑在内存里）**，**大多只读**（少数可写，用于控制进程与内核行为）。

**接口优势**：它是**约定俗成的目录树 + POSIX 文件调用**（open/read/close），
命令行上可以直接 `cd`、`cat`、`grep`、`awk` 探索；
文件访问权限顺带提供了用户级安全。**极端情况下**（ps、top 之类跑不起来），
仍能用 shell 内建命令从 /proc 目录里做进程调试。

**开销**：**读大多数 /proc 文件的开销可以忽略**；例外是**某些与内存映射相关、需要走页表的文件**。

**每进程可用文件**（Linux 5.4 示例，原文 L415-470）：limits（生效的资源限制）、maps（映射的内存区）、
sched（各种调度器统计）、schedstat（CPU 运行时间、延迟、时间片）、smaps（映射内存区 + 用量统计）、
stat（进程状态与统计，含总 CPU 与内存用量）、statm（以页为单位的内存摘要）、status（stat 与 statm 的带标签版）、
fd（文件描述符符号链接目录）、cgroup（cgroup 成员信息）、task（每线程统计目录）。
**具体文件清单取决于内核版本与 CONFIG 选项**。

**系统级文件**（原文 L477-541）：cpuinfo、diskstats、interrupts（每 CPU 中断计数）、
loadavg、meminfo、net/dev、net/netstat、net/tcp、**pressure/（PSI 压力停顿信息）**、
schedstat、self（当前进程 PID 目录的符号链接）、slabinfo、stat、zoneinfo。

**观测者效应**——本节最值得记住的一句：

> top(1) repeats this for all active processes on the system. On some systems, especially those with many processes, the overhead from performing these can become noticeable, especially for versions of top(1) that repeat this sequence for every process on every screen update. This can lead to situations where top(1) reports that top itself is the highest CPU consumer!

机制很直白：top 对**每个活跃进程**重复「stat → open → read → close」这一串，
**进程一多、刷新一频繁，这个开销就变得显著**，于是出现「top 把自己报成最高 CPU 消费者」的荒诞结果。
（原文附了 top 读 /proc/14704/stat 的 strace 原文，可作对照。）

**CPU 利用率统计的精度**（原文 L542-545）：

> The accuracy of these statistics depends on the kernel configuration. The default configuration (CONFIG_TICK_CPU_ACCOUNTING) measures CPU utilization with a granularity of clock ticks

**默认粒度是时钟节拍**（可能是 4 毫秒，取决于 CONFIG_HZ），原文认为这**一般够用**；
要更高精度有三条路：**VIRT_CPU_ACCOUNTING_NATIVE / VIRT_CPU_ACCOUTING_GEN**（更高分辨率计数器，**有小的性能代价**）、
**IRQ_TIME_ACCOUNTING**（更准的 IRQ 时间）、**用 MSR 或 PMC 测**。

**格式与代价**：/proc 文件**通常是文本格式**，命令行与 shell 处理很方便，
但**内核要把统计编码成文本，用户态工具再解析文本**，两头都有开销。
**netlink 是更高效的二进制接口**（§4.3.4）。

**文档位置**（原文 L566-570）：proc(5) man page、`Documentation/filesystems/proc.txt`；
diskstats 另有 `Documentation/iostats.txt`，调度器统计另有 `Documentation/scheduler/sched-stats.txt`。
**读内核源码**能搞清楚每个字段的确切来源，读消费这些字段的工具源码也有用。
另有三个 CONFIG 依赖：**schedstat 需 CONFIG_SCHEDSTATS、sched 需 CONFIG_SCHED_DEBUG、pressure 需 CONFIG_PSI**。

### §4.3.2 /sys

**机制**：Linux 提供 **sysfs**，挂在 /sys，随 **2.6 内核**引入，
为内核统计提供**基于目录的结构**。原文特别对比了它与 /proc 的不同：
**/proc 是随时间演化出来的**，各种系统统计**大多被加在顶层目录**；
**sysfs 原本是为设备驱动统计设计的**，现已扩展到任意统计类型。

CPU 硬件缓存就是个好例子（原文 L583-600）：

```bash
$ grep . /sys/devices/system/cpu/cpu0/cache/index*/level
/sys/devices/system/cpu/cpu0/cache/index0/level:1
/sys/devices/system/cpu/cpu0/cache/index1/level:1
/sys/devices/system/cpu/cpu0/cache/index2/level:2
/sys/devices/system/cpu/cpu0/cache/index3/level:3
$ grep . /sys/devices/system/cpu/cpu0/cache/index*/size
/sys/devices/system/cpu/cpu0/cache/index0/size:32K
/sys/devices/system/cpu/cpu0/cache/index1/size:32K
/sys/devices/system/cpu/cpu0/cache/index2/size:1024K
/sys/devices/system/cpu/cpu0/cache/index3/size:33792K
```

即 CPU 0 有**两个 32KB 的 L1、一个 1MB 的 L2、一个 33MB 的 L3**。

**规模与可写性**：/sys 通常有**几万个只读统计文件**，**外加大量可写文件**用于改内核状态——
例如把 CPU 上下线就是往名为 `online` 的文件里写 `1` 或 `0`。
原文也指出：**用命令行文本（`echo 1 > filename`）改状态是可行的**，不一定要二进制接口。

### §4.3.3 Delay Accounting

**机制**：开了 **CONFIG_TASK_DELAY_ACCT** 的 Linux 系统会**按任务统计四类等待时间**：

> Linux systems with the CONFIG_TASK_DELAY_ACCT option track time per task in the following states:

- **Scheduler latency**：等 CPU 的时间
- **Block I/O**：等块 I/O 完成的时间
- **Swapping**：等换页的时间（内存压力）
- **Memory reclaim**：等内存回收例程的时间

**一个容易忽略的技术细节**：**调度器延迟统计实际来自 schedstats**（前面 /proc 提到的那个），
**但被和其他延迟状态一起暴露出来**；它住在 `struct sched_info` 里，**不是** `struct task_delay_info`。

**读取接口**：**taskstats**——一个**基于 netlink** 的接口，用于取每任务/每进程统计。
内核源码里对应两处：`Documentation/accounting/delay-accounting.txt`（文档）与
`tools/accounting/getdelays.c`（示例消费者）。示例输出（原文 L645-656）：

```bash
$ ./getdelays -dp 17451
print delayacct stats ON
PID    17451

CPU             count     real total  virtual total    delay total  delay average
                  386     3452475144    31387115236     1253300657          3.247ms
IO              count    delay total  delay average
                  302     1535758266              5ms
SWAP            count    delay total  delay average
                    0              0              0ms
RECLAIM         count    delay total  delay average
                    0              0              0ms
```

**时间单位是纳秒**（除非另有说明）。原文对这个例子的解读是：
该系统 CPU 负载很重，被检查的进程**正在遭受调度延迟**（平均 3.247ms 的调度延迟）。

**判据**：**「进程卡住」必须先拆成「等 CPU」与「等 I/O」**——delay accounting 是少数能直接做这个拆分的源。
只报总延迟、不拆等待原因，等于没诊断。

### §4.3.4 netlink

**机制**：netlink 是一个**特殊的 socket 地址族（AF_NETLINK）**，用来取内核信息：

> netlink is a special socket address family (AF_NETLINK) for fetching kernel information. Use of netlink involves opening a networking socket with the AF_NETLINK address family and then using a series of send(2) and recv(2) calls to pass requests and receiving information in binary structs. While this is a more complicated interface to use than /proc, it is more efficient, and also supports notifications.

三个要点：**① 二进制结构**（不是文本，这是它比 /proc 快的原因）；
**② 支持通知**（不只是拉取，还能被内核主动推送）；**③ 用起来比 /proc 复杂**。
**libnetlink** 库降低了使用门槛。

**netlink 组**：NETLINK_ROUTE（路由信息，/proc/net/route 也有）、NETLINK_SOCK_DIAG（socket 信息）、
NETLINK_SELINUX（SELinux 事件通知）、NETLINK_AUDIT（审计）、NETLINK_SCSITRANSPORT、NETLINK_CRYPTO。

**哪些命令在用它**：ip(8)、ss(8)、routel(8)，以及老的 ifconfig(8) 与 netstat(8)。
用 strace(1) 能直接看到这条路径（原文 L669-672）：

```bash
# strace ss
[...]
socket(AF_NETLINK, SOCK_RAW|SOCK_CLOEXEC, NETLINK_SOCK_DIAG) = 3
[...]
```

### §4.3.5 Tracepoints

**机制**：tracepoints 是**基于静态插桩（static instrumentation，第 1 章 §1.7.3）的 Linux 内核事件源**：

> Tracepoints are hard-coded instrumentation points placed at logical locations in kernel code.

**位置是「逻辑上该有的地方」**：系统调用的起止、调度事件、文件系统操作、磁盘 I/O 都有 tracepoint。
由 Mathieu Desnoyers 开发，**2009 年随 Linux 2.6.32 首次发布**。
原文的定级是：**稳定 API，但数量有限**（作者脚注自称 "best-effort stable"，并承认见过 tracepoint 变化）。

**为什么重要**：tracepoint 支撑着那些**超越汇总统计**的高级追踪工具。
原文的对比很关键：**函数级追踪（kprobes）能提供相近的能力，但只有 tracepoints 提供稳定接口**，
因此才能据此开发出**健壮**的工具。

**术语坑（原文专门澄清过，值得记）**：
严格说 **tracepoint 是放在内核源码里的追踪函数（tracing hooks）**——
例如 `trace_sched_wakeup()`，在 `kernel/sched/core.c` 里被调用。
这个 tracepoint 可以被追踪器以 `sched:sched_wakeup` 这个名字插桩，
**但那严格说是一个 trace event**，由 **TRACE_EVENT 宏**定义。
TRACE_EVENT 同时**定义并格式化其参数、自动生成 `trace_sched_wakeup()` 代码、
并把该 trace event 放进 tracefs 与 perf_event_open(2) 接口**。
追踪工具主要插桩的是 trace event，却常把它们叫「tracepoints」；
**perf(1) 把 trace event 叫 "Tracepoint event"，这非常误导——
因为 kprobe 与 uprobe 派生的 trace event 也被标成 "Tracepoint event"。**

**format string**：每个 tracepoint 有一个格式串，内含**事件参数**（关于该事件的额外上下文），
结构在 `/sys/kernel/debug/tracing/events` 下的 `format` 文件里（原文 L755-774）：

```bash
# cat /sys/kernel/debug/tracing/events/block/block_rq_issue/format
name: block_rq_issue
ID: 1080
format:
        field:unsigned short common_type;  offset:0;  size:2;  signed:0;
        field:unsigned char common_flags;  offset:2;  size:1;  signed:0;
        field:unsigned char common_preempt_count;  offset:3;  size:1;  signed:0;
        field:int common_pid;   offset:4;  size:4;  signed:1;

        field:dev_t dev;        offset:8;  size:4;  signed:0;
        field:sector_t sector;  offset:16;  size:8;  signed:0;
        field:unsigned int nr_sector;  offset:24;  size:4;  signed:0;
        field:unsigned int bytes;      offset:28;  size:4;  signed:0;
        field:char rwbs[8];   offset:32;  size:8;  signed:1;
        field:char comm[16];  offset:40;  size:16;  signed:1;
        field:__data_loc char[] cmd;  offset:56;  size:4;  signed:1;

print fmt: "%d,%d %s %u (%s) %llu + %u [%s]", ((unsigned int) ((REC->dev) >> 20)),
((unsigned int) ((REC->dev) & ((1U << 20) - 1))), REC->rwbs, REC->bytes,
__get_str(cmd), (unsigned long long)REC->sector, REC->nr_sector, REC->comm
```

**最后一行就是那个格式串与它的参数列表**，前面一行行的是各字段的偏移与大小——
这解释了工具输出里每一列是怎么来的。**追踪器通常能按名字访问这些参数**：
perf(1) 用 `--filter 'bytes > 65536'` 按参数过滤（**该参数是 Linux 5.5 才加的**），
bpftrace 则用 `args->bytes` 直接取。

**接口两条**：**tracefs 文件**（通常挂在 /sys/kernel/debug/tracing）
或 **perf_event_open(2) 系统调用**。原文的对比很实际：
**tracefs 不便支持并发使用者**，所以作者的 Ftrace 版 iosnoop 得自己加一个
`/var/tmp/.ftrace-lock` 来挡并发；而 **perf_event_open(2) 支持并发使用者，应当优先用它**
（作者的 BCC 版 biosnoop 就是这么做的）。

**开销——本节最该背下来的数字**：

> On typical systems of today (4 to 128 CPUs), I find that event rates of less than 10,000 per second cost negligible overhead, and only over 100,000 does the overhead begin to become measurable.

**事件率低于每秒一万，开销可忽略；超过每秒十万，开销才开始可测。**
举例：磁盘事件通常每秒不到一万，**但调度事件经常远超每秒十万，因此追踪起来很贵**。

**还有「禁用态开销」**：禁用的 tracepoint 只是一小段指令——
**x86_64 上是 5 字节的 nop**；另外函数末尾还挂了个 tracepoint handler，会让函数文本略微变大。
原文自己的实测：**最小 tracepoint 开销是 96 纳秒 CPU 时间**；
Linux 4.7（2018）新增的 **raw tracepoints** 避免了构造稳定 tracepoint 参数的成本，进一步降低开销。

**判据**：**要稳定、要能长期依赖、要写工具 → tracepoint**；
**没有对应 tracepoint 又必须看 → 才考虑 kprobes**。

### §4.3.6 kprobes

**机制**：kprobes 是**基于动态插桩**（第 1 章 §1.7.3）的内核事件源，
**能追踪任何内核函数或指令**，**2004 年随 Linux 2.6.9 出现**。
原文给它的定性是**不稳定 API**——因为它暴露的是**原始内核函数与参数，跨版本可能变**。

**内部实现**：标准做法是**改写运行中内核代码的指令文本**，在需要处插入插桩：

> The standard method is to modify the instruction text of running kernel code to insert instrumentation where needed.

**一个优化**：给**函数入口**插桩时，kprobes 可以**借用已有的 Ftrace 函数追踪**（开销更低），
可用 `debug.kprobes-optimization` 这个 sysctl 开关。

**定位**：原文说得很重——kprobes 是**「最后手段」**，是关于生产环境内核行为的**近乎无限的信息来源**，
对观察其它工具看不见的性能问题可能至关重要。
**没有 kprobes 的话，最后手段就是改内核源码加插桩、重编译、重新部署。**

**与 tracepoints 的对比**（原文 Table 4.3）：

| 细节 | kprobes | Tracepoints |
|---|---|---|
| 类型 | 动态 | 静态 |
| 粗略事件数 | 50,000+ | 1,000+ |
| 内核维护 | 无 | 需要 |
| 禁用态开销 | 无 | 极小（NOP + 元数据） |
| 稳定 API | 否 | 是 |

**kprobe 事件只在追踪器创建它们时存在**，默认情况下内核代码照常运行。

**kretprobes**：追踪内核函数的返回与返回值，做法是**在函数入口挂一个 kprobe，
由它插入一个 trampoline 函数来插桩返回点**。与 kprobes 配对、并由追踪器记录时间戳，
就能**测出内核函数的耗时**。原文的 bpftrace 例子用直方图呈现 do_nanosleep() 的耗时分布
（原文 L959-977）：**1280 次落在 0 毫秒桶**，另有 2 次落在 512–1024 毫秒桶。

**接口与开销量级**（本节最硬的数字）：

> The kprobes interface is similar to tracepoints. There is a way to instrument them via /sys files, via the perf_event_open(2) syscall (which is preferred), and also via the register_kprobe() kernel API. The overhead is similar to that of tracepoints when the entries to functions are traced (Ftrace method, if available), and higher when function offsets are traced (breakpoint method) or when kretprobes are used (trampoline method). For a particular system I measured the minimum kprobe CPU cost to be 76 nanoseconds, and the minimum kretprobe CPU cost to be 212 nanoseconds

**三条路径**：/sys 文件、**perf_event_open(2)（首选）**、register_kprobe() 内核 API。
**三种实现方式的开销递增**：**函数入口（Ftrace 法）≈ tracepoint**；
**函数内偏移（breakpoint 法）更高**；**kretprobe（trampoline 法）最高**。
实测**最小 kprobe 76 纳秒、最小 kretprobe 212 纳秒**。

**用法示例**（原文 L923-929）：

```bash
# bpftrace -e 'kprobe:do_nanosleep { printf("sleep by: %s\n", comm); }'
Attaching 1 probe...
sleep by: mysqld
sleep by: mysqld
sleep by: sleep
^C
```

**kprobe 事件的创建与销毁是随 bpftrace 程序的起止而定的**（Ctrl-C 即移除）。

### §4.3.7 uprobes

**机制**：uprobes 是 **kprobes 的用户态版本**——**动态插桩应用与库里的函数**，
提供**不稳定 API**，让你能钻进其它工具够不着的软件内部。
**2012 年随 Linux 3.5 出现**。

**行为上与调试器加断点一致**：uprobes **按需创建 uprobe 事件**，**默认用户态代码照常运行**——
就像「加断点之前，函数是未经改动的」。

**uretprobes**：追踪用户态函数的返回与返回值，与 uprobes 类似。
原文的警告是：**uretprobes 的开销会显著扭曲对快函数的测量**。

**开销**（与 kprobes 差一个数量级）：

> uprobes currently work by trapping into the kernel. This costs much higher CPU overheads than kprobes or tracepoints. For a particular system I measured, the minimum uprobe cost 1,287 nanoseconds, and the minimum uretprobe cost 1,931 nanoseconds

**为什么贵**：**uprobes 靠陷入内核实现**（而 kprobes 是改写指令、在内核里就地执行）。
**uretprobe 更贵，因为它是一个 uprobe 加一个 trampoline**。

**接口**：与 kprobes 类似——/sys 文件，以及 **perf_event_open(2)（首选）**。

**用法示例**（原文 L997-1013）：`bpftrace -l 'uprobe:/bin/bash:*'` 列出 bash 里所有可插桩的函数入口，
作者的机器上有 **1507 个**候选。

**判据**：**能用 uprobe 就别上 uretprobe**（尤其测快函数）；
**能用 USDT 就别用 uprobe**（USDT 是稳定且有文档的，见下节）。

### §4.3.8 USDT

**机制**：USDT（User-level statically-defined tracing）是 **tracepoints 的用户态版本**。
原文用一句对称的话把它钉死：**「USDT 之于 uprobes，正如 tracepoints 之于 kprobes。」**

**它是「某些应用和库自己加的探针」**：提供**稳定且有文档**的应用级事件追踪 API。
**Java JDK、PostgreSQL 数据库、libc 里都有 USDT 探针**（原文 L1046-1057）：

```bash
# bpftrace -lv 'usdt:/usr/lib/jvm/openjdk/libjvm.so:*'
usdt:/usr/lib/jvm/openjdk/libjvm.so:hotspot:class__loaded
usdt:/usr/lib/jvm/openjdk/libjvm.so:hotspot:class__unloaded
usdt:/usr/lib/jvm/openjdk/libjvm.so:hotspot:method__compile__begin
usdt:/usr/lib/jvm/openjdk/libjvm.so:hotspot:method__compile__end
usdt:/usr/lib/jvm/openjdk/libjvm.so:hotspot:gc__begin
usdt:/usr/lib/jvm/openjdk/libjvm.so:hotspot:gc__end
```

这个 JDK 版本总共 **524 个** USDT 探针，覆盖类加载/卸载、方法编译、GC。

**USDT 与普通应用日志的差别，是本节的核心**（原文 L1052-1057）：
**很多应用已经有自己的事件日志，但 USDT 探针能被各种追踪器使用，
从而把应用上下文与内核事件（磁盘、网络 I/O）结合起来。**
原文给的对比极锋利：**应用级日志可能告诉你「数据库查询慢是文件系统 I/O 慢」，
而追踪器能揭示更多——查询慢其实是文件系统里的锁竞争，而不是你以为的磁盘 I/O。**

**限制一：必须编进可执行文件**——这是 USDT 最实际的门槛：

> USDT probes must be compiled into the executable they instrument. This is not possible for JIT-compiled languages such as Java, which usually compiles on the fly.

而且**打包版本往往没开**：OpenJDK 就是这样，需要**用相应配置选项从源码重编**，
那个选项可能叫 `--enable-dtrace-probes`（沿 DTrace 的名字）。

**限制二的解法：dynamic USDT**——**把探针预编译成一个共享库，
并提供接口让 JIT 语言去调用它**。Java、Node.js 等都有 dynamic USDT 库；
**解释型语言有同样的问题，也需要 dynamic USDT**。

**实现**：Linux 上 **USDT 探针是用 uprobes 实现的**（开销见 §4.3.7）；
此外**像 tracepoints 一样，USDT 探针会在代码里放 nop 指令**。

### §4.3.9 硬件计数器（PMCs）

**机制**：PMC（performance monitoring counters）是**处理器上的可编程硬件寄存器**，
提供 **CPU 周期级**的底层性能信息。别名一堆：CPCs、PICs、PMU events，**指的都是同一个东西**。

**为什么不可替代**（原文 L1080-1082）：**只有通过 PMC 才能测量 CPU 指令的执行效率、
CPU 缓存命中率、内存与设备总线的利用率、互连利用率、停顿周期等等**，
用它们分析性能能导向具体的优化。

**Intel 架构事件集**：虽然 PMC 很多，Intel 选了**七个**作为「架构集」，
提供核心功能的高层概览，**可以用 `cpuid` 指令检查它们是否存在**（原文 Table 4.4）：

| 事件名 | UMask | Event Select | 示例事件掩码助记符 |
|---|---|---|---|
| UnHalted Core Cycles | 00H | 3CH | CPU_CLK_UNHALTED.THREAD_P |
| Instruction Retired | 00H | C0H | INST_RETIRED.ANY_P |
| UnHalted Reference Cycles | 01H | 3CH | CPU_CLK_THREAD_UNHALTED.REF_XCLK |
| LLC References | 4FH | 2EH | LONGEST_LAT_CACHE.REFERENCE |
| LLC Misses | 41H | 2EH | LONGEST_LAT_CACHE.MISS |
| Branch Instruction Retired | 00H | C4H | BR_INST_RETIRED.ALL_BRANCHES |
| Branch Misses Retired | 00H | C5H | BR_MISP_RETIRED.ALL_BRANCHES |

**用法**：`perf stat` 不指定事件（无 `-e`）时，**默认就采这七个架构 PMC**（原文 L1157-1170）：

```bash
# perf stat gzip words

 Performance counter stats for 'gzip words':

        156.927428      task-clock (msec)         #    0.987 CPUs utilized
                 1      context-switches          #    0.006 K/sec
                 0      cpu-migrations            #    0.000 K/sec
               131      page-faults               #    0.835 K/sec
       209,911,358      cycles                    #    1.338 GHz
       288,321,441      instructions              #    1.37  insn per cycle
        66,240,624      branches                  #  422.110 M/sec
         1,382,627      branch-misses             #    2.09% of all branches

       0.159065542 seconds time elapsed
```

**关键指标是 `insn per cycle`（IPC，每周期指令数）**——它衡量 CPU 执行指令的效率，越高越好。

**接口**：Linux 上 PMC 通过 **perf_event_open(2)** 访问，perf(1) 是主要消费者。

**硬件约束——这一条决定了 PMC 的用法**：

> While there are hundreds of PMCs available, there is only a fixed number of registers available in the CPUs to measure them at the same time, perhaps as few as six.

**可用 PMC 成百上千，但能同时测的寄存器数量固定，最少可能只有六个。**
所以只能**挑六个想测的**，或者**轮换不同的 PMC 组合**来采样（perf(1) 自动支持轮换）。
**其它软件计数器不受这个约束**。

**两种模式**：**counting**（数事件，**开销实际上为零**）与
**overflow sampling**（每 N 个事件触发一次中断以捕获状态）。
**counting 用来量化问题，overflow sampling 用来指出是哪条代码路径 responsible**。
perf(1) 用 **stat 子命令做 counting**、**record 子命令做 sampling**。

**挑战一：采样归属会错**（原文 L1188）：

> Overflow sampling may not record the correct instruction pointer that triggered the event, due to interrupt latency (often called “skid”) or out-of-order instruction execution.

**中断延迟造成的「skid」或乱序执行**，会让采样记到错误的指令指针上。
**测 CPU 周期时这通常无害**（有些 profiler 还故意引入抖动、或用 99 Hz 这样的偏移采样率来避免同频）；
**但测 LLC miss 这类事件时，采样到的指令指针必须准**。
解法是处理器的 **precise event** 支持：**Intel 用 PEBS**（precise event-based sampling，
用硬件缓冲记录更准的指令指针），**AMD 用 IBS**；Linux perf(1) 支持它们。

**挑战二：云上拿不到**（原文 L1194-1198）：
**很多云环境对来宾禁用了 PMC 访问**。技术上可以开：
**Xen hypervisor 有 `vpmu` 命令行选项**可以按不同集合把 PMC 暴露给来宾
（作者的代码允许 "ipc" 与 "arch" 两种模式，本质上是给 Xen 现有 vpmu 支持加了层防火墙）；
**Amazon 为 Nitro hypervisor 来宾开放了很多 PMC**；也有云厂商提供**裸金属实例**，来宾独占处理器，PMC 全开放。

**文档**：PMC 是**处理器专属**的，查对应厂商手册（Intel SDM Vol.3B 第 19 章、
AMD Family 17h 手册 §2.1.1、Armv8-A 手册 D7.10）。
跨厂商统一命名的努力是 **PAPI**，但**操作系统对它的支持参差**——PAPI 需要频繁更新以把名字映射到厂商编码。

### §4.3.10 其它数据源

原文列的清单（本篇按「用途」重排，条目与原文 L1214-1240 一致）：

| 源 | 机制与用途 |
|---|---|
| **MSRs** | PMC 就是用型号专用寄存器实现的；另有 MSR 显示系统配置与健康状况：**CPU 时钟频率、使用率、温度、功耗**。可用哪些取决于**处理器型号、BIOS 版本与设置、hypervisor 设置**。用途之一是**精确的按周期测 CPU 利用率** |
| **ptrace(2)** | 控制进程追踪，gdb(1) 与 strace(1) 靠它。**基于断点，可能让目标慢一百多倍** |
| **Function profiling** | `mcount()` 或 `__fentry__()` 被加在 x86 上**所有未内联内核函数的入口**，供 Ftrace 高效做函数追踪；**不用时它们被转成 nop 指令** |
| **Network sniffing (libpcap)** | 从网络设备抓包做协议与性能深挖；Linux 上经 libpcap 与 /proc/net/dev，由 tcpdump(8) 消费；**抓全部包有 CPU 与存储开销** |
| **netfilter conntrack** | netfilter 技术允许**在事件上执行自定义处理器**，不止防火墙，还做连接跟踪（conntrack），**可生成网络流日志** |
| **Process accounting** | 主机时代为「给部门计费」而生的机制；Linux 上的 `atop(1)` 用它**捕捉那些短命进程**——这些进程在 /proc 快照里会被漏掉 |
| **Software events** | 与硬件事件相对、由软件实现的事件，**缺页是一个例子**；经 **perf_event_open(2)** 提供，perf(1) 与 bpftrace 都能用 |
| **System calls** | 有些系统调用直接给指标：`getrusage(2)` 给进程自己的资源用量，含**用户态与内核态时间、缺页、消息数、上下文切换数** |

**ptrace 的代价值得单独强调**（它常被当成 strace 的默认选项）：

> ptrace(2) : This syscall controls process tracing, which is used by gdb(1) for process debugging and strace(1) for tracing syscalls. It is breakpoint-based and can slow the target over one hundred-fold. Linux also has tracepoints, introduced in Section 4.3.5 , Tracepoints , for more efficient syscall tracing.

**慢一百多倍**，而且原文直接给出替代路径：**用 tracepoints 做更高效的系统调用追踪**。

**「还有更多」**（原文 L1236-1241）：取决于内核版本与启用的选项，还可能有
I/O accounting、blktrace、timer_stats、lockstat、debugfs。
**找法原文给了一条很实用的建议**：**去读你想观测的那块内核源码，
看里面放了哪些统计或 tracepoint。**

**没有统计可用时**（原文 L1243）：除了动态插桩（kprobes/uprobes），
**gdb(1)、lldb(1) 这类调试器能取内核与应用的变量**来照亮调查。

**对照：Solaris Kstat**（原文 L1244-1275）——**另一种提供系统统计的方式**：
Solaris 用 Kstat 框架给出**一致的分层结构**，每个统计项用**四元组**命名：

```
module:instance:name:statistic
```

- **module**：通常是创建该统计的内核模块，如 SCSI 磁盘驱动 `sd`、ZFS 文件系统 `zfs`
- **instance**：模块的多个实例，如每块 SCSI 磁盘一个 `sd` 实例
- **name**：统计组的名字
- **statistic**：单个统计项的名字

kstats 通过**二进制内核接口**访问，各有各的库。示例（原文 L1273-1275）：
`kstat -p unix:0:system_misc:nproc` 输出 `unix:0:system_misc:nproc        94`，即当前运行进程数。

**与 Linux 的对比（原文 L1275）**：

> In comparison, the /proc/stat-style sources on Linux have inconsistent formatting and usually require text parsing to process, costing some CPU cycles.

**Linux 的 /proc/stat 类源格式不一致，通常要文本解析，耗 CPU 周期**——这正是 netlink 存在的理由（§4.3.4）。

## §4.4 sar(1)

**定位**（原文 L1279、L1281）：

> sar(1) was introduced in Section 4.2.4 , Monitoring , as a key monitoring facility. While there has been much excitement recently with BPF tracing superpowers (and I’m partly responsible), you should not overlook the utility of sar(1)—it’s an essential systems performance tool that can solve many performance issues on its own. The Linux version of sar(1) is also well-designed, having self-descriptive column headings, network metric groups, and detailed documentation (man pages).

> sar(1) is provided via the sysstat package.

作者给 sar(1) 的分量很高，而且**自己交代了立场**（「我本人对 BPF 的兴奋也有份」）——新工具越热，越需要旧工具兜底。
Linux 版 sar(1) 的三个设计优点：**列名自解释**、**按网络指标分组**、**man page 详尽**。

### §4.4.1 sar(1) 覆盖面

> Figure 4.6 shows the observability coverage from the different sar(1) command line options.

> This figure shows that sar(1) provides broad coverage of the kernel and devices, and even has observability for fans. The -m (power management) option also supports other arguments not shown in this figure, including IN for voltage inputs, TEMP for device temperatures, and USB for USB device power statistics.

**覆盖面 = 内核 + 设备 + 硬件传感器**（连风扇都有观测点）。
`-m`（power management）除图上所画外还有三个参数：`IN`（电压输入）、`TEMP`（设备温度）、`USB`（USB 设备功耗统计）。
原文 Figure 4.6 是一张「选项 → 覆盖哪些子系统」的对照图，本篇不复述图内逐项细节。

### §4.4.2 sar(1) 采集与归档

**第一步：确认采集是否已启用**（无参数跑一次 sar）：

> You may find that sar(1) data collecting (monitoring) is already enabled for your Linux systems. If it isn’t, you need to enable it. To check, simply run sar without options. For example:

```bash
$ sar
Cannot open /var/log/sysstat/sa19: No such file or directory
Please check if data collecting is enabled
```

> The output shows that sar(1) data collecting is not yet enabled on this system (the sa19 file refers to the daily archive for the 19th of the month). The steps to enable it may vary based on your distribution.

**读法**：`sa19` 是**当月 19 日的归档文件**，报「打不开」就说明**归档根本没在写**。
启用步骤各发行版不同，原文给的是 Ubuntu 的路径。

**Ubuntu 启用步骤**：

> On this Ubuntu system, I can enable sar(1) data collecting by editing the /etc/default/sysstat file and setting ENABLED to be true:

```bash
ubuntu# vi /etc/default/sysstat
#
# Default settings for /etc/init.d/sysstat, /etc/cron.d/sysstat
# and /etc/cron.daily/sysstat files
#

# Should sadc collect system activity informations? Valid values
# are "true" and "false". Please do not put other values, they
# will be overwritten by debconf!
ENABLED="true"
```

改完重启：

```bash
ubuntu# service sysstat restart
```

**采集节奏**由 sysstat 的 crontab 决定：

```bash
ubuntu# cat /etc/cron.d/sysstat
# The first element of the path is a directory where the debian-sa1
# script is located
PATH=/usr/lib/sysstat:/usr/sbin:/usr/sbin:/usr/bin:/sbin:/bin

# Activity reports every 10 minutes everyday
5-55/10 * * * * root command -v debian-sa1 > /dev/null && debian-sa1 1 1

# Additional run at 23:59 to rotate the statistics file
59 23 * * * root command -v debian-sa1 > /dev/null && debian-sa1 60 2
```

> The syntax 5-55/10 means it will record every 10 minutes for the minute range 5 to 55 minutes past the hour. You can adjust this to suit the resolution desired: the syntax is documented in the crontab(5) man page. More frequent data collection will increase the size of the sar(1) archive files, which can be found in /var/log/sysstat.

**这里有个容易看漏的细节**：默认是「**每小时第 5 到 55 分钟之间的 10 个整点**」，
即 5-55/10 展开是 5、15、25、35、45、55 这些分钟；**每小时头 5 分钟没有数据**，
而且 **23:59 那一趟是轮转归档文件**（`debian-sa1 60 2`）——不是普通采样。
**频率越高，/var/log/sysstat 的归档越大**。

作者自己的改法（每 5 分钟、记全部统计组）：

```bash
*/5 * * * * root command -v debian-sa1 > /dev/null && debian-sa1 1 1 -S ALL
```

> The */5 will record every five minutes, and the -S ALL will record all statistics. By default sar(1) will record most (but not all) statistic groups. The -S ALL option is used to record all statistic groups—it is passed to sadc(1), and documented in the man page for sadc(1). There is also an extended version, -S XALL , which records additional breakdowns of statistics.

**默认记的是「大部分」统计组，不是全部**；`-S ALL` 补齐全部，`-S XALL` 再加**更细的拆分**。
两者都是**传给 sadc(1) 的**参数（在采集端生效，不是在报告端）。

**报告**（原文 L1360、L1381、L1383）：

> sar(1) can be executed with any of the options shown in Figure 4.6 to report the selected statistic group. Multiple options can be specified. For example, the following reports CPU statistics (- u ), TCP ( -n TCP ), and TCP errors ( -n ETCP ):

```bash
$ sar -u -n TCP,ETCP
Linux 4.15.0-66-generic (bgregg)  01/19/2020         _x86_64_        (8 CPU)

10:40:01 AM     CPU     %user     %nice   %system   %iowait    %steal     %idle
10:45:01 AM     all      6.87      0.00      2.84      0.18      0.00     90.12
10:50:01 AM     all      6.87      0.00      2.49      0.06      0.00     90.58
[...]
10:40:01 AM  active/s passive/s    iseg/s    oseg/s
10:45:01 AM      0.16      0.00     10.98      9.27
10:50:01 AM      0.20      0.00     10.40      8.93
[...]
10:40:01 AM  atmptf/s  estres/s retrans/s isegerr/s   orsts/s
10:45:01 AM      0.04      0.02      0.46      0.00      0.03
10:50:01 AM      0.03      0.02      0.53      0.00      0.03
[...]
```

> The first line of output is a system summary, showing the kernel type and version, the hostname, the date, processor architecture, and number of CPUs.

> Running sar -A will dump all statistics.

**多个选项可以并列**（`-u` 是 CPU、`-n TCP` 是 TCP、`-n ETCP` 是 TCP 错误），
所以**一条命令可以同时取多组指标**，不必反复执行。
**第一行是系统摘要**（内核版本、主机名、日期、架构、CPU 数）——归档文件脱离原机器后，这一行是唯一的身份标识。

**输出格式**：sysstat 另带一个 `sadf(1)`，把 sar 统计转成别的格式。

> The sysstat package comes with an sadf(1) command for viewing sar(1) statistics in different formats, including JSON, SVG, and CSV. The following examples emit the TCP ( -n TCP ) statistics in these formats.

**JSON**（原文 L1391、L1417）：

> JavaScript Object Notation (JSON) can be easily parsed and imported by many programming languages, making it a suitable output format when building other software upon sar(1).

> You can process the JSON output at the command line using the jq(1) tool.

```bash
$ sadf -j -- -n TCP
{"sysstat": {
  "hosts": [
    {
      "nodename": "bgregg",
      "sysname": "Linux",
      "release": "4.15.0-66-generic",
      "machine": "x86_64",
      "number-of-cpus": 8,
      "file-date": "2020-01-19",
      "file-utc-time": "18:40:01",
      "statistics": [
        {
          "timestamp": {"date": "2020-01-19", "time": "18:45:01", "utc": 1,
"interval": 300},
          "network": {
            "net-tcp": {"active": 0.16, "passive": 0.00, "iseg": 10.98, "oseg": 9.27}
          }
        },
[...]
```

**SVG**（原文 L1421）：

> sadf(1) can emit Scalable Vector Graphics (SVG) files that can be viewed in web browsers. Figure 4.7 shows an example. You can use this output format to build rudimentary dashboards.

**CSV**（原文 L1431）：

> The comma-separated values (CSV) format is intended for import by databases (and uses a semicolon):

```bash
$ sadf -d -- -n TCP
# hostname;interval;timestamp;active/s;passive/s;iseg/s;oseg/s
bgregg;300;2020-01-19 18:45:01 UTC;0.16;0.00;10.98;9.27
bgregg;299;2020-01-19 18:50:01 UTC;0.20;0.00;10.40;8.93
bgregg;300;2020-01-19 18:55:01 UTC;0.12;0.00;9.27;8.07
[...]
```

⚠️ **一个纯工程的坑**：CSV 叫 CSV，实际分隔符是**分号**，不是逗号。
按逗号去 split 导入数据库，会得到**整行一列**的假表。

### §4.4.3 sar(1) 实时模式

> When executed with an interval and optional count, sar(1) does live reporting. This mode can be used even when data collection is not enabled.

```bash
$ sar -n TCP 1 5
Linux 4.15.0-66-generic (bgregg)  01/19/2020         _x86_64_        (8 CPU)

03:09:04 PM  active/s passive/s    iseg/s    oseg/s
03:09:05 PM      1.00      0.00     33.00     42.00
03:09:06 PM      0.00      0.00    109.00     86.00
03:09:07 PM      0.00      0.00    107.00     67.00
03:09:08 PM      0.00      0.00    104.00    119.00
03:09:09 PM      0.00      0.00     70.00     70.00
Average:         0.20      0.00     84.60     76.80
```

> Data collecting is intended for long intervals, such as five or ten minutes, whereas live reporting allows you to look at per-second variation.

**关键**：**实时模式不需要先启用采集**（它现场读计数器，不读归档）。
**归档与实时是两套时间分辨率**：归档面向 5–10 分钟的长间隔，实时面向**逐秒波动**。
末尾的 `Average:` 行是这 5 次的均值——**注意它是均值不是总量**，均值会把尖峰抹平。

### §4.4.4 sar(1) 文档

> The sar(1) man page documents the individual statistics and includes SNMP names in square brackets. For example:

```bash
$ man sar
[...]
              active/s
                     The number of times TCP connections have  made  a  direct
                     transition  to  the  SYN-SENT state from the CLOSED state
                     per second [tcpActiveOpens].

              passive/s
                     The number of times TCP connections have  made  a  direct
                     transition  to  the  SYN-RCVD state from the LISTEN state
                     per second [tcpPassiveOpens].

              iseg/s
                     The total number of segments received per second, includ-
                     ing  those  received  in  error  [tcpInSegs].  This count
                     includes segments received on currently established  con-
                     nections.
[...]
```

> Specific uses of sar(1) are described later in this book; see Chapters 6 to 10 . Appendix C is a summary of the sar(1) options and metrics.

**man page 里的方括号 SNMP 名**（`[tcpActiveOpens]`、`[tcpPassiveOpens]`、`[tcpInSegs]`）
是每个指标的**稳定标识符**——把 sar 的历史归档和外部监控系统对接时，
用这些名字比用「第几列」更靠得住。
原书附录 C 是 sar 选项与指标的汇总表；本库另有 [[附录B-Appendix B： sar Summary|附录B： sar Summary]] 一篇，正是这份速查表。

## §4.5 追踪工具

> Linux tracing tools use the previously described events interfaces (tracepoints, kprobes, uprobes, USDT) for advanced performance analysis. The main tracing tools are:

**五个工具，五种定位**（以下引文逐字来自原文）：

**perf(1)** ——官方 profiler：

> perf(1) : The official Linux profiler. It is excellent for CPU profiling (sampling of stack traces) and PMC analysis, and can instrument other events, typically recording to an output file for post-processing.

**Ftrace** ——官方 tracer：

> Ftrace : The official Linux tracer, it is a multi-tool composed of different tracing utilities. It is suited for kernel code path analysis and resource-constrained systems, as it can be used without dependencies.

**BPF（BCC / bpftrace）**：

> BPF (BCC, bpftrace) : Extended BPF was introduced in Chapter 3 , Operating Systems , Section 3.4.4 , Extended BPF . It powers advanced tracing tools, the main ones being BCC and bpftrace. BCC provides powerful tools, and bpftrace provides a high-level language for custom one-liners and short programs.

**SystemTap**：

> SystemTap : A high-level language and tracer with many tapsets (libraries) for tracing different targets [Eigler 05] [Sourceware 20] . It has recently been developing a BPF backend, which I recommend (see the stapbpf(8) man page).

**LTTng**：

> LTTng : A tracer optimized for black-box recording: optimally recording many events for later analysis [LTTng 20] .

**读这张表的三个抓手**：① **perf(1) 强在 profiling 与 PMC**；
② **Ftrace 强在内核代码路径分析，而且无依赖**——资源受限的系统上只有它能上；
③ **BCC/bpftrace 是「其余一切」的工具箱**，bpftrace 提供高层语言，
可以写 one-liner 和短程序（[[Chapter 3. Operating Systems|Chapter 3 §3.4.4]] 已介绍 eBPF 基础）。
SystemTap 的 BPF 后端作者**明确推荐**（`stapbpf(8)`）；LTTng 的定位是**黑盒记录**——尽量多地录事件，事后分析。

**原书的编排是刻意的**：

> The first three tracers are covered in Chapter 13 , perf ; Chapter 14 , Ftrace ; and Chapter 15 , BPF . The chapters that now follow (5 to 12) include various uses of these tracers, showing the commands to type and how to interpret the output. This ordering is deliberate, focusing on uses and performance wins first, and then covering the tracers in more detail later if and as needed.

即：**先讲怎么用、带来什么性能收益，再回头讲工具内部**。
本篇所在的这本书里，perf / Ftrace / BPF 各自另有专章（[[Chapter 13. perf]]、[[Chapter 14. Ftrace]]、[[Chapter 15. BPF]]），本篇不重复其内部机制。

**作者在 Netflix 的实际分工**（原文 L1512）：

> At Netflix, I use perf(1) for CPU analysis, Ftrace for kernel code digging, and BCC/bpftrace for everything else (memory, file systems, disks, networking, and application tracing).

**CPU 分析 → perf(1)；内核代码挖掘 → Ftrace；其余（内存、文件系统、磁盘、网络、应用）→ BCC/bpftrace。**

## §4.6 观察可观测性

**本章最该被记住的一段**（原文 L1516）：

> Observability tools and the statistics upon which they are built are implemented in software, and all software has the potential for bugs. The same is true for the documentation that describes the software. Regard with a healthy skepticism any statistics that are new to you, questioning what they really mean and whether they are really correct.

**前提一句话**：可观测性工具和它们统计的指标**都是软件写的**，
**软件就可能有 bug**；**描述软件的文档同样是软件**，也可能有 bug。
所以对**任何你还不熟的指标**都要保持健康怀疑：**它到底在量什么？它对不对？**

**指标可能出的六类问题**（原文 L1520–L1532，逐条照抄）：

> Tools and measurements are sometimes wrong.

> Man pages are not always right.

> Available metrics may be incomplete.

> Available metrics may be poorly designed and confusing.

> Metric collectors (e.g., that parse tool output) can have bugs.

> Metric processing (algorithms/spreadsheets) can also introduce errors.

**这六条里最阴的是第四、第五、第六条**——工具和测量本身都对，
**是中间那层「把工具输出变成报表的东西」把数字搞坏了**。
原文给了一个真实案例（原文 L1530 脚注）：

> 13 In this case the tool and measurement are correct, but an automated collector has introduced errors. At Surge 2013 I gave a lightning talk on an astonishing case [Gregg 13c] : a benchmarking company reported poor metrics for a product I was supporting, and I dug in. It turned out the shell script they used to automate the benchmark had two bugs. First, when processing output from fio(1), it would take a result such as “100KB/s” and use a regular expression to elide nun-numeric characters, including “KB/s” to turn this into “100”. Since fio(1) reported results with different units (bytes, Kbytes, Mbytes), this introduced massive (1024x) errors. Second, they also elided decimal places, so a result of “1.6” became “16”.

**这个案例的两个 bug 都是「正则清洗」惹的**：把 `100KB/s` 洗成 `100`（**丢了单位，差 1024 倍**），
又把 `1.6` 洗成 `16`（**丢了小数点**）。
**测量没错，是「测量结果到数字」这一步错了**——这直接对应上一节说的「指标采集器有 bug」。
（原文此处的 “nun-numeric” 系原书排印/转换之误，疑为 “non-numeric”；引文照录不改。）

**怎么验证**（原文 L1534、L1536、L1538）：

> When multiple observability tools have overlapping coverage, you can use them to cross-check each other. Ideally, they will source different instrumentation frameworks to check for bugs in the frameworks as well. Dynamic instrumentation is especially useful for this purpose, as custom tools can be created to double-check metrics.

> Another verification technique is to apply known workloads and then to check that the observability tools agree with the results you expect. This can involve the use of micro-benchmarking tools that report their own statistics for comparison.

> Sometimes it isn’t the tool or statistic that is in error, but the documentation that describes it, including man pages. The software may have evolved without the documentation being updated.

**两条验证路径**：① **交叉验证**——用覆盖率重叠的工具互相核对，
**最好是来自不同插桩框架的**（这样连框架本身的 bug 也能查出来）；
**动态插桩尤其适合**，因为可以临时写个自定义工具去对账。
② **已知负载对照**——施加一个你已知结果的负载，看工具报出来的数是否对得上。

**取舍是明说的**（原文 L1540）：

> Realistically, you may not have time to double-check every performance measurement you use and will do this only if you encounter unusual results or a result that is company critical. Even if you do not double-check, it can be valuable to be aware that you didn’t and that you assumed the tools were correct.

**不要求每个数都双查**——只在**结果异常**或**结果对公司关键**时查。
但即使没查，**「知道自己没查、且默认工具是对的」这件事本身有价值**。

**最后是「缺指标」这个更隐蔽的问题**（原文 L1542、L1544）：

> Metrics can also be incomplete. When faced with a large number of tools and metrics, it may be tempting to assume that they provide complete and effective coverage. This is often not the case: metrics may have been added by programmers to debug their own code, and later built into observability tools without much study of real customer needs. Some programmers may not have added any at all to new subsystems.

> An absence of metrics can be more difficult to identify than the presence of poor metrics. Chapter 2 , Methodologies , can help you find these missing metrics by studying the questions you need answered for performance analysis.

**指标多半是程序员为自己调试代码而加的**，后来被顺手并进观测工具，
**没人研究过真实客户到底需要什么**；**新子系统里可能一个指标都没加**。
**「没有指标」比「指标很糟」更难发现**——
解法是回到 [[Chapter 2. Methodologies|Chapter 2 方法论]]：**从「我要回答什么问题」倒推需要哪些指标**。

## 章末选型判据

**按「你手上是什么问题」查表，不按工具名查表**：

| 你要回答的问题 | 选它 | 判据依据 |
|---|---|---|
| 「昨天上午 10 点那台机器的 CPU/网络到底多少」 | `sar` 归档（`/var/log/sysstat/saNN`） | §4.2.4 / §4.4.2 |
| 「现在这几秒在抖，抖在哪」 | `sar -n TCP 1 5` 实时模式（**不需要先启用采集**） | §4.4.3 |
| 「要把历史指标接进数据库/自己的系统」 | `sadf -j`（配 `jq`）或 `sadf -d`（**注意分隔符是分号**） | §4.4.2 |
| 「哪些指标叫什么名字、要怎么解释」 | `man sar` 的 **SNMP 方括号名** + 附录 C | §4.4.4 |
| 「哪条路径在吃 CPU / 热点分布在哪」 | perf(1)（profiling，采样栈） | §4.2.2 / §4.5 |
| 「这一次请求为什么慢、每一次 I/O 发生了什么」 | tracing：系统级优先 perf trace / Ftrace / bpftrace | §4.2.3 / §4.5 |
| 「要内核里某个函数被调了多少次、每次多久」 | tracepoints（首选，开销最低）→ kprobes（无现成 tracepoint 时） | §4.3.5 / §4.3.6 |
| 「要用户态函数（malloc 等）的每次调用」 | uprobes（**开销比 kernel 侧高一两个数量级**）；能改代码就用 USDT | §4.3.7 / §4.3.8 |
| 「要 cache miss、分支错误这类硬件事件」 | perf(1) + PMC（**受 CPU 寄存器数量限制，云上常常拿不到**） | §4.3.9；云上 PMC 常拿不到，参见 [[Chapter 11. Cloud Computing]] |
| 「某个计数器值我不信」 | 换一个**不同插桩框架**的同类工具交叉验证；或施加已知负载对照 | §4.6 |
| 「我想看的东西根本没指标」 | 回到问题清单倒推（[[Chapter 2. Methodologies]]），而不是继续翻工具 man page | §4.6 |

**几条不需要查表、必须记住的硬规则**：

- **生产环境不要用 gdb/ptrace 做逐事件追踪**——断点式追踪能让目标**慢一百倍以上**（§4.3.10）。
- **资源受限的系统上只有 Ftrace 能上**，因为它**无依赖**（§4.5）；反过来，**Ftrace 是内核专用**，
  看用户态要走 perf(1) 或 BCC/bpftrace。
- **内核侧的插桩开销比用户态低一个数量级**：tracepoint 约 96ns、kprobe 76ns / kretprobe 212ns，
  而 uprobe 1287ns / uretprobe 1931ns（**且这些是某台特定机器的实测值**）。
  **要频繁采样就往内核侧挪**（§4.3.5–§4.3.7）。
- **profiler 用 99 Hz 而不是 100 Hz**，是为了不和被测活动「踩在同一个节拍上」而导致多算或少算（§4.2.2）。
- **任何数字在被信任之前先问一句**：它到底在量什么？如果它来自「解析工具输出的自动化采集器」，
  那它可能比工具本身更不可靠（§4.6）。
