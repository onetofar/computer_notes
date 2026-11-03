---
title: Chapter 3. Operating Systems（精读）
category: 性能工程
tags:
  - 操作系统
  - 内核
  - 系统调用
  - 上下文切换
  - 中断
  - 调度器
  - 虚拟内存
difficulty: 进阶
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）第3章 Operating Systems"
source_section: "精读 §3.1 / §3.2.1 / §3.2.2 / §3.2.3 / §3.2.4 / §3.2.5 / §3.2.6–3.2.7 / §3.2.8 / §3.2.9 / §3.2.10–3.2.13 / §3.2.14 / §3.2.15 / §3.2.16–3.2.17 / §3.3.1–3.3.3 / §3.4.1 / §3.4.2 / §3.4.3 / §3.4.4 / §3.5.1–3.5.4 / §3.6；§3.7 Exercises 与 §3.8 References 不立篇（练习题与参考文献是全书共用的自测/索引材料，不承载机制陈述）；§3.2.1 / §3.2.4 / §3.2.5 / §3.2.6 / §3.2.7 / §3.2.8 / §3.2.9 / §3.2.10 / §3.2.14 / §3.3 / §3.4 / §3.5 / §3.6 下的无编号子标题按原文文字引用，不编 § 号"
skills:
  - obsidian-markdown
---

# Chapter 3. Operating Systems

> **小节地图**：本章是全书的**操作系统前置知识**——不是性能方法论，是「被测对象本身」。
> 立篇：§3.1（术语口径）、§3.2.1–§3.2.5（内核 / 内核态与用户态 / 系统调用 / 中断 / 时钟）、
> §3.2.6–§3.2.7（进程与栈）、§3.2.8（虚拟内存）、§3.2.9（调度器）、
> §3.2.10–§3.2.13（I/O 栈、缓存、网络、设备驱动）、§3.2.14（多处理器与 IPI）、
> §3.2.15（抢占）、§3.2.16–§3.2.17（资源控制与可观测性）、§3.3（Unix/BSD/Solaris 谱系）、
> §3.4（Linux：演进 / systemd / KPTI / eBPF）、§3.5（PGO、unikernel、微内核与混合内核、分布式 OS）、
> §3.6（内核比较）。
> **不立篇**：§3.7 Exercises、§3.8 References（自测与文献，不承载机制）。
> ⚠️ **本章不讲 VMM / 硬件辅助虚拟化的开销**：原文只点到 KVM（§3.4.1）、jails / Zones（§3.3.2 / §3.3.3）、
> unikernel 与微内核的模型差异（§3.5.2 / §3.5.3）；虚拟化与云的性能代价在**第 11 章 Cloud Computing**。
> 底层依赖：CPU 调度见第 6 章、内存见第 7 章、文件系统见第 8 章、磁盘见第 9 章、网络见第 10 章、
> 基准测试见第 12 章、eBPF 工具见第 15 章。
> 同层对照：[[Chapter 9. Disks]]（同一套术语在 I/O 层的展开）、[[Chapter 1. Introduction]]（方法论起点）。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §3.1 上下文切换 | 03-Chapter 3. Operating Systems.md L79 | Context switch : A switch from running one thread or process to another. This is a normal function of the kernel CPU scheduler, and involves switching the set of running CPU registers (the thread context) to a new set. |
| §3.1 模式切换 | 03-Chapter 3. Operating Systems.md L81 | Mode switch : A switch between kernel and user modes. |
| §3.1 系统调用 | 03-Chapter 3. Operating Systems.md L83 | System call (syscall) : A well-defined protocol for user programs to request the kernel to perform privileged operations, including device I/O. |
| §3.1 陷入（trap） | 03-Chapter 3. Operating Systems.md L87 | Trap : A signal sent to the kernel to request a system routine (privileged action). Trap types include system calls, processor exceptions, and interrupts. |
| §3.1 硬件中断 | 03-Chapter 3. Operating Systems.md L89 | Hardware interrupt : A signal sent by physical devices to the kernel, usually to request servicing of I/O. An interrupt is a type of trap. |
| §3.2.1 单体内核 | 03-Chapter 3. Operating Systems.md L99 | The kernel is the core software of the operating system. What it does depends on the kernel model: Unix-like operating systems including Linux and BSD have a monolithic kernel that manages CPU scheduling, memory, file systems, network protocols, and system devices (disks, network interfaces, etc.). This kernel model is shown in Figure 3.1 . |
| §3.2.1 应用可直调 syscall | 03-Chapter 3. Operating Systems.md L107 | System libraries are pictured here as a broken ring to show that applications can call system calls ( syscalls ) directly. |
| §3.2.2 特权环 | 03-Chapter 3. Operating Systems.md L133 | Kernel and user mode are implemented on processors using privilege rings (or protection rings ) following the model in Figure 3.1 . For example, x86 processors support four privilege rings, numbered 0 to 3. |
| §3.2.2 模式切换 vs 上下文切换 | 03-Chapter 3. Operating Systems.md L143 | All system calls mode switch. Some system calls also context switch : those that are blocking, such as for disk and network I/O, will context switch so that another thread can run while the first is blocked. |
| §3.2.2 切换有开销 | 03-Chapter 3. Operating Systems.md L145 | Since mode and context switches cost a small amount of overhead (CPU cycles), |
| §3.2.3 系统调用要少 | 03-Chapter 3. Operating Systems.md L161 | System calls request the kernel to perform privileged system routines. There are hundreds of system calls available, but some effort is made by kernel maintainers to keep that number as small as possible, to keep the kernel simple |
| §3.2.4 中断 | 03-Chapter 3. Operating Systems.md L247 | An interrupt is a signal to the processor that some event has occurred that needs processing, and interrupts the current execution of the processor to handle it. It typically causes the processor to enter kernel mode if it isn’t already, save the current thread state, and then run an interrupt service routine (ISR) to process the event. |
| §3.2.4 同步中断：责任软件还在 CPU 上 | 03-Chapter 3. Operating Systems.md L285 | For these interrupts, the responsible software and instruction are still on CPU. |
| §3.2.8 虚拟内存按需映射 | 03-Chapter 3. Operating Systems.md L439 | Virtual memory is made possible by support in both the processor and operating system. It is not real memory, and most operating systems map virtual memory to real memory only on demand, when the memory is first populated (written). |
| §3.2.9 CPU-bound 与 I/O-bound | 03-Chapter 3. Operating Systems.md L473 | A commonly used scheduling policy dating back to UNIX identifies CPU-bound workloads and decreases their priority, allowing I/O-bound workloads—where low-latency responses are more desirable—to run sooner. |
| §3.2.15 可抢占内核 | 03-Chapter 3. Operating Systems.md L653 | Kernel preemption support allows high-priority user-level threads to interrupt the kernel and execute. This enables real-time systems that can execute work within a given time constraint, including systems in use by aircraft and medical devices. A kernel that supports preemption is said to be fully preemptible , although practically it will still have some small critical code paths that cannot be interrupted. |
| §3.2.16 cgroups 资源控制 | 03-Chapter 3. Operating Systems.md L663 | For Linux, control groups (cgroups) have been developed and integrated in Linux 2.6.24 (2008), and various additional controls have been added since then. |
| §3.4.1 CFS | 03-Chapter 3. Operating Systems.md L855 | CFS (2.6.23): Completely fair scheduler. |
| §3.4.3 KPTI 的代价 | 03-Chapter 3. Operating Systems.md L1012 | The kernel page table isolation (KPTI) patches added to Linux 4.14 in 2018 are a mitigation for the Intel processor vulnerability called “meltdown.” Older Linux kernel versions had KAISER patches for a similar purpose, and other kernels have employed mitigations as well. While these work around the security issue, they also reduce processor performance due to extra CPU cycles and additional TLB flushing on context switches and syscalls. |
| §3.6 微基准比较内核 | 03-Chapter 3. Operating Systems.md L1084 | Kernel performance is commonly compared using micro-benchmarks, and this is error-prone. |

> **书里没说的**（本篇的推论，原文没有，别当原文观点）：
> - **「mode switch vs context switch vs syscall vs trap vs interrupt」的五分表**是本篇为检索方便整理的；
>   原文 §3.1 是五个独立词条（L79/L81/L83/L87/L89），没有并列成一张对照表。
> - **「系统调用开销 = 固定成本 + 阻塞等待」这个二分是本篇的拆法**：原文只说切换「cost a small amount of
>   overhead (CPU cycles)」（L145），没有把「等待」与「开销」分开算；本篇这样拆是为了不让阻塞时间
>   被误当成系统调用本身的成本。
> - **「容器里看到的是宿主全局状态」这一层因果是本篇的推论**：原文只说 mount namespace 用于容器（L495）、
>   cgroups 是资源控制的机制（L659/L663），没有讨论「容器内 /proc 视图与宿主的关系」；
>   那部分属于第 11 章。
> - **「CFS 是 §3.2.9 描述的现代调度器」是本篇的连线**：原文在 §3.4.1 才把 CFS 列进 Linux 演进表（L855），
>   §3.2.9 并未点名 CFS；本篇据此把两者接起来。
> - **「PGO / unikernel / 微内核对性能的实际影响」在本篇被标为「本篇的读法」**：原文 §3.5 是三段
>   机制陈述（怎么工作），没有给出可比较的性能数字。

## §3.1 术语：先把五个词分清

这一节是全章的**口径表**。性能分析里最贵的错误往往不是「算错了」，而是「把两件不同的事当成同一件」——
本章要分清的是**模式切换（mode switch）** 与 **上下文切换（context switch）**，以及它们与
**系统调用 / 陷入 / 中断** 的从属关系。

| 词 | 原文定义要点 | 谁发起 | 原文位置 |
|---|---|---|---|
| **Context switch** 上下文切换 | 从运行一个线程/进程切到另一个；内核调度器的正常职能；要把运行中的 CPU 寄存器组换成新的一组 | 内核调度器 | L79 |
| **Mode switch** 模式切换 | 内核态与用户态之间的切换 | 内核（进入）/内核（返回） | L81 |
| **System call** 系统调用 | 用户程序请求内核执行特权操作（**含设备 I/O**）的一套约定协议 | 用户程序 | L83 |
| **Trap** 陷入 | 发给内核、请求执行某个特权动作的信号；**包含系统调用、处理器异常、中断三类** | 软件或硬件 | L87 |
| **Hardware interrupt** 硬件中断 | 物理设备发给内核的信号，通常是请求服务 I/O；**中断是陷入的一种** | 硬件设备 | L89 |

**三个必须钉死的层次关系**（原文 L87/L89 明说）：

```
trap（陷入）
 ├─ system call   ← 软件程序主动请求特权操作
 ├─ exception     ← 指令执行中的异常条件
 └─ interrupt     ← 硬件设备异步请求服务
```

**判据（这一节最实用的一条）**：报告里写「系统调用开销高」之前，先问三句：
1. 这是**模式切换**的成本（用户态↔内核态）还是**上下文切换**的成本（线程 A→线程 B）？
2. 是**固定成本**（几次切换的 CPU 周期）还是**等待成本**（阻塞在 I/O 上的时间）？
3. 阻塞型系统调用**同时**触发两者（见 §3.2.2）。

三者的处理手段完全不同：模式切换靠 vDSO / 内存映射 / kernel bypass 消除（§3.2.2）；
上下文切换靠减少线程数、加大时间片来降低频率（§3.2.9）；
等待成本靠异步 I/O 消除（§9.3.12）。**混着算，就会开错药。**

## §3.2.1 内核：这一层到底管什么

**机制**：内核是操作系统的核心软件，职责边界由**内核模型**决定。Unix-like 系统（Linux、BSD）是
**单体内核（monolithic kernel）**——一个程序同时管 CPU 调度、内存、文件系统、网络协议和系统设备：

> The kernel is the core software of the operating system. What it does depends on the kernel model: Unix-like operating systems including Linux and BSD have a monolithic kernel that manages CPU scheduling, memory, file systems, network protocols, and system devices (disks, network interfaces, etc.). This kernel model is shown in Figure 3.1 .

内核**外面**是系统库，再外面是应用。注意原文那张图里系统库被画成**断开的环**：

> System libraries are pictured here as a broken ring to show that applications can call system calls ( syscalls ) directly.

**这一句的机制含义**（也是「层」这一讲的核心）：**库不是必经之路**。Golang runtime 自带 syscall 层，
不需要 libc；内核旁路（DPDK）、内存映射 I/O、`sendfile(2)`、`io_uring` 都能绕开常规内核路径。
**系统调用少而精、库层厚而可变**，是 Unix 哲学的直接产物（详见 §3.2.3）。

**内核什么时候在跑**（原文 L123）：内核主要**按需执行**——用户程序发起系统调用，或设备送来中断。
另有少量内核线程异步做家务（内核时钟例程、内存管理任务），它们刻意做得很轻、占用很少 CPU。

**容易错在哪**：以为「计算密集型负载与内核无关」。原文明确否定了这种直觉：

> The kernel also chooses which CPU a thread will run on and can choose CPUs with warmer hardware caches or better memory locality for the process, to significantly improve performance.

即使应用全程在用户态，**CPU 竞争**（谁拿到哪个核）和**放置决策**（NUMA 亲和、缓存热度）仍在内核手里。

**判据**：看到「内核占用了 30% CPU」时，先分清是**系统调用频率**、**上下文切换频率**还是**中断频率**
贡献的——三者的调优方向完全不同（§3.2.2 / §3.2.3 / §3.2.4）。

## §3.2.2 内核态与用户态：模式切换与上下文切换

**机制**：内核跑在**内核态（kernel mode）**，可以全权访问设备、执行特权指令；用户程序跑在
**用户态（user mode）**，要特权操作必须通过系统调用请求。两者靠处理器上的**特权环**实现：

> Kernel and user mode are implemented on processors using privilege rings (or protection rings ) following the model in Figure 3.1 . For example, x86 processors support four privilege rings, numbered 0 to 3.

只有两三个环会被用到：用户态、内核态，以及**存在时的 hypervisor**（这就是虚拟化的特权层，见第 11 章）。
特权指令只允许在内核态执行；在用户态执行会触发**异常**，由内核处理（例如生成 permission denied）。

### 两个切换的分工（本章最该背下来的一段）

> All system calls mode switch. Some system calls also context switch : those that are blocking, such as for disk and network I/O, will context switch so that another thread can run while the first is blocked.

翻译成机制：

- **每次**系统调用 ⇒ 至少一次 **mode switch**（进内核 + 回用户态）。
- **阻塞型**系统调用（磁盘、网络 I/O）⇒ **额外**一次 **context switch**（把 CPU 让给别的线程）。
- 上下文切换**换的是寄存器组**（thread context），模式切换**换的是特权级**。

### 开销与四条规避路径

> Since mode and context switches cost a small amount of overhead (CPU cycles),

代价是**少量 CPU 周期**——但注意脚注：自 Meltdown 缓解补丁（KPTI）之后，**上下文切换变得更贵**（§3.4.3）。
原文列了四条规避手段：

| 手段 | 机制 | 例子 |
|---|---|---|
| **User-mode syscalls** | 把某些系统调用实现在用户态库里 | Linux **vDSO**：把 `gettimeofday(2)`、`getcpu(2)` 映射进进程地址空间 |
| **Memory mappings** | 用映射而不是系统调用搬数据 | 需求分页、data store、内存映射 I/O |
| **Kernel bypass** | 用户态直接访问设备，绕过内核路径 | DPDK（Data Plane Development Kit） |
| **Kernel-mode applications** | 把应用整个放进内核 | TUX web server、Extended BPF |

**容易错在哪**：把「每次 syscall 的固定成本」和「阻塞 syscall 的等待时间」混进一个数字。
后者是**延迟**，不是**开销**——它由 I/O 决定，消不掉模式切换也消不掉。

**一条架构细节**：某些处理器（如 SPARC）给内核单独的地址空间，这意味着**模式切换还必须切换虚拟内存上下文**，
代价比 x86 上更高（原文 L157）。

**判据**：报告「系统调用开销」时，必须拆成三栏——**模式切换次数 × 每次成本**、
**上下文切换次数 × 每次成本**、**阻塞等待时间**。第三栏不该记在「开销」里。

## §3.2.3 系统调用

**机制**：系统调用是用户程序请求内核执行特权例程的入口。原文对**数量**有明确的态度：

> There are hundreds of system calls available, but some effort is made by kernel maintainers to keep that number as small as possible, to keep the kernel simple

**少而稳**是 Unix 哲学：复杂接口在**用户态库**里构建（那里更好开发维护），典型如 C 标准库（libc / glibc）。
所以「系统调用少」不等于「接口少」——接口的丰富性在库层。

### 要记住的系统调用（原文 Table 3.1）

| 系统调用 | 说明 | 系统调用 | 说明 |
|---|---|---|---|
| `read(2)` | 读字节 | `stat(2)` | 取文件属性 |
| `write(2)` | 写字节 | `ioctl(2)` | 设置 I/O 属性及其他杂项 |
| `open(2)` | 打开文件 | `mmap(2)` | 把文件映射进内存地址空间 |
| `close(2)` | 关闭文件 | `brk(2)` | 扩展堆指针 |
| `fork(2)` | 创建新进程 | `futex(2)` | 快速用户态互斥锁 |
| `clone(2)` | 创建新进程或线程 | `connect(2)` | 连接网络主机 |
| `exec(2)` | 执行新程序 | `accept(2)` | 接受网络连接 |

每个系统调用都有 man page（在 **man 第 2 节**），接口简单一致，用**错误码**描述错误（如 `ENOENT`
表示 “no such file or directory”，glibc 通过 `errno` 变量暴露）。

### `ioctl(2)`：一个系统调用撑起整个工具接口

`ioctl(2)` 常被管理类工具用来请求「没有更明显的系统调用可用」的杂项动作。它的价值在原文的 perf 例子：

> Instead of system calls being added for each action, a single system call is added: perf_event_open(2), which returns a file descriptor for use with ioctl(2). This ioctl(2) can then be called using different arguments to perform the different desired actions.

**机制要点**：**新增一个动作不必新增一个系统调用**——加一个参数即可。这正是「系统调用数量要压小」这条
Unix 哲学在实践中的样子。

### 系统调用**频率**是可优化的量

`mmap(2)` 除了映射文件，还常被用来分配进程工作内存，代替基于 `brk(2)` 的 `malloc(2)`，
**以降低系统调用率并改善性能**——但原文立刻加了限定：这种做法**不总是奏效**，因为存在内存映射管理的取舍。

**判据**：想优化 syscall 开销，先用 tracing **数出「每个请求触发多少次系统调用」**，
再决定是「减调用次数」还是「改调用方式」。原文明说 syscall 率高是 KPTI 成本的主要放大器（§3.4.3）。

## §3.2.4 中断：与系统调用的分界

**机制**：中断是「有事件发生、需要处理」的信号，它会打断当前执行：

> An interrupt is a signal to the processor that some event has occurred that needs processing, and interrupts the current execution of the processor to handle it. It typically causes the processor to enter kernel mode if it isn’t already, save the current thread state, and then run an interrupt service routine (ISR) to process the event.

注意这条定义里的三步：**进内核态 → 保存当前线程状态 → 跑 ISR**。这与系统调用共享「进内核态 + 保存上下文」，
区别在于**发起者**和**触发时机**。

### 异步中断 vs 同步中断

- **异步中断**：外部硬件生成，**与当前运行的软件无关**。硬件中断的例子（原文列表）：磁盘 I/O 完成、
  硬件故障指示、网卡收到包、键鼠输入。
- **同步中断**：由软件指令生成。原文用 trap / exception / fault 三个词描述（并指出这些术语常被混用）：
  - **Traps**：刻意调入内核，如 `int` 指令（Linux x86 上的 syscall 可以是 `int 0x80`）；
  - **Exceptions**：异常条件，如除零；
  - **Faults**：常指内存事件，如访问无 MMU 映射的地址引发的缺页。

分界线在原文 L285 这一句：

> For these interrupts, the responsible software and instruction are still on CPU.

**异步中断发生时，触发它的那段软件早就不在 CPU 上了；同步中断发生时，责任软件和指令还在 CPU 上。**

### 系统调用 vs 中断：对照

| | 系统调用 | 硬件中断 | 同步中断（trap/exception/fault） |
|---|---|---|---|
| 发起者 | 用户程序 | 物理设备 | 软件指令 |
| 是否异步 | 否（与当前指令同步） | 是 | 否 |
| 典型例子 | `read(2)` | 磁盘完成、网卡收包 | `int 0x80`、除零、缺页 |
| 阻塞时 | 可能触发上下文切换 | 通常不阻塞触发者 | 通常不阻塞 |

### 中断线程：top half 与 bottom half

ISR 要**尽快**返回，以减少对正在运行的线程的干扰。若中断处理需要做更多工作（尤其是可能阻塞在锁上），
就交给**中断线程**处理。Linux 的实现是把驱动拆成两半：

> On Linux, device drivers can be modeled as two halves, with the top half handling the interrupt quickly, and scheduling work to a bottom half to be processed later [Corbet 05] .

- **top half**：快，因为**它在关中断状态下运行**（推迟新中断的投递），跑太久会给别的线程带来延迟问题；
- **bottom half**：可以是 **tasklet**，也可以是 **work queue**（后者本身是可被内核调度的线程，必要时能睡眠）。

Linux 网络驱动是典型：top half 处理入站包的 IRQ，调用 bottom half 把包推上协议栈，
bottom half 以 **softirq（软件中断）** 实现。

### 中断延迟与中断屏蔽

> The time from an interrupt’s arrival to when it is serviced is the interrupt latency , which is dependent on the hardware and implementation.

中断延迟是实时 / 低延迟系统的研究对象。

**中断屏蔽（interrupt masking）**：有些内核代码路径不能被安全地中断——典型是**在系统调用中获取了自旋锁**，
而该锁也可能被中断路径需要；带着锁被中断会死锁。内核可以通过设置 CPU 的中断屏蔽寄存器**临时关中断**：

> The interrupt disabled time should be as short as possible, as it can perturb the timely execution of applications that are woken up by other interrupts.

**关中断时长本身就是性能分析目标**——Ftrace 的 **irqsoff tracer** 直接支持这种分析。

**NMI（不可屏蔽中断）**：某些高优先级事件不能被忽略。原文例子是 Linux 的 **IPMI 看门狗定时器**：
一段时间内没有中断就判定内核像是锁死了，于是发 NMI 重启系统。

**判据**：追一个**延迟尖刺**（latency spike）时，标准起手式是：**先看 irqsoff（关中断时长），
再看调度延迟（run queue 等待），最后看上下文切换**。关中断区段是延迟尖刺最常见的元凶。

## §3.2.5 时钟与空闲

**机制**：Unix 内核的核心组件是 `clock()` 例程，由**定时器中断**驱动，历史上跑在 60 / 100 / 1,000 Hz
（每一拍叫一个 **tick**）。它干的事（原文 L311）：

> Its functions have included updating the system time, expiring timers and time slices for thread scheduling, maintaining CPU statistics, and executing scheduled kernel routines.

**注意这一行里的 "time slices"——时间片是时钟例程发的**，这是 §3.2.9 调度粒度的上游来源。

两类性能问题（原文自己分的）：

| 问题 | 机制 | 后果 |
|---|---|---|
| **Tick latency** | 100 Hz 时钟下，定时器最多要等到下一个 tick 才被处理，**额外最多 10 ms 延迟** | 定时器精度差；已用**高精度实时中断**修复 |
| **Tick overhead** | tick 消耗 CPU 周期、轻微扰动应用 | **操作系统抖动（jitter）** 的成因之一；还会打断处理器的空闲省电状态 |

现代内核把大量功能从时钟例程移到**按需中断**，做成了 **tickless kernel**（无节拍内核）：
减少开销、让处理器能保持更长的睡眠状态、提升能效。

Linux 的具体形态（原文 L325）：

> The Linux clock routine is scheduler_tick(), and Linux has ways to omit calling the clock while there isn’t any CPU load. The clock itself typically runs at 250 Hertz (configured by the CONFIG_HZ Kconfig option and variants), and its calls are reduced by the NO_HZ functionality (configured by CONFIG_NO_HZ and variants), which is now commonly enabled [Linux 20a] .

**空闲线程**：没有活干时，内核调度一个占位线程等待工作；现代 Linux 的 idle task 可以执行 **hlt** 指令
把 CPU 断电，直到下一个中断到来（省电）。

**判据**：看到**周期性抖动**（latency 曲线上的规律尖峰）先怀疑 tick 周期；
「CPU 明明很闲却还在耗电」先怀疑 tick 打断了 idle（看 NO_HZ / NO_HZ_FULL 是否生效，见 §3.4.1）。

## §3.2.6–3.2.7 进程与栈

**进程**是执行用户程序的環境：由**内存地址空间 + 文件描述符 + 线程栈 + 寄存器**组成。
**线程**是可执行上下文（栈 + 寄存器 + 指令指针）；进程含一个或多个线程，共享地址空间与文件描述符。
在 Linux 上，**线程与进程都叫 task**。

**进程状态**（原文 L361）：**on-proc**（正在 CPU 上跑）/ **ready-to-run**（可运行但在等 run queue）/
**sleep**（多数 I/O 会阻塞，把进程挂起直到 I/O 完成）/ **zombie**（终止后等父进程回收状态）。

**进程创建**：`fork(2)`（Linux 的 C 库通常包在通用的 `clone(2)` 之上）复制进程，
可用**写时复制（COW）**策略——先加引用，任一方修改被共享的内存时才真正复制，
从而推迟或消除内存复制，降低内存与 CPU 开销。

**栈**是 LIFO 的临时数据区：函数调用时返回地址压栈，必要时寄存器也压栈；一组栈帧构成一次函数执行的现场。
**栈回溯（stack walking）**能回答「为什么会执行到这里」——原文直言这些栈是性能分析与调试的**无价工具**。

内核栈的样子（原文示例，照抄）：

```
    tcp_sendmsg+1
    sock_sendmsg+62
    SYSC_sendto+319
    sys_sendto+14
    do_syscall_64+115
    entry_SYSCALL_64_after_hwframe+61
```

栈通常**按叶到根打印**：第一行是当前正在执行的函数，往下依次是父函数、祖父函数……
右侧的数字是**指令偏移量**（在该函数内的位置），只有需要指令级理解时才有用。

**用户栈与内核栈**：执行系统调用时，线程同时拥有用户级栈与内核级栈；
**被阻塞线程的用户栈在系统调用期间不变**（信号处理器是例外，可能借用用户栈）。
Linux 上系统调用用**每线程的内核异常栈**，另外还有软中断与硬中断各自的栈。

**判据**：说某个线程「慢」，第一件事是**看它当时的栈**——栈直接告诉你它在等谁
（等锁？等 I/O？等调度？）。

## §3.2.8 虚拟内存：页表与 TLB 的入口

**机制**：虚拟内存是主存的抽象，给进程与内核各自一份**几乎无限**的私有视图，
支持多任务（各自私有地址空间、互不争用）与**超额订阅**（在主存与辅存之间透明地映射）。
它由**处理器与操作系统共同实现**：

> Virtual memory is made possible by support in both the processor and operating system. It is not real memory, and most operating systems map virtual memory to real memory only on demand, when the memory is first populated (written).

**「on demand」这四个字是本章与第 7 章的接口**：虚拟地址到物理地址的映射在**首次写入**时才建立，
走的是**页表**；而页表项在 CPU 里由 **TLB** 缓存。

### 两种把内存挪到辅存的机制

| 机制 | 单位 | 评价 |
|---|---|---|
| **Process swapping** 进程换出 | 整个进程 | 原始 Unix 方法，**可能造成严重性能损失** |
| **Paging** 分页 | 页（如 4 KB） | 更高效，BSD 引入分页虚拟内存时加入 Unix |

两者都在**最近最少使用（LRU）**原则下把内存换到辅存、需要时再换回。
**Linux 的术语陷阱**：Linux 里说 "swapping" 指的是 **paging**，Linux 内核**不支持**老的整线程 / 整进程换出。

### 页表 / TLB 的成本在本章的落点

本章不展开 TLB 机制，但两处**明确提到它的成本**：

1. **§3.4.3 KPTI**：缓解 Meltdown 的补丁在**上下文切换与系统调用**时额外触发 **TLB flush**，
   因此让上下文切换变贵；同版本的 **PCID** 可以避免一部分 flush。
2. **§3.2.14 IPI**：多核系统上，某个 CPU 修改映射后，需要用**核间中断**通知其他 CPU
   让其缓存的地址转换项失效——**这就是 TLB 一致性维护的入口**。

**判据**：**syscall 密集型负载**（每秒几十万次调用）要单独算一笔 TLB 成本：
Netflix 实测 KPTI 的总代价是 0.1%–6%，且**随 syscall 率上升**（§3.4.3）。
大页（huge pages）是缓解手段之一——flush 之后 TLB 暖回来更快。

## §3.2.9 调度器

**机制**：Unix 及其衍生是**分时系统**，靠瓜分执行时间让多个进程同时跑。
**调度器**是内核的关键组件，做两件事：把**线程映射到 CPU**，以及维护**优先级**。
调度器跟踪所有处于 ready-to-run 状态的线程，传统上放在**按优先级划分的 run queue** 里；
现代内核可能为**每个 CPU** 维护独立的队列，并用队列以外的数据结构跟踪线程。
当想跑的线程多于可用 CPU 时，低优先级线程排队等待。

**工作负载的两种分类**（原文的划分）：

| 类型 | 特征 | 瓶颈 |
|---|---|---|
| **CPU-bound** | 重计算（科学与数学分析），运行时间以秒/分/小时计 | 受 CPU 资源限制 |
| **I/O-bound** | 几乎不计算（web server、file server、交互式 shell），追求低延迟响应 | 受存储或网络 I/O 限制 |

**经典 Unix 调度策略**（原文 L473）：

> A commonly used scheduling policy dating back to UNIX identifies CPU-bound workloads and decreases their priority, allowing I/O-bound workloads—where low-latency responses are more desirable—to run sooner.

机制是计算**近期 CPU 时间 ÷ 实际经过时间**的比值，比值高（偏 CPU-bound）就降优先级。
副作用是**偏好短作业**——而短作业通常正是 I/O-bound 与人机交互型作业。

**现代内核**支持多个**调度类 / 调度策略**（Linux），各自用不同算法管理优先级与可运行线程；
其中包括**实时调度**，其优先级高于所有非关键工作（**包括内核线程**），
与**抢占**（§3.2.15）配合，为需要严格时限的系统提供可预测的低延迟调度。

**时间片从哪来**：时钟中断到期（§3.2.5）——所以**时钟频率与调度粒度是耦合的**：
tick 越密，调度越细，但 tick 开销越大。

**判据**：「进程卡」有两种完全不同的形态，必须先用数据分开：
- **没拿到 CPU**（在 run queue 上等）⇒ 调度延迟 / 优先级问题，Linux 上可用 **delay accounting**（§3.4.1）量化；
- **拿到了 CPU 但算得慢** ⇒ 应用自身问题，别再调内核参数。

## §3.2.10–3.2.13 I/O 栈、缓存、网络与设备驱动

**文件系统的命名空间**：操作系统提供一个**全局文件命名空间**，是从根目录 "/" 开始的**自顶向下的树**。
各文件系统通过**挂载（mounting）**把自己的树接到某个目录（挂载点）上，用户因此可以透明地穿行，
不必关心底层是哪种文件系统。典型顶层目录：`etc`（配置）、`usr`（系统程序与库）、`dev`（设备节点）、
`var`（含日志等变动文件）、`tmp`、`home`。`/proc`、`/dev` 是**内核动态创建**的。

**隔离手段**：内核提供多种方式把进程隔离到文件命名空间的一部分，包括 `chroot(8)`，以及 Linux 上的
**mount namespaces**——**常用于容器**（详见 §3.2.16 与第 11 章）。

**VFS（虚拟文件系统）**：内核用来抽象各种文件系统类型的接口，由 Sun Microsystems 最初开发，
目的是让 UFS 与 NFS 更容易共存。它让新增文件系统类型变得容易，并支撑全局命名空间。

**I/O 栈**：从用户态软件到存储设备的路径叫 **I/O 栈**。原文的通用模型图里有一条**左侧直连块设备、
绕过文件系统**的路径——管理工具与数据库有时走这条路。

**缓存层**（原文 Table 3.2，按检查顺序）：客户端缓存（浏览器）→ 应用缓存 → web server 缓存 →
缓存服务器（memcached）→ 数据库缓存（MySQL buffer cache）→ 目录缓存（dcache）→ inode 缓存 →
操作系统 buffer cache → 文件系统主缓存（page cache、ZFS ARC）→ 文件系统二级缓存（ZFS L2ARC）→
设备缓存（ZFS vdev）→ 块缓存 → 磁盘控制器缓存（RAID 卡缓存）→ 存储阵列缓存 → 盘上缓存。

**设备驱动**：字符设备（raw device，无缓冲顺序访问，可小到单字符；键盘、串口），
块设备（以**块**为单位，历史上是 512 字节，可按偏移随机访问）。Unix 原始的块设备接口会缓存块缓冲区
以提升性能，**Linux 里这个 buffer cache 已经是 page cache 的一部分**。

**网络栈**：现代内核内置一整套网络协议（**TCP/IP 栈**），用户态应用通过**socket**（可编程端点）访问网络。
连接物理网络的是**网络接口**，通常由 **NIC** 提供；历史上系统管理员的职责是给网口配 IP 地址，
如今这项映射多由 **DHCP** 自动完成。协议本身很少变，但**传输层协议有新的采纳者：QUIC**；
变化更频繁的是**协议增强与选项**（新的 TCP 选项、新的 TCP 拥塞控制算法），
它们通常都需要内核支持（**用户态协议实现**是例外）。另外还有对不同网卡的支持——每种新网卡都需要新的内核驱动。

**判据**：本节与第 8、9 章强耦合——**「磁盘慢」的根因常常在缓存层，而不在设备**。
排查顺序固定：先确认这次 I/O 穿过了哪些缓存层（§9.3.3 的 15 层清单），再看设备。

## §3.2.14 多处理器：SMP、NUMA 与 IPI

**机制**：多处理器支持让操作系统用多个 CPU 并行执行工作，通常实现为 **SMP**（所有 CPU 地位平等）。
这在技术上并不容易——并行线程之间如何访问与共享内存和 CPU 是难题。
在多路系统上，还可能有**连接到不同 socket（物理处理器）的主存组**，构成 **NUMA** 架构，
带来额外性能挑战（详见第 6、7 章）。

**IPI（核间中断）** 是多核之间的协调机制：

> A CPU can request other CPUs, or all CPUs, to immediately perform such work using an inter-processor interrupt (IPI) (also known as an SMP call or a CPU cross call ).

典型用途之一是**内存转换项的缓存一致性**——通知其他 CPU「某条目若已缓存，现在已过期」，
也就是 §3.2.8 说的 **TLB 一致性**。原文还明确：**IPI 也可以用于抢占**。

**判据**：在多核机器上做「改页表权限 / 改映射」这类操作时，会引发**跨核 IPI 风暴**；
追踪里看到成片的 IPI 事件时，先怀疑是不是有频繁的映射变更（而不是「网络中断多」）。

## §3.2.15 抢占：可抢占内核 vs 不可抢占内核

**机制**：内核抢占支持允许**高优先级的用户线程打断内核并执行**。这让实时系统能在给定时间约束内完成工作
（航空与医疗设备是原文明说的例子）。支持抢占的内核称为 **fully preemptible**——
尽管实践中仍有少量**不能被打断的关键代码路径**：

> Kernel preemption support allows high-priority user-level threads to interrupt the kernel and execute.

Linux 提供三种配置（原文 L655）：

| Kconfig 选项 | 语义 | 取舍 |
|---|---|---|
| `CONFIG_PREEMPT_NONE` | 关闭抢占 | **吞吐更好，延迟更差** |
| `CONFIG_PREEMPT_VOLUNTARY` | **自愿式抢占**：内核代码在逻辑停止点检查并执行抢占 | 避免全抢占内核的复杂度，为常见负载提供低延迟抢占；Linux 常用此选项 |
| `CONFIG_PREEMPT` | 除关键区段外**全部内核代码**可抢占 | 最低延迟 |

**容易错在哪**：以为「可抢占内核 = 任何地方都能被打断」。**关中断区段是硬约束**：
自旋锁保护的关键代码期间不能被打断，否则死锁（§3.2.4）。
所以即便全可抢占，仍有**无法抢占的短窗口**——而延迟尖刺往往就出在这些窗口里。

**判据**：做低延迟系统时，先确认内核是 `CONFIG_PREEMPT` 或 `CONFIG_PREEMPT_VOLUNTARY`；
只报内核版本不报这两个选项，等于没说。吞吐型批处理则相反——`CONFIG_PREEMPT_NONE` 更合适。

## §3.2.16–3.2.17 资源控制与可观测性：容器就在这一层

**机制（资源控制）**：操作系统可以提供各种可配置的控制项，用于微调对 CPU、内存、磁盘、网络的访问。
这些**资源控制（resource controls）**用于在运行不同应用、或承载多租户（云计算）的系统上管理性能。
两类形态：

- **固定上限**：对单个进程（或进程组）设死限额；
- **份额式**：允许闲置额度被共享。

历史脉络：早期 Unix 与 BSD 有基本的**每进程资源控制**，包括 `nice(1)` 的 CPU 优先级与 `ulimit(1)` 的资源上限。
Linux 侧则是 **cgroups**（2.6.24 / 2008 年集成进内核），以及改进的统一层级方案 **cgroup v2**
（Linux 4.5 / 2016 提供，CPU 控制器在 4.15 加入）。

**机制（可观测性）**：操作系统由**内核、库、程序**组成；这些程序里包含**观察系统活动、分析性能的工具**，
通常装在 `/usr/bin` 与 `/usr/sbin`，也可以安装第三方工具补充可观测性。

**容器 / 虚拟化在这一层的坐标**（本章只给坐标，代价分析在第 11 章）：

| 机制 | 原文位置 | 说明 |
|---|---|---|
| `chroot(8)` / mount namespaces | §3.2.10（L495） | 把进程隔离到文件命名空间的一部分；**容器常用** |
| **Jails** | §3.3.2（L757） | FreeBSD 的轻量 OS 级虚拟化，多个 guest 共享一个内核 |
| **Zones** | §3.3.3（L785） | Solaris 的 OS 级虚拟化，**如今广泛以 Linux 容器形式出现** |
| **KVM** | §3.4.1（L871） | Linux 上的虚拟机技术，可创建运行**自己内核**的虚拟 OS 实例 |
| cgroups / cgroup v2 | §3.2.16（L659/L663） | 资源用量可**测量与限制**——多租户归因的唯一抓手 |

**判据**：在容器 / 虚拟化环境里做性能分析，**先确认资源归因的口径**：
容器共享宿主内核，内核态观测到的是**宿主的全局状态**；
「这个容器占了多少 CPU」这个问题，只有 cgroup 那一层的计数器能回答。
本篇明确标注：这一段因果是**本篇的推论**，原文只给了机制，没有给观测语义。

## §3.3 内核谱系：Unix、BSD、Solaris

这一节回答的是「**某个性能机制是从哪家内核来的**」——它不是性能机制本身，是溯源表。

**Unix**（1969 起，Ken Thompson、Dennis Ritchie 等）留下两句值得记住的话。第二句是设计原则：

> The kernel is the only UNIX code that cannot be substituted by a user to his own liking. For this reason, the kernel should make as few real decisions as possible.

Unix 早期**已经**为高性能做了几件事：进程有**调度优先级**（降低高优先级工作的 run-queue 延迟）；
磁盘 I/O 以**大块（512 字节）**执行并缓存在**每设备 buffer cache** 中；
空闲进程可以被换出以腾出主存；以及当然是**多任务**（提升作业吞吐）。

**系统调用数量的增长**是复杂度信号。原文 Table 3.3 按 man 第 2 节的条目数统计（有意的粗略比较）：

| 内核 | 系统调用数 |
|---|---|
| UNIX Version 7 | 48 |
| SunOS (Solaris) 5.11 | 142 |
| FreeBSD 12.0 | 222 |
| Linux 2.6.32-21-server | 408 |
| Linux 2.6.32-220.el6.x86_64 | 427 |
| Linux 3.2.6-3.fc16.x86_64 | 431 |
| Linux 4.15.0-66-generic | 480 |
| Linux 5.3.0-1010-aws | 493 |

Ken Thompson 在 ACM Turing 纪念活动上的原话（原文 L721）：

> UNIX had twenty system calls at the very first, and today Linux—which is a direct descendant—has over a thousand . . . I just worry about the complexity and the size of things that grow.

原文明说这只是**有文档的系统调用**，内核通常还提供更多供操作系统软件私有使用。

**BSD**（1978 起）贡献的性能相关机制：**分页虚拟内存**（不再整进程换出）、**需求分页**
（推迟物理内存映射，避免可能永不被使用的页付出早期性能与内存代价）、**FFS**
（把磁盘分配按柱面组分组，大幅减少碎片并提升旋转盘性能）、**高性能 TCP/IP 协议栈**、
**Sockets**（连接端点 API）、**Jails**（轻量 OS 级虚拟化）、**Kernel TLS**（把 TLS 处理搬进内核）。
BSD 至今以协议栈性能著称——Netflix 的 CDN 就用 FreeBSD。

**Solaris**（1982 起）贡献：**VFS**、**完全可抢占内核**（为高优先级与实时工作提供低延迟）、
**多处理器支持**（ASMP 与 SMP）、**slab 分配器**（用每 CPU 的预分配缓冲区缓存提升性能，
其衍生已成为包括 Linux 在内的内核标准）、**DTrace**（静态 + 动态追踪框架，生产环境实时可观测性，
Linux 的对应物是 BPF 与 bpftrace）、**Zones**、**ZFS**。

**判据**：这张谱系表的实际用途是**定位机制出处**——例如「slab 分配器为什么有 per-CPU 缓存」、
「VFS 为什么长得像这样」，答案在 Solaris；「需求分页为什么叫 demand」在 BSD；
「内核为什么要有系统调用数量上限」在 Unix。追机制的历史有助于避免重复踩坑。

## §3.4 Linux

### §3.4.1 Linux 内核演进（挑性能相关的看）

原文列了几十项。下面是**与本篇主题直接相关**的条目（版本号为首次引入的版本）：

| 版本 | 机制 | 与本篇的关系 |
|---|---|---|
| 2.5.7 | **Futex** | 高性能用户态同步原语——把「几乎不冲突」的锁开销从内核搬到用户态 |
| 2.5.36 / 2.6.38 | **Huge pages / Transparent huge pages** | 减少 TLB 压力，也加快 KPTI flush 后的 TLB 预热 |
| 2.5.43 | **RCU** | 读拷贝更新：多数只读的数据可并发读，提升可扩展性 |
| 2.5.46 / 4.5 | **epoll** / epoll 可扩展性 | 避免「惊群」：不再为每个事件唤醒所有等待线程 |
| 2.6.13 | **Voluntary kernel preemption** | 低延迟调度而不必承担全抢占内核的复杂度（§3.2.15） |
| 2.6.18 / 2.6.20 | **Delay accounting / IO accounting** | 量化「等 CPU 的时间」「按进程统计 I/O」 |
| 2.6.21 / 3.10 / 3.12 | **DynTicks / NO_HZ_FULL** | 空闲时不再触发时钟中断；NO_HZ_FULL 让**非空闲**线程也能免 tick（§3.2.5） |
| 2.6.23 | **CFS** | 完全公平调度器（§3.2.9） |
| 2.6.24 / 4.5 / 4.15 | **cgroups / cgroup v2 / CPU 控制器** | 资源限制与归因（§3.2.16） |
| 3.13 / 5.0 | **Multiqueue block I/O / 多队列成为默认** | 每 CPU 提交队列，替代单请求队列（第 9 章） |
| 3.18+ | **Extended BPF** | 内核态可编程（§3.4.4） |
| 4.9 | **硬件延迟 tracer** | 捕捉硬件与固件造成的系统延迟（含 SMI） |
| 4.14 | **PCID** | 减少上下文切换时的 TLB flush（§3.2.8） |
| 4.20 / 5.2 | **PSI** | CPU / 内存 / I/O 的**压力停顿**指标，5.2 起支持阈值通知 |
| 5.1 | **io_uring** | 应用与内核之间用共享环形缓冲的通用异步接口 |
| 5.8 | **perf flame graphs** | perf 支持火焰图 |

原文最后提醒：还有大量**未被列出**的小改进（锁、驱动、VFS、文件系统、异步 I/O、内存分配器、NUMA、
新指令集、GPU、perf 与 Ftrace），以及 systemd 带来的启动时间改善。

### §3.4.2 systemd：启动时间是可测量的

systemd 是 Linux 上常用的服务管理器（替代原始 UNIX init），特点是**依赖感知的服务启动**与**服务耗时统计**。
调优启动时间是这个领域偶发的任务，systemd 的耗时统计正好告诉你**该调哪里**。

```
# systemd-analyze
Startup finished in 1.657s (kernel) + 10.272s (userspace) = 11.930s
graphical.target reached after 9.663s in userspace
```

上面的输出说明系统（此例为到达 `graphical.target`）**用了 9.663 秒**启动。
更细的信息用 `critical-chain` 子命令：

```
# systemd-analyze critical-chain
The time when unit became active or started is printed after the "@" character.
The time the unit took to start is printed after the "+" character.

graphical.target @9.663s
└─multi-user.target @9.661s
  └─snapd.seeded.service @9.062s +62ms
    └─basic.target @6.336s
      └─sockets.target @6.334s
        └─snapd.socket @6.316s +16ms
          └─sysinit.target @6.281s
            └─cloud-init.service @5.361s +905ms
              └─systemd-networkd-wait-online.service @3.498s +1.860s
                └─systemd-networkd.service @3.254s +235ms
                  └─network-pre.target @3.251s
                    └─cloud-init-local.service @2.107s +1.141s
                      └─systemd-remount-fs.service @391ms +81ms
                        └─systemd-journald.socket @387ms
                          └─system.slice @366ms
                            └─-.slice @366ms
```

这段输出给出的是**关键路径（critical path）**：造成延迟的那条步骤序列。
在这个例子里最慢的服务是 `systemd-networkd-wait-online.service`，启动花了 1.86 秒。

其他有用的子命令：**blame**（最慢的初始化项）与 **plot**（输出 SVG 图）。

**判据**：优化启动时间，**不要按「哪个服务看着慢」去猜，要看 critical-chain**——
链上非关键的服务再慢也不影响总时长。

### §3.4.3 KPTI（Meltdown）：一段「安全补丁的性能账本」

**机制**：**内核页表隔离（KPTI）** 补丁在 2018 年随 Linux 4.14 加入，用于缓解 Intel 的
**Meltdown** 漏洞；更早的 Linux 版本有功能相近的 **KAISER** 补丁。

> While these work around the security issue, they also reduce processor performance due to extra CPU cycles and additional TLB flushing on context switches and syscalls.

**账本**（原文 L1014）：Netflix 云上生产负载实测影响在 **0.1% 到 6%** 之间，
**取决于负载的 syscall 率**（syscall 率越高，代价越大）。

**还能再降**：一是**使用大页**，让 flush 之后的 TLB 暖得更快；二是**用追踪工具检查系统调用**，
找出降低 syscall 率的办法——而这类追踪工具本身就是用扩展 BPF 实现的。

**同版本加入的 PCID** 支持让一部分 TLB flush 可以避免（前提是处理器支持 pcid）。

**判据**：**syscall 率是安全补丁成本的放大器**。当你在有 KPTI 的系统上做 syscall 密集型优化，
省下的不只是切换本身，还有 TLB flush。测这条收益时，**必须报告 syscall 率**，否则数字没有可比性。

### §3.4.4 Extended BPF

**来历**：BPF 最初是 Berkeley Packet Filter，1992 年的一个冷门技术，用来改善抓包工具的性能。
2013 年 Alexei Starovoitov 提议重写，2014 年随 Linux 内核发布，
使 BPF 变成一个**通用执行引擎**——可用于网络、可观测性与安全。

**构成**：指令集 + 存储对象（**maps**）+ **helper 函数**；因有虚拟指令集规范，可以视为一台虚拟机。
BPF 程序跑在**内核态**，被配置为在各种事件上运行：**socket 事件、tracepoints、USDT 探针、
kprobes、uprobes、perf_events**。

> BPF bytecode must first pass through a verifier that checks for safety, ensuring that the BPF program will not crash or corrupt the kernel.

还可用 **BTF（BPF Type Format）**理解数据类型与结构；输出可以走 **perf ring buffer**（高效地输出逐事件数据）
或 **maps**（适合统计）。

**为什么它对性能分析重要**：它给现有的内核事件源**加上了可编程性**。
一个 BPF 程序可以**记录 I/O 的开始与结束时间戳**来测耗时，并写进自定义直方图——
这正是本书大量 BPF 工具的原理（前端是 BCC 与 bpftrace，见第 15 章）。

**判据**：**能用事件时间戳算出来的东西，就不要用计数器差值算**——
BPF 让「每个 I/O 的真实耗时分布」成为可能，这是第 9 章里「平均值会隐藏多峰分布」的正面解法。

## §3.5 其他主题

### §3.5.1 PGO 内核

**PGO（profile-guided optimization，又称 FDO）** 用 CPU profile 信息改进编译器的决策，
可以用于内核构建，流程是：**在生产中采 CPU profile → 基于该 profile 重新编译内核 → 部署新内核**。
JVM 这类运行时自动做这件事（配合 JIT 重编译热点方法）；做 PGO 内核则要手工走完上述三步。

相关编译优化是 **LTO**（链接时优化，把整个二进制一次性编译以跨程序优化）。
Windows 内核大量使用 LTO 与 PGO，**PGO 带来 5% 到 20% 的提升**；Google 也用 LTO 与 PGO 内核。

内核 PGO 通常需要跑一个**带插桩的**内核来采 profile；Google 发布的 **AutoFDO** 工具
免掉了这个特殊内核：**用 `perf(1)` 从普通内核采 profile**，再转换成编译器能用的格式。

**判据**：PGO 的收益**绑定具体负载**——它优化的是「那个负载的热路径」。
把别人的 PGO 内核拿来跑你的负载，是没有依据的。

### §3.5.2 Unikernels

**机制**：unikernel 是**单应用机器镜像**，把内核、库、应用软件合在一起编译，
通常跑在**单一地址空间**里（可以是硬件 VM，也可以是裸机）。
好处：**更少的指令文本 ⇒ 更高的 CPU 缓存命中率**，以及更少的安全漏洞面。

**代价（本节最该记的一句）**：

> This also creates a problem: there may be no SSH, shells, or performance tools available for you to log in and debug the system, nor any way to add them.

原文还提到他自己为证明可行性，从 **Xen dom0** 里写了一个粗略的 CPU profiler 去分析 domU 的 unikernel，
并做出了 CPU 火焰图。

**判据**：unikernel 的取舍是「**性能与攻击面**」换「**可运维性**」。
没有 shell、没有性能工具的环境，**事故时的排查手段为零**——这在生产上必须先解决。

### §3.5.3 微内核与混合内核

**微内核**：内核软件保持最小，只保留内存管理、线程管理、进程间通信（IPC）等 essentials；
**文件系统、网络栈、驱动都放到用户态**实现。好处是这些用户态组件**更容易修改与替换**——
就像你可以选择装哪个数据库或 web 服务器一样，你也能选择装哪套网络栈。
而且**容错性更好：驱动崩了不会拖垮整个内核**。代表：QNX、Minix 3。

**代价**：**额外的 IPC 步骤**会降低 I/O 等操作的性能。

**混合内核**是两者的折中：把**性能关键的服务搬回内核态**（直接函数调用而非 IPC，像单体内核那样），
但**保留微内核的模块化与容错性**。代表：Windows NT 内核、Plan 9 内核。

**判据**：**「模块化」与「直接调用」不可兼得**：驱动在用户态 = 好换好容错，但每次 I/O 多几次 IPC。
判断标准是**该路径是否在热路径上**——热路径搬回内核，冷路径留在用户态。

### §3.5.4 分布式操作系统

**机制**：分布式操作系统把**一个操作系统实例**跑在一组通过网络互联的独立计算节点上，
每个节点上常用**微内核**。代表：Plan 9、Inferno。

原文引 Rob Pike 的话指出这个模型虽 innovative，但**未获广泛使用**。
今天云上扩展计算节点的常见模型是：**在一组相同的 OS 实例之间做负载均衡**，按负载扩缩（第 11 章）。

**判据**：本章的立场很明确——**分布式 OS 是历史选项，不是今天的默认选择**；
云上的等价物是「多个独立 OS 实例 + 负载均衡」。

## §3.6 内核比较：为什么「哪个内核最快」是个陷阱

**机制**：哪个内核最快？答案**部分取决于 OS 配置、负载，以及内核参与的程度**。
原文的总体判断是 Linux 会胜出，理由是它在性能改进、应用与驱动支持、以及庞大社区上的投入；
TOP500 超级计算机榜单自 1993 年起跟踪，**2017 年起 100% 是 Linux**。
也有例外——**Netflix 云上用 Linux，CDN 用 FreeBSD**（FreeBSD OCA 团队的 kernel 改进带来的收益，
2019 年做过 Linux 5.0 与 FreeBSD 的生产对比）。

**但本节最该记住的是这句警告**：

> Kernel performance is commonly compared using micro-benchmarks, and this is error-prone.

微基准可能发现某内核在**某个系统调用**上快得多，而那个系统调用**在生产负载里根本不用**；
或者用了，但**带了些微基准没测的标志位**，而那些标志位**对性能影响极大**。
准确比较内核性能是资深性能工程师的活，可能要花几周（方法论见第 12 章）。

**还有一条容易忽略的前提**：Linux 上性能特性与可调项极多，为每个负载配置与调优已成为负担；
原文见过**大量未调优的部署**。所以比较内核时先问：**每个内核都调优了吗？**

**判据**：任何「A 内核比 B 内核快 N%」的结论，若没有交代**负载、可调参数、具体路径**，
一律当作无效信息。

## 章末选型判据

| 场景 | 怎么做 | 别怎么做 |
|---|---|---|
| 要解释「系统调用开销」 | 拆三栏：模式切换次数×成本、上下文切换次数×成本、阻塞等待时间 | 把阻塞等待也算成「开销」 |
| 要降低系统调用**频率** | 先用 tracing 数「每请求多少次 syscall」，再改调用方式（mmap 代替 brk 等） | 凭感觉优化；也**别忘了报 syscall 率**——它是 KPTI 成本的放大器 |
| 要消除模式切换 | 用户态 syscalls（vDSO）、内存映射、kernel bypass、内核态应用 | 以为「所有系统调用都必须进内核」 |
| 追一个延迟尖刺 | 先看 **irqsoff**（关中断时长），再看调度延迟，最后看上下文切换 | 先怀疑应用代码 |
| 怀疑内核干扰了计算密集型负载 | 查 CPU 竞争与 CPU 放置（缓存热度、NUMA 亲和） | 以为「应用在用户态跑就与内核无关」 |
| 配低延迟系统 | `CONFIG_PREEMPT` 或 `CONFIG_PREEMPT_VOLUNTARY`；并确认大页已启用 | 只报内核版本不报抢占配置 |
| 配吞吐型批处理 | `CONFIG_PREEMPT_NONE`（吞吐更好、延迟更差） | 盲目照抄低延迟配置 |
| 「进程卡」的排查 | 先用 delay accounting 分开「等 CPU」与「算得慢」；再看栈 | 一上来就调内核参数 |
| 怀疑 TLB 是瓶颈 | 看 syscall 率、是否在有 KPTI 的内核上、大页是否生效、PCID 是否可用 | 把 TLB flush 成本归进「上下文切换」一笔账 |
| 容器里做性能归因 | 用 cgroup 层的计数器（cgroup v2）确认「这个容器用了多少」 | 直接看系统级 CPU 百分比就下结论 |
| 判断该用微内核还是单体内核 | 按「该路径是否在热路径上」决定驱动放用户态还是内核态 | 认为「模块化」与「直接调用」可以兼得 |
| 考虑 unikernel | 先确认**可运维性**方案（没有 shell/工具时怎么排查） | 只看「缓存命中率高、攻击面小」 |
| 比较两个内核 | 用生产负载 + 相同可调参数 + 相同调用路径 | 用微基准的单个系统调用数字下结论 |
| 用 PGO 内核 | 针对**自己的负载**采 profile 重编译 | 直接沿用别人的 PGO 内核 |
| 优化系统启动时间 | `systemd-analyze critical-chain` 看关键链 | 按「哪个服务看着慢」去猜 |
