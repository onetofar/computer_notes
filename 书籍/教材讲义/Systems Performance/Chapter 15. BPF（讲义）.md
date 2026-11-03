---
title: Chapter 15. BPF（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - BPF
  - bpftrace
  - BCC
  - 内核可观测
  - 性能工程
source: "《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg，Addison-Wesley 2020，英文原版）第 15 章"
companion: "[[15-Chapter 15. BPF]]"
---

# 第 15 章 讲义：BPF 让你在内核里安全地挂钩子

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[15-Chapter 15. BPF|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：`/home/xier/dev/game-project/otf-server/` 的 epoll reactor（`src/net/tcp_transport.cpp` 的 `TcpTransport::Poll` / `Ctl`）——本章给这一处装一把**内核侧**的尺子。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原章。

---

## 0. 开局：37 毫秒的漂移，从哪儿来的？

你盯着 `otf-server` 的 Prometheus 端点，看见 `otf_tick_drift_max_ms` 冒出一个 **37**。

你手上的指标能告诉你什么？`otf_connections`（当前连接数）、`otf_frames_sent` / `otf_frames_recv`（累计收发帧）、`otf_epoll_ctl_add` / `_del` / `_mod`（累计的 `epoll_ctl` 次数）、`otf_tick_drift_max_ms`（逻辑帧落后墙钟的峰值）。

它们全是**结果**。而 37 毫秒是**过程**造成的：这个进程某一拍被内核抢走了 CPU？还是 `epoll_wait` 一口气返回太多连接、这一拍处理不完？还是某个 socket 的写缓冲一直不 ready、拖住整轮？**结果**不回答「慢在哪」，只回答「慢了」。

要看过程就得进内核，而内核不是你的代码，你没法往里面加 `printf`。`strace` 把每个系统调用拽到用户态（生产上不能用）、`/proc` 采样只看快照、`perf` 看 CPU 不看语义——三条老路都堵着。

BPF 给的是第四条：**让你在内核里执行你自己写的程序**。不重启内核、不装 agent、不改一行 C——写几行 bpftrace 挂上去，内核在事件发生的当场把答案算好。

（推演）可以把 BPF 想成给内核装了一副**能现场写规则的显微镜**：规则不是焊死在镜片里的，是你按下快门那一刻才写上去的。

这一章讲这副显微镜的两套操作台：**BCC**（一整套现成工具，像 C）与 **bpftrace**（一门小语言，像 shell/awk）。

---

## 1. §15.1–15.1.8 BCC：一整套「开箱即用」的性能分析工具

### 1.1 为什么 BPF 值得单开一章

> What makes BPF different from other tracers is that it is programmable. It allows user-defined programs to be executed on events, programs that can perform filtering, save and retrieve information, calculate latency, perform in-kernel aggregation and custom summaries, and more.

关键在最后的**聚合**二字。别的追踪器通常要把**所有**事件倒到用户态再后处理；BPF 让这件事**在内核上下文里就地完成**：

> While other tracers may require dumping all events to user space and post-processing them, BPF allows such processing to occur efficiently in kernel context. This makes it practical to create performance tools that would otherwise cost too much overhead for production use.

于是「一百万次事件流」变成「一张算好的直方图」，用户态只负责读那张图。**这就是 BPF 能进生产的全部理由**——不是它更快，是它让「重」变成「轻」。

作者用 `biolatency(8)` 演示（照抄）：

```
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
         4 -> 7          : 8       |*************                           |

flags = ReadAhead-Read
     msecs               : count     distribution
         0 -> 1          : 3031     |****************************************|
         2 -> 3          : 10       |                                        |
         4 -> 7          : 5        |                                        |
         8 -> 15         : 3        |                                        |
```

> This tool uses BPF to summarize the histograms in kernel space for efficiency, so the user-space component only needs to read the already-summarized histograms (the count columns) and print them.

注意：**内核里已经按 flags 分好组、算好桶计数了**；用户态只把数字画成 ASCII 条。

### 1.2 两种工具形态

| 形态 | 代表 | 设计哲学 |
|---|---|---|
| **单用途** | `biolatency` / `biotop` / `runqlat` / `tcplife` / `execsnoop` … | 「do one job and do it well」：默认输出简洁到「直接跑就够用」，选项只是定制 |
| **多用途** | `argdist` / `funccount` / `funcslower` / `funclatency` / `stackcount` / `trace` | 像 `perf(1)`：支持多个事件源，灵活但难用 |

单用途工具的设计理念原话是：

> I developed many of these according to the same “do one job and do it well” philosophy as those in perf-tools in Chapter 14 . This design includes making their default output concise and often just sufficient. You can “just run biolatency” without needing to learn any command line options, and usually get just enough output to solve your problem without clutter.

完整清单在原书表 15.2（约 40 个）与表 15.3。对本章实战锚点最相关的是 **`runqlat(8)`**（`Summarize run queue (scheduler) latency as a histogram`）。

### 1.3 one-liners：把知识压进命令行

BCC 的多用途工具能现场拼出一行命令。原文给的（全部照抄）：

- 数 VFS 内核调用：`funcgraph 'vfs_*'`
- 数 TCP 内核调用：`funccount 'tcp_*'`
- 每秒 TCP 发送次数：`funccount -i 1 'tcp_send*'`
- 每秒块设备 I/O 速率：`funccount -i 1 't:block:*'`
- 每秒 libc `getaddrinfo()` 速率：`funccount -i 1 c:getaddrinfo`
- 数导致块 I/O 的调用栈：`stackcount t:block:block_rq_insert`
- 数导致线程阻塞的调用栈：`stackcount t:sched:sched_switch`
- 跟踪 `do_sys_open()` 并打印文件名：`trace 'do_sys_open "%s", arg2'`
- 跟踪 `do_sys_open()` 的返回值：`trace 'r::do_sys_open "ret: %d", retval'`
- 跟踪 `do_nanosleep()` 及用户态栈：`trace -U 'do_nanosleep "mode: %d", arg2'`
- 按返回值统计 VFS 读：`argdist -H 'r::vfs_read()'`
- 按 syscall ID 计数：`argdist.py -C 't:raw_syscalls:sys_enter():int:args->id'`
- 按 fd 计数某进程的 libc `write()`：`argdist -p 181 -C 'p:c:write(int fd):int:fd'`

`trace(8)` 的输出长这样：

```
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

`trace` 的语法**受 `printf(3)` 启发**——格式串 + 参数；`argdist` 的语法是「探针 + 返回类型 + 参数」。

### 1.4 BCC vs bpftrace：C 与 shell 的差别

原书表 15.1 里最刺眼的两个数字：工具数 `>80 (bcc)` vs `>30 (bpftrace)`，**平均程序长度 228 行 vs 28 行**。作者自己的类比：

> The difference between BCC and bpftrace is like the difference between C programming and shell scripting, where BCC is like C programming (some of it is C programming) and bpftrace is like shell scripting.

> BCC tools can take ten times as long to develop as bpftrace tools, and can have ten times as many lines of code. Since developing a tool typically requires multiple iterations, I’ve found that it saves time to first develop tools in bpftrace, which is quicker, and then port them to BCC if needed.

**实践建议**：先在 bpftrace 里试，验证思路了再搬进 BCC。

> 【告示】Netflix 在所有云实例上默认装了这两个工具，分工很清楚：BCC 的罐头工具做存储/网络 I/O 的大盘分析（还常驻一个基于 `tcplife(8)` 的守护进程，把网络事件写进云存储做流分析），bpftrace 则在需要深挖内核与应用病理时才现场写。

---

## 2. §15.2–15.2.3 bpftrace：把内核变成 awk 一样好写的地方

### 2.1 它到底是什么

> bpftrace is an open-source tracer built upon BPF and BCC, which provides not only a suite of performance analysis tools, but also a high-level language to help you develop new ones. The language has been designed to be simple and easy to learn. It is the awk(1) of tracing, and is based on awk(1). In awk(1), you write a program stanza to process an input line, and with bpftrace you write a program stanza to process an input event.

一句话：**awk 处理输入行，bpftrace 处理输入事件**。

最小示例（照抄原书）：

```
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

这一行塞了四件事：`kr:` 是 kretprobe（挂 `tcp_recvmsg` 的**返回**）、`/retval >= 0/` 过滤掉负的错误码、`@recv_bytes[comm]` 是**以进程名为键的 map**、`hist(retval)` 是 2 的幂次直方图。按下 Ctrl-C，bpftrace 收到 SIGINT 后自动把 map 打印出来。

### 2.2 安装

`bpftrace` 在 Ubuntu / Fedora / Gentoo / Debian / OpenSUSE / CentOS 上都有包，RHEL 8.2 里是 Technology Preview；此外还有 Docker 镜像、只依赖 glibc 的独立二进制、源码构建。内核配置要求与 BCC 相同（`INSTALL.md` 所列）：`CONFIG_BPF=y`、`CONFIG_BPF_SYSCALL=y`、`CONFIG_BPF_EVENTS=y`。

> bpftrace requires Linux 4.9 or newer.

### 2.3 one-liners

现成工具在仓库（图 15.3），作者另在 `bpf-perf-tools-book` 放了一批。one-liners 按目标分七组（CPUs / Memory / File Systems / Disk / Networking / Applications / Kernel），原文给的命令全部照抄如下：

```
bpftrace -e 'tracepoint:syscalls:sys_enter_execve { join(args->argv); }'
bpftrace -e 'tracepoint:raw_syscalls:sys_enter { @[pid, comm] = count(); }'
bpftrace -e 'profile:hz:49 /pid == 189/ { @[ustack] = count(); }'
bpftrace -e tracepoint:syscalls:sys_enter_brk { @[ustack, comm] = count(); }
bpftrace -e 'tracepoint:exceptions:page_fault_user { @[ustack, comm] =
    count(); }'
bpftrace -e 'tracepoint:vmscan:* { @[probe]++; }'
bpftrace -e 't:syscalls:sys_enter_openat { printf("%s %s\n", comm,
    str(args->filename)); }'
bpftrace -e 'tracepoint:syscalls:sys_exit_read { @ = hist(args->ret); }'
bpftrace -e 'kprobe:vfs_* { @[probe] = count(); }'
bpftrace -e 'tracepoint:ext4:* { @[probe] = count(); }'
bpftrace -e 't:block:block_rq_issue { @bytes = hist(args->bytes); }'
bpftrace -e 't:block:block_rq_issue { @[ustack] = count(); }'
bpftrace -e 't:block:block_rq_issue { @[args->rwbs] = count(); }'
bpftrace -e 't:syscalls:sys_enter_accept* { @[pid, comm] = count(); }'
bpftrace -e 'kr:sock_sendmsg,kr:sock_recvmsg /retval > 0/ {
    @[pid, comm] = sum(retval); }'
bpftrace -e 'k:tcp_sendmsg { @send_bytes = hist(arg2); }'
bpftrace -e 'kr:tcp_recvmsg /retval >= 0/ { @recv_bytes = hist(retval); }'
bpftrace -e 'k:udp_sendmsg { @send_bytes = hist(arg2); }'
bpftrace -e 'u:/lib/x86_64-linux-gnu/libc-2.27.so:malloc { @[ustack(5)] =
    sum(arg0); }'
bpftrace -e 't:syscalls:sys_enter_kill { printf("%s -> PID %d SIG %d\n",
    comm, args->pid, args->sig); }'
bpftrace -e 'tracepoint:raw_syscalls:sys_enter {
    @[ksym(*(kaddr("sys_call_table") + args->id * 8))] = count(); }'
bpftrace -e 'kprobe:attach* { @[probe] = count(); }'
bpftrace -e 'kprobe:vfs_write { @[arg2] = count(); }'
bpftrace -e 'k:vfs_read { @ts[tid] = nsecs; } kr:vfs_read /@ts[tid]/ {
    @ = hist(nsecs - @ts[tid]); delete(@ts[tid]); }'
bpftrace -e 't:sched:sched_switch { @[kstack, ustack, comm] = count(); }'
bpftrace -e 'profile:hz:99 /pid/ { @[kstack] = count(); }'
```

> 【告示】`bpftrace -e tracepoint:syscalls:sys_enter_brk { … }` 这一行**原文就没有加引号**（照抄如此，不替它修）。

---

## 3. §15.2.4 bpftrace 编程：从一行到一段

### 3.1 一个完整的程序

原文的入门程序 `vfsread.bt`（照抄）：

```
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

输出：

```
# bpftrace vfsread.bt
Attaching 2 probes...
^C

@us:
[0]                   23 |@                                                   |
[1]                  138 |@@@@@@@                                           |
[2, 4)               538 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@               |
[4, 8)               744 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[8, 16)              641 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@        |
[16, 32)             122 |@@@@@@@@@                                            |
[32, 64)              13 |                                                    |
[64, 128)             17 |@                                                   |
[128, 256)             2 |                                                    |
[256, 512)             0 |                                                    |
[512, 1K)              1 |                                                    |
```

三点值得抄下来：**① map 名字带单位**——`@us` 这个名字会印在输出里，叫 `@us` / `@bytes` / `@latency_ns` 就自带单位；**② 过滤 `/@start[tid]/` 不是可选的**——没有它，追踪开始时已在进行中的 `vfs_read()` 会「只看到结束、看不到开始」，delta 变成 `now - 0`，直方图被彻底污染；**③ 一定要 `delete(@start[tid])`**——不删，map 会随线程数膨胀。

想按进程拆开？改一行：

```
@us[pid, comm] = hist($duration_us);
```

> With traditional system tools, like iostat(1) and vmstat(1), the output is fixed and cannot be easily customized. But with bpftrace, the metrics you see can be further broken down into parts and enhanced with metrics from other probes until you have the answers you need.

### 3.2 骨架：探针 / 过滤 / 动作

```
probes { actions }
probes /filter/ { actions }
...
```

这正是 awk 的 `/pattern/ { actions }`；多个动作块**可任意顺序执行**，只要各自的探针+过滤命中。

**探针格式**（照抄）：

```
type:identifier1[:identifier2[...]]
```

```
kprobe:vfs_read
uprobe:/bin/bash:readline
```

`kprobe` 只需内核函数名；`uprobe` 需要**二进制路径 + 函数名**两者。多个探针用逗号共用动作块：`probe1,probe2,... { actions }`。`BEGIN` / `END` 是两个不需要额外标识符的特殊探针：

```
BEGIN { printf("Tracing. Hit Ctrl-C to end.\n"); }
```

### 3.3 通配符、注释、Hello World

```
kprobe:vfs_*
```

会挂上所有 `vfs_` 开头的内核函数——但：

> Instrumenting too many probes may cost unnecessary performance overhead.

bpftrace 有 `BPFTRACE_MAX_PROBES` 环境变量兜底（当前默认 512）；作者还提醒超过 512 个会让 bpftrace 启停变慢，因为它一个一个挂。**先预检，再上**：

```
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

> You can test your wildcards before using them by running bpftrace -l to list the matched probes

（原文提醒：探针名要加引号，防止 shell 意外展开。）注释用 `//` 单行、`/* … */` 多行（与 C 一样）。

```
# bpftrace -e 'BEGIN { printf("Hello, World!\n"); }'
Attaching 1 probe...
Hello, World!
^C
```

### 3.4 过滤器与动作

过滤器是布尔表达式，**不给条件时等于「非零」**：`/pid == 123/`、`/pid/`（等价于 `/pid != 0/`）、`/pid > 100 && pid < 1000/`。动作是一条或多条以分号分隔的语句：

```
{ action one; action two; action three }
```
```
{ $x = 42; printf("$x is %d", $x); }
```

### 3.5 三种变量

| 类型 | 前缀 | 生命周期 | 例子 |
|---|---|---|---|
| 内建 built-in | 无 | 只读 | `pid` / `tid` / `comm` / `nsecs` / `curtask` |
| 临时 scratch | `$` | **只在本动作块内** | `$x = 1;` |
| map | `@` | 全局，跨探针 | `@start[tid] = nsecs;` |

```
$x = 1;
$y = “hello”;
$z = (struct task_struct *)curtask;
```

（`“hello”` 用了中文引号，照抄原书排版。）

map 可带键当哈希表用：`probe1 { @a = 1; }` / `probe2 { $x = @a; }`；多键 map 的例子是 `@path[pid, $fd] = str(arg0);`。

> 【原文提醒】`@start[tid] = nsecs;` 这种写法很常见：把时间戳按**线程 ID** 存进 `@start`，线程之间不会互相覆盖。

### 3.6 map 函数

```
@x = count();
```
```
@x++;
```
```
@y = sum($x);
```
```
@z = hist($x);
```
```
print(@x);
```
```
delete(@start[tid]);
```

细节很重要：`@x = count()` 用的是**每 CPU map**（per-CPU map）；`@x++` 用的是**全局 CPU map**、提供整数类型——原文提醒后者**可能有小的误差**，因为并发更新。

### 3.7 运行方式

`bpftrace -e program` 或 `bpftrace file.bt`。`.bt` 后缀不是必须，但便于识别；文件头写解释器行就能当程序跑：

```
#!/usr/local/bin/bpftrace
```

```
./file.bt
```

> bpftrace must be executed by the root user (super-user).

---

## 4. §15.2.5–15.2.6 参考速查与文档去处

### 4.1 探针类型（表 15.4）

| 类型 | 简写 | 说明 |
|---|---|---|
| tracepoint | `t` | 内核静态埋点 |
| usdt | `U` | 用户态静态定义埋点 |
| kprobe | `k` | 内核动态函数插桩 |
| kretprobe | `kr` | 内核动态函数返回插桩 |
| kfunc | `f` | 内核动态函数插桩（基于 BPF） |
| kretfunc | `fr` | 内核动态函数返回插桩（基于 BPF） |
| uprobe | `u` | 用户态动态函数插桩 |
| uretprobe | `ur` | 用户态动态函数返回插桩 |
| software | `s` | 内核软件事件 |
| hardware | `h` | 硬件计数器事件 |
| watchpoint | `w` | 内存 watchpoint |
| profile | `p` | 定时采样（全 CPU） |
| interval | `i` | 定时上报（单 CPU） |
| BEGIN / END | | 程序开始 / 结束 |

`kfunc`/`kretfunc` 是基于 eBPF trampoline 与 BTF 的新接口，开销更低。

### 4.2 探针有多吵：先量再挂

> As a rough guide for the computers of today, I would consider less than 100k kprobe or tracepoint events per second to be low frequency.

量法（照抄）：

```
# bpftrace -e 'k:vfs_read { @ = count(); } interval:s:1 { exit(); }'
```

作者故意只用 1 秒，就是为了**把探针自身的开销压到最低**，免得测出来的频率被探针成本污染。

### 4.3 参数：`args->` 与 `-lv`

tracepoint 的参数按 format 文件里的字段名访问：

```
bpftrace -e 'tracepoint:syscalls:sys_enter_read { @req_bytes = hist(args->count); }'
```

字段可以列出来（照抄）：

```
# bpftrace -lv 'tracepoint:syscalls:sys_enter_read'
tracepoint:syscalls:sys_enter_read
    int __syscall_nr;
    unsigned int fd;
    char * buf;
    size_t count;
```

> These fields can be listed from the format file in /sys or from bpftrace using -lv

### 4.4 流控

三种测试：过滤器、三元、if。

```
test ? true_statement : false_statement
```
```
$abs = $x >= 0 ? $x : - $x;
```
```
if (test) { true_statements }
if (test) { true_statements } else { false_statements }
```
```
if ($inet_family == $AF_INET) {
    // IPv4
    ...
} else {
    // assume IPv6
    ...
}
```

`else if` 从 bpftrace v0.10.0 起支持。循环用 `unroll()`；`while()` 需要 Linux 5.3 及以后（用的是 5.3 加进来的内核 BPF 循环支持）：

```
while (test) {
    statements
}
```

布尔运算符（表 15.5）：`==` `!=` `>` `<` `>=` `<=` `&&` `||`，可用括号分组。运算符（表 15.6）：`=`、`+ - * /`、`++ --`、`& | ^`、`!`、`<< >>`、以及 `+= -= *= /= %= &= ^= <<= >>=`。

### 4.5 内建变量（表 15.7，节选）

`pid`（进程 ID，内核 tgid）、`tid`（线程 ID，内核 pid）、`uid`、`username`、`nsecs`、`elapsed`、`cpu`、`comm`、`kstack`、`ustack`、`arg0 … argN`、`args`、`sarg0 … sargN`、`retval`、`func`、`probe`、`curtask`、`cgroup`（当前进程的默认 cgroup v2 ID）、`$1 … $N`（位置参数）。

> All integers are currently uint64.

### 4.6 函数（表 15.8）与异步陷阱

`printf` / `time` / `join` / `str` / `buf` / `strncmp` / `sizeof` / `kstack` / `ustack` / `ksym` / `usym` / `kaddr` / `uaddr` / `reg` / `ntop` / `cgroupid` / `system` / `cat` / `signal` / `override` / `exit`。

**异步这一条要记住**：

> Some of these functions are asynchronous: The kernel queues the event, and a short time later it is processed in user space. The asynchronous functions are printf() , time() , cat() , join() , and system() .

`printf()` 是异步的——内核把事件排队，稍后才在用户态打印。`kstack()` / `ustack()` / `ksym()` / `usym()` 则是**同步记录地址、异步翻译符号**。（`override(u64 rc)` 原文带一条警告：*a small mistake could panic or corrupt the kernel*。）

### 4.7 map 函数（表 15.9）

`count()` / `sum()` / `avg()` / `min()` / `max()` / `stats()` / `hist()` / `lhist()` / `delete()` / `print()` / `clear()` / `zero()`。其中 `print()` / `clear()` / `zero()` 也是异步的。

线性直方图的例子（照抄）：

```
# bpftrace -e 'tracepoint:syscalls:sys_enter_read {
    @fd[comm] = lhist(args->fd, 0, 100, 1); }'
Attaching 1 probe...
^C
[…]
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

### 4.8 文档去处（§15.2.6）

bpftrace 仓库里：`docs/reference_guide.md`、`docs/tutorial_one_liners.md`。原书其他章节也有 bpftrace：第 5 章 §5.5.7、第 6 章 §6.6.20、第 7 章 §7.5.13、第 8 章 §8.6.15、第 9 章 §9.6.11、第 10 章 §10.6.12，以及第 4 章与第 11 章。更细的内容在作者的上一本书 *BPF Performance Tools*（第 5 章专讲 bpftrace 语言）。原文还记了一笔账：上一本书标为「planned」的能力（`while()`、`else-if`、`signal()`、`override()`、watchpoint）如今都已落地。

---

## 5. 实战锚点：给 otf-server 装一把内核侧的尺子

### 5.1 仓库现状：诚实交代

**这个仓库里没有任何 BPF 资产。** grep 实测 `grep -rn "bpftrace\|BCC\|libbpf\|ebpf\|eBPF" src/ tests/`，唯一命中的是 `tests/fp_wire.cpp` 里一个十六进制常量 `0xAABBCCDDu`（碰巧含 "BCC" 三个字母）。**没有 `.bt` 脚本、没有 BCC 依赖、CI 里也没有。**

这台机器上 `which bpftrace` / `which bcc` 也都是空的——所以**下面所有命令是「按书里的语法 + 本仓库符号（`nm` 实测）」写出来的，没有在本机执行过**。第一次跑之前请自己校准探针名与参数名。

被观测的对象（都已 grep 确认）：

| 位置 | 事实 |
|---|---|
| `src/net/tcp_transport.cpp:246` | `TcpTransport::Poll(int timeout_ms)` → `::epoll_wait(epoll_fd_, events_.data(), (int)events_.size(), timeout_ms)` |
| `src/net/tcp_transport.cpp:19` | `kEpollBatch = 256` —— 一批最多 256 个事件 |
| `src/net/tcp_transport.cpp:20` | `kReadChunk = 4096` —— `OnReadable` 每次 `::read(fd, buf, sizeof(buf))` |
| `src/net/tcp_transport.cpp:197` | `TcpTransport::Ctl(int op, int fd, uint32_t events)` —— **全文件唯一** `::epoll_ctl` 出口 |
| `src/net/tcp_transport.cpp:399` / `:276` | `Accept()` 与 `AcceptNew()` 两个 accept 出口 |
| `src/net/metrics_server.cpp:23` | `kMaxServePerPoll = 8` —— 指标端口每轮最多服务 8 个连接 |
| `src/proto/protocol.h:160` | `kTickRateHz = 20` —— 一拍 50 ms |

### 5.2 目标一：每 tick 处理了多少连接

主循环 `src/main.cpp:314` 每拍调一次 `transport.Poll(sched.PollTimeoutMs(now))`，而 `Poll` 里**恰好一次** `epoll_wait`。所以 **`epoll_wait` 的返回值 = 这一拍处理的连接数**。

```
bpftrace -e 'tracepoint:syscalls:sys_exit_epoll_wait /args->ret >= 0/ { @events = hist(args->ret); }'
```

- `args->ret >= 0` 是必需的：`-1` 表示 `epoll_wait` 出错（仓库在 `EINTR` 时静默返回、其他错误 `log::warn`）。
- **先确认这台机器上的探针名**：现代内核一般是 `sys_exit_epoll_wait`，但 glibc 可能走 `epoll_pwait` / `epoll_pwait2`。跑一遍 `bpftrace -l 'tracepoint:syscalls:*epoll*'` 看清楚再改。

直方图怎么读：**峰值在 1~2** = 绝大多数拍只有一两个连接有事件，正常；**撞到 256** = 批量上限被打满了！`kEpollBatch = 256` 是硬天花板，`epoll_wait` 一次最多返回 256 个事件，剩下的**推迟到下一拍**——直方图在这个点上撞平，就是「这一拍处理不完」的证据；**长尾拖到几百** = 事件在几拍里分批消化，说明有连接在被反复唤醒。

### 5.3 目标二：read / write 的次数与字节数

书里给的 `read` 直方图（照抄）：

```
bpftrace -e 'tracepoint:syscalls:sys_exit_read { @ = hist(args->ret); }'
```

套到本仓库：**`kReadChunk = 4096`** —— `OnReadable` 每次只读 4096 字节，直方图的**主峰应落在 4096 附近**；如果大量样本挤在 4096，说明**单次读经常读不满**，对端在发小包。**返回 0 = 对端 FIN**（仓库把 0 当关闭处理），所以 `args->ret == 0` 的计数 = 对端主动断开的次数。**返回 -EAGAIN（`-11`）**：非阻塞 socket 没有更多数据；`OnReadable` 循环读到 EAGAIN 为止，所以 **EAGAIN 的次数 ≈ 活跃连接数**——这是「活跃连接数」的内核侧代理值。

`write` 同理：

```
bpftrace -e 'tracepoint:syscalls:sys_exit_write { @ = hist(args->ret); }'
```

`FlushWrite` 是循环写，`args->ret == -11`（EAGAIN）意味着**发送缓冲满了**——那一刻正是仓库去挂 `EPOLLOUT` 的原因。

### 5.4 目标三：accept 次数，以及「同一个进程里的两个 accept 来源」

书里的 accept 计数（照抄）：

```
bpftrace -e 't:syscalls:sys_enter_accept* { @[pid, comm] = count(); }'
```

但**在这个仓库里，这一行会同时数到三种 accept**：`TcpTransport::AcceptNew()`（`tcp_transport.cpp:276`，游戏端口的批量 accept）、`TcpTransport::Accept()`（`tcp_transport.cpp:399`，游戏端口的另一条路径）、以及 `MetricsServer::Poll()` 里的 `::accept`（`metrics_server.cpp:114`，**指标端口**）。tracepoint 只看得到「系统调用」，分不清是哪个 listener。**指标端口被采集器轮询时 accept 计数会突然尖峰**——`kMaxServePerPoll = 8` 决定了尖峰的量级。**要分开，只能挂到具体的 C++ 函数上**（下一节）。

### 5.5 目标四：挂到用户态函数上（uprobe）

原书给 uprobe 的格式是 `uprobe:/bin/bash:readline`——**二进制路径 + 冒号 + 函数名**。本仓库是 C++，符号是 mangled 的。`nm` 实测（`build/dev/otf_server`）：

| C++ 符号 | mangled 名 |
|---|---|
| `otf::net::TcpTransport::Poll(int)` | `_ZN3otf3net12TcpTransport4PollEi` |
| `otf::net::TcpTransport::AcceptNew()` | `_ZN3otf3net12TcpTransport9AcceptNewEv` |
| `otf::net::TcpTransport::Ctl(int, int, unsigned int)` | `_ZN3otf3net12TcpTransport3CtlEiij` |
| `otf::net::MetricsServer::Poll()` | `_ZN3otf3net13MetricsServer4PollEv` |

于是「每 tick 一次 Poll」可以这样数：

```
bpftrace -e 'uprobe:/home/xier/dev/game-project/otf-server/build/dev/otf_server:_ZN3otf3net12TcpTransport4PollEi { @polls = count(); }'
```

**三个必须知道的坑**：**① 二进制要有符号**——仓库 `CMakeLists.txt:17` 的默认构建类型是 `RelWithDebInfo`（带 `-g`），够用，但它是优化过的，函数可能被内联或重排，uprobe 报「找不到符号」就退回 Debug 构建再试。**② C++ 成员函数有隐含的 `this`**——`Ctl` 的第一个机器参数是 `this` 不是 `op`，bpftrace 各版本对 `arg0` 算不算 `this` **处理并不一致**，先用 `bpftrace -l 'uprobe:/path/to/otf_server:*TcpTransport*'` 确认能挂上，再用一个已知值（`op` 只可能是 1/2/3）校准 `argN` 的偏移。**③ 路径要绝对路径**，且指向**正在跑的那个**二进制。

### 5.6 目标五：把「EPOLLOUT 不变量」从外部验一遍

仓库 `src/net/tcp_transport.h` 的 `EpollCtlCounters` 有一组计数器：`add` / `del` / `mod` / `add_with_out` / `mod_with_out` / `mod_without_out`。文件头注释明说：**syscall 失败也计入**，而且**计数器全绿不代表内核真的收到了那个注册值**。

这正是 BPF 的用武之地——`Ctl(int op, int fd, uint32_t events)` 的第三个参数**就是事件掩码本身**（一个整数，不是指针）：

```
bpftrace -e 'uprobe:/home/xier/dev/game-project/otf-server/build/dev/otf_server:_ZN3otf3net12TcpTransport3CtlEiij { @[arg1] = count(); }'
```

- `arg1` 是 `op`（`EPOLL_CTL_ADD=1` / `DEL=2` / `MOD=3`），`arg2` 是 `events` 掩码（`EPOLLIN=1`、`EPOLLOUT=4`）。
- 于是「`add_with_out` 恒为 0」这条不变量可以**从进程外面**独立复核：`op==1` 的那格里，掩码绝不能带 `EPOLLOUT(4)`。

**这一件事 syscall tracepoint 做不到。** `epoll_ctl` 的事件掩码躺在**用户内存**的 `struct epoll_event` 里，`sys_enter_epoll_ctl` 只能给你一个指针，给不了指针指向的内容。uprobe 才能。

内核侧能看到的是「有没有真的成功」：

```
bpftrace -e 'tracepoint:syscalls:sys_enter_epoll_ctl { @[args->op] = count(); }'
```
```
bpftrace -e 'tracepoint:syscalls:sys_exit_epoll_ctl { @[args->ret] = count(); }'
```

`args->op`：`1=ADD`、`2=DEL`、`3=MOD`；`args->ret`：`0` 成功、`-1` 失败。**两条拼起来才是完整的真相**：应用层计数器数的是「我们打算做什么」，`sys_exit_epoll_ctl` 数的是「内核到底收下了没有」——仓库注释担心的正是这中间的空隙。

### 5.7 目标六：tick 漂移的另一半

仓库的 `otf_tick_drift_max_ms` 是**应用层**测的：墙钟时间和逻辑帧号一比，得出这一拍落后多少。它测的是「**结果**」。内核侧还有另一半：这个线程在 run queue 上**等了多久**。原书表 15.2 里的 `runqlat(8)` 干的正是这件事，bpftrace 版本可以先从上下文切换入手（书里照抄）：

```
bpftrace -e 't:sched:sched_switch { @[kstack, ustack, comm] = count(); }'
```

为什么这对本仓库重要：`otf-server` 是**单线程 reactor**，主循环线程被内核抢走 CPU，就等于**整拍丢掉**。`runqlat` 的峰值高 = 机器上有别的进程在抢；此时 `otf_tick_drift_max_ms` 的升高**不一定是自己的 bug**。两个指标一起看，才能分清「我写得慢」和「我被让出去了」。

### 5.8 目标七：先用 `-lv` 列参数

每次挂新探针前，先花两秒看它有哪些参数（原书用法，照抄）：

```
# bpftrace -lv 'tracepoint:syscalls:sys_enter_read'
tracepoint:syscalls:sys_enter_read
    int __syscall_nr;
    unsigned int fd;
    char * buf;
    size_t count;
```

同一个习惯套到 epoll 上（`bpftrace -lv 'tracepoint:syscalls:sys_exit_epoll_wait'`）——**参数名以本机内核为准，不要照抄别的机器的输出**。

### 5.9 对照：应用层 `Metrics` 与 BPF 的分工

| | 仓库的 `Metrics` | BPF |
|---|---|---|
| 位置 | 用户态，进程内 | 内核上下文 |
| 能看到 | 连接数、房间数、收发帧、epoll_ctl 次数、漂移峰值 | 上面这些**加上**内核知道而应用层不知道的：cgroup、namespace、调度延迟、内核栈 |
| 成本 | 一次原子加 | 探针开销（先量频率：**< 100k events/s** 才算低频） |

原文第 11 章 §11.3.4.2 有一节现成的例子：bpftrace 可以从 `curtask` 一路走到 UTS namespace 拿到**容器名**：

```
$task = (struct task_struct *)curtask;
$nodename = $task->nsproxy->uts_ns->name.nodename;
@new_processes[$nodename] = count();
```

**「BPF 的价值之一是它能读到 cgroup / namespace 这类只有内核知道的东西」**——仓库现在是**单进程裸跑**，这条暂时用不上；可一旦进容器，`otf_connections` 就分不清是哪个 pod 的了。

顺带一句安全性：第 3 章 §3.4.4 说过，BPF 字节码必须先过 verifier——*BPF bytecode must first pass through a verifier that checks for safety, ensuring that the BPF program will not crash or corrupt the kernel*。**这就是「在内核里执行用户写的程序」敢开放的前提**：不是靠信任你，是靠一道机器执行的准入闸门。

### 5.10 缺口清单（本仓库欠的）

1. **零 BPF 资产**：没有 `.bt` 脚本、没有 BCC 工具、没有把这套观测接进 CI 的打算。
2. **现有指标全是应用层的**：`otf_epoll_ctl_add/del/mod` 数的是「我们发起了几次」，不是「内核接受了几次」——失败的那些**照样计数**（仓库自己在注释里承认了）。
3. **没有 per-tick 的连接级视图**：现在只知道累计的 `connections` / `frames_sent` / `frames_recv`；「这一拍唤醒了几个连接」这个量**只有内核侧知道**。
4. **两个 accept 出口无法区分**：游戏端口 vs 指标端口，在 tracepoint 层是一锅粥。
5. **本机没装 bpftrace/bcc**：上面每一条命令都需要先自己校准（探针名、参数名、`this` 偏移）。
6. **生产上要 root**：bpftrace 必须 root 跑，容器里还得额外放开权限——这是落地的真实阻力，不是技术问题。

---

## 6. 收尾：本章金句清单

1. **What makes BPF different from other tracers is that it is programmable.** —— BPF 与别的追踪器的分水岭是「可编程」：不是给你更多开关，是让你写规则。
2. **While other tracers may require dumping all events to user space and post-processing them, BPF allows such processing to occur efficiently in kernel context.** —— 别的事件都往用户态倒，BPF 在内核里就地算完。
3. **This makes it practical to create performance tools that would otherwise cost too much overhead for production use.** —— 这一句才是 BPF 能进生产的全部理由：不是更快，是把「重」变成「轻」。
4. **This tool uses BPF to summarize the histograms in kernel space for efficiency, so the user-space component only needs to read the already-summarized histograms (the count columns) and print them.** —— 用户态只负责「打印」不负责「统计」；这是内核态聚合最直白的证据。
5. **The difference between BCC and bpftrace is like the difference between C programming and shell scripting, where BCC is like C programming (some of it is C programming) and bpftrace is like shell scripting.** —— 选工具就是选编程语言：复杂工具用 BCC，一次性问题用 bpftrace。
6. **BCC tools can take ten times as long to develop as bpftrace tools, and can have ten times as many lines of code.** —— 十倍时间、十倍行数；所以先在 bpftrace 里试。
7. **It is the awk(1) of tracing, and is based on awk(1).** —— 一句话定义 bpftrace：追踪世界的 awk。
8. **In awk(1), you write a program stanza to process an input line, and with bpftrace you write a program stanza to process an input event.** —— awk 吃行，bpftrace 吃事件；骨架因此完全一致。
9. **Instrumenting too many probes may cost unnecessary performance overhead.** —— 通配符挂太宽，观测本身就成了负载；所以有 `BPFTRACE_MAX_PROBES` 这道闸。
10. **You can test your wildcards before using them by running bpftrace -l to list the matched probes** —— 先 `-l` 预检再真挂；这是所有「观测先炸生产」的通用解药。
11. **bpftrace must be executed by the root user (super-user).** —— 内核的尺子，握它的人得是 root——落地时最先撞到的墙。
12. **As a rough guide for the computers of today, I would consider less than 100k kprobe or tracepoint events per second to be low frequency.** —— 十万 events/s 是一道分水岭；先量频率，再决定挂不挂。
13. **With traditional system tools, like iostat(1) and vmstat(1), the output is fixed and cannot be easily customized.** —— 传统工具给你的是一张定死的报表。
14. **But with bpftrace, the metrics you see can be further broken down into parts and enhanced with metrics from other probes until you have the answers you need.** —— 而 BPF 让你把指标按需拆开、与其他探针拼起来——这才是「回答问题」而不只是「打印数据」。
15. **Some of these functions are asynchronous: The kernel queues the event, and a short time later it is processed in user space.** —— `printf()` 是异步的：内核排队，用户态稍后才打印。写程序时要记得这个延迟。
