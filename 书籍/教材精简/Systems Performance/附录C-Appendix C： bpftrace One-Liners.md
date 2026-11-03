---
title: "附录C-Appendix C： bpftrace One-Liners（精读）"
category: 性能工程
tags:
  - bpftrace
  - eBPF
  - 内核追踪
  - 性能观测
  - 速查表
difficulty: 进阶
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）附录C bpftrace One-Liners"
source_section: "精读 §附录 C"
skills:
  - obsidian-markdown
---

# 附录C-Appendix C： bpftrace One-Liners

> **小节地图**：本篇是**一份速查表**，不是理论章。原文只有一级标题（`# Appendix C: bpftrace One-Liners`），
> 正文是「一句说明 + 一条命令」的重复结构，按 **CPUs / Memory / File Systems / Disks / Networking** 五个域排开，
> 共 67 条一行命令。
> 本篇按**观测对象**重组为九节：§附录 C 定位与边界、§附录 C 一行命令的骨架、
> §附录 C 进程与系统调用、§附录 C CPU 与调度延迟、§附录 C 内核函数插桩、§附录 C 内存、
> §附录 C 文件系统、§附录 C 磁盘、§附录 C 网络。
> **不立篇**：无（原文没有编号子节，也没有独立的 References 段）。
> 底层依赖：bpftrace 语法与探针类型见 [[Chapter 15. BPF]]；内核侧可观测性见 [[Chapter 14. Ftrace]]、
> [[Chapter 13. perf]]；被测机制分别在 [[Chapter 3. Operating Systems]]、[[Chapter 6. CPUs]]、
> [[Chapter 7. Memory]]、[[Chapter 8. File Systems]]、[[Chapter 9. Disks]]、[[Chapter 10. Network]]；
> 方法论见 [[Chapter 2. Methodologies]]；同一套「速查表」的写法见
> [[附录B-Appendix B： sar Summary]] 与 [[附录A-Appendix A： USE Method： Linux]]。
> ⚠️ **本篇的性质**：原文**只给「哪条命令量什么」，不给任何阈值、判据或读数解释**。
> 所以下面每张表里的「异常信号」「往哪查」两列**全部是本篇的推演**，不是原文观点。
> ⚠️ **原文稿有 3 处缺陷**（1 处 epub 占位、1 处拼写错误、1 处命令残缺），
> 本篇照抄不改，纠错见文末「纠错补充」。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。
原文每条命令**先一句英文说明、再一个代码块**，因此引文多为「这句说明」或「这条命令」整行。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §附录 C 定位与边界 | 附录C-Appendix C： bpftrace One-Liners.md L25 | This appendix contains some handy bpftrace one-liners. Apart from being useful in themselves, they can help you learn bpftrace, one line at a time. Most of these were included in previous chapters. Many may not work right away: They may depend on the presence of certain tracepoints or functions, or on a specific kernel version or configuration. |
| §附录 C 定位与边界 | 附录C-Appendix C： bpftrace One-Liners.md L27 | See Chapter 15 , Section 15.2 , for an introduction to bpftrace. |
| §附录 C 一行命令的骨架 | 附录C-Appendix C： bpftrace One-Liners.md L43 | bpftrace -e 'tracepoint:raw_syscalls:sys_enter { @[pid, comm] = count(); }' |
| §附录 C 一行命令的骨架 | 附录C-Appendix C： bpftrace One-Liners.md L204 | bpftrace -l 'usdt:/lib/x86_64-linux-gnu/libc.so.6:*' |
| §附录 C 进程与系统调用 | 附录C-Appendix C： bpftrace One-Liners.md L31 | Trace new processes with arguments: |
| §附录 C 进程与系统调用 | 附录C-Appendix C： bpftrace One-Liners.md L108 | Trace new threads via pthread_create(): |
| §附录 C CPU 与调度延迟 | 附录C-Appendix C： bpftrace One-Liners.md L59 | Sample user and kernel stacks at 49 Hertz, system wide, with the process name: |
| §附录 C CPU 与调度延迟 | 附录C-Appendix C： bpftrace One-Liners.md L80 | Sample user-level stacks at 49 Hertz, for processes named “mysqld”: |
| §附录 C CPU 与调度延迟 | 附录C-Appendix C： bpftrace One-Liners.md L99 | bpftrace -e 'tracepont:sched:sched_switch { @[kstack] = count(); }' |
| §附录 C 内核函数插桩 | 附录C-Appendix C： bpftrace One-Liners.md L453 | Count all TCP functions (adds high overhead to TCP): |
| §附录 C 内存 | 附录C-Appendix C： bpftrace One-Liners.md L119 | Sum libc malloc() request bytes by user stack and process (high overhead): |
| §附录 C 内存 | 附录C-Appendix C： bpftrace One-Liners.md L162 | bpftrace -e 'software:page-fault:1 { @[comm, pid] = count(); }' |
| §附录 C 内存 | 附录C-Appendix C： bpftrace One-Liners.md L178 | Count swapins by process: |
| §附录 C 文件系统 | 附录C-Appendix C： bpftrace One-Liners.md L241 | Show the distribution of read() syscall request sizes: |
| §附录 C 文件系统 | 附录C-Appendix C： bpftrace One-Liners.md L255 | Count read() syscall errors by error code: |
| §附录 C 磁盘 | 附录C-Appendix C： bpftrace One-Liners.md L342 | Trace block I/O errors with device and I/O type: |
| §附录 C 网络 | 附录C-Appendix C： bpftrace One-Liners.md L403 | Count socket send/receive bytes by on-CPU PID and process name: |
| §附录 C 网络 | 附录C-Appendix C： bpftrace One-Liners.md L446 | Count TCP retransmits by type and remote host (assumes IPv4): |

> **书里没说的**（本篇的推论，原文没有，别当原文观点）：
> - **「原文只给命令、不给读数」是本篇最关键的一条边界**：原文 67 条里没有一条出现阈值、
>   「正常范围是多少」或「异常时怎么办」。本篇各表的「异常信号」「往哪查」两列是本篇按
>   [[Chapter 2. Methodologies]] 的思路补的，**不是原文陈述**。
> - **「按观测对象重组」是本篇的重组**：原文按 CPUs / Memory / File Systems / Disks / Networking
>   五个资源域排，本篇按「你想量什么」重排（进程 / 调度 / 内核函数 / 内存 / 文件系统 / 磁盘 / 网络），
>   **命令逐字照抄**，只是归堆方式变了。
> - **「输出里看哪个字段」这一层是本篇补的**：原文只说「count 什么」，不说 map 打印出来后
>   怎么读。本篇按 bpftrace 的输出形态（map 型按键聚合、直接打印型逐条输出）解释。
> - **探针前缀（`t:` / `k:` / `kr:` / `u:` / `usdt:` / `software:` / `profile:hz:`）的对照表
>   取自 [[Chapter 15. BPF]] §15.2.5 的原文表**，不来自本附录；本附录只是用到了它们。
> - **「software:page-fault:1 末尾的 `1` 筛的是写类缺页」是本篇的推演**：bpftrace 的 software 探针
>   可按 PFEC 位过滤，但**具体位含义请在本机内核上实测确认**，原文本附录没解释这个参数。
> - **「on-CPU 采样 vs off-CPU 采样」的分工是本篇的读法**：原文 L94 只把 `sched_switch` 那条
>   标成 "off-CPU kernel stacks"，没说 `profile:hz:` 只在 on-CPU 时采样；后者见
>   [[Chapter 6. CPUs]] 与 [[Chapter 15. BPF]] 的采样机制说明。
> - **本篇未收录的采集侧内容**（bpftrace 安装、权限与内核配置、`BPFTRACE_MAX_PROBES`、
>   输出落盘与后处理）原文一概没有，本篇也不补——那是部署问题，不是本附录的职责。

## §附录 C 定位与边界

**机制**：这是一张 **bpftrace 一行命令的速查表**。原文自陈了三件事：

> This appendix contains some handy bpftrace one-liners. Apart from being useful in themselves, they can help you learn bpftrace, one line at a time. Most of these were included in previous chapters. Many may not work right away: They may depend on the presence of certain tracepoints or functions, or on a specific kernel version or configuration.

拆开读，**最要紧的是最后一句**：**很多条命令照抄不能直接跑**——它依赖某个 tracepoint 或内核函数
在你这台机器上**恰好存在**，也依赖**某个内核版本或配置**。这决定了本附录的正确用法：

| 原文的定位 | 读法（推演） |
|---|---|
| "handy one-liners" | 它是**工具箱**，不是教程；每条只回答「量什么」 |
| "help you learn bpftrace, one line at a time" | 它同时是**语法样例**——读一条就学会一个语法点 |
| "Most of these were included in previous chapters" | 它是前面各章的**横向索引**，不是新知识 |
| "may not work right away" | **先用 `bpftrace -l` 确认探针存在**，再插桩 |

bpftrace 的语法入门在第 15 章：

> See Chapter 15 , Section 15.2 , for an introduction to bpftrace.

**容易错在哪**（推演）：把速查表当「可运行脚本集」批量粘贴。原文已经说了 **Many may not work right away**，
而且**其中至少一条命令本身是残缺的**（见文末纠错补充）。正确的用法是**读骨架、换参数、只跑一条**。

**判据**：想学 bpftrace 语法 → 读这 67 条；想找「这个现象用哪条命令量」→ 查这 67 条；
想理解 bpftrace 的语法与探针类型 → 去 [[Chapter 15. BPF]] §15.2。

## §附录 C 一行命令的骨架

**机制**：原文 67 条里，除去 3 条 `bpftrace -l` 列举式与若干 `printf`/`time()` 直接打印式，
其余**全部是同一个形状**：

```bash
bpftrace -e '探针类型:目标 /谓词/ { @[键] = 聚合(参数); }'
```

四个部件，**每个部件都是可替换的槽位**：

| 部件 | 作用 | 原文里的例子 |
|---|---|---|
| **探针类型:目标** | 在哪儿插桩 | `tracepoint:syscalls:sys_enter_execve`、`kprobe:vfs_*`、`u:/lib/.../libc.so.6:malloc`、`profile:hz:49` |
| **/谓词/** | 什么时候算数（不满足就跳过） | `/pid == 189/`、`/comm == "mysqld"/`、`/args->ret < 0/`、`/(int32)retval > 0/` |
| **@[键]** | 按什么维度分组统计 | `@[pid, comm]`、`@[probe]`、`@[func]`、`@[kstack]`、`@[ustack]` |
| **聚合(参数)** | 算什么、算完存哪 | `count()`、`sum(arg0)`、`hist(args->bytes)`、`lhist(cpu, 0, 128, 1)` |

**换掉「键」就换一个观测维度**——这是本附录最值得学的一点。原文同一批探针上给出了多种键：

> bpftrace -e 'tracepoint:raw_syscalls:sys_enter { @[pid, comm] = count(); }'

同一个 `sys_enter` 探针，键换成 `[probe]` 就变成「按系统调用名分类」，键换成 `[func]` 就变成
「按内核函数名分类」。**探针决定「在哪儿」，键决定「按什么看」。**

### 探针前缀速查（对照表来自 [[Chapter 15. BPF]] §15.2.5 的原文表）

| 原文附录里出现的写法 | 完整名 | 插桩对象 |
|---|---|---|
| `tracepoint:...` / `t:...` | tracepoint | 内核静态插桩点 |
| `kprobe:...` / `k:...` | kprobe | 内核动态函数入口 |
| `kr:...` | kretprobe | 内核动态函数**返回** |
| `u:...` | uprobe | 用户态动态函数入口 |
| `usdt:...` | usdt | 用户态静态定义追踪 |
| `software:...` | software | 内核软件事件 |
| `profile:hz:N` | profile | 跨所有 CPU 的定时采样 |

**`kr:` 与 `k:` 的差别是有代价的**：`kr:` 拿得到 `retval`（返回值），所以能统计**字节数**、
**错误码**、**成功与失败**；`k:` 只有入口，拿不到返回值。原文里凡是「统计字节」「统计错误」
的条目几乎都用 `kr:`——这不是随意选择。

### 先列举、再插桩：`bpftrace -l`

**机制**：既然原文说很多条命令「may not work right away」，那**第一步是确认探针存不存在**。
bpftrace 的列举模式就是干这个的：

> bpftrace -l 'usdt:/lib/x86_64-linux-gnu/libc.so.6:*'

原文共给了 3 条列举命令：

```bash
bpftrace -l 'usdt:/lib/x86_64-linux-gnu/libc.so.6:*'
bpftrace -l 't:kmem:*'
bpftrace -l 't:*:mm_*'
```

**读法**：`bpftrace -l` **不加 `-e`**，打印的是**匹配到的探针清单**（一探针一行）。
所以它的输出不是统计结果，而是**「你这台机器上能插哪些桩」的清单**。

**判据（推演）**：任何一条要跑的探针，**先用 `-l` 跑一遍**；清单为空 ⇒ 那条命令在当前内核上
不成立（探针没编进去 / 内核版本不匹配 / 配置没开），此时换命令而不是换参数。

### 输出形态：两种，读法完全不同

| 输出形态 | 原文例子 | 长什么样 | 怎么读 |
|---|---|---|---|
| **map 型**（`@` 开头） | `@[pid, comm] = count();` | 退出时按键打印「键 → 值」 | **值是聚合结果**（次数 / 字节数 / 直方图），键就是维度 |
| **直接打印型** | `printf(...)`、`time();` | 每命中一条立刻输出一行 | **一行一事件**，自己数行数或看时间戳 |

**推演**：读 map 型输出时，先问「**键是什么、值是什么**」——同一个 `count()`，
键是 `[pid, comm]` 就按进程分，键是 `[kstack]` 就按阻塞点分。**不看键就解读值，等于白读。**

## §附录 C 进程与系统调用

这一域回答三个问题：**谁在起进程、哪个进程在疯狂调系统调用、谁在开线程。**

### 谁在起进程（带命令行）

> Trace new processes with arguments:

```bash
bpftrace -e 'tracepoint:syscalls:sys_enter_execve { join(args->argv); }'
```

**机制**：`execve` 是「执行一个新程序」的系统调用，探针 `sys_enter_execve` 在**进入内核的那一刻**触发；
`join(args->argv)` 把 `argv` 数组拼成一条完整命令行直接打印。

**读数判断**：输出是**一条命令行一行**。看的是**命令行本身**，不是次数。
**异常信号（推演）**：
- 同一批命令行**短时间反复出现** ⇒ 进程在反复重启（崩溃循环 / 守护进程拉起失败 / 脚本 fork 风暴）；
- 命令行里出现**不是你预期的那个程序** ⇒ 有第三方在偷偷拉进程；
- 命令行**为空或只有路径没有参数** ⇒ 触发路径可能绕过了正常封装。

**往哪查**：拿命令行的**哈希或路径**去对进程状态；对到 `execve` 频繁 ⇒ 去看服务日志与 supervisor 配置。

### 哪个进程在疯狂调系统调用

> Count syscalls by process:

```bash
bpftrace -e 'tracepoint:raw_syscalls:sys_enter { @[pid, comm] = count(); }'
```

> Count syscalls by syscall probe name:

```bash
bpftrace -e 'tracepoint:syscalls:sys_enter_* { @[probe] = count(); }'
```

**机制（两条的区别是本节的关键）**：两条探针**位置完全不同**：

| 命令 | 探针 | 特点 |
|---|---|---|
| 按进程数 | `tracepoint:raw_syscalls:sys_enter` | **统一入口**——所有系统调用共用一个 tracepoint，只有裸的 syscall id，**没有结构化参数** |
| 按调用名数 | `tracepoint:syscalls:sys_enter_*` | **每个系统调用各有自己的 tracepoint**，携带该调用专属的 `args->` 字段 |

所以：**要「谁调得多」用前者，要「调的是哪种」用后者**。想要「既知道是谁、又知道调了什么」，
只能自己拼键（如 `@[pid, comm, probe]`）——原文没给这条，本篇标为推演。

**读数判断**：

| 读数 | 含义 | 异常信号（推演） | 往哪查 |
|---|---|---|---|
| `@[pid, comm]` 的值 | 该进程的系统调用总次数 | 单进程**量级**上远超同类 ⇒ 忙轮询 / 失败重试风暴 | 该进程的代码路径；再看下面的「按调用名」缩小 |
| `@[probe]` 的值 | 每种系统调用各被调了多少次 | 某一种调用**占绝大多数** ⇒ 负载类型直接现形：`read`/`write` 主导 = I/O 型；`madvise`/`mprotect` 主导 = 内存型 | 拿到名字后换对应的专门探针（如 `sys_enter_read`） |

**推演**：这两条是**「广度扫描」的起点**，不是终点。它们告诉你「异常在哪个进程 / 哪类调用」，
不告诉你「慢在哪个函数」——那要靠 §附录 C CPU 与调度延迟 里的栈采样。

### 谁在开线程

> Trace new threads via pthread_create():

```bash
bpftrace -e 'u:/lib/x86_64-linux-gnu/libpthread-2.27.so:pthread_create {
    printf("%s by %s (%d)\n", probe, comm, pid); }'
```

**机制**：`pthread_create` 在**用户态**（libpthread 库里），所以要用 **uprobe**（`u:`），
探针目标是「库路径 + 函数名」两段式——与内核函数只需给函数名的 `kprobe:` 不同。
`printf` 直接打印：探针名、创建者进程名、创建者 PID。

**读数判断**：这是**直接打印型**输出，一行一次线程创建。
**异常信号（推演）**：
- **创建速率高** ⇒ 线程在反复创建销毁（线程池没复用 / 每次请求开一个线程 / 线程泄漏）；
- **创建者集中在一个进程** ⇒ 那个进程是线程工厂；
- **同一个 `comm` 短时间出现大量创建** ⇒ 该进程的线程数在失控增长。

**推演**：看到「线程创建很频繁」时，先确认**线程总数**是否也在涨（前者是创建速率，后者是存量）——
只涨创建速率不涨总数，说明线程在正常回收；两个都涨，才是泄漏。

## §附录 C CPU 与调度延迟

**机制**：这一域的两类命令**测的是完全不同的两段时间**，混用会得出错误结论：

| 命令族 | 测的是 | 典型形态 |
|---|---|---|
| `profile:hz:N` | **on-CPU 时间**——进程**正在跑**的时候，它在干什么 | 采样用户栈 / 内核栈 |
| `sched_switch` | **off-CPU 时间**——进程**被换下去**的时候，它卡在哪 | 上下文切换事件上的内核栈 |

### on-CPU 采样：谁在烧 CPU

> Sample running process names at 99 Hertz:

```bash
bpftrace -e 'profile:hz:99 { @[comm] = count(); }'
```

> Sample user and kernel stacks at 49 Hertz, system wide, with the process name:

```bash
bpftrace -e 'profile:hz:49 { @[kstack, ustack, comm] = count(); }'
```

> Sample user-level stacks at 49 Hertz, for PID 189:

```bash
bpftrace -e 'profile:hz:49 /pid == 189/ { @[ustack] = count(); }'
```

> Sample user-level stacks 5 frames deep at 49 Hertz, for PID 189:

```bash
bpftrace -e 'profile:hz:49 /pid == 189/ { @[ustack(5)] = count(); }'
```

> Sample user-level stacks at 49 Hertz, for processes named “mysqld”:

```bash
bpftrace -e 'profile:hz:49 /comm == "mysqld"/ { @[ustack] = count(); }'
```

**机制**：`profile:hz:N` 是**定时采样探针**——每 N Hz 在每个 CPU 上抓一次当前执行栈。
采样频率在原文里有两种取值，且**不是随意的**：

| 频率 | 原文用在 | 含义 |
|---|---|---|
| **99 Hz** | 只采 `@[comm]`（进程名） | 维度粗 ⇒ 可以采得密一点，统计更准 |
| **49 Hz** | 采 `[kstack]`、`[ustack]`（完整栈） | 维度细（栈很贵） ⇒ 频率降一半 |

**读数判断**：`@[kstack, ustack, comm]` 的**值 = 采样命中次数**，所以**值越大 = 那个栈被采到的次数越多
= 那条代码路径占用的 CPU 时间越多**。这正是 CPU 火焰图的原料。
**异常信号（推演）**：
- 某个 `ustack` 的值**遥遥领先** ⇒ 热点函数就在这条栈上；
- 同一个进程名下**很多条不同的 ustack 值接近** ⇒ 不是单点热点，而是**面状开销**（该换算法，而不是优化某个函数）；
- `kstack` 的值高但 `ustack` 的值低 ⇒ CPU 时间花在**内核态**（系统调用 / 驱动 / 缺页处理），
  该往 syscall 计数（§附录 C 进程与系统调用）查，而不是往用户代码查。

**`ustack(5)` 这个括号是本节最实用的一处细节**：只保留**栈顶 5 帧**。
**推演**：栈越深，采样成本越高、输出越难读。先用 5 帧定位到「哪几个函数」，
需要更深的上下文再去掉括号重采——**别一上来就采全栈**。

**谓词怎么选**：`/pid == 189/` 精确但**进程重启后 pid 会变**；
`/comm == "mysqld"/` 按名字，**跨重启稳定但同名进程会混在一起**。
**推演**：先用 pid 精确定位、确认是目标后，改用 comm 做长时间观测。

### off-CPU：进程被换下去时卡在哪

> Count kernel CPU scheduler tracepoints:

```bash
bpftrace -e 'tracepont:sched:* { @[probe] = count(); }'
```

> Count off-CPU kernel stacks for context switch events:

```bash
bpftrace -e 'tracepont:sched:sched_switch { @[kstack] = count(); }'
```

⚠️[纠错] 上面两条命令里的 `tracepont` 是**原文的真实笔误**，正确写法是 `tracepoint`。
照抄必然报「找不到探针」。纠错补充见文末。

**机制**：`sched:*` 是**内核调度器的 tracepoint 集合**；`sched_switch` 在**每次上下文切换**时触发，
此时抓的 `kstack` 是**被换下去那个线程当时所在的内核栈**——**它卡在哪，一目了然**。

**读数判断**：`@[kstack]` 的值 = **该内核栈上发生过多少次切换**。
**异常信号（推演）**：
- 某个 `kstack` 的值**特别高** ⇒ 大量线程**反复阻塞在同一个地方**（同一把锁、同一个设备、同一个页等待）；
- 栈顶集中在**文件系统或网络函数** ⇒ 线程在等 I/O，不是 CPU 不够（这时调 CPU 配额是白费力气，
  见 [[Chapter 6. CPUs]] 的饱和与调度器延迟）；
- 栈顶集中在**同步原语**（lock / futex / mutex 一类）⇒ 锁竞争，往 §附录 C 内存 与
  [[Chapter 8. File Systems]] 查。

**判据（本域最重要的一条）**：
**「进程慢」有两个完全不同的形态，必须先用这两条命令分开**——
**on-CPU 高**（`profile:hz:` 采到很多次）⇒ 代码本身慢；
**off-CPU 高**（`sched_switch` 采到很多次）⇒ 它在等东西，CPU 是闲的。
**只查一边，就会开错药。**

## §附录 C 内核函数插桩

**机制**：这一域全是**给内核函数插桩**，用 `kprobe:`（入口）与 `kretprobe:`（返回）。

> Count kernel function calls beginning with “vfs_”:

```bash
bpftrace -e 'kprobe:vfs_* { @[func] = count(); }'
```

> Count VFS calls for PID 181:

```bash
bpftrace -e 'kprobe:vfs_* /pid == 181/ { @[probe] = count(); }'
```

> Count SCSI driver function calls:

```bash
bpftrace -e 'kprobe:scsi* { @[func] = count(); }'
```

> Count all TCP functions (adds high overhead to TCP):

```bash
bpftrace -e 'k:tcp_* { @[func] = count(); }'
```

**通配符是这一域的核心手法**：`vfs_*` 一次插桩**整个 VFS 函数族**，`scsi*` 整个 SCSI 驱动，
`tcp_*` 整个 TCP 协议栈。**原文自己给 `tcp_*` 加了警告**：

> Count all TCP functions (adds high overhead to TCP):

**读数判断**：`@[func]` 的值 = **该内核函数被进入的次数**。
**异常信号（推演）**：
- 某个函数的值**远高于同族其他函数** ⇒ 这条路径被反复走（可能是**绕路**：本该走批量接口却逐个调用）；
- **某函数的值异常高但耗时不高** ⇒ 调用频繁但单次便宜（典型是**小而多**）；
- **函数之间比例失衡** ⇒ 比例本身就是线索（读多写少 / 元数据操作远多于数据操作 ⇒ 工作集不匹配）。

**开销纪律（推演，原文只对 `tcp_*` 明确警告）**：**通配符越宽、探针越多，开销越大**——
每个探针都要在函数入口挂一次。所以：

| 想做的事 | 该用的宽度 | 理由 |
|---|---|---|
| 摸清「这条路径上有哪些函数」 | 先 `bpftrace -l 'kprobe:vfs_*'` **数一数匹配几个** | 探针数直接决定开销 |
| 定位「是哪个函数」 | 先用**窄**探针（`kprobe:vfs_read`）验证猜想 | 便宜的假设验证永远先做 |
| 全量摸底 | 宽探针 + **短时间窗口** | 宽探针只在「短窗口」里可接受 |

**判据**：**先用 `-l` 数探针个数，再决定要不要插。** 匹配数上百的通配符，
在生产环境里就是一次自伤式采样。

## §附录 C 内存

这一域回答四个问题：**堆在哪儿涨、页从哪儿缺、回收在做什么、有没有在换页。**

### 堆：谁在申请内存

> Sum libc malloc() request bytes by user stack and process (high overhead):

```bash
bpftrace -e 'u:/lib/x86_64-linux-gnu/libc.so.6:malloc {
    @[ustack, comm] = sum(arg0); }'
```

> Sum libc malloc() request bytes by user stack for PID 181 (high overhead):

```bash
bpftrace -e 'u:/lib/x86_64-linux-gnu/libc.so.6:malloc /pid == 181/ {
    @[ustack] = sum(arg0); }'
```

> Show libc malloc() request bytes by user stack for PID 181 as a power-of-2 histogram (high overhead):

```bash
bpftrace -e 'u:/lib/x86_64-linux-gnu/libc.so.6:malloc /pid == 181/ {
    @[ustack] = hist(arg0); }'
```

**机制**：`malloc` 在用户态 libc 里 ⇒ **uprobe**；`arg0` 是**第一个参数，即申请字节数**；
`sum(arg0)` 是**累加申请量**（不是次数！）。原文特别标了 **high overhead**——
用户态函数调用极其频繁，插桩点几乎等于**每一次分配**。

**读数判断**：
- `@[ustack, comm] = sum(arg0)` 的**值是字节数**（不是次数）⇒ 直接和进程的 RSS 对账；
- `hist(arg0)` 是 **2 的幂直方图**（原文写明 power-of-2）⇒ 看**申请尺寸的分布形状**。

**异常信号（推演）**：

| 读数 | 异常的样子 | 往哪查 |
|---|---|---|
| 某条 ustack 的**字节数**占比极高 | 内存被**一处**吃掉了 | 那条栈上的分配逻辑（缓存？批量结构？无界容器？） |
| `hist` 集中在**小尺寸桶** | 大量**小对象**分配 ⇒ 分配器压力与碎片 | 对象数量级、是否该换池/arena |
| `hist` 集中在**大尺寸桶** | 少数**大块**分配 ⇒ 多半是缓冲区/数组 | 缓冲区是否可复用、是否有超量预分配 |
| 字节数**涨但不降** | 分配量与释放量失衡 ⇒ 有东西只申请不释放 | 该分配点的生命周期 |

**推演**：`sum` 与 `hist` 是**两个维度的问题**——`sum` 答「总量」，`hist` 答「总量由什么形状构成」。
两个一起看才能区分「一个大块」和「很多小块」。

### 堆扩张与页缺失

> Count process heap expansion (brk(2)) by code path:

```bash
bpftrace -e 'tracepoint:syscalls:sys_enter_brk { @[ustack, comm] = count(); }'
```

> Count page faults by process:

```bash
bpftrace -e 'software:page-fault:1 { @[comm, pid] = count(); }'
```

> Count user page faults by user-level stack trace:

```bash
bpftrace -e 't:exceptions:page_fault_user { @[ustack, comm] = count(); }'
```

**机制**：三条量三件不同的事：

| 命令 | 机制 | 量的是 |
|---|---|---|
| `sys_enter_brk` | `brk(2)` 是**进程向内核要堆空间**（传统分配器扩堆用） | 堆**扩张**的次数，按**代码路径** |
| `software:page-fault:1` | **software 探针**挂在内核的页错误事件上 | 页错误**总数**，按进程 |
| `t:exceptions:page_fault_user` | **tracepoint** `page_fault_user` | **用户态**页错误，按**用户栈** |

**推演**：`software:` 走的是内核里的软件事件计数（不区分用户态/内核态，
末尾的 `1` 是错误码过滤位，**具体筛的是哪一类请实测确认**）；
`t:exceptions:page_fault_user` 只数**用户态**触发的页错误，且**带用户栈**——所以它能定位到「哪段代码在碰新页」。

**读数判断与异常信号（推演）**：

| 读数 | 异常的样子 | 往哪查 |
|---|---|---|
| `@[ustack, comm]`（brk）值高 | 堆**反复扩张** ⇒ 分配器在反复要内存 | 该路径的分配/释放配比 |
| `@[comm, pid]`（页错误）值高 | 页错误**速率**高 ⇒ 内存**被换出过**或在反复触碰新页 | RSS 与可用内存；再看下面的 swapin |
| `@[ustack, comm]`（用户页错误）值高 | 集中在**少数几条代码路径** ⇒ 这些路径在**反复触碰未驻留的页** | 那段代码的工作集大小 |

**判据**：**页错误分不清「小错」和「大错」**（前者不落盘、后者要读盘）。
原文给了 `software:page-fault:1` 这个总数，但没有区分。
**推演**：要区分**主要缺页**，得用 `perf` 的 page-fault 事件或 `sar -B` 的 `majflt/s`
（见 [[附录B-Appendix B： sar Summary]]），**别拿 bpftrace 这个总数去推磁盘压力**。

### 回收、换页、迁移与压实

> Count vmscan operations by tracepoint:

```bash
bpftrace -e 'tracepoint:vmscan:* { @[probe]++; }'
```

> Count swapins by process:

```bash
bpftrace -e 'kprobe:swap_readpage { @[comm, pid] = count(); }'
```

> Count page migrations:

```bash
bpftrace -e 'tracepoint:migrate:mm_migrate_pages { @ = count(); }'
```

> Trace compaction events:

```bash
bpftrace -e 't:compaction:mm_compaction_begin { time(); }'
```

> Sum kernel kmem cache allocation bytes by kernel stack trace:

```bash
bpftrace -e 't:kmem:kmem_cache_alloc { @bytes[kstack] = sum(args->bytes_alloc); }'
```

**机制**：这五条量的是**内核在内存紧张时的四条动作线**：

| 命令 | 内核在做什么 | 读数 |
|---|---|---|
| `vmscan:*` | **回收扫描**——内核在找可回收的页 | `@[probe]` = 各扫描阶段各跑了多少次 |
| `kprobe:swap_readpage` | **换入**——把页从 swap 读回内存 | `@[comm, pid]` = 谁在被换入 |
| `mm_migrate_pages` | **页迁移**——在 NUMA 节点之间搬页 | `@` = 总迁移次数 |
| `mm_compaction_begin` | **压实**——整理碎片以便申请大页 | `time()` = **压实事件的时间戳** |
| `kmem_cache_alloc` | **内核 slab 分配** | `@bytes[kstack]` = **内核态**分配了多少字节 |

**读数判断与异常信号（推演）**：

| 读数 | 异常的样子 | 往哪查 |
|---|---|---|
| `vmscan` 频繁 | 内核**一直在回收** ⇒ 内存压力真实存在 | 可用内存、swap 使用量（[[Chapter 7. Memory]]） |
| `swap_readpage` 按进程计数高 | **某个进程在被换入** ⇒ 它的工作集超了 | 该进程 RSS 与内存配额 |
| `mm_migrate_pages` 次数高 | 页在 **NUMA 节点间来回搬** ⇒ 内存亲和性差 | CPU/内存亲和性设置、first-touch 策略 |
| `mm_compaction_begin` 时间戳密集 | **压实风暴** ⇒ 碎片化严重 | 透明大页配置、分配尺寸分布 |
| `@bytes[kstack]` 某个栈的值大 | **内核态**在某条栈上分配大量 slab ⇒ 驱动或子系统泄漏 | 那条内核栈对应的子系统 |

**推演**：这四条是**「内存紧张」的下游症状**。真正该先看的是**可用内存与 swap 存量**
（[[Chapter 7. Memory]]、[[附录B-Appendix B： sar Summary]]）；bpftrace 这几条告诉你的是
「紧张到什么程度、谁在承受」。

## §附录 C 文件系统

**机制**：这一域有一个贯穿全篇的对照——**请求量 vs 实际量**。

> Trace files opened via openat(2) with process name:

```bash
bpftrace -e 't:syscalls:sys_enter_openat { printf("%s %s\n", comm,
    str(args->filename)); }'
```

> Count read syscalls by syscall type:

```bash
bpftrace -e 'tracepoint:syscalls:sys_enter_*read* { @[probe] = count(); }'
```

> Count write syscalls by syscall type:

```bash
bpftrace -e 'tracepoint:syscalls:sys_enter_*write* { @[probe] = count(); }'
```

> Show the distribution of read() syscall request sizes:

```bash
bpftrace -e 'tracepoint:syscalls:sys_enter_read { @ = hist(args->count); }'
```

> Show the distribution of read() syscall read bytes (and errors):

```bash
bpftrace -e 'tracepoint:syscalls:sys_exit_read { @ = hist(args->ret); }'
```

> Count read() syscall errors by error code:

```bash
bpftrace -e 't:syscalls:sys_exit_read /args->ret < 0/ { @[- args->ret] = count(); }'
```

**机制（请求量 vs 实际量是本域的核心）**：原文用**同一个 `read`，在入口和出口各插一次**：

| 命令 | 插桩点 | 量的是 |
|---|---|---|
| `sys_enter_read` + `hist(args->count)` | **入口** | **请求了多少字节**（申请量） |
| `sys_exit_read` + `hist(args->ret)` | **出口** | **实际读回多少字节**（含错误，`ret<0`） |
| `sys_exit_read /args->ret < 0/` | **出口 + 过滤** | **按错误码分组**的失败次数 |

`args->ret` 是**返回值**：负数表示失败，`- args->ret` 把 errno **取正**当键。

**读数判断与异常信号（推演）**：

| 读数 | 异常的样子 | 往哪查 |
|---|---|---|
| `sys_enter_read` 的直方图 | 申请量集中在**小桶** ⇒ 大量**小读** | 该进程的缓冲策略（是不是没缓冲） |
| `sys_exit_read` 的直方图 | **出现负值桶** ⇒ 读**失败**混在直方图里 | 拆出错误码看（下一条） |
| 错误码分组的值 | 某一类 errno **占绝大多数** ⇒ 单一原因（权限？路径？设备没了？） | 该 errno 的含义；openat 的失败路径 |
| `@[probe]`（read/write 分类） | 某类调用占绝大多数 | 回到 §附录 C 进程与系统调用 的按名计数 |
| `openat` 的逐行输出 | 同一路径**反复被打开** ⇒ 缓存没起作用 / 配置反复重载 | 该路径的打开者 |

**推演**：**「申请了很多但读回很少」是这一域最有价值的一个信号**——
它直接指向**短读**（read 返回的比请求的少），这是很多应用层「数据不完整」的根因。

### 文件系统内部：谁在做、做了什么

> Count ext4 tracepoints:

```bash
bpftrace -e 'tracepoint:ext4:* { @[probe] = count(); }'
```

> Count xfs tracepoints:

```bash
bpftrace -e 'tracepoint:xfs:* { @[probe] = count(); }'
```

> Count ext4 file reads by process name and user-level stack:

```bash
bpftrace -e 'kprobe:ext4_file_read_iter { @[ustack, comm] = count(); }'
```

> Count dcache references by process name and PID:

```bash
bpftrace -e 'kprobe:lookup_fast { @[comm, pid] = count(); }'
```

> Trace ZFS spa_sync() times:

```bash
bpftrace -e 'kprobe:spa_sync { time("%H:%M:%S ZFS spa_sync()\n"); }'
```

**机制**：这五条分三个层次：

| 层次 | 命令 | 机制 |
|---|---|---|
| **文件系统动作** | `tracepoint:ext4:*` / `xfs:*` | 整个文件系统的 tracepoint 集合，**按探针名**分类计数 ⇒ 文件系统内部做了哪些事 |
| **文件读路径** | `kprobe:ext4_file_read_iter` | 真正的**文件读**，带**用户栈** ⇒ 哪个进程、哪段代码在读文件 |
| **路径解析** | `kprobe:lookup_fast` | dentry 缓存的**快速查找**路径 ⇒ 路径解析走了多少次缓存命中 |
| **ZFS 事务** | `kprobe:spa_sync` | ZFS 的**同步点**，`time()` 打印**时刻** ⇒ 同步节奏 |

**读数判断与异常信号（推演）**：

| 读数 | 异常的样子 | 往哪查 |
|---|---|---|
| `@[probe]`（ext4/xfs 动作分布） | 某一类动作**占绝大多数** ⇒ 元数据操作压倒数据操作 | 工作集与挂载参数（[[Chapter 8. File Systems]]） |
| `@[ustack, comm]`（ext4 读） | 集中在一个进程 ⇒ **它**在狂读文件 | 该进程的读路径（有没有该用 mmap 的地方） |
| `@[comm, pid]`（lookup_fast） | 次数高但实际读很少 ⇒ **路径解析**成了热点 | 路径是否过长、是否反复 chdir |
| `spa_sync` 时间戳 | 同步点**密集** ⇒ 事务提交频繁 ⇒ 写延迟毛刺 | 提交频率与批量大小 |

**推演**：`lookup_fast` 的计数高**本身不是坏事**（说明缓存在生效），但**如果它高到成为热点**，
说明路径解析的开销盖过了实际数据 I/O——这是**换更短路径 / 少 chdir** 的信号。

## §附录 C 磁盘

**机制**：这一域从**块层**往下看，四条命令覆盖了「块 I/O 事件、请求特征、错误、SCSI 层」。

> Count block I/O tracepoints events:

```bash
bpftrace -e 'tracepoint:block:* { @[probe] = count(); }'
```

> Summarize block I/O size as a histogram:

```bash
bpftrace -e 't:block:block_rq_issue { @bytes = hist(args->bytes); }'
```

> Count block I/O request user stack traces:

```bash
bpftrace -e 't:block:block_rq_issue { @[ustack] = count(); }'
```

> Count block I/O type flags:

```bash
bpftrace -e 't:block:block_rq_issue { @[args->rwbs] = count(); }'
```

> Trace block I/O errors with device and I/O type:

```bash
bpftrace -e 't:block:block_rq_complete /args->error/ {
    printf("dev %d type %s error %d\n", args->dev, args->rwbs, args->error); }'
```

> Count SCSI opcodes:

```bash
bpftrace -e 't:scsi:scsi_dispatch_cmd_start { @opcode[args->opcode] =
    count(); }'
```

> Count SCSI result codes:

```bash
bpftrace -e 't:scsi:scsi_dispatch_cmd_done { @result[args->result] = count(); }'
```

**机制**：

| 命令 | 插桩点 | 量的是 |
|---|---|---|
| `block:*` | 块层 tracepoint 集合 | 块层**事件总量**，按事件名 |
| `block_rq_issue` + `hist(args->bytes)` | 请求**下发**时 | 请求**大小分布** |
| `block_rq_issue` + `@[ustack]` | 请求下发时 | **谁**发起的 I/O（用户栈） |
| `block_rq_issue` + `@[args->rwbs]` | 请求下发时 | **读写类型标志**（R/W/同步位） |
| `block_rq_complete` + `/args->error/` | 请求**完成**时（**只在出错时**） | 设备号 + 类型 + 错误码 |
| `scsi_dispatch_cmd_start` | SCSI 命令**发出**时 | **操作码**分布（读？写？sync？） |
| `scsi_dispatch_cmd_done` | SCSI 命令**返回**时 | **结果码**分布（成功/各类错误） |

**推演**：`block_rq_issue` 上插了**三个探针**（大小 / 用户栈 / 类型标志）——它们是**同一个时刻的三种切面**，
说明这一时刻的请求既要知道**多大**、又要知道**谁发的**、还要知道**是读是写**。

**读数判断与异常信号（推演）**：

| 读数 | 异常的样子 | 往哪查 |
|---|---|---|
| `hist(args->bytes)` 集中在**小桶** | 大量**小块随机 I/O** ⇒ 顺序性差，队列深度上不去 | 合并 I/O（[[Chapter 9. Disks]]） |
| `@[args->rwbs]` 里**写**占比高 | **写放大**嫌疑 | 刷盘频率、是否该批量提交 |
| `@[ustack]` 集中在一个栈 | **一个**调用点在狂发 I/O | 该调用点（循环里逐条读？） |
| `block_rq_complete` 有输出 | **块层真出错了** ⇒ 设备/路径/驱动问题 | 输出里的 `dev` 与 `error`：先定位盘，再看错误码 |
| `@opcode` 集中在某操作码 | 请求**类型单一**（全是读 or 全是写） | 负载类型；写密集 ⇒ 关注刷盘 |
| `@result` 出现错误码 | **SCSI 层失败** ⇒ 低于块层，往硬件/驱动查 | 错误码含义、设备健康 |

**判据**：**块层错误要往上追一层**。`block_rq_complete` 打印出的 `dev` 是**设备号**，
把它和 `lsblk`/`iostat` 对上，才知道是哪块盘；`@result` 的结果码再往下定位到 SCSI 语义。

## §附录 C 网络

**机制**：这一域最多（20 条），但骨架只有三档：**socket 层 → 传输层（TCP/UDP）→ 设备层**。

### socket 层：谁在收发

> Count socket accept(2)s by PID and process name:

```bash
bpftrace -e 't:syscalls:sys_enter_accept* { @[pid, comm] = count(); }'
```

> Count socket connect(2)s by PID and process name:

```bash
bpftrace -e 't:syscalls:sys_enter_connect { @[pid, comm] = count(); }'
```

> Count socket connect(2)s by user stack trace:

```bash
bpftrace -e 't:syscalls:sys_enter_connect { @[ustack, comm] = count(); }'
```

> Count socket send/receives by direction, on-CPU PID, and process name:

```bash
bpftrace -e 'k:sock_sendmsg,k:sock_recvmsg { @[func, pid, comm] = count(); }'
```

> Count socket send/receive bytes by on-CPU PID and process name:

```bash
bpftrace -e 'kr:sock_sendmsg,kr:sock_recvmsg /(int32)retval > 0/ { @[pid, comm] =
    sum((int32)retval); }'
```

**机制**：这一段把「谁、方向、多少」三件事拆成三条命令：

| 命令 | 键 | 值 |
|---|---|---|
| accept / connect 计数 | `[pid, comm]` | **次数** |
| connect 按栈 | `[ustack, comm]` | 次数，**带调用路径** |
| sock 收发计数 | `[func, pid, comm]` | 次数，**`func` 区分方向**（`sock_sendmsg` vs `sock_recvmsg`） |
| sock 收发字节 | `[pid, comm]` | **字节数**（`kr:` 拿到 `retval`） |

**推演**：`on-CPU PID` 这个措辞很关键——它指的是**执行这次收发动作的那个线程**，
**不一定等于发起连接的进程**（异步模型里收发在别的线程上做）。
所以**「谁在收」要看 `comm`，不要只看 `pid`**。

**读数判断与异常信号（推演）**：

| 读数 | 异常的样子 | 往哪查 |
|---|---|---|
| `accept` 计数高 | **连接建立速率**高 | 是不是在反复重连（对端拒绝？keepalive 配错？） |
| `connect` 按栈集中 | **一条代码路径**在狂建连接 | 连接池失效 / 重试策略 |
| `[func, pid, comm]` 里**收发比**失衡 | 发远多于收 ⇒ **对端不读**（背压） | 对端消费能力；或发送缓冲被填满 |
| `[pid, comm]` 的**字节数**巨大 | 单个进程**带宽独占** | 该进程是不是在做批处理/转储 |
| `(int32)retval > 0` 的过滤 | 过滤掉了**失败与零字节**的收发 | 失败收发要看 `sys_exit_*` 的错误码 |

### 传输层：TCP 与 UDP

> Count TCP connects by on-CPU PID and process name:

```bash
bpftrace -e 'k:tcp_v*_connect { @[pid, comm] = count(); }'
```

> Count TCP accepts by on-CPU PID and process name:

```bash
bpftrace -e 'k:inet_csk_accept { @[pid, comm] = count(); }'
```

> Count TCP send/receives by on-CPU PID and process name:

```bash
bpftrace -e 'k:tcp_sendmsg,k:tcp_recvmsg { @[func, pid, comm] = count(); }'
```

> TCP send bytes as a histogram:

```bash
bpftrace -e 'k:tcp_sendmsg { @send_bytes = hist(arg2); }'
```

> TCP receive bytes as a histogram:

```bash
bpftrace -e 'kr:tcp_recvmsg /retval >= 0/ { @recv_bytes = hist(retval); }'
```

> Count TCP retransmits by type and remote host (assumes IPv4):

```bash
bpftrace -e 't:tcp:tcp_retransmit_* { @[probe, ntop(2, args->saddr)] = count(); }'
```

> Count all TCP functions (adds high overhead to TCP):

```bash
bpftrace -e 'k:tcp_* { @[func] = count(); }'
```

> Count UDP send/receives by on-CPU PID and process name:

```bash
bpftrace -e 'k:udp*_sendmsg,k:udp*_recvmsg { @[func, pid, comm] = count(); }'
```

> UDP send bytes as a histogram:

```bash
bpftrace -e 'k:udp_sendmsg { @send_bytes = hist(arg2); }'
```

> UDP receive bytes as a histogram:

```bash
bpftrace -e 'kr:udp_recvmsg /retval >= 0/ { @recv_bytes = hist(retval); }'
```

**机制**：TCP 这一组的骨架是**「三次握手类 / 收发类 / 字节类 / 重传类」**：

| 组 | 探针 | 说明 |
|---|---|---|
| 建连 | `k:tcp_v*_connect`、`k:inet_csk_accept` | **通配符 `tcp_v*_connect` 同时覆盖 v4 与 v6** |
| 收发次数 | `k:tcp_sendmsg,k:tcp_recvmsg` | 键里的 `func` 区分方向 |
| 收发字节 | `arg2`（发送长度）/ `retval`（接收长度） | **入参用 `k:`，返回值用 `kr:`** |
| 重传 | `t:tcp:tcp_retransmit_*` | 键 = **探针名 + 远端地址** |
| 全量 | `k:tcp_*` | 原文明确警告 **high overhead** |

**`ntop(2, args->saddr)`** 是本域唯一需要解释的一处：`ntop` 把**二进制地址转成点分字符串**，
`2` 是**地址族 AF_INET**（IPv4）——所以原文在旁边标注了 **assumes IPv4**。
**推演**：IPv6 环境下这条命令要么不触发、要么给出不可读的地址，**要量 IPv6 得换 `ntop(10, ...)`**
（AF_INET6）——原文没给这一条，属本篇推演。

**读数判断与异常信号（推演）**：

| 读数 | 异常的样子 | 往哪查 |
|---|---|---|
| `tcp_v*_connect` 值高 | **建连速率**高（每次握手都有成本） | 连接池、重试退避 |
| `tcp_sendmsg` 的 `hist(arg2)` | 集中在**小桶** ⇒ **小包海量** ⇒ 带宽被包头吃掉 | 批量/缓冲 |
| `tcp_recvmsg` 的 `hist(retval)` | 出现**大桶** ⇒ 少量大包；**集中在 0 附近** ⇒ 大量空读 | 读缓冲大小 |
| `tcp_retransmit_*` 按远端地址分组 | **某个远端**重传集中 ⇒ 到该主机的路径有问题（丢包 / 对端慢） | 网络路径，不是应用 |
| `k:tcp_*` 全量计数 | **只作短窗口摸底**，绝不做长时间采样 | 见 §附录 C 内核函数插桩 的探针数纪律 |

### 设备层：网卡

> Count transmit kernel stack traces:

```bash
bpftrace -e 't:net:net_dev_xmit { @[kstack] = count(); }'
```

> Show receive CPU histogram for each device:

```bash
bpftrace -e 't:net:netif_receive_skb { @[str(args->name)] = lhist(cpu, 0, 128, 1); }'
```

> Count ieee80211 layer functions (adds high overhead to packets):

```bash
bpftrace -e 'k:ieee80211_* { @[func] = count()'
```

> Count all ixgbevf device driver functions (adds high overhead to ixgbevf):

```bash
bpftrace -e 'k:ixgbevf_* { @[func] = count(); }'
```

> Count all iwl device driver tracepoints (adds high overhead to iwl):

```bash
bpftrace -e 't:iwlwifi:*,t:iwlwifi_io:* { @[probe] = count(); }'
```

**机制**：

| 命令 | 量的是 |
|---|---|
| `net_dev_xmit` + `@[kstack]` | **发送路径**上的内核栈分布 ⇒ 发送卡在哪一层 |
| `netif_receive_skb` + `lhist(cpu, 0, 128, 1)` | **每块网卡**的**收包落在哪些 CPU 上**（0–127，步长 1） |
| `ieee80211_*` / `ixgbevf_*` / `iwlwifi:*` | **无线栈 / 具体网卡驱动**的函数与 tracepoint 计数 |

**`lhist(cpu, 0, 128, 1)` 是本域最实用的一条**：它把「收包在哪个 CPU」画成**直方图**。
**推演**：读法是**看分布形状**——
**全挤在一两个 CPU 上** ⇒ 收包没有分散（**RPS/RSS 没开或没配好**），
那个 CPU 会成为瓶颈；**均匀铺开** ⇒ 分布正常。
这是**网卡软中断亲和性**问题的一手证据。

**推演**：`ieee80211_*` / `ixgbevf_*` / `iwlwifi:*` 三条都是**驱动级全量计数**，
原文对后两条都明确标了 **high overhead**。**推演**：驱动级探针在**生产流量大时开销极高**，
只在**低流量窗口**或**实验环境**用；无线那两条（`ieee80211_*`）在**无线网卡上才有意义**。

## 纠错补充

**本篇严格照抄原文命令**，但原文稿本身有 3 处缺陷。三处都**不改原文**，在此列明。

### (a) 65 行 epub 图片占位符混在命令之间

**错误点**：原文稿里**每条命令的代码块之前都跟着一行** `Click here to view code image`
（全文 65 行；67 条命令里只有 `bpftrace -l 't:kmem:*'` 与 `bpftrace -l 't:*:mm_*'` 这两条没有）。

**修正依据**：这是 epub 抽取时**代码块配图**留下的占位文本，不属于命令本身。

**正确结论**：**抄命令时不要把它一起复制进终端。**（本篇的代码块里已全部剔除这一行。）

### (b) `tracepoint` 被拼成 `tracepont`（第 92、99 行）

**错误点**：原文的两条调度器命令写成 `tracepont:sched:*` 与 `tracepont:sched:sched_switch`。

**修正依据**：bpftrace 的 tracepoint 探针类型名是 `tracepoint`（快捷写法 `t:`），
没有 `tracepont` 这个类型；同一条命令用 `t:` 也能写成 `t:sched:*`。

**正确结论**：**正确写法是 `tracepoint:sched:*` 与 `tracepoint:sched:sched_switch`**，
照抄原文必然报「找不到探针」。这是**原文的真实笔误**，不是抽取错误。

### (c) 第 500 行命令残缺（少一个引号、一个分号）

**错误点**：原文写的是 `bpftrace -e 'k:ieee80211_* { @[func] = count()'`——收尾的 `'` 与 `;` 都不在了。

**修正依据**：同域其他 `kprobe:` 命令的收尾都是 `{ @[func] = count(); }'`。

**正确结论**：**完整写法是 `bpftrace -e 'k:ieee80211_* { @[func] = count(); }'`**，
照抄原文会因引号未闭合而语法错误。

## 章末选型判据

| 你要回答的问题 | 用哪条 / 哪一族 | 别用什么 |
|---|---|---|
| 「这个现象该插哪个探针」 | 先 `bpftrace -l '<探针模式>'` **确认存在**，再插桩 | 直接抄命令就跑（原文明说 many may not work right away） |
| 「是 CPU 不够，还是在等」 | **两条都跑**：`profile:hz:49`（on-CPU）+ `sched_switch`（off-CPU） | 只跑其中一条就下结论（见 [[Chapter 6. CPUs]]） |
| 「CPU 热点在哪个函数」 | `profile:hz:49 { @[kstack, ustack, comm] = count(); }`，先用 `ustack(5)` 限深 | 一上来采全栈（成本高、输出难读） |
| 「谁在狂调系统调用」 | `raw_syscalls:sys_enter` 数**总数**，再 `syscalls:sys_enter_*` 数**分类** | 只看总数就去猜是哪类调用 |
| 「内存被谁吃掉了」 | `u:...:malloc` + `sum(arg0)`（**字节数**）+ `hist(arg0)`（**尺寸分布**） | 把 `sum` 的值当次数读 |
| 「是缺页还是换页」 | `software:page-fault:1`（**计数**）+ `kprobe:swap_readpage`（**谁在换入**） | 用页错误计数去推磁盘压力（没区分主要/次要缺页） |
| 「读多还是读回得少」 | `sys_enter_read`（**请求量**）对比 `sys_exit_read`（**实际量**） | 只看其中一个 |
| 「磁盘在忙还是在错」 | `block_rq_issue` 看**特征**（大小/读写/谁），`block_rq_complete` 看**错误** | 只看 `block:*` 的总量（看不出错在哪） |
| 「网络慢在哪一跳」 | socket 计数 → TCP 收发字节 → `tcp_retransmit_*`（**按远端地址**） | 只看应用侧超时（超时不分丢包还是对端慢） |
| 「网卡收包有没有压在单核」 | `netif_receive_skb` + `lhist(cpu, 0, 128, 1)` | 只看网卡吞吐量（不暴露 CPU 分布） |
| 「要不要用这个宽探针」 | 先 `-l` **数匹配数**，再决定窗口长度 | `k:tcp_*` / `k:ixgbevf_*` 这类**通配全量**做长时间采样（原文已警告 high overhead） |
| 「学 bpftrace 语法」 | 本附录 67 条 + [[Chapter 15. BPF]] §15.2 | 只读命令不读语法（会连`kr:` 与 `k:` 的差别都分不清） |
| 「这批读数算不算异常」 | **没有阈值可抄**——原文一条阈值都没给，阈值必须自己按基线定 | 拿本附录当判据表（它是**量什么**的索引，不是**正常多少**的字典） |
