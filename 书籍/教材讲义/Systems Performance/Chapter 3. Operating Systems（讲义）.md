---
title: Chapter 3. Operating Systems（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - Systems Performance
  - 性能工程
  - 操作系统
  - 系统调用
  - 调度器
source: "《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg，Addison-Wesley 2020，英文原版）第 3 章"
companion: "[[03-Chapter 3. Operating Systems]]"
---

# 第 3 章 讲义：CPU 到底在等什么——四个状态

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[03-Chapter 3. Operating Systems|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：`otf-server/src/net/tcp_transport.cpp` —— `epoll_wait` 与读写系统调用的**实际频率**，以及由此产生的模式切换成本。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原章。

---

## 0. 开局：一次请求的四段时间

抓一次包看本仓库的服务器：

```
00:00.000  客户端 → 服务器   攻击包（5 字节）
00:00.001  服务器            epoll_wait 返回，读到数据
00:00.001  服务器            解码 → 投递 → 更新 sim
00:00.002  服务器 → 客户端   快照广播（若干字节）
```

这 2 毫秒里，CPU 大部分时间**什么也没做**——它在等 socket 可读。

如果换成同步阻塞的写法（每个连接一个线程，`read()` 阻塞），那这 2 毫秒里会多出**几千次**不必要的线程切换：别的线程的定时器到期 → 内核唤醒 → 上下文切换 → 空转 → 再被切换回来。

**性能问题的九成不是「CPU 不够」，而是「CPU 在等」。** 而「等」这件事发生在内核里，你从用户态看不见。

本章要给你的是**看懂「等」的工具箱**：内核态与用户态、系统调用、中断、时钟与 idle、调度器、缓存层次。读完这一章，你应该能回答一个问题：**我的进程此刻到底在干什么？**

---

## 1. §3.1 Terminology：先把词定死

原文在章首给了一份速查表。这几个词后面每节都要用，所以先钉住：

| 词 | 原文定义 |
|---|---|
| **Operating system** | 让系统能启动并执行程序的软件与文件，包含内核、管理工具、系统库 |
| **Kernel** | 管理系统的程序（依内核模型而定，管硬件设备、内存、CPU 调度），运行在**特权 CPU 模式**即 kernel mode |
| **Process** | 执行程序的 OS 抽象与环境。程序跑在 user mode，通过**系统调用或陷入内核**访问内核能力 |
| **Thread** | 可被调度到 CPU 上运行的执行上下文。内核有多个线程，一个进程含一个或多个 |
| **Task** | **Linux 的可运行实体**：可以指单线程进程、多线程进程里的一个线程，或内核线程 |
| **Context switch** | 从跑一个线程/进程切到另一个。这是内核 CPU 调度器的正常职能，涉及**换一组运行寄存器** |
| **Mode switch** | 内核态与用户态之间的切换 |
| **System call (syscall)** | 用户程序请求内核执行特权操作的**定义良好的协议** |
| **Processor** | 物理芯片（包含一个或多个 CPU）。别和 process 搞混 |
| **Trap** | 给内核的信号，请求内核例程（特权动作）。类型包括系统调用、处理器异常、中断 |
| **Hardware interrupt** | 物理设备发给内核的信号，通常是请求服务 I/O。**中断是 trap 的一种** |

这张表里最容易被搞混、也最影响性能判断的是**最后三行的关系**：

```
              ┌──────────────────────────┐
              │  Trap（给内核的信号）      │
              ├───────────┬──────────────┤
              │ 系统调用    │ 处理器异常    │ 中断 ← trap 的一种
              └───────────┴──────────────┘
```

**一个系统调用 = 一次 trap = 一次 mode switch**（不一定伴随 context switch）。这是本节的核心。

---

## 2. §3.2.2 Kernel and User Modes：模式切换与上下文切换

### 2.1 两种特权级

内核跑在 **kernel mode**，可以直接访问硬件、执行特权指令；它**仲裁设备访问**以支持多任务，防止进程与用户在未授权时互相访问数据。

用户程序跑在 **user mode**，需要特权操作时通过**系统调用**请求内核。

x86 提供 4 个特权环（0–3），通常只用两三个：user mode、kernel mode、以及有的话 hypervisor。特权指令只允许在 kernel mode 执行；在 user mode 执行会触发**异常**，由内核处理（比如产生 permission denied）。

### 2.2 关键区分：mode switch ≠ context switch

原文这一段是全章最该记住的：

> Switching between kernel and user modes is a mode switch.

> All system calls mode switch. Some system calls also context switch: those that are blocking, such as for disk and network I/O, will context switch so that another thread can run while the first is blocked.

画成表：

| | mode switch | context switch |
|---|---|---|
| 换的是什么 | CPU 的特权级 | **运行的那一组线程** |
| 什么时候发生 | 每次系统调用、每次中断 | 调度器换线程时 |
| 代价量级 | 小（几个 CPU 周期级） | 更大（换寄存器、可能 TLB/缓存抖动） |

原文接着列出**减少模式切换的几种优化**：

1. **用户态系统调用**：某些系统调用可以在用户态库里实现。Linux 导出 **vDSO**（虚拟动态共享对象），映射进进程地址空间，里面有 `gettimeofday(2)`、`getcpu(2)` 这类系统调用。
2. **内存映射**：用于需求分页，也可做数据存储与其他 I/O，**避开系统调用开销**。
3. **内核旁路**：用户态程序直接访问设备，绕过系统调用与常规内核路径。原文举例 **DPDK**（数据平面开发套件）。
4. **内核态应用**：如 TUX web 服务器（`Lever 00`），以及原文 `Figure 3.2` 里的扩展 BPF。

还有一个容易忽略的细节：

> Kernel and user mode have their own software execution contexts, including a stack and registers.

**内核栈与用户栈是分开的。** 某些架构（如 SPARC）内核还有独立地址空间，于是模式切换还得换虚拟内存上下文。

> （推演）这一节的实践含义是：**每一次 `read()` / `write()` / `epoll_wait()` 都是一次模式切换**。所以「系统调用次数」本身就是一个性能指标——这正是本篇实战锚点要算的东西。

### 2.3 §3.2.3 System Calls：系统调用

系统调用是**请求内核执行特权例程**的协议。原文说内核维护者有意**保持系统调用数量尽量少**，好让内核简单（Unix 哲学；`Thompson 78`）；更复杂的接口在用户态用系统库搭起来（libc / glibc）。

原文的 `Table 3.1` 关键系统调用：

| 系统调用 | 说明 |
|---|---|
| `read(2)` / `write(2)` | 读 / 写字节 |
| `open(2)` / `close(2)` | 打开 / 关闭文件 |
| `fork(2)` / `clone(2)` | 创建进程 / 创建进程或线程 |
| `exec(2)` | 执行新程序 |
| `connect(2)` / `accept(2)` | 连接主机 / 接受连接 |
| `stat(2)` | 取文件统计信息 |
| `ioctl(2)` | 设置 I/O 属性或杂项操作 |
| `mmap(2)` | 把文件映射到内存地址空间 |
| `brk(2)` | 扩展堆指针 |
| `futex(2)` | 快速用户态互斥 |

原文挑了四个「用途不那么显然」的：

- **`ioctl(2)`**：最难的学，因为语义模糊。它常用于请求内核执行杂项动作，尤其系统管理工具。原文举例：`perf(1)` 不为每个动作加一个系统调用，而是加一个 **`perf_event_open(2)`**，返回 fd，然后用 `ioctl(2)` 带不同参数执行不同动作（如 `ioctl(fd, PERF_EVENT_IOC_ENABLE)`）。**参数比系统调用更容易加和改。**
- **`mmap(2)`**：常用于把可执行文件与库映射进进程地址空间，也可用于内存映射文件；有时也用它代替基于 `brk(2)` 的 `malloc(2)` 来分配工作内存，以**降低系统调用率**（但不一定奏效，因为内存映射管理本身有代价）。
- **`brk(2)`**：扩展堆指针，界定进程工作内存大小。通常由内存分配库在 `malloc(3)` 无法从现有堆空间满足时调用。
- **`futex(2)`**：处理用户态锁的**一部分——可能阻塞的那部分**。

系统调用都有 man page，接口通常简单一致，需要时用错误码描述（如 `ENOENT`）。

### 2.4 §3.2.4 Interrupts：中断

> An interrupt is a signal to the processor that some event has occurred that needs processing

中断通常会让处理器进入内核模式（若还不在）、保存当前线程状态，然后运行**中断服务例程（ISR）**。

**两类中断**（`Figure 3.4`）：

| | 异步中断 | 同步中断 |
|---|---|---|
| 来源 | 外部硬件 | 软件指令 |
| 例子 | 磁盘 I/O 完成、硬件故障、网卡收包、键鼠输入 | `int 0x80` 这类陷阱、除零异常、缺页 |
| 关键点 | **到达时，被中断的代码可能已经不在 CPU 上了** | 责任软件和指令**仍在 CPU 上** |

原文的 `Figure 3.5` 给了异步中断的经典场景：MySQL 在 CPU 0 上读文件 → 内容要从磁盘取 → 调度器切到另一个线程（Java 应用）→ 磁盘 I/O 完成时**数据库早就不在 CPU 0 上了** → 完成中断以虚线（异步）画在图上。

**中断线程**：ISR 要**尽快跑完**，以减小对被中断线程的影响。如果中断需要做的工作不止一点点（尤其可能**在锁上阻塞**），就交给**中断线程**处理，由内核调度。

Linux 的做法是驱动分成两半（`Corbet 05`）：

- **top half**：快速处理中断，运行在**中断禁用模式**（推迟新中断投递），跑太久会给其他线程带来延迟问题。
- **bottom half**：稍后处理，可以是 **tasklet** 或 **work queue**（后者是可被内核调度、必要时能睡眠的线程）。

Linux 网络驱动的 top half 处理入站包的 IRQ，调用 bottom half 把包推上网络栈；网络栈的 bottom half 实现为 **softirq**。

**中断延迟**：

> The time from an interrupt’s arrival to when it is serviced is the interrupt latency

它取决于硬件与实现，是实时/低延迟系统的研究对象。

**中断屏蔽**：有些内核代码路径不能安全地被中断（如系统调用里拿了自旋锁，而中断处理也要这把锁——持有它被中断会死锁）。内核可以临时**屏蔽中断**（设置 CPU 的中断屏蔽寄存器）。屏蔽时间应尽可能短，因为它会扰动那些靠其他中断唤醒的应用。**中断禁用时间本身是性能分析的目标**（Ftrace 的 `irqsoff` tracer 直接支持这种分析）。

有些高优先级事件不能忽略，实现为**不可屏蔽中断（NMI）**。例如 Linux 用 IPMI 看门狗定时器检查内核是否因长时间没有中断而卡死，是则发 NMI 重启。

### 2.5 §3.2.5 Clock and Idle：时钟与空闲线程

Unix 内核的核心组件是 `clock()` 例程，**由定时器中断触发**。历史上执行频率是 60、100 或 1000 Hz（Hertz，每秒周期数），每次执行叫一个 **tick**。

它的职责：更新系统时间、**让定时器与线程时间片到期**、维护 CPU 统计、执行已排程的内核例程。

原文列出两类问题：

- **Tick latency**：100 Hz 的时钟，定时器最多要等下一个 tick 才被处理，**额外延迟可达 10 ms**。后来用**高精度实时中断**修复，让执行立即发生。
- **Tick overhead**：tick 消耗 CPU 周期、轻微扰动应用，是所谓**操作系统抖动（jitter）**的一个来源。现代处理器还有动态电源特性，能在空闲时关掉部分单元；而时钟例程会打断这种空闲，**白白消耗功耗**。

现代内核把很多功能从时钟例程移到**按需中断**，做成 **tickless kernel**，让处理器能更久地待在睡眠状态。

Linux 的时钟例程是 `scheduler_tick()`，典型跑在 **250 Hz**（`CONFIG_HZ`），并用 **NO_HZ**（`CONFIG_NO_HZ`）在无负载时省略时钟调用。

**空闲线程**：没有活干时，内核调度一个**占位线程**等待工作。现代 Linux 的 idle task 可以执行 `hlt` 指令让 CPU 停摆，直到下一个中断到来，从而省电。

> （推演）**tickless 与本仓库的关系**：`otf-server` 的主循环 `transport.Poll(sched.PollTimeoutMs(now))` 的超时被 clamp 到 `[0, 50] ms`。这意味着**即使完全没事件，它也会每 50 ms 醒一次**。这是**逻辑正确性要求**（20 Hz tick 必须按时跑），但代价是每秒 20 次「醒来看一眼没有事」。在 Linux 上这对应 **20 次定时器唤醒 + 20 次 epoll_wait 返回**，也就是 20 次 mode switch 的地板开销。**这个地板开销是本仓库单线程架构的固有成本**，也是它「idle 时也不完全空闲」的原因。

---

## 3. §3.2.6–§3.2.8 Processes / Stacks / Virtual Memory

原文用三小节讲进程、栈与虚拟内存。

**§3.2.6 Processes**：进程是执行程序的抽象；`fork(2)` 创建，`exec(2)` 换程序映像；进程有**自己的环境**（进程环境包含什么，原文在 §3.2.6 有一节专门讲）。

**§3.2.7 Stacks**：栈是执行上下文的一部分。原文讲了**怎么读栈**（从栈帧回溯出调用链——这正是 perf / flame graph 的基础），以及**用户栈与内核栈是分开的**。

**§3.2.8 Virtual Memory**：

> Virtual memory : An abstraction of main memory that supports multitasking and oversubscription. It is, practically, an infinite resource.

「实际上是无限资源」——这句是虚拟内存存在的**根本理由**：它让**超额分配**成为可能，让每个进程都以为自己独占了全部地址空间。

---

## 4. §3.2.9 Schedulers：调度器与 CPU-bound / I/O-bound ★

### 4.1 调度器做什么

Unix 及其衍生系统是**分时系统**，通过瓜分执行时间让多个进程「同时」运行。调度过程由**调度器**完成，它是内核的关键组件；`Figure 3.12` 表明调度器作用于**线程**（Linux 里叫 **task**），把它们映射到 CPU。

> The scheduler keeps track of all threads in the ready-to-run state, traditionally on per-priority queues called run queues

（现代内核可能**按 CPU 各有一组队列**，还可能用队列之外的数据结构。）

**线程数 > CPU 数时，低优先级线程排队等。** 大多数内核线程的优先级比用户态进程高。

进程优先级可以被动态调整来优化特定负载。

### 4.2 CPU-bound vs I/O-bound ★

原文把负载分成两类，**这是全章最有用的分类**：

| | **CPU-bound** | **I/O-bound** |
|---|---|---|
| 特征 | 重计算（科学/数学分析） | 做 I/O、计算很少 |
| 例子 | 数值分析、编码 | web 服务器、文件服务器、交互式 shell |
| 预期运行时间 | 长（秒、分、时、天） | 短，要**低延迟响应** |
| 被什么限制 | **CPU 资源** | **I/O 资源**（存储或网络） |

原文接着讲一个经典策略（`Thompson 78`）：**识别出 CPU-bound 负载并降低其优先级**，让 I/O-bound 负载（低延迟响应更可取）先跑。做法是计算**近期计算时间与真实时间之比**，比值高（计算多）就降优先级。

> 这机制**偏好短运行进程**，而那通常正是做 I/O 的进程，包括人类交互进程。

现代内核支持多种**调度类 / 调度策略**（Linux），例如**实时调度**——优先级高于一切非关键工作，包括内核线程；配合**抢占**提供可预测的低延迟调度。

> （推演）**本仓库属于哪一类？** 两类都有，但**混合形态**：
> - 网络侧是 **I/O-bound**（epoll 等待 socket）——低延迟响应很重要。
> - `src/sim/` 是 **CPU-bound**（每 tick 跑 Boss 状态机、玩家模拟、命中判定）——运行时间短但每帧必做。
>
> 而本仓库**没有用户态抢占**（它是单线程 reactor，一个线程从头跑到尾）。所以 CPU-bound 那一段**无法被 I/O-bound 那一段插队**——**这正是本仓库架构的根本约束**，也是第 2 章说的「单线程 = 容量型资源 = 100% 即饱和」的具体机制。

---

## 5. §3.2.10–§3.2.17：文件、缓存、网络、多核与资源控制

### 5.1 §3.2.11 Caching：缓存层次

因为磁盘 I/O 历来高延迟，软件栈的很多层都在**缓存读、缓冲写**。原文 `Table 3.2` 列了从先检查到后检查的层次：

```
 1  客户端缓存（浏览器缓存）
 2  应用缓存
 3  Web 服务器缓存（Apache cache）
 4  缓存服务器（memcached）
 5  数据库缓存（MySQL buffer cache）
 6  目录缓存（dcache）
 7  文件元数据缓存（inode cache）
 8  操作系统缓冲缓存（buffer cache）
 9  文件系统主缓存（page cache、ZFS ARC）
10  文件系统二级缓存（ZFS L2ARC）
11  设备缓存（ZFS vdev）
12  块缓存（buffer cache）
13  磁盘控制器缓存（RAID 卡缓存）
14  存储阵列缓存
15  磁盘上的缓存
```

原文举 buffer cache 为例：它是主内存里存最近用过的磁盘块的区域；磁盘读命中缓存就**立刻返回**，避开磁盘 I/O 的高延迟。

### 5.2 §3.2.12 Networking

现代内核提供内置协议栈（TCP/IP stack），用户态通过**套接字（socket）**访问网络。物理设备是**网络接口**，通常由网卡提供。

### 5.3 §3.2.14 Multiprocessor：SMP 与 IPI

多处理器支持通常实现为 **SMP**（对称多处理），所有 CPU 平权。难点在于**并行线程之间的内存与 CPU 共享访问**；还可能有 **NUMA** 架构（不同插槽接不同内存 bank），带来额外性能挑战。

**IPI（inter-processor interrupt，处理器间中断，也叫 SMP call / CPU cross call）**：多核系统里 CPU 之间需要协调时（例如**缓存一致性**：某条内存翻译项在别的 CPU 缓存里已失效，需要通知它们），一个 CPU 可以要求其他 CPU（或所有 CPU）立刻执行这项工作。IPI 被设计成**执行要快**，以最小化对其他线程的打断。**IPI 也可用于抢占。**

### 5.4 §3.2.15 Preemption：抢占

内核抢占允许**高优先级用户态线程打断内核并执行**。这让实时系统能在给定时间约束内完成工作（航空、医疗设备都用）。支持抢占的内核称为**完全可抢占（fully preemptible）**，不过实际上仍有少量不可中断的关键代码路径。

Linux 另一种是**自愿内核抢占**：在代码里的逻辑停止点检查并执行抢占，避免完全抢占内核的复杂度，同时给常见负载提供低延迟抢占。

### 5.5 §3.2.16 Resource Management：资源控制

OS 可提供各种可配置的控制来微调对 CPU、内存、磁盘、网络的访问。早期 Unix/BSD 有基本的**每进程**资源控制（`nice(1)` 的 CPU 优先级、`ulimit(1)` 的资源限制）。

Linux 的 **cgroups** 在 2.6.24（2008）整合进来，之后不断加控制项；**cgroup v2**（统一层级）在 Linux 4.5（2016）提供。

### 5.6 §3.2.17 Observability

OS 由内核、库、程序组成；这些程序包含**观测系统活动与分析性能的工具**，通常装在 `/usr/bin` 与 `/usr/sbin`。第三方工具也可能被安装以提供额外观测能力。

---

## 6. §3.4 Linux：Linux 内核的性能演进

Linux 1991 年由 Linus Torvalds 创造。原文列了它的「家谱」——每个前身贡献了什么：

| 来源 | 贡献 |
|---|---|
| **Unix（及 Multics）** | 操作系统分层、系统调用、多任务、进程、进程优先级、虚拟内存、全局文件系统、文件权限、设备节点、缓冲缓存 |
| **BSD** | 分页虚拟内存、需求分页、快速文件系统（FFS）、TCP/IP 网络栈、套接字 |
| **Solaris** | VFS、NFS、页缓存、统一页缓存、slab 分配器 |
| **Plan 9** | 资源 fork（`rfork`），用于创建进程与线程之间不同层级的共享 |

### 6.1 §3.4.1 Linux Kernel Developments

原文列了长长一串带版本号的性能特性。**对本仓库最相关的几条**：

| 版本 | 特性 | 与本仓库的关系 |
|---|---|---|
| 2.5.7 | **Futex** | 快速用户态互斥。本仓库单线程，**用不上** |
| 2.5.46 | **epoll** | 高效在大量 fd 上等待 I/O 的系统调用，**改善服务器应用性能**——本仓库的命根子 |
| 2.6.18 | **Delay accounting** | 追踪每任务的延迟状态 |
| 2.6.21 | **DynTicks** | 动态 tick，空闲时时钟中断不触发，省 CPU 与功耗 |
| 2.6.23 | **CFS** | 完全公平调度器 |
| 2.6.24 | **cgroups** | 控制组：**测量并限制**一组进程的资源用量 |
| 2.6.28 | **Tracepoints** | 内核静态 tracepoint |
| 2.6.31 | **perf** | Linux 性能事件工具集 |
| 3.5 | **uprobes** | 用户态软件动态追踪基础设施 |
| 3.10+ | **NO_HZ_FULL** | 无 tick 内核，避免负载扰动 |
| 3.13 | **多队列块 I/O** | 每 CPU 的 I/O 提交队列 |
| 4.5 | **epoll 可扩展性** | 多线程场景下，epoll(7) 避免为每个事件唤醒所有等待同批 fd 的线程（**惊群问题**） |
| 4.20 / 5.2 | **PSI** | 压力停顿信息：显示**在 CPU、内存或 I/O 上停顿的时间**；5.2 加了阈值通知 |
| 5.1 | **io_uring** | 通用异步接口，应用与内核之间用共享环形缓冲 |
| 5.4 | **MADV_COLD / MADV_PAGEOUT** | `madvise(2)` 提示内核内存「要但不要马上」/「可立即回收」 |

两条值得展开：

**epoll 可扩展性（4.5）**：

> epoll(7) avoids waking up all threads that are waiting on the same file descriptors for each event, which caused a thundering-herd performance issue

这说的是**多线程**场景。本仓库单线程，**惊群问题对它不存在**——但这恰恰说明：**如果将来把本仓库改成多线程 reactor，epoll 的这个 4.5 特性就是必须依赖的地基**。

**PSI（4.20）**：这是 Linux 上最接近第 2 章「饱和度」的现成指标——它直接告诉你「有多少时间进程因为等 CPU / 等内存 / 等 I/O 而**停顿**」。**本仓库完全没用它。**

### 6.2 §3.4.2 systemd / §3.4.3 KPTI / §3.4.4 Extended BPF

原文介绍 systemd 作为 init 系统（`systemd-analyze` 等工具）、KPTI（Meltdown 缓解，4.14 引入，代价是上下文切换与系统调用更贵，因为要额外刷 TLB）、扩展 BPF（3.18+，内核态安全执行环境）。

> （推演）KPTI 那一段对本仓库有实际意义：**现代 Linux 上每次系统调用的 TLB 开销被放大了**。也就是说，「少做系统调用」在今天的内核上比十年前更值钱。

---

## 7. 实战锚点：otf-server 的 epoll_wait 与读写系统调用频率

前面讲的是操作系统。本节把镜头对准 `src/net/tcp_transport.cpp`——**这个仓库的每一行系统调用都在模式切换**。

### 7.1 系统调用清单：全仓库只有这几处

`tcp_transport.cpp` 的文件头注释里写了一条纪律：**`::epoll_ctl` 在全仓库只有一个调用点**（`RawEpollCtl`），注释里直接给了核查命令：

```
grep -n '::epoll_ctl' src/net/tcp_transport.cpp
```

同样的纪律在 `metrics_server.cpp` 里也有（`::accept`、`::read`、`::write`、`::poll`、`::fcntl` 各自集中）。

按 §3.2.3 的分类，本仓库用到的系统调用：

| 系统调用 | 在哪 | 频率 |
|---|---|---|
| `epoll_wait` | `TcpTransport::Poll` | **每轮主循环 1 次** |
| `epoll_ctl` | `RawEpollCtl`（唯一调用点） | 每次连接状态变化 1 次（add/del/mod） |
| `read` | `OnReadable` / `Recv` | **每次可读事件至少 1 次**，直到 EAGAIN |
| `write` | `FlushWrite` | 每次发送至少 1 次，直到写完或 EAGAIN |
| `accept` | `AcceptNew` | 每次新连接 1 次 |
| `fcntl` | `AcceptNew`（设 O_NONBLOCK） | 每次新连接 1 次 |
| `setsockopt` | `AcceptNew`（设 TCP_NODELAY） | 每次新连接 1 次 |
| `poll` | `metrics_server.cpp` | 短写时按需 |

### 7.2 epoll_wait：每 50 ms 一次的地板

`TcpTransport::Poll` 的实现：

```cpp
void TcpTransport::Poll(int timeout_ms) {
  const int n = ::epoll_wait(epoll_fd_, events_.data(),
                             static_cast<int>(events_.size()), timeout_ms);
  if (n < 0) {
    // EINTR 只是「被信号打断」，不是错误
    if (errno != EINTR) log::warn(...);
    return;
  }
  ...
}
```

三个观察：

1. **批量大小是编译期常量** `kEpollBatch = 256`（`tcp_transport.cpp` 第 19 行），`events_.resize(kEpollBatch)` 在构造时一次性分配。**这是零分配的**——每次 `epoll_wait` 不碰分配器。
2. **超时由主循环 clamp 到 `[0, 50] ms`**（`server_config.h` 的 `kMaxPollTimeoutMs = 50`，注释写明「50 = 一个 tick 周期；再大就意味着『tick 没按时跑』，那本身就是 bug」）。
3. **EINTR 被安静地当作正常返回**——这正是 §3.2.4 说的「信号打断」，处理方式正确。

**频率**：主循环每轮调一次，20 Hz tick ⇒ **每秒约 20 次 `epoll_wait`**（外加补帧时的额外轮次）。

按第 2 章 §2.3.2 的时间尺度表，`epoll_wait` 本身是**系统调用**（一次 mode switch，几十到几百纳秒），而**它阻塞等待的时间**不是 CPU 时间——那是 idle。

> （推演）**这里有一个可被观测的量：`epoll_wait` 返回的 `n` 与「阻塞了多久」。** 本仓库目前**两者都不可见**：`n`（就绪事件数）没有进指标，阻塞时长也没有。这正是第 2 章 A3 指标建议的来源——**把 Poll 的耗时和 Advance 的耗时分开量**，就能知道这 50 ms 的窗口里，CPU 到底干了多少、真的空等了多久。

### 7.3 read：每次事件至少一次系统调用

`OnReadable` 的循环是本仓库**最热的一条路径**：

```cpp
void TcpTransport::OnReadable(int fd) {
  uint8_t buf[kReadChunk];          // kReadChunk = 4096
  // ★ 必须循环读到 EAGAIN（§6.4.1）：漏读 = 遗留数据下一帧不再触发。
  for (;;) {
    if (!is_open(fd)) return;
    const ssize_t n = ::read(fd, buf, sizeof(buf));
    if (n > 0) {
      if (on_bytes_ && !on_bytes_(...)) { Close(...); return; }
      continue;
    }
    if (n == 0) { Close(...); return; }   // 对端 FIN
    if (errno == EINTR) continue;         // 信号打断，重试
    if (errno == EAGAIN || errno == EWOULDBLOCK) return;  // ★ 读干净了
    Close(...);                          // 硬错误：RST/EIO
    return;
  }
}
```

**系统调用次数分析**（按 §3.2.3 的表，`read(2)` 是最普通的系统调用）：

- **最坏情况**：一次可读事件只有 100 字节数据 → `read` 返回 100 → 循环 → 再 `read` → EAGAIN ⇒ **2 次系统调用**。
- **最好情况**：一次可读事件有 4096×N 字节 → N 次成功的 `read` + 1 次返回 EAGAIN ⇒ **N+1 次系统调用**。
- **最坏**：客户端发 256 KB（`kMaxFd` 相关的缓冲上限是 128 KB）⇒ **33 次 `read`**，一次可读事件就 33 次模式切换。

**这就是「I/O 大小」这个性能杠杆在本仓库的具体形态。** 第 5 章 §5.2.1 会专门讲「选 I/O 大小」——**同样的字节数，用 4 KB 还是 64 KB 读，系统调用次数差 16 倍。**

> （推演）这里有一个明确的可优化点，也是本仓库**已知的缺口**：`kReadChunk = 4096` 是**栈上固定缓冲**，每次只读 4 KB。TCP 的接收窗口远大于 4 KB 时，一次可读事件里的数据往往超过 4 KB，于是循环里的 `read` 被调用很多次。**把 `kReadChunk` 提到 64 KB（仍是栈缓冲，或改用成员缓冲避免大栈占用），系统调用次数能降一个数量级。** 代价是每次事件多 memcpy 一次（数据量不变，只是分块变大）——**这正是第 5 章「选 I/O 大小」的权衡**：大 I/O 省系统调用，小 I/O 省内存与延迟。

### 7.4 write：FlushWrite 的循环

```cpp
void TcpTransport::FlushWrite(int fd) {
  Conn& c = At(fd);
  while (c.out_sent < c.out.size()) {
    const ssize_t n = ::write(fd, c.out.data() + c.out_sent,
                              c.out.size() - c.out_sent);
    if (n > 0) { c.out_sent += n; continue; }
    if (n < 0 && errno == EINTR) continue;
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
    Close(...);
    return;
  }
  c.out.clear();
  c.out_sent = 0;
  DisableWrite(...);   // ★ 写完立刻摘掉 EPOLLOUT
}
```

**关键设计**：写完**立刻摘掉 EPOLLOUT**。这对应第 2 章 USE 方法里「EPOLLOUT 按需开关」的不变量，本仓库用 `otf_epoll_ctl_add_with_out`（**应恒为 0**）来守。

按 §3.2.2 的说法，`write` 是 mode switch；如果 socket 缓冲满，返回 EAGAIN —— **注意此时不发生 context switch**（非阻塞 socket），只发生模式切换后立刻返回。这正是非阻塞 I/O 的价值：**把「阻塞等待」变成「返回后再来」。**

### 7.5 一个必须说清的缺口：发送缓冲不是环形缓冲

`Send` 的实现里有这样一段注释：

```cpp
const std::size_t old = c.out.size();
c.out.resize(old + n);
std::memcpy(c.out.data() + old, buf, n);
```

`std::vector` 的 `resize` **保留已有 capacity**，所以稳态下零分配——这是好的。但**它是线性缓冲，不是环形缓冲**。

第 5 章 §5.2.3 会讲环形缓冲：用 start/end 指针，**满了就把旧数据覆盖或回收**，避免「已发送但未清理的数据一直占着 buffer 头部」。

**本仓库的实际情况**：`c.out_sent` 是发送偏移，`c.out` 是完整缓冲。写完后 `c.out.clear()` + `c.out_sent = 0`。**在「写完才 clear」这个模式下，它其实等价于环形缓冲**——因为没写完时不会 clear，`c.out_sent` 之前的空间虽然占着，但**不会无限增长**（因为 `out_sent` 只增不减，且只有写完才清空）。

**但代价是**：一个慢客户端只要让 socket 缓冲填满，`c.out` 就会一直增长到 `kSendBufCap = 128 KB` 才被断开。**128 KB × 1024 连接 = 128 MB 的最坏占用**——这是本仓库**用内存换延迟**的一处明确取舍，也是第 2 章「容量型资源到 100% 就是饱和」的直接体现。

> （推演）如果要上真正的环形缓冲（固定大小、可复用空间），收益是**内存占用从「峰值 × 连接数」降到「固定 × 连接数」**，且不需要 `resize` 的拷贝。代价是要自己维护读写指针与「半包」处理。这属于**已知缺口**，不是 bug。

### 7.6 「CPU 在等什么」在本仓库的完整答案

把本节串起来，本仓库的 CPU 时间去向是：

```
每 50 ms 一轮主循环：
  ├─ epoll_wait           1 次系统调用（阻塞期间 CPU 空闲）
  ├─ 每个就绪 fd：
  │   ├─ read              1~33 次系统调用（视数据量）
  │   ├─ 解码 + 投递      纯 CPU
  │   ├─ write             1~N 次系统调用（视积压）
  │   └─ sim 推进         纯 CPU（20 Hz 固定）
  ├─ metrics_server.Poll  每轮一次（内部还有 poll/accept/read/write）
  └─ 补帧判定            纯 CPU
```

**四种状态**（这就是标题里的「四个状态」）：

| 状态 | 本仓库对应 | 可观测性 |
|---|---|---|
| **用户态在跑**（user mode） | 解码、sim、快照组装 | ❌ 完全没有 |
| **内核态在跑**（kernel mode） | `read`/`write`/`epoll_wait` 的内核部分 | ❌ 完全没有 |
| **就绪等 CPU**（runnable） | 单线程 reactor 下**几乎不存在**——没有第二个线程可切 | ❌ 不适用 |
| **阻塞等 IO**（blocked/off-CPU） | `epoll_wait` 阻塞的那几十毫秒 | ❌ 完全没有 |

**结论**：本仓库**四种状态一个都观测不到**。它只暴露了「帧数、连接数、错误数」这类**结果指标**，没有暴露任何**状态指标**。

按第 5 章 §5.4.5 的九状态模型，本仓库的线程状态会**全部塌缩成两类**：

- **User**：解码 + sim + 快照（纯 CPU）
- **Net I/O**：`epoll_wait` 阻塞期间

（其余七种——Kernel、Runnable、Swapping、Disk I/O、Sleeping、Lock、Idle——**在本仓库的架构下要么不存在，要么不可观测**。）

**最小可行的观测方案**（承接第 2 章的 A3）：

```
每轮主循环：
  t0 = now()
  transport.Poll(timeout)        ← 这段是「等 IO」
  t1 = now()
  sched.Advance(...)             ← 这段是「烧 CPU」
  t2 = now()

  metrics.on_poll_wait_ms(t1 - t0)     ← Net I/O 状态时长
  metrics.on_sim_cpu_ms(t2 - t1)       ← User 状态时长
```

有了这两个数，本仓库第一次能回答「我的 CPU 在等什么」——**而这正是本篇标题那个问题的答案**。

---

## 8. 收尾：本章金句清单

1. **Mode switch : A switch between kernel and user modes.**
   —— 换特权级；每次系统调用都发生一次。

2. **Context switch : A switch from running one thread or process to another.**
   —— 换线程；比模式切换贵得多，两者**不是一回事**。

3. **All system calls mode switch. Some system calls also context switch**
   —— 全部系统调用都切模式；**只有阻塞型**的还会切线程。

4. **The kernel is the only UNIX code that cannot be substituted by a user to his own liking.**
   —— 内核是用户**换不掉**的那部分代码，所以它应该少做决定。

5. **System call (syscall) : A well-defined protocol for user programs to request the kernel to perform privileged operations, including device I/O.**
   —— 系统调用是**协议**，不是函数调用。

6. **An interrupt is a signal to the processor that some event has occurred that needs processing**
   —— 中断是「有事件要处理」的信号；它会把 CPU 拉进内核态。

7. **Interrupt service routines (ISRs) are designed to operate as quickly as possible**
   —— ISR 要快；活儿多就交给 bottom half / 中断线程。

8. **The time from an interrupt’s arrival to when it is serviced is the interrupt latency**
   —— 中断延迟：到达 → 被服务，这段是实时系统的关键指标。

9. **Virtual memory : An abstraction of main memory that supports multitasking and oversubscription. It is, practically, an infinite resource.**
   —— 虚拟内存之所以存在，是为了**超额分配**。

10. **Unix and its derivatives are time-sharing systems, allowing multiple processes to run at the same time by dividing execution time among them.**
    —— 分时系统的本质是**瓜分时间片**。

11. **The scheduler keeps track of all threads in the ready-to-run state**
    —— 就绪队列是调度器的账本；「就绪」和「在跑」是两回事。

12. **CPU-bound : Applications that perform heavy compute**
    —— CPU-bound 被 **CPU 资源**限制；典型运行时间是秒/分/时/天。

13. **I/O-bound : Applications that perform I/O, with little compute**
    —— I/O-bound 被 **I/O 资源**限制；它要的是低延迟。

14. **epoll (2.5.46): A system call for efficiently waiting for I/O across many open file descriptors, which improves the performance of server applications.**
    —— epoll 的全部意义就是**在大量 fd 上高效地等**。

15. **cgroups (2.6.24): Control groups allow resource usage to be measured and limited for groups of processes.**
    —— cgroups 让「测量 + 限制」成为可能，是云与容器的地基。

16. **PSI (4.20, 5.2): Pressure stall information (PSI) is a set of new metrics to show time spent stalled on CPU, memory, or I/O.**
    —— PSI 是 Linux 上现成的**饱和度**指标：等 CPU / 等内存 / 等 I/O 各停了多少。

17. **io_uring (5.1): A generic asynchronous interface for fast communication between applications and the kernel, making use of shared ring buffers.**
    —— io_uring 用**共享环形缓冲**把异步 I/O 做到用户态。

18. **The kernel page table isolation (KPTI) patches added to Linux 4.14 in 2018 are a mitigation for the Intel processor vulnerability called “meltdown.”**
    —— Meltdown 的代价是**上下文切换与系统调用更贵**（多刷 TLB）——今天「少做系统调用」比十年前更值钱。

19. **Delay accounting (2.6.18): Tracks per-task delay states.**
    —— 内核早就能告诉你每个任务「卡在哪」了；本仓库没用。

20. **The kernel is the program that manages the system, including (depending on the kernel model) hardware devices, memory, and CPU scheduling.**
    —— 内核 = 设备 + 内存 + CPU 调度的管理者；性能问题的战场就在这三样。
