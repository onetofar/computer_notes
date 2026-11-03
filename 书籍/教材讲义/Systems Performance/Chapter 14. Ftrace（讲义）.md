---
title: Chapter 14. Ftrace（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - ftrace
  - tracepoint
  - kprobes
  - 内核追踪
  - 系统性能
source: "《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg，Addison-Wesley 2020，英文原版）第 14 章"
companion: "[[14-Chapter 14. Ftrace]]"
---

# 第 14 章 讲义：ftrace 是内核的黑匣子

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[14-Chapter 14. Ftrace|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：`otf-server/src/net/tcp_transport.cpp` 的 epoll 事件循环（`Poll` / `OnReadable` / `FlushWrite` / `Ctl`）与 `src/app/tick_scheduler.cpp` 的 `PollTimeoutMs`——本仓库**零 tracing 资产**，本章讲的正是它缺的那一块。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原章。

---

## 0. 开局：看不见的那 8 秒

服务器跑着跑着，玩家反馈「操作延迟」。你 `strace -p` 挂上去，看到主循环卡在一次 `epoll_wait` 上——**8 秒**。

可这个仓库的超时是明写着的：`config::kMaxPollTimeoutMs = 50`，`TickScheduler::PollTimeoutMs()` 把 `epoll_wait` 的 timeout 死死 clamp 在 `[0, 50]` 毫秒（`src/app/tick_scheduler.cpp:36-41`）。**8 秒和 50 毫秒之间发生了什么，`strace` 一个字都不说**——它只看得见系统调用的进出边界，看不见边界以内：内核为什么没按 timeout 醒、进程为什么没被唤醒、是网卡没来数据、还是有人把 CPU 抢走了。

这就是 ftrace 的位置。（推演）strace 像是趴在门缝里听声音——能听见「有人敲了门」和「门开了」，但门里发生了什么、谁拦着门，全看不见；ftrace 则是**把机器拆开、装一支黑匣子**，让内核自己在每一个关键节点按下暂停键。

而这一章要回答的最实用的问题是：**我怎么知道我的进程到底调了哪些内核函数、调了多少次、每次多久？** 前面第 4 章讲过 tracepoints / kprobes / uprobes 是什么（那是**概念**），这一章讲的是**怎么把它们拧起来用**——一整套只用 shell 就能上手的内核观测流水线。

---

## 1. §14.1 Capabilities Overview：先把「两种工具」分清

Ftrace 是 Linux 官方的 tracer，由 Steven Rostedt 写，2008 年随 Linux 2.6.27 进内核。它的定位有三个特点：**官方**、**零用户态前端依赖**（适合存储紧张的嵌入式）、**服务器环境也用得上**。

它能回答四类问题（原书原列）：

- How often are certain kernel functions called?（某个内核函数被调了多频繁）
- What code path led to calling this function?（是什么代码路径调用了它）
- What child functions does this kernel function call?（它内部又调了谁）
- What is the highest latency caused by preemption disabled code paths?（关抢占的代码路径造成了多大延迟）

### profiler 与 tracer：一条线画在「统计」和「逐条」之间

> **Profilers provide statistical summaries such as counts and histograms, and tracers provide per-event details.**

（推演）体检报告 vs 病历流水：**体检**给你一组计数与直方图（多少项、分布如何），**流水**给你每一条事件的明细（谁在什么时候干的）。前者便宜，后者昂贵——本书后面反复用这个差别来安排「先用便宜的筛，再用贵的看」的顺序。

原书开篇给的例子是 `funcgraph(8)`，它用一个 Ftrace tracer 显示 `vfs_read()` 的子调用：

```
# funcgraph vfs_read
Tracing "vfs_read"... Ctrl-C to end.
 1)               |  vfs_read() {
 1)               |    rw_verify_area() {
 1)               |      security_file_permission() {
 1)               |        apparmor_file_permission() {
 1)               |          common_file_perm() {
 1)   0.763 us    |            aa_file_perm();
 1)   2.209 us    |          }
 1)   3.329 us    |        }
 1)   0.571 us    |        __fsnotify_parent();
 1)   0.612 us    |        fsnotify();
 1)   7.019 us    |      }
 1)   8.416 us    |    }
 1)               |    __vfs_read() {
 1)               |      new_sync_read() {
 1)               |        ext4_file_read_iter() {
[...]
```

左边第二列是**每个函数的耗时**（us = 微秒），所以你能看出「父函数慢，是哪个子函数拖的」。

### 原书的两张清单

**表 14.1（Ftrace profilers）**：`function`（内核函数统计，§14.3）、`kprobe profiler`（已启用 kprobe 的计数，§14.6.5）、`uprobe profiler`（§14.7.4）、`hist triggers`（事件上的自定义直方图，§14.10）。

**表 14.2（Ftrace 与 event tracers）**：`function`、`tracepoints`、`kprobes`、`uprobes`、`function_graph`，以及一批**只在表里出现、原书没有专节展开**的：`wakeup` / `wakeup_rt`（测调度器延迟）、`irqsoff` / `preemptoff` / `preemptirqsoff`（关中断 / 关抢占期间的延迟）、`blk`、`hwlat`、`mmiotrace`、`nop`。

> ⚠️ 原书明确标注：`irqsoff`、`preemptoff`、`preemptirqsoff` 需要内核开 `CONFIG_PREEMPTIRQ_EVENTS`。
> ⚠️ `wakeup` / `irqsoff` / `preemptoff` 这几个 tracer **在本章没有独立小节**（表 14.2 的 Section 列写的是 `-`）。它们怎么用？按 §14.2 的 tracefs 通法：把名字写进 `current_tracer` 即可。本讲义不替原书编一节出来。

看自己内核上有哪些 tracer：

```
# cat /sys/kernel/debug/tracing/available_tracers
hwlat blk mmiotrace function_graph wakeup_dl wakeup_rt wakeup function nop
```

---

## 2. §14.2 tracefs (/sys)：一切都是文件

Ftrace 的控制界面是 **tracefs** 文件系统，应该挂在 `/sys/kernel/tracing`：

```
mount -t tracefs tracefs /sys/kernel/tracing
```

Ftrace 原本是 debugfs 的一部分，后来被拆成独立的 tracefs。debugfs 挂着的时候，tracefs 会作为 `tracing` 子目录挂在里面：

```
# mount -t debugfs,tracefs
debugfs on /sys/kernel/debug type debugfs (rw,relatime)
tracefs on /sys/kernel/debug/tracing type tracefs (rw,relatime)
```

> ⚠️ **原文稿里所有例子用的都是 `/sys/kernel/debug/tracing`**（Ubuntu 19.10 的布局）。原书明确说：将来应该改成 `/sys/kernel/tracing`。**本讲义照抄原文的路径，不替它「修正」**——你要按自己机器的实际挂载点来。

挂不上 tracefs？原书给了一条最常见的原因：

> Note that if tracefs fails to mount, one possible reason is that your kernel has been built without the Ftrace config options (CONFIG_FTRACE, etc.).

### §14.2.1 tracefs Contents：工具箱地图

```
# ls -F /sys/kernel/debug/tracing
available_events            max_graph_depth      stack_trace_filter
available_filter_functions  options/             synthetic_events
available_tracers           per_cpu/             timestamp_mode
buffer_percent              printk_formats       trace
buffer_size_kb              README               trace_clock
buffer_total_size_kb        saved_cmdlines       trace_marker
current_tracer              saved_cmdlines_size  trace_marker_raw
dynamic_events              saved_tgids          trace_options
dyn_ftrace_total_info       set_event            trace_stat/
enabled_functions           set_event_pid        trace_options
error_log                   set_ftrace_filter    tracing_cpumask
events/                     set_ftrace_notrace   tracing_max_latency
free_buffer                 set_ftrace_pid       tracing_on
function_profile_enabled    set_graph_function   tracing_thresh
hwlat_detector/             set_graph_notrace    uprobe_events
instances/                  snapshot             uprobe_profile
kprobe_events               stack_max_size
kprobe_profile              stack_trace
```

（上面 `trace_options` 一行原书输出里只出现一次；这是抽取时表格化造成的重复，实际目录里是每个文件各一行。）

**表 14.3 里最该先记住的几个**：

| 文件 | 读写 | 干什么 |
|---|---|---|
| `available_tracers` | 读 | 列出可用 tracer |
| `current_tracer` | 读写 | **当前启用哪个 tracer** |
| `function_profile_enabled` | 读写 | 开关 function profiler |
| `available_filter_functions` | 读 | 列出可被 trace 的函数 |
| `set_ftrace_filter` | 读写 | **选哪些函数要 trace** |
| `tracing_on` | 读写 | 开关环形缓冲区的输出 |
| `trace` / `trace_pipe` | 读写/读 | tracer 的输出 |
| `trace_options` | 读写 | 输出格式选项 |
| `trace_stat/` | 读写 | function profiler 的统计结果 |
| `kprobe_events` / `uprobe_events` | 读写 | kprobe / uprobe 的配置 |
| `events/` | 读写 | tracepoint / kprobe / uprobe 的控制文件 |
| `instances/` | 读写 | **给并发用户各开一份 tracer** |

启用一个 tracer 就是往 `current_tracer` 写名字：

```
# cat /sys/kernel/debug/tracing/current_tracer
nop
# echo blk > /sys/kernel/debug/tracing/current_tracer
```

`nop` = no operation，也就是**当前没有任何 tracer 在跑**。这是最安全的默认状态——不主动关掉的话，你的机器一直在裸奔。

原书在这里给了一个很实用的观察：**Ftrace 几乎零依赖**（"virtually zero dependencies"，只需要一个 shell；`cat(1)` 甚至可以用 shell builtin 顶替）。

### instances：多个人同时用

`current_tracer` 一次只能设一个 tracer——这是 Ftrace 早期的限制（Steven Rostedt 做 realtime patch set 时的自用工具）。后来加的 `instances/` 目录解决它：每个 instance 有自己的 `current_tracer` 和输出文件，**各自独立地跑 tracing**。

（推演）这像共享一台示波器：全局档位只有一格，谁按下去谁就用；`instances/` 相当于给每人分一台。

---

## 3. §14.3 Ftrace Function Profiler：先数，再看

Function profiler 给出**内核函数调用的统计**，适合回答「这个负载上，哪些内核函数在跑、哪些最慢」。

原书说这是他**理解一段负载的内核执行路径时的起点**，因为它「efficient and costs relatively low overhead」（高效、开销相对低）。

### 它是怎么做到的

工作方式是在**每个内核函数入口插入编译期 profiling 调用**，思路来自编译器 profiler——像 gcc 的 `-pg` 选项插入 `mcount()` 供 gprof 用；gcc 4.6 起这个调用改叫 `__fentry__()`。

给每个内核函数都插调用，开销听起来很大，但问题被解决了：

> when not in use, these calls are typically replaced with fast nop instructions, and only switched to the `__fentry__()` calls when needed

不用的时候，这些调用被换成**一条 nop 指令**；要用时才切回 `__fentry__()`。

### 命令序列

先看未启用时的原始状态：

```
# cd /sys/kernel/debug/tracing
# cat set_ftrace_filter
#### all functions enabled ####
# cat function_profile_enabled
0
```

然后统计所有以 “tcp” 开头的内核函数调用，约 10 秒：

```
# echo 'tcp*' > set_ftrace_filter
# echo 1 > function_profile_enabled
# sleep 10
# echo 0 > function_profile_enabled
# echo > set_ftrace_filter
```

> ⚠️ 原书的一个小坑提醒：要用 `> 0 >`（带空格），**不是** `> 0>`——后者是文件描述符 0 的重定向。同理避免 `> 1>`。

统计结果在 `trace_stat/` 目录，每个 CPU 一个 `function` 文件（这是 2-CPU 机器）：

```
# head trace_stat/function*
==> trace_stat/function0 <==
  Function                       Hit    Time            Avg             s^2
  --------                       ---    ----            ---             ---
  tcp_sendmsg                 955912    2788479 us      2.917 us        3734541 us
  tcp_sendmsg_locked          955912    2248025 us      2.351 us        2600545 us
  tcp_push                    955912    852421.5 us     0.891 us        1057342 us
  tcp_write_xmit              926777    674611.1 us     0.727 us        1386620 us
  tcp_send_mss                955912    504021.1 us      0.527 us        95650.41 us
  tcp_current_mss             964399    317931.5 us      0.329 us        136101.4 us
  tcp_poll                    966848    216701.2 us      0.224 us        201483.9 us
  tcp_release_cb              956155    102312.4 us      0.107 us        188001.9 us

==> trace_stat/function1 <==
  Function                       Hit    Time            Avg             s^2
  --------                       ---    ----            ---             ---
  tcp_sendmsg                 317935    936055.4 us      2.944 us        13488147 us
  tcp_sendmsg_locked          317935    770290.2 us      2.422 us        8886817 us
  tcp_write_xmit              348064    423766.6 us     1.217 us        226639782 us
  tcp_push                    317935    310040.7 us      0.975 us        4150989 us
  tcp_tasklet_func             38109    189797.2 us     4.980 us        2239985 us
  tcp_tsq_handler              38109    180516.6 us     4.736 us        2239552 us
  tcp_tsq_write.part.0         29977    173955.7 us     5.802 us        1037352 us
  tcp_send_mss                317935    165881.9 us      0.521 us        352309.0 us
```

五列依次是：函数名、**调用次数**、函数内总时间、平均耗时、**标准差**（`s^2`）。这张表一眼告诉你 `tcp_sendmsg()` 最频繁（CPU0 上 95 万次），平均 2.9 微秒。

### 开销纪律

> A small amount of overhead is added to the profiled functions during profiling.

而且：`set_ftrace_filter` **留空 = 所有内核函数都被 profile**（就是开头那句 `#### all functions enabled ####`）。原书提醒务必用过滤器限制范围。

**用它的正确姿势**（这也是本章最重要的一条方法论）：

> You can use function profiling from the previous section to find out the rate of functions before tracing them.

**先用便宜的 profiler 测频率，再用贵的 tracer 看细节。** 这条纪律在实战锚点里会直接兑现。

---

## 4. §14.4 Ftrace Function Tracing：逐条事件的流水

Function tracer 打印**每次**内核函数调用的明细，能看出函数序列、时间戳模式、以及**在 CPU 上的是哪个进程、哪个 PID**。

但它的代价写在了明处：

> The overhead of function tracing is higher than function profiling, and so tracing is best suited for relatively infrequent functions (less than 1,000 calls per second).

**每秒不到 1000 次调用的函数，才值得逐条 trace。**

### §14.4.1 用 trace 读

```
# cd /sys/kernel/debug/tracing
# echo 1 > tracing_on
# echo '*sleep' > set_ftrace_filter
# echo function > current_tracer
# sleep 10
# cat trace > /tmp/out.trace01.txt
# echo nop > current_tracer
# echo > set_ftrace_filter
```

输出：

```
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
      multipathd-348   [001] .... 332762.532879: hrtimer_nanosleep <-
__x64_sys_nanosleep
      multipathd-348   [001] .... 332762.532880: do_nanosleep <-hrtimer_nanosleep
           sleep-4203  [001] .... 332762.722497: __x64_sys_nanosleep <-do_syscall_64
           sleep-4203  [001] .... 332762.722498: hrtimer_nanosleep <-
__x64_sys_nanosleep
           sleep-4203  [001] .... 332762.722498: do_nanosleep <-hrtimer_nanosleep
      multipathd-348   [001] .... 332763.532966: __x64_sys_nanosleep <-do_syscall_64
[...]
```

那几行 `_-----=> irqs-off` 的图例是**延迟标记**：一个事件里发生了多少层中断关闭 / 需要重调度 / 硬中断 / 抢占深度 / 延迟。这套标记在 §14.8 会再出现一次（那里有完整的字符表）。

最后两个字段是**当前函数**和**调用它的父函数**。

清空缓冲区：往 `trace` 写一个换行（`# > trace`）；把 `current_tracer` 设回 `nop` 也会清空。

### §14.4.2 用 trace_pipe 读

`trace_pipe` 是同一个缓冲区的另一个接口，读它得到的是**无尽的事件流**，而且**读走即消费**：

> It also consumes events, so after reading them once they are no longer in the trace buffer.

```
# echo '*sleep' > set_ftrace_filter
# echo function > current_tracer
# cat trace_pipe
      multipathd-348   [001] .... 332624.519190: __x64_sys_nanosleep <-do_syscall_64
      multipathd-348   [001] .... 332624.519192: hrtimer_nanosleep <-
__x64_sys_nanosleep
      multipathd-348   [001] .... 332624.519192: do_nanosleep <-hrtimer_nanosleep
      multipathd-348   [001] .... 332625.519272: __x64_sys_nanosleep <-do_syscall_64
      multipathd-348   [001] .... 332625.519274: hrtimer_nanosleep <-
__x64_sys_nanosleep
      multipathd-348   [001] .... 332625.519275: do_nanosleep <-hrtimer_nanosleep
            cron-504   [001] .... 332625.560150: __x64_sys_nanosleep <-do_syscall_64
            cron-504   [001] .... 332625.560152: hrtimer_nanosleep <-
__x64_sys_nanosleep
            cron-504   [001] .... 332625.560152: do_nanosleep <-hrtimer_nanosleep
^C
# echo nop > current_tracer
# echo > set_ftrace_filter
```

选哪个？原书给的分界很清楚：

> The trace_pipe file is handy for watching low-frequency events, but for high-frequency events you will want to capture them to a file for later analysis using the trace file shown earlier.

**低频实时看，高频落盘。**

### §14.4.3 Options

```
# echo 0 > options/irq-info
# cat trace
# tracer: function
#
# entries-in-buffer/entries-written: 3300/3300   #P:2
#
#           TASK-PID     CPU#   TIMESTAMP  FUNCTION
#              | |         |       |         |
      multipathd-348   [001]  332762.532877: __x64_sys_nanosleep <-do_syscall_64
      multipathd-348   [001]  332762.532879: hrtimer_nanosleep <-__x64_sys_nanosleep
      multipathd-348   [001]  332762.532880: do_nanosleep <-hrtimer_nanosleep
[...]
# echo 1 > options/irq-info
```

关掉 `irq-info` 后，那列 `....` 标记就没了。全部选项在 `options/` 目录里，名字相当直观：

```
# ls options/
annotate          funcgraph-abstime   hex              stacktrace
bin               funcgraph-cpu       irq-info         sym-addr
blk_cgname        funcgraph-duration  latency-format   sym-offset
blk_cgroup        funcgraph-irqs      markers          sym-userobj
blk_classic       funcgraph-overhead  overwrite        test_nop_accept
block             funcgraph-overrun   print-parent     test_nop_refuse
context-info      funcgraph-proc      printk-msg-only  trace_printk
disable_on_free   funcgraph-tail      raw              userstacktrace
display-graph     function-fork       record-cmd       verbose
event-fork        function-trace      record-tgid
func_stack_trace  graph-time          sleep-time
```

其中 `stacktrace` 和 `userstacktrace` 会**附加内核栈和用户栈**——原书说这是「理解函数为什么被调用」的利器。

---

## 5. §14.5 Tracepoints：内核自己埋好的静态探针

> **Tracepoints are kernel static instrumentation**

第 4 章介绍过 tracepoint 的概念，这里是**怎么用**。它们本质上是放在内核源码里的 tracing 函数，通过一个定义并格式化参数的 trace event 接口暴露出来；trace event 在 tracefs 可见，**与 Ftrace 共用输出和控制文件**。

```
# cd /sys/kernel/debug/tracing
# echo 1 > events/block/block_rq_issue/enable
# cat trace_pipe
            sync-4844  [001] .... 343996.918805: block_rq_issue: 259,0 WS 4096 ()
2048 + 8 [sync]
            sync-4844  [001] .... 343996.918808: block_rq_issue: 259,0 WSM 4096 ()
10560 + 8 [sync]
            sync-4844  [001] .... 343996.918809: block_rq_issue: 259,0 WSM 4096 ()
38424 + 8 [sync]
            sync-4844  [001] .... 343996.918809: block_rq_issue: 259,0 WSM 4096 ()
4196384 + 8 [sync]
            sync-4844  [001] .... 343996.918810: block_rq_issue: 259,0 WSM 4096 ()
4462592 + 8 [sync]
^C
# echo 0 > events/block/block_rq_issue/enable
```

`events/` 下**每个 trace system 一个目录**（如 `block`），每个 event 一个子目录：

```
# ls events/block/block_rq_issue/
enable  filter  format  hist  id  trigger
```

`enable` 开关它；`filter` 过滤；`format` 是参数与格式串的定义；`hist` 是直方图；`trigger` 是触发器。

### §14.5.1 Filter

过滤语法受限：

```
field operator value
```

数值操作符：`==, !=, <, <=, >, >=, &`；字符串操作符：`==, !=, ~`（`~` 是 shell 通配式匹配，支持 `*`、`?`、`[]`）。可以用括号分组，用 `&&`、`||` 组合。

```
# echo 'bytes > 65536' > events/block/block_rq_insert/filter
# cat trace_pipe
    kworker/u4:1-7173  [000] .... 378115.779394: block_rq_insert: 259,0 W 262144 ()
5920256 + 512 [kworker/u4:1]
    kworker/u4:1-7173  [000] .... 378115.784654: block_rq_insert: 259,0 W 262144 ()
5924336 + 512 [kworker/u4:1]
    kworker/u4:1-7173  [000] .... 378115.789136: block_rq_insert: 259,0 W 262144 ()
5928432 + 512 [kworker/u4:1]
^C
# echo 0 > events/block/block_rq_insert/filter
```

`echo 0` 重置过滤器。

### §14.5.2 Trigger

> A trigger runs an extra tracing command when an event fires.

那条命令可以是：开关别的 tracing、打印栈、给 tracing 缓冲区拍快照。可用的 trigger 列在 `trigger` 文件里：

```
# cat events/block/block_rq_issue/trigger
# Available triggers:
# traceon traceoff snapshot stacktrace enable_event disable_event enable_hist
disable_hist hist
```

最妙的一种用法是**「只保留出事之前的那段」**：在错误条件上挂一个 trigger，让它 `traceoff`（停录）或 `snapshot`（拍快照）——缓冲区里就只剩下错误发生之前的现场。

trigger 可以和 filter 组合，用 `if` 关键字：

```
# echo 'traceoff if bytes > 65536' > events/block/block_rq_insert/trigger
```

---

## 6. §14.6–14.7 kprobes 与 uprobes：自己下探针

上一节的 tracepoint 是**内核作者埋好的**。这一节是**你自己埋**。

### §14.6 kprobes：内核里的动态探针

> **kprobes are kernel dynamic instrumentation**

kprobe 事件创建出来之后，同样出现在 `events/` 目录里，与 tracepoint 共享 tracefs 的输出与控制文件。和 function tracer 一样，它们追的是内核函数，但**能做得更多**：可以下在函数的**具体偏移**（单条指令）上，可以报告**函数参数和返回值**。

```
# echo 'p:brendan do_nanosleep' >> kprobe_events
# echo 1 > events/kprobes/brendan/enable
# cat trace_pipe
      multipathd-348   [001] .... 345995.823380: brendan: (do_nanosleep+0x0/0x170)
      multipathd-348   [001] .... 345996.823473: brendan: (do_nanosleep+0x0/0x170)
      multipathd-348   [001] .... 345997.823558: brendan: (do_nanosleep+0x0/0x170)
^C
# echo 0 > events/kprobes/brendan/enable
# echo '-:brendan' >> kprobe_events
```

完整语法（引自内核源码 `Documentation/trace/kprobetrace.rst`）：

```
  p[:[GRP/]EVENT] [MOD:]SYM[+offs]|MEMADDR [FETCHARGS]  : Set a probe
  r[MAXACTIVE][:[GRP/]EVENT] [MOD:]SYM[+0] [FETCHARGS]  : Set a return probe
  -:[GRP/]EVENT                                         : Clear a probe
```

**自定义名字很有用**——它让不同用户能共存。原书举了 BCC 的命名习惯（函数名 + `bcc` + PID）：

```
# cat /sys/kernel/debug/tracing/kprobe_events
p:kprobes/p_blk_account_io_start_bcc_19454 blk_account_io_start
p:kprobes/p_blk_mq_start_request_bcc_19454 blk_mq_start_request
```

> ⚠️ 原书注：新内核上 BCC 已改用 `perf_event_open(2)` 来挂 kprobe，所以这些事件**不出现在 `kprobe_events` 里**。

### §14.6.2 Arguments

kprobe 能读函数参数和返回值，这是 function tracing 做不到的。

```
# echo 'p:brendan do_nanosleep hrtimer_sleeper=$arg1 hrtimer_mode=$arg2' >>
kprobe_events
# echo 1 > events/kprobes/brendan/enable
# cat trace_pipe
      multipathd-348   [001] .... 349138.128610: brendan: (do_nanosleep+0x0/0x170)
hrtimer_sleeper=0xffffaa6a4030be80 hrtimer_mode=0x1
      multipathd-348   [001] .... 349139.128695: brendan: (do_nanosleep+0x0/0x170)
hrtimer_sleeper=0xffffaa6a4030be80 hrtimer_mode=0x1
      multipathd-348   [001] .... 349140.128785: brendan: (do_nanosleep+0x0/0x170)
hrtimer_sleeper=0xffffaa6a4030be80 hrtimer_mode=0x1
^C
# echo 0 > events/kprobes/brendan/enable
# echo '-:brendan' >> kprobe_events
```

`$arg1` / `$arg2` 这种别名是 **Linux 4.20** 才加的。更早的版本必须用**寄存器名**：

```
# echo 'p:brendan do_nanosleep hrtimer_sleeper=%di hrtimer_mode=%si' >> kprobe_events
```

x86_64 用 AMD64 ABI，前两个参数在 `rdi` 和 `rsi`。

### §14.6.3 Return Values

`$retval` 是返回值的别名，配合 kretprobe 用：

```
# echo 'r:brendan do_nanosleep ret=$retval' >> kprobe_events
# echo 1 > events/kprobes/brendan/enable
# cat trace_pipe
      multipathd-348   [001] d... 349782.180370: brendan:
(hrtimer_nanosleep+0xce/0x1e0 <- do_nanosleep) ret=0x0
      multipathd-348   [001] d... 349783.180443: brendan:
(hrtimer_nanosleep+0xce/0x1e0 <- do_nanosleep) ret=0x0
      multipathd-348   [001] d... 349784.180530: brendan:
(hrtimer_nanosleep+0xce/0x1e0 <- do_nanosleep) ret=0x0
^C
# echo 0 > events/kprobes/brendan/enable
# echo '-:brendan' >> kprobe_events
```

### §14.6.4 Filters and Triggers

> Filters and triggers can be used from the events/kprobes/... directory, as they were with tracepoints

自定义的 `$arg1` 会变成 `format` 文件里的一个**字段**，因此可以直接拿来过滤：

```
# echo 'hrtimer_mode != 1' > events/kprobes/brendan/filter
```

### §14.6.5 kprobe Profiling

```
# cat /sys/kernel/debug/tracing/kprobe_profile
  p_blk_account_io_start_bcc_19454                        1808               0
  p_blk_mq_start_request_bcc_19454                         677               0
  p_blk_account_io_completion_bcc_19454                    521              11
  p_kbd_event_1_bcc_1119                                   632               0
```

三列：探针名、**命中数**、**miss 数**（探针命中了但出错了、没被记下来的那些）。

原书的用法说明很直白：function profiler 也能数函数调用，但 kprobe profiler 适合**检查监控软件常开的那批 kprobe 有没有在疯狂触发**。

### §14.7 uprobes：用户态的动态探针

> **uprobes are user-level dynamic instrumentation**

内核**没有**用户态软件的符号信息，所以 uprobe 需要**路径 + 偏移**：

```
  p[:[GRP/]EVENT] PATH:OFFSET [FETCHARGS] : Set a uprobe
  r[:[GRP/]EVENT] PATH:OFFSET [FETCHARGS] : Set a return uprobe (uretprobe)
  -:[GRP/]EVENT                           : Clear uprobe or uretprobe event
```

```
# readelf -s /bin/bash | grep -w readline
   882: 00000000000b61e0   153 FUNC    GLOBAL DEFAULT   14 readline
# echo 'p:brendan /bin/bash:0xb61e0' >> uprobe_events
# echo 1 > events/uprobes/brendan/enable
# cat trace_pipe
            bash-3970  [000] d... 347549.225818: brendan: (0x55d0857b71e0)
            bash-4802  [000] d... 347552.666943: brendan: (0x560bcc1821e0)
            bash-4802  [000] d... 347552.799480: brendan: (0x560bcc1821e0)
^C
# echo 0 > events/uprobes/brendan/enable
# echo '-:brendan' >> uprobe_events
```

⚠️ **原书在这里放了一个 WARNING，值得整段照抄**：

> WARNING: If you mistakenly use a symbol offset that is midway through an instruction, you will corrupt the target process (and for shared instruction text, all processes that share it!). The example technique of using readelf(1) to find the symbol offset may not work if the target binary has been compiled as a position-independent executable (PIE) with address space layout randomization (ASLR). I do not recommend that you use this interface at all: switch to a higher-level tracer that takes care of symbol mappings for you (e.g., BCC or bpftrace).

参数 / 返回值 / 过滤 / 触发 / profiling 全部与 kprobe 同构（§14.7.2–14.7.4），不再展开。

---

## 7. §14.8 Ftrace function_graph：把调用树画出来

Function graph tracer 打印**调用图**，揭示代码的流动关系。开头那个 `funcgraph(8)` 就是它。

```
# cd /sys/kernel/debug/tracing
# cat set_graph_function
#### all functions enabled ####
# cat current_tracer
nop
```

### §14.8.1 Graph Tracing

```
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
 1)               |      rcu_note_context_switch() {
[...]
 5) $ 1000383 us  |  } /* do_nanosleep */
^C
# echo nop > current_tracer
# echo > set_graph_function
```

左列是 CPU 号和函数内耗时。**高延迟会带一个特殊字符**（原书引 [Rostedt 08] 给的表）：

| 字符 | 含义 |
|---|---|
| `$` | Greater than 1 second |
| `@` | Greater than 100 ms |
| `*` | Greater than 10 ms |
| `#` | Greater than 1 ms |
| `!` | Greater than 100 μs |
| `+` | Greater than 10 μs |

**关键的一条观察**：这个例子**故意没设函数过滤器**，所以能看到所有子调用——但代价是开销会**膨胀报告出来的耗时**。

> It is still generally useful for locating the origin of high latencies, which can dwarf the added overhead.

要更准的耗时，就缩小追踪范围：

```
# echo do_nanosleep > set_ftrace_filter
# cat trace_pipe
[...]
 7) $ 1000130 us  |  } /* do_nanosleep */
^C
```

同一个负载（`sleep 1`），加了过滤器之后，`do_nanosleep()` 的报告耗时从 **1000383 μs 降到 1000130 μs**——差的 253 μs 就是「追踪所有子函数」本身的成本。**这是全章最直白的一次「测量改变被测量」演示。**

### §14.8.2 Options

```
# ls options/funcgraph-*
options/funcgraph-abstime   options/funcgraph-irqs      options/funcgraph-proc
options/funcgraph-cpu       options/funcgraph-overhead  options/funcgraph-tail
options/funcgraph-duration  options/funcgraph-overrun
```

分别控制 CPU ID、进程名、函数耗时、延迟标记的显示。

---

## 8. §14.9 Ftrace hwlat：看不见的硬件级扰动

hwlat（hardware latency detector）是一个**专用 tracer**：它能发现**外部硬件事件对 CPU 性能的扰动**——那些内核和其他工具都看不见的事件，比如 SMI（系统管理中断）和 hypervisor 扰动（包括**吵闹邻居** noisy neighbor 造成的）。

工作方式：跑一段**关中断的代码循环**当实验，测量每次迭代的耗时；一次只在一个 CPU 上跑，轮流轮转。每个 CPU 上最慢的那次迭代会被打印出来——只要它超过阈值（10 微秒，阈值可用 `tracing_thresh` 文件配置）。

```
# cd /sys/kernel/debug/tracing
# echo hwlat > current_tracer
# cat trace_pipe
           <...>-5820  [001] d... 354016.973699: #1     inner/outer(us): 2152/1933
ts:1578801212.559595228
           <...>-5820  [000] d... 354017.985568: #2     inner/outer(us):   19/26
ts:1578801213.571460991
           <...>-5820  [001] dn.. 354019.009489: #3     inner/outer(us): 1699/5894
ts:1578801214.595380588
           <...>-5820  [000] d... 354020.033575: #4     inner/outer(us):   43/49
ts:1578801215.619463259
           <...>-5820  [001] d... 354021.057566: #5     inner/outer(us):   18/45
ts:1578801216.643451721
           <...>-5820  [000] d... 354022.081503: #6     inner/outer(us):   18/38
ts:1578801217.667385514
^C
# echo nop > current_tracer
```

`inner/outer(us)` 是「循环内」和「绕回下一轮」两段的时间。第一行的 2152 μs 远超 10 μs 阈值——这就是一次外部扰动。

⚠️ **原书给它下了一个很硬的定义**：

> WARNING: I'd classify hwlat as a microbenchmark tool rather than an observability tool, because it performs an experiment that will itself perturb the system: it will make one CPU busy for the width duration, with interrupts disabled.

**它是微基准工具，不是观测工具**——它自己会扰动系统：让一个 CPU 在 `width` 时长里一直忙着，且中断全关。

---

## 9. §14.10 Ftrace Hist Triggers：让内核替你算

这一节是全章最值钱的部分。它是 Linux 4.7 由 Tom Zanussi 加入的高级能力：**在事件上创建自定义直方图**，按一个或多个维度拆分计数。

### 为什么它值钱

原书在 §14.13.2 讲清楚了动机：`iolatency(8)` 这类工具的做法是「trace 两个 tracepoint → 用户态读全部事件 → 解析 → 用 awk 后处理成直方图」。磁盘 I/O 频率低，这么干还能接受；但**网络 I/O 或调度事件频率高，开销就不可接受了**。

> Extended BPF solved this problem by allowing the histogram summary to be calculated in kernel space, and only the summary is passed to user space, greatly reducing overhead.

Ftrace 现在的 hist trigger + synthetic event 就是往这个方向补的——**把统计搬回内核**。

### 基本用法

```
echo 'hist: expression ' > events/.../trigger : Create a hist trigger.

sleep duration : Allow histogram to populate.

cat events/.../hist : Print the histogram.

echo '!hist: expression ' > events/.../trigger : Remove it.
```

表达式格式：

```
hist:keys=<field1[,field2,...]>[:values=<field1[,field2,...]>]
  [:sort=<field1[,field2,...]>][:size=#entries][:pause][:continue]
  [:clear][:name=histname1][:<handler>.<action>] [if <filter>]
```

### §14.10.1 单键

用 `raw_syscalls:sys_enter` 这个 tracepoint，按进程 ID 统计 syscall 次数：

```
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
{ common_pid:        345 } hitcount:          3
{ common_pid:        504 } hitcount:          8
{ common_pid:        494 } hitcount:         20
{ common_pid:        502 } hitcount:         30
{ common_pid:        344 } hitcount:         32
{ common_pid:        348 } hitcount:         36
{ common_pid:      32399 } hitcount:        136
{ common_pid:      32400 } hitcount:        138
{ common_pid:      32379 } hitcount:        177
{ common_pid:      32296 } hitcount:        187
{ common_pid:      32396 } hitcount:     882604

Totals:
    Hits: 883372
    Entries: 12
    Dropped: 0
# echo '!hist:key=common_pid' > events/raw_syscalls/sys_enter/trigger
```

末尾三行统计：写进哈希的次数（`Hits`）、哈希里的条目数（`Entries`）、**因超出哈希大小被丢弃的次数（Dropped`）**。有 drop 就加 `:size=`，默认 2048。

### §14.10.2 Fields 与 §14.10.3 Modifiers

字段来自事件的 `format` 文件：

```
# cat events/raw_syscalls/sys_enter/format
[...]
        field:int common_pid;       offset:4;  size:4;    signed:1;

        field:long id;   offset:8;  size:8;    signed:1;
        field:unsigned long args[6];    offset:16;   size:48;  signed:0;
```

`id` 字段就是 syscall ID：

```
# echo 'hist:key=id' > events/raw_syscalls/sys_enter/trigger
# cat events/raw_syscalls/sys_enter/hist
[...]
{ id:         14 } hitcount:         48
{ id:          1 } hitcount:      80362
{ id:          0 } hitcount:      80396
[...]
```

怎么知道 ID 0 和 1 是谁？

```
# more /usr/include/x86_64-linux-gnu/asm/unistd_64.h
[...]
#define __NR_read 0
#define __NR_write 1
[...]
```

**`read` 和 `write`**。这一步是全章最实用的一个技巧：**用内核自己数 syscall 分布，不用猜。**

修饰符 `.execname` / `.syscall` 把名字带出来：

```
# echo 'hist:key=common_pid.execname' > events/raw_syscalls/sys_enter/trigger
[...]
{ common_pid: bash            [     32379] } hitcount:        166
{ common_pid: sshd            [     32296] } hitcount:        259
{ common_pid: dd              [     32396] } hitcount:     869024
[...]
```

### §14.10.4 PID Filters 与 §14.10.5 Multiple Keys

要验证「dd 进程是不是就是那些 read/write」，把 PID 和 syscall ID 合起来：

```
# echo 'hist:key=id.syscall if common_pid==32396' > \
    events/raw_syscalls/sys_enter/trigger
# cat events/raw_syscalls/sys_enter/hist
# event histogram
#
# trigger info: hist:keys=id.syscall:vals=hitcount:sort=hitcount:size=2048 if common_
pid==32396 [active]
#

{ id: sys_write                     [  1] } hitcount:     106425
{ id: sys_read                      [  0] } hitcount:     106425

Totals:
    Hits: 212850
    Entries: 2
    Dropped: 0
```

多键版本（进程名 + syscall ID 一起拆）：

```
# echo 'hist:key=common_pid.execname,id' > events/raw_syscalls/sys_enter/trigger
# sleep 10
# cat events/raw_syscalls/sys_enter/hist
# event histogram
#
# trigger info: hist:keys=common_pid.execname,id:vals=hitcount:sort=hitcount:size=2048
[active]
[...]
{ common_pid: sshd            [   14250], id:         23 } hitcount:         36
{ common_pid: bash            [   14261], id:         13 } hitcount:         42
{ common_pid: sshd            [   14250], id:         14 } hitcount:         72
{ common_pid: dd              [   14325], id:          0 } hitcount:    9195176
{ common_pid: dd              [   14325], id:          1 } hitcount:    9195176

Totals:
    Hits: 18391064
    Entries: 75
    Dropped: 0
```

### §14.10.6 Stack Trace Keys

把**整条内核栈**当作直方图的 key——这是作者专门向 Tom Zanussi 提的需求，用来回答「是什么代码路径导致了这件事」：

```
# echo 'hist:key=stacktrace' > events/block/block_rq_issue/trigger
# sleep 10
# cat events/block/block_rq_issue/hist
[...]
{ stacktrace:
         nvme_queue_rq+0x16c/0x1d0
         __blk_mq_try_issue_directly+0x116/0x1c0
         blk_mq_request_issue_directly+0x4b/0xe0
         blk_mq_try_issue_list_directly+0x46/0xb0
         blk_mq_sched_insert_requests+0xae/0x100
         blk_mq_flush_plug_list+0x1e8/0x290
         blk_flush_plug_list+0xe3/0x110
         blk_finish_plug+0x26/0x34
         read_pages+0x86/0x1a0
         __do_page_cache_readahead+0x180/0x1a0
         ondemand_readahead+0x192/0x2d0
         page_cache_sync_readahead+0x78/0xc0
         generic_file_buffered_read+0x571/0xc00
         generic_file_read_iter+0xdc/0x140
         ext4_file_read_iter+0x4f/0x100
         new_sync_read+0x122/0x1b0
} hitcount:        266

Totals:
    Hits: 522
    Entries: 10
    Dropped: 0
```

### §14.10.7 Synthetic Events：配对 sys_enter / sys_exit

这是本章**最实用的一节**：用合成事件算出「每一次 syscall 的延迟」，按 syscall ID 和名字出直方图。

```
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
[...]
{ lat_us:    5779085, id: sys_epoll_wait                [232] } hitcount:          1
{ lat_us:    6232897, id: sys_poll                      [  7] } hitcount:          1
{ lat_us:    6233840, id: sys_poll                      [  7] } hitcount:          1
{ lat_us:    6233884, id: sys_futex                     [202] } hitcount:          1
{ lat_us:    7028672, id: sys_epoll_wait                [232] } hitcount:          1
{ lat_us:    9999049, id: sys_poll                      [  7] } hitcount:          1
{ lat_us:   10000097, id: sys_nanosleep                 [ 35] } hitcount:          1
{ lat_us:   10001535, id: sys_wait4                     [ 61] } hitcount:          1
{ lat_us:   10002176, id: sys_select                    [ 23] } hitcount:          1
[...]
```

原书的**表 14.4** 把这套语法逐句拆开了：

| 说明 | 语法 |
|---|---|
| 建一个叫 `syscall_latency` 的合成事件，带两个参数 `lat_us` 和 `id` | `echo 'syscall_latency u64 lat_us; long id' >> synthetic_events` |
| `sys_enter` 发生时，以 `common_pid` 为 key 记一个直方图，并把当前微秒时间存进变量 `ts0` | `echo 'hist:keys=common_pid: ... >> events/raw_syscalls/sys_enter/trigger` + `ts0=common_timestamp.usecs` |
| `sys_exit` 发生时，算出 `now - ts0` 存进 `lat_us` | `lat_us=common_timestamp.usecs-$ts0` |
| 比对两个事件的直方图 key，相同（同一个 `common_pid`）就说明延迟算对了 | `onmatch(raw_syscalls.sys_enter)` |
| 最后触发合成事件 | `.trace(syscall_latency,$lat_us,id)` |

> **这就是「sys_enter / sys_exit 成对才能算单次 syscall 耗时」的原始机制**：时间戳在 enter 时存下，exit 时相减，靠 `common_pid` 配对。

原书自嘲的一句总结：

> In a way, we're starting to write mini programs.

以及他为什么认真对待这件事：

> It’s incredibly powerful, built into the kernel, and can be used via shell scripting alone.

⚠️ 原书也提醒：输出可能极长，因为**每个微秒 × 每个 syscall ID 都是一个 key**，实践中会超过默认的 2048 条。加 `:size=...`，或者用 `.log2` 修饰符把延迟记成对数（分辨率够用，条目数大减）。

**清理**要按相反顺序把每条命令都带 `!` 前缀再执行一遍。

---

## 10. §14.11–14.12 trace-cmd 与 perf ftrace：别再手写 echo 了

### §14.11 trace-cmd

trace-cmd 是 Steven Rostedt 等人开发的开源 Ftrace 前端，支持子命令、二进制输出格式等。**它支持 Ftrace 的 function 与 function_graph tracer，也支持 tracepoint 以及已配置好的 kprobe / uprobe。**

```
# trace-cmd record -p function -l do_nanosleep sleep 10
  plugin 'function'
CPU0 data recorded at offset=0x4fe000
    0 bytes in size
CPU1 data recorded at offset=0x4fe000
    4096 bytes in size
# trace-cmd report
CPU 0 is empty
cpus=2
           sleep-21145 [001] 573259.213076: function:             do_nanosleep
      multipathd-348   [001] 573259.523759: function:             do_nanosleep
      multipathd-348   [001] 573260.523923: function:             do_nanosleep
      multipathd-348   [001] 573261.524022: function:             do_nanosleep
      multipathd-348   [001] 573262.524119: function:             do_nanosleep
[...]
```

原书对比道：trace-cmd **比等价的 tracefs 命令更简洁**，而且**更安全**：

> It is also safer: many subcommands handle cleaning up tracing state when done.

### §14.11.1 子命令

**表 14.5**：`record`（记录到 trace.dat）、`report`（读 trace.dat）、`stream`（trace 并打印到 stdout）、`list`（列出可用事件）、`stat`（内核 tracing 子系统状态）、`profile`（生成内核时间与延迟报告）、`listen`（接受网络上的 trace 请求）。另有 `start` / `stop` / `restart` / `clear`。

```
# trace-cmd record -h

trace-cmd version 2.8.3

usage:
 trace-cmd record [-v][-e event [-f filter]][-p plugin][-F][-d][-D][-o file] \
           [-q][-s usecs][-O option ][-l func][-g func][-n func] \
           [-P pid][-N host:port][-t][-r prio][-b size][-B buf][command ...]
           [-m max][-C clock]
          -e run command with event enabled
          -f filter for previous -e event
          -R trigger for previous -e event
          -p run command with plugin enabled
          -F filter only on the given process
          -P trace the given pid like -F for the command
          -c also trace the children of -F (or -P if kernel supports it)
          -C set the trace clock
          -T do a stacktrace on all events
          -l filter function name
          -g set graph function
          -n do not trace function
[...]
```

### §14.11.2 trace-cmd One-Liners

```
trace-cmd list
trace-cmd list -t
trace-cmd list -e
trace-cmd list -e syscalls:
trace-cmd list -e syscalls:sys_enter_nanosleep -F
```

```
trace-cmd record -p function -l function_name
trace-cmd record -p function -l 'tcp_*'
trace-cmd record -p function -l 'tcp_*' sleep 10
trace-cmd record -p function -l 'vfs_*' -F ls
trace-cmd record -p function -l 'vfs_*' -F -c bash
trace-cmd record -p function -l 'vfs_*' -P 21124
```

```
trace-cmd record -p function_graph -g function_name
trace-cmd record -p function_graph -g do_nanosleep sleep 10
```

```
trace-cmd record -e sched:sched_process_exec
trace-cmd record -e sched_process_exec
trace-cmd record -e block_rq_issue -T
trace-cmd record -e block
trace-cmd record -e probe:brendan sleep 10
trace-cmd record -e syscalls -F ls
```

```
trace-cmd report
trace-cmd report --cpu 0
```

```
trace-cmd record -p sched_switch
trace-cmd listen -p 8081
trace-cmd record ... -N addr:port
```

### §14.11.3 trace-cmd vs. perf(1)

**表 14.6** 里挑几行关键的：二进制输出文件 `perf.data` vs `trace.dat`；PMUs（`perf` 能，`trace-cmd` 不能）；**定时采样**（`perf` 能，`trace-cmd` 不能）；function / function_graph tracing（`trace-cmd` 完整支持，`perf` 只是「部分」）；**网络客户端/服务端**（`trace-cmd` 有，`perf` 没有）；输出文件开销（`perf` 低，`trace-cmd` **Very low**）。

```
# perf record -e syscalls:sys_enter_nanosleep -a sleep 10
# perf script
```

```
# trace-cmd record -e syscalls:sys_enter_nanosleep sleep 10
# trace-cmd report
```

> One advantage of trace-cmd is its better support for the function and function_graph tracers.

### §14.11.4–14.11.6

```
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
   0.153 us   |      get_nohz_timer_target();
   [...]
```

KernelShark 是 trace-cmd 输出文件的**可视化 GUI**（Steven Rostedt 创作，现由 Yordan Karadzhov 用 Qt 重写）：

```
# trace-cmd record -e 'sched:*'
# kernelshark
```

上半屏是**每 CPU 的时间线**（不同任务不同颜色），下半屏是事件表。可以拖拽缩放、右键设过滤。**用它来找「不同线程交互造成的性能问题」。**

### §14.12 perf ftrace

perf(1) 也有个 `ftrace` 子命令，能访问 function 与 function_graph tracer：

```
# perf ftrace -T do_nanosleep -a sleep 10
 0)  sleep-22821   |               |  do_nanosleep() {
 1)  multipa-348   |               |  do_nanosleep() {
 1)  multipa-348   | $ 1000068 us  |  }
 1)  multipa-348   |               |  do_nanosleep() {
 1)  multipa-348   | $ 1000068 us  |  }
[...]
```

```
# perf ftrace -G do_nanosleep -a sleep 10
 1)  sleep-22828   |               |  do_nanosleep() {
 1)  sleep-22828   |   ==========> |
 1)  sleep-22828   |               |    smp_irq_work_interrupt() {
 1)  sleep-22828   |               |      irq_enter() {
 1)  sleep-22828   |   0.258 us    |        rcu_irq_enter();
 1)  sleep-22828   |   0.800 us    |      }
 1)  sleep-22828   |               |    __wake_up() {
 1)  sleep-22828   |               |      __wake_up_common_lock() {
 1)  sleep-22828   |   0.491 us    |          _raw_spin_lock_irqsave();
[...]
```

⚠️ 原书提醒：这是个**薄包装**，不与 perf 的其他能力集成——比如它把输出打到 stdout，**不使用 `perf.data`**。

---

## 11. §14.13 perf-tools：现成的饭

perf-tools 是作者自己开发的一套基于 Ftrace 与 perf(1) 的进阶分析工具集，**Netflix 的服务器上默认安装**。它大多是 **shell 脚本**，作用是「自动设置 tracefs 的 `/sys` 文件」。

```
# execsnoop
Tracing exec()s. Ctrl-C to end.
   PID   PPID ARGS
  6684   6682 cat -v trace_pipe
  6683   6679 gawk -v o=1 -v opt_name=0 -v name= -v opt_duration=0 [...]
  6685  20997 man ls
  6695   6685 pager
  6691   6685 preconv -e UTF-8
  6692   6685 tbl
  6693   6685 nroff -mandoc -rLL=148n -rLT=148n -Tutf8
  6698   6693 locale charmap
  6699   6693 groff -mtty-char -Tutf8 -mandoc -rLL=148n -rLT=148n
  6700   6699 troff -mtty-char -Tutf8 -mandoc -rLL=148n -rLT=148n
  6701   6699 grotty
[...]
```

（注意第一行：`execsnoop` 自己启动的 `cat -v trace_pipe` 和 `gawk` 也会出现在输出里——**观测工具自己也是被观测对象**。）

**表 14.7 里的单用途工具**：`bitesize`、`cachestat`、`execsnoop`、`iolatency`、`iosnoop`、`killsnoop`、`opensnoop`、`tcpretrans`。

```
# iolatency
Tracing block I/O. Output every 1 seconds. Ctrl-C to end.

  >=(ms) .. <(ms)   : I/O      |Distribution                          |
       0 -> 1       : 731      |######################################|
       1 -> 2       : 318      |#################                     |
       2 -> 4       : 160      |#########                             |
```

**表 14.8 里的多用途工具**：`funccount`、`funcgraph`、`functrace`、`funcslower`、`kprobe`、`perf-stat-hist`、`syscount`、`tpoint`、`uprobe`。

### §14.13.4 perf-tools One-Liners

```
funccount 'tcp_*'
funccount -t 10 -i 1 'vfs*'
funcgraph do_nanosleep
funcgraph -m 3 do_nanosleep
functrace -p 198 '*sleep'
funcslower vfs_read 10000
kprobe p:do_sys_open
kprobe 'r:do_sys_open $retval'
kprobe 'p:do_sys_open mode=$arg3:u16'
kprobe 'p:do_sys_open mode=%dx:u16'
kprobe 'p:do_sys_open filename=+0($arg2):string'
kprobe 'p:do_sys_open filename=+0(%si):string'
kprobe 'p:do_sys_open file=+0($arg2):string' 'file ~ "*stat"'
kprobe -s p:tcp_retransmit_skb
tpoint -l
tpoint -s block:block_rq_issue
uprobe p:bash:readline
uprobe 'r:bash:readline +0($retval):string'
uprobe 'p:/bin/bash:readline prompt=+0(%di):string'
uprobe -p 1234 p:libc:gettimeofday
uprobe 'r:libc:fopen file=$retval' 'file == 0'
```

### §14.13.5 Example

```
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
vfs_open                           1003
vfs_getattr                        1390
vfs_getattr_nosec                  1390
vfs_read                           2604
```

> Since its overhead is relatively low, I can use it to check whether function call rates are low enough for more expensive tracing.

**这就是 §14.3 那条方法论的现成版本：`funccount` 便宜，用它确认某个函数的调用频率够低，再上贵的 tracer。**

### §14.13.6 perf-tools vs. BCC/BPF

原书列了 perf-tools 相对 BPF 的四个优势：`funccount` 用的 Ftrace function profiling 比 BCC 的 kprobe 版**更高效、限制更少**；`funcgraph` 在 BCC 里**根本不存在**（它依赖 Ftrace 的 function_graph）；hist trigger 未来会 powering 更高效的 perf-tools；**依赖少**（通常只要一个 shell 和 awk），适合资源受限的嵌入式环境。

> A man with one tracer knows what events happened; a man with two tracers knows that one of them is broken, and searches lkml hoping for a patch.

（作者说这是他改写的一句名言。）

---

## 12. 实战锚点：otf-server 怎么面对这件事

### 12.1 先说缺口：这个仓库里**一条 tracing 资产都没有**

在仓库根执行：

```bash
grep -rniE 'trace-cmd|ftrace|bpftrace|\bperf\b' --exclude-dir=build .
```

**0 命中。** 没有脚本、没有文档、`dev.sh` 里也没有。`Dockerfile` 只装了 `g++-12 / ninja-build / cmake / git`，连 `trace-cmd` 都没有。

再看清一件容易混的事：`src/net/metrics.cpp` 里那一串

```
"otf_connections %lld\n"
"otf_rooms %lld\n"
"otf_frames_sent %lld\n"
...
"otf_epoll_ctl_add %lld\n"
"otf_epoll_ctl_del %lld\n"
"otf_epoll_ctl_mod %lld\n"
"otf_epoll_ctl_add_with_out %lld\n";
```

是**应用层自建指标**，Prometheus 文本格式（`metrics_server.cpp` 里 `Content-Type: text/plain; version=0.0.4; charset=utf-8`），内容是**进程自己数的**。它和 ftrace 观测的是**两个不同的世界**：

| | `otf_*` 指标 | ftrace |
|---|---|---|
| 视角 | 进程自己的视角（我数了多少次） | 内核的视角（内核真的收到了吗） |
| 载体 | 用户态 `Metrics` 类 + HTTP 端点 | tracefs 文件系统 |
| 部署前提 | 无（进程内就有） | 需要 tracefs 挂载 + 权限 + 内核开 `CONFIG_FTRACE` |

第 11 章 §11.2.4.2 的结论正好可以用在这里：**硬件虚拟化的 guest 里这些内核 tracing 工具能用**（guest 跑的是自己的内核，root 有完整内核访问权），但**容器里可能就不行了**（"with containers these kernel tracing tools may not work"）。本仓库的 `Dockerfile` 就是 `ubuntu:22.04` 且**没挂 tracefs**——所以在容器里跑，`/sys/kernel/debug/tracing` 大概率是空的。这不是疏漏，是**架构上必须先想清楚的事**：观测手段要部署在哪一层，得在写脚本之前决定。

### 12.2 最有价值的锚点：`epoll_ctl` 计数器说谎的那一半

`src/net/tcp_transport.h:31-52` 的 `EpollCtlCounters` 头注释，把这个仓库的观测边界写得很清楚。原话：

> ★ 为什么需要它：`epoll_ctl` **没有**「查询已注册事件」的 API

> ① **syscall 失败也计入**。`Ctl` 先记账再调内核，所以计数器全绿**不代表**内核真的收到了那个注册值（变异 M16 验证过这一点：计数器可以全绿而内核压根没收到 MOD）。

> ② 因此**时序判据抓不到它**。M2c（永远挂 EPOLLOUT）下，`Poll(60)` 实测仍是 60ms——因为排空之后 socket 真的不可写，LT 就不报 EPOLLOUT。**抓 M2c 的只有本计数器，没有第二条防线。**

这是全仓库最诚实的自我批评：**用「我们打算注册成什么样」代替了「内核实际收到了什么」**。`Ctl()` 里那句注释更是把这条纪律焊死了：

> 先记账再调：即使内核调用失败，这次「我们打算注册成什么样」也已被记录，否则统计会在出错路径上漏记，而漏记正好会掩盖 bug。

**ftrace 能补上这半句。** `epoll_ctl` 有 syscall tracepoint，成对采集就能拿到**内核实际收到的注册值**和**syscall 的返回值**（`EINVAL` / `ENOENT` / `ENOSPC`）。这一对事件的名字在原书里没有直接出现（原书只给了 `syscalls:sys_enter_nanosleep` 一个例子），但命名规律与查询方法是原书给全的：

```
trace-cmd list -e syscalls:
trace-cmd list -e syscalls:sys_enter_nanosleep -F
```

**所以正确的做法不是照抄一个名字，而是先 `list` 再用。**（延伸用法：把原书 `syscalls:sys_enter_nanosleep` 的那个 `nanosleep` 换成 `epoll_ctl` / `read` / `write` / `accept` / `epoll_wait`，是同一套命名规律；具体可用的事件名以 `trace-cmd list -e syscalls:` 的实际输出为准。）

> **本讲义最值钱的一句话**：这个仓库最缺的不是指标，是**一条能说出「内核到底收到了什么」的独立证据链**。自建计数器是「自证」，ftrace 是「他证」。原书在 §14.13.6 借了一句名言说的正是这件事——**两个 tracer 互相印证，才知道其中一个是不是坏了。**

### 12.3 能落地的四组观测

**（1）每 tick 醒几次、`epoll_wait` 睡了多久。**

这是开局那个「8 秒」问题的正面回答。原文 §14.10.7 的合成事件原样可用——它算的正是**每一次 syscall 从进入到退出的延迟**：

```
# cd /sys/kernel/debug/tracing
# echo 'syscall_latency u64 lat_us; long id' >> synthetic_events
# echo 'hist:keys=common_pid:ts0=common_timestamp.usecs' >> \
    events/raw_syscalls/sys_enter/trigger
# echo 'hist:keys=common_pid:lat_us=common_timestamp.usecs-$ts0:'\
    'onmatch(raw_syscalls.sys_enter).trace(syscall_latency,$lat_us,id)' >>\
    events/raw_syscalls/sys_exit/trigger
# echo 'hist:keys=lat_us,id.syscall:sort=lat_us' >> \
    events/synthetic/syscall_latency/trigger
```

配上 §14.10.2 的 syscall ID 分布（`hist:key=id.syscall`），就能一眼看出：**每 tick 有几次 `epoll_wait`、每次睡多久、最长的一次是不是真的睡过了 50 ms**（仓库的超时上限 `config::kMaxPollTimeoutMs = 50`）。超时上限被突破 = 有一次 `epoll_wait` 没按 timeout 返回，那是**内核侧的问题**，不是仓库的逻辑问题。

**（2）每连接每轮读 / 写几次。**

```
trace-cmd record -e syscalls -F otf_server sleep 10
trace-cmd report
```

（原文给的是 `trace-cmd record -e syscalls -F ls`，同一形状。）数 `sys_enter_read` / `sys_enter_write` 的条数：

- `TcpTransport::OnReadable` 循环 `::read(fd, buf, sizeof(buf))` 直到 EAGAIN，**每轮可能不止一次**；
- `TcpTransport::FlushWrite` 循环 `::write(fd, c.out.data() + c.out_sent, c.out.size() - c.out_sent)` 直到写完或 EAGAIN，**同样不止一次**；
- `TcpTransport::AcceptNew` 循环 `::accept(listen_fd_, nullptr, nullptr)` 直到 EAGAIN。

「一轮 Poll 里发生了几百次 read」不一定是 bug（LT 模式下确实可能），但**它是你判断「这轮 IO 有多碎」的第一手数字**，也是决定要不要做零拷贝/批量读的依据。

**（3）内核侧的调用路径。**

```
trace-cmd record -p function -l 'epoll*' sleep 10
trace-cmd record -p function_graph -g do_syscall_64 sleep 10
```

（原文给的是 `-p function -l 'tcp_*'` / `-l 'vfs_*'`，以及 `-p function_graph -g do_nanosleep`——同一形状，换过滤串。）`vfs_read` / `vfs_write` 正是仓库的 `::read` / `::write` 在内核里落到的地方；函数图能看出 `epoll_wait` 内部到底做了什么。

⚠️ **务必按 §14.3 的纪律来**：function tracer 只适合**每秒不到 1000 次**的函数。`epoll_wait` 每秒 20 次（20 Hz tick），完全在安全区；`vfs_read` / `vfs_write` 的频率取决于连接数与消息量——**先用 `funccount` 之类的便宜工具数一遍**，确认频率，再决定要不要逐条 trace。

**（4）调度视角。**

```
trace-cmd record -e 'sched:*'
```

（原文在 §14.11.5 KernelShark 一节给的就是这一行。）`sched_switch` / `sched_wakeup` 告诉你：**主循环是被谁抢了 CPU、抢了多久**。单线程 reactor 的服务器，理论上「谁也不该抢」——如果 `sched_switch` 显示有别的进程在抢，那就是部署环境的干扰（而不是仓库的问题）。

### 12.4 一条必须纠正的误解：`set_ftrace_filter` 追不了 `TcpTransport::*`

很多教程会顺手说「用 function tracer + `set_ftrace_filter` 过滤到你自己的函数」。**在本仓库这是错的**，而且原书写得很清楚：

- 表 14.2：`function` = **Kernel function call tracer**（追的是内核函数）；
- 表 14.2：`uprobes` = **User-level dynamic instrumentation**（追用户态函数）；
- §14.6：kprobes「are similar to the Ftrace function tracer ... in that they trace kernel functions」。

`TcpTransport::OnReadable` / `FlushWrite` / `Ctl` 是**用户态 C++ 函数**，`set_ftrace_filter` 里根本不会有它们（`available_filter_functions` 列的是内核符号）。要追用户态函数只有两条路：

1. **uprobes**（§14.7）——但原书在这里放了 WARNING：偏移算错会**破坏目标进程**，PIE + ASLR 下 `readelf` 找偏移那招可能不work，而且原书明确说「I do not recommend that you use this interface at all」，应该换成会处理符号映射的高层 tracer（BCC / bpftrace）。**BCC 与 bpftrace 是第 15 章的内容。**
2. **BPF**（第 15 章）。

（顺带一提：本仓库的 `build/dev/otf_server` 是 `with debug_info, not stripped`，符号在——但「符号在」不等于「原书推荐的 uprobe_events 用法可用」。）

### 12.5 开销纪律（全部来自原文）

按本章的原文结论，落到本仓库的操作纪律是：

1. **先便宜后昂贵**——`funccount` / function profiler 确认频率，再上 function tracer（§14.3、§14.13.5）。
2. **高频事件别走用户态解析**——`iolatency` 那套「用户态读全部事件 + awk 后处理」在磁盘 I/O 上还能忍，在**网络 I/O** 上开销不可接受（§14.13.2 原文点名）。本仓库的高频路径就是网络 I/O ⇒ 要么用 hist trigger / synthetic event 把统计放进内核，要么用 BPF。
3. **高频就落盘，别在线打印**——`trace_pipe` 是给低频实时看的；高频用 `trace` 落文件（§14.4.2），或者直接用 `trace-cmd record`（输出文件开销 "Very low"，表 14.6）。
4. **别忘了关**——`echo nop > current_tracer`、`echo > set_ftrace_filter`、删掉 `kprobe_events` / `synthetic_events` 的条目。原书每个例子的收尾都在做这件事。

### 12.6 这个仓库还缺什么、缺了会怎么错

| 缺口 | 现状 | 缺了会怎么错 |
|---|---|---|
| **无 tracing 资产** | 全仓 0 命中 | 想看内核发生了什么的人只能 `strace`（看不到边界以内）或加日志（改动被观测的进程） |
| **无环形发送缓冲** | `Conn::out` 是 `std::vector`，`Send` 里 `resize + memcpy` | 峰值字节数决定分配次数；`kSendBufCap = 128 * 1024` 的背压上限是硬断开（"发送队列超上限，断开该连接"） |
| **无对象池** | 每条连接一个 `Conn`，每帧一次 `std::vector` 扩容 | 稳态下 `resize` 保留 capacity、零分配，但**首次**扩容仍是一次分配 |
| **单线程事件循环，IO 与逻辑未拆** | `Poll` → `OnReadable` → 回调 → 直接改状态 | IO 抖动与逻辑耗时**无法分离**测量——`epoll_wait` 的等待时长和 tick 的执行时长混在同一条时间线上 |
| **`otf_*` 是自建指标** | 应用层数自己 | 「我们打算注册成什么样」≠「内核收到了什么」——见 §12.2 |

**最小落地清单**（如果只做三件事）：

1. 在仓库里加一个 `tools/trace/` 目录，放**两个**脚本：`collect.sh`（`trace-cmd record -e syscalls -F otf_server` 落盘）和 `analyze.sh`（`trace-cmd report` + 按 syscall ID 计数）。**先用 `hist:key=id.syscall` 把 syscall 分布拿出来**（§14.10.2 的形状），这是最省事也最有信息量的一步。
2. 在 `dev.sh` 里加一个 `trace` 子命令，**先把 tracefs 挂上再跑**——原书那条「tracefs 挂不上可能是内核没开 `CONFIG_FTRACE`」的提醒，在这里就是一条前置检查。
3. 明确写下**观测发生在哪一层**：容器内还是宿主机。第 11 章 §11.2.4.2 说容器里内核 tracing 工具可能用不了；本仓库的 `Dockerfile` 是容器，**那么答案很可能是「在宿主机上观测，容器里被观测」**——这个决定不做，脚本写了也跑不起来。

---

## 13. 收尾：本章金句清单

1. **Ftrace is the official Linux tracer, a multi-tool composed of different tracing utilities.** —— 一个工具箱，而不是一个工具；这一章讲的就是怎么把这箱工具按顺序用起来。
2. **Profilers provide statistical summaries such as counts and histograms, and tracers provide per-event details.** —— 统计与明细的分界，全章的「先便宜后昂贵」纪律就建立在这句话上。
3. **The overhead of function tracing is higher than function profiling, and so tracing is best suited for relatively infrequent functions (less than 1,000 calls per second).** —— 逐事件追踪的成本写在书上：每秒 1000 次是分水岭。
4. **You can use function profiling from the previous section to find out the rate of functions before tracing them.** —— 先测频率再上 tracer，这是本章唯一真正需要背下来的工作流。
5. **when not in use, these calls are typically replaced with fast nop instructions, and only switched to the `__fentry__()` calls when needed** —— 「不用的插桩是 nop」是「零开销设计」在内核里的标准答案。
6. **A small amount of overhead is added to the profiled functions during profiling.** —— 便宜不等于免费；过滤器该用就用。
7. **The trace_pipe file is handy for watching low-frequency events, but for high-frequency events you will want to capture them to a file for later analysis using the trace file shown earlier.** —— 实时看 vs 落盘后看，选错的代价是「把被观测系统拖垮」。
8. **It also consumes events, so after reading them once they are no longer in the trace buffer.** —— `trace_pipe` 是「读走即消费」；想留证据就别用它。
9. **kprobes are kernel dynamic instrumentation** —— 内核里的探针可以自己下；能读参数（`$arg1`）和返回值（`$retval`）是它相对 function tracer 的全部优势。
10. **I do not recommend that you use this interface at all: switch to a higher-level tracer that takes care of symbol mappings for you (e.g., BCC or bpftrace).** —— 原书在 uprobe 前面直接泼冷水；知道边界在哪，比会用更重要。
11. **It is still generally useful for locating the origin of high latencies, which can dwarf the added overhead.** —— 定位慢的源头，测量误差可以忽略；测量精确的耗时，就必须缩小追踪范围。
12. **In a way, we’re starting to write mini programs.** —— hist trigger + synthetic event 的本质：在 tracefs 里写小程序，让内核替你算。
13. **It’s incredibly powerful, built into the kernel, and can be used via shell scripting alone.** —— 内核里的能力，shell 就能用——这正是 ftrace 在嵌入式与生产环境里的护城河。
14. **It is also safer: many subcommands handle cleaning up tracing state when done.** —— 手写 `echo` 容易忘记收尾；`trace-cmd` 会替你把状态清干净。
15. **The perf-tools themselves are mostly implemented as shell scripts that automate setting the tracefs /sys files.** —— 「用 shell 脚本自动化 tracefs」这个模式，正是本仓库 `tools/` 目录最该抄的形态。
16. **Extended BPF solved this problem by allowing the histogram summary to be calculated in kernel space, and only the summary is passed to user space, greatly reducing overhead.** —— 统计必须在内核里做完——这一句是本仓库选型（ftrace / hist / BPF）时最该拿出来讲的理由。
17. **A man with one tracer knows what events happened; a man with two tracers knows that one of them is broken, and searches lkml hoping for a patch.** —— 自建计数器是自证；ftrace 是他证。两个 tracer 互相印证，才知道其中一个是不是坏了。
