---
title: 附录 C. bpftrace One-Liners（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - bpftrace
  - eBPF
  - 观测工具
  - 速查表
  - 内核网络
source: "《Systems Performance》（Brendan Gregg，2018，英文原版）附录 C： bpftrace One-Liners"
companion: "[[附录C-Appendix C： bpftrace One-Liners]]"
---

# 附录 C 讲义：67 条一行命令，其实只回答 5 个问题

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[附录C-Appendix C： bpftrace One-Liners|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：`src/net/metrics.h:29`（本仓的观测面 = 应用内固定计数器，**看不到内核那一层**）、`src/net/tcp_transport.cpp:51`（`RawEpollCtl` —— 全文件唯一调 `::epoll_ctl` 的地方）、`src/net/tcp_transport.cpp:78`（`SO_RCVBUF`/`SO_SNDBUF` **刻意不调**，这条决定缓冲什么时候开始丢包）。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原附录。

⚠️ **这一篇是速查表，不是章节。** 原文稿 515 行里正文只有**三句话**，剩下 67 条命令 + 67 行占位垃圾。⇒ **逐条翻译命令没有价值**（原文稿就在库里，可以直接复制）。这一篇做三件事：① 给这 67 条命令**建一根轴**（它在回答什么问题）；② 讲**读输出该看哪几个字段**；③ **诚实对照本仓**——哪些命令用不上，以及**恰恰是用不上的那些，才对应着本仓看不见的那一层**。

---

## 0. 开局：三个指标全绿，玩家却说「卡」

某天 `otf-server` 上线，玩家反馈：**偶发卡顿**。你打开指标端点（`src/net/metrics_server.cpp`），所有数字都正常：

| 指标 | 值 | 说明 |
|---|---|---|
| `otf_frames_sent` / `otf_frames_recv` | 稳定上涨 | 帧在流 |
| `otf_heartbeat_timeouts` | 0 | 没有连接被判死 |
| `otf_tick_drift_max_ms` | 有峰值 | ★ 逻辑帧**确实**漂了 |

⇒ 「确实漂了，但帧没丢、连接没死」。**那卡在哪？** 问题在于：**上面这三个指标全都在应用边界的内侧。** 本仓能回答「我的代码有没有按节奏跑」，回答不了这三件事：

1. **`epoll_wait` 到底等了多久**——本仓只有一个 IO 等待点（`src/main.cpp:314` 的 `transport.Poll`），但**等待的分布**没人记。
2. **内核 socket 缓冲有没有丢包**——包在进 `epoll_wait` 之前就被内核扔了，应用层根本没见过它（对应 `sar -n EDEV` 的 `rxdrop/s`）。
3. **TCP 有没有重传**——重传发生在内核，本仓的 `frames_recv` 只数「收到几帧」。

⇒ **这三条，恰好就是附录 C 里那几条命令能回答的。** 所以这份附录的价值不是「教我敲一行命令」，而是**「告诉我还有哪些问题，是我自己压根没想到要问的」**。

---

## 1. §附录 C 的定位：它是一份索引，不是一篇教程

### 1.1 三句话正文 + 五组分类

全文正文只有这些：

> This appendix contains some handy bpftrace one-liners.

> Apart from being useful in themselves, they can help you learn bpftrace, one line at a time.

> Most of these were included in previous chapters.

⇒ 三句话给了三个信息：它**自认是速查表**（"handy"）；它有**教学意图**（一条命令一个知识点）；它**大部分从前面各章抄来**——所以它是索引，不是新增内容。正文之后是五个域：

| 域 | 条数 | 这个域在回答 |
|---|---|---|
| CPUs | 12 | 谁在跑、跑多久、切了多少次 |
| Memory | 14 | 堆从哪来、缺页、回收、换出 |
| File Systems | 13 | 谁在开文件、读了多少、有没有错 |
| Disks | 8 | 设备层排队与错误 |
| Networking | 20 | 连接、发收、重传、丢包 |

### 1.2 一条命令的骨架：探针 + 谓词 + 键 + 动作

67 条命令拆开看，**结构全都是同一个**。用 Networking 那条最典型的：

```bash
bpftrace -e 'kr:tcp_recvmsg /retval >= 0/ { @recv_bytes = hist(retval); }'
```

- **探针（挂哪儿）**：`kr:tcp_recvmsg` —— `kr` = kretprobe，挂在内核函数**返回**处。
- **谓词（过滤）**：`/retval >= 0/` —— 只在返回值为正时计数（排掉错误码）。
- **键（维度）**：本例无键（`@recv_bytes` 是单键）；可以是 `comm` / `pid` / `probe` / `func` / `ustack` / `kstack`。
- **动作（怎么聚合）**：`hist(retval)` —— `count()` / `sum()` / `hist()` / `lhist()` / `printf()` / `time()`。

探针类型六种，附录里都有例子：

| 写法 | 挂在哪 | 附录中的例子 | 你能看到什么 |
|---|---|---|---|
| `t:` | tracepoint | `t:syscalls:sys_enter_read` | 内核**公开的、带名字的**事件接口 |
| `k:` | 内核函数入口 | `k:tcp_sendmsg` | 函数被调用那一刻，能拿**入参** |
| `kr:` | 内核函数返回 | `kr:tcp_recvmsg` | 返回那一刻，能拿**返回值** |
| `u:` | 用户态函数 | `u:/lib/x86_64-linux-gnu/libc.so.6:malloc` | **你自己程序**里的函数调用 |
| `profile:hz:N` | 定时采样 | `profile:hz:49` | 采样时刻**谁在 CPU 上**（不是事件！） |
| `software:` | 内核软件事件 | `software:page-fault:1` | 软件级事件（页错误） |

### 1.3 ⚠️ 有三处**照抄就会翻车**

本篇的纪律是「代码块照抄原文，不修正」，但这份原文**本身就有错**：

- **(a) 每条命令前面都有一行占位垃圾**（67 条里有 64 条前面跟着）：`Click here to view code image` —— 这是 epub 里代码块的图片占位，**复制时不要一起复制进终端**。
- **(b) `tracepoint` 被拼成了 `tracepont`**（CPUs 域的调度器两条，第 92、99 行）：

```bash
bpftrace -e 'tracepont:sched:* { @[probe] = count(); }'
```

⇒ 照抄必然报错（找不到 probe），正确写法是 `tracepoint:sched:*`。**这是原文的真实笔误，不是抽取错误。**

- **(c) 第 500 行那条命令少了一个引号和一个分号**：`bpftrace -e 'k:ieee80211_* { @[func] = count()'` —— 收尾的 `'` 和 `;` 都不在了。

⇒ **推演（讲解性类比，非原文）**：速查表的读者通常**不是复制粘贴的读者**——你真正会做的是「读懂骨架，改两三个词，跑一条自己的」，而这三条恰好是「读骨架」时最容易顺手抄错的。⇒ **所以第 2 节的重点不是命令本身，是每条命令的「意图」——意图抄不走，命令才抄得走。**

---

## 2. §附录 C 五个域：这 67 条命令分别在回答什么问题

### 2.1 CPUs（12 条）：把「谁在占 CPU」拆成四个不同的问题

| 你想问的 | 附录的命令 | 输出里看什么 |
|---|---|---|
| 进程在干什么 | `tracepoint:syscalls:sys_enter_execve { join(args->argv); }` | 打印出来的**完整命令行** |
| 系统调用用了多少 | `tracepoint:raw_syscalls:sys_enter { @[pid, comm] = count(); }` | 每个进程一个计数 |
| 热点在哪 | `profile:hz:49 { @[kstack, ustack, comm] = count(); }` | **用户栈 + 内核栈**两个维度 |
| 切换时在等谁 | `tracepoint:sched:sched_switch { @[kstack] = count(); }` | 上下文切换时的**内核栈** |

⇒ **「采样」和「事件」是两种视角，别混用**：`profile:hz:49` 是**定时采样**，问「在 CPU 上时程序在哪」（找**热点**）；`sched_switch` 是**事件**，问「被切走那一刻它在等什么」（找**等待点**）。**这两个问题互不替代。**

⇒ 本节还藏着一条**通用骨架**：`bpftrace -e 'profile:hz:49 /pid == 189/ { @[ustack] = count(); }'` —— 把 `/pid == 189/` 换成 `/comm == "mysqld"/`，把 `@[ustack]` 换成 `@[kstack]`，加 `ustack(5)` 就是「只看栈顶 5 帧」。⇒ **推演（讲解性类比，非原文）**：这 67 条里大约三分之二，都是这条命令换掉「探针名 / 谓词 / 键 / 动作」四个零件得到的。**学会这一条，附录就变成 4 个零件的说明书。**

### 2.2 Memory（14 条）：堆从哪来、页从哪缺、内存怎么被回收

| 你想问的 | 附录的命令 | 输出里看什么 |
|---|---|---|
| 堆从哪来 | `u:...libc.so.6:malloc { @[ustack, comm] = sum(arg0); }` | **请求字节**之和（按用户栈分组） |
| 堆在哪个调用点变大 | `tracepoint:syscalls:sys_enter_brk { @[ustack, comm] = count(); }` | `brk` 的调用栈 |
| 缺页 / 换出 | `software:page-fault:1 { @[comm, pid] = count(); }` / `kprobe:swap_readpage { @[comm, pid] = count(); }` | 每进程页错误数 / 谁在换入 |

⇒ 三条要**特别小心**的读法：

1. **`sum(arg0)` 是「请求字节」，不是「实际占用字节」。** `malloc(1024)` 请求 1024，但分配器可能给你 1032（对齐），也可能给你一整块 4KB。**这两者的差就是分配器的账。**
2. **带 `(high overhead)` 标注的会拖慢被测程序**——每个事件都要抓用户栈。附录在 malloc 那条上明写了 `high overhead`，这是**作者在提醒你别在生产机上乱跑**。
3. **`software:page-fault:1` 的 `:1` 是精度参数**（每页错误采 1 个），不是「1 秒一次」——**采样，不是计数**。

### 2.3 File Systems（13 条）：请求量 vs 实际量，是这一域最值得学的一对

| 你想问的 | 附录的命令 | 输出里看什么 |
|---|---|---|
| 谁在开文件 | `t:syscalls:sys_enter_openat { printf("%s %s\n", comm, str(args->filename)); }` | 进程名 + **文件名** |
| 读写的量级 | `tracepoint:syscalls:sys_enter_*read* { @[probe] = count(); }` | **按 probe 名**分组（read / pread64 / readv 分开） |
| 请求量分布 | `tracepoint:syscalls:sys_enter_read { @ = hist(args->count); }` | **请求**了多少字节 |
| 实际量分布 | `tracepoint:syscalls:sys_exit_read { @ = hist(args->ret); }` | **实际**收到多少字节（**含错误码**） |
| 错误长什么样 | `t:syscalls:sys_exit_read /args->ret < 0/ { @[- args->ret] = count(); }` | **正数**错误码的分布 |

⇒ **这一对是全附录最值得学的一处**：`sys_enter_read` 的 `args->count` 是**请求量**（我要读 4096 字节）；`sys_exit_read` 的 `args->ret` 是**实际量**（实际只读到 812 字节，或返回 -11）。**「我以为读了 4KB，实际只拿到 812 字节」——这就是短读（short read）。** 单看任何一边都发现不了它，**必须成对看**。⇒ 顺带一个读法细节：`@[- args->ret]` 里的**负号是故意加的**——`args->ret` 是负的 errno（`-11` = EAGAIN），取反成 `11` 只是为了**让输出的键读起来像错误码**。不改逻辑，只改可读性。

### 2.4 Disks（8 条）：设备层的排队与错误

| 你想问的 | 附录的命令 | 输出里看什么 |
|---|---|---|
| 块设备上有多少事发生 | `tracepoint:block:* { @[probe] = count(); }` | 每个 block 事件 |
| 单次 I/O 多大 | `t:block:block_rq_issue { @bytes = hist(args->bytes); }` | **请求大小**分布 |
| I/O 是什么类型 | `t:block:block_rq_issue { @[args->rwbs] = count(); }` | `rwbs` 位标志（读/写/同步/…） |
| 有没有 I/O 错误 | `t:block:block_rq_complete /args->error/ { printf(...); }` | **设备号 + 类型 + 错误码** |

⇒ **必须看懂这一对：`rq_issue` 是发起，`rq_complete` 是完成。** `args->error` 字段**只在 `rq_complete` 上有**，`rq_issue` 上没有 ⇒ 想查错误，必须挂在完成事件上。⇒ **推演（讲解性类比，非原文）**：`issue` 与 `complete` 之间的**时间差**就是「这次 I/O 的延迟」，**同一时刻未完成的请求数**就是「设备队列有多深」。**队列越深，延迟越高**——这就是「加 CPU 没用、要加盘」的物理含义。**而这两种量，应用层一个都看不见。**

### 2.5 Networking（20 条）：本仓最该看的一节

附录里最长的一域，也是**唯一一个和「在线服务」直接相关的域**。按层看会清楚很多：

| 层 | 命令 | 回答 |
|---|---|---|
| socket | `t:syscalls:sys_enter_accept* { @[pid, comm] = count(); }` | 谁在建连接 |
| socket | `t:syscalls:sys_enter_connect { @[ustack, comm] = count(); }` | ★ **从哪个调用点连出去的** |
| socket | `k:sock_sendmsg,k:sock_recvmsg { @[func, pid, comm] = count(); }` | 收发各多少次 |
| socket | `kr:sock_sendmsg,kr:sock_recvmsg /(int32)retval > 0/ { @[pid, comm] = sum((int32)retval); }` | 收发各多少**字节** |
| TCP | `k:tcp_v*_connect` / `k:inet_csk_accept` | TCP 层建连 / 接入 |
| TCP | `k:tcp_sendmsg { @send_bytes = hist(arg2); }` | 发出字节分布（**请求量**） |
| TCP | `kr:tcp_recvmsg /retval >= 0/ { @recv_bytes = hist(retval); }` | 收到字节分布（**实际量**） |
| TCP | ★ `t:tcp:tcp_retransmit_* { @[probe, ntop(2, args->saddr)] = count(); }` | ★ **重传：按类型 + 远端主机** |
| 设备 | `t:net:net_dev_xmit { @[kstack] = count(); }` | **发送路径**的内核栈 |
| 设备 | `t:net:netif_receive_skb { @[str(args->name)] = lhist(cpu, 0, 128, 1); }` | 收包**落在哪个 CPU** |

⇒ **三条读法**：

1. **`t:tcp:tcp_retransmit_*` 是全附录最有价值的一条命令。** 输出 `@[probe, ntop(2, args->saddr)]` 直接就是「**哪个远端、哪类重传**」（`ntop(2, args->saddr)` 把二进制地址转成点分十进制）。⇒ **推演（讲解性类比，非原文）**：重传率是**网络健康度的第一指标**——它涨，要么是丢包（网络/对端），要么是拥塞（自己发太快），而这两种原因**在应用层看起来一模一样**：「就是有点卡」。
2. **`k:` 和 `kr:` 的差别，在这里正好是「请求量 vs 实际量」**：`arg2` 是**请求发多少**，`retval` 是**实际收到多少**——和 2.3 那对 `read` 同理。
3. **`net_dev_xmit` 抓发送路径的内核栈，`netif_receive_skb` 抓接收路径**：两条一起回答「**内核在替我发包时做了哪些事**」——而这些事**全部发生在应用代码之外**。

---

## 3. 读输出：`@` 里的键和值各是什么

附录里出现的动作只有六种，**每种对应一种「输出长什么样」**：

| 动作 | 输出形态 | 读什么 |
|---|---|---|
| `count()` | 每个键一个整数 | 频次排名，**从大到小**看 |
| `sum(x)` | 每个键一个整数（累加值） | 总量，**不是速率** |
| `hist(x)` / `lhist(x, min, max, step)` | **对数桶** / **线性桶** | 分布形状，**看桶宽不看绝对值** |
| `printf(...)` | **逐条事件行** | 单条事件的细节 |
| `time()` | 一个时间戳 | 事件**何时**发生 |

⇒ **三条读法**：

1. **`hist` 是对数桶。** 输出里那些 `1`, `2`, `4`, `8`, `16`, `32`… 不是数值，是**区间边界**。⇒ 「请求大小集中在 4KB–8KB 桶」，而不是「平均 4KB」。**`hist` 给你形状，不给你均值。**
2. **`printf` 与聚合是两种东西。** 聚合是「统计」，`printf` 是「事件流」。附录里 `printf` 只在两处出现：`openat`（看文件名）和 `block_rq_complete /args->error/`（**只在出错时打印**）。⇒ **带谓词的 `printf` 是「异常事件日志」，不带谓词的是「事件流水」。**
3. **结果什么时候出来？** `bpftrace` 收到 `Ctrl-C` 时才把 `@` 里的 map 打印出来（附录自己指到 §15.2 讲）。⇒ **推演（讲解性类比，非原文）**：这意味着这些命令**不是监控，是诊断**——你得先有怀疑对象，再去跑它，然后停下来读结果。

⇒ **键的选择就是「你想要的维度」**：`comm`（进程名，**16 字符会截断**）、`pid`（进程号）、`probe`（探针名）、`func`（函数名）、`ustack`（用户栈，找**哪个调用点**，**开销大**）、`kstack`（内核栈，找**卡在内核哪**）。⚠️ **`comm` 会截断**，两个不同进程可能显示成同一个 `comm`——要区分就用 `pid`。

---

## 4. 实战锚点：`otf-server` 面对这份附录

### 4.1 先说诚实结论：**本仓一行 bpftrace 都没有**

全仓搜不到 `bpftrace` / `eBPF` / `perf` / `Ftrace` 的任何使用。本仓的观测面是**应用内埋点**：`src/net/metrics.cpp` 是一组**固定计数器**（累加量 + 峰值量）；`src/net/metrics_server.cpp` 把它渲染成文本（`otf_frames_sent`、`otf_tick_drift_max_ms` …）走一个**独立端点**；`src/app/reap_guard.h`、`src/app/tick_scheduler.cpp` 的日志相当于**追踪**。⇒ **本仓没有 profiler，没有 sar，没有内核级观测。** 这是**阶段一的现状**，不是疏漏。

### 4.2 但「用不上」的那几条，正好是**本仓看不见的那一层**

把附录的命令翻译成传统指标，再对照 `src/net/metrics.h` 的字段：

| 附录里的命令 | 对应传统指标 | 本仓有吗 |
|---|---|---|
| `t:tcp:tcp_retransmit_*` | `retrans/s`（`sar -n ETCP`） | ❌ **没有** |
| `t:net:net_dev_xmit` / `netif_receive_skb` | `rxdrop/s`、`txdrop/s`（`sar -n EDEV`） | ❌ **没有** |
| `kr:sock_recvmsg` / `kr:tcp_recvmsg` 的字节直方图 | 收**字节** | ❌ 只有 `otf_frames_recv`（**帧数**） |
| `tracepoint:sched:sched_switch`（off-CPU 栈） | off-CPU 时间 | ❌ **没有** |
| `sys_enter_read` / `sys_exit_read` 的请求量 vs 实际量 | 短读 | ❌ **没有** |

⇒ **本仓的观测面在应用边界的内侧，内核边界那一层是盲区。** ⇒ **推演（讲解性类比，非原文）**：`otf_frames_recv` 像「前台签收的包裹数」，`rxdrop/s` 是「**快递公司门口扔掉的包裹数**」。盯前台签收数看，你永远不会发现后门在丢件。**这正是 SP 第 4 章（Observability Tools）的主题：观测手段决定你能看见哪一类故障。**

### 4.3 但确实有三条，在本仓**是可以真用的**

**(a) `malloc` 探针 —— 验证「每帧 0 次堆分配」这个说法。**

`src/net/frame_codec.h` 的 `BeginFrame` 注释里写着一句很硬的话：

> 重构前 `EncodeWorldSnapshot`（98B payload）是每帧 **8 次**堆分配 —— 走 BeginFrame 之后是**每帧 0 次分配、0 次额外搬运**。

这句话目前**只有注释和 benchmark 佐证**。而附录里正好有一条能**直接数堆分配点**的命令（⚠️ 附录在这一条上明写了 `high overhead`）：

```bash
bpftrace -e 'u:/lib/x86_64-linux-gnu/libc.so.6:malloc { @[ustack, comm] = sum(arg0); }'
```

⇒ **在压测环境跑它，`@[ustack]` 里出现 `BeginFrame` 的调用点吗？** 出现 ⇒ 「每帧 0 次分配」这句注释在**生产形态**下也成立。

**(b) `profile:hz` 采样 —— 区分「卡在 `epoll_wait`」和「卡在 tick」。**

本仓**唯一的 IO 等待点**是 `src/main.cpp:314` 的 `transport.Poll(sched.PollTimeoutMs(now))`；`src/main.cpp:337` 的 `metrics.on_tick_drift(drift)` 只告诉你「**迟了**」，不告诉你「**迟在哪一步**」。⇒ 用 `profile:hz:49 { @[kstack, ustack, comm] = count(); }` 采样，看 `kstack` 里出现的是不是 `epoll_wait`——**这一步在应用代码里根本看不到**（你看不见自己「没在跑」的时间）。

**(c) `tcp_transport.cpp:78` 那条「刻意不调」—— 决定了缓冲什么时候开始丢。**

```cpp
// ⚠️ SO_RCVBUF / SO_SNDBUF 刻意不调（§6.4.3）：带宽余量 20 倍以上，
//   调它只会带来"为什么我调了没用"的困惑。
```

⇒ 注释里「带宽余量 20 倍以上」是一个**假设**，不是**观测**。而附录里的 `net_dev_xmit` / `netif_receive_skb` / `tcp_retransmit_*` 正是**这个假设成立与否的观测手段**。⇒ **推演（讲解性类比，非原文）**：这是「**参数是靠论证定下来的**」，而「**参数最终要不要改**是靠观测定的」——两者不是一回事。

### 4.4 一个必须说清的反例：**bpftrace 替代不了「唯一入口」**

本仓有一处讲究的设计：`src/net/tcp_transport.cpp:51` 的 `RawEpollCtl`，是**全文件唯一**调 `::epoll_ctl` 的地方（文件里还写了一条 grep 自查命令）。⇒ 很容易产生一个错误联想：「那我是不是也能用 bpftrace 挂到 `epoll_ctl` 上，在**内核那侧**再记一笔账？」**不行。** 两个原因：

1. `epoll_ctl` 是**用户态系统调用**。内核探针（`k:`/`kr:`）挂的是**内核函数**，不是你的调用点。想看你**自己代码**的调用点，那叫**埋点**——本仓已经做了（`TcpTransport::Ctl` 先记账再调 `RawEpollCtl`）。
2. 就算挂到了内核的 `epoll_ctl` 实现上，你看到的也只是「**一次调用**」，看不到 epoll **内部等的时长**——那部分在 `epoll_wait` 的等待队列里，是**另一件事**。

⇒ **推演（讲解性类比，非原文）**：`RawEpollCtl` 是「**唯一的门**」，bpftrace 是「**走廊里的摄像头**」。摄像头能看见谁进出这扇门，但它**替代不了那扇门本身的存在**。⇒ **别因为有了摄像头就不装门。**

### 4.5 如果要补观测，先补哪两个

本仓**已经**有、但没接进指标端点的一个现成量：`TcpTransport::queued_bytes(ConnId)`（`src/net/tcp_transport.cpp:191`）—— 该连接的**发送积压字节数**（`c.out.size() - c.out_sent`）。⇒ **这是应用自己唯一能看见的「背压」**：它涨，说明对端读得慢或网络堵。接进 `MetricsSnapshot` 是**纯应用内**的事，不需要任何内核工具；它能补上 `otf_frames_sent` 的盲区：**帧数掩盖了「小包风暴」**。

第二个（重传 / 丢包）**应用内拿不到**，只能从内核取（`/proc/net/snmp`、`/proc/net/netstat` 或 `nstat`），**或者**用附录里那条 `t:tcp:tcp_retransmit_*`。⇒ 本篇最实际的一条结论：**要拿到重传率，你迟早得碰内核那一层。**

---

## 5. 收尾：本章金句清单

1. **This appendix contains some handy bpftrace one-liners.** —— 它自认是速查表：**「handy」** 不是「完备」。
2. **Apart from being useful in themselves, they can help you learn bpftrace, one line at a time.** —— 附录有教学意图：**一条命令一个知识点**。所以读它的正确姿势是拆零件，不是背。
3. **Most of these were included in previous chapters.** —— 它是**各章的汇总索引**。想学原理回正文，这里只负责「能跑的那一行」。
4. **Many may not work right away: They may depend on the presence of certain tracepoints or functions, or on a specific kernel version or configuration.** —— ⚠️ **最该记住的一句**：这些命令**依赖内核版本与配置**，抄下来不代表能跑。附录自己先把这话说在前面了。
5. **See Chapter 15 , Section 15.2 , for an introduction to bpftrace.** —— 附录**没有**教学，只有指路：`§15.2` 才是入门。（这一句里逗号前的空格是 epub 抽取的痕迹）
6. **Trace new processes with arguments:** —— `join(args->argv)` 拼出**完整命令行**：进程名 `comm` 只有 16 字符，要看全就得靠它。
7. **Sample user and kernel stacks at 49 Hertz, system wide, with the process name:** —— **采样**（不是事件）：回答「在 CPU 上时你在哪」。
8. **Count off-CPU kernel stacks for context switch events:** —— 与上一条互补：**事件**回答「被切走时你在等谁」。**热点与等待点，是两个问题。**
9. **Sum libc malloc() request bytes by user stack and process (high overhead):** —— `high overhead` 是**作者的警告**：抓 `ustack` 会拖慢被测程序，**别在生产机上乱跑**。
10. **Count process heap expansion (brk(2)) by code path:** —— `brk` 按**代码路径**分组：堆在哪儿变大，比「堆有多大」有用。
11. **Show the distribution of read() syscall request sizes:** —— 这是**请求量**。
12. **Show the distribution of read() syscall read bytes (and errors):** —— 这是**实际量**（还含错误码）。**成对读才看得见短读**——单看任何一边都发现不了。
13. **Count read() syscall errors by error code:** —— 附录在命令里用 `@[- args->ret]` 把 errno **取反成正的**，只为可读。
14. **Trace block I/O errors with device and I/O type:** —— 错误只在 **`rq_complete`** 上（`rq_issue` 没有 `error` 字段）；「发起 vs 完成」的时间差就是 I/O 延迟。
15. **Count socket send/receives by direction, on-CPU PID, and process name:** —— `sock_*` 是**socket 层**，`tcp_*` 是 **TCP 层**，`net_*` 是**设备层**：同一个现象要在三层各看一次。
16. **Count socket send/receive bytes by on-CPU PID and process name:** —— **字节**比**次数**有用：次数看不出「小包风暴」。
17. **Count TCP retransmits by type and remote host (assumes IPv4):** —— ★ **全附录最有价值的一条**：重传率是网络健康度的第一指标，而丢包与拥塞**在应用层长得一模一样**。
18. **Count all TCP functions (adds high overhead to TCP):** —— 「全量挂」永远有代价：作者宁可一条条精确挂，也不让你全挂。

---

> **收尾一句**：这份附录真正的产物不是 67 条命令，而是**一张「我还没问过内核哪些问题」的清单**。命令抄得走，问题抄不走——**问题才是这 67 行想教给你的东西**。
