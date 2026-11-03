---
title: Chapter 15. BPF（精读）
category: 性能工程
tags:
  - BPF
  - eBPF
  - BCC
  - bpftrace
  - 内核追踪
  - 可观测性
difficulty: 进阶
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）第15章 BPF"
source_section: "精读 章首导论（原文 §15 前置引言，无小节号）/ §15.1 / §15.1.1 / §15.1.3 / §15.1.4 / §15.1.5 / §15.1.6 / §15.1.7 / §15.1.8 / §15.2 / §15.2.1 / §15.2.3 / §15.2.4 / §15.2.5 / §15.2.6；§15.1.2 Tool Coverage 并入 §15.1.3（只说明 Figure 15.2 的单箭头/双箭头读法，不单立）；§15.2.2 Tools 并入 §15.2.3（只说明 Figure 15.3 的配色来源，不单立）；§15.3 References 不立篇（参考文献列表，不承载机制陈述）；§15.1.5 与 §15.2.4 / §15.2.5 下的无编号子标题按原文文字引用，不编 § 号"
skills:
  - obsidian-markdown
---

# Chapter 15. BPF

> **小节地图**：本章讲的是 **BPF 的两个追踪前端**——BCC（工具集 + 编程框架）与 bpftrace（自带一门小语言）。
> 立篇：章首导论（BPF 为什么可编程、为什么能在内核里聚合）、§15.1（BCC 是什么、装在哪、单用途工具与多用途工具的分界、one-liners、多工具示例、BCC 与 bpftrace 的取舍、文档）、
> §15.2（bpftrace 是什么、装在哪、one-liners 按目标分组、编程语法、参考手册、文档与版本差异）。
> **不立篇**：§15.1.2、§15.2.2（都是「图怎么看」的说明，并入相邻节）；§15.3 References（文献列表）。
> 底层依赖：扩展 BPF 的内核机制见 [[Chapter 3. Operating Systems]]（§3.4.4）；同族的两个追踪器见第 13 章 perf、第 14 章 Ftrace。
> 本篇的实践面：磁盘侧工具见第 9 章、文件系统侧见第 8 章、应用侧见第 5 章。

### 章首导论：BPF 为什么值得单独一章

**机制**：BPF（扩展 BPF）在第 3 章 §3.4.4 介绍过——它是一个内核执行环境，让追踪器拿到**可编程**的能力。
本章讲的是它的两个前端（BCC、bpftrace），以及这些工具在前几章里被用到的场景。

BPF 与其他追踪器的根本差别只有一条：

> What makes BPF different from other tracers is that it is programmable.

「可编程」具体意味着：用户定义的程序在事件上执行，能过滤、保存与取回信息、算延迟、做**内核内聚合**与自定义汇总。
为什么这件事重要，原文给的对比是：

> While other tracers may require dumping all events to user space and post-processing them, BPF allows such processing to occur efficiently in kernel context.

**这是本章所有机制的根**：其他追踪器要把事件全部导出到用户态再后处理，BPF 把处理留在内核上下文里，
于是「开销大到不能上生产」的那一类性能工具变得可行（**推演**：原文没有把「可编程 → 内核内聚合 → 生产可用」
串成显式论证链，但两句话确实是这个意思）。

BPF 工具能回答的问题，原文列了五类：**磁盘 I/O 延迟的直方图**、**CPU 调度器延迟**、**应用是否受文件系统延迟之苦**、
**有哪些 TCP 会话、各持续多久**、**哪些代码路径在阻塞、阻塞多久**。这五类几乎就是本书其余各章要回答的问题。

### 两个前端的定位

> The differences between BCC and bpftrace may be obvious from their usage in prior chapters: BCC is suited for complex tools, and bpftrace is suited for ad hoc custom programs.

Table 15.1 把两者的差异列成一张表（**表是原文的**）：

| 特性 | BCC | bpftrace |
|---|---|---|
| 各仓库工具数量 | >80 (bcc) | >30 (bpftrace)，另 >120 (bpf-perf-tools-book) |
| 工具用法 | 通常支持复杂选项（-h、-P PID 等）与参数 | 通常简单：无选项，零或一个参数 |
| 工具文档 | man page、示例文件 | man page、示例文件 |
| 编程语言 | 用户态 Python/Lua/C/C++，内核态 C | bpftrace |
| 编程难度 | 难 | 容易 |
| 每事件输出类型 | 任意 | 文本、JSON |
| 汇总类型 | 任意 | 计数、最小、最大、总和、平均、log2 直方图、线性直方图；可按零个或多个键分组 |
| 库支持 | 有（如 Python import） | 无 |
| 平均程序长度（不含注释） | 228 行 | 28 行 |

**「228 行 vs 28 行」这一栏是最直观的成本信号**：BCC 工具是要长期维护的工程，Bpftrace 工具是一次性脚本。

### 生产环境怎么用（Netflix 的做法）

原文给的现实案例：Facebook 和 Netflix 都在用两个前端；**Netflix 默认在所有云实例上安装它们**，
用在「云级监控与看板之后」的更深一层分析：

- **BCC 罐装工具**：命令行直接用，分析存储 I/O、网络 I/O 与进程执行；
- **BCC 工具被看板系统自动执行**：产出**调度器延迟热力图、磁盘 I/O 延迟热力图、off-CPU 火焰图**；
- **一个常驻 BCC 守护进程**：基于 `tcplife(8)`，把网络事件记到云存储，做流分析；
- **bpftrace**：需要时**定制** bpftrace 工具，去理解内核与应用层面的病理。

**读法（推演）**：这条分工线就是「**已知的问题用罐装 BCC，未知的问题用自写 bpftrace**」——
Netflix 把罐装工具接进自动化看板（稳定、可重复），把探索性工作交给 bpftrace（快、可弃）。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| 章首导论 | 15-Chapter 15. BPF.md L41 | What makes BPF different from other tracers is that it is programmable. |
| 章首导论 | 15-Chapter 15. BPF.md L41 | While other tracers may require dumping all events to user space and post-processing them, BPF allows such processing to occur efficiently in kernel context. |
| 章首导论 | 15-Chapter 15. BPF.md L69 | The differences between BCC and bpftrace may be obvious from their usage in prior chapters: BCC is suited for complex tools, and bpftrace is suited for ad hoc custom programs. |
| §15.1 BCC 是什么 | 15-Chapter 15. BPF.md L153 | The BPF Compiler Collection (or “bcc” after the project and package names) is an open-source project containing a large collection of advanced performance analysis tools, as well as a framework for building them. |
| §15.1 BCC 是什么 | 15-Chapter 15. BPF.md L181 | This tool uses BPF to summarize the histograms in kernel space for efficiency, so the user-space component only needs to read the already-summarized histograms (the count columns) and print them. |
| §15.1.1 安装 | 15-Chapter 15. BPF.md L193 | The INSTALL.md also lists kernel configuration requirements (which include CONFIG_BPF=y, CONFIG_BPF_SYSCALL=y, CONFIG_BPF_EVENTS=y). BCC requires at least Linux 4.4 for some of the tools to work; for most of the tools, 4.9 or newer is required. |
| §15.1.3 单用途工具 | 15-Chapter 15. BPF.md L207 | You can “just run biolatency” without needing to learn any command line options, and usually get just enough output to solve your problem without clutter. |
| §15.1.4 多用途工具 | 15-Chapter 15. BPF.md L481 | The multi-purpose tools are listed on the left of Figure 15.2 . These support multiple event sources and can perform many roles, similar to perf(1), although this also makes them complex to use. |
| §15.1.7 BCC vs. bpftrace | 15-Chapter 15. BPF.md L689 | BCC allows the BPF program at the heart of the tracing tool to be developed in C, enabling full control. This comes at the cost of complexity: BCC tools can take ten times as long to develop as bpftrace tools, and can have ten times as many lines of code. |
| §15.1.7 BCC vs. bpftrace | 15-Chapter 15. BPF.md L691 | The difference between BCC and bpftrace is like the difference between C programming and shell scripting, where BCC is like C programming (some of it is C programming) and bpftrace is like shell scripting. |
| §15.2 bpftrace 是什么 | 15-Chapter 15. BPF.md L750 | The language has been designed to be simple and easy to learn. It is the awk(1) of tracing, and is based on awk(1). In awk(1), you write a program stanza to process an input line, and with bpftrace you write a program stanza to process an input event. |
| §15.2 bpftrace 是什么 | 15-Chapter 15. BPF.md L777 | Using a concise syntax, this bpftrace one-liner has used a kretprobe to instrument tcp_recvmsg(), filtered for when the return value is positive (to exclude negative error codes), and populated a BPF map object called @recv_bytes with a histogram of the return value, saved using the process name ( comm ) as a key. When Ctrl-C is typed and bpftrace receives the signal (SIGINT) it ends and automatically prints out BPF maps. |
| §15.2.1 安装 | 15-Chapter 15. BPF.md L789 | For documentation on these options see INSTALL.md in the bpftrace repository [Iovisor 20a], which also lists kernel requirements (which include CONFIG_BPF=y, CONFIG_BPF_SYSCALL=y, CONFIG_BPF_EVENTS=y). bpftrace requires Linux 4.9 or newer. |
| §15.2.4 探针格式 | 15-Chapter 15. BPF.md L1127 | The kprobe probe type instruments kernel function calls, and only needs one identifier: the kernel function name. The uprobe probe type instruments user-level function calls, and needs both the path to the binary and the function name. |
| §15.2.4 探针通配符 | 15-Chapter 15. BPF.md L1152 | Instrumenting too many probes may cost unnecessary performance overhead. To avoid hitting this by accident, bpftrace has a tunable maximum number of probes it will enable, set via the BPFTRACE_MAX_PROBES environment variable (it currently defaults to 512 |
| §15.2.4 Map 函数 | 15-Chapter 15. BPF.md L1304 | However, this uses a global CPU map, instead of a per-CPU map, to provide @x as an integer. This global integer type is sometimes necessary for some programs that require an integer and not a count, but bear in mind that there may be a small error margin due to concurrent updates. |
| §15.2.4 计时 vfs_read() | 15-Chapter 15. BPF.md L1358 | This times the duration of the vfs_read() kernel function by instrumenting its start using a kprobe and storing a timestamp in a @start hash keyed on thread ID, and then instrumenting its end by using a kretprobe and calculating the delta as: now - start. |
| §15.2.4 计时 vfs_read() | 15-Chapter 15. BPF.md L1358 | A filter is used to ensure that the start time was recorded; otherwise, the delta calculation becomes bogus for vfs_read() calls that were in progress when tracing begins, as the end is seen but not the start (the delta would become: now - 0). |
| §15.2.5 探针频率 | 15-Chapter 15. BPF.md L1506 | As a rough guide for the computers of today, I would consider less than 100k kprobe or tracepoint events per second to be low frequency. |
| §15.2.5 异步语义 | 15-Chapter 15. BPF.md L1905 | The asynchronous functions are printf() , time() , cat() , join() , and system() . The functions kstack() , ustack() , ksym() , and usym() record addresses synchronously, but do symbol translation asynchronously. |

> **书里没说的**（本篇有几处推演/归纳，原文没有，别当成原文观点）：
> - **「BPF 的价值 = 把事件处理留在内核」这条因果链**是本篇从原文两句拼起来的：原文只说 BPF「可编程」
>   （L41 前半句）与「可在内核上下文里高效处理」（L41 后半句），**没有**把它展开成「因此生产环境可用」的显式论证。
> - **「§15.1 与 §15.2 的分工」是本篇的归纳**：原文的表述是「BCC 适合复杂工具，bpftrace 适合临时自定义程序」（L69），
>   并给出了 C 与 shell 脚本的类比（L691）；「按开发速度分工」「先 bpftrace 写、成熟后再移植 BCC」这两条
>   **是原文给的工作流建议**（L689 后半句），但「工具平均长度 228 行 vs 28 行」是 Table 15.1 里的统计，
>   原文**没有**说这是开发速度差异的原因。
> - **「100k 事件/秒 = 低频」是原文的粗略经验值**（L1506，原文自称 "a rough guide"、且明说取决于 CPU 速度、
>   事件数、headroom 与插桩成本），**不是**通用阈值。
> - **本篇加的「内核态聚合 vs 用户态后处理」开销对比表**是推演：原文只说 BCC 工具在**内核空间**做直方图汇总、
>   用户态只读已汇总结果（L181），**没有**给出两者的开销数字。

## §15.1 BCC：工具集与编程框架

### §15.1 BCC 是什么

> The BPF Compiler Collection (or “bcc” after the project and package names) is an open-source project containing a large collection of advanced performance analysis tools, as well as a framework for building them.

**两个身份**：① 一大批高级性能分析工具；② 一个**用来造这些工具的框架**。由 Brendon Blanco 创建，
作者也参与了开发并写了其中许多工具。

原文用 `biolatency(8)` 举例（磁盘 I/O 延迟的 2 的幂直方图，可按 I/O flags 拆分）：

```bash
# biolatency.py -mF
Tracing block device I/O... Hit Ctrl-C to end.
^C

flags = Priority-Metadata-Read
     msecs               : count     distribution
         0 -> 1          : 90       |****************************************|

flags = Write
     msecs               : count     distribution
         0 -> 1          : 24       |****************************************|
         2 -> 3          : 0        |                                        |
         4 -> 7          : 8        |*************                           |

flags = ReadAhead-Read
     msecs               : count     distribution
         0 -> 1          : 3031     |****************************************|
         2 -> 3          : 10       |                                        |
         4 -> 7          : 5        |                                        |
         8 -> 15         : 3        |                                        |
```

输出本身就说明了工具的设计意图：**写延迟是双峰的**（0→1 与 4→7 两个峰），而且**大量 I/O 带 "ReadAhead-Read" flag**——
即预读把很多 I/O 变成了顺序读。双峰这件事用 `iostat` 的平均值是看不出来的（见第 9 章）。

**开销机制在这一句里**：

> This tool uses BPF to summarize the histograms in kernel space for efficiency, so the user-space component only needs to read the already-summarized histograms (the count columns) and print them.

**汇总发生在内核里，用户态只负责打印。** 这就是「BPF 可编程」在工具层面的兑现方式：
如果直方图在用户态算，每个事件都要跨界传一次。

### §15.1.1 安装

包在多数发行版都有（Ubuntu、Debian、RHEL、Fedora、Amazon Linux），装起来很trivial；
包名按维护者习惯叫 **"bcc-tools"、"bpfcc-tools" 或 "bcc"**。也可以从源码构建。

> The INSTALL.md also lists kernel configuration requirements (which include CONFIG_BPF=y, CONFIG_BPF_SYSCALL=y, CONFIG_BPF_EVENTS=y). BCC requires at least Linux 4.4 for some of the tools to work; for most of the tools, 4.9 or newer is required.

**两个硬门槛**：内核必须开 `CONFIG_BPF=y`、`CONFIG_BPF_SYSCALL=y`、`CONFIG_BPF_EVENTS=y`；
版本上 BCC 至少要 **Linux 4.4**（部分工具），多数工具要 **4.9+**。

### §15.1.3 单用途工具（§15.1.2 工具覆盖图并入本节）

Figure 15.2 里的工具按**箭头形状**分两类：**单箭头 = 单用途工具**；**左侧带双箭头的 = 多用途工具**（双箭头表示覆盖面）。

单用途工具的设计哲学：

> You can “just run biolatency” without needing to learn any command line options, and usually get just enough output to solve your problem without clutter.

三条设计约束：① **默认输出简洁、往往已经够用**；② **不需要先学命令行选项**；
③ **选项是留给定制的**（例如 `biolatency(8) -F` 按 I/O flags 拆分）。

Table 15.2 列了 40 余个单用途工具及其在本书的位置，其中承重的几类（**表为原文**）：

| 工具 | 做什么 | 本书位置 |
|---|---|---|
| `biolatency(8)` | 块 I/O 延迟直方图 | 9.6.6 |
| `biotop(8)` | 按进程汇总块 I/O | 9.6.8 |
| `biosnoop(8)` | 逐条跟踪块 I/O（含延迟） | 9.6.7 |
| `btrfsdist(8)` / `btrfsslower(8)` | btrfs 操作延迟直方图 / 慢操作 | 8.6.13 / 8.6.14 |
| `cpudist(8)` | 按进程汇总 on-CPU 与 off-CPU 时间直方图 | 6.6.15 |
| `drsnoop(8)` | 跟踪直接内存回收事件（含 PID 与延迟） | 7.5.11 |
| `execsnoop(8)` | 通过 execve(2) 跟踪新进程 | 1.7.3、5.5.5 |
| `ext4dist(8)` / `ext4slower(8)` | ext4 延迟直方图 / 慢操作 | 8.6.13 / 8.6.14 |
| `offcputime(8)` | 按栈汇总 off-CPU 时间 | 5.5.3 |
| `profile(8)` | 用定时采样对栈做 CPU profiling | 5.5.2 |
| `runqlat(8)` / `runqlen(8)` | run queue 延迟直方图 / 长度 | 6.6.16 / 6.6.17 |
| `syscount(8)` | 系统调用计数与延迟汇总 | 5.5.6 |
| `tcplife(8)` / `tcpretrans(8)` / `tcptop(8)` | TCP 会话生命周期 / 重传 / 按主机与 PID 的吞吐 | 10.6.9 / 10.6.11 / 10.6.10 |
| `xfsdist(8)` / `xfsslower(8)` / `zfsdist(8)` / `zfsslower(8)` | 各文件系统的延迟直方图 / 慢操作 | 8.6.13 / 8.6.14 |

**命名规律（推演，但 40 余个条目无一例外）**：`xxxdist` = **汇**总成直方图，`xxxslower` = **逐条追踪**慢操作，
`xxxsnoop` = **逐条**跟踪事件，`xxxlatency` = 延迟直方图。
所以「我要分布」用 dist、「我要看慢的那几条」用 slower、「我要逐条流水」用 snoop。

### §15.1.4 多用途工具

> The multi-purpose tools are listed on the left of Figure 15.2 . These support multiple event sources and can perform many roles, similar to perf(1), although this also makes them complex to use.

**机制**：多用途工具支持**多个事件源**、能扮演多种角色，用法像 `perf(1)`——
代价就是「复杂」。Table 15.3 的六款：`argdist(8)`（函数参数值的直方图或计数）、
`funccount(8)`（函数调用计数）、`funcslower(8)`（慢函数跟踪）、`funclatency(8)`（函数延迟直方图）、
`stackcount(8)`（导致某事件的栈计数）、`trace(8)`（带过滤的任意函数跟踪）。

### §15.1.5 One-Liners（按工具分组）

这些一行命令默认**全系统跟踪直到 Ctrl-C**。它们是「记不住参数时的速查表」：

`funccount(8)` —— 数函数调用：

```bash
funcgraph 'vfs_*'
funccount 'tcp_*'
funccount -i 1 'tcp_send*'
funccount -i 1 't:block:*'
funccount -i 1 c:getaddrinfo
```

`stackcount(8)` —— 数「导致某件事的栈」：

```bash
stackcount t:block:block_rq_insert
stackcount -P ip_output
stackcount t:sched:sched_switch
```

`trace(8)` —— 跟踪函数并打印参数：

```bash
trace 'do_sys_open "%s", arg2'
trace 'r::do_sys_open "ret: %d", retval'
trace -U 'do_nanosleep "mode: %d", arg2'
trace 'pam:pam_start "%s: %s", arg1, arg2'
```

`argdist(8)` —— 按参数值做直方图或计数：

```bash
argdist -H 'r::vfs_read()'
argdist -p 1005 -H 'r:c:read()'
argdist.py -C 't:raw_syscalls:sys_enter():int:args->id'
argdist -C 'p::tcp_sendmsg(struct sock *sk, struct msghdr *msg, size_t size):u32:size'
argdist -H 'p::tcp_sendmsg(struct sock *sk, struct msghdr *msg, size_t size):u32:size'
argdist -p 181 -C 'p:c:write(int fd):int:fd'
argdist -C 'r::__vfs_read():u32:$PID:$latency > 100000
```

**读法（推演）**：这四条 one-liner 的前缀语法是一致的——
`p:`（入口）、`r:`（返回）、`r::`（返回且函数名带命名空间）、`t:`（tracepoint）、`c:`（libc）、
`u:`（USDT）、`k:`（内核）、`kr:`（内核返回）；后缀 `-H` 是**直方图**、`-C` 是**计数**；
`-P PID` 限定进程；`-i 1` 是每秒汇总一次。

### §15.1.6 多工具示例

用多用途工具 `trace(8)` 跟踪内核函数 `do_sys_open()` 并把第二个参数当字符串打印：

```bash
# trace 'do_sys_open "%s", arg2'
PID     TID     COMM        FUNC             -
28887   28887   ls          do_sys_open      /etc/ld.so.cache
28887   28887   ls          do_sys_open      /lib/x86_64-linux-gnu/libselinux.so.1
28887   28887   ls          do_sys_open      /lib/x86_64-linux-gnu/libc.so.6
28887   28887   ls          do_sys_open      /lib/x86_64-linux-gnu/libpcre2-8.so.0
28887   28887   ls          do_sys_open      /lib/x86_64-linux-gnu/libdl.so.2
28887   28887   ls          do_sys_open      /lib/x86_64-linux-gnu/libpthread.so.0
28887   28887   ls          do_sys_open      /proc/filesystems
28887   28887   ls          do_sys_open      /usr/lib/locale/locale-archive
[...]
```

**机制**：`trace` 的语法**仿 `printf(3)`**——支持格式串 + 参数；这里 `arg2` 被当字符串打印，
是因为它装的就是文件名。原文的补充：`trace(8)` 与 `argdist(8)` 都支持这种语法，
所以能用它们拼出大量自定义 one-liner；而 bpftrace 把它推进一步，**提供一门完整的语言**来写一行或多行程序。

### §15.1.7 BCC vs. bpftrace：这一节是全章的选型判据

> BCC allows the BPF program at the heart of the tracing tool to be developed in C, enabling full control. This comes at the cost of complexity: BCC tools can take ten times as long to develop as bpftrace tools, and can have ten times as many lines of code.

**收益**：BCC 允许用 **C** 写工具内核里的 BPF 程序，**完全的控制力**。
**代价**：复杂度——开发时间可能是 bpftrace 工具的**十倍**，代码行数也可能是**十倍**。

作者的类比：

> The difference between BCC and bpftrace is like the difference between C programming and shell scripting, where BCC is like C programming (some of it is C programming) and bpftrace is like shell scripting.

**类比的落点**：不是「C 比 shell 强大」这种泛泛之谈，而是**开发速度 vs 控制力**这条轴。
原文还给了一条**工作流建议**（L689 后半句）：因为开发一个工具通常要迭代多轮，
**先用 bpftrace 快速写，需要时再移植到 BCC**。作者自己的日常用法也印证了这一点：
大量使用预制的 C 程序（`top(1)`、`vmstat(1)`），也写大量一次性的 shell 脚本——
BCC 与 bpftrace 在他这里是同一个分工。

**「什么该用哪个」的推演**（原文未给决策表）：

| 你要什么 | 选谁 |
|---|---|
| 一个已知的问题、已知的口径（磁盘延迟直方图、off-CPU 时间） | **BCC 罐装工具**（已有 man page 与示例文件） |
| 一个没头绪的问题，需要自己定义「算什么」 | **bpftrace one-liner**（几秒出一条） |
| 要长期维护、要进看板、要进 CI 的工具 | **BCC**（C 的可维护性 + 完全控制力） |
| 要在多个环境里快速复现一段分析 | **bpftrace 脚本**（一行一文件，无依赖） |
| 需要复杂库（Python 生态）、需要任意输出格式 | **BCC**（Python/Lua/C/C++，库支持=yes） |

### §15.1.8 文档

三层文档，随手可查：

1. **`-h` 用法摘要**（每个工具都有，例如 `funccount -h` 会打印完整用法、选项与示例）；
2. **man page**：仓库里 `man/man8/funccount.8`；
3. **examples 文件**：仓库里 `examples/funccount_example.txt`，含**带注释的输出示例**——
   这是最实用的那一层，因为它示范的是「输出长什么样、该怎么读」。

仓库里另有：`docs/tutorial.md`（终端用户教程）、`docs/tutorial_bcc_python_developer.md`（BCC Python 开发者教程）、
`docs/reference_guide.md`（参考指南）。作者另一本书《BPF Performance Tools》第 4 章专门讲 BCC。

## §15.2 bpftrace：一门小语言 + 一套工具

### §15.2 bpftrace 是什么

> The language has been designed to be simple and easy to learn. It is the awk(1) of tracing, and is based on awk(1). In awk(1), you write a program stanza to process an input line, and with bpftrace you write a program stanza to process an input event.

**身份**：bpftrace 是**构建在 BPF 与 BCC 之上**的开源追踪器，既提供一批工具，也提供一门高层语言。
**语言定位**：**「追踪界的 awk(1)」**——在 awk 里你写一个程序段处理一行输入，
在 bpftrace 里你写一个程序段处理**一个事件**。由 Alastair Robertson 创建，作者是主要贡献者。

原文用一条 one-liner 示范，并逐句拆解了它：

> Using a concise syntax, this bpftrace one-liner has used a kretprobe to instrument tcp_recvmsg(), filtered for when the return value is positive (to exclude negative error codes), and populated a BPF map object called @recv_bytes with a histogram of the return value, saved using the process name ( comm ) as a key. When Ctrl-C is typed and bpftrace receives the signal (SIGINT) it ends and automatically prints out BPF maps.

对应的命令与输出（**原文照抄**）：

```bash
# bpftrace -e 'kr:tcp_recvmsg /retval >= 0/ { @recv_bytes[comm] = hist(retval); }'
Attaching 1 probe...
^C

@recv_bytes[sshd]:
[32, 64)               7 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[64, 128)              2 |@@@@@@@@@@@@@@                                      |
@recv_bytes[nodejs]:
[0]                   82 |@@@@@@@@@@@@@@@@@@@@@@@@@@                          |
[1]                  135 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@        |
[2, 4)               153 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@  |
[4, 8)                12 |@@@                                                 |
[8, 16)                6 |@                                                   |
[16, 32)              32 |@@@@@@@@@@                                          |
[32, 64)             158 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[64, 128)            155 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ |
[128, 256)            14 |@@@@                                                |
```

**四个要点**（按原文的拆解顺序）：

1. **探针类型**：`kr:` = kretprobe，插桩 `tcp_recvmsg()` 的**返回**；
2. **过滤器**：`/retval >= 0/` 排除负的错误码——**不加这个过滤，错误返回会污染延迟分布**；
3. **Map 与键**：`@recv_bytes[comm] = hist(retval)` —— 把返回值的直方图存进 BPF map 对象 `@recv_bytes`，
   **以进程名 `comm` 为键**（于是同一台机上不同进程的分布被分开）；
4. **收尾**：收到 **SIGINT**（Ctrl-C）即结束，并**自动打印所有 BPF map**。

**输出读法**：nodejs 的接收尺寸是**双峰**的——一个峰在 0–4 字节，另一个在 32–128 字节。
这跟第 10 章里「nodejs 收小包与大包混着来」的判断是同一件事。

### §15.2.1 安装

包覆盖 Ubuntu、Fedora、Gentoo、Debian、OpenSUSE、CentOS（**RHEL 8.2 上是 Technology Preview**）。
除包之外还有三种形态：**Docker 镜像**、**只依赖 glibc 的独立二进制**、以及从源码构建。

> For documentation on these options see INSTALL.md in the bpftrace repository [Iovisor 20a], which also lists kernel requirements (which include CONFIG_BPF=y, CONFIG_BPF_SYSCALL=y, CONFIG_BPF_EVENTS=y). bpftrace requires Linux 4.9 or newer.

**内核要求与 BCC 相同**（`CONFIG_BPF=y`、`CONFIG_BPF_SYSCALL=y`、`CONFIG_BPF_EVENTS=y`），
**版本要求更高**：bpftrace 需要 **Linux 4.9 或更新**。

### §15.2.3 One-Liners（按目标分组；§15.2.2 工具图并入本节）

Figure 15.3 的配色：**黑色 = bpftrace 仓库自带的工具**；**红/灰 = 作者在《BPF Performance Tools》
仓库里另做的工具**。原文明确说这些 one-liner 除了有用，**同时也是这门语言的迷你示例**。

**CPUs**

```bash
bpftrace -e 'tracepoint:syscalls:sys_enter_execve { join(args->argv); }'
bpftrace -e 'tracepoint:raw_syscalls:sys_enter { @[pid, comm] = count(); }'
bpftrace -e 'profile:hz:49 /pid == 189/ { @[ustack] = count(); }'
```

**Memory**

```bash
bpftrace -e tracepoint:syscalls:sys_enter_brk { @[ustack, comm] = count(); }
bpftrace -e 'tracepoint:exceptions:page_fault_user { @[ustack, comm] =
    count(); }'
bpftrace -e 'tracepoint:vmscan:* { @[probe]++; }'
```

**File Systems**

```bash
bpftrace -e 't:syscalls:sys_enter_openat { printf("%s %s\n", comm,
    str(args->filename)); }'
    str(args->filename)); }'
bpftrace -e 'tracepoint:syscalls:sys_exit_read { @ = hist(args->ret); }'
bpftrace -e 'kprobe:vfs_* { @[probe] = count(); }'
bpftrace -e 'tracepoint:ext4:* { @[probe] = count(); }'
```

**Disk**

```bash
bpftrace -e 't:block:block_rq_issue { @bytes = hist(args->bytes); }'
bpftrace -e 't:block:block_rq_issue { @[ustack] = count(); }'
bpftrace -e 't:block:block_rq_issue { @[args->rwbs] = count(); }'
```

**Networking**

```bash
bpftrace -e 't:syscalls:sys_enter_accept* { @[pid, comm] = count(); }'
bpftrace -e 'kr:sock_sendmsg,kr:sock_recvmsg /retval > 0/ {
    @[pid, comm] = sum(retval); }'
bpftrace -e 'k:tcp_sendmsg { @send_bytes = hist(arg2); }'
bpftrace -e 'kr:tcp_recvmsg /retval >= 0/ { @recv_bytes = hist(retval); }'
bpftrace -e 'k:udp_sendmsg { @send_bytes = hist(arg2); }'
```

**Applications**

```bash
bpftrace -e 'u:/lib/x86_64-linux-gnu/libc-2.27.so:malloc { @[ustack(5)] =
    sum(arg0); }'
bpftrace -e 't:syscalls:sys_enter_kill { printf("%s -> PID %d SIG %d\n",
    comm, args->pid, args->sig); }'
    comm, args->pid, args->sig); }'
```

**Kernel**

```bash
bpftrace -e 'tracepoint:raw_syscalls:sys_enter {
    @[ksym(*(kaddr("sys_call_table") + args->id * 8))] = count(); }'
bpftrace -e 'kprobe:attach* { @[probe] = count(); }'
bpftrace -e 'kprobe:vfs_write { @[arg2] = count(); }'
bpftrace -e 'k:vfs_read { @ts[tid] = nsecs; } kr:vfs_read /@ts[tid]/ {
    @ = hist(nsecs - @ts[tid]); delete(@ts[tid]); }'
bpftrace -e 't:sched:sched_switch { @[kstack, ustack, comm] = count(); }'
bpftrace -e 'profile:hz:99 /pid/ { @[kstack] = count(); }'
```

**读法（推演）**：这一组 one-liner 覆盖了本书的几乎全部问题类型，可见 bpftrace 的表达力已经够用；
其中 `@[...] = count()`（按维度计数）、`hist()`（2 的幂直方图）、`sum()`（按维度求和）、
`ustack`/`kstack`（按栈归因）这四个 map 函数，就是**所有 bpftrace 分析的骨架**。

### §15.2.4 编程：语法与数据结构

原文这一节的写法是**照着 awk 原始论文的六页篇幅组织的**；语言本身受 **awk 和 C** 以及
**DTrace、SystemTap** 影响。

#### 1. Usage

`bpftrace -e program` 执行程序并插桩它定义的事件，程序**一直跑到 Ctrl-C 或显式调用 `exit()`**。
以 `-e` 参数运行的程序叫 **one-liner**；也可以存成文件再跑：`bpftrace file.bt`。
`.bt` 扩展名不是必需的，但便于识别。在文件顶部放一行解释器声明：

```bash
#!/usr/local/bin/bpftrace
```

文件就能 `chmod a+x` 后当普通程序执行（`./file.bt`）。
**权限要求**：bpftrace **必须由 root（超级用户）执行**；有的环境直接用 root shell，
有的偏好用 `sudo(1)`（`sudo ./file.bt`）。原文脚注说明：bpftrace 检查的是 **UID 0**，
未来版本可能改成检查更细的权限。

#### 2. Program Structure

程序是一串「探针 + 动作」：

```bash
probes { actions }
probes /filter/ { actions }
```

探针触发时执行对应动作；动作前可以带一个**可选的过滤表达式**，只有过滤为真才执行。
这与 awk 的 `/pattern/ { actions }` 同形。多个动作块之间**执行顺序不保证**。

#### 3. Comments

单行注释用 `//`，多行注释用 C 风格的 `/* ... */`（也可用作行内注释片段）。

#### 4. Probe Format

> The kprobe probe type instruments kernel function calls, and only needs one identifier: the kernel function name. The uprobe probe type instruments user-level function calls, and needs both the path to the binary and the function name.

**格式**：`type:identifier1[:identifier2[...]]`，层级由探针类型决定——
`kprobe:vfs_read` 只要一个标识符（内核函数名），`uprobe:/bin/bash:readline` 要两个（二进制路径 + 函数名）。
多个探针可用逗号分隔共用一组动作：`probe1,probe2,... { actions }`。
另有两个特殊类型：**`BEGIN`**（程序开始时）与 **`END`**（程序结束时）触发，行为像 awk。

#### 5. Probe Wildcards

部分探针类型支持通配符（`kprobe:vfs_*` 会插桩所有以 "vfs_" 开头的内核函数）。
原文给了一个**很实用的护栏**：

> To avoid hitting this by accident, bpftrace has a tunable maximum number of probes it will enable, set via the BPFTRACE_MAX_PROBES environment variable (it currently defaults to 512

**护栏**：探针插桩太多会带来不必要的性能开销，bpftrace 用 `BPFTRACE_MAX_PROBES` 环境变量
限制最多启用多少探针，**当前默认 512**。原文脚注补充：超过 512 目前会让 bpftrace 启停变慢
（因为它是一个一个插桩的）；未来内核侧计划做批量插桩，届时这个上限可以大幅提高甚至取消。

**先验证再跑**：用 `bpftrace -l` 列出通配符匹配到的探针——

```bash
# bpftrace -l 'kprobe:vfs_*'
kprobe:vfs_fallocate
kprobe:vfs_truncate
kprobe:vfs_open
kprobe:vfs_setpos
kprobe:vfs_llseek
[…]
bpftrace -l 'kprobe:vfs_*' | wc -l
56
```

**探针名要加引号**，否则会被 shell 展开。

#### 6. Filters

过滤器是布尔表达式，闸住动作是否执行。`/pid == 123/` 只在 pid 等于 123 时执行。
**只写变量名时，含义是「非零」**——`/pid/` 等价于 `/pid != 0/`。
过滤器可以用布尔运算符组合，例如 `/pid > 100 && pid < 1000/` 要求两个表达式都为真。

#### 7. Actions

动作可以是单条语句，也可以是用分号隔开的多条，**最后一条也可以带分号**：

```bash
{ action one; action two; action three }
{ $x = 42; printf("$x is %d", $x); }
```

语句写在 bpftrace 语言里，与 C 语言相似：可以操作变量、调用 bpftrace 函数。

#### 8. Hello, World!

```bash
# bpftrace -e 'BEGIN { printf("Hello, World!\n"); }'
Attaching 1 probe...
Hello, World!
^C
```

写成文件时，`BEGIN` 与 `{...}` 分行并缩进只是可读性考虑，不是必须。

#### 9. Functions

除 `printf()` 外，常用的还有：**`exit()`**（退出 bpftrace）、**`str(char *)`**（从指针取字符串）、
**`system(format[, arguments ...])`**（在 shell 里跑命令）。
例如 `printf("got: %llx %s\n", $x, str($x)); exit();`
先按十六进制打印 `$x`，再当作 NULL 结尾的字符数组打印，然后退出。

#### 10. Variables：三类

**① 内置变量（built-ins）**：bpftrace 预定义、通常**只读**。重要的有
`pid`（进程 ID）、`tid`（线程 ID）、`uid`、`username`、`comm`（进程名）、
`nsecs`（纳秒时间戳）、`elapsed`（自初始化起的纳秒数）、`cpu`、`kstack`/`ustack`、
`arg0..argN`、`args`、`retval`、`func`、`probe`、`curtask`、`cgroup`。
**所有整数目前都是 uint64**；这些变量都指向**探针触发那一刻**的线程、探针、函数与 CPU。

**② 临时变量（scratch，`$` 前缀）**：名字与类型**在首次赋值时确定**：

```bash
$x = 1;
$y = “hello”;
$z = (struct task_struct *)curtask;
```

上面分别声明 `$x` 为整数、`$y` 为字符串、`$z` 为指向 `struct task_struct` 的指针。
**临时变量只能在赋值它的那一个动作块里使用**；引用一个没有赋过值的变量，bpftrace 会报错——这反而帮你抓拼写错误。

**③ Map 变量（`@` 前缀）**：用 BPF map 存储对象，可以做**全局存储、在动作之间传数据**：

```bash
probe1 { @a = 1; }
probe2 { $x = @a; }
```

`@a` 在 probe1 触发时被置 1，probe2 触发时读出。原文给的边界条件：
**如果 probe1 先触发、probe2 后触发，`$x` 是 1；否则是 0（未初始化）**。

**键（key）**：map 可以有一个或多个键元素，**当作哈希表（关联数组）**用。
最常见的是 `@start[tid] = nsecs;`——把 `nsecs` 存进名为 `@start` 的 map，
**以当前线程 ID 为键**，这样各线程各存各的时间戳，**不会被别的线程覆盖**。
多键的例子：`@path[pid, $fd] = str(arg0);`（同时用 `pid` 内置变量与 `$fd` 临时变量做键）。

#### 11. Map Functions

```bash
@x = count();
@x++;
@y = sum($x);
@z = hist($x);
print(@x);
delete(@start[tid]);
```

**`count()` 与 `++` 的差别，是这一节唯一需要小心的地方**：

> However, this uses a global CPU map, instead of a per-CPU map, to provide @x as an integer. This global integer type is sometimes necessary for some programs that require an integer and not a count, but bear in mind that there may be a small error margin due to concurrent updates.

即：`@x = count();` 用的是 **per-CPU map**，`@x` 成为**类型为 count 的特殊对象**；
而 `@x++;` 用的是**全局 CPU map**，`@x` 是**一个整数**——
这个全局整数类型有时是必需的（程序要整数而不是 count），但**由于并发更新，可能有小的误差**。

**其余四个**：`sum($x)` 求和；`hist($x)` 存 2 的幂直方图（打印时给桶计数与 ASCII 直方图）；
`print(@x)` 打印这个 map（例如挂在 interval 事件上）——**不常用**，因为 bpftrace 终止时会自动打印所有 map；
`delete(@start[tid])` 删掉 `@start` 里键为 `tid` 的键值对。
原文脚注补充了「终止时自动打印」的理由：**运行时打印开销更高**，因为 map 正在被更新，会拖慢 map 遍历。

#### 12. Timing vfs_read()：把语法用起来

**原文照抄的程序**：

```bash
#!/usr/local/bin/bpftrace

// this program times vfs_read()

kprobe:vfs_read
{
        @start[tid] = nsecs;
}

kretprobe:vfs_read
/@start[tid]/
{
        $duration_us = (nsecs - @start[tid]) / 1000;
        @us = hist($duration_us);
        delete(@start[tid]);
}
```

**机制**（原文拆成两句话，这是本章最值得记住的一段）：

> This times the duration of the vfs_read() kernel function by instrumenting its start using a kprobe and storing a timestamp in a @start hash keyed on thread ID, and then instrumenting its end by using a kretprobe and calculating the delta as: now - start.

> A filter is used to ensure that the start time was recorded; otherwise, the delta calculation becomes bogus for vfs_read() calls that were in progress when tracing begins, as the end is seen but not the start (the delta would become: now - 0).

**三处设计各司其职**：

1. **入口探针（kprobe）**：把开始时间存进 `@start`，**以线程 ID 为键**；
2. **返回探针（kretprobe）**：算 `now - start`，除以 1000 换成微秒；
3. **过滤器 `/@start[tid]/`**：**保证开始时间确实被记录过**——
   否则追踪刚开始时「已经在飞」的调用，只看得到结束、看不到开始，**delta 会变成 now - 0**，
   污染出一个巨大的假延迟。**这是 kprobe/kretprobe 配对计时的通用陷阱**（推演：原文只举了 vfs_read 这一个例子，
   但「只看到尾、看不到头」的成因与 vfs_read 无关，对任何成对探针都成立）。

**输出示例**（原文）：

```bash
# bpftrace vfsread.bt
Attaching 2 probes...
^C

@us:
[0]                   23 |@                                                   |
[1]                  138 |@@@@@@@@@                                           |
[2, 4)               538 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@               |
[4, 8)               744 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[8, 16)              641 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@        |
[16, 32)             122 |@@@@@@@@                                            |
[32, 64)              13 |                                                    |
[64, 128)             17 |@                                                   |
[128, 256)             2 |                                                    |
[256, 512)             0 |                                                    |
[512, 1K)              1 |                                                    |
```

**命名即文档**：这个直方图 map 命名为 "us"，是因为**map 名字会被打印出来**——
原文的建议是给 map 起有意义的、带单位的名字（"bytes"、"latency_ns"），
让输出**自带注解**、可自我解释。

**可定制性是 bpftrace 相对传统工具的核心优势**：把 `hist()` 那行改成

```bash
@us[pid, comm] = hist($duration_us);
```

就得到**每个 (PID, 进程名) 一份直方图**。原文的对比：传统工具（`iostat(1)`、`vmstat(1)`）的输出是**固定的、无法轻易定制**；
而 bpftrace 让你**把指标继续拆细、并与其他探针的指标合并**，直到拿到需要的答案。
更完整的例子见第 8 章 §8.6.15（BPFtrace → VFS Latency Tracing，按文件系统/套接字等类型再拆 vfs_read() 延迟）。

### §15.2.5 参考

#### 1. Probe Types

Table 15.4 的探针类型与快捷别名（**表为原文**）：

| Type | 快捷 | 作用 |
|---|---|---|
| `tracepoint` | `t` | 内核静态插桩点 |
| `usdt` | `U` | 用户态静态定义追踪 |
| `kprobe` | `k` | 内核动态函数插桩 |
| `kretprobe` | `kr` | 内核动态函数返回插桩 |
| `kfunc` | `f` | 内核动态函数插桩（基于 BPF） |
| `kretfunc` | `fr` | 内核动态函数返回插桩（基于 BPF） |
| `uprobe` | `u` | 用户态动态函数插桩 |
| `uretprobe` | `ur` | 用户态动态函数返回插桩 |
| `software` | `s` | 内核软件事件 |
| `hardware` | `h` | 硬件计数器事件 |
| `watchpoint` | `w` | 内存 watchpoint |
| `profile` | `p` | 跨所有 CPU 的定时采样 |
| `interval` | `i` | 定时上报（来自单个 CPU） |
| `BEGIN` / `END` | — | bpftrace 开始 / 结束 |

**大部分探针类型都是既有内核技术的接口**（kprobes、uprobes、tracepoints、USDT、PMC——第 4 章讲这些怎么工作）；
**`kfunc`/`kretfunc` 是新的低开销接口**，基于 **eBPF trampoline 与 BTF**。

**探针频率是开销的主要来源**：

> As a rough guide for the computers of today, I would consider less than 100k kprobe or tracepoint events per second to be low frequency.

原文的完整意思：调度事件、内存分配、网络包这些探针**触发极其频繁**；
**降低开销的第一手段是换用触发频率更低的事件**；不确定频率时可以**用 bpftrace 自己测**
（数一秒的 `vfs_read` kprobe 次数）。而「多少算高」取决于 CPU 速度、事件数、headroom 与插桩成本——
**100k/秒 是原文给的粗略经验线，不是通用阈值**。

#### Probe Arguments

不同探针类型提供不同类型的参数。tracepoint 把 format 文件里的字段按**字段名**放进 `args` 结构：

```bash
bpftrace -e 'tracepoint:syscalls:sys_enter_read { @req_bytes = hist(args->count); }'
```

字段清单可以查 `/sys` 下的 format 文件，或用 `bpftrace -lv` 列出：

```bash
# bpftrace -lv 'tracepoint:syscalls:sys_enter_read'
tracepoint:syscalls:sys_enter_read
    int __syscall_nr;
    unsigned int fd;
    char * buf;
    size_t count;
```

#### 2. Flow Control

**三种测试**：过滤器（filter）、三元运算符（ternary）、if 语句。
布尔运算符表（**表为原文**）：`==`、`!=`、`>`、`<`、`>=`、`<=`、`&&`、`||`；
表达式可用括号分组。

**三元运算符**：`test ? true_statement : false_statement`，例如
`$abs = $x >= 0 ? $x : - $x;`（求绝对值）。

**if 语句**：`if (test) { true_statements }`、`if (test) { ... } else { ... }`；
原文举的例子是**对 IPv4 与 IPv6 分别处理**（用 `$inet_family` 与 `$AF_INET`）。
**`else if` 自 bpftrace v0.10.0 起支持**。

**循环**：bpftrace 支持**展开循环** `unroll()`；**Linux 5.3 及以后的内核还支持 `while()` 循环**
（用的是 5.3 加入的内核 BPF 循环支持）。

**运算符表**（**表为原文**）：`=` 赋值；`+ - * /` 四则（**仅整数**）；`++ --` 自增自减；
`& | ^` 位与/位或/位异或；`!` 逻辑非；`<< >>` 逻辑移位；
`+= -= *= /= %= &= ^= <<= >>=` 复合赋值。这些运算符都是**仿 C** 设计的。

#### 3. Variables

内置变量表（**表为原文**）：

| 变量 | 类型 | 说明 |
|---|---|---|
| `pid` | integer | 进程 ID（内核 tgid） |
| `tid` | integer | 线程 ID（内核 pid） |
| `uid` / `username` | integer / string | 用户 ID / 用户名 |
| `nsecs` / `elapsed` | integer | 纳秒时间戳 / 自 bpftrace 初始化起的纳秒数 |
| `cpu` | integer | 处理器 ID |
| `comm` | string | 进程名 |
| `kstack` / `ustack` | string | 内核栈 / 用户态栈 |
| `arg0..argN` / `args` | integer / struct | 部分探针类型的参数 |
| `sarg0..sargN` | integer | 栈上的参数 |
| `retval` | integer | 部分探针类型的返回值 |
| `func` / `probe` | string | 被追踪函数名 / 当前探针全名 |
| `curtask` | struct/integer | 内核 task_struct（有无类型信息决定是结构体还是 64 位整数） |
| `cgroup` | integer | 当前进程的默认 cgroup v2 ID（用于与 `cgroupid()` 比较） |
| `$1..$N` | int, char * | bpftrace 程序的位置参数 |

**两条硬约束**：**所有整数目前都是 uint64**；**这些变量都指向探针触发那一刻**的线程、探针、函数与 CPU。

#### 4. Functions

Table 15.8 的函数清单（**表为原文**）：`printf()`、`time()`、`join()`、`str()`、`buf()`（指针的十六进制字符串）、
`strncmp()`、`sizeof()`、`kstack()`、`ustack()`、`ksym()`、`usym()`、`kaddr()`、`uaddr()`、
`reg()`（读寄存器）、`ntop()`（IP 地址的字符串形式）、`cgroupid()`、`system()`、`cat()`、
`signal()`（给当前 task 发信号）、`override()`、`exit()`。

**`override()` 带着原文自己的警告**：只在清楚自己在做什么时用——**一个小错误就可能 panic 或者损坏内核**。

**同步 / 异步的区分，是这一节最容易被忽略的一条**：

> The asynchronous functions are printf() , time() , cat() , join() , and system() . The functions kstack() , ustack() , ksym() , and usym() record addresses synchronously, but do symbol translation asynchronously.

**异步函数**（`printf()`、`time()`、`cat()`、`join()`、`system()`）：内核**排队事件**，稍后在用户态处理。
**栈与符号函数**（`kstack()`、`ustack()`、`ksym()`、`usym()`）：**地址是同步记录的，但符号翻译是异步的**。
**实践含义（推演）**：探针里先记地址、事后才出符号——所以在高频探针上「打印栈」比「记栈」贵得多。

#### 5. Map Functions

Table 15.9（**表为原文**）：`count()`、`sum()`、`avg()`、`min()`、`max()`、`stats()`、
`hist()`（2 的幂直方图）、`lhist()`（线性直方图，带 min/max/step）、`delete()`、
`print(@m [, top [, div]])`、`clear()`、`zero()`。

**异步性同样存在**：`print()`、`clear()`、`zero()` 是**异步的**（内核排队，用户态稍后处理），
原文提醒**写程序时要记住这个延迟**。

**线性直方图的例子**（按进程名统计 `read(2)` 的文件描述符分布，步长为 1）：

```bash
# bpftrace -e 'tracepoint:syscalls:sys_enter_read {
    @fd[comm] = lhist(args->fd, 0, 100, 1); }'
Attaching 1 probe...
^C
[...]
@fd[sshd]:
[4, 5)                22 |                                                    |
[5, 6)                 0 |                                                    |
[6, 7)                 0 |                                                    |
[7, 8)                 0 |                                                    |
[8, 9)                 0 |                                                    |
[9, 10)                0 |                                                    |
[10, 11)               0 |                                                    |
[11, 12)               0 |                                                    |
[12, 13)            7760 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
```

**输出读法**：这台机上 **sshd 进程典型地从文件描述符 12 读取**。
原文特别说明了**区间记号**：**"[" 表示 ">="，")" 表示 "<"**，
也就是**左闭右开**的区间。**记错这一条就会读错每个桶的含义**。

### §15.2.6 文档与版本差异

**更多 bpftrace 例子在本书的这些位置**：第 5 章 §5.5.7、第 6 章 §6.6.20、第 7 章 §7.5.13、
第 8 章 §8.6.15、第 9 章 §9.6.11、第 10 章 §10.6.12；第 4 章（可观测性工具）与第 11 章（云计算）也有。
仓库里的文档：`docs/reference_guide.md`、`docs/tutorial_one_liners.md`。
作者另一本书《BPF Performance Tools》第 5 章专门讲 bpftrace 语言。

**版本差异（原文明确列出）**：原文写作时（2020 年前后），作者在《BPF Performance Tools》里标为
"planned" 的能力**已经全部落地**——**`while()` 循环、`else if`、`signal()`、`override()`、
watchpoint 事件**；另外新增的有 **`kfunc` 探针类型、`buf()`、`sizeof()`**。
原文的判断是**不会再有太多新东西**：**bpftrace 现有的能力已经足够支撑那 120 多个已发布的 bpftrace 工具**。

## 章末选型判据

**先问「这个问题有没有现成工具」**

| 场景 | 用什么 | 别用什么 |
|---|---|---|
| 磁盘 I/O 延迟分布（想知道是不是双峰、离群在哪） | `biolatency(8)`，要按 I/O flags 拆再加 `-F` | `iostat` 的平均值（平均会把双峰抹平） |
| 按进程看块 I/O / off-CPU 时间 | `biotop(8)`、`offcputime(8)` | 临时写 bpftrace 重复造轮子 |
| 调度延迟、run queue 长度 | `runqlat(8)`、`runqlen(8)` | 采样式统计（采样看不到尾延迟） |
| TCP 会话、重传、按主机吞吐 | `tcplife(8)`、`tcpretrans(8)`、`tcptop(8)` | 自己解析 `ss` 输出 |
| 问题没头绪 / 要自定义「算什么」 | **bpftrace one-liner**，先写一行看分布 | 一上来写 BCC 工具（迭代成本十倍） |
| 要长期维护、要进看板或 CI | **BCC**（C 的可维护性 + 完全控制力） | bpftrace 脚本（原文：适合一次性） |
| 探针插桩不确定会不会太贵 | 先 `bpftrace -l '<通配符>'` 看匹配数，再看 `BPFTRACE_MAX_PROBES`（默认 512） | 直接 `kprobe:vfs_*` 满地插 |

**bpftrace 侧的三条硬规则**

1. **返回值要先过滤**：延迟类统计一律加 `/retval >= 0/`（或 `> 0/`），否则错误码会污染分布。
2. **成对探针必须配过滤器**：kretprobe 侧要加 `/@start[tid]/` 这类「开始确实记录过」的过滤，
   否则「追踪开始前就在飞」的调用会算出 `now - 0` 的假延迟。
3. **map 按线程/进程分键**：`@start[tid]`、`@us[pid, comm]`——
   不分键就会互相覆盖；分键则直接得到「每个进程一份」的答案。

**两个不该忽略的坑**

- **`@x++` 与 `count()` 不是一回事**：前者是全局 CPU map 的整数，**并发更新下有小的误差**；
  要准确计数用 `count()`（per-CPU map）。
- **打印是异步的**：`printf()`、`print()`、`clear()`、`zero()` 都在内核排队、用户态稍后处理；
  **终止时自动打印所有 map 反而开销更低**（运行中打印会拖慢 map 遍历，因为 map 正在被更新）。

**什么时候干脆别用 BPF**

- 事件频率太高（调度、内存分配、网络包）：**先换低频事件**，而不是硬上；
  粗略经验线是**低于 100k 事件/秒**（原文自己的说法，且取决于 CPU 与 headroom）。
- 需要**生产环境长期常驻**的采集：先算清楚开销——BPF 的优势正是把处理留在内核，
  这也是它比「导出到用户态再后处理」的追踪器更可上生产的原因。
