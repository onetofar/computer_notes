---
title: Chapter 13. perf（精读）
category: 性能工程
tags:
  - perf
  - 性能分析
  - 事件追踪
  - PMU
  - 火焰图
difficulty: 进阶
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）第13章 perf"
source_section: "精读 §13.1 / §13.2 / §13.3 / §13.4 / §13.4.1 / §13.5 / §13.6 / §13.7 / §13.7.1 / §13.7.2 / §13.7.3 / §13.8 / §13.8.1 / §13.8.2 / §13.8.3 / §13.8.4 / §13.8.5 / §13.9 / §13.9.1 / §13.9.2 / §13.9.3 / §13.10 / §13.10.1 / §13.10.2 / §13.11 / §13.11.1 / §13.11.2 / §13.12 / §13.12.1 / §13.13；§13.14 perf Documentation 与 §13.15 References 不立篇（只给 man page、wiki 教程与文献清单位置，不承载机制陈述）；§13.2 下的 Listing Events / Counting Events / Profiling / Static Tracing / Dynamic Tracing / Reporting 与 §13.7.1 下的 kprobe Arguments、§13.7.2 下的 uprobe Arguments 是原文无编号子标题，按原文文字引用，不编 § 号"
skills:
  - obsidian-markdown
---

# Chapter 13. perf

> **小节地图**：这一章是**工具本身**的手册——前面各章讲「用 perf 分析某个目标」，本章讲「perf 是什么、有哪些事件源、每个子命令在机制上做什么」。
> 立篇：§13.1（子命令模型）、§13.2（one-liners 能力清单）、§13.3（事件类型全景）、§13.4（硬件事件与可读名映射）、§13.4.1（频率采样）、§13.5（软件事件）、§13.6（tracepoint）、§13.7（probe events 总论）、§13.7.1（kprobes 与取参数）、§13.7.2（uprobes）、§13.7.3（USDT）、§13.8–§13.8.5（perf stat 及其选项/间隔/每 CPU/过滤器/shadow statistics）、§13.9–§13.9.3（perf record 及其选项/CPU profiling/栈回溯）、§13.10–§13.10.2（perf report 的 TUI 与 STDIO）、§13.11–§13.11.2（perf script、火焰图、trace scripts）、§13.12–§13.12.1（perf trace 与内核版本差异）、§13.13（其他命令与硬件追踪）。
> **不立篇**：§13.14 perf Documentation、§13.15 References（只给 man page / wiki 教程 / 文献清单位置）。
> 底层依赖：PMU 与 PMC 机制见 [[Chapter 6. CPUs]]，栈与调度器见 [[Chapter 3. Operating Systems]]，缺栈与 perf 的用例见 [[Chapter 5. Applications]]，block I/O 事件的下游见 [[Chapter 9. Disks]]，测量方法论见 [[Chapter 12. Benchmarking]]。
> 同层对照：[[Chapter 2. Methodologies]]（怎么问问题）、[[Chapter 7. Memory]]、[[Chapter 8. File Systems]]、[[Chapter 11. Cloud Computing]]。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| 章首定位 | 13-Chapter 13. perf.md L25 | perf(1) is the official Linux profiler and is in the Linux kernel source under tools/perf. 1 It is a multi-tool that has profiling, tracing, and scripting capabilities, and is the front-end to the kernel perf_events observability subsystem. perf_events is also known as Performance Counters for Linux (PCL) or Linux Performance Events (LPE). perf_events and the perf(1) front-end began with performance monitoring counter (PMC) capabilities, but have since grown to support event-based tracing sources as well: tracepoints, kprobes, uprobes, and USDT. |
| §13.1 子命令模型 | 13-Chapter 13. perf.md L83 | perf(1)’s capabilities are invoked via subcommands. As a common usage example, the following uses two subcommands: record to instrument events and save them to a file, and then report to summarize the contents of the file. These subcommands are explained in Section 13.9 , perf record , and Section 13.10 , perf report . |
| §13.3 事件类型 | 13-Chapter 13. perf.md L704 | The tracepoint and SDT events mostly list static instrumentation points, but if you have created some dynamic instrumentation probes, those will be listed as well. I included an example in the output: probe:do_nanosleep is described as a “Tracepoint event” that is based on a kprobe. |
| §13.4 硬件事件 | 13-Chapter 13. perf.md L740 | There are many processor types, and new versions are released regularly. It is possible that human-readable mappings for your processor are not yet available in perf(1) or are in a newer kernel version. Some PMCs may never be exposed via a human-readable name. I regularly have to switch from human-readable names to raw event descriptors once I move to deeper PMCs that lack mappings. There may also be bugs in the mappings, and if you ever encounter a suspicious PMC result, you may wish to try the raw event descriptor to double-check. |
| §13.4.1 频率采样 | 13-Chapter 13. perf.md L765 | The output shows that frequency sampling is enabled ( freq 1 ) with a sample frequency of 4000. This tells the kernel to adjust the rate of sampling so that roughly 4,000 events per second per CPU are captured. This is desirable, since some PMCs instrument events that can occur billions of times per second (e.g., CPU cycles) and the overhead of recording every event would be prohibitive. 3 But this is also a gotcha: the default output of perf(1) (without the very verbose option: -vv ) does not say that frequency sampling is in use, and you may be expecting to record all events. This event frequency only affects the record subcommand; stat counts all events. |
| §13.5 软件事件 | 13-Chapter 13. perf.md L830 | This time, period sampling is used (no freq 1 ), with a sample period of 1 (equivalent to -c 1 ). This captures every event. You can do the same with software events by specifying -c 1 , for example: |
| §13.7 探针事件 | 13-Chapter 13. perf.md L870 | perf(1) uses the term probe events to refer to kprobes, uprobes, and USDT probes. These are “dynamic” and must first be initialized before they can be traced: they are not present in the output of perf list by default (some USDT probes may be present because they have been auto-initialized). Once initialized, they are listed as “Tracepoint event.” |
| §13.7.1 kprobes | 13-Chapter 13. perf.md L927 | There are at least four different ways to instrument the arguments to kernel functions. |
| §13.7.1 kprobes | 13-Chapter 13. perf.md L978 | This only works if the systems have the same processor ABI and kernel versions, otherwise the wrong register locations may be instrumented. |
| §13.7.2 uprobes | 13-Chapter 13. perf.md L1102 | While uprobes can provide visibility into application internals, they are an unstable interface as they are instrumenting the binary directly, which can change between software versions. USDT probes are preferred whenever available. |
| §13.7.3 USDT | 13-Chapter 13. perf.md L1181 | A common problem, which has been fixed since Linux 4.20, is that some USDT probes require a semaphore in the process address space to be incremented to activate them properly. sdt_node:http__server__request is one such probe, and without incrementing the semaphore it will not record any events. |
| §13.8 perf stat | 13-Chapter 13. perf.md L1185 | The perf stat subcommand counts events. This can be used to measure the rate of events, or to check if an event is occurring at all. perf stat is efficient: it counts software events in kernel context and hardware events using PMC registers. This makes it suited to gauge the overhead of a more expensive perf record subcommand by first checking the event rate using perf stat . |
| §13.8.2 间隔统计 | 13-Chapter 13. perf.md L1258 | The counts column shows the number of events since the previous interval. Browsing this column can show time-based variation. The last line shows the count between the previous line and the time I typed Ctrl-C to end perf(1). That time was 0.214 seconds, as seen by the delta in the time column. |
| §13.9 perf record | 13-Chapter 13. perf.md L1329 | Note that the output includes the size of the perf.data file (6.060 Mbytes), the number of samples it contains (23,526), and the number of times perf(1) woke up to record data (9 times). The data is passed from the kernel to user-space via per-CPU ring buffers, and to keep context-switch overheads to a minimum perf(1) is woken up an infrequent and dynamic number of times to read them. |
| §13.9.2 CPU Profiling | 13-Chapter 13. perf.md L1387 | The ordering picks the most accurate CPU profiling mechanism available. The :ppp , :pp , and :p syntax activate precise event sampling modes, and can be applied to other events (apart from cycles) that support them. Events may also support different levels of precise. On Intel, precise events use PEBS; on AMD, they use IBS. These were defined in Section 4.3.9 , under the PMC Challenges heading. |
| §13.9.3 栈回溯 | 13-Chapter 13. perf.md L1401 | Note that if stack traces appear broken, it may be due to the software not honoring the frame pointer register. This was discussed in Chapter 5 , Applications , Section 5.6.2 , Missing Stacks . Apart from recompiling the software with frame pointers (e.g., gcc(1) -fno-omit-frame-pointer ), a different stack walking method may also work, selected using --call-graph . Options include: |
| §13.10 perf report | 13-Chapter 13. perf.md L1429 | It is also possible to summarize perf.data using external tools. These tools may process the output of perf script , covered in Section 13.11 , perf script . You may find perf report is sufficient in many situations, and use externals tools only when necessary. perf report summarizes using either an interactive text user interface (TUI) or a text report (STDIO). |
| §13.11 perf script | 13-Chapter 13. perf.md L1495 | The perf script subcommand by default prints each sample from perf.data, and is useful for spotting patterns over time that may be lost in a report summary. Its output may be used for generating flame graphs, and it also has the ability to run trace scripts that automate recording and reporting events in custom ways. These topics are summarized in this section. |
| §13.11.1 火焰图 | 13-Chapter 13. perf.md L1568 | Flame graphs visualize stack traces. While commonly used for CPU profiles, they can visualize any collection of stack traces collected by perf(1), including on context switch events to see why threads are leaving CPU, and on block I/O creation to see which code paths are creating disk I/O. |
| §13.12 perf trace | 13-Chapter 13. perf.md L1600 | The perf trace subcommand will trace system calls by default and print output live (no perf.data file). It was introduced in Chapter 5 , Applications , Section 5.5.1 , perf , as a lower-overhead version of strace(1) that can trace system-wide. perf trace can also inspect any event using similar syntax to perf record . |

> **书里没说的**（本篇的推论，原文没有，别当原文观点）：
> - **「先 stat 量速率、再 record 采样」这条工作流顺序**，原文 §13.8 是以「perf stat 高效，适合用来估算更贵的
>   record 子命令的开销」一句话表达的；本篇把它展开成固定的两步顺序，是本篇的读法。
> - **「probe 事件是有状态的资源，必须成对 add/del」**这个提法是本篇的说法：原文只分别描述了
>   `--add` 创建与 `--del` 删除两个动作，没有把它们组合成一条生命周期规则。
> - **「perf 是五类用法的统一入口」这张分类表**是本篇整理的：原文按子命令逐个介绍，
>   Table 13.1 给的是「子命令 ↔ 所属章节」的对应，不是按用途分类。
> - **「默认事件优先级 = 最准确优先」与「频率采样只影响 record」**这两条原文都明说（L1387 / L765），
>   但原文没有把它们连成「统计与采样在语义上不等价」这条结论——这条推导是本篇的。
> - **「USDT 事件创建后会在 perf list 里出现两条」**是原文描述的现象（SDT event 与 Tracepoint event 并列），
>   本篇把它归因到「元数据 vs 可插桩事件」两层，是本篇的重新表述。

## §13.1 子命令模型：一个工具，五种用法

**这段在讲什么机制**：perf 的所有能力都挂在**子命令**上，没有全局模式。原文给的最基本工作流是
两个子命令接力——`record` 负责插桩事件并把结果落盘，`report` 负责把文件汇总成人能读的结果：

> perf(1)’s capabilities are invoked via subcommands. As a common usage example, the following uses two subcommands: record to instrument events and save them to a file, and then report to summarize the contents of the file. These subcommands are explained in Section 13.9 , perf record , and Section 13.10 , perf report .

```bash
# perf record -F 99 -a -- sleep 30
[ perf record: Woken up 193 times to write data ]
[ perf record: Captured and wrote 48.916 MB perf.data (11880 samples) ]
# perf report --stdio
[...]
# Overhead  Command          Shared Object              Symbol
# ........  ...............  .........................  ............................
#
    21.10%  swapper          [kernel.vmlinux]           [k] native_safe_halt
     6.39%  mysqld           [kernel.vmlinux]           [k] _raw_spin_unlock_irqrest
     4.66%  mysqld           mysqld                     [.] _Z8ut_delaym
     2.64%  mysqld           [kernel.vmlinux]           [k] finish_task_switch
[...]
```

**为什么重要**：perf 不是「一个 profiler」，而是**五种观测方式共用一套事件语法**：

| 用法 | 子命令 | 产物 |
|---|---|---|
| 计数 | `stat` | 终端上的数字（不落盘） |
| 采样（存证据） | `record` | `perf.data` |
| 汇总 | `report` | TUI 或文本报告 |
| 逐样本展开 | `script` | 逐行事件流（可再加工） |
| 实时 | `trace` | 实时打印（不落盘） |

**容易错在哪**：以为「perf record 就是 perf」。原文 Table 13.1 列了二十多个子命令，
其中 `c2c`（缓存伪共享）、`lock`（锁）、`mem`（内存访问）、`kmem`（内核内存）、`kvm`（虚拟机）、
`sched`（调度器统计）、`bench`（微基准）、`annotate`（反汇编标注）、`diff`（两个 profile 做差）
都是**特定分析域的专用入口**，不是 `record` 的参数变体。

**判据**：先问「我要的是**数字**、**证据文件**、还是**时间线**」，再选子命令；
`stat` 计数与 `record` 采样在语义上不等价（见 §13.4.1）。

## §13.2 One-Liners：用示例解释能力

**这段在讲什么机制**：§13.2 是一张**能力速查表**，把 perf 的用法按目的分成六组
（Listing Events / Counting Events / Profiling / Static Tracing / Dynamic Tracing / Reporting）。
原文明确说它是一个更大的在线清单的子集（原话见 §13.2 开头）。

| 用途组 | 代表命令 |
|---|---|
| Listing Events | `perf list`、`perf list 'sched:*'`、`perf list block`、`perf probe -l` |
| Counting Events | `perf stat command`、`perf stat -a sleep 5`、`perf stat -e raw_syscalls:sys_enter -I 1000 -a` |
| Profiling | `perf record -F 99 command`、`perf record -F 99 -a -g sleep 10`、`perf record -e LLC-load-misses -c 100 -ag sleep 5` |
| Static Tracing | `perf record -e sched:sched_switch -a -g sleep 1`、`perf record -e block:block_rq_issue --filter 'bytes >= 65536'` |
| Dynamic Tracing | `perf probe --add tcp_sendmsg`、`perf probe 'tcp_sendmsg bytes=%cx'` |
| Reporting | `perf report`、`perf report -n --stdio`、`perf script --header`、`perf annotate --stdio` |

**为什么重要**：这六组恰好对应「**有哪些事件 → 事件怎么数 → 事件怎么采 → 事件怎么追 → 探针怎么建 → 结果怎么看**」，
顺序本身就是一条使用路径。

**兼容性要点**：`-a`（所有 CPU）从 **Linux 4.11 起是默认**，所以老文章里的 `-a` 在新内核上可以省掉；
反过来，看到老命令带 `-a` 不必怀疑语法。

**容易错在哪**：one-liner 是**能力演示**，不是最佳实践——它们省略了开销评估与结果解读。

## §13.3 事件类型全景：perf list 到底列了什么

**这段在讲什么机制**：`perf list` 的输出**按事件类型分组**，原文给出的类型清单是理解 perf 的总纲：

| 类型 | 原文定义要点 |
|---|---|
| Hardware event | 处理器事件，用 PMC 实现 |
| Software event | 内核里的计数器事件 |
| Hardware cache event | 处理器缓存事件（PMC） |
| Kernel PMU event | PMU 事件（PMC） |
| cache / floating point 等 | 处理器厂商事件（PMC）加简短描述 |
| Raw hardware event descriptor | 用原始编码指定的 PMC |
| Hardware breakpoint | 处理器断点事件 |
| Tracepoint event | 内核静态插桩事件 |
| SDT event | 用户态静态插桩事件（USDT） |
| pfm-events | libpfm 事件（Linux 5.8 起） |

**关键机制**：静态插桩点和**已经初始化过的动态探针混在同一个桶里**：

> The tracepoint and SDT events mostly list static instrumentation points, but if you have created some dynamic instrumentation probes, those will be listed as well. I included an example in the output: probe:do_nanosleep is described as a “Tracepoint event” that is based on a kprobe.

也就是说，`perf list` 里看到 `probe:do_nanosleep` 被标成 “Tracepoint event”，它**不是**内核里的静态插桩点，
而是一个你已经创建过的 kprobe。**「perf list 显示了什么」取决于当前系统上留下了什么探针状态**（推演）。

**容易错在哪**：把 `perf list` 当成静态的事件目录来读，会把本机残留的探针误当成系统自带能力。

**判据**：判断某个事件「是不是内核自带的」，看它有没有 `probe:` 前缀；有前缀就是本机动态创建的。

## §13.4 硬件事件：可读名只是映射，不是保证

**这段在讲什么机制**：硬件事件由 PMC 实现，编码与**具体处理器型号**绑定（event select + umask 两个寄存器码）。
原文以 Intel 的 branch instructions 为例：原始描述符写成 `r00c4`，其中 `r` 前缀表示 raw，
`00c4` 是寄存器码的十六进制简写（umask 0x0 + event select 0xc4）。这些码只印在处理器手册里。

perf 的易用之处在于它提供**可读名映射**，把编码翻译成 `branch-instructions` 这样的名字。
但原文对这个映射的可靠性给了非常克制的评价：

> There are many processor types, and new versions are released regularly. It is possible that human-readable mappings for your processor are not yet available in perf(1) or are in a newer kernel version. Some PMCs may never be exposed via a human-readable name. I regularly have to switch from human-readable names to raw event descriptors once I move to deeper PMCs that lack mappings. There may also be bugs in the mappings, and if you ever encounter a suspicious PMC result, you may wish to try the raw event descriptor to double-check.

**四条承重结论**：

1. 可读名**可能还没进你手上的内核版本**——映射随内核版本走。
2. 有些 PMC **永远没有**可读名（尤其偏深的 PMU 事件）。
3. 映射**本身可能有 bug**——原文还提到见过可读名指向错误 PMC 的情况，且这从 perf 输出里看不出来，
   需要你本来就熟悉该 PMC、有「什么算正常」的预期，才能看出异常。
4. **对策是原始事件描述符**——遇到可疑数字，用 raw 编码复核一次。

**容易错在哪**：把「perf 里能写的名字」当成「这个事件在你的 CPU 上就是这个含义」。

## §13.4.1 频率采样：默认状态下你并没有录到全部

**这段在讲什么机制**：`perf record` 采 PMC 时**默认用频率采样**：内核按「每秒约 N 次」的速率抽事件，
而不是每个事件都记录。默认值是每 CPU 4000 次/秒：

> The output shows that frequency sampling is enabled ( freq 1 ) with a sample frequency of 4000. This tells the kernel to adjust the rate of sampling so that roughly 4,000 events per second per CPU are captured. This is desirable, since some PMCs instrument events that can occur billions of times per second (e.g., CPU cycles) and the overhead of recording every event would be prohibitive. 3 But this is also a gotcha: the default output of perf(1) (without the very verbose option: -vv ) does not say that frequency sampling is in use, and you may be expecting to record all events. This event frequency only affects the record subcommand; stat counts all events.

**四个要点**：

- **为什么要这样**：cycles 这类 PMC 每秒能发生几十亿次，逐个记录的开销不可接受。
- **最大的坑**：**默认输出（不加 `-vv`）不会告诉你开了频率采样**——你以为录了全部，其实录的是抽样。
- **统计与采样语义不同**：频率采样**只影响 `record`**；`stat` 永远全量计数。
- **怎么调**：`-F` 改频率（次/秒），`-c` 改周期（每 N 个事件采一次，即 overflow sampling）。

```bash
# perf record -vve cycles -a sleep 1
Using CPUID GenuineIntel-6-8E
intel_pt default config: tsc,mtc,mtc_period=3,psb_period=3,pt,branch
------------------------------------------------------------
perf_event_attr:
  size                             112
  { sample_period, sample_freq }   4000
  sample_type                      IP|TID|TIME|CPU|PERIOD
  disabled                         1
  inherit                          1
  mmap                             1
  comm                             1
  freq                             1
[...]
[ perf record: Captured and wrote 3.360 MB perf.data (3538 samples) ]
```

**硬上限（两个 sysctl）**：采样率与 perf 自身占用的 CPU 都有天花板，用 `sysctl(8)` 查看与设置：

```bash
# sysctl kernel.perf_event_max_sample_rate
kernel.perf_event_max_sample_rate = 15500
# sysctl kernel.perf_cpu_time_max_percent
kernel.perf_cpu_time_max_percent = 25
```

原文示例系统上：最大采样率 15500 Hz，**PMU 中断允许占用的 CPU 最多 25%**。

**还必须查一件事**：内核会为了保护自己**节流采样率并丢事件**（原文脚注 3），
所以要看 lost events——办法是看汇总计数器 `perf report -D | tail -20`。

**判据**：**任何一次 `perf record` 之前，先用 `-vv` 确认采样模式**；
出现「样本数比预期少很多」时，第一反应是频率采样，第二反应是丢事件，而不是「代码没走到那条路」。

## §13.5 软件事件：默认采样模式与 tracepoint 不一样

**这段在讲什么机制**：软件事件（`context-switches`、`cpu-clock` 等）是**内核里用软件插桩的计数器**，
底层对应硬件事件。原文用它和等价的 tracepoint 做了一次并列对比：

| 事件 | 类型 | 默认采样模式 | 效果 |
|---|---|---|---|
| `context-switches` | Software event | 频率采样，4000/秒 | 只采到一部分 |
| `sched:sched_switch` | Tracepoint | 周期采样，period = 1（等价 `-c 1`） | **捕获每一个事件** |

> This time, period sampling is used (no freq 1 ), with a sample period of 1 (equivalent to -c 1 ). This captures every event. You can do the same with software events by specifying -c 1 , for example:

**为什么重要**：这是本章最容易被忽略的**语义不等价**——**两个看起来描述同一件事的事件，默认行为完全不同**。
想数「上下文切换了多少次」用 `stat`；想看「每一次切换时线程站在哪」必须用 tracepoint 版的
`sched:sched_switch`（默认就是每事件一次），而且要带 `-g` 才有栈。

**容易错在哪**：用 `context-switches` 软件事件去 record，然后抱怨「切换记录怎么这么少」。

**代价**：全量记录事件量与开销都不小，**上下文切换尤其频繁**。原文的建议是先查频率：

> Be careful with the volume of recording every event and the overheads involved, especially for context switches, which can be frequent. You can use perf stat to check their frequency: see Section 13.8 , perf stat.

**判据**：**「要次数」用 stat，「要现场」用 record + tracepoint**；
真要全量录，先用 `perf stat` 量一下这个事件的速率。

## §13.6 Tracepoint Events：内核静态插桩点

**这段在讲什么机制**：tracepoint 是内核里预先写好的静态插桩点，是本章里**最稳定**的事件源——
不需要 debuginfo，不需要参考机，不需要猜寄存器。原文在本节给的三件事：

```bash
perf record -e block:block_rq_issue -a sleep 10; perf script
cat /sys/kernel/debug/tracing/events/block/block_rq_issue/format
perf record -e block:block_rq_issue --filter 'bytes > 65536' -a sleep 10
```

- **format 文件是元数据入口**：每个事件的字段名（`bytes`、`prev_pid` 等）定义在
  `/sys/kernel/debug/tracing/events/<group>/<event>/format` 里。§13.8.4 的过滤器表达式能用哪些名字，
  取决于这个文件。
- **过滤在事件源侧做**：`--filter` 是内核侧过滤，只记录满足条件的那些事件，**比录下来再筛省得多**。

**容易错在哪**：拿内核 tracepoint 的字段名去猜用户态探针的字段名——两套命名互不相通。

## §13.7 Probe Events：动态探针的总论

**这段在讲什么机制**：perf 用「probe events」统称三类动态探针：

> perf(1) uses the term probe events to refer to kprobes, uprobes, and USDT probes. These are “dynamic” and must first be initialized before they can be traced: they are not present in the output of perf list by default (some USDT probes may be present because they have been auto-initialized). Once initialized, they are listed as “Tracepoint event.”

**三个承重事实**：

1. **动态探针是「先创建、后使用」的两步**——不创建就不会出现在 `perf list` 里，也就无从使用。
2. **创建之后它们被标成 “Tracepoint event”**——和静态 tracepoint 混在一起（§13.3 的坑）。
3. **创建的是资源，要显式清理**——`perf probe --del`。

```bash
perf probe --add do_nanosleep
perf record -e probe:do_nanosleep -a sleep 5
perf script
perf probe --del do_nanosleep
```

**容易错在哪**：在一台共享的机器上建了探针就忘了删——它会一直留在系统里，影响别人看到的 `perf list`，也占着插桩点。

## §13.7.1 kprobes：内核函数插桩与「怎么拿到参数」

**机制**：kprobe 在内核函数入口插桩；在函数名后加 `%return` 就变成 **kretprobe**，
可以配合特殊变量 `$retval` 读返回值。

**真正的难点是参数**，原文给了四种办法：

> There are at least four different ways to instrument the arguments to kernel functions.

| 办法 | 前提 | 做法 |
|---|---|---|
| 1. 内核 debuginfo | 目标机装了 | `perf probe --vars` 列出变量名，创建探针时带上 |
| 2. **参考机法** | 有一台**硬件与内核完全相同**的机器 | 在参考机上装 debuginfo，用 `-n`（dry run）+ `-v` 打出寄存器位置，拷到目标机 |
| 3. 自己按 ABI 推 | 知道处理器 ABI | 直接写寄存器位置 |
| 4. BTF | 内核带 BTF（BPF type format） | 新的调试信息来源，**更可能默认可用** |

办法 1 的输出长这样：

```bash
# perf probe --vars do_nanosleep
Available variables at do_nanosleep
        @<do_nanosleep+0>
                enum hrtimer_mode       mode
                struct hrtimer_sleeper* t
```

办法 2 的 dry run 会直接告诉你变量在哪个寄存器：

```bash
# perf probe -nv 'do_nanosleep mode'
[...]
Writing event: p:probe/do_nanosleep _text+10806336 mode=%si:x32
```

**办法 2 的硬前提**（原文单独强调）：

> This only works if the systems have the same processor ABI and kernel versions, otherwise the wrong register locations may be instrumented.

**容易错在哪**：
- 以为 dry run 会在目标机上「装上」探针——它只打印，不创建（原文：dry run 不创建事件）。
- 参考机 ABI/内核不一致还照抄寄存器位置——插出来的值是垃圾，而且**不报错**。
- 生产环境常常根本没有内核 debuginfo（原文原话：as I often find in production）。

**判据**：拿不到参数值时，先用办法 2 在**严格同型号同内核**的参考机上试；拿不到就退回办法 3 按 ABI 推，
或者放弃取参数、只做「进入/返回时序」的观测。

## §13.7.2 uprobes：用户态函数插桩

**机制**：uprobes 在**用户态函数**入口插桩，用 `-x` 指定二进制路径，事件名自动带上前缀：

```bash
# perf probe -x /lib/x86_64-linux-gnu/libc.so.6 --add fopen
Added new event:
  probe_libc:fopen     (on fopen in /lib/x86_64-linux-gnu/libc-2.27.so)
You can now use it in all perf tools, such as:

        perf record -e probe_libc:fopen -aR sleep 1
```

`%return` 变体是 **uretprobe**，返回值同样用 `$retval` 读。

**取参数的语法分解**（原文逐项解释），以 x86_64 为例：

```bash
# perf probe -x /lib/x86_64-linux-gnu/libc.so.6 --add 'fopen filename=+0(%di):string
mode=%si:u8'
[...]
# perf script
             run 28882 [013] 4503285.383830: probe_libc:fopen: (7fbe130e6e30)
filename="/etc/nsswitch.conf" mode=147
             run 28882 [013] 4503285.383997: probe_libc:fopen: (7fbe130e6e30)
filename="/etc/passwd" mode=17
```

- `filename=` —— 输出里的**别名**，纯标注用。
- `%di`、`%si` —— x86_64 上按 AMD64 ABI，前两个函数参数所在的寄存器。
- `+0(...)` —— **解引用偏移 0 处的内容**；不加它就会把地址本身当字符串打印出来。
- `:string` —— 按字符串打印；`:u8` —— 按无符号 8 位整数打印。

**这一节最重要的一句**：

> While uprobes can provide visibility into application internals, they are an unstable interface as they are instrumenting the binary directly, which can change between software versions. USDT probes are preferred whenever available.

**为什么重要**：uprobe **直接插桩二进制**，二进制一升级（函数改名、参数变、符号表变）插桩点就失效或错位。
原文给的判据是「**能用 USDT 就用 USDT**」。

**容易错在哪**：把 uprobe 当成长期观测接口用在一个频繁发版的应用上——某天它悄悄不工作了。

## §13.7.3 USDT：稳定的用户态插桩点

**机制**：USDT 是**用户态静态插桩点**，为「应用内部发生了什么」提供稳定接口。原文给的流程是三步：

1. **让 perf 认识这个二进制**：`perf buildid-cache --add $(which node)`。
   （二进制里有没有 USDT 点，可以用 `readelf -n` 查 ELF notes 段。）
2. **在 `perf list` 里看到它们**：标为 `[SDT event]`。
3. **创建事件才能真正插桩**——USDT 探针本身也是用 uprobe 机制实现的。

```bash
# perf buildid-cache --add $(which node)
# perf list | grep sdt_node
  sdt_node:gc__done                                  [SDT event]
  sdt_node:http__client__request                     [SDT event]
  sdt_node:http__server__request                     [SDT event]
  sdt_node:net__server__connection                   [SDT event]
# perf probe sdt_node:http__server__request
Added new event:
  sdt_node:http__server__request (on %http__server__request in
home/bgregg/Build/node-v12.4.0/out/Release/node)
```

**创建之后 `perf list` 里同一个东西出现两条**——一条 `[SDT event]`（USDT 元数据），
一条 `[Tracepoint event]`（可插桩的 trace event）。原文说这与其他事件的工作方式是一致的。

**最大的坑**：

> A common problem, which has been fixed since Linux 4.20, is that some USDT probes require a semaphore in the process address space to be incremented to activate them properly. sdt_node:http__server__request is one such probe, and without incrementing the semaphore it will not record any events.

**「一条事件都录不到」的最常见原因**，不是探针没建对，而是**信号量没递增**。

**另一个坑**：USDT 的参数常是结构体和字符串，perf 默认把它们打成**指针地址**。
原文指出可以在建探针时转型（例如把第三个参数转成名叫 `address` 的字符串），
但紧接着写明**当时这不工作**。

**容易错在哪**：建好 USDT 探针、perf record 也跑了、结果零样本——先怀疑信号量，再怀疑权限。

## §13.8 perf stat：先用便宜的尺子量

**这段在讲什么机制**：`perf stat` **只计数，不记录**。它的实现决定了它便宜：
软件事件在内核上下文计数，硬件事件用 PMC 寄存器计数。

> The perf stat subcommand counts events. This can be used to measure the rate of events, or to check if an event is occurring at all. perf stat is efficient: it counts software events in kernel context and hardware events using PMC registers. This makes it suited to gauge the overhead of a more expensive perf record subcommand by first checking the event rate using perf stat .

**为什么这是本章最该记住的一句**：**先用 stat 量速率，再决定要不要上 record**。
record 会落盘、会带栈、开销大一个量级；在不知道事件密度的情况下直接 record，很容易录出一堆无用的大文件。

```bash
# perf stat -e sched:sched_switch -a -- sleep 1
 Performance counter stats for 'system wide':

             5,705      sched:sched_switch

       1.001892925 seconds time elapsed
```

### §13.8.1 Options

| 选项 | 含义 |
|---|---|
| `-a` | 跨所有 CPU 记录（**Linux 4.11 起已是默认**） |
| `-e event` | 记录指定事件（可多个） |
| `--filter filter` | 给事件设布尔过滤表达式 |
| `-p PID` | 只记录该进程 |
| `-t TID` | 只记录该线程 |
| `-G cgroup` | 只记录该 cgroup（**容器场景**） |
| `-A` | 按 CPU 分别显示计数 |
| `-I interval_ms` | 每隔指定毫秒输出一行 |
| `-v` / `-vv` | 更啰嗦的输出 |

事件可以是 tracepoint、软件事件、硬件事件、kprobe、uprobe、USDT probe——**同一套 `-e` 语法通吃**。
支持 **glob 风格通配符**（`*` 匹配任意多个字符，`?` 匹配任意一个字符）：

```bash
# perf stat -e 'sched:*' -a
# perf stat -e 'sched:*' -e 'block:*' -a
# perf stat -e 'sched:*,block:*' -a
```

**不指定事件时，perf stat 默认用架构 PMC**（即 cycles、instructions 这类）。

### §13.8.2 Interval Statistics：看随时间的变化

`-I 1000` 每秒打印一行，**counts 列是「距上一个间隔」的事件数**，不是累计值：

> The counts column shows the number of events since the previous interval. Browsing this column can show time-based variation. The last line shows the count between the previous line and the time I typed Ctrl-C to end perf(1). That time was 0.214 seconds, as seen by the delta in the time column.

```bash
# perf stat -e sched:sched_switch -a -I 1000
#           time             counts unit events
     1.000791768              5,308      sched:sched_switch
     2.001650037              4,879      sched:sched_switch
     3.002348559              5,112      sched:sched_switch
     4.003017555              5,335      sched:sched_switch
     5.003760359              5,300      sched:sched_switch
^C     5.217339333              1,256      sched:sched_switch
```

**为什么重要**：**逐行浏览这一列才能看出时间维度的抖动**（周期性的尖峰、缓慢的趋势）。
只看一个总数，这些全被抹平了。最后一行是 Ctrl-C 到上一行之间的计数，时间列的 delta 就是那 0.214 秒。

### §13.8.3 Per-CPU Balance：负载是否均衡

`-A` 按**每个逻辑 CPU** 分别输出，可直接看出负载倾斜；另有 `--per-socket`、`--per-core` 做
socket / core 级聚合。

```bash
# perf stat -e sched:sched_switch -a -A -I 1000
#           time CPU                counts unit events
     1.000351429 CPU0                 1,154      sched:sched_switch
     1.000351429 CPU1                   555      sched:sched_switch
     1.000351429 CPU2                   492      sched:sched_switch
     1.000351429 CPU3                   925      sched:sched_switch
```

### §13.8.4 Event Filters：把过滤下推到内核

> A filter may be provided for some event types (Tracepoint events) to test event arguments with a Boolean expression. The event will only be counted if the expression is true.

```bash
# perf stat -e sched:sched_switch --filter 'prev_pid == 25467' -a -I 1000
#           time             counts unit events
     1.000346518                131      sched:sched_switch
     2.000937838                145      sched:sched_switch
     3.001370500                 11      sched:sched_switch
     4.001905444                217      sched:sched_switch
```

**两点要注意**：
- 过滤**只对部分事件类型有效**（主要是 tracepoint），不是所有事件都支持。
- 参数名**每个事件自定义**，要查 `/sys/kernel/debug/tracing/events` 下该事件的 format 文件。

**为什么重要**：过滤在**内核侧**完成，`stat` 只回传命中数——这是「只关心一个 PID 的切换」时最省的办法。

### §13.8.5 Shadow Statistics：自动附带的派生指标

某些事件组合会被自动附上**派生统计（shadow statistics）**，打印在 `#` 后面。
典型是 cycles + instructions ⇒ 给出 IPC：

```bash
# perf stat -e cycles,instructions -a
^C
 Performance counter stats for 'system wide':

     2,895,806,892      cycles
     6,452,798,206      instructions              #    2.23  insn per cycle

       1.040093176 seconds time elapsed
```

**IPC 是「性能是否接近上限」的粗判据**：接近 1 说明前端/依赖停顿多，明显高于 1 说明在跑吞吐型负载。
（这一层因果是推演，原文只给出 IPC 的来源与读法。）

## §13.9 perf record：把证据录进 perf.data

**这段在讲什么机制**：`record` 把事件录到文件（默认 `perf.data`）供后续分析。
事件用 `-e` 指定，多事件可以用多个 `-e` 或逗号分隔。

**承重的一句在传输机制上**：

> Note that the output includes the size of the perf.data file (6.060 Mbytes), the number of samples it contains (23,526), and the number of times perf(1) woke up to record data (9 times). The data is passed from the kernel to user-space via per-CPU ring buffers, and to keep context-switch overheads to a minimum perf(1) is woken up an infrequent and dynamic number of times to read them.

三个要点：
- 数据经**每 CPU 环形缓冲**从内核送到用户态；
- perf 被唤醒的次数是**低频且动态**的——目的就是**压低上下文切换开销**（对比 §3.2.2 的切换成本）；
- 输出里的三个数字各有含义：文件大小、样本数、唤醒次数。**唤醒次数多 ⇒ 缓冲溢出风险高**（推演）。

```bash
# perf record -e sched:sched_switch -a
^C[ perf record: Woken up 9 times to write data ]
[ perf record: Captured and wrote 6.060 MB perf.data (23526 samples) ]
```

**时长控制**：用 `-- sleep N` 里的 dummy 命令限定录制时长（`sleep 1` 是最常用的）。

### §13.9.1 Options

与 `stat` 同名的选项：`-a`、`-e`、`--filter`、`-p`、`-t`、`-G`、`-v`。
record 独有：`-g`（记栈）、`--call-graph mode`（指定栈回溯方式：fp / dwarf / lbr）、
`-o file`（输出文件名）。

**注意**：`--filter`、`-p`、`-t`、`-G` 在 record 里同样可用——**过滤是内核侧行为，两边一致**。

### §13.9.2 CPU Profiling：不写 -e 时它选什么

**机制**：`perf record` 不指定 `-e` 时，按下面顺序取**第一个可用**的事件——
原文的措辞是「选最准确的 CPU profiling 机制」：

> The ordering picks the most accurate CPU profiling mechanism available. The :ppp , :pp , and :p syntax activate precise event sampling modes, and can be applied to other events (apart from cycles) that support them. Events may also support different levels of precise. On Intel, precise events use PEBS; on AMD, they use IBS.

原文列出的顺序（逐字）：

```bash
cycles:ppp : CPU cycle-based frequency sampling with precise set to zero skid
cycles:pp : CPU cycle-based frequency sampling with precise set to requesting zero skid (which may not be zero in practice)
cycles:p : CPU cycle-based frequency sampling with precise set to requesting constant skid
cycles : CPU cycle-based frequency sampling (no precise)
cpu-clock : Software-based CPU frequency sampling
```

**precise 的含义**：采样点能精确对应到真正触发事件的那条指令，而不是「附近某处」——
这直接影响你看到的热点符号是不是可信。Intel 上是 **PEBS**，AMD 上是 **IBS**（机制见 [[Chapter 6. CPUs]]）。
`:ppp` / `:pp` / `:p` 这套语法也能用在 cycles 以外支持 precise 的事件上。

**容易错在哪**：看到 `cycles:pppH` 这样的事件名后缀就以为是「某个特殊模式」——它只是
precise 修饰符加上其它标志位（H、宿主等）的组合。

### §13.9.3 Stack Walking：栈为什么断了

**机制一：max-stack 优于统一的 -g**。它有两个好处——**能指定最大深度**，且**不同事件可以用不同深度**：

```bash
# perf record -e sched:sched_switch/max-stack=5/,sched:sched_wakeup/max-stack=1/ \
    -a -- sleep 1
```

**机制二：栈断裂的根因通常是帧指针**：

> Note that if stack traces appear broken, it may be due to the software not honoring the frame pointer register. This was discussed in Chapter 5 , Applications , Section 5.6.2 , Missing Stacks . Apart from recompiling the software with frame pointers (e.g., gcc(1) -fno-omit-frame-pointer ), a different stack walking method may also work, selected using --call-graph . Options include:

| 方式 | 机制 | 代价 |
|---|---|---|
| `--call-graph fp` | 帧指针回溯（**默认**） | 依赖软件的编译选项遵守帧指针 |
| `--call-graph dwarf` | 基于 debuginfo 回溯 | 需要 debuginfo（有些软件由 `-dbgsym` / `-dbg` 包提供） |
| `--call-graph lbr` | Intel LBR（处理器提供） | 深度通常只有 **16 帧**（Haswell 起），Skylake 起 32 帧 |

**为什么重要**：这三条的取舍是**准确度 vs 成本 vs 深度**的三方权衡——
fp 便宜但依赖编译选项，dwarf 准但要 debuginfo，lbr 是硬件但太浅。

**容易错在哪**：栈断断续续时先去改应用代码；其实先问「它编译时有没有保留帧指针」。

## §13.10 perf report：把 perf.data 汇总成人看的东西

**机制**：`report` 读 perf.data 并输出汇总，两种形态——**TUI（默认，交互）** 与 **STDIO（文本，可重定向）**。
选项：`--tui` / `--stdio` / `-i file`（输入文件）/ `-n`（加一列样本计数）/ `-g options`（改调用图显示）。

**关于外部工具的定位**，原文说得很克制：

> It is also possible to summarize perf.data using external tools. These tools may process the output of perf script , covered in Section 13.11 , perf script . You may find perf report is sufficient in many situations, and use externals tools only when necessary. perf report summarizes using either an interactive text user interface (TUI) or a text report (STDIO).

**外部工具处理的是 `perf script` 的输出**，不是 perf.data 本身；而且**大多数情况下 report 就够了**。

### §13.10.1 TUI

交互界面：可以选中函数与线程往下钻。原文给的例子是 99 Hz、10 秒、无栈的 CPU profile
（默认事件此时是 `cpu-clock:pppH`）。

### §13.10.2 STDIO

非交互，**适合重定向到文件**——贴到聊天、邮件、工单系统里。原文推荐加 `-n` 带上样本数列。

**栈样本的合并规则**（读 STDIO 报告必须知道）：

> Stack trace samples are merged as a hierarchy, beginning from the root function on the left and moving through child functions down and to the right. The right-most function is that of the event (in this case, the on-CPU function), and to its left is its ancestry.

**最右边是事件发生时的函数**（on-CPU 函数），**它左边是它的祖先链**。
所以从右往左读就是「谁调用了谁」。

**顺序可以翻转**：

> This left-to-right ordering is called caller by perf(1). You can flip this to callee ordering, where the event function is on the left and its ancestry is down and to the right using: -g callee (it was the default; perf(1) switched to the caller ordering in Linux 4.4).

**版本坑**：caller 顺序是 **Linux 4.4 起**的默认；更早的默认是 callee。
别人给你的报告左右顺序和你现在看到的不一样，多半是内核版本差异。

## §13.11 perf script：逐样本看时间线

**机制**：`script` **默认逐条打印 perf.data 里的每个样本**。

> The perf script subcommand by default prints each sample from perf.data, and is useful for spotting patterns over time that may be lost in a report summary. Its output may be used for generating flame graphs, and it also has the ability to run trace scripts that automate recording and reporting events in custom ways.

**这是 report 与 script 的分工**：report 是**汇总**（丢失时间维度），script 是**原始流**（保留时间维度）。

**输出字段**（原文逐项解释，以第一行为例）：进程名、线程 ID、CPU ID、时间戳（秒）、
Period（由 `-F 99` 推出）、事件名、事件参数（对 cpu-clock 是指令指针、函数名+偏移、段名）。
⚠️ **默认字段集合会随版本变化**，别的事件也不一定带 period 字段。

**要稳定输出就显式指定字段**：

> Since it can be important to produce consistent output, especially for post-processing, you can use the -F option to specify the fields. I frequently use it for including the process ID, as it is missing from the default field set. I’d also recommend adding --header to include the perf.data metadata.

原文的两条建议：**用 `-F` 固定字段**（默认集合里没有 pid，原文常自己加），
**加 `--header`**（把 perf.data 的元数据一起存下来：采集时间、主机名、内核版本、perf 版本、采集命令行等）。

```bash
# perf script --header -F comm,pid,tid,cpu,time,event,ip,sym,dso,trace
# ========
# captured on    : Sun Jan  5 23:43:56 2020
# header version : 1
# data offset    : 264
# data size      : 2393000
# feat offset    : 2393264
# hostname : bgregg-mysql
# os release : 5.4.0
# perf version : 5.4.0
# arch : x86_64
# nrcpus online : 4
# nrcpus avail : 4
# cpudesc : Intel(R) Xeon(R) Platinum 8175M CPU @ 2.50GHz
# cpuid : GenuineIntel,6,85,4
# total memory : 15923672 kB
# cmdline : /usr/bin/perf record -F 99 -a -g -- sleep 30
# ========
```

**为什么重要**：**header 里的 `cmdline` 与 `os release` 是复现与对比的前提**——
同一份样本在别的机器上跑，结论不能直接照搬。

### §13.11.1 Flame Graphs

> Flame graphs visualize stack traces. While commonly used for CPU profiles, they can visualize any collection of stack traces collected by perf(1), including on context switch events to see why threads are leaving CPU, and on block I/O creation to see which code paths are creating disk I/O.

**承重的是「any collection」这四个字**：火焰图不限于 CPU profile——
在**上下文切换事件**上做火焰图，看的是「线程为什么离开 CPU」；
在 **block I/O 创建事件**上做火焰图，看的是「哪条代码路径在造 I/O」（下游见 [[Chapter 9. Disks]]）。

**工具现状**：perf **内建**火焰图支持是 **Linux 5.8** 加的（`perf script report flamegraph`）；
常见实现是作者自研版与一个 d3 版本。**FlameScope** 是另一个工具，
把**亚秒偏移热力图**与火焰图叠加，用来看**时间维度**的变化。

**容易错在哪**：把火焰图当成「CPU 热点图」——它其实是「栈集合分布图」，
喂给它什么事件就回答什么问题。

### §13.11.2 Trace Scripts

`perf script -l` 列出可用 trace script（如 `sched-migration`、`net_dropmonitor`、
`syscall-counts-by-pid`、`failed-syscalls-by-pid`、`export-to-sqlite`、`stackcollapse`），
作为 `perf script` 的参数执行；**也可以自己用 Perl 或 Python 写**。

**为什么重要**：trace script 是把「原始事件流」变成「一次性结论」的自动化层——
上面那几个现成的脚本，本质就是本篇选型判据里「按 PID 统计系统调用」「统计丢包」的可执行版本。

## §13.12 perf trace：实时打印

**机制**：

> The perf trace subcommand will trace system calls by default and print output live (no perf.data file). It was introduced in Chapter 5 , Applications , Section 5.5.1 , perf , as a lower-overhead version of strace(1) that can trace system-wide. perf trace can also inspect any event using similar syntax to perf record .

三个要点：
- **默认追系统调用**，实时打印，**不产生 perf.data**；
- 是 **strace(1) 的低开销替代**，且**可以全系统范围追踪**（strace 要逐进程 attach）；
- 事件语法与 `record` 相同，所以**能查任意事件**，不限于系统调用。

```bash
# perf trace -e block:block_rq_issue,block:block_rq_complete
     0.000 auditd/391 block:block_rq_issue:259,0 WS 8192 () 16046032 + 16 [auditd]
     0.566 systemd-journa/28651 block:block_rq_complete:259,0 WS () 16046032 + 16 [0]
     0.748 jbd2/nvme0n1p1/174 block:block_rq_issue:259,0 WS 61440 () 2100744 + 120
[jbd2/nvme0n1p1-]
     1.436 systemd-journa/28651 block:block_rq_complete:259,0 WS () 2100744 + 120 [0]
```

**过滤器支持字符串常量**（来自内核头文件）：

```bash
# perf trace -e syscalls:*enter_mmap --filter='flags==SHARED'
     0.000 env/14780 syscalls:sys_enter_mmap(len: 27002, prot: READ, flags: SHARED,
fd: 3)
    16.145 grep/14787 syscalls:sys_enter_mmap(len: 27002, prot: READ, flags: SHARED,
fd: 3)
```

**beautification**：注意上面输出里的 `prot: READ` 而不是 `prot: 1`——
perf 把格式串里的数值换成了可读字符串，原文把这项能力称为 “beautification”。

### §13.12.1 Kernel Versions：默认值随版本变

原文给了三条版本边界：

| 变化 | 版本 |
|---|---|
| `-a`（全 CPU 追踪）成为默认 | Linux 3.8 |
| `perf trace` 不再默认追全部系统调用（`--no-syscalls` 成默认） | Linux 4.19 |
| `--filter` 可用 | Linux 5.5 |
| perf 内建火焰图支持 | Linux 5.8 |
| `-a` 在 `stat`/`record` 里成为默认 | Linux 4.11 |

**容易错在哪**：拿旧文章里的命令在新机器上跑，输出量与预期差一个数量级——
多半是默认值变了，不是系统出问题了。

## §13.13 其他命令与硬件追踪

原文补列的专用子命令：

| 命令 | 用途 |
|---|---|
| `perf c2c`（Linux 4.10+） | 缓存到缓存传输与**缓存行伪共享**分析 |
| `perf kmem` | 内核内存（slab）分配分析 |
| `perf kvm` | KVM guest 分析 |
| `perf lock` | 锁分析 |
| `perf mem` | 内存访问分析 |
| `perf sched` | 内核调度器统计（延迟） |
| `perf script` | 自定义 perf 工具 |

**更高一档的能力**：**在事件上启动 BPF 程序**；以及**硬件追踪**（Intel PT、ARM CoreSight）做**逐指令**分析。
原文给的例子是 Intel PT 记录用户态周期：

```bash
# perf record -e intel_pt/cyc/u date
Sat Jul 11 05:52:40 PDT 2020
[ perf record: Woken up 1 times to write data ]
[ perf record: Captured and wrote 0.049 MB perf.data ]
# perf script --insn-trace
        date 31979 [003] 653971.670163672:      7f3bfbf4d090 _start+0x0 (/lib/x86_64-
linux-gnu/ld-2.27.so) insn: 48 89 e7
```

装上 Intel XED 之后，机器码会被翻译成汇编：

```bash
# perf script --insn-trace --xed
date 31979 [003] 653971.670163672: ... (/lib/x86_64-linux-gnu/ld-2.27.so) mov %rsp,
%rdi
date 31979 [003] 653971.670163672: ... (/lib/x86_64-linux-gnu/ld-2.27.so) callq
0x7f3bfdf4dea0
```

**原文对这个量级的评价**：

> While this is incredible detail, it is also verbose. The full output was 266,105 lines, and that’s just for the date(1) command.

**26 万行，只是一个 `date(1)`**——逐指令追踪是**取证级**工具，不是日常分析工具。

## 章末选型判据

| 场景 | 用什么 | 别用什么 |
|---|---|---|
| 想知道某事件一分钟发生几次 | `perf stat -e <event> -a -- sleep 60` 先量速率 | 直接上 `perf record` 全量录 |
| 想知道 CPU 花在哪些函数上 | `perf record -F 99 -a -g -- sleep 30` + `perf report --stdio -n` | 只看 `top` 的 CPU 百分比 |
| 想知道线程为什么离开 CPU | 对 `sched:sched_switch` 做 record（默认即每事件一次）+ `-g` 栈 | 用 `context-switches` 软件事件去 record（默认只采一部分） |
| 样本数比预期少很多 | 先 `-vv` 确认是不是频率采样，再查 lost events（`perf report -D \| tail -20`） | 直接下结论「代码没走那条路」 |
| 怀疑某个 PMC 数字不对 | 用原始事件描述符（`rNNN` / `cpu/event=…,umask=…/`）复核一次 | 相信可读名映射一定对 |
| 想知道「随时间有没有周期性抖动」 | `perf stat -I 1000` 逐行看 counts 列（是**区间值**不是累计值） | 只看一个总数 |
| 想知道负载是否均衡 | `perf stat -A`（另有 `--per-socket`、`--per-core`） | 看系统级 CPU 百分比 |
| 只关心某个 PID 的上下文切换 | `perf stat -e sched:sched_switch --filter 'prev_pid == N' -a`（内核侧过滤） | 全量录下来再筛 |
| 栈回溯断断续续 | 先查是否保留帧指针；换 `--call-graph dwarf`（需 dbgsym）或 `lbr` | 直接改应用代码 |
| 想要应用内部的**稳定**观测点 | 优先 USDT：`perf buildid-cache --add` → `perf list` → `perf probe` | 优先 uprobe（直插二进制，发版即失效） |
| USDT 探针零样本 | 先查是不是**信号量**没递增（Linux 4.20 起已修该问题） | 怀疑探针没建对 |
| 生产机没有 debuginfo | 用**严格同 ABI 同内核版本**的参考机 `-n -v` dry run 取寄存器位置 | 直接猜寄存器位置 |
| 要看实时、低开销的系统调用流 | `perf trace`（可 `--filter`，4.19 起默认不追其它系统调用） | `strace -f`（更贵、需逐进程 attach） |
| 要出「这段时间最热的栈」 | `perf script` + 火焰图（perf 内建支持需 Linux 5.8+） | 只交 `perf report` 汇总（时间维度已被抹平） |
| 要做逐指令取证 | `perf record -e intel_pt/cyc/u` + `perf script --insn-trace` | 日常性能分析（一个 `date(1)` 就是 26 万行） |
| 结果要发给别人 | `perf script --header -F <字段>`（含 pid，含元数据） | 直接贴默认字段输出（默认字段集合会随版本变） |
