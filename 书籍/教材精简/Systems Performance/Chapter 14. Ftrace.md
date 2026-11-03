---
title: Chapter 14. Ftrace（精读）
category: 性能工程
tags:
  - Ftrace
  - tracefs
  - kprobes
  - uprobes
  - trace-cmd
  - 内核追踪
difficulty: 进阶
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）第14章 Ftrace"
source_section: "精读 §14.1 / §14.2 / §14.2.1 / §14.3 / §14.4 / §14.4.1 / §14.4.2 / §14.4.3 / §14.5 / §14.5.1 / §14.5.2 / §14.6 / §14.6.1 / §14.6.2 / §14.6.3 / §14.6.4 / §14.6.5 / §14.7 / §14.7.1 / §14.7.2 / §14.7.3 / §14.7.4 / §14.8 / §14.8.1 / §14.8.2 / §14.9 / §14.10 / §14.10.1 / §14.10.2 / §14.10.3 / §14.10.4 / §14.10.5 / §14.10.6 / §14.10.7 / §14.11 / §14.11.1 / §14.11.2 / §14.11.3 / §14.11.4 / §14.11.5 / §14.11.6 / §14.12 / §14.13 / §14.13.1 / §14.13.2 / §14.13.3 / §14.13.4 / §14.13.5 / §14.13.6 / §14.13.7；§14.14 Ftrace Documentation 与 §14.15 References 不立篇（文档链接清单与文献表是全书共用的索引材料，不承载机制陈述）；§14.11.2 下的 Listing Events / Function Tracing / Function Graph Tracing / Event Tracing / Reporting / Other Capabilities 与 §14.13.4 下的 Ftrace Profilers / Ftrace Tracers / Event Tracing / CPU Registers 是无编号子标题，按原文文字引用，不编 § 号"
skills:
  - obsidian-markdown
---

# Chapter 14. Ftrace

> **小节地图**：本章讲的不是某个性能指标，而是**内核里可观测性的基础设施**——怎么在 Linux 上
> 把内核运行时的事件抓出来、统计出来、画出来。
> 立篇：§14.1（profiler 与 tracer 两条轴）、§14.2 / §14.2.1（tracefs 控制面）、
> §14.3（函数 profiler）、§14.4 / §14.4.1 / §14.4.2 / §14.4.3（函数 tracer 与三种输出接口）、
> §14.5 / §14.5.1 / §14.5.2（tracepoint：静态插桩 + filter + trigger）、
> §14.6 / §14.6.1–§14.6.5（kprobes：动态插桩、参数、返回值、profiler）、
> §14.7 / §14.7.1–§14.7.4（uprobes：用户态动态插桩）、
> §14.8 / §14.8.1 / §14.8.2（function_graph 调用图）、§14.9（hwlat）、
> §14.10 / §14.10.1–§14.10.7（hist triggers 与 synthetic events）、
> §14.11 / §14.11.1–§14.11.6（trace-cmd 前端）、§14.12（perf ftrace）、
> §14.13 / §14.13.1–§14.13.7（perf-tools 工具集）。
> **不立篇**：§14.14 Ftrace Documentation（只是内核文档与前端站点链接清单）、
> §14.15 References（文献表）。
> 底层依赖：tracepoint / kprobe / uprobe 的**首次出现**在第 4 章 Observability Tools；
> perf(1) 在第 13 章；eBPF / BCC / bpftrace 在第 15 章。
> 同层对照：[[Chapter 3. Operating Systems]]（内核态机制本身：中断、抢占、上下文切换）、
> [[Chapter 9. Disks]]（块 I/O 事件的业务含义）、[[Chapter 12. Benchmarking]]（实验方法学）。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §14.1 能力总览 | 14-Chapter 14. Ftrace.md L89 | While perf(1) uses subcommands for different functionality, Ftrace has profilers and tracers . Profilers provide statistical summaries such as counts and histograms, and tracers provide per-event details. |
| §14.2 tracefs | 14-Chapter 14. Ftrace.md L268 | Ftrace was originally part of the debugfs file system until it was split into its own tracefs. When debugfs is mounted, it still preserves the original directory structure by mounting tracefs as a tracing subdirectory. You can list both debugfs and tracefs mount points using: |
| §14.3 函数 profiler | 14-Chapter 14. Ftrace.md L429 | The function profiler works by using compiled-in profiling calls at the start of every kernel function. This approach is based on how compiler profilers work, such as gcc(1)’s -pg option, which inserts mcount() calls for use with gprof(1). Since gcc(1) version 4.6, this mcount() call is now __fentry__(). Adding calls to every kernel function sounds like it should cost significant overhead, which would be a concern for something that may rarely be used, but the overhead problem has been solved: when not in use, these calls are typically replaced with fast nop instructions, and only switched to the __fentry__() calls when needed [Gregg 19f] . |
| §14.4 函数 tracer | 14-Chapter 14. Ftrace.md L493 | The function tracer prints per-event details for kernel function calls, and uses the function profiling instrumentation described in the previous section. This can show the sequence of various functions, timestamp-based patterns, and the on-CPU process name and PID that may be responsible. The overhead of function tracing is higher than function profiling, and so tracing is best suited for relatively infrequent functions (less than 1,000 calls per second). You can use function profiling from the previous section to find out the rate of functions before tracing them. |
| §14.4.2 trace_pipe | 14-Chapter 14. Ftrace.md L573 | The trace_pipe file is a different interface for reading the trace buffer. Reads from this file return an endless stream of events. It also consumes events, so after reading them once they are no longer in the trace buffer. |
| §14.5.2 trigger | 14-Chapter 14. Ftrace.md L720 | A trigger runs an extra tracing command when an event fires. That command may be to enable or disable other tracing, print a stack trace, or take a snapshot of the tracing buffer. Available trigger commands can be listed from the trigger file when no trigger is currently set. For example: |
| §14.6 kprobes | 14-Chapter 14. Ftrace.md L743 | kprobes are kernel dynamic instrumentation, and were introduced in Chapter 4 , Observability Tools , Section 4.3.6 , kprobes . kprobes create kprobe events for tracers to use, which share tracefs output and control files with Ftrace. kprobes are similar to the Ftrace function tracer, covered in Section 14.4 , in that they trace kernel functions. kprobes, however, can be customized further, can be placed on function offsets (individual instructions), and can report function argument and return values. |
| §14.6.2 参数别名 | 14-Chapter 14. Ftrace.md L820 | Accessing arguments to functions as $arg1, $arg2, etc., was added in Linux 4.20. Prior Linux versions required the use of register names. |
| §14.6.3 返回值 | 14-Chapter 14. Ftrace.md L835 | The special alias $retval for the return value is available for use with kretprobes. The following example uses it to show the return value of do_nanosleep(): |
| §14.7.1 uprobe 事件 | 14-Chapter 14. Ftrace.md L920 | The syntax now requires a path and an offset for the uprobe. The kernel does not have symbol information for user-space software, so this offset must be determined and provided to the kernel using user-space tools. |
| §14.7.1 偏移风险 | 14-Chapter 14. Ftrace.md L939 | WARNING: If you mistakenly use a symbol offset that is midway through an instruction, you will corrupt the target process (and for shared instruction text, all processes that share it!). The example technique of using readelf(1) to find the symbol offset may not work if the target binary has been compiled as a position-independent executable (PIE) with address space layout randomization (ASLR). I do not recommend that you use this interface at all: switch to a higher-level tracer that takes care of symbol mappings for you (e.g., BCC or bpftrace). |
| §14.8.1 函数图 | 14-Chapter 14. Ftrace.md L1021 | This example deliberately did not set a function filter (set_ftrace_filter), so that all child calls can be seen. This does, however, cost some overhead, inflating the reported durations. It is still generally useful for locating the origin of high latencies, which can dwarf the added overhead. When you want more accurate times for a given function, you can use a function filter to reduce the functions traced. For example, to only trace do_nanosleep(): |
| §14.9 hwlat | 14-Chapter 14. Ftrace.md L1083 | WARNING: I’d classify hwlat as a microbenchmark tool rather than an observability tool, because it performs an experiment that will itself perturb the system: it will make one CPU busy for the width duration, with interrupts disabled. |
| §14.10 直方图触发器 | 14-Chapter 14. Ftrace.md L1087 | Hist triggers is an advanced Ftrace capability added to Linux 4.7 by Tom Zanussi, which allows the creation of custom histograms on events. It is another form of statistical summary, allowing counts to be broken down by one or more components. |
| §14.10.1 单键 | 14-Chapter 14. Ftrace.md L1146 | The output shows that PID 32396 performed 882,604 syscalls while tracing, and listed the counts for the other PIDs. The final few lines show statistics: the number of writes to the hash ( Hits ), the entries in the hash ( Entries ), and how many writes were dropped if the entries exceeded the hash size ( Dropped ). If drops occur, you can increase the size of the hash when declaring it; it defaults to 2048. |
| §14.10.7 合成事件 | 14-Chapter 14. Ftrace.md L1301 | This is where things start to get really weird (if they haven’t already). A synthetic event can be created that is triggered by other events, and can combine their event arguments in custom ways. To access event arguments from prior events, they can be saved into a histogram and fetched by the later synthetic event. |
| §14.10.7 哈希即程序 | 14-Chapter 14. Ftrace.md L1379 | Ftrace histograms are implemented as a hash object (key/value store), and the earlier examples only used these hashes for output: showing syscall counts by PID and ID. With synthetic events, we’re doing two extra things with these hashes: A) storing values that are not part of the output (timestamps) and B) in one event, fetching key/value pairs that were set by another event. We’re also performing arithmetic: a minus operation. In a way, we’re starting to write mini programs. |
| §14.11.3 trace-cmd vs perf | 14-Chapter 14. Ftrace.md L1782 | One advantage of trace-cmd is its better support for the function and function_graph tracers. |
| §14.13.2 单用途工具 | 14-Chapter 14. Ftrace.md L2006 | The way I implemented this helps to explain the need for extended BPF. iolatency(8) traces block I/O issue and completion tracepoints, reads all events in user-space, parses them, and post-processes them into these histograms using awk(1). Since disk I/O has a relatively low frequency on most servers, this approach was possible without onerous overhead. But the overhead would be prohibitive for more frequent events, such as network I/O or scheduling. Extended BPF solved this problem by allowing the histogram summary to be calculated in kernel space, and only the summary is passed to user space, greatly reducing overhead. Ftrace now supports some similar capabilities with hist triggers and synthetic events, described in Section 14.10 , Ftrace Hist Triggers (I need to update iolatency(8) to make use of them). |
| §14.13.6 perf-tools vs BCC | 14-Chapter 14. Ftrace.md L2272 | funcgraph(8) : This tool does not exist in BCC, since it uses Ftrace function_graph tracing. |

> **书里没说的**（本篇的推论，原文没有，别当原文观点）：
> - **「profiler → tracer → graph 三级成本阶梯」是本篇为检索方便整理的排序**；原文只在 §14.4 给出
>   「function tracing 适合每秒少于 1000 次的函数」这一个门槛，没有给出各级开销的量化倍数。
> - **「trace_pipe 是消费式 ⇒ 已被它读过的数据无法再用 trace 文件回看」这条后果链是本篇的推论**；
>   原文分别说了 trace_pipe 会消费事件、以及 trace_pipe 适合低频而高频应存文件，但没把两者连起来。
> - **「hist triggers 是 Ftrace 版的 eBPF 内核态聚合」这一定位是本篇的连线**：原文 §14.13.2 说
>   extended BPF 解决「在内核态算摘要」的问题，§14.10.7 说 hist triggers 与 synthetic events
>   提供了类似能力，但没把两者写成替代关系。
> - **「生产环境一律用 instances 目录」是本篇的工程建议**；原文只说 instances 是为了并发用户支持。
> - **本章没有给出任何 Ftrace 的定量开销数字**（CPU 百分比、事件丢失率），原文本章也没有；
>   要量化开销请看第 13 章 perf。

## §14.1 能力总览：profiler 与 tracer 是两条轴

**机制**：Ftrace 是 Linux 的**官方 tracer**，由 Steven Rostedt 创建，2008 年随 Linux 2.6.27 合入。
原文的定位句是：

> Ftrace is the official Linux tracer, a multi-tool composed of different tracing utilities.

它不是「一个工具」，而是**一堆工具共用一个内核基础设施**。这个基础设施就是 tracefs——
一套文件，每个文件是一个控制旋钮。

它最重要的一个属性是**零前端依赖**：

> It can be used without any additional user-level front end, making it especially suited for embedded Linux environments where storage space is at a premium.

这决定了 Ftrace 与 perf(1)、BCC 的分野：后两者要装、要编译、要常驻；Ftrace 只要一个 shell。
§14.13 的 perf-tools 就是把这条性质封装成「每个工具一个 Unix 命令」的产物。

### 两条轴：profiler 与 tracer

> While perf(1) uses subcommands for different functionality, Ftrace has profilers and tracers . Profilers provide statistical summaries such as counts and histograms, and tracers provide per-event details.

**这是全章最该先记住的区分**：

| | Profiler（剖析器） | Tracer（追踪器） |
|---|---|---|
| 输出 | 统计摘要：计数、直方图 | 逐事件明细 |
| 典型代表 | function profiler、kprobe profiler、uprobe profiler、hist triggers | function、function_graph、hwlat、tracepoint、kprobe、uprobe |
| 成本 | 低 | 高 |
| 典型用途 | 「哪些函数在用、最慢的是哪个」 | 「谁调用了它、调用路径是什么、子函数耗时多少」 |

原文在章首列了 Ftrace 能回答的四类问题，正好对应这两条轴：

- How often are certain kernel functions called? → **profiler 轴**
- What code path led to calling this function? → **tracer 轴**
- What child functions does this kernel function call? → **tracer 轴**（function_graph）
- What is the highest latency caused by preemption disabled code paths? → **tracer 轴**（irqsoff / preemptoff）

### 章首给的章节地图

原文把 14.3–14.13 按「profilers → tracers → front ends」排布，这个顺序有讲究：

- **Profilers**：§14.3（function profiler）、§14.10（hist triggers）
- **Tracers**：§14.4（function）、§14.5（tracepoints）、§14.6（kprobes）、§14.7（uprobes）、
  §14.8（function_graph）、§14.9（hwlat）
- **Front ends**：§14.11（trace-cmd）、§14.12（perf ftrace）、§14.13（perf-tools）

hist triggers 被特意放到 14.10，是因为它**同时用到 profiler 和 tracer 的概念**：

> Ftrace hist triggers is an advanced topic that requires covering both profilers and tracers first, hence its later location in this chapter.

### 两张能力表（Table 14.1 / Table 14.2）

原文用两张表给出 Linux 5.2 的完整能力清单，本篇整理成表：

**Table 14.1 Ftrace profilers**

| Profiler | 说明 | 本章位置 |
|---|---|---|
| function | 内核函数统计 | §14.3 |
| kprobe profiler | 已启用的 kprobe 计数 | §14.6.5 |
| uprobe profiler | 已启用的 uprobe 计数 | §14.7.4 |
| hist triggers | 事件上的自定义直方图 | §14.10 |

**Table 14.2 Ftrace 与 event tracers**

| Tracer | 说明 | 本章位置 |
|---|---|---|
| function | 内核函数调用追踪器 | §14.4 |
| tracepoints | 内核静态插桩（event tracer） | §14.5 |
| kprobes | 内核动态插桩（event tracer） | §14.6 |
| uprobes | 用户态动态插桩（event tracer） | §14.7 |
| function_graph | 带子调用层级图的内核函数追踪 | §14.8 |
| wakeup / wakeup_rt | 测最大 CPU 调度延迟 / 实时的最大调度延迟 | — |
| irqsoff | 关中断事件与延迟（带代码位置） | — |
| preemptoff | 抢占关闭事件与延迟 | — |
| preemptirqsoff | irqsoff 与 preemptoff 的组合 | — |
| blk | 块 I/O 追踪器（blktrace(8) 用它） | — |
| hwlat | 硬件延迟追踪器：检测外部扰动 | §14.9 |
| mmiotrace | 追踪模块对硬件的调用 | — |
| nop | 停用其它 tracer 的特殊 tracer | — |

表里的脚注是一条硬约束：**irqsoff / preemptoff / preemptirqsoff 需要 `CONFIG_PREEMPTIRQ_EVENTS` 打开**。
本章没有展开这三个 tracer（它们在第 3 章的延迟分析口径里被引用过），但**它们是否可用取决于内核配置**，
这是排查「为什么 irqsoff 找不到」的第一答案。

注意表里 **tracepoints / kprobes / uprobes 不是 Ftrace 自带的 tracer，而是「event tracer」**——
它们与 Ftrace 共享同一套 tracefs 输出与控制文件，所以原文把它们一并收进本章：

> These event tracers are Ftrace-like, sharing similar configuration and output interfaces, and are included in this chapter.

还有一条操作性细节：**表里的 tracer 名就是配置时写进 `current_tracer` 的关键字**。

查看当前内核支持哪些 tracer：

```bash
# cat /sys/kernel/debug/tracing/available_tracers
hwlat blk mmiotrace function_graph wakeup_dl wakeup_rt wakeup function nop
```

## §14.2 与 §14.2.1 tracefs：Ftrace 的控制面

**机制**：Ftrace 的一切操作都是**读写文件**。接口是 tracefs 文件系统。

**挂载点**：原文说 tracefs「应该」挂在 `/sys/kernel/tracing`：

```bash
mount -t tracefs tracefs /sys/kernel/tracing
```

但历史上是另一回事：

> Ftrace was originally part of the debugfs file system until it was split into its own tracefs. When debugfs is mounted, it still preserves the original directory structure by mounting tracefs as a tracing subdirectory.

也就是说 **debugfs 被挂载时，tracefs 会自动作为它的 `tracing` 子目录挂进去**，于是路径变成
`/sys/kernel/debug/tracing`。原文的示例全部用这个路径，并明说「未来会改成 `/sys/kernel/tracing`」。
**判据：找不到 Ftrace 目录时，两个路径都试一遍**（推演：这是同一套接口的两个挂载点，不是两套机制）。

诊断挂载失败：

> Note that if tracefs fails to mount, one possible reason is that your kernel has been built without the Ftrace config options (CONFIG_FTRACE, etc.).

**这是本章第一个内核配置闸门**：tracefs 挂不上 → 内核没编 Ftrace，后面所有能力都不存在。

### 为什么是「文件」而不是「命令」

> Other Ftrace control and output files can also be used via echo(1) and cat(1). This means that Ftrace has virtually zero dependencies to be used (only a shell is needed).

**一个 echo 一个 cat 就是全部接口**。这条性质是后面 §14.13 的 perf-tools 能存在的前提：
perf-tools 本质就是**自动帮你写这些文件的 shell 脚本**。

### 关键控制文件（Table 14.3 摘要）

| 文件 | 访问 | 作用 |
|---|---|---|
| `available_tracers` | 读 | 列出本内核支持的 tracer |
| `current_tracer` | 读写 | 当前启用的 tracer（写名字即切换） |
| `function_profile_enabled` | 读写 | 开关函数 profiler |
| `available_filter_functions` | 读 | 列出可追踪的函数 |
| `set_ftrace_filter` | 读写 | 选函数 |
| `set_graph_function` | 读写 | 选 function_graph 的根函数 |
| `set_ftrace_notrace` | 读写 | 排除函数 |
| `tracing_on` | 读写 | 开关输出环形缓冲区 |
| `trace` | 读写 | tracer 的输出（环形缓冲） |
| `trace_pipe` | 读 | tracer 的输出（消费式流） |
| `trace_options` / `options/` | 读写 | 输出格式选项 |
| `trace_stat/` | 读写 | 函数 profiler 的输出 |
| `kprobe_events` / `uprobe_events` | 读写 | kprobe / uprobe 的配置 |
| `events/` | 读写 | event tracer 控制目录（tracepoint、kprobe、uprobe） |
| `instances/` | 读写 | **并发用户用的 Ftrace 实例** |

### 并发用户：instances 是本章的工程要害

> Steven Rostedt built Ftrace for his own use while developing the realtime patch set, and initially it did not support concurrent users. For example, the current_tracer file can only be set to one tracer at a time.

早期的 Ftrace **一次只能有一个 tracer**——因为 `current_tracer` 是全局单值。
后来加的 `instances/` 目录解决了这个问题：**每个实例有自己的 `current_tracer` 和输出文件**，
可以独立追踪。

**判据（推演，本篇的工程建议）**：在共享的生产机上，永远在 `instances/` 下建自己的实例
（`mkdir instances/myprobe`）再写 `instances/myprobe/current_tracer`，
不要直接抢全局 `current_tracer`——否则两个人同时追，一个人的 tracer 会被另一个人的
`echo nop` 关掉，而且丢的是**两个人的数据**。

## §14.3 函数 profiler：先用便宜的量频率

**机制**：函数 profiler 给出**内核函数调用的统计**，适合摸清「哪些内核函数在用、哪些最慢」。
原文说这是他理解内核执行的**起点**，因为它高效、开销相对低：

> The function profiler provides statistics on kernel function calls, and is suited for exploring which kernel functions are in use and identifying which are the slowest.

**实现原理**（本章最值得记住的一段机制）：

> The function profiler works by using compiled-in profiling calls at the start of every kernel function.

类比是编译器 profiler：gcc 的 `-pg` 选项会在每个函数开头插 `mcount()` 调用供 gprof 用；
gcc 4.6 起这个调用改名 `__fentry__()`。Ftrace 的做法一致——**编译期在每个内核函数头部插桩**。

「在每个函数都插调用，开销不是应该很大吗？」原文直接回答了：

> when not in use, these calls are typically replaced with fast nop instructions, and only switched to the __fentry__() calls when needed

**关键机制**：不用的时候，插桩点是**一条 nop 指令**（几乎零成本）；启用 profiler 时才替换成真正的
`__fentry__()` 调用。所以「全局插桩」的开销不是「每次调用都付一次函数调用」，而是「每次调用多一条 nop」。

**前置条件**：需要 `CONFIG_FUNCTION_PROFILER=y` 内核选项。

### 用法：四个写操作

未启用时的初始状态（`set_ftrace_filter` 为空意味着「全部函数都开」）：

```bash
# cd /sys/kernel/debug/tracing
# cat set_ftrace_filter
#### all functions enabled ####
# cat function_profile_enabled
0
```

统计所有以 `tcp` 开头的内核函数调用约 10 秒：

```bash
# echo 'tcp*' > set_ftrace_filter
# echo 1 > function_profile_enabled
# sleep 10
# echo 0 > function_profile_enabled
# echo > set_ftrace_filter
```

**shell 语法陷阱（原文专门用 Tip 标出）**：必须写 ` 0 > `（带空格），不能写 ` 0> `——
后者是**对文件描述符 0 的重定向**，语义完全不同。同理 ` 1 > ` 也是危险写法。

### 输出：每 CPU 一个文件

统计结果在 `trace_stat/function*` 里，**每个 CPU 一个文件**（示例是 2-CPU 系统）：

```bash
# head trace_stat/function*
==> trace_stat/function0 <==
  Function                       Hit    Time            Avg             s^2
  --------                       ---    ----            ---             ---
  tcp_sendmsg                 955912    2788479 us      2.917 us        3734541 us
  tcp_sendmsg_locked          955912    2248025 us      2.351 us        2600545 us
  tcp_push                    955912    852421.5 us     0.891 us        1057342 us
  tcp_write_xmit              926777    674611.1 us     0.727 us        1386620 us
...
==> trace_stat/function1 <==
  Function                       Hit    Time            Avg             s^2
  --------                       ---    ----            ---             ---
  tcp_sendmsg                 317935    936055.4 us     2.944 us        13488147 us
...
```

五列的含义：`Function` 函数名、`Hit` 调用次数、`Time` 函数内总时间、`Avg` 平均耗时、
`s^2` 标准差。**按 CPU 分开**这一点很重要：TCP 相关函数在两个 CPU 上的调用量差了一个数量级
（955912 vs 317935），这类负载不均衡在汇总成一个数之前是看不见的。

### 开销纪律

> A small amount of overhead is added to the profiled functions during profiling. If the set_ftrace_filter is left blank, all kernel functions are profiled

**filter 留空 = 全函数都被 profile**。所以要**用 filter 限定范围**（上面用 `tcp*`），
这是把 profiler 开销压下来的唯一手段。

## §14.4 函数 tracer：逐事件明细及其代价

**机制**：函数 tracer 输出**逐事件**的内核函数调用明细，复用 §14.3 那套插桩。
它能给出：函数调用序列、时间戳模式、以及当时占用 CPU 的**进程名与 PID**。

> The overhead of function tracing is higher than function profiling, and so tracing is best suited for relatively infrequent functions (less than 1,000 calls per second). You can use function profiling from the previous section to find out the rate of functions before tracing them.

**这是本章最该背下来的一条操作规则**：**先 profiler 量频率，确认某个函数每秒调用低于约 1000 次，
再对它开 function tracer**。频率超标时 tracer 会把环形缓冲冲垮，丢事件。

### §14.4.1 用 trace 文件读输出

```bash
# cd /sys/kernel/debug/tracing
# echo 1 > tracing_on
# echo '*sleep' > set_ftrace_filter
# echo function > current_tracer
# sleep 10
# cat trace > /tmp/out.trace01.txt
# echo nop > current_tracer
# echo > set_ftrace_filter
```

输出长这样（注意表头本身就是一份字段说明）：

```bash
# more /tmp/out.trace01.txt
# tracer: function
#
# entries-in-buffer/entries-written: 57/57   #P:2
#
#                              _-----=> irqs-off
#                             / _----=> need-resched
#                            | / _---=> hardirq/softirq
#                            || / _--=> preempt-depth
#                            ||| /     delay
#           TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
#              | |       |   ||||       |         |
      multipathd-348   [001] .... 332762.532877: __x64_sys_nanosleep <-do_syscall_64
      multipathd-348   [001] .... 332762.532879: hrtimer_nanosleep <-__x64_sys_nanosleep
      multipathd-348   [001] .... 332762.532880: do_nanosleep <-hrtimer_nanosleep
           sleep-4203  [001] .... 332762.722497: __x64_sys_nanosleep <-do_syscall_64
...
```

**五列**：TASK-PID（进程名-PID）、CPU#、flags（那五个点）、TIMESTAMP（秒）、FUNCTION（当前函数 <- 调用者）。

**flags 列（那五个点）是最容易被忽略、也最有用的一列**：它是
`irqs-off` / `need-resched` / `hardirq-softirq` / `preempt-depth` / `delay` 五个位。
**这正是第 3 章那套延迟分析口径的现场数据**（见 [[Chapter 3. Operating Systems]]）：
一行的点变成别的标记，意味着采样时处在中断或抢占上下文里——那几行不能当成普通用户态执行来读。

**缓冲区语义**：

> The trace buffer is also cleared when the current_tracer is set back to nop as I did in the example steps to disable tracing. It is also cleared when trace_pipe is used.

写一个换行到 `trace` 也能清空。三条清空路径（`current_tracer` 设回 nop / 写 `trace` / 用过 `trace_pipe`）
是「为什么我的 trace 是空的」的三个答案。

### §14.4.2 trace_pipe：消费式读取

> The trace_pipe file is a different interface for reading the trace buffer. Reads from this file return an endless stream of events. It also consumes events, so after reading them once they are no longer in the trace buffer.

**两个性质**：① 无尽流；② **消费**——读过的事件就没了。

```bash
# echo '*sleep' > set_ftrace_filter
# echo function > current_tracer
# cat trace_pipe
      multipathd-348   [001] .... 332624.519190: __x64_sys_nanosleep <-do_syscall_64
      multipathd-348   [001] .... 332624.519192: hrtimer_nanosleep <-__x64_sys_nanosleep
      multipathd-348   [001] .... 332624.519192: do_nanosleep <-hrtimer_nanosleep
            cron-504   [001] .... 332625.560150: __x64_sys_nanosleep <-do_syscall_64
^C
# echo nop > current_tracer
# echo > set_ftrace_filter
```

**选择规则（原文明确说了）**：

> The trace_pipe file is handy for watching low-frequency events, but for high-frequency events you will want to capture them to a file for later analysis using the trace file shown earlier.

| | `trace_pipe` | `trace` |
|---|---|---|
| 读法 | 无尽流，边跑边看 | 快照式，读的是缓冲区当前内容 |
| 是否消费 | **是**，读过即消失 | 否，可反复读 |
| 适合 | 低频事件实时观察 | 高频事件落盘后分析 |
| 清缓冲 | 用过就清空 | 写换行可清空 |

### §14.4.3 Options：改输出格式

选项在 `trace_options` 文件或 `options/` 目录里。例如关掉 flags 列：

```bash
# echo 0 > options/irq-info
# cat trace
# tracer: function
...
#           TASK-PID     CPU#   TIMESTAMP  FUNCTION
      multipathd-348   [001]  332762.532877: __x64_sys_nanosleep <-do_syscall_64
...
# echo 1 > options/irq-info
```

两个高价值选项：

> These options include stacktrace and userstacktrace , which will append kernel and user stack traces to the output: this is useful for understanding why functions were called.

**`stacktrace` 与 `userstacktrace` 是「为什么被调用」的答案**——前者补内核栈，后者补用户栈。
定位「这个内核调用是从哪条用户态路径进来的」必须开 `userstacktrace`。

## §14.5 Tracepoint：内核里的静态插桩

**机制**：tracepoint 是**内核静态插桩**——追踪函数就写在内核源码里，用一个定义并格式化参数的
trace event 接口来暴露。它在第 4 章介绍过，本章讲怎么用。

tracepoint 与 Ftrace **共享同一套 tracefs 输出与控制文件**，所以用法完全一致。

### 控制目录结构

每个 trace system 一个目录，每个 event 一个子目录：

```bash
# ls events/block/block_rq_issue/
enable  filter  format  hist  id  trigger
```

六个文件的分工（这是 §14.5 的全部机制）：

| 文件 | 作用 |
|---|---|
| `enable` | 开关这个事件 |
| `filter` | 条件过滤（§14.5.1） |
| `format` | 字段定义 + 打印格式（过滤与直方图的字段来源） |
| `hist` | 直方图结果（§14.10） |
| `id` | 事件 ID |
| `trigger` | 事件触发时执行的命令（§14.5.2） |

开一个 tracepoint 并实时看：

```bash
# cd /sys/kernel/debug/tracing
# echo 1 > events/block/block_rq_issue/enable
# cat trace_pipe
            sync-4844  [001] .... 343996.918805: block_rq_issue: 259,0 WS 4096 ()
...
^C
# echo 0 > events/block/block_rq_issue/enable
```

前五列与 §14.4.1 相同（进程名-PID、CPU ID、flags、时间戳、事件名），**其余是 tracepoint 的格式串**
——格式串的内容由 `format` 文件定义。块 I/O 字段的业务含义见 [[Chapter 9. Disks]]。

### §14.5.1 Filter：事件级过滤

> A filter can be included to record the event only when a Boolean expression has been met. It has a restricted syntax:

语法是 `field operator value`：

- **字段**来自 `format` 文件（Tracepoints Arguments and Format String 那一节），
- **数值运算符**：`==`、`!=`、`<`、`<=`、`>`、`>=`、`&`（按位与），
- **字符串运算符**：`==`、`!=`、`~`（**shell glob 式匹配**，通配符 `*`、`?`、`[]`），
- 可用**括号分组**，用 `&&`、`||` 组合。

例：只记 64 KB 以上的大 I/O。

```bash
# echo 'bytes > 65536' > events/block/block_rq_insert/filter
# cat trace_pipe
    kworker/u4:1-7173  [000] .... 378115.779394: block_rq_insert: 259,0 W 262144 ()
...
^C
# echo 0 > events/block/block_rq_insert/filter
```

**`echo 0` 重置过滤器**（注意是写 0，不是写空串）。

### §14.5.2 Trigger：事件触发一条命令

> A trigger runs an extra tracing command when an event fires. That command may be to enable or disable other tracing, print a stack trace, or take a snapshot of the tracing buffer.

可用命令（在没有设 trigger 时 `cat` 一下 trigger 文件就能列出来）：

```bash
# cat events/block/block_rq_issue/trigger
# Available triggers:
# traceon traceoff snapshot stacktrace enable_event disable_event enable_hist
disable_hist hist
```

**最经典的用法是「抓现场」**：

> One use case for triggers is when you wish to see events that led to an error condition: a trigger can be placed on the error condition that either disables tracing ( traceoff ) so that the trace buffer only contains the prior events, or takes a snapshot ( snapshot ) to preserve it.

**这是排障的黄金模式**：不去「时刻盯着缓冲区」，而是**让错误条件自己触发停录 / 快照**，
事后缓冲区里就只剩错误发生前的历史。

trigger 可以和 filter 用 `if` 组合：

```bash
# echo 'traceoff if bytes > 65536' > events/block/block_rq_insert/trigger
```

更复杂的动作交给 hist trigger（§14.10）。

## §14.6 kprobes：内核里的动态插桩

**机制**：kprobes 是**内核动态插桩**。它在 `kprobe_events` 里创建出 kprobe 事件，
供 tracer 使用，并**共享 Ftrace 的 tracefs 输出与控制文件**。

> kprobes, however, can be customized further, can be placed on function offsets (individual instructions), and can report function argument and return values.

**kprobes 比 function tracer 强在四点**：

1. **可自定义**（事件名、参数名）；
2. **可下探到函数内偏移**（单条指令级别，而不只是函数入口）；
3. **能报告参数**；
4. **能报告返回值**。

### §14.6.1 事件追踪

创建 / 启用 / 删除三步：

```bash
# echo 'p:brendan do_nanosleep' >> kprobe_events
# echo 1 > events/kprobes/brendan/enable
# cat trace_pipe
      multipathd-348   [001] .... 345995.823380: brendan: (do_nanosleep+0x0/0x170)
      multipathd-348   [001] .... 345996.823473: brendan: (do_nanosleep+0x0/0x170)
^C
# echo 0 > events/kprobes/brendan/enable
# echo '-:brendan' >> kprobe_events
```

**三个动作都是「追加一行特殊语法」**（创建 / 删除都靠 `kprobe_events`）。语法全貌：

```
  p[:[GRP/]EVENT] [MOD:]SYM[+offs]|MEMADDR [FETCHARGS]  : Set a probe
  r[MAXACTIVE][:[GRP/]EVENT] [MOD:]SYM[+0] [FETCHARGS]  : Set a return probe
  -:[GRP/]EVENT                                         : Clear a probe
```

**命名是共享资源**：多个用户 / 多个工具同时用 kprobe，事件名会撞车。原文明说
「自定义名字对区分不同 kprobe 使用者很有用」，并给出 BCC 的命名法——**函数名 + "bcc" + BCC 进程 PID**：

```bash
# cat /sys/kernel/debug/tracing/kprobe_events
p:kprobes/p_blk_account_io_start_bcc_19454 blk_account_io_start
p:kprobes/p_blk_mq_start_request_bcc_19454 blk_mq_start_request
```

顺带一个版本差异：**新版 BCC 已改用 `perf_event_open(2)` 接口挂 kprobe**，
所以用 perf_event_open 创建的事件**不会出现在 `kprobe_events` 里**——查不到不等于没挂上。

### §14.6.2 参数

> Accessing arguments to functions as $arg1, $arg2, etc., was added in Linux 4.20. Prior Linux versions required the use of register names.

```bash
# echo 'p:brendan do_nanosleep hrtimer_sleeper=$arg1 hrtimer_mode=$arg2' >> \
kprobe_events
# echo 1 > events/kprobes/brendan/enable
# cat trace_pipe
      multipathd-348   [001] .... 349138.128610: brendan: (do_nanosleep+0x0/0x170)
hrtimer_sleeper=0xffffaa6a4030be80 hrtimer_mode=0x1
...
```

**自定义参数名（`hrtimer_sleeper=`）既是输出标签，也是后面的过滤字段**（见 §14.6.4）。

**寄存器名是 Linux 4.20 之前的老路**：

```bash
# echo 'p:brendan do_nanosleep hrtimer_sleeper=%di hrtimer_mode=%si' >> kprobe_events
```

> To use register names, you need to know the processor type and the function calling convention in use.

**这是寄存器法的根本约束**：`%di`、`%si` 是 x86_64 的 AMD64 ABI 约定（第 1、2 个参数在 rdi、rsi）。
换架构就得换寄存器名，调用约定可查 `man 2 syscall`（§14.13.4 有摘录）。

### §14.6.3 返回值

> The special alias $retval for the return value is available for use with kretprobes.

```bash
# echo 'r:brendan do_nanosleep ret=$retval' >> kprobe_events
# echo 1 > events/kprobes/brendan/enable
# cat trace_pipe
      multipathd-348   [001] d... 349782.180370: brendan:
(hrtimer_nanosleep+0xce/0x1e0 <- do_nanosleep) ret=0x0
...
```

注意 **`r:` 前缀 = 返回探针（kretprobe）**，输出里还带上了**返回点所在的偏移**（`+0xce/0x1e0`）
和**调用者**（`hrtimer_nanosleep`）——这是探针能给的、function tracer 给不了的信息。

### §14.6.4 过滤与触发

与 tracepoint 完全同构（§14.5.1 / §14.5.2），操作目录是 `events/kprobes/...`。
关键在 `format` 文件：自定义的参数名会**变成可过滤的字段**。

```bash
# cat events/kprobes/brendan/format
name: brendan
ID: 2024
format:
        field:unsigned short common_type;  offset:0;  size:2;    signed:0;
        ...
        field:u64 hrtimer_sleeper;     offset:16;  size:8;    signed:0;
        field:u64 hrtimer_mode;  offset:24;     size:8;     signed:0;
print fmt: "(%lx) hrtimer_sleeper=0x%Lx hrtimer_mode=0x%Lx", REC->__probe_ip, REC-
>hrtimer_sleeper, REC->hrtimer_mode
```

所以可以直接按参数过滤：

```bash
# echo 'hrtimer_mode != 1' > events/kprobes/brendan/filter
```

**判据**：`format` 文件是**一切按字段操作的字典**——过滤（§14.5.1）、直方图键（§14.10.2）
都要从它取字段名。换了内核版本先 `cat format` 再写表达式。

### §14.6.5 kprobe profiler

kprobe 启用期间，Ftrace 会**给这些事件计数**，结果在 `kprobe_profile`：

```bash
# cat /sys/kernel/debug/tracing/kprobe_profile
  p_blk_account_io_start_bcc_19454                        1808               0
  p_blk_mq_start_request_bcc_19454                         677               0
  p_blk_account_io_completion_bcc_19454                    521              11
  p_kbd_event_1_bcc_1119                                   632               0
```

> The columns are: the probe name (its definition can be seen by printing the kprobe_events file), the hit count, and the miss-hits count (where the probe was hit but then an error was encountered and it wasn’t recorded: it was missed).

三列：探针名 / **命中数** / **miss-hits 数**（探针被触发了，但之后出错、没被记录下来的次数）。

**用途不是统计自己的探针，而是审计别人的**：原文说他用它检查监控软件**常驻挂着的 kprobe**
是不是触发太频繁、能不能关掉。**判据：`miss-hits` 那列非零说明探针本身有问题**
（比如探的函数在被优化掉的路径上）。

## §14.7 uprobes：用户态的动态插桩

**机制**：uprobes 是**用户态动态插桩**，控制文件是 `uprobe_events`，与 Ftrace 共享同一套
tracefs 输出与控制文件。语法：

```
  p[:[GRP/]EVENT] PATH:OFFSET [FETCHARGS] : Set a uprobe
  r[:[GRP/]EVENT] PATH:OFFSET [FETCHARGS] : Set a return uprobe (uretprobe)
  -:[GRP/]EVENT                           : Clear uprobe or uretprobe event
```

### §14.7.1 事件追踪：符号偏移要你自己算

> The syntax now requires a path and an offset for the uprobe. The kernel does not have symbol information for user-space software, so this offset must be determined and provided to the kernel using user-space tools.

**这是 uprobes 与 kprobes 的根本不对称**：内核有内核符号表，所以 kprobe 可以直接写函数名；
**内核没有用户态符号信息**，uprobe 必须给「路径 + 偏移」，而且这个偏移**只能由用户态工具算出来**。

原文用 `readelf(1)` 演示：

```bash
# readelf -s /bin/bash | grep -w readline
   882: 00000000000b61e0   153 FUNC    GLOBAL DEFAULT   14 readline
# echo 'p:brendan /bin/bash:0xb61e0' >> uprobe_events
# echo 1 > events/uprobes/brendan/enable
# cat trace_pipe
            bash-3970  [000] d... 347549.225818: brendan: (0x55d0857b71e0)
...
^C
# echo 0 > events/uprobes/brendan/enable
# echo '-:brendan' >> uprobe_events
```

**原文随后是一条 WARNING，本篇认为它是全章最该被记住的操作红线**：

> WARNING: If you mistakenly use a symbol offset that is midway through an instruction, you will corrupt the target process (and for shared instruction text, all processes that share it!). The example technique of using readelf(1) to find the symbol offset may not work if the target binary has been compiled as a position-independent executable (PIE) with address space layout randomization (ASLR). I do not recommend that you use this interface at all: switch to a higher-level tracer that takes care of symbol mappings for you (e.g., BCC or bpftrace).

拆开看有三层：

1. **偏移算错 → 破坏目标进程**（落在一条指令中间）；共享指令页时**连带所有共享该页的进程**一起坏。
2. **`readelf` 找偏移的方法对 PIE + ASLR 的二进制无效**——运行时地址是随机化的，静态偏移对不上。
3. **作者本人不建议用这个接口**，建议换 BCC / bpftrace 这种**替你处理符号映射**的高层 tracer。

**判据：需要用户态函数的参数时，直接上 BCC / bpftrace（第 15 章），不要手算 uprobe 偏移。**

### §14.7.2 参数与返回值

与 kprobes 同构：创建 uprobe 时指定即可，语法见内核源码 `uprobetracer.rst`。

### §14.7.3 过滤与触发

与 kprobes 同构，操作目录 `events/uprobes/...`。

### §14.7.4 uprobe profiler

计数在 `uprobe_profile`：

```bash
# cat /sys/kernel/debug/tracing/uprobe_profile
  /bin/bash brendan                                                   11
```

三列：**路径** / **探针名** / **命中数**（比 kprobe_profile 少一列 miss-hits）。

## §14.8 function_graph：把调用图画出来

**机制**：function_graph tracer **打印函数的调用图**，揭示代码流。本章开头 `funcgraph(8)` 的例子
就是它（来自 perf-tools），这里讲 tracefs 界面。

初始状态同样要先确认没有别的 tracer 在跑：

```bash
# cd /sys/kernel/debug/tracing
# cat set_graph_function
#### all functions enabled ####
# cat current_tracer
nop
```

### §14.8.1 图追踪

```bash
# echo do_nanosleep > set_graph_function
# echo function_graph > current_tracer
# cat trace_pipe
 1)   2.731 us    |  get_xsave_addr();
 1)               |  do_nanosleep() {
 1)               |    hrtimer_start_range_ns() {
 1)               |      lock_hrtimer_base.isra.0() {
 1)   0.297 us    |        _raw_spin_lock_irqsave();
 1)   0.843 us    |      }
 1)   0.276 us    |      ktime_get();
 1)   0.340 us    |      get_nohz_timer_target();
 1)   0.474 us    |      enqueue_hrtimer();
 1)   0.339 us    |      _raw_spin_unlock_irqrestore();
 1)   4.438 us    |    }
 1)               |    schedule() {
...
 5) $ 1000383 us  |  } /* do_nanosleep */
^C
# echo nop > current_tracer
# echo > set_graph_function
```

左列是 **CPU 编号**（这里是 5）和**函数内耗时**；括号缩进表示**调用层级**。

**高延迟用特殊字符标记**，这是 function_graph 最有用的可读性设计：

| 标记 | 含义 |
|---|---|
| `$` | 大于 1 秒 |
| `@` | 大于 100 毫秒 |
| `*` | 大于 10 毫秒 |
| `#` | 大于 1 毫秒 |
| `!` | 大于 100 微秒 |
| `+` | 大于 10 微秒 |

**一眼扫过去就能定位尖刺**，不用自己算阈值。

### 一个必须知道的权衡：filter 会改变时长

> This example deliberately did not set a function filter (set_ftrace_filter), so that all child calls can be seen. This does, however, cost some overhead, inflating the reported durations. It is still generally useful for locating the origin of high latencies, which can dwarf the added overhead.

原文给了**同一个负载、同一个函数**的两次读数：

| 追踪方式 | `do_nanosleep()` 报告时长 |
|---|---|
| 不设 filter（看全部子调用） | 1000383 μs |
| 设 `set_ftrace_filter`（只追这一个函数） | 1000130 μs |

差值就是追踪开销本身（约 253 μs）。**判据**：

- **定位延迟来源** → 不设 filter，全部子调用都要看（开销会被「大延迟 dwarfs」掉）；
- **要精确的函数耗时** → 设 filter，把追踪范围缩到目标函数。

这两件事用同一套接口，但**读数含义不同**，不能混着报。

### §14.8.2 Options

```bash
# ls options/funcgraph-*
options/funcgraph-abstime   options/funcgraph-irqs      options/funcgraph-proc
options/funcgraph-cpu       options/funcgraph-overhead  options/funcgraph-tail
options/funcgraph-duration  options/funcgraph-overrun
```

分别控制：绝对时间、是否显示中断状态、进程名、CPU ID、函数耗时、overhead 标记、调用尾部、overrun 标记。

## §14.9 hwlat：抓那些内核看不见的硬件扰动

**机制**：hwlat 是**专用 tracer**，检测**外部硬件事件对 CPU 性能的扰动**——那些内核和其它工具
根本看不见的事件：

> The hardware latency detector (hwlat) is an example of a special-purpose tracer. It can detect when external hardware events perturb CPU performance: events that are otherwise invisible to the kernel and other tools.

原文举的例子：**SMI（系统管理中断）**和**虚拟化扰动（包括 noisy neighbor 邻居干扰）**。

**做法是「拿实验当探针」**：**关中断跑一个代码循环**，测每次迭代耗时；
一次只在一个 CPU 上跑，**轮流换 CPU**；**超过阈值**（10 微秒，用 `tracing_thresh` 可配）才打印
每个 CPU 上最慢的那次。

```bash
# cd /sys/kernel/debug/tracing
# echo hwlat > current_tracer
# cat trace_pipe
           <...>-5820  [001] d... 354016.973699: #1     inner/outer(us): 2152/1933
ts:1578801212.559595228
           <...>-5820  [000] d... 354017.985568: #2     inner/outer(us):   19/26
ts:1578801213.571460991
           <...>-5820  [001] dn.. 354019.009489: #3     inner/outer(us): 1699/5894
ts:1578801214.595380588
...
^C
# echo nop > current_tracer
```

**读法**：序号 `#N` 之后是 `inner/outer(us)`——**inner = 循环体内部的耗时，outer = 绕到下一轮的控制逻辑耗时**；
末尾还有一个绝对时间戳。示例第一行 inner 2152 μs、outer 1933 μs，**远超 10 μs 阈值 ⇒ 外部扰动**。

参数：循环跑一个称为 **width** 的时长，每 **window** 周期里跑一次 width 实验；
两者都在 `hwlat_detector/` 下的 `width`、`window` 文件里配置，单位微秒。

**但它不是观测工具**：

> WARNING: I’d classify hwlat as a microbenchmark tool rather than an observability tool, because it performs an experiment that will itself perturb the system: it will make one CPU busy for the width duration, with interrupts disabled.

**判据**：hwlat **自己就是扰动源**——它会让一个 CPU 在整个 width 期间忙着跑循环、而且关着中断。
所以它是**微基准**，不是在线观测；**不能在生产负载上随手开**，也不能用它去测「关中断时长」
（它自己就在关中断）。

## §14.10 hist triggers：在内核里做自己的直方图

**机制**：hist triggers 是 Linux 4.7 引入的高级能力（作者 Tom Zanussi），**在事件上创建自定义直方图**：

> Hist triggers is an advanced Ftrace capability added to Linux 4.7 by Tom Zanussi, which allows the creation of custom histograms on events. It is another form of statistical summary, allowing counts to be broken down by one or more components.

**注意最后一句**：它是「另一种统计摘要」——也就是说，hist triggers 站在 **profiler 那一轴**上，
但它**建在 tracer 的事件之上**。这就是它被安排在 14.10 的原因。

### 生命周期：四步

```
echo 'hist: expression ' > events/.../trigger : Create a hist trigger.

sleep duration : Allow histogram to populate.

cat events/.../hist : Print the histogram.

echo '!hist: expression ' > events/.../trigger : Remove it.
```

**`!hist:` 是删除前缀**，和 `kprobe_events` 的 `-:name` 一样是「用特殊语法追加」。

表达式全貌：

```
hist:keys=<field1[,field2,...]>[:values=<field1[,field2,...]>]
  [:sort=<field1[,field2,...]>][:size=#entries][:pause][:continue]
  [:clear][:name=histname1][:<handler>.<action>] [if <filter>]
```

### §14.10.1 单键：按 PID 统计系统调用

```bash
# cd /sys/kernel/debug/tracing
# echo 'hist:key=common_pid' > events/raw_syscalls/sys_enter/trigger
# sleep 10
# cat events/raw_syscalls/sys_enter/hist
# event histogram
#
# trigger info: hist:keys=common_pid.execname:vals=hitcount:sort=hitcount:size=2048
[active]
#
{ common_pid:        347 } hitcount:          1
{ common_pid:      32396 } hitcount:     882604

Totals:
    Hits: 883372
    Entries: 12
    Dropped: 0
# echo '!hist:key=common_pid' > events/raw_syscalls/sys_enter/trigger
```

**尾部三行统计是判断数据可不可信的唯一依据**：

> The final few lines show statistics: the number of writes to the hash ( Hits ), the entries in the hash ( Entries ), and how many writes were dropped if the entries exceeded the hash size ( Dropped ). If drops occur, you can increase the size of the hash when declaring it; it defaults to 2048.

| 字段 | 含义 | 怎么用 |
|---|---|---|
| `Hits` | 写入哈希的次数 | 实际事件总数 |
| `Entries` | 哈希条目数 | **接近 size 就说明要溢出了** |
| `Dropped` | 因超出哈希大小被丢弃的写次数 | **非 0 ⇒ 数据缺失，必须重配** |

**判据：看到 `Dropped` 非 0 就不要用这份直方图下结论**——加 `:size=` 加大哈希重跑。
默认 size 是 2048。

### §14.10.2 字段：键从 format 文件取

字段来自事件的 `format` 文件：

```bash
# cat events/raw_syscalls/sys_enter/format
...
        field:int common_pid;       offset:4;  size:4;    signed:1;

        field:long id;   offset:8;    size:8;    signed:1;
        field:unsigned long args[6];    offset:16;   size:48;  signed:0;
```

把 `id`（syscall ID）当键：

```bash
# echo 'hist:key=id' > events/raw_syscalls/sys_enter/trigger
# cat events/raw_syscalls/sys_enter/hist
{ id:         14 } hitcount:         48
{ id:          1 } hitcount:      80362
{ id:          0 } hitcount:      80396
...
```

拿到 ID 后还要**翻译成名字**——对照系统头文件：

```bash
# more /usr/include/x86_64-linux-gnu/asm/unistd_64.h
...
#define __NR_read 0
#define __NR_write 1
...
```

### §14.10.3 修饰符：让输出自带名字

> Since PID and syscall ID breakdowns are common, hist triggers supports modifiers that annotate the output: .execname for PIDs, and .syscall for syscall IDs.

```bash
# echo 'hist:key=common_pid.execname' > events/raw_syscalls/sys_enter/trigger
{ common_pid: bash            [     32379] } hitcount:        166
{ common_pid: dd              [     32396] } hitcount:     869024
...
```

**两个修饰符**：`.execname` 给 PID 附进程名、`.syscall` 给 syscall ID 附名字。
它们是**纯展示层**的——不改统计口径，只让输出不用再手工查表。

### §14.10.4 PID 过滤：把两个维度拼起来

用 `if` 把 PID 作为条件：

```bash
# echo 'hist:key=id.syscall if common_pid==32396' >> \
    events/raw_syscalls/sys_enter/trigger
# cat events/raw_syscalls/sys_enter/hist
{ id: sys_write                     [  1] } hitcount:     106425
{ id: sys_read                      [  0] } hitcount:     106425
```

**这一步的价值**：看到「PID 维度有 dd、syscall 维度有 read/write」只能推出**猜想**；
加上 `if common_pid==32396` 之后**猜想被证实**——dd 确实在做 read/write。
**判据：维度之间的关联要用 filter 证实，不能用并列的直方图推。**

### §14.10.5 多键：交叉维度

```bash
# echo 'hist:key=common_pid.execname,id' > events/raw_syscalls/sys_enter/trigger
# sleep 10
# cat events/raw_syscalls/sys_enter/hist
{ common_pid: sshd            [   14250], id:         23 } hitcount:         36
{ common_pid: bash            [   14261], id:         13 } hitcount:         42
{ common_pid: dd              [   14325], id:          0 } hitcount:    9195176
{ common_pid: dd              [   14325], id:          1 } hitcount:    9195176
```

**一次拿到「进程 × syscall」的交叉表**，比 §14.10.4 的「先看再 filter」一步到位。

### §14.10.6 栈回溯键：按调用路径聚合计数

> I frequently wish to know the code path that led to the event, and I suggested that Tom Zanussi add functionality for Ftrace to use an entire kernel stack trace as a key.

```bash
# echo 'hist:key=stacktrace' > events/block/block_rq_issue/trigger
# sleep 10
# cat events/block/block_rq_issue/hist
{ stacktrace:
         nvme_queue_rq+0x16c/0x1d0
         __blk_mq_try_issue_directly+0x116/0x1c0
         blk_mq_request_issue_directly+0x4b/0xe0
         ...
         new_sync_read+0x122/0x1b0
} hitcount:        266
```

**这是 hist triggers 最被低估的能力**：不只统计「发生了什么」，而是统计
「**通过哪条代码路径发生的**」——每条栈一个键，命中数就是这条路径的频次。

**这正好补上 §14.10.1–§14.10.5 的短板**：直方图按 PID / syscall / 栈分组，
本质上是把「多维计数」下沉到内核里做，不用导出全部事件到用户态。

### §14.10.7 合成事件：直方图变成可编程的

> A synthetic event can be created that is triggered by other events, and can combine their event arguments in custom ways. To access event arguments from prior events, they can be saved into a histogram and fetched by the later synthetic event.

**这是全章最强的机制**。原文的完整例子（计算所有系统调用的延迟）：

```bash
# cd /sys/kernel/debug/tracing
# echo 'syscall_latency u64 lat_us; long id' >> synthetic_events
# echo 'hist:keys=common_pid:ts0=common_timestamp.usecs' >> \
    events/raw_syscalls/sys_enter/trigger
# echo 'hist:keys=common_pid:lat_us=common_timestamp.usecs-$ts0:'\
    'onmatch(raw_syscalls.sys_enter).trace(syscall_latency,$lat_us,id)' >>\
    events/raw_syscalls/sys_exit/trigger
# echo 'hist:keys=lat_us,id.syscall:sort=lat_us' >> \
    events/synthetic/syscall_latency/trigger
# sleep 10
# cat events/synthetic/syscall_latency/hist
{ lat_us:    5779085, id: sys_epoll_wait                [232] } hitcount:          1
{ lat_us:   10000097, id: sys_nanosleep                 [  35] } hitcount:          1
{ lat_us:   10002176, id: sys_select                    [  23] } hitcount:          1
...
```

**机制拆解（原书 Table 14.4 逐条解释）**：

| 步骤 | 做了什么 |
|---|---|
| `synthetic_events` | 定义一个合成事件 `syscall_latency`，带两个字段 `lat_us`、`id` |
| sys_enter 上的 hist | 以 `common_pid` 为键，把当前时间戳（微秒）存进直方图变量 `ts0` |
| sys_exit 上的 hist | 同样以 `common_pid` 为键，计算 `lat_us = 当前时间戳 - ts0` |
| `onmatch(raw_syscalls.sys_enter)` | **比对两个事件的直方图键**：键相同（同一 PID）才说明这次延迟算对了 |
| `.trace(syscall_latency,$lat_us,id)` | 触发合成事件，把算好的延迟和 syscall ID 传进去 |
| 合成事件上的 hist | 把合成事件本身做成按 `lat_us` + `id.syscall` 排序的直方图 |

**为什么这算「编程」**：

> Ftrace histograms are implemented as a hash object (key/value store), and the earlier examples only used these hashes for output: showing syscall counts by PID and ID. With synthetic events, we’re doing two extra things with these hashes: A) storing values that are not part of the output (timestamps) and B) in one event, fetching key/value pairs that were set by another event. We’re also performing arithmetic: a minus operation. In a way, we’re starting to write mini programs.

**三个升级动作**：① 存**不属于输出**的值（时间戳）；② 跨事件**取**别的事件设的键值对；
③ 在内核里**做算术**（减法）。所以 hash 不再是计数器，而是**变量存储**。

作者的态度也很直接：

> Yes, I now do need to explore adopting synthetic events for some use cases. It’s incredibly powerful, built into the kernel, and can be used via shell scripting alone.

**代价与必须处理的细节**：

> The output is also very long because it is recording a key for every microsecond and syscall ID combination, and in practice I’ve exceeded the default hist size of 2048.

**每个微秒 × 每个 syscall ID 就是一个键** ⇒ 条目爆炸，必然超过默认 size=2048。两种解法：
**`:size=...` 加大哈希**，或用 **`.log2` 修饰符把延迟按 2 的幂记录**——后者条目数大减，
分辨率仍够做延迟分析。

**清理**：原文一句话——

> To disable and clean up this event, echo all the strings in reverse order with a “!” prefix.

**逆序**、每条加 `!` 前缀，再写回原来那些 trigger / synthetic_events 文件。


## §14.11 trace-cmd：把 /sys 变成命令

**机制**：trace-cmd 是**开源的 Ftrace 前端**（Steven Rostedt 等人开发）。它有子命令、有选项、
有二进制输出格式。事件源包括 Ftrace 的 function 与 function_graph tracer，以及 tracepoint
和**已经配置好**的 kprobe / uprobe。

用法上它比裸写 `/sys` 简洁，而且**更安全**：

> The output begins with the sleep(1) invoked by trace-cmd (it configures tracing and then launches the provided command), and then various calls from multipathd PID 348. This example also shows that trace-cmd is more concise than the equivalent tracefs commands in /sys. It is also safer: many subcommands handle cleaning up tracing state when done.

**「子命令会负责清理追踪状态」是它最大的安全优势**——手写 `/sys` 时忘了
`current_tracer` 设回 `nop`、忘了清 filter，追踪状态会一直挂着。

```bash
# trace-cmd record -p function -l do_nanosleep sleep 10
  plugin 'function'
CPU0 data recorded at offset=0x4fe000
    0 bytes in size
CPU1 data recorded at offset=0x4fe000
    4096 bytes in size
# trace-cmd report
cpus=2
           sleep-21145 [001] 573259.213076: function:             do_nanosleep
      multipathd-348   [001] 573259.523759: function:             do_nanosleep
...
```

注意 `record` **先配置追踪再启动命令**，所以被追的命令本身也会出现在结果里。

### §14.11.1 子命令

| 命令 | 作用 |
|---|---|
| `record` | 追踪并录到 trace.dat |
| `report` | 读 trace.dat |
| `stream` | 追踪并直接打印到 stdout |
| `list` | 列出可用事件 |
| `stat` | 显示内核追踪子系统状态 |
| `profile` | 追踪并生成内核时间与延迟报告 |
| `listen` | 接受网络上的追踪请求 |

另有 `start` / `stop` / `restart` / `clear` 用于跨多次 `record` 的长期追踪。

`record` 的常用选项（原文 `trace-cmd record -h` 输出的前 12 项，共 35 项）：

```bash
trace-cmd record [-v][-e event [-f filter]][-p plugin][-F][-d][-D][-o file] \
           [-q][-s usecs][-O option ][-l func][-g func][-n func] \
           [-P pid][-N host:port][-t][-r prio][-b size][-B buf][command ...]
          -e run command with event enabled
          -f filter for previous -e event
          -p run command with plugin enabled
          -F filter only on the given process
          -P trace the given pid like -F for the command
          -c also trace the children of -F (or -P if kernel supports it)
          -C set the trace clock
          -T do a stacktrace on all events
          -l filter function name
          -g set graph function
          -n do not trace function
```

**注意 `-p` = plugin，而原文明确说 plugin 指的就是 Ftrace tracer**（function、function_graph、hwlat）。
另外 `-e` 后可跟 `-f`（给上一个事件加过滤）、`-R`（给上一个事件加 trigger）——**§14.5.1 / §14.5.2
的能力被提升成了命令行选项**。

### §14.11.2 One-liners

原文按用途给了成套命令（**用的时候直接照抄，别自己拼**）：

- **Listing Events**：`trace-cmd list` / `list -t`（tracer）/ `list -e`（事件源）/ `list -e syscalls:` / `list -e syscalls:sys_enter_nanosleep -F`（看 format）
- **Function Tracing**：`record -p function -l function_name`；`-l 'tcp_*'` 全系统；加 `sleep 10` 限时；`-F ls` 只追某进程；`-F -c bash` 连子进程；`-P 21124` 指定 PID
- **Function Graph Tracing**：`record -p function_graph -g function_name`
- **Event Tracing**：`record -e sched:sched_process_exec`；`-e block_rq_issue -T`（带内核栈）；`-e block`（整组）；`-e probe:brendan`（已建好的 kprobe）；`-e syscalls -F ls`
- **Reporting**：`trace-cmd report` / `report --cpu 0`
- **Other Capabilities**：`-p sched_switch`（插件）、`listen -p 8081`（网络收请求）、`-N addr:port`（远程执行）

### §14.11.3 trace-cmd vs. perf(1)

**Table 14.6 逐行对照**（本篇整理）：

| 属性 | perf(1) | trace-cmd |
|---|---|---|
| 二进制输出文件 | `perf.data` | `trace.dat` |
| Tracepoints | 支持 | 支持 |
| kprobes | 支持 | **部分**（只能用已由别的方式创建好的事件） |
| uprobes | 支持 | **部分** |
| USDT | 支持 | **部分** |
| PMCs（硬件计数器） | **支持** | **不支持** |
| 定时采样 | **支持** | **不支持** |
| function tracing | 部分（经 ftrace 子命令） | **支持** |
| function_graph tracing | 部分 | **支持** |
| 网络 client/server | 不支持 | **支持** |
| 输出文件开销 | 低 | **极低** |
| 前端 | 多种 | KernelShark |
| 源码位置 | Linux 源码 `tools/perf` | git.kernel.org |

> One advantage of trace-cmd is its better support for the function and function_graph tracers.

**分工一句话（判据）**：**要 PMC 和定时采样 → perf(1)；要内核函数级追踪与调用图 → trace-cmd。**
两者都能做 tracepoint 追踪，所以「二选一」的战场不在 tracepoint 上。

### §14.11.4 trace-cmd function_graph

```bash
# trace-cmd record -p function_graph -g do_nanosleep sleep 10
  plugin 'function_graph'
CPU0 data recorded at offset=0x4fe000
    12288 bytes in size
CPU1 data recorded at offset=0x501000
    45056 bytes in size
# trace-cmd report | cut -c 66-

              |  do_nanosleep() {
              |    hrtimer_start_range_ns() {
              |      lock_hrtimer_base.isra.0() {
   0.250 us   |        _raw_spin_lock_irqsave();
   0.688 us   |      }
   0.190 us   |      ktime_get();
```

**`cut -c 66-` 是原文给的一个实用技巧**：trace-cmd 的报告前面带一大堆追踪字段，
裁掉前 65 列就只剩**调用图 + 耗时**这两列要看的东西。

### §14.11.5 KernelShark

KernelShark 是 trace-cmd 输出文件的**可视化前端**，由 Ftrace 的作者本人创建，
后由 Yordan Karadzhov 用 Qt 重写（**1.0 是 Qt 版，0.99 及更早是 GTK 版**）。

```bash
# trace-cmd record -e 'sched:*'
# kernelshark
```

- 上半屏是**每 CPU 时间线**，不同任务用不同颜色；
- 下半屏是**事件表**；
- 交互：**点击并向右拖 = 放大**所选时间范围，**向左拖 = 缩小**；
  **右键事件**可以设置过滤等动作；
- 读默认的 `trace.dat`（可用 `-i` 指定别的文件）。

**用途定位（原文一句话）**：

> KernelShark can be used to identify performance issues caused by the interaction between different threads.

**判据：怀疑「多线程互相拖累」（锁竞争、优先级反转、调度延迟）时，KernelShark 的
每 CPU 时间线是最直接的证据形式**——单线程火焰图看不出「谁在等谁」。

### §14.11.6 文档

包安装后有 `trace-cmd(1)` 及 `trace-cmd-record(1)` 等 man page，源码里也在 `Documentation` 目录；
作者还推荐了维护者 Steven Rostedt 讲 Ftrace 与 trace-cmd 的演讲。

## §14.12 perf ftrace：perf(1) 的薄包装

perf(1) 有一个 `ftrace` 子命令，能用 function 与 function_graph tracer：

```bash
# perf ftrace -T do_nanosleep -a sleep 10
 0)  sleep-22821   |               |  do_nanosleep() {
 1)  multipa-348   | $ 1000068 us  |  }
...

# perf ftrace -G do_nanosleep -a sleep 10
 1)  sleep-22828   |               |    smp_irq_work_interrupt() {
 1)  sleep-22828   |   0.258 us    |        rcu_irq_enter();
 1)  sleep-22828   |   0.800 us    |      }
...
```

**`-T` = function tracer，`-G` = function_graph**；`-p` 可以匹配 PID。

**但它的定位必须说清楚**：

> The ftrace subcommand supports a few options including -p to match on a PID. This is a simple wrapper that does not integrate with other perf(1) capabilities: for example, it prints the trace output to stdout and does not use the perf.data file.

**「薄包装」的后果**：输出直接打到 stdout、**不落 perf.data**，所以
**没法用 perf report / perf script 后续分析**，也没法和 perf 的其它子命令共享同一次采集。
**判据：要 perf.data 就别用 ftrace 子命令，要「看一眼」用它最省事。**

## §14.13 perf-tools：把 Ftrace 封装成 Unix 工具

**机制**：perf-tools 是 Brendan Gregg 自己写的**开源高级性能分析工具集**，
基于 Ftrace 与 perf(1)，**Netflix 服务器默认安装**。设计目标是**易装（少依赖）+ 简单用**：

> perf-tools is an open-source collection of Ftrace- and perf(1)-based advanced performance analysis tools developed by myself and installed by default on servers at Netflix [Gregg 20i] . I designed these tools to be easy to install (few dependencies) and simple to use: each should do one thing and do it well.

**实现方式决定了它的价值**：perf-tools 本身**大多是 shell 脚本**，自动化地设置 tracefs 的 `/sys` 文件——
所以「Ftrace 零依赖」这条性质一路传导到了工具层。

### §14.13.1 工具覆盖范围

原文用 Figure 14.4 画了一张覆盖图：**单箭头**的工具是单用途（针对一个系统区域），
**双箭头**的是多用途（横跨多个区域）。

### §14.13.2 单用途工具

**Table 14.7**（本篇整理）：

| 工具 | 底层 | 作用 |
|---|---|---|
| `bitesize(8)` | perf | 磁盘 I/O 大小直方图 |
| `cachestat(8)` | Ftrace | page cache 命中/未命中统计 |
| `execsnoop(8)` | Ftrace | 追踪新进程（execve(2)）及其参数 |
| `iolatency(8)` | Ftrace | 磁盘 I/O 延迟直方图 |
| `iosnoop(8)` | Ftrace | 磁盘 I/O 详情（含延迟） |
| `killsnoop(8)` | Ftrace | kill(2) 信号追踪（进程与信号详情） |
| `opensnoop(8)` | Ftrace | open(2) 族系统调用追踪（文件名） |
| `tcpretrans(8)` | Ftrace | TCP 重传追踪（地址与内核状态） |

**设计哲学**：

> The single-purpose tools such as execsnoop(8) do one job and do it well (Unix philosophy). This design includes making their default output concise and often sufficient, which helps aid learning.

**「默认输出简洁且通常就够用」是关键**：不用先学命令行选项，直接跑就能拿到够用的信息；
选项是给定制用的，不是给入门用的。

`execsnoop(8)` 的输出（顺带说明它能干什么）：

```bash
# execsnoop
Tracing exec()s. Ctrl-C to end.
   PID   PPID ARGS
  6684   6682 cat -v trace_pipe
  6685  20997 man ls
  6695   6685 pager
  6691   6685 preconv -e UTF-8
  6693   6685 nroff -mandoc -rLL=148n -rLT=148n -Tutf8
...
```

**用途是抓「转瞬即逝的进程」**——这些进程短到别的工具根本看不见。

`iolatency(8)` 的输出是**分桶直方图**：

```bash
# iolatency
Tracing block I/O. Output every 1 seconds. Ctrl-C to end.

  >=(ms) .. <(ms)   : I/O      |Distribution                          |
       0 -> 1       : 731      |######################################|
       1 -> 2       : 318      |#################                     |
       2 -> 4       : 160      |#########                             |
```

### 这段为什么值得单列：用户态聚合的天花板

> The way I implemented this helps to explain the need for extended BPF. iolatency(8) traces block I/O issue and completion tracepoints, reads all events in user-space, parses them, and post-processes them into these histograms using awk(1). Since disk I/O has a relatively low frequency on most servers, this approach was possible without onerous overhead. But the overhead would be prohibitive for more frequent events, such as network I/O or scheduling.

**机制拆解**：iolatency 追 issue 与 completion **两个** tracepoint → **把全部事件读进用户态** →
**解析** → **用 awk 后处理**成直方图。

**为什么当时可行**：磁盘 I/O 频率低。
**为什么后来不可行**：换成网络 I/O 或调度事件，频率高一个量级，
「全部事件过用户态」这条路径的开销就**不可接受**。

**extended BPF 的解法**：**在内核态算好直方图，只把摘要传到用户态**。

**Ftrace 现在也有类似能力**（原文自己点的）：

> Ftrace now supports some similar capabilities with hist triggers and synthetic events

**这就是 §14.10 的意义所在**——hist triggers + synthetic events 把「在内核态做聚合」
的能力搬进了 Ftrace。**判据：需要在高频事件上做自定义分组统计时，用 hist triggers（§14.10）
而不是「导出到用户态再 awk」**。

### §14.13.3 多用途工具

**Table 14.8**（本篇整理）：

| 工具 | 底层 | 作用 |
|---|---|---|
| `funccount(8)` | Ftrace | 统计内核函数调用次数 |
| `funcgraph(8)` | Ftrace | 追踪内核函数并显示子调用代码流 |
| `functrace(8)` | Ftrace | 追踪内核函数 |
| `funcslower(8)` | Ftrace | 追踪慢于阈值的内核函数 |
| `kprobe(8)` | Ftrace | 内核函数动态追踪 |
| `perf-stat-hist(8)` | perf(1) | tracepoint 参数的自定义 2 的幂聚合 |
| `syscount(8)` | perf(1) | 汇总系统调用 |
| `tpoint(8)` | Ftrace | 追踪 tracepoint |
| `uprobe(8)` | Ftrace | 用户态函数动态追踪 |

**多用途工具支持多种事件源、能扮演多个角色**，类似 perf(1) 与 trace-cmd——但也**因此更难用**。

### §14.13.4 One-liners

原文把命令按四组给（用的时候直接照抄）：

**Ftrace Profilers**

```bash
funccount 'tcp_*'                  # 统计所有内核 TCP 函数
funccount -t 10 -i 1 'vfs*'        # 每 1 秒打印一次 top 10
```

**Ftrace Tracers**

```bash
funcgraph do_nanosleep             # 显示子调用
funcgraph -m 3 do_nanosleep        # 只展开 3 层
functrace -p 198 '*sleep'          # 只追 PID 198
funcslower vfs_read 10000          # 只报慢于 10 ms 的调用
```

**Event Tracing**

```bash
kprobe p:do_sys_open
kprobe 'r:do_sys_open $retval'
kprobe 'p:do_sys_open mode=$arg3:u16'
kprobe 'p:do_sys_open filename=+0($arg2):string'
kprobe 'p:do_sys_open file=+0($arg2):string' 'file ~ "*stat"'
kprobe -s p:tcp_retransmit_skb
tpoint -l
tpoint -s block:block_rq_issue
uprobe p:bash:readline
uprobe 'r:bash:readline +0($retval):string'
uprobe 'r:libc:fopen file=$retval' 'file == 0'
```

**注意 `kprobe` 的参数能力比裸 kprobe_events 更强**：`:u16` / `:string` 是**类型标注**，
`+0($arg2):string` 是**解引用取字符串**，`'file ~ "*stat"'` 是**在工具层直接带过滤条件**——
这些在 §14.6 里要手写更长的表达式。

**CPU Registers**（原文这一节专门讲寄存器名）

> Function argument aliases ($arg1, ..., $argN) is a newer Ftrace capability (Linux 4.20+). For older kernels (or processor architectures missing the aliases), you will need to use CPU register names instead

调用约定查 `man 2 syscall`：

```bash
$ man 2 syscall
       Arch/ABI      arg1  arg2  arg3  arg4  arg5  arg6  arg7  Notes
       ──────────────────────────────────────────────────────────────
       sparc/32      o0    o1    o2    o3    o4    o5    -
       tile          R00   R01   R02   R03   R04   R05   -
       x86-64        rdi   rsi   rdx   r10   r8    r9    -
       x32           rdi   rsi   rdx   r10   r8    r9    -
```

**这一节的实际价值**：老内核或非 x86 架构上，写 kprobe 参数必须知道**前 6 个参数分别在哪个寄存器**。

### §14.13.5 一个例子

```bash
# funccount 'vfs_*'
Tracing "vfs_*"... Ctrl-C to end.
^C
FUNC                              COUNT
vfs_fsync_range                      10
vfs_statfs                           10
vfs_readlink                         35
vfs_statx                           673
vfs_write                           782
vfs_statx_fd                        922
vfs_open                            1003
vfs_getattr                         1390
vfs_getattr_nosec                   1390
vfs_read                            2604
```

> I regularly use funccount(8) to determine which kernel functions are frequently called, and which are called at all. Since its overhead is relatively low, I can use it to check whether function call rates are low enough for more expensive tracing.

**这段把 §14.3 与 §14.4 的关系闭环了**：funccount 开销低 ⇒ 先用它确认「哪些函数被调用、调用多频繁」
⇒ 再判断哪些函数**频率低到可以承受更贵的追踪** ⇒ 只对那几个上 tracer。

### §14.13.6 perf-tools vs. BCC/BPF

perf-tools 诞生于 Netflix 跑 Linux 3.2（没有 extended BPF）的时候；
后来 Netflix 换新内核，作者把很多工具重写成了 BPF 版。BCC 和 bpftrace 都在第 15 章。

**但 perf-tools 仍然保留**，原文给了四条理由：

- **funccount(8)**：perf-tools 版用 **Ftrace 函数 profiling**，比 BCC 现在的 kprobe 版
  「much more efficient and less constrained」；
- **funcgraph(8)**：
  > funcgraph(8) : This tool does not exist in BCC, since it uses Ftrace function_graph tracing.
  —— 因为它用的是 Ftrace 的 function_graph，**BCC 里没有对应物**；
- **Hist Triggers**：将成为未来 perf-tools 的动力，比 kprobe 版 BPF 更高效；
- **Dependencies**：
  > Dependencies : perf-tools remain useful for resource-constrained environments (e.g., embedded Linux) as they typically only require a shell and awk(1).
  —— **只要 shell 和 awk**，所以在资源受限环境（嵌入式 Linux）里依然有用。

**还有一条实践**：作者有时**用 perf-tools 交叉验证 BPF 工具**。原文脚注里的那句话值得抄下来：

> A man with one tracer knows what events happened; a man with two tracers knows that one of them is broken, and searches lkml hoping for a patch.

### §14.13.7 文档

每个工具都有 usage 消息（`-h`）、man page、以及一个 **examples 文件**
（如 `funccount_example.txt`，里面是带说明的输出示例）。

```bash
# funccount -h
USAGE: funccount [-hT] [-i secs] [-d secs] [-t top] funcstring
                 -d seconds      # total duration of trace
                 -h              # this usage message
                 -i seconds      # interval summary
                 -t top          # show top num entries only
                 -T              # include timestamp (for -i)
```

## 章末选型判据

| 场景 | 用什么 | 别用什么 |
|---|---|---|
| 想知道某个内核函数被调多少次、每次多久 | `funccount 'tcp_*'` / 函数 profiler（§14.3） | 直接上 function tracer（§14.4）——开销高一个量级 |
| 想知道某个函数被谁调用、调用链长什么样 | `funcgraph` / function_graph tracer（§14.8） | 在高频函数上开 function tracer 逐条读 |
| 延迟尖刺（>1 ms）找元凶 | function_graph 的高延迟标记 + 不设 filter 看全部子调用（§14.8.1） | 直接读某个函数的时长——不设 filter 时时长被开销抬高（原文示例 1000383 → 1000130 μs） |
| 要某个函数的**精确**耗时 | 先 `set_ftrace_filter` 收窄再追（§14.8.1） | 用不设 filter 的读数当精确值 |
| 已知函数调用频率低于约 1000 次/秒，要逐事件明细 | function tracer + `trace` 文件（§14.4.1） | trace_pipe 实时刷——高频会刷爆终端 |
| 实时盯低频事件 | `trace_pipe`（§14.4.2） | `trace` 文件——要反复重读，且会被覆盖 |
| 想知道「为什么被调用」 | 开 `options/stacktrace` 与 `options/userstacktrace`（§14.4.3） | 只看单行事件 |
| 要内核函数的参数 / 返回值 | kprobe / kretprobe（§14.6.2 / §14.6.3），Linux 4.20+ 用 `$argN` | 手写寄存器名——除非老内核或缺别名的架构（§14.13.4） |
| 要用户态函数的参数 | BCC / bpftrace（第 15 章） | 手算 uprobe 偏移——偏移落在指令中间会**破坏目标进程**（§14.7.1） |
| 要按维度分组的计数（谁、哪个 syscall、哪条栈） | hist triggers：`key=`、`.execname`、`if` 过滤、多键、`key=stacktrace`（§14.10） | 把全部事件导到用户态再 awk 聚合——网络/调度场景开销不可接受（§14.13.2） |
| 要自定义延迟分布（enter/exit 配对） | synthetic events（§14.10.7） | 默认 size=2048——每个微秒一个键必然溢出 |
| 直方图出现 `Dropped` 非 0 | 加 `:size=` 重跑，或用 `.log2` 修饰符（§14.10.1 / §14.10.7） | 直接拿这份数据下结论 |
| 想知道外部硬件扰动（SMI、noisy neighbor） | hwlat（§14.9），**在隔离时段跑** | 把 hwlat 当在线观测工具——它自己关中断占满一个 CPU |
| 怀疑多线程互相拖累 | trace-cmd + KernelShark 的每 CPU 时间线（§14.11.5） | 只看单线程火焰图 |
| 要 PMC 或定时采样 | perf(1)（第 13 章） | trace-cmd（§14.11.3：PMCs 与定时采样都不支持） |
| 要 perf.data 后续用 perf report 分析 | perf(1) | `perf ftrace` 子命令（§14.12：它只打到 stdout，不用 perf.data） |
| 生产机上要跟别人同时追踪 | 在 `instances/` 下建自己的实例（§14.2.1） | 直接写全局 `current_tracer`——一次只能设一个 tracer（§14.2.1） |
| 内核太老 / 嵌入式，装不了 BPF | perf-tools（只要 shell + awk，§14.13.6） | BPF 全家桶 |

**四条跨工具的通用规则**

1. **按成本从低到高往上爬，别跳级**：profiler（计数/直方图）→ tracer（逐事件）→ function_graph（调用图）。
   原文只给了一个明确门槛——**function tracing 适合每秒少于约 1000 次的函数**（§14.4），
   所以「先量频率、再决定追不追」是本章唯一量化的操作规则。
2. **过滤器先于 trace，但过滤器会改变你看到的东西**：`set_ftrace_filter` / `set_graph_function` /
   事件的 `filter` 都降开销，但也决定了你**能不能看到完整的子调用**（§14.8.1）。
   两者不是同一次实验——**定位来源**和**测精确耗时**要分两次追。
3. **退出必清理**：`current_tracer` 设回 `nop`、`set_ftrace_filter` 清空、
   事件 `enable` 写 0、`filter` 写 0、hist 写 `!hist:`、
   synthetic events **逆序**加 `!` 清理（§14.10.7）。**trace-cmd 的子命令会自动做这些**（§14.11）——
   这就是优先用前端的理由。
4. **先确认内核给不给这个能力，再排查用法**：tracefs 挂不上 → 没编 `CONFIG_FTRACE`（§14.2）；
   函数 profiler 要 `CONFIG_FUNCTION_PROFILER=y`（§14.3）；
   irqsoff / preemptoff / preemptirqsoff 要 `CONFIG_PREEMPTIRQ_EVENTS`（§14.1）；
   `$argN` 别名要 Linux 4.20+（§14.6.2）；hist triggers 要 Linux 4.7+（§14.10）。

