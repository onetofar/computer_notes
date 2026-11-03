---
title: 第 13 章 perf（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - perf
  - 性能剖析
  - 火焰图
  - PMC
  - Linux
source: "《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg，Addison-Wesley 2020，英文原版）第 13 章"
companion: "[[13-Chapter 13. perf]]"
---

# 第 13 章 讲义：perf 是 Linux 的 X 光

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[13-Chapter 13. perf|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：本仓库 `otf-server`（C++20、零三方依赖、单线程 epoll reactor，约 9700 行）的 IO 热路径 `src/net/tcp_transport.cpp` —— `TcpTransport::Poll` / `OnReadable` / `FlushWrite` / `Ctl`，以及仓库自建的观测口 `TcpTransport::EpollCtlCounters`、`net::Metrics` / `net::MetricsServer`。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原章。实战锚点一节里的 perf 命令是**按本章词汇为本仓库组合**的，不是原书原句。

---

## 0. 开局："卡" 这一个字，底下埋着四种完全不同的病

服务器卡了。监控里 `otf_tick_drift_max_ms` 跳了一个数。第一个问题永远是：**是哪一层卡了？**

答案可能是这四个里的任意一个：

- 主循环卡在 `epoll_wait` 里 —— 那其实**不叫卡**，叫「没事干、在等 I/O」，此时 CPU 是闲的；
- 卡在 `OnReadable` 里那一串 `::read` —— 一次唤醒读了上百轮，**热点在解析**；
- 卡在 `FlushWrite` 的 `::write` —— 发送队列积压，**热点在出网**；
- 卡在 tick 里的世界推进 —— 事件循环一点问题没有，**热点在玩法逻辑**。

这四种病在 `top` 上长得一模一样，在日志里也长得一模一样。只有 perf 能把它们分开，因为它分别回答四个不同的问题：

> Which code paths are consuming CPU resources?
>
> Are the CPUs stalled on memory loads/stores?
>
> For what reasons are threads leaving the CPU?
>
> What is the pattern of disk I/O?

（推演）把 perf 当成 X 光片：平片只告诉你「这里有个东西」，造影才告诉你「这里是个什么形状的病」。本章讲的就是这台机器的按钮面板 —— 事件有哪些类型、每个子命令管什么、参数怎么给。作者把结构摆得很清楚：**前几章讲「用 perf 分析某个具体目标」，这一章讲「perf 自己是什么」。**

---

## 1. §13.1 Subcommands Overview：perf 是一台多面相机

perf(1) 就住在内核源码树里（`tools/perf`），是内核 perf_events 子系统的前端 —— 这个子系统最初只为 PMC（性能监控计数器）而生，后来长出了 tracepoint、kprobe、uprobe、USDT 这些「事件源」。

> perf(1) is the official Linux profiler and is in the Linux kernel source under tools/perf.

**「子命令」是理解 perf 的钥匙。** 最常见的两步走是 `record`（采样落盘）→ `report`（汇总成表）：

```
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

这段里藏着四个要点：**`-F 99`** 是采样频率；**`-a`** 是「所有 CPU」；末尾那三行 `Woken up / Captured and wrote / samples` 是体检报告；报告的表头四列 `Overhead`（占比）/ `Command`（哪个进程）/ `Shared Object`（用户态还是内核态）/ `Symbol`（哪个函数）—— **第三列是灵魂**，一眼区分「热点在内核」还是「热点在你自己的代码里」。

原书表 13.1 标注了各子命令归属的小节：`record`→§13.9、`report`→§13.10、`stat`→§13.8、`script`→§13.11、`trace`→§13.12、`probe`→§13.7、`list`→§13.3。另有 `annotate`（行级反汇编）、`archive`（把符号打包成可移植文件）、`bench`、`c2c`（缓存行假共享）、`diff`（两份 profile 做差）、`evlist`、`kmem`、`kvm`、`lock`、`mem`、`sched`、`timechart`、`top` 等。

> Figure 13.1 Commonly used perf subcommands

**记住五个就够**：`record` 采样落盘，`report` 汇总成表，`stat` 直接数，`script` 逐条倒出样本，`trace` 实时跟着跑。

---

## 2. §13.2 One-Liners：先用一句话问一句

作者给了一份「一句话清单」（完整版在 [Gregg 20h]），用它快速划出 perf 的能力边界。下面按类摘录（命令均为原书原文）：

```
perf list
perf list 'sched:*'
perf list block
perf probe -l
```

```
perf stat command
perf stat -p PID
perf stat -a sleep 5
perf stat -e LLC-loads,LLC-load-misses,LLC-stores,LLC-prefetches command
perf stat -e r003c -a sleep 5
perf stat -e raw_syscalls:sys_enter -I 1000 -a
perf stat -e 'syscalls:sys_enter_*' -p PID
perf stat -e 'block:*' -a sleep 10
```

```
perf record -F 99 command
perf record -F 99 -a -g sleep 10
perf record -F 99 -p PID --call-graph dwarf sleep 10
perf record -F 99 -a --call-graph lbr sleep 10
perf record -e LLC-load-misses -c 100 -ag sleep 5
perf record -e cycles:up -a sleep 5
perf top -F 49 -ns comm,dso
```

```
perf record -e sched:sched_process_exec -a
perf record -e context-switches -a -g sleep 1
perf record -e sched:sched_switch -a -g sleep 1
perf record -e sched:sched_switch/max-stack=5/ -a sleep 1
perf record -e syscalls:sys_enter_connect -a -g
perf record -e 'block:block_rq_issue,block:block_rq_complete' -a
perf record -e block:block_rq_issue --filter 'bytes >= 65536'
perf trace -e block:block_rq_issue
```

```
perf probe --add tcp_sendmsg
perf probe --del tcp_sendmsg
perf probe 'tcp_sendmsg bytes=%cx'
perf probe -x /lib/x86_64-linux-gnu/libc.so.6 --add fopen
```

```
perf report
perf report -n --stdio
perf script --header
perf script --header -F comm,pid,tid,cpu,time,event,ip,sym,dso
perf script report flamegraph
perf annotate --stdio
```

> Note that many of these one-liners use -a to specify all CPUs, but this became the default in Linux 4.11 and can be elided in that and later kernels.

（推演）把这张表当「perf 的普通话词典」用：每记不住一个参数，回来查一行，而不是去读 man page。

---

## 3. §13.3 perf Events：事件的十种类型

`perf list` 列出所有已知事件 —— 原书那份输出在作者的机器上有 **4,402 行**。输出的类型标签共十种：

- **Hardware event** —— 多为处理器事件，用 PMC 实现；
- **Software event** —— 内核计数器事件；
- **Hardware cache event** —— 处理器缓存事件（PMC）；
- **Kernel PMU event** —— PMU 事件（PMC）；
- **cache, floating point …** —— 处理器厂商事件（PMC）及简短说明；
- **Raw hardware event descriptor** —— 用裸编码指定的 PMC；
- **Hardware breakpoint** —— 处理器断点事件；
- **Tracepoint event** —— 内核静态插桩事件；
- **SDT event** —— 用户态静态插桩事件（USDT）；
- **pfm-events** —— libpfm 事件（Linux 5.8 新增）。

`perf list` 支持拿子串去搜（`perf list mem_load_l3`），命中项在输出里高亮。原文的输出样例是 `cache: mem_load_l3_hit_retired.xsnp_hit` 这类条目，描述末尾标着 `(Precise event)` —— 指支持精确事件采样（PEBS）的事件。

（推演）十种事件源像十种不同口径的尺子：选错尺子，量出来的数就没意义。想看「谁在烧 CPU」用硬件事件；想看「进程为什么被踢下 CPU」用软件事件；想看「某次读写什么时候发生」用 tracepoint。

---

## 4. §13.4 Hardware Events：硬件计数器，以及它的映射表会骗人

硬件事件是第 4 章 PMC 的老朋友。它们用**处理器专属的编码**配置，比如 Intel 上的分支指令可以写成裸描述符 `r00c4`（umask `0x0` + event select `0xc4`）。你不需要背这些码 —— perf 提供了人可读的别名（`branch-instructions` 之类）。但作者连着给了两条警告：

> You are not expected to remember these codes, and you will only refer to them in the processor manuals if needed.

> There may also be bugs in the mappings, and if you ever encounter a suspicious PMC result, you may wish to try the raw event descriptor to double-check.

（推演）人可读的别名是一张**会过期的便利贴**：处理器换了、perf 版本变了，映射就可能悄悄错位。所以「数字看着不对」时，第一反应应该是拿裸编码复核一遍。

### §13.4.1 Frequency Sampling：那个默认不告诉你的坑

用 `perf record` 采 PMC 时默认走**频率采样** —— 不是每次事件都记，而是让内核调节速率、每秒约采 N 次。原因是有些 PMC 一秒能发生几十亿次（全记下来的开销不可接受）。

`perf record -vve cycles -a sleep 1` 的输出里有两行关键：`{ sample_period, sample_freq }   4000` 与 `freq                             1` —— **默认采样频率 4000 Hz**（每秒每 CPU 约 4000 个样本）。

> But this is also a gotcha: the default output of perf(1) (without the very verbose option: -vv ) does not say that frequency sampling is in use, and you may be expecting to record all events.

**注意这个坑的形状**：它不报错、不警告，只在你以为「每个周期都记下来了」时安静地少记。必须加 `-v`（甚至 `-vv`）才看得见。

`stat` 不受这个影响 —— 它永远全量计数：

> This event frequency only affects the record subcommand; stat counts all events.

还有两件事要查：**丢事件**和**采样率上限**。

> Always check for lost events to see if this occurred (e.g., check the summary counters from: perf report -D | tail -20 ).

```
# sysctl kernel.perf_event_max_sample_rate
kernel.perf_event_max_sample_rate = 15500
# sysctl kernel.perf_cpu_time_max_percent
kernel.perf_cpu_time_max_percent = 25
```

即：采样率天花板 15,500 Hz，且 perf 本身最多允许占 25% CPU 时间（靠 PMU 中断实现）。`-F` 改频率、`-c` 改成「每 N 个事件采一次」（overflow sampling）。

---

## 5. §13.5 Software Events：内核计数器，以及它和 tracepoint 的差别

软件事件「通常对应硬件事件，但用软件实现」，同样有默认采样频率（通常 4000）。作者在这里做了一组非常值得学的对照 —— **同一个现象（上下文切换），软件事件和 tracepoint 的采样语义完全不同**：

```
# perf record -vve context-switches -a -- sleep 1
[...]
------------------------------------------------------------
perf_event_attr:
  type                             1
  size                             112
  config                           0x3
  { sample_period, sample_freq }   4000
  sample_type                      IP|TID|TIME|CPU|PERIOD
[...]
  freq                             1
[...]
[ perf record: Captured and wrote 3.227 MB perf.data (660 samples) ]
```

而对应的 tracepoint：

```
# perf record -vve sched:sched_switch -a sleep 1
[...]
------------------------------------------------------------
perf_event_attr:
  type                             2
  size                             112
  config                           0x131
  { sample_period, sample_freq }   1
  sample_type                      IP|TID|TIME|CPU|PERIOD|RAW
[...]
[ perf record: Captured and wrote 3.360 MB perf.data (3538 samples) ]
```

**软件事件是频率采样（4000 Hz，660 个样本），tracepoint 是周期采样（period=1，等价 `-c 1`，一个不漏，3538 个样本）。** 想让软件事件也全量捕获，就得显式写 `-c 1`：

```
perf record -vve context-switches -a -c 1 -- sleep 1
```

> Be careful with the volume of recording every event and the overheads involved, especially for context switches, which can be frequent.

（推演）这条对「查上下文切换风暴」极其实用：默认采样下你只看到 660 个样本，它说明切换**很频繁**，不说明**每一次**都什么样；`-c 1` 才让你看清每一次切换的完整时间线（谁被踢下去了、被谁顶上来）。

---

## 6. §13.6 Tracepoint Events：内核的静态插桩点

tracepoint 是内核作者**预先埋好**的观测点，perf 直接订阅：

```
perf record -e block:block_rq_issue -a sleep 10; perf script
```

想知道某个 tracepoint 的参数和格式串，读它的 format 文件：

```
cat /sys/kernel/debug/tracing/events/block/block_rq_issue/format
```

还能带过滤：

```
perf record -e block:block_rq_issue --filter 'bytes > 65536' -a sleep 10
```

（推演）tracepoint 之于 perf，就像「系统预留的取餐窗口」：位置由内核作者决定，稳定、免费、可移植。代价是**你只能看内核作者认为值得看的东西** —— 想看自己代码的某个瞬间，就得自己开窗（下一节的 probe）。

---

## 7. §13.7 Probe Events：自己开窗

perf 里的 "probe events" 指三类：**kprobe**（内核函数）、**uprobe**（用户态函数）、**USDT**（用户态静态插桩）。它们是**动态**的，必须先初始化，默认不出现在 `perf list` 里。

### §13.7.1 kprobes

标准四步：

```
perf probe --add do_nanosleep
perf record -e probe:do_nanosleep -a sleep 5
perf script
perf probe --del do_nanosleep
```

参数有四种拿法：**有内核 debuginfo** → 直接用变量名（`perf probe --vars do_nanosleep` 能列出 `mode`、`t`）；**没有 debuginfo**（"as I often find in production"）→ 用寄存器位置，办法是找一台**同硬件同内核**的机器装上 debuginfo 当参考机，用空跑抄寄存器位置：

```
# perf probe -nv 'do_nanosleep mode'
[...]
Writing event: p:probe/do_nanosleep _text+10806336 mode=%si:x32
[...]
```

**知道处理器 ABI** → 自己算；**新的路子** → BPF type format（BTF）。

> This only works if the systems have the same processor ABI and kernel versions, otherwise the wrong register locations may be instrumented.

返回函数用 `%return`（这是 kretprobe），返回值用特殊变量 `$retval`：

```
perf probe 'do_nanosleep%return $retval'
```

### §13.7.2 uprobes

做法和 kprobe 平行，只是要指明二进制：

```
# perf probe -x /lib/x86_64-linux-gnu/libc.so.6 --add fopen
Added new event:
  probe_libc:fopen     (on fopen in /lib/x86_64-linux-gnu/libc-2.27.so)
You can now use it in all perf tools, such as:

        perf record -e probe_libc:fopen -aR sleep 1
```

删掉用 `--del`，返回值用 `%return` + `$retval`。

> While uprobes can provide visibility into application internals, they are an unstable interface as they are instrumenting the binary directly, which can change between software versions. USDT probes are preferred whenever available.

**这句是本节的题眼**：uprobe 是直接往二进制身上插针，版本一换位置就变。USDT 才是那个「稳定接口」。

### §13.7.3 USDT

USDT 是**静态定义**的插桩点，位置写在 ELF notes 里（`readelf -n` 能看到）。先让 perf 认识它：

```
# perf buildid-cache --add $(which node)
```

之后 `perf list` 里就能看到 `sdt_node:http__server__request  [SDT event]` 这一类条目。

**但「看得见」不等于「能采」**：还得像 uprobe 那样创建事件，才能真的被插桩：

```
# perf probe sdt_node:http__server__request
Added new event:
  sdt_node:http__server__request (on %http__server__request in
home/bgregg/Build/node-v12.4.0/out/Release/node)

You can now use it in all perf tools, such as:

        perf record -e sdt_node:http__server__request -aR sleep 1
```

采出来是这样（参数里有些是结构体和字符串，perf 只能打成指针）：

```
# perf record -e sdt_node:http__server__request -a
^C[ perf record: Woken up 1 times to write data ]
[ perf record: Captured and wrote 3.924 MB perf.data (2 samples) ]
# perf script
            node 16282 [006] 510375.595203: sdt_node:http__server__request:
(55c3d8b03530) arg1=140725176825920 arg2=140725176825888 arg3=140725176829208
arg4=39090 arg5=140725176827096 arg6=140725176826040 arg7=20
```

还有一个坑：某些 USDT 探针需要进程地址空间里的一个**信号量**被递增才会激活（Linux 4.20 之前必须手动加），否则**一个事件都采不到**。

（推演）USDT 的代价是「要在代码里埋点」；收益是「埋点的位置在版本之间不变」。对一个要长期演进的服务，这笔买卖通常划算 —— 埋点处就是你要的语义边界，而不是「某个函数第 N 行」。

---

## 8. §13.8 perf stat：数事件，不落盘

`perf stat` 干的事就是计数 —— 量事件速率，或者确认某事件到底有没有发生。

> perf stat is efficient: it counts software events in kernel context and hardware events using PMC registers.

> This makes it suited to gauge the overhead of a more expensive perf record subcommand by first checking the event rate using perf stat .

**这就是 perf 的正确使用顺序**：先用便宜的 `stat` 摸清事件速率，确认采样开销可接受，再用贵的 `record`。

```
# perf stat -e sched:sched_switch -a -- sleep 1
 Performance counter stats for 'system wide':

             5,705      sched:sched_switch

       1.001892925 seconds time elapsed
```

### §13.8.1 Options

`-a`（所有 CPU，Linux 4.11 起是默认）、`-e event`、`--filter filter`、`-p PID`、`-t TID`、`-G cgroup`（容器）、`-A`（按 CPU 分列）、`-I interval_ms`（按间隔打印）、`-v` / `-vv`。

事件可以是 tracepoint、软件事件、硬件事件、kprobe、uprobe、USDT 探针，**支持通配**（`*` 任意、`?` 单字符）：`perf stat -e 'sched:*' -a`。多个 `-e` 或一个逗号分隔的列表都行：`perf stat -e 'sched:*' -e 'block:*' -a` 与 `perf stat -e 'sched:*,block:*' -a`。**不指定事件时，默认统计体系结构 PMC。**

### §13.8.2 Interval Statistics

`-I` 让你按间隔看趋势 —— 排查「偶发卡顿」时这是最有用的一个选项：

```
# perf stat -e sched:sched_switch -a -I 1000
#           time             counts unit events
     1.000791768              5,308      sched:sched_switch
     2.001650037              4,879      sched:sched_switch
     3.002348559              5,112      sched:sched_switch
     4.003017555              5,335      sched:sched_switch
     5.003760359              5,300      sched:sched_switch
^C     5.217339333              1,256      sched:sched_switch
```

`counts` 列是**距上一间隔的增量**。最后一行是你按 Ctrl-C 那一刻的部分区间（0.214 秒）。

### §13.8.3 Per-CPU Balance

`-A` 按逻辑 CPU 分列 —— 用来查「负载是不是全压在一个核上」；另有 `--per-socket` 与 `--per-core`：

```
# perf stat -e sched:sched_switch -a -A -I 1000
#           time CPU                counts unit events
     1.000351429 CPU0                 1,154      sched:sched_switch
     1.000351429 CPU1                   555      sched:sched_switch
     1.000351429 CPU2                   492      sched:sched_switch
     1.000351429 CPU3                   925      sched:sched_switch
[...]
```

### §13.8.4 Event Filters

有些事件类型（tracepoint）支持用布尔表达式过滤参数：

```
# perf stat -e sched:sched_switch --filter 'prev_pid == 25467' -a -I 1000
#           time             counts unit events
     1.000346518                131      sched:sched_switch
     2.000937838                145      sched:sched_switch
     3.001370500                 11      sched:sched_switch
     4.001905444                217      sched:sched_switch
[...]
```

这些参数是**每个事件自己定制的**，可以从 `/sys/kernel/debug/tracing/events` 下各事件的 format 文件里查到。

### §13.8.5 Shadow Statistics

某些事件组合会附带打印「影子统计」—— 由别的计数器**顺带算出来**的派生量。经典例子是 IPC：

```
# perf stat -e cycles,instructions -a
^C
 Performance counter stats for 'system wide':

     2,895,806,892      cycles
     6,452,798,206      instructions              #    2.23  insn per cycle

       1.040093176 seconds time elapsed
```

> perf(1) has a variety of shadow statistics that will be printed when certain combinations of events are instrumented.

影子统计打在一侧的 `#` 之后。`perf stat` 不带事件时会有好几条。

---

## 9. §13.9 perf record：采样、落盘

`perf record` 把事件记进文件（默认 `perf.data`）供事后分析。事件写在 `-e` 后面，多个事件可以同时记：

```
# perf record -e sched:sched_switch -a
^C[ perf record: Woken up 9 times to write data ]
[ perf record: Captured and wrote 6.060 MB perf.data (23526 samples) ]
```

> The data is passed from the kernel to user-space via per-CPU ring buffers, and to keep context-switch overheads to a minimum perf(1) is woken up an infrequent and dynamic number of times to read them.

（推演）这个「内核 → 用户态」的搬运机制决定了两件事：**perf 自己会占 CPU**（所以有 `perf_cpu_time_max_percent` 上限），以及**采样是分批的**（所以 "Woken up 9 times" 有意义 —— 它是 perf 惊醒了多少次去捞环形缓冲区）。

### §13.9.1 Options

`-a`、`-e event`、`--filter filter`、`-p PID`、`-t TID`、`-G cgroup`、`-g`（记栈）、`--call-graph mode`、`-o file`、`-v` / `-vv`。

### §13.9.2 CPU Profiling

最常用的用法：全 CPU、99 Hz、30 秒、带栈。

```
perf record -F 99 -a -g -- sleep 30
```

**不指定 `-e` 时，perf 按这个顺序挑第一个可用的**：`cycles:ppp`（precise，零 skid）→ `cycles:pp`（请求零 skid，实际未必）→ `cycles:p`（请求恒定 skid）→ `cycles`（无 precise）→ `cpu-clock`（纯软件采样）。**这个顺序就是「精确度优先」的排序**：`:ppp` / `:pp` / `:p` 是 precise event sampling 模式，Intel 上用 PEBS、AMD 上用 IBS。

### §13.9.3 Stack Walking：栈断了怎么修

栈断了，通常是**软件没遵守帧指针寄存器约定**：

> Note that if stack traces appear broken, it may be due to the software not honoring the frame pointer register.

> Apart from recompiling the software with frame pointers (e.g., gcc(1) -fno-omit-frame-pointer ), a different stack walking method may also work, selected using --call-graph .

三条路：**`--call-graph dwarf`**（用 debuginfo 回溯，需要可执行文件带 debuginfo，某些发行版用 `-dbgsym` / `-dbg` 包提供）、**`--call-graph lbr`**（Intel LBR，硬件提供，但栈深通常只有 16 帧，Haswell 之后 32）、**`--call-graph fp`**（帧指针回溯，**默认值**）。

（推演）这条对「Release 构建 + 默认栈回溯」是判死刑的一节：`-O2` 会把帧指针当可丢弃寄存器，于是默认的 `fp` 回溯出来的栈是断的。

---

## 10. §13.10 perf report：把 perf.data 汇总成表

选项：`--tui`（默认）、`--stdio`、`-i file`、`-n`（加一列样本数）、`-g options`（调栈的显示方式）。

### §13.10.1 TUI

TUI 是默认界面，可交互导航、选函数和线程看细节。它的 STDIO 版本（见 §13.1）长这样：

```
# perf record -F 99 -a -- sleep 30
[ perf record: Woken up 193 times to write data ]
[ perf record: Captured and wrote 48.916 MB perf.data (11880 samples) ]
# perf report
Samples: 11K of event 'cpu-clock:pppH', Event count (approx.): 119999998800
Overhead  Command          Shared Object              Symbol
  21.10%  swapper          [kernel.vmlinux]           [k] native_safe_halt
   6.39%  mysqld           [kernel.vmlinux]           [k] _raw_spin_unlock_irqrestor
   4.66%  mysqld           mysqld                     [.] _Z8ut_delaym
   2.64%  mysqld           [kernel.vmlinux]           [k] finish_task_switch
   2.59%  oltp_read_write  [kernel.vmlinux]           [k] finish_task_switch
   2.03%  mysqld           [kernel.vmlinux]           [k] exit_to_usermode_loop
   1.68%  mysqld           mysqld                     [.] _Z15row_search_mvccPh15pag
   1.40%  oltp_read_write  [kernel.vmlinux]           [k] _raw_spin_unlock_irqrestor
[...]
```

### §13.10.2 STDIO

`--stdio` 不交互，但**适合重定向到文件**再分享（贴进聊天、邮件、工单系统）。作者习惯加 `-n` 带上样本数列。带栈的报告长这样：

```
# perf record -F 99 -a -g -- sleep 30
[ perf record: Woken up 8 times to write data ]
[ perf record: Captured and wrote 2.282 MB perf.data (11880 samples) ]
# perf report --stdio
[...]
# Children      Self  Command          Shared Object               Symbol
# ........  ........  ...............  ..........................  .................
#
    50.45%     0.00%  mysqld           libpthread-2.27.so          [.] start_thread
            |
            ---start_thread
               |
               |--44.75%--pfs_spawn_thread
               |          |
               |           --44.70%--handle_connection
               |                     |
               |                      --44.55%--_Z10do_commandP3THD
               |                                |
               |                                |--42.93%--_Z16dispatch_commandP3THD
               |                                |          |
               |                                |           --40.92%--_Z19mysqld_stm
               |                                |                     |
[...]
```

**读法**：栈样本被合并成层级，**最左边是根函数，往右下是子函数，最右端是事件发生时真正在 CPU 上的那个函数**。

> This left-to-right ordering is called caller by perf(1). You can flip this to callee ordering, where the event function is on the left and its ancestry is down and to the right using: -g callee (it was the default; perf(1) switched to the caller ordering in Linux 4.4).

（推演）`Children` 与 `Self` 两列要一起看：`Self` 是这个函数**自己**烧的 CPU，`Children` 是它**带着子孙**烧的。找真热点看 `Self`，找「谁把 CPU 吃掉了」看 `Children`。

---

## 11. §13.11 perf script：把样本逐条倒出来

```
# perf script
          mysqld  8631 [000] 4142044.582702:   10101010 cpu-clock:pppH:
c08fd9 _Z19close_thread_tablesP3THD+0x49 (/usr/sbin/mysqld)
          mysqld  8619 [001] 4142044.582711:   10101010 cpu-clock:pppH:
79f81d _ZN5Field10make_fieldEP10Send_field+0x1d (/usr/sbin/mysqld)
          mysqld 22432 [002] 4142044.582713:   10101010 cpu-clock:pppH:
ffffffff95530302 get_futex_key_refs.isra.12+0x32 (/lib/modules/5.4.0-rc8-virtua...
[...]
```

字段依次是：进程名、线程 ID、CPU ID、时间戳、Period（由 `-F 99` 推出来的）、事件名、事件参数（对 `cpu-clock` 来说是指针、函数名+偏移、段名）。`report` 是汇总表，`script` 是**逐条明细** —— 找「随时间变化的模式」时用它，因为汇总表会把那种模式抹平。

输出格式可以固定下来（作者强烈建议加 `--header` 带上 perf.data 元数据 —— 里面有采集时间、主机名、内核版本、perf 版本、CPU 型号、`cmdline` 原始命令行等）：

```
# perf script --header -F comm,pid,tid,cpu,time,event,ip,sym,dso
```

### §13.11.1 Flame Graphs

> Flame graphs visualize stack traces. While commonly used for CPU profiles, they can visualize any collection of stack traces collected by perf(1), including on context switch events to see why threads are leaving CPU, and on block I/O creation to see which code paths are creating disk I/O.

perf 内建的火焰图支持是 Linux 5.8 加的：

```
perf script report flamegraph
```

作者自己的实现是 [FlameGraph](https://github.com/brendangregg/FlameGraph)，另有一个 d3 版。完整的生成步骤在第 6 章 §6.6.13（CPU Flame Graphs），可视化原理在 §6.7.3。

### §13.11.2 Trace Scripts

`perf script -l` 能列出内置的 trace script：

```
# perf script -l
List of available trace scripts:
[...]
  event_analyzing_sample               analyze all perf samples
  mem-phys-addr                        resolve physical address samples
  intel-pt-events                      print Intel PT Power Events and PTWRITE
  sched-migration                      sched migration overview
  net_dropmonitor                      display a table of dropped frames
  syscall-counts-by-pid [comm]         system-wide syscall counts, by pid
  failed-syscalls-by-pid [comm]        system-wide failed syscalls by pid
  export-to-sqlite [database name] [columns] [calls] export perf data to a sqlite3
database
  stackcollapse                        produce callgraphs in short form for scripting
use
```

（推演）`syscall-counts-by-pid` 这类脚本是「一句话拿到 syscall 画像」的捷径，省得自己写 `perf script` 后处理。

---

## 12. §13.12 perf trace：实时跟着跑

> The perf trace subcommand will trace system calls by default and print output live (no perf.data file).

> It was introduced in Chapter 5 , Applications , Section 5.5.1 , perf , as a lower-overhead version of strace(1) that can trace system-wide.

**它是 strace 的低开销、系统级版本，而且不产生 perf.data**（输出直接打屏）。

```
# perf trace -e block:block_rq_issue,block:block_rq_complete
     0.000 auditd/391 block:block_rq_issue:259,0 WS 8192 () 16046032 + 16 [auditd]
     0.566 systemd-journa/28651 block:block_rq_complete:259,0 WS () 16046032 + 16 [0]
     0.748 jbd2/nvme0n1p1/174 block:block_rq_issue:259,0 WS 61440 () 2100744 + 120
[jbd2/nvme0n1p1-]
     1.436 systemd-journa/28651 block:block_rq_complete:259,0 WS () 2100744 + 120 [0]
     1.515 kworker/0:1H-k/365 block:block_rq_issue:259,0 FF 0 () 0 + 0 [kworker/0:1H]
     1.543 kworker/0:1H-k/365 block:block_rq_issue:259,0 WFS 4096 () 2100864 + 8
[kworker/0:1H]
     2.074 sshd/6463 block:block_rq_complete:259,0 WFS () 2100864 + 8 [0]
     2.077 sshd/6463 block:block_rq_complete:259,0 WFS () 2100864 + 0 [0]
  1087.562 kworker/0:1H-k/365 block:block_rq_issue:259,0 W 4096 () 16046040 + 8
[kworker/0:1H]
[...]
```

过滤器和 `perf record` 一样可用，还能用内核头文件里的字符串常量：

```
# perf trace -e syscalls:*enter_mmap --filter='flags==SHARED'
     0.000 env/14780 syscalls:sys_enter_mmap(len: 27002, prot: READ, flags: SHARED,
fd: 3)
    16.145 grep/14787 syscalls:sys_enter_mmap(len: 27002, prot: READ, flags: SHARED,
fd: 3)
    18.704 cut/14791 syscalls:sys_enter_mmap(len: 27002, prot: READ, flags: SHARED,
fd: 3)
[...]
```

注意 `prot: READ` 而不是 `prot: 1` —— 这就是 perf 说的「beautification」。

### §13.12.1 Kernel Versions

- Linux 4.19 之前，`perf trace` 默认**还会**插桩所有 syscall（`--syscalls`），要关掉得写 `--no-syscalls`（现在它是默认）；
- 跨所有 CPU 跟踪（`-a`）自 Linux 3.8 起是默认；
- 过滤器（`--filter`）是 Linux 5.5 加的。

---

## 13. §13.13–13.15 Other Commands / Documentation / References

**其他子命令**：`perf c2c`（Linux 4.10+）缓存到缓存、缓存行**假共享**分析；`perf kmem` 内核内存分配分析；`perf kvm` KVM guest 分析；`perf lock` 锁分析；`perf mem` 内存访问分析；`perf sched` 内核调度器统计。

更进阶的能力：在事件上启动 BPF 程序、用 Intel PT / ARM CoreSight 做**逐指令**分析。例如 `perf record -e intel_pt/cyc/u date` 记录 `date(1)` 的用户态周期，再用 `perf script --insn-trace` 打出机器码、用 `perf script --insn-trace --xed` 翻成汇编。

**文档**：每个子命令都有 man page，名字是 `perf-<子命令>`（如 `perf-record(1)`），在源码树 `tools/perf/Documentation` 下。另有 wiki.kernel.org 上的教程、Vince Weaver 的非官方事件页、以及作者自己的 [Gregg 20f]（含完整 one-liner 清单）。

---

## 14. 实战锚点：otf-server 怎么面对这件事

> 本节里的 perf 命令**是按本章词汇为本仓库组合的**，不是原书原句；原书原句一律带引号并保持原样。

### 14.1 我们在 perf 之前已经做了什么

仓库**自建了一套观测口**，而且建得相当认真 —— 这本身就是本章最好的反衬：**在没有 kernel 级观测的年代，人是怎么先把自己的盲区写清楚的。**

| 仓库里的东西 | 位置 | 它能回答什么 |
|---|---|---|
| `TcpTransport::EpollCtlCounters` | `src/net/tcp_transport.h:45` | add / del / mod / add_with_out / mod_with_out / mod_without_out 六个计数 |
| `net::Metrics` / `MetricsSnapshot` | `src/net/metrics.h` | `connections` / `rooms` / `frames_sent` / `frames_recv` / `heartbeat_timeouts` / `oversize_rejects` / `tick_drift_max_ms`（`std::optional`，nullopt = 一次都没记过）/ `snapshot_send_ms_max` / `epoll_ctl_*` |
| `Metrics::render()` | `src/net/metrics.cpp:128` | Prometheus text exposition 格式，每行 `otf_x <value>` |
| `net::MetricsServer` | `src/net/metrics_server.h` / `.cpp` | **独立 socket，不注册进游戏的 epoll**；`Poll()` 每 tick 调一次（`src/main.cpp:365`），一轮最多服务 `kMaxServePerPoll = 8` 个连接，`kListenBacklog = 64` |

而 `EpollCtlCounters` 的注释里，作者已经把**这个计数器的边界**写得一清二楚：

> `epoll_ctl` **没有**「查询已注册事件」的 API，所以「EPOLLOUT 是不是常开」这件事，从 Conn 里的 flag 是**看不出来**的 —— flag 说 false 而内核真常开的情形，flag 层面完全测不出来。

> ① **syscall 失败也计入**。`Ctl` 先记账再调内核，所以计数器全绿 **不代表**内核真的收到了那个注册值（变异 M16 验证过这一点：计数器可以全绿而内核压根没收到 MOD）。它证明的是「本实现打算注册成什么样」，仅此而已。

**这两条注释，恰好定义了 perf 的位置**：仓库的计数器只能回答「**我们打算**注册成什么样」，回答不了「**内核实际上**做了什么」。后者只有内核自己知道 —— 而 perf 就在内核里。

（推演）这就像账本 vs 银行流水：你自己的账本可以记得一丝不苟，但它证明不了 ATM 到底吐了多少钱。

### 14.2 逐个对应：本仓库的指标 ↔ perf 的事件

| 仓库现在能看到 | perf 能补上的（内核视角） | 原书小节 |
|---|---|---|
| `otf_epoll_ctl_add/del/mod`（我们**打算**调几次） | `syscalls:sys_enter_epoll_ctl` 实际调了几次 | §13.8 / §13.12 |
| `otf_tick_drift_max_ms`（主循环落后多少 ms） | `context-switches`（被踢下 CPU 几次）、`cpu-migrations`（被迁核几次） | §13.5 |
| `otf_frames_recv` / `otf_frames_sent`（业务帧数） | `syscalls:sys_enter_read` / `sys_enter_write` 实际系统调用数 | §13.2 |
| `otf_snapshot_send_ms_max`（快照发送峰值） | `sched:sched_switch` 时间线：谁占着 CPU、谁在等 | §13.6 |
| `otf_connections`（连接数） | `perf c2c` 查缓存行假共享（多连接时的隐藏开销） | §13.13 |

### 14.3 针对本仓库的 perf 命令

**① 先摸基线（§13.8）** —— 别一上来就 `record`，先用便宜的 `stat`：

```
perf stat -p <PID>
perf stat -e context-switches,cpu-clock -p <PID> -I 1000
```

系统级对照（这是**第 16 章**的锚点：原书案例研究用 `perf stat -e cs -a -I 1000` 数系统级上下文切换速率，再 `-p <PID>` 缩小到容器进程）：

```
perf stat -e cs -a -I 1000
perf stat -e cs -p <PID> -I 1000
```

**② CPU 剖析（§13.9.2）** —— 定位 `Poll` / `OnReadable` / `FlushWrite` 谁在烧 CPU：

```
perf record -F 99 -g --call-graph dwarf -- ./build/dev/otf_server
```

（`-F 99` 与 `--call-graph dwarf` 的写法来自 §13.2 / §13.9.3；把被测目标从 `sleep 10` 换成仓库二进制是本章之外的操作。）

> ⚠️ **为什么必须写 `--call-graph dwarf`**：仓库的 CMake 默认构建类型是 `RelWithDebInfo`（`CMakeLists.txt:16-18`），编译选项里**没有** `-fno-omit-frame-pointer`（我 grep 过 `CMakeLists.txt`：只有 `CMAKE_CXX_STANDARD 20`、`CMAKE_EXPORT_COMPILE_COMMANDS ON` 和一串 `-Werror` 类告警开关）。而 `fp`（帧指针回溯）是 `--call-graph` 的默认值 —— §13.9.3 明说栈断了就换回溯方式。`dwarf` 需要 debuginfo，`RelWithDebInfo` 自带 `-g`，正好够用。

出报告（§13.10）与行级（§13.2 Reporting）：

```
perf report --stdio -n
perf annotate --stdio
```

留档用（§13.11）：

```
perf script --header -F comm,pid,tid,cpu,time,event,ip,sym,dso
```

**③ 软件事件（§13.5）** —— 上下文切换风暴的现场取证：

```
perf record -vve context-switches -a -c 1 -- sleep 1
```

对照组是同名的 tracepoint：

```
perf record -vve sched:sched_switch -a sleep 1
```

**两者的差别正是本节要看的**：仓库是单线程 reactor，理论上上下文切换应该极少。如果 `context-switches` 的数字远超预期，那问题不在别的地方（信号处理、页错误、别的进程抢核）。

**④ syscall 视角（§13.12）** —— 这个 reactor 的全部系统调用就那么几个，直接点名：

```
perf trace -e epoll_wait,read,write,accept
```

本仓库的热路径正好全在这四个里（`src/net/tcp_transport.cpp`）：`::epoll_wait`（:247）、`::read`（:343）、`::write`（:364）、`::accept`（:278 与 :401）。

（推演）这条命令在本仓库上有特殊价值：`OnReadable` 的 `::read` 是**循环到 EAGAIN** 的（`src/net/tcp_transport.cpp:333-358`），一次唤醒可能读几十轮。如果 trace 显示 `read` 调用次数远超「连接数 × 消息数」，那要么是消息拆得太碎（`kReadChunk = 4096` 太小），要么是有人在灌包。

**⑤ 火焰图（§13.11.1）** —— perf 内建（Linux 5.8+）：

```
perf script report flamegraph
```

用作者自己的实现（**这一步的具体脚本名属第 6 章 §6.6.13 的内容，不在本章原文范围内**）：

```
perf script | stackcollapse-perf.pl > perf.folded
perf.folded | flamegraph.pl > perf.svg
```

**⑥ PMC（§13.4）** —— 查缓存未命中：

```
perf stat -e cache-misses,LLC-load-misses -p <PID>
```

> ⚠️ **云上 PMC 可能根本不可用。** 第 16 章原文（§16.1.5）的原话是：**Performance monitoring counters (PMCs) can explain CPU cycle performance, and are available on certain instances in AWS EC2.** —— 「certain instances」，不是所有机型。所以 `perf stat -e cache-misses` 在某些实例上会直接报「事件不支持」，**那不是你的代码有问题，是这台机器没有那个计数器**。

### 14.4 诚实的缺口

这一节比上面三节都值钱 —— **缺口才是「perf 之前我们在干什么」的答案**：

1. **没有 perf 友好的编译选项。** `CMakeLists.txt` 里没有 `-fno-omit-frame-pointer`（§13.9.3 明确点名这个选项）。后果：默认的 `fp` 栈回溯在 `-O2` 下大概率断链，只能靠 `--call-graph dwarf` 兜底。
2. **没有符号导出流程。** 原书有 `perf archive` 子命令专门做「把 debug 和符号信息打包成可移植的 perf.data」；仓库里没有任何 build-id 缓存或符号归档流程，`perf.data` 换个机器就解不开符号。
3. **没有火焰图 CI。** 仓库的回归里没有任何性能剖析环节，性能回归只能靠 `otf_tick_drift_max_ms` 这类**间接指标**发现。
4. **仓库里完全没有 perf 相关的脚本或文档。** 我 grep 过整个仓库（`*.md` / `*.sh` / `*.ps1` / `*.yaml` / `CMakeLists.txt`），`perf` 零命中（只有一个 CMake 配置日志里 `performance` 的误配）。
5. **`EpollCtlCounters` 与 perf 之间没有桥。** 仓库的计数器说「我们打算注册成什么样」，perf 能说「内核实际上收到了什么」，但**没有任何机制把这两者对账**。

（推演）一个成本很低的补法：在 `dev.sh` 里加一个 `perf profile` preset，专门跑 `perf record -F 99 -g --call-graph dwarf -- ./build/dev/otf_server` 并把 `perf report --stdio -n` 存进 `build/`；再在 `CMakeLists.txt` 里加一个 `PerfRelWithDebInfo` 预设（`-O2 -g -fno-omit-frame-pointer`）。两个改动加起来不到二十行，能把 §13.9.3 那条「栈断了」的坑一次堵死。

### 14.5 一句话总结这一节

**仓库已经把自己能观测的部分测得很细（甚至细到在注释里承认「我的计数器证明不了内核真的收到了」）—— 而 perf 正是来补那半块的。** 原书一句话把这事说尽了：

> For what reasons are threads leaving the CPU?

---

## 15. 收尾：本章金句清单

1. **perf(1) is the official Linux profiler and is in the Linux kernel source under tools/perf.** —— Linux 的官方 profiler 就住在内核源码树里；它不是普通用户程序，而是内核观测子系统的门面。
2. **Which code paths are consuming CPU resources?** —— 所有性能问题的第一问，也是这一章存在的理由。
3. **For what reasons are threads leaving the CPU?** —— 上下文切换的**原因**，比切换的次数更值得查。
4. **Note that many of these one-liners use -a to specify all CPUs, but this became the default in Linux 4.11 and can be elided in that and later kernels.** —— 版本差异会咬人：`-a` 现在可以省，但老脚本里的 `-a` 也不能删（它可能是给别的内核用的）。
5. **There may also be bugs in the mappings, and if you ever encounter a suspicious PMC result, you may wish to try the raw event descriptor to double-check.** —— 人可读的别名会错位；数字可疑时，用裸编码复核。
6. **But this is also a gotcha: the default output of perf(1) (without the very verbose option: -vv ) does not say that frequency sampling is in use, and you may be expecting to record all events.** —— 全章最狠的一个坑：它不报错，只是**安静地少记数据**。
7. **This event frequency only affects the record subcommand; stat counts all events.** —— 想全量计数就用 `stat`，别指望 `record`。
8. **Always check for lost events to see if this occurred (e.g., check the summary counters from: perf report -D | tail -20 ).** —— 采了多少和丢了多少，是两件事，要分别看。
9. **Be careful with the volume of recording every event and the overheads involved, especially for context switches, which can be frequent.** —— 全量捕获上下文切换，代价可能比事件本身还贵。
10. **perf stat is efficient: it counts software events in kernel context and hardware events using PMC registers.** —— `stat` 便宜，正因如此它是 `record` 的前置步骤。
11. **Note that if stack traces appear broken, it may be due to the software not honoring the frame pointer register.** —— 栈断了不一定是 perf 的锅，先怀疑编译器把帧指针优化掉了。
12. **Apart from recompiling the software with frame pointers (e.g., gcc(1) -fno-omit-frame-pointer ), a different stack walking method may also work, selected using --call-graph .** —— 断栈有两条修法：改编译，或换回溯方式（dwarf / lbr / fp）。
13. **While uprobes can provide visibility into application internals, they are an unstable interface as they are instrumenting the binary directly, which can change between software versions. USDT probes are preferred whenever available.** —— 能看进程序内部的工具，往往也最不稳定；要稳定，就得埋点。
14. **The data is passed from the kernel to user-space via per-CPU ring buffers, and to keep context-switch overheads to a minimum perf(1) is woken up an infrequent and dynamic number of times to read them.** —— perf 自己也要花 CPU；这就是 `perf_cpu_time_max_percent` 存在的理由。
15. **This left-to-right ordering is called caller by perf(1). You can flip this to callee ordering, where the event function is on the left and its ancestry is down and to the right using: -g callee (it was the default; perf(1) switched to the caller ordering in Linux 4.4).** —— 报告的读法会随版本变；看图先确认方向。
16. **Flame graphs visualize stack traces. While commonly used for CPU profiles, they can visualize any collection of stack traces collected by perf(1), including on context switch events to see why threads are leaving CPU, and on block I/O creation to see which code paths are creating disk I/O.** —— 火焰图不只画 CPU 火焰，也能画「谁在离开 CPU」和「谁在制造磁盘 I/O」。
17. **The perf trace subcommand will trace system calls by default and print output live (no perf.data file).** —— `perf trace` 是 strace 的低开销系统级替代品，而且不留文件。
18. **Performance monitoring counters (PMCs) can explain CPU cycle performance, and are available on certain instances in AWS EC2.** ——（第 16 章 §16.1.5）云上的 PMC 不是标配；「事件不支持」通常不是代码的问题。
