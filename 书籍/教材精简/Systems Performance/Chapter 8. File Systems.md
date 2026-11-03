---
title: Chapter 8. File Systems（精读）
category: 性能工程
tags:
  - 文件系统
  - 页缓存
  - 预取
  - 写回缓存
  - 延迟分析
  - VFS
difficulty: 进阶
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）第8章 File Systems"
source_section: "精读 §8.1 / §8.2.1 / §8.2.2 / §8.2.3 / §8.3.1 / §8.3.2 / §8.3.3 / §8.3.4 / §8.3.5 / §8.3.6 / §8.3.7 / §8.3.8 / §8.3.9 / §8.3.10 / §8.3.11 / §8.3.12 / §8.3.13 / §8.3.14 / §8.3.15 / §8.3.16 / §8.4.1 / §8.4.2 / §8.4.3 / §8.4.4 / §8.4.5 / §8.4.6 / §8.5.1 / §8.5.2 / §8.5.3 / §8.5.4 / §8.5.5 / §8.5.6 / §8.5.7 / §8.5.8 / §8.6.1 / §8.6.2 / §8.6.3 / §8.6.4 / §8.6.5 / §8.6.6 / §8.6.7 / §8.6.8 / §8.6.9 / §8.6.10 / §8.6.11 / §8.6.12 / §8.6.13 / §8.6.14 / §8.6.15 / §8.6.17 / §8.6.18 / §8.7.1 / §8.7.2 / §8.7.3 / §8.8.1 / §8.8.2 / §8.8.3；§8.9 Exercises 与 §8.10 References 不立篇（练习题与参考文献是全书共用的自测/索引材料，不承载机制陈述）；§8.2 / §8.3 / §8.4 / §8.5 / §8.6 / §8.7 / §8.8 下的无编号子标题（Unrelated / Indirect / Implicit / Deflated / Inflated / Example / Transaction Cost / One-Liners / Syscall Tracing / VFS Tracing / File System Internals / ZFS 等）按原文文字引用，不编 § 号；原文稿 §8.6 的编号从 §8.6.15 直接跳到 §8.6.17，原书没有 §8.6.16 这一节，不补造"
skills:
  - obsidian-markdown
---

# Chapter 8. File Systems

> **小节地图**：本章讲的是**文件系统这一层**的性能方法论——术语口径 → 三个模型 → 十六个概念 → 架构 → 八种方法论 → 可观测工具 → 实验 → 调优。
> 立篇：§8.1（术语）、§8.2（接口 / 缓存 / 二级缓存三个模型）、§8.3（§8.3.1–§8.3.16，本章主体：延迟、缓存、预取、写回、同步写、Direct I/O、内存映射、元数据、逻辑与物理 I/O、操作不等价、时间戳、容量）、§8.4（I/O 栈 / VFS / 四类缓存 / 文件系统特性 / 文件系统类型 / 卷与池）、§8.5（磁盘分析、延迟分析、工作负载刻画、性能监控、静态调优、缓存调优、工作负载隔离、微基准）、§8.6（十七节可观测工具）、§8.7（即席测试 / 微基准工具 / 清缓存）、§8.8（应用调用 / ext4 / ZFS）。
> **不立篇**：§8.9 Exercises、§8.10 References（自测与文献索引，不承载机制陈述）。
> ⚠️ 原文稿 §8.6 的编号从 §8.6.15 直接跳到 §8.6.17（原书没有 §8.6.16 这一节），本篇照抄真实编号，不补造。
> 底层依赖：磁盘层见 [[Chapter 9. Disks]]，页缓存与内存见 [[Chapter 7. Memory]]，方法论骨架见 [[Chapter 2. Methodologies]]，内核与系统调用见 [[Chapter 3. Operating Systems]]，基准测试方法见 [[Chapter 12. Benchmarking]]。
> 同层对照：[[Chapter 5. Applications]]（应用侧 I/O 形态）、[[Chapter 6. CPUs]]（CPU 侧成本）、[[Chapter 11. Cloud Computing]]（云上存储与多租户干扰）、[[Chapter 14. Ftrace]]（tracepoint 机制底座）。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §8.3.1 延迟是首要指标 | 08-Chapter 8. File Systems.md L139 | File system latency is the primary metric of file system performance, measured as the time from a logical file system request to its completion. It includes time spent in the file system and disk I/O subsystem, and waiting on disk devices—the physical I/O. Application threads often block during an application request in order to wait for file system requests to complete—in this way, file system latency directly and proportionally affects application performance. |
| §8.3.2 缓存占内存是正常的 | 08-Chapter 8. File Systems.md L149 | Over time, the cache grows, while free memory for the operating system shrinks. This can alarm new users, but is perfectly normal. The principle is: If there is spare main memory, do something useful with it. When applications need more memory, the kernel should quickly free it from the file system cache for use. |
| §8.3.3 碎片 | 08-Chapter 8. File Systems.md L199 | Due to the performance characteristics of certain storage devices (described in Chapter 9 , Disks ), file systems have historically attempted to reduce random I/O by placing file data on disk sequentially and contiguously. The term fragmentation describes what happens file systems do this poorly, causing files to be scattered over a drive, so that sequential logical I/O yields random physical I/O. |
| §8.3.4 预取 | 08-Chapter 8. File Systems.md L207 | Prefetch is a common file system feature for solving this problem. It can detect a sequential read workload based on the current and previous file I/O offsets, and then predict and issue disk reads before the application has requested them. This populates the file system cache, so that if the application does perform the expected read, it results in a cache hit (the data needed was already in the cache). An example scenario is as follows: |
| §8.3.6 写回缓存 | 08-Chapter 8. File Systems.md L235 | Write-back caching is commonly used by file systems to improve write performance. It works by treating writes as completed after the transfer to main memory, and writing them to disk sometime later, asynchronously . The file system process for writing this “dirty” data to disk is called flushing . An example sequence is as follows: |
| §8.3.7 同步写 | 08-Chapter 8. File Systems.md L253 | A synchronous write completes only when fully written to persistent storage (e.g., disk devices), which includes writing any file system metadata changes that are necessary. These are much slower than asynchronous writes (write-back caching), since synchronous writes incur disk device I/O latency (and possibly multiple I/O due to file system metadata). Synchronous writes are used by some applications such as database log writers, where the risk of data corruption from asynchronous writes is unacceptable. |
| §8.3.8 Direct I/O | 08-Chapter 8. File Systems.md L273 | Direct I/O allows applications to use a file system but bypass the file system cache, for example, by using the O_DIRECT open(2) flag on Linux. This is similar to synchronous writes (but without the guarantees that O_SYNC offers), and it works for reads as well. It isn’t as direct as raw device I/O, since mapping of file offsets to disk offsets must still be performed by file system code, and I/O may also be resized to match the size used by the file system for on-disk layout (its record size) or it may error (EINVAL). Depending on the file system, this may not only disable read caching and write buffering but may also disable prefetch. |
| §8.3.10 内存映射 | 08-Chapter 8. File Systems.md L291 | I’ve noticed a tendency to try using mmap(2) to solve file system performance issues without first analyzing them. If the issue is high I/O latency from disk devices, avoiding the small syscall overheads with mmap(2) may accomplish very little, when the high disk I/O latency is still present and dominant. |
| §8.3.11 元数据 | 08-Chapter 8. File Systems.md L297 | While data describes the contents of files and directories, metadata describes information about them. Metadata may refer to information that can be read from the file system interface (POSIX) or information needed to implement the file system on-disk layout. These are called logical and physical metadata, respectively. |
| §8.3.12 逻辑与物理 I/O | 08-Chapter 8. File Systems.md L319 | File systems do much more than present persistent storage (the disks) as a file-based interface. They cache reads, buffer writes, map files to address spaces, and create additional I/O to maintain the on-disk physical layout metadata that they need to record where everything is. This can cause disk I/O that is unrelated, indirect, implicit, inflated, or deflated as compared to application I/O. Examples follow. |
| §8.3.13 操作不等价 | 08-Chapter 8. File Systems.md L405 | As is clear from the previous sections, file system operations can exhibit different performance based on their type. You can’t tell much about a workload of “500 operations/s” from the rate alone. Some operations may return from the file system cache at main memory speeds; others may return from disk and be orders of magnitude slower. Other determinant factors include whether operations are random or sequential, reads or writes, synchronous writes or asynchronous writes, their I/O size, whether they include other operation types, their CPU execution cost (and how CPU-loaded the system is), and the storage device characteristics. |
| §8.4.2 VFS 术语 | 08-Chapter 8. File Systems.md L491 | The terminology used by the Linux VFS interface can be a little confusing, since it reuses the terms inodes and superblocks to refer to VFS objects—terms that originated from Unix file system on-disk data structures. The terms used for Linux on-disk data structures are usually prefixed with their file system type, for example, ext4_inode and ext4_super_block. The VFS inodes and VFS superblocks are in memory only. |
| §8.4.3 Dcache 负缓存 | 08-Chapter 8. File Systems.md L537 | The Dcache also performs negative caching , which remembers lookups for nonexistent entries. This improves the performance of failed lookups, which commonly occur when searching for shared libraries. |
| §8.4.4 写时复制 | 08-Chapter 8. File Systems.md L575 | This helps maintain file system integrity in the event of a system failure, and also improves performance by turning random writes into sequential ones. |
| §8.5.2 syscall 层混杂 | 08-Chapter 8. File Systems.md L865 | Syscalls catch all file system types, including non-storage file systems (statistics, sockets), which may be confusing unless filtered. Adding to the confusion, there may also be multiple syscalls for the same file system function. For example, for read, there may be read(2), pread64(2), preadv(2), preadv2(2), etc., all of which need to be measured. |
| §8.5.2 事务成本 | 08-Chapter 8. File Systems.md L895 | percent time in file system = 100 * total blocking file system latency/application transaction time |
| §8.5.8 工作集 | 08-Chapter 8. File Systems.md L1069 | A critical factor when micro-benchmarking file systems is the working set size (WSS): the volume of data that is accessed. Depending on the benchmark, this may be the total size of the files in use. A small working set size may return entirely from the file system cache in main memory (DRAM), unless a direct I/O flag is used. A large working set size may return mostly from storage devices (disks). The performance difference can be multiple orders of magnitude. Running a benchmark against a newly mounted file system and then a second time after caches have been populated and comparing the results of the two is often a good illustration of WSS. (Also see Section 8.7.3 , Cache Flushing .) |
| §8.6.18 单值误导 | 08-Chapter 8. File Systems.md L2216 | The distribution of file system latency is expected to be bimodal: one mode at low latency for file system cache hits, and another at high latency for cache misses (storage device I/O). For this reason, representing the distribution as a single value—such as a mean, mode, or median—is misleading. |
| §8.7.3 清缓存 | 08-Chapter 8. File Systems.md L2349 | It can be especially useful to free everything (3) before other benchmark runs, so that the system begins in a consistent state (a cold cache), helping to provide consistent benchmark results. |
| §8.8.3 ZFS 记录大小 | 08-Chapter 8. File Systems.md L2613 | The most important parameter to tune is usually record size, to match the application I/O. It usually defaults to 128 Kbytes, which can be inefficient for small, random I/O. Note that this does not apply to files that are smaller than the record size, which are saved using a dynamic record size equal to their file length. Disabling atime can also improve performance if those timestamps are not needed. |

> **书里没说的**（本篇有几处推论，原文没有，别当成原文观点）：
> - **「1 字节写放大成 128 KB」的倍率是本篇按原文数字算的**：原文只说 "multiple reads (128 Kbytes in total) and more writes (over 128 Kbytes)"，没给出放大倍数。
> - **「五类物理 I/O ↔ 工具」的映射是本篇整理的**：§8.3.12 的 Unrelated / Indirect / Implicit / Deflated / Inflated 分类是原文的，但原文没有把它和 §8.6 的工具逐条对应起来。
> - **「Linux 缺 FS 统计 ⇒ 必须靠 BPF」这根因果是本篇的连线**：原文 §8.5.4 只说 Linux 通常没有现成的文件系统操作统计（例外是 NFS 的 nfsstat(8)），§8.6 才说 "Many of the tracing tools are BPF-based"；把两句连成一条因果是本篇的读法。
> - **「平均延迟指标在缓存型负载上基本失效」是本篇的强化表述**：原文说命中率超过 99% 时 per-interval averages 会被缓存命中延迟主导，本篇把它推成「必须看分布」这一操作结论。
> - **「trace VFS 而不是 trace read(2)」是原文给的第四条解法**（§8.6.15），本篇把它与 §8.5.2 的四层测量表连起来，是本篇的连线。

## §8.1 术语：先把口径钉死

这一节是全章的**口径表**，也是后面所有分析的前提。文件系统的性能讨论里，最贵的错误
不是「算错了」，而是**把两件不同的事当成同一件**。本章要分清的核心是三条：

| 词 | 原文定义要点 | 关键陷阱 |
|---|---|---|
| **Logical I/O** | 应用发给文件系统的 I/O | 只看它，会把「磁盘在转」误当成「应用在等」 |
| **Physical I/O** | 文件系统直接发给磁盘的 I/O | 只看它，会把后台 flush 当成应用延迟 |
| **File system cache** | 主存（通常是 DRAM）里用来缓存文件系统内容的一块区域 | 它的膨胀会挤掉「空闲内存」，但这是正常的 |
| **Operations** | read(2) / write(2) / open(2) / close(2) / stat(2) / mkdir(2) 等请求 | open/close **不算 I/O**，但会更新元数据、可能引起间接磁盘 I/O |
| **Block size** | 又称 record size，文件系统在盘上组织数据的组大小 | 与磁盘设备的块大小、系统的页大小是三个不同的东西 |
| **inode** | 存放权限、时间戳、数据指针等元数据的数据结构 | VFS 的 inode 与盘上的 inode 不是同一个（见 §8.4.2） |
| **VFS** | 内核里抽象并支持不同文件系统类型的接口 | 它的测量点会混进非存储型文件系统 |
| **Volume / Volume manager** | 一台存储之上的一个实例；管理物理存储、创建虚拟卷的软件 | 卷与池（pool）的隔离语义不同（见 §8.4.6） |

原文对 **I/O 口径**的划定尤其要注意：它明确把 `open(2)` 与 `close(2)` 排除在 I/O 之外，
但紧接着加了一句「although those calls update metadata and can cause indirect disk I/O」——
**这两个调用会引起间接的磁盘 I/O**。这正是 §8.3.12 讲「间接 I/O」的伏笔。

**判据**：任何一份文件系统性能报告，第一件事是**声明它量的是逻辑 I/O 还是物理 I/O**。
两者数字对不上不是 bug，是定义不同。

## §8.2 模型：三个简化模型

这一节用三个图把整章的机制铺开，是后面 §8.3 的骨架。

### §8.2.1 文件系统接口

模型：把文件系统看成**黑箱**，只盯对象操作的延迟。原文给了一张接口图，并标注了
逻辑操作与物理操作各自发生的位置（详见 §8.3.12）。

内核可能提供更多变体：Linux 有 `readv(2)`、`writev(2)`、`openat(2)` 等。
**「研究文件系统性能可以把它当黑箱，聚焦对象操作的延迟」**——这句话是全章方法论的起点
（对应 §8.5.2 的延迟分析）。

### §8.2.2 文件系统缓存

模型：主存里一块通用缓存。读操作要么**命中缓存**（cache hit），要么**未命中**
（cache miss）去磁盘取；未命中的数据会被放进缓存，即**预热**缓存。

同一个缓存还可能**缓冲写**（buffer writes），延后落盘——具体机制随文件系统类型而异（§8.4）。
内核通常也提供**绕过缓存**的方式（§8.3.8 的 Raw / Direct I/O）。

### §8.2.3 二级缓存

模型：在主存之外再加一层**二级缓存**，可以是任意类型的内存（原文画的是闪存）。
这一层是作者本人 2007 年为 ZFS 首创的——即 **L2ARC**。

## §8.3 概念：十六个承重机制

这一节是本章的主体，十六条概念各自解释一类性能现象。读法是「**现象 → 机制**」。

### §8.3.1 文件系统延迟：首要指标

> File system latency is the primary metric of file system performance, measured as the time from a logical file system request to its completion. It includes time spent in the file system and disk I/O subsystem, and waiting on disk devices—the physical I/O. Application threads often block during an application request in order to wait for file system requests to complete—in this way, file system latency directly and proportionally affects application performance.

三个要点：① 延迟 = **从逻辑请求到完成**；② 它包含文件系统内的时间 + 磁盘 I/O 子系统 + 等设备的
**物理 I/O** 时间；③ 应用线程通常在请求期间**阻塞**，所以文件系统延迟**直接等比例**地影响应用性能。

**反例：什么时候延迟不影响应用**（原文列了三种）：

- 用了**非阻塞 I/O**（§8.3.9）；
- **预取**（§8.3.4）——预取发起的读不是应用要的；
- I/O 是由**异步线程**发起的（例如后台 flush 线程）。

识别这三种情况的办法：应用若提供细粒度文件系统指标，可以直接看；否则用**内核 tracing 工具**
拿到引发该逻辑 I/O 的**用户态栈**，再回溯到是哪段应用代码发的。

**原文给的一条硬警告**：操作系统历来不把文件系统延迟做成可观测项，只给**磁盘设备级统计**。
而设备级统计常常与应用性能无关、甚至**具有误导性**——典型的例子是**后台 flush**：
从设备统计看像是一阵一阵的高延迟磁盘 I/O，看着很吓人，**但没有应用在等它们完成**。

**判据**：拿到磁盘繁忙率就下结论说「磁盘是瓶颈」之前，先问一句：
**这些磁盘 I/O 有对应的应用在阻塞吗**（§8.5.2 的事务成本公式可以量化回答）。

### §8.3.2 缓存：占内存是正常的

> Over time, the cache grows, while free memory for the operating system shrinks. This can alarm new users, but is perfectly normal. The principle is: If there is spare main memory, do something useful with it. When applications need more memory, the kernel should quickly free it from the file system cache for use.

机制：文件系统用主存做缓存提升性能。对应用是**透明**的——逻辑 I/O 延迟大幅下降，
因为数据从主存返回而不是从慢得多的磁盘。

**缓存占内存不是泄漏**：原则是「**有空闲主存就拿来做点有用的事**」；
等应用需要内存了，内核会**快速从文件系统缓存里释放**。

文件系统用缓存提升**读**性能，用**缓冲**提升**写**性能。典型上有多类缓存共存
（原文 Table 8.1 摘录）：

| 缓存 | 例子 |
|---|---|
| Page cache | 操作系统页缓存 |
| File system primary cache | ZFS ARC |
| File system secondary cache | ZFS L2ARC |
| Directory cache | dentry cache |
| inode cache | inode cache |
| Device cache | ZFS vdev |
| Block device cache | Buffer cache |

完整清单见 [[Chapter 3. Operating Systems]]（含应用级与设备级缓存）。

### §8.3.3 随机与顺序 I/O

定义：按**每次 I/O 的文件偏移**划分。顺序 I/O 每次偏移接上一次结束处；随机 I/O
彼此之间看不出关系。**「随机文件系统负载」也可以指随机访问大量不同的文件**。

机制：受存储设备性能特性支配，文件系统历来**试图把随机 I/O 减少**——把文件数据
在盘上顺序且连续地摆放。摆放不好的后果就是**碎片（fragmentation）**：

> Due to the performance characteristics of certain storage devices (described in Chapter 9 , Disks ), file systems have historically attempted to reduce random I/O by placing file data on disk sequentially and contiguously. The term fragmentation describes what happens file systems do this poorly, causing files to be scattered over a drive, so that sequential logical I/O yields random physical I/O.

**碎片 = 顺序的逻辑 I/O 变成了随机的物理 I/O**。注意这一定义的关键：它说的是
**逻辑与物理的错配**，不是「盘上不连续」这表象。

文件系统会**测量逻辑 I/O 的访问模式**，识别出顺序负载，然后用**预取或预读**去改善性能。
这对**旋转盘**很有用，**对闪存帮助不大**（闪存本身没有寻道延迟）。

### §8.3.4 预取

问题场景：顺序读大量文件数据（例如文件系统备份）。数据可能**大到装不进缓存**，
或者**只读一次**、按缓存淘汰策略不太可能被保留——两种情况下**缓存命中率都很低**，负载表现差。

机制：

> Prefetch is a common file system feature for solving this problem. It can detect a sequential read workload based on the current and previous file I/O offsets, and then predict and issue disk reads before the application has requested them. This populates the file system cache, so that if the application does perform the expected read, it results in a cache hit (the data needed was already in the cache). An example scenario is as follows:

原文给的六步场景（照抄顺序）：

1. 应用发起 `read(2)`，控制权交给内核；
2. 数据未缓存，文件系统把读发给磁盘；
3. **上一次的文件偏移指针与当前位置比较**，若是顺序的，文件系统追加发出**预取读**；
4. 第一次读完成，内核把数据与控制权交回应用；
5. 任何预取读完成，**把缓存填好**，供未来的应用读用；
6. 未来的顺序应用读**从内存缓存快速完成**。

**双向性**：预取检测得好 ⇒ 顺序读性能显著提升，磁盘能跑在应用请求前面
（前提是**带宽够**）；检测得差 ⇒ **发出应用并不需要的 I/O**，污染缓存、浪费磁盘与 I/O 传输资源。
文件系统通常允许**调优预取**。

### §8.3.5 Read-Ahead

历史上手册里 **prefetch 也叫 read-ahead**。但 Linux 把 read-ahead 这个词用在了**系统调用**
`readahead(2)` 上——它允许应用**显式地预热文件系统缓存**。

**术语陷阱（推演）**：在本章的语义里，「预取」是**文件系统自动**做的事，
「`readahead(2)`」是**应用主动**做的事；读 Linux 文档时两者会混着出现。

### §8.3.6 写回缓存

> Write-back caching is commonly used by file systems to improve write performance. It works by treating writes as completed after the transfer to main memory, and writing them to disk sometime later, asynchronously . The file system process for writing this “dirty” data to disk is called flushing . An example sequence is as follows:

机制：**转移到主存即视为写完成**，稍后**异步**落盘。把脏数据写到磁盘的过程叫 **flushing**。

原文的五步序列：应用 `write(2)` → 数据从应用地址空间**拷贝进内核** → 内核**把该系统调用视为完成**、
把控制权交回应用 → 稍后某个**异步内核任务**发现这些数据并发出磁盘写。

**代价是可靠性**：主存是易失的，断电会丢脏数据；也可能**写得不完整**，留下损坏的盘上状态。
若**文件系统元数据**损坏，文件系统可能**再也挂不上**，只能靠备份恢复，造成长时间停机；
更糟的是损坏影响到应用读到的文件内容。

平衡方案：默认提供写回缓存，同时提供**同步写**选项绕过它、直写持久设备（§8.3.7）。

### §8.3.7 同步写

> A synchronous write completes only when fully written to persistent storage (e.g., disk devices), which includes writing any file system metadata changes that are necessary. These are much slower than asynchronous writes (write-back caching), since synchronous writes incur disk device I/O latency (and possibly multiple I/O due to file system metadata). Synchronous writes are used by some applications such as database log writers, where the risk of data corruption from asynchronous writes is unacceptable.

两个要点：① **只有完全落到持久存储才算完成**，包括必要的**文件系统元数据变更**；
② 因此**比异步写慢得多**，因为要承担设备 I/O 延迟，甚至因元数据产生**多次 I/O**。

典型用户：**数据库日志写入器**（数据损坏风险不可接受）。

两种形态：

| 形态 | 做法 | 特点 |
|---|---|---|
| **Individual Synchronous Writes** | 用 `O_SYNC` 打开，或其变体 `O_DSYNC` / `O_RSYNC`（Linux 2.6.31 起 glibc 把这两个映射为 `O_SYNC`） | 每个 I/O 都同步落盘；某些文件系统还有挂载选项可强制所有写同步 |
| **Synchronously Committing Previous Writes** | 在代码检查点用 `fsync(2)` **一次性提交此前的异步写** | **把写分组**，性能更好；还能借**写取消**避免多次元数据更新 |

**顺带的坑**：关闭文件句柄、或未提交缓冲区过多时也会触发提交落盘。前者常见于
**解包大量文件的压缩包时出现长时间停顿**，**尤其是走 NFS 时**。

**判据**：需要「不丢数据」时，先问「**能不能用 `fsync(2)` 分组提交，而不是每次都 `O_SYNC`**」——
原文明确说分组能提升性能，还能减少元数据更新次数。

### §8.3.8 Raw 与 Direct I/O

两种绕过缓存的形态：

- **Raw I/O**：**完全绕过文件系统**，直接对磁盘偏移发 I/O。数据库常用，因为它们能
  更好地管理并缓存自己的数据。代价是软件更复杂、管理更难——**常规的文件系统工具集
  用不了**（备份/恢复、可观测性都失灵）。
- **Direct I/O**：**仍然用文件系统，但绕过文件系统缓存**，Linux 上用 `open(2)` 的
  `O_DIRECT` 标志：

> Direct I/O allows applications to use a file system but bypass the file system cache, for example, by using the O_DIRECT open(2) flag on Linux. This is similar to synchronous writes (but without the guarantees that O_SYNC offers), and it works for reads as well. It isn’t as direct as raw device I/O, since mapping of file offsets to disk offsets must still be performed by file system code, and I/O may also be resized to match the size used by the file system for on-disk layout (its record size) or it may error (EINVAL). Depending on the file system, this may not only disable read caching and write buffering but may also disable prefetch.

三个必须记住的限定：① **它不像 `O_SYNC` 那样提供持久性保证**；② **文件偏移到磁盘偏移的映射
仍由文件系统代码完成**，所以它「不如 raw 设备 I/O 那样直接」；③ **I/O 尺寸会被调整**以匹配
文件系统的盘上记录大小，否则**报错 `EINVAL`**；④ **依文件系统而定，还可能关掉预取**。

**判据**：用 Direct I/O 之前先确认三件事——**记录大小与你的 I/O 尺寸对不对得上**、
**预取被关掉后顺序读会不会更糟**、**你打算怎么观测这块盘**（raw 工具链用不了）。

### §8.3.9 非阻塞 I/O

常态：文件系统 I/O 要么**立即完成**（例如命中缓存），要么**等待**（例如等磁盘）。
需要等待时，**应用线程阻塞并让出 CPU**，让别的线程跑。

需要非阻塞的场合：**避免线程创建的性能与资源开销**。做法是用 `open(2)` 的
`O_NONBLOCK` 或 `O_NDELAY` 标志，让读写**返回 `EAGAIN` 错误**而不是阻塞，由应用稍后重试。
（支持与否依文件系统而定——有些只对**咨询锁或强制锁**生效。）

另外还有**独立的异步 I/O 接口**：`aio_read(3)` / `aio_write(3)`；
Linux 5.1 新增了 **`io_uring`**，在易用性、效率和性能上都有改进。

应用侧的非阻塞 I/O 见 [[Chapter 5. Applications]]。

### §8.3.10 内存映射文件

机制：把文件映射到进程地址空间、**直接访问内存偏移**，可以绕开 `read(2)` / `write(2)`
系统调用的**系统调用执行与上下文切换开销**；如果内核支持把文件数据缓冲区**直接映射**到
进程地址空间，还能**避免数据被拷贝两次**。

映射用 `mmap(2)` 创建、`munmap(2)` 移除，可用 `madvise(2)` 调优（§8.8.1）。
有些应用在配置里提供「mmap 模式」开关，例如 Riak 用 mmap 做它的内存数据存储。

**作者的一句提醒（很重）**：

> I’ve noticed a tendency to try using mmap(2) to solve file system performance issues without first analyzing them. If the issue is high I/O latency from disk devices, avoiding the small syscall overheads with mmap(2) may accomplish very little, when the high disk I/O latency is still present and dominant.

**没有先分析就上 mmap，是最常见的误治**。若瓶颈是磁盘设备的高延迟，
省掉的那点系统调用开销**几乎没用**——高延迟依然主导。

多处理器系统上用映射还有一个缺点：**保持各 CPU 的 MMU 同步的开销**，
具体是**移除映射时的 CPU 跨核调用（TLB shootdowns）**。依内核与映射方式，
可以用**延迟 TLB 更新（lazy shootdowns）**来减小。

**判据**：上 mmap 之前先量一次 I/O 延迟的**量级**——若已是毫秒级（磁盘主导），
换 mmap 无意义；若是微秒级（CPU/系统调用主导），才有讨论价值。

### §8.3.11 元数据

> While data describes the contents of files and directories, metadata describes information about them. Metadata may refer to information that can be read from the file system interface (POSIX) or information needed to implement the file system on-disk layout. These are called logical and physical metadata, respectively.

**两类元数据**：

| 类别 | 定义 | 读写方 |
|---|---|---|
| **Logical metadata**（逻辑元数据） | 能从**文件系统接口（POSIX）**读到的信息 | **应用** |
| **Physical metadata**（物理元数据） | 维护**盘上布局**所必需的信息 | 文件系统自己 |

逻辑元数据又分两种触发表述：

- **显式**：读文件统计（`stat(2)`）、创建删除文件（`creat(2)`、`unlink(2)`）与目录
  （`mkdir(2)`、`rmdir(2)`）、设置文件属性（`chown(2)`、`chmod(2)`）；
- **隐式**：**访问时间戳更新**、目录修改时间戳更新、**已用块位图更新**、空闲空间统计。

**「metadata-heavy」负载**通常指**逻辑元数据**密集——例如 web server 大量 `stat(2)`
以确认文件自上次缓存以来没变，其频率**远高于读取文件数据内容**。

物理元数据的内容依文件系统而定，可能包括**超级块、inode、数据指针块（一级、二级……）、空闲链表**。

原文点明：**逻辑与物理元数据的存在，正是逻辑 I/O 与物理 I/O 有差别的原因之一**。

**判据**：`stat(2)` 数量远大于 `read(2)` 数量，是典型的**元数据重负载**——
这时该查的是 dentry/inode 缓存命中率与目录布局，而不是磁盘带宽。

### §8.3.12 逻辑 I/O 与物理 I/O：五类错配

> File systems do much more than present persistent storage (the disks) as a file-based interface. They cache reads, buffer writes, map files to address spaces, and create additional I/O to maintain the on-disk physical layout metadata that they need to record where everything is. This can cause disk I/O that is unrelated, indirect, implicit, inflated, or deflated as compared to application I/O. Examples follow.

原文给的五类错配（这一节是全章**最该背下来的分类表**）：

| 类别 | 定义 | 典型成因 |
|---|---|---|
| **Unrelated**（无关） | 与应用无关的磁盘 I/O | 其他应用；**其他租户**（虚拟化下可见）；其他内核任务（如重建软件 RAID、**异步校验**）；管理任务（如备份） |
| **Indirect**（间接） | 由应用引起、但**没有对应的应用 I/O** | **预取**；**缓冲**（写回缓存延后并合并写，有的系统缓冲几十秒后变成大而少的突刺） |
| **Implicit**（隐式） | 由**应用事件**（而非显式读写）触发的磁盘 I/O | **内存映射的 load/store 指令**——可能触发读或写，写还可能被缓冲后延后落盘；**由指令而非系统调用触发**，所以按 `read(2)` / `write(2)` 追根本找不到源头 |
| **Deflated**（缩量） | 磁盘 I/O **小于**应用 I/O，甚至不存在 | 缓存命中；**写取消**（同一偏移多次修改，落盘前合并成一次）；压缩；**合并（coalescing）**（顺序 I/O 合并后再发盘，减少的是**次数**不是总量）；内存文件系统（如 tmpfs，可能永不落盘） |
| **Inflated**（膨胀） | 磁盘 I/O **大于**应用 I/O（这是常态） | **文件系统元数据**；**记录大小**（向上取整膨胀字节数，或切碎膨胀次数）；**日志（journaling）**——写两遍，一遍日志一遍最终目的地；卷管理器的**校验 parity 读改写**；**RAID 膨胀**（额外 parity 或镜像副本） |

**原文的 1 字节写例子（照抄顺序，理解放大的来源）**：

1. 应用对已存在文件做 **1 字节写**；
2. 文件系统定位到该字节属于一个 **128 Kbyte 记录**，该记录**未被缓存**（但引用它的元数据被缓存了）；
3. 文件系统请求把该记录**从盘上读上来**；
4. 磁盘设备层把 128 Kbyte 的读**拆成更小的、适合设备的读**；
5. 磁盘执行**多次小读，合计 128 Kbytes**；
6. 文件系统用新字节**替换记录里的那 1 个字节**；
7. 稍后，文件系统或内核请求把 128 Kbyte 的脏记录**写回**；
8. 磁盘写入 128 Kbyte 记录（必要时拆分）；
9. 文件系统写**新的元数据**（例如更新引用以支持写时复制，或更新访问时间）；
10. 磁盘执行**更多写**。

结论：**应用只做了 1 字节写，磁盘却做了 128 Kbyte 的多次读和超过 128 Kbyte 的多次写**。

**这一节的应用**：磁盘层指标与文件系统层指标**对不上是正常的**，对不上时要能说出
**属于哪一类错配**——这决定了往哪查。

### §8.3.13 操作并不等价

> As is clear from the previous sections, file system operations can exhibit different performance based on their type. You can’t tell much about a workload of “500 operations/s” from the rate alone. Some operations may return from the file system cache at main memory speeds; others may return from disk and be orders of magnitude slower. Other determinant factors include whether operations are random or sequential, reads or writes, synchronous writes or asynchronous writes, their I/O size, whether they include other operation types, their CPU execution cost (and how CPU-loaded the system is), and the storage device characteristics.

**「500 ops/s」这个数字几乎没有信息量**。决定性因素清单：

1. 命中缓存还是落到磁盘（**数量级差异**）；
2. 随机还是顺序；
3. 读还是写；
4. 同步写还是异步写；
5. I/O 尺寸；
6. 是否夹带其他类型操作；
7. **CPU 执行成本**（以及**系统当前的 CPU 负载**）；
8. 存储设备特性。

原文用 ZFS 在一颗空闲的 Intel Xeon 2.4 GHz 多核上测出的**操作延迟**做示例（Table 8.2 摘录）：

| 操作 | 平均延迟（μs） |
|---|---|
| `open(2)`（inode 已缓存） | 2.2 |
| `close(2)`（无脏数据待刷） | 0.7 |
| `read(2)` 4 Kbytes（已缓存） | 3.3 |
| `read(2)` 128 Kbytes（已缓存） | 13.9 |
| `write(2)` 4 Kbytes（异步） | 9.3 |
| `write(2)` 128 Kbytes（异步） | 55.2 |

**注意这组数字的两个前提**：① **完全没碰存储设备**——它测的是**文件系统软件 + CPU 速度**
（有些特殊文件系统从不访问持久设备）；② **单线程**——并行 I/O 性能会受
**文件系统锁的类型与组织方式**影响。

**判据**：做任何「操作率」对比之前，先把**操作类型拆开**看（读/写/stat/open/close 分开计），
否则两个「都是 500 ops/s」的负载可能差两个数量级。

### §8.3.14 特殊文件系统

文件系统的意图通常是**持久存储数据**，但 Linux 上有若干**特殊类型**用于其他目的：
临时文件（`/tmp`）、内核设备路径（`/dev`）、系统统计（`/proc`）、系统配置（`/sys`）。

列出 Linux 上**不使用存储设备**的特殊文件系统：`grep '^nodev' /proc/filesystems`。

**判据**：这层直接解释了 §8.5.2 的一个坑——**在 syscall 层测量时，socket、procfs、
tmpfs 的操作会混进来**，必须过滤。

### §8.3.15 访问时间戳

很多文件系统支持**访问时间戳**，记录每个文件与目录被**读**（访问）的时间。
后果是：**每次读文件都要更新文件元数据**，从而**制造出一份写负载，消耗磁盘 I/O 资源**。
§8.8 讲怎么关掉这些更新。

有些文件系统会**推迟并合并**访问时间戳的写，以减少对活跃负载的干扰。

**判据**：读多写少的负载上，`atime` 是一笔**纯开销**；Linux 默认的 `relatime`
已经把更新量压到很小（§8.8.2）。

### §8.3.16 容量

文件系统**快满时性能会退化**，两个原因：

1. 写新数据时，**找空闲块需要更多 CPU 时间与磁盘 I/O**；
2. **空闲区域更小、更稀疏**，导致更小的 I/O 或随机 I/O，性能下降。

原文脚注给了 ZFS 的具体阈值：池存储超过某个阈值（**最初 80%，后来 99%**）时，
ZFS 会**切换到一种更慢的空闲块查找算法**。

严重程度取决于：文件系统类型、盘上布局、是否用**写时复制**、以及存储设备。

## §8.4 架构：I/O 栈、缓存、特性、类型、卷与池

这一节给的是**背景知识**，用途是「**知道该分析并调优文件系统的哪个组件**」。

### §8.4.1 文件系统 I/O 栈

模型：从应用与系统库 → 系统调用 → 内核的路径。**从系统调用直接到磁盘设备子系统的路径
是 raw I/O**；**经由 VFS 和文件系统的是文件系统 I/O**（其中 Direct I/O 跳过文件系统缓存）。

更高层的 I/O 栈图在 [[Chapter 3. Operating Systems]]，磁盘侧的细节在 [[Chapter 9. Disks]]。

### §8.4.2 VFS

VFS 为不同文件系统类型提供**统一接口**，起源于 SunOS，已成为文件系统的标准抽象。

**术语陷阱（这一节最该记住的）**：

> The terminology used by the Linux VFS interface can be a little confusing, since it reuses the terms inodes and superblocks to refer to VFS objects—terms that originated from Unix file system on-disk data structures. The terms used for Linux on-disk data structures are usually prefixed with their file system type, for example, ext4_inode and ext4_super_block. The VFS inodes and VFS superblocks are in memory only.

即：VFS 的 inode / superblock 是**内存对象**，盘上的对应物带**文件系统前缀**
（`ext4_inode`、`ext4_super_block`）。两者**同名不同物**。

VFS 的另一个用途是**测量任何文件系统的共同位置**——可以用操作系统提供的统计，
也可以用静态或动态插桩。

### §8.4.3 文件系统缓存：四类

Unix 最初只有 **buffer cache**（块设备接口层）来提升块设备访问性能；
如今 Linux 有多类缓存。

**Buffer Cache**：Unix 在块设备接口缓存磁盘块，是**独立、定长**的缓存。后来加入页缓存后，
两者之间**平衡不同负载很麻烦**，还有**双重缓存与同步**的开销。解法是**用页缓存来存放
buffer cache**——SunOS 引入的做法，叫 **unified buffer cache**。Linux 自 2.4 起同样如此
（所以图里 buffer cache 画的是虚线边框）。buffer cache 的功能仍在（提升块设备 I/O 性能），
**这个术语在 Linux 可观测工具里仍然出现**（例如 `free(1)`）。其大小是动态的，可从 `/proc` 观察。

**Page Cache**：1985 年 SunOS 虚拟内存重写时引入，后加入 SVR4 Unix。它缓存**虚拟内存页**
（包括映射的文件系统页），提升文件与目录 I/O 性能。相比 buffer cache 更高效，
因为后者**每次查找都要把文件偏移翻译成磁盘偏移**。大小动态：会用可用内存、需要时释放。
Linux 的页缓存同样动态，并有 **swappiness** 这个可调项在「淘汰页缓存」与「换出」之间取平衡
（见 [[Chapter 7. Memory]]）。

**脏页何时落盘**（原文列的四条触发原因）：

- **经过一个时间间隔（30 秒）**；
- **`sync(2)`、`fsync(2)`、`msync(2)` 系统调用**；
- **脏页太多**（`dirty_ratio` 与 `dirty_bytes` 两个可调项）；
- **页缓存里没有可用页**。

Linux 2.6.32 之前是一池 **pdflush** 线程（2 到 8 个，按需）；之后换成**每设备一个的
flusher 线程**（名为 `flush`），以更好地平衡各设备负载、提升吞吐。
内存吃紧时还有另一个内核线程 **kswapd**（page scanner）会调度脏页落盘以腾出内存页。

**Dentry Cache（Dcache）**：记住**目录项（`struct dentry`）到 VFS inode 的映射**，
类似早期的 Unix 目录名查找缓存（DNLC）。它改进的是**路径名查找**（例如 `open(2)`）性能：
遍历路径时每一步名字查找都能**直接命中 Dcache 拿到 inode 映射**，不必逐级走目录内容。
条目存**哈希表**里（按**父 dentry + 目录项名**哈希）以便快速、可扩展地查找。

近年的改进是 **RCU-walk**：尝试在**不更新 dentry 引用计数**的情况下走完路径名——
引用计数在多 CPU、高路径查找率下会造成缓存一致性导致的扩展性问题。
遇到**不在缓存里**的 dentry，RCU-walk **退回较慢的引用计数 walk（ref-walk）**
（查找与阻塞期间必须要引用计数）。繁忙负载下 dentry 数据大概率已在缓存，RCU-walk 一般能成功。

> The Dcache also performs negative caching , which remembers lookups for nonexistent entries. This improves the performance of failed lookups, which commonly occur when searching for shared libraries.

**负缓存**记住「查过、确实不存在」的条目——**失败的查找**（例如搜共享库时的路径试探）
因此也变快。Dcache 动态增长，内存紧张时**按 LRU 收缩**，大小可从 `/proc` 观察。

**Inode Cache**：存放 VFS inode（`struct inodes`），每个描述一个文件系统对象的属性，
其中很多由 `stat(2)` 返回。这些属性在文件系统负载里被频繁访问——**打开文件时检查权限**、
**修改时更新时间戳**。VFS inode 存**哈希表**（按 **inode 号 + 文件系统 superblock** 哈希），
不过**多数查找其实是经由 Dentry cache 完成的**。inode 缓存动态增长，**至少持有 Dcache 映射到的
全部 inode**；内存有压力时收缩，**丢弃没有对应 dentry 的 inode**。大小见
`/proc/sys/fs/inode*`。

### §8.4.4 文件系统特性

**Block vs. Extent**：

- **块式**：数据存在**定长块**里，由元数据块中的指针引用。大文件需要**大量块指针和元数据块**，
  且块的位置可能变得**分散**，导致随机 I/O。有些块式文件系统会尽量**连续摆放**来避免这点；
  另一种做法是**可变块大小**——文件变大时用更大的块，从而**降低元数据开销**。
- **扩展（extent）式**：为文件**预分配连续空间**，按需增长。扩展**长度可变**，代表一块或多块
  连续块。它**改善流式性能**，也能**改善随机 I/O**（数据局部化），并且**改善元数据性能**
  （要跟踪的对象更少），且**不浪费未使用块的空间**。

**Journaling（日志）**：

> A file system journal (or log ) records changes to the file system so that, in the event of a system crash or power failure, changes can be replayed atomically—either succeeding in their entirety or failing.

这让文件系统能**快速恢复到一致状态**。**没有日志的文件系统在崩溃时可能损坏**——
若某次变更的数据与元数据只写入了一部分；恢复需要**遍历所有文件系统结构**，
对**TB 级文件系统可能要花几个小时**。

日志是**同步写盘**的，有些文件系统可以配置**用独立设备**。有的日志**同时记录数据与元数据**
——**所有 I/O 写两遍**，消耗更多存储 I/O 资源；有的**只记录元数据**，靠**写时复制**保证数据完整性。

还有一种**只有日志的文件系统**：**日志结构文件系统（log-structured）**，
所有数据与元数据更新都写入**连续循环的日志**。这优化了写性能——**写总是顺序的，
且可以合并成更大的 I/O**。

**Copy-on-Write（写时复制）**：不覆写已有块，而是三步：① **把块写到新位置**（一份新副本）；
② **更新对新块的引用**；③ **把旧块加入空闲链表**。

> This helps maintain file system integrity in the event of a system failure, and also improves performance by turning random writes into sequential ones.

它同时买到两样：**故障时的一致性** + **把随机写变成顺序写**的性能。
**副作用**：文件系统接近容量上限时，COW 会让文件的盘上布局**碎片化**，性能下降
（HDD 尤甚）；**碎片整理（defragmentation）**（若该文件系统提供）可以恢复性能。

**Scrubbing（数据擦洗）**：**异步读所有数据块并校验 checksum**，尽早发现失效硬盘——
理想情况下趁故障还能被 RAID 救回时发现。但**擦洗的读 I/O 会伤性能**，
应当**低优先级发起**或**在低负载时段发起**。

**其他影响性能的特性**：快照、压缩、内建冗余、去重、TRIM 支持等。

### §8.4.5 文件系统类型

这一节汇总常见文件系统的**性能特性**。下表是原文各小节的机制要点对照：

| 类型 | 承重机制 |
|---|---|
| **FFS**（Berkeley fast file system） | **柱面组（cylinder groups）**：分区切成多个组，每组有自己的 inode 数组与数据块；文件 inode 与数据尽量留在**同一柱面组内**，减少寻道。块大小提高到 **4 Kbyte 最小值**（吞吐提升、间接指针块变少），小文件可用 **1 Kbyte 碎片**。**块交错（block interleaving）**：顺序文件块在盘上**留一个或多个块的间隔**，给内核与处理器时间去发下一个读——否则下一个块可能**在磁头就绪前就转过去了**，白白等一整圈 |
| **ext3** | **日志**：ordered mode（仅元数据）或 journal mode（元数据+数据），**崩溃后开机更快，无需 fsck**；还能通过**合并元数据写**改善部分写负载。**独立日志设备**：日志负载不与读负载争抢。**Orlov 块分配器**：把顶层目录分散到各柱面组，子目录与内容更容易共置，减少随机 I/O。**目录索引**：加哈希 B 树，加速目录查找 |
| **ext4** | **扩展（extents）**提升连续摆放、减少随机 I/O、加大顺序 I/O 尺寸；**预分配**（`fallocate(2)`）申请大概连续的空间，改善后续写性能；**延迟分配**：块分配推迟到刷盘时才做，让写能**成组**（配合 multiblock 分配器）从而减少碎片；**更快的 fsck**：标记未分配块与 inode 条目，缩短 fsck 时间。另有大容量、journal checksum、纳秒时间戳、快照。特性状态可查 `/sys/fs/ext4/features` |
| **XFS** | **分配组（AG）**：分区切成等大的 AG，可**并行访问**；各 AG 的元数据（inode、空闲块链表）**独立管理**以限制争用，而文件与目录可跨 AG。**扩展**、**日志**、**独立日志设备**、**条带化分配**（建在条带 RAID/LVM 上时，为数据与日志各给一个条带单元，让分配对齐底层硬件）、**延迟分配**（刷盘时才分配扩展；**在内存中为文件预留块**，保证刷盘时有空间）、**在线碎片整理**（XFS 用扩展与延迟分配防碎片，但某些负载与条件仍会碎片化） |
| **ZFS** | **池化存储**：所有设备放进一个池，从池里创建文件系统，**所有设备可并行**以拿最大吞吐与 IOPS；RAID 0/1/10/Z/Z2/Z3。**COW**：复制被修改的块，再成组顺序写。**事务组（TXG）日志**：改动按批提交，成或全不成，**盘上格式始终一致**。**ARC**：同时用 MRU 与 MFU 两种算法，靠 **ghost list** 元数据预测「若由它统治全部主存会怎样」，在两者间平衡主存。**智能预取**：对元数据、znode、vdev 分别用不同预取。**多预取流**：同一文件上多个流式读者会互相打断，ZFS 跟踪各预取流并允许新流加入。**快照**：COW 架构下几乎瞬时创建。**ZIO 流水线**：设备 I/O 由多级流水线处理，每级由线程池服务。**压缩**（`lzjb` 轻量，以 CPU 换 I/O）。**SLOG**：同步写可写到独立设备，避免与池盘争用；SLOG 只在系统故障时回放时被读。**L2ARC**：主存之外的二级缓存，缓存 SSD 上的**随机读**；**不缓冲写**，只含**已存在于池盘的干净数据**；也可复制 ARC 中的数据以便主存被扰动时更快恢复。**去重**：避免记录重复数据——好处是设备 I/O 减少，**坏处是哈希表装不进主存时设备 I/O 会被显著放大**，所以初版只面向「哈希表始终装得进主存」的负载。**⚠️ 代价**：ZFS 默认向存储设备发**缓存刷新（cache flush）命令**以保证断电时写已完成——这是完整性特性，但**会让必须等待刷新的 ZFS 操作产生延迟** |
| **btrfs** | 基于**写时复制 B 树**的现代文件系统 + 卷管理一体（与 ZFS 类似）。特性：池化存储、大容量、扩展、COW、卷伸缩、**子卷（subvolume）**、块设备增删、快照、克隆、压缩（zlib/LZO）、多种 checksum（crc32c、xxhash64、sha256、blake2b）。**在线平衡**：对象可在设备之间迁移以均衡负载；**按子卷的日志树**可用于日志化同步 COW 负载 |

**FFS 的历史教训（原文引用 McKusick 的实验，摘录）**：把块大小从 512 翻倍到 1024 后，
吞吐翻倍，但**磁盘带宽只用了约 4%**——主因是**空闲链表**最初有序，但**随着文件创建删除很快被打乱**，
最终**完全随机**，文件的块被随机分配到盘上，**每次块访问前都要先寻道**。
结果：早期传输率可达 175 KB/s，**几周正常使用后跌到 30 KB/s**。
**这是「碎片随时间劣化性能」的原始案例。**

### §8.4.6 卷与池

历史上文件系统建在**单盘或单分区**上。**卷（volume）与池（pool）**让文件系统建在**多盘**上，
可用不同 RAID 策略（见 [[Chapter 9. Disks]]）。

- **卷**：把多块盘呈现为**一块虚拟盘**，文件系统建在其上。**建在整盘（而非切片/分区）上时，
  卷可以隔离负载**，减少争用。Linux 上的卷管理软件是 **LVM**；硬件 RAID 卡也能提供卷。
- **池**：多块盘放进一个**存储池**，从池里创建**多个**文件系统。比卷**更灵活**——
  文件系统可以增删，**不受底层设备约束**。现代文件系统（ZFS、btrfs）都走这条路，LVM 也可实现。

**取舍**：池化存储能让**所有盘服务所有文件系统**（吞吐更高），但**负载不隔离**——
有时会用**多个池**来隔离，代价是灵活性（盘一开始就得归入某个池）。

卷/池场景下的性能注意事项：

| 项 | 要点 |
|---|---|
| **Stripe width** | 与负载匹配 |
| **Observability** | **虚拟设备的利用率会让人困惑**——要去看**底层的物理设备** |
| **CPU overhead** | 尤其在**做 RAID parity 计算**时；现代 CPU 快已缓解（也可**卸载到硬件 RAID 卡**） |
| **Rebuilding / resilvering** | 空盘加入 RAID 组（换盘）并被填满以加入该组的过程。**消耗 I/O 资源**，**可能持续数小时甚至数天** |

**Rebuilding 是一个正在变糟的问题**（原文原话）：**存储设备容量的增长速度快于其吞吐增长**，
于是**重建时间变长**，**重建期间发生故障或介质错误的风险也变大**。
可能的话，**对未挂载的盘做离线重建**可以缩短重建时间。


## §8.5 方法论：八种分析打法

原文 Table 8.3 把这八种分成两类：**观测类**（磁盘分析、延迟分析、工作负载刻画、
性能监控、静态调优、缓存调优）与**实验类**（微基准）；缓存调优与工作负载隔离同时带「调优」属性。

**作者建议的起始顺序**（可按环境调整）：**延迟分析 → 性能监控 → 工作负载刻画 →
微基准 → 静态性能调优**。

### §8.5.1 磁盘分析

传统排障策略是**忽略文件系统、只看磁盘性能**——假设**最坏的 I/O 就是磁盘 I/O**，
于是只看磁盘就等于「聚焦在预期的问题源上」。

**在简单文件系统与小缓存的年代这行得通；如今它令人困惑，并且会漏掉整类问题**
（§8.3.12 的五类错配）。

**判据**：如果你的分析只产出磁盘指标，那它**必然漏**掉缓存命中、缓冲、预取、元数据、
写取消、压缩这些全部效应。

### §8.5.2 延迟分析

**起手式**：先**测文件系统操作的延迟**。要包含**所有对象操作**，不只是 I/O
（例如**也要含 `sync(2)`**）。

延迟公式：`operation latency = time (operation completion) - time (operation request)`

**测点有四层**（原文 Table 8.4）：

| 层 | 优点 | 缺点 |
|---|---|---|
| **Application**（应用层） | 最贴近「文件系统延迟对应用的影响」；还能看应用上下文，判断延迟是否发生在**应用的主功能期间**还是异步期间 | 手法**随应用与版本而异** |
| **Syscall interface**（系统调用接口） | 接口有文档；**常可由操作系统工具与静态追踪观测** | **会混进所有文件系统类型**（包括统计、socket 这类非存储文件系统），不过滤会很困惑；**同一个功能可能有多个系统调用**，都得上（`read(2)`、`pread64(2)`、`preadv(2)`、`preadv2(2)`……） |
| **VFS** | 所有文件系统的**标准接口**；**一次调用覆盖文件系统操作**（例如 `vfs_write()`） | 同样会**混入所有文件系统类型**，不过滤会困惑 |
| **Top of file system type**（文件系统类型内部） | 只追踪目标文件系统类型；有更多内部上下文 | **文件系统专属**，追踪手法随软件版本而异（虽然像 VFS 的接口映射到 VFS 后变动不大） |

> Syscalls catch all file system types, including non-storage file systems (statistics, sockets), which may be confusing unless filtered. Adding to the confusion, there may also be multiple syscalls for the same file system function. For example, for read, there may be read(2), pread64(2), preadv(2), preadv2(2), etc., all of which need to be measured.

**选层取决于工具可得性**，原文给了三条检查路径：① **应用文档**（有些应用已提供文件系统延迟
指标或能开启采集）；② **操作系统工具**（理想情况下按文件系统或应用分开的统计）；
③ **动态插桩**（Linux kprobes/uprobes，所有层都能用自定义追踪程序观测，**无需重启**）。

**呈现方式**：延迟可以是**每区间平均值**、**分布**（直方图或热力图，见 §8.6.18），
或**每次操作及其延迟的完整列表**。

> **关键警告（原文）**：对**缓存命中率超过 99%** 的文件系统，**每区间平均值会被缓存命中延迟主导**。
> 这很遗憾——**孤立的高延迟实例（outlier）很难从平均值里看出来，但它们恰恰是要找的**。
> 要研究这些 outlier，就得看**完整分布或逐次操作的延迟**，同时也能看到**不同延迟层级**
> （缓存命中与未命中）的效应。

找到高延迟后，**继续向下钻取文件系统**，确定来源。

#### 事务成本（Transaction Cost）

另一种呈现方式：把文件系统延迟表示为**一次应用事务中花在文件系统上的总时间**：

> percent time in file system = 100 * total blocking file system latency/application transaction time

这让文件系统的成本**以应用性能的形式被量化**，并能**预测优化收益**。
指标可以按区间平均，也可以**逐事务**给出。

原文的两个例子：

- 事务耗时 **200 ms**，其中在多次文件系统 I/O 上阻塞了 **180 ms** ⇒ **90%**。
  **消除文件系统延迟最多能让性能提升 10 倍**。
- 事务耗时 **200 ms**，其中只有 **2 ms** 在文件系统里 ⇒ 文件系统与**整条磁盘 I/O 栈**
  **只贡献了 1%**。原文称这个结果**极其有用，因为它能把性能调查导向真正的延迟来源**。

**补充**：若应用用**非阻塞**方式发 I/O，事务期间应用能继续在 CPU 上跑，
此时「阻塞文件系统延迟」**只衡量应用被调度下 CPU 的那部分时间**。

**判据（这一节最实用的一条）**：分析结束时，**必须能用「应用事务中有百分之多少的时间
在文件系统上」回答问题**。答不出来，说明还没测到应用层。

### §8.5.3 工作负载刻画

**刻画所施加的负载**是容量规划、基准测试与负载模拟的重要前提，**也常常能带来最大的性能收益**
——因为它会暴露**可以消除的不必要工作**。

基本属性（原文明列）：

- **操作率与操作类型**；
- **文件 I/O 吞吐**；
- **文件 I/O 尺寸**；
- **读写比**；
- **同步写比例**；
- **随机还是顺序的文件偏移访问**。

**这些特征可能逐秒变化**（尤其是定时触发的应用任务），所以**要同时抓最大值和平均值**，
**更好的是看完整的时间分布**。

**原文给的负载描述范例**（金融交易数据库）：随机读负载，平均 **18,000 读/s**、
平均读尺寸 **4 Kbytes**；**总操作率 21,000 ops/s**，包含读、stat、open、close，
以及约 **200 次同步写/s**；**写率平稳，读率波动**，峰值 **39,000 读/s**。

**进阶刻画清单**（原文以「待考虑的问题」形式列出，也可当作排查清单）：

1. 文件系统**缓存命中率**是多少？未命中率呢？
2. 文件系统**缓存容量与当前用量**？
3. 还有**哪些别的缓存**（目录、inode、buffer），它们的统计如何？
4. **过去有没有调优过文件系统**？有没有参数被设成非默认值？
5. **哪些应用或用户在用这个文件系统**？
6. **哪些文件和目录在被访问、被创建、被删除**？
7. 有没有遇到**错误**？是无效请求还是文件系统本身的问题？
8. **文件系统 I/O 为什么会发出**（用户态调用路径是什么）？
9. 应用**在多大程度上直接（同步地）请求文件系统 I/O**？
10. **I/O 到达时间的分布**是什么？

**性能刻画**（看结果，不是看负载）：

1. 文件系统操作的**平均延迟**？
2. 有没有**高延迟 outlier**？
3. **完整的延迟分布**？
4. 有没有针对文件系统或磁盘 I/O 的**系统资源控制（resource control）**，是否生效？

前三个问题**可以按操作类型分别问**。

**事件追踪**：追踪工具可以把**所有**文件系统操作及其细节记到日志里（操作类型、参数、
路径名、起止时间戳、完成状态、进程 ID 与名字）。原文说它**可能是工作负载刻画的终极工具**，
但实践中**开销可能很大**（因为文件系统操作频率高），**除非重度过滤**（例如只记慢 I/O，
见 §8.6.14 的 `ext4slower(8)`）。

### §8.5.4 性能监控

关键指标只有两个：**操作率**与**操作延迟**。

- **操作率**是所施加负载最基本的特征；**延迟**是产生的性能。
- 正常或坏延迟的**取值取决于你的负载、环境与延迟要求**。若不确定，
  **可以做已知为好 / 已知为坏的负载的微基准**来标定（例如通常命中缓存的负载 vs 通常不命中的）。
- 延迟可按**每秒平均值**监控，也可带上**最大值与标准差**。
  **理想情况应当能看完整分布**（直方图或热力图）以发现 outlier 与其他模式。
- **操作率与延迟都可以按操作类型分别记录**（读、写、stat、open、close……），
  这对调查工作负载与性能变化**帮助极大**——能定位到**具体哪类操作**变了。
- 用了**基于文件系统的资源控制**的系统，可以加统计显示**何时处于被限速状态**。

**原文的一条现实**：**在 Linux 上通常没有现成的文件系统操作统计**（例外是 NFS，可用 `nfsstat(8)`）。
⇒ Linux 上必须自己用追踪工具补这块（§8.6）。

### §8.5.5 静态性能调优

静态调优关注**已配置的环境**。原文给的文件系统静态配置检查表：

1. 挂载并**正在使用**的文件系统有多少个？
2. **记录大小（record size）**是多少？
3. **访问时间戳**开着吗？
4. 还开了**哪些其他选项**（压缩、加密……）？
5. **文件系统缓存**怎么配的？最大多大？
6. **其他缓存**（目录、inode、buffer）怎么配的？
7. 有没有**二级缓存**在用？
8. 有多少**存储设备**、在用哪些？
9. **存储设备的配置**是什么？RAID？
10. 用了**哪些文件系统类型**？
11. **文件系统（或内核）的版本**是什么？
12. 有没有**应当考虑的文件系统 bug / 补丁**？
13. 有没有在用**针对文件系统 I/O 的资源控制**？

**判据**：这张表的价值在于**揭示被忽略的配置选择**——系统常常是**为一种负载配的，
后来被挪作他用**。

### §8.5.6 缓存调优

内核与文件系统会用多种缓存：buffer cache、目录缓存、inode 缓存、文件系统（页）缓存。
这些都可以**检查**，也常常**可以调优**（视可调项而定）。

### §8.5.7 工作负载隔离

某些负载在**独占的文件系统与磁盘设备**上表现更好。这种做法俗称
**「separate spindles（分轴）」**——因为在两个负载位置之间来回寻道造成的随机 I/O
**对旋转盘尤其糟糕**。

典型例子：数据库的**日志文件**与**数据文件**分开放不同的文件系统与磁盘。
数据库的安装指南通常就会建议数据存储的摆放位置。

### §8.5.8 微基准

基准工具可用来测试**不同文件系统类型或同一文件系统内不同设置**在给定负载下的性能。
可测的典型因素（原文明列）：

- **操作类型**：读、写及其他文件系统操作的速率；
- **I/O 尺寸**：1 字节到 1 Mbyte 乃至更大；
- **文件偏移模式**：随机或顺序；
- **随机访问分布**：均匀、随机、还是 **Pareto**；
- **写类型**：异步或同步（`O_SYNC`）；
- **工作集大小**：能否装进文件系统缓存；
- **并发度**：并行 I/O 数量或执行 I/O 的线程数；
- **内存映射**：用 `mmap(2)` 而非 `read(2)` / `write(2)` 访问；
- **缓存状态**：缓存是「冷」（未填充）还是「热」；
- **文件系统可调项**：压缩、数据去重等。

常见组合是四种基本形态：**随机读、顺序读、随机写、顺序写**。
原文**故意不把 direct I/O 列进来**——它在本章的意图是**绕过文件系统、测磁盘设备性能**
（见 [[Chapter 9. Disks]]）。

**关键因子是工作集大小（WSS）**：

> A critical factor when micro-benchmarking file systems is the working set size (WSS): the volume of data that is accessed. Depending on the benchmark, this may be the total size of the files in use. A small working set size may return entirely from the file system cache in main memory (DRAM), unless a direct I/O flag is used. A large working set size may return mostly from storage devices (disks). The performance difference can be multiple orders of magnitude. Running a benchmark against a newly mounted file system and then a second time after caches have been populated and comparing the results of the two is often a good illustration of WSS. (Also see Section 8.7.3 , Cache Flushing .)

**WSS 太小 ⇒ 全部从内存缓存返回**（除非用了 direct I/O 标志）；
**WSS 太大 ⇒ 大部分从存储设备返回**。**两者性能差好几个数量级**。

**原文 Table 8.5 的期望值**（内存 128 Gbytes）：

| 内存 | 文件总大小（WSS） | 基准 | 期望 |
|---|---|---|---|
| 128 Gbytes | 10 Gbytes | 随机读 | **100% 缓存命中** |
| 128 Gbytes | 10 Gbytes | 随机读，direct I/O | **100% 磁盘读**（因为 direct I/O） |
| 128 Gbytes | 1,000 Gbytes | 随机读 | **大部分磁盘读**，约 **12% 缓存命中** |
| 128 Gbytes | 10 Gbytes | 顺序读 | **100% 缓存命中** |
| 128 Gbytes | 1,000 Gbytes | 顺序读 | 缓存命中（**多数来自预取**）与磁盘读的混合 |
| 128 Gbytes | 10 Gbytes | 缓冲写 | **大部分缓存命中（缓冲）**，另有部分阻塞取决于文件系统行为 |
| 128 Gbytes | 10 Gbytes | 同步写 | **100% 磁盘写** |

**原文的一条批评**：有些磁盘基准工具**不说明自己到底测了什么**，可能**声称测磁盘
却用了很小的文件总大小**——结果全部从缓存返回，**根本没测到磁盘**。

有些磁盘基准工具**通过文件系统、用 direct I/O 来绕开缓存与缓冲**。
此时**文件系统仍在扮演小角色**：额外的代码路径开销，以及**文件布局与盘上布局的映射差异**。

基准的通用话题见 [[Chapter 12. Benchmarking]]。

## §8.6 可观测工具

这一节是全章篇幅最大的一节（十七节）。原文先给了一张工具总表：

| 节 | 工具 | 说明 |
|---|---|---|
| §8.6.1 | `mount` | 列出文件系统与挂载标志 |
| §8.6.2 | `free` | 缓存容量统计 |
| §8.6.3 | `top` | 含内存使用摘要 |
| §8.6.4 | `vmstat` | 虚拟内存统计 |
| §8.6.5 | `sar` | 多种统计，含历史记录 |
| §8.6.6 | `slabtop` | 内核 slab 分配器统计 |
| §8.6.7 | `strace` | 系统调用追踪 |
| §8.6.8 | `fatrace` | 用 fanotify 追踪文件系统操作 |
| §8.6.9 | `LatencyTOP` | 显示系统范围的延迟来源 |
| §8.6.10 | `opensnoop` | 追踪被打开的文件 |
| §8.6.11 | `filetop` | 按 IOPS 与字节数排出的热点文件 |
| §8.6.12 | `cachestat` | 页缓存统计 |
| §8.6.13 | `ext4dist`（xfs, zfs, btrfs, nfs） | 显示 ext4 操作延迟分布 |
| §8.6.14 | `ext4slower`（xfs, zfs, btrfs, nfs） | 显示慢的 ext4 操作 |
| §8.6.15 | `bpftrace` | 自定义文件系统追踪 |
| §8.6.17 | 其他工具 | 见本节末的表 |
| §8.6.18 | 可视化 | 热力图等 |

⚠️ **原文编号从 §8.6.15 直接跳到 §8.6.17**（原书没有 §8.6.16）。

**工具的来源分层**（原文）：传统工具 `mount(8)`、`free(1)`、`top(1)`、`vmstat(8)`、`sar(1)`
**大多在其他 Unix-like 系统上也可用**（它们就起源于那些系统）；
**多数追踪工具基于 BPF**，使用 **BCC 与 bpftrace 前端**（见 [[Chapter 14. Ftrace]]）——
具体是 `opensnoop(8)`、`filetop(8)`、`cachestat(8)`、`ext4dist(8)`、`ext4slower(8)`。

### §8.6.1 mount

`mount` 列出已挂载的文件系统与挂载标志：

```bash
$ mount
/dev/nvme0n1p1 on / type ext4 (rw,relatime,discard)
devtmpfs on /dev type devtmpfs (rw,relatime,size=986036k,nr_inodes=246509,mode=755)
sysfs on /sys type sysfs (rw,nosuid,nodev,noexec,relatime)
proc on /proc type proc (rw,nosuid,nodev,noexec,relatime)
securityfs on /sys/kernel/security type securityfs (rw,nosuid,nodev,noexec,relatime)
tmpfs on /dev/shm type tmpfs (rw,nosuid,nodev)
[...]
```

第一行说明 `/dev/nvme0n1p1` 上的 ext4 挂在 `/`，标志是 `rw`、`relatime`、`discard`。
**`relatime` 是性能相关的选项**：它**减少 inode 访问时间的更新**（及随之而来的磁盘 I/O 开销）
——只有当 modify 或 change 时间也在更新时，**或者上次更新已超过一天**，才更新访问时间。

### §8.6.2 free

`free` 显示内存与 swap 统计：

```bash
$ free -m
            total      used      free    shared  buff/cache   available
Mem:         1950       568       163         0        1218        1187
Swap:           0         0         0
$ free -mw
            total      used      free    shared     buffers       cache   available
Mem:         1950       568       163         0          84        1133        1187
Swap:           0         0         0
```

**宽格式**多出 `buffers` 列（**buffer cache** 大小）与 `cache` 列（**页缓存** 大小）；
默认输出把两者合并成 `buff/cache`。

**`available` 列很重要**（`free(1)` 的新功能）：显示**不需要 swap 就能拿到的内存量**，
它考虑了**不能立即回收**的那部分内存。这些字段也能从 `/proc/meminfo` 读（单位是 KB）。

### §8.6.3 top

某些版本的 `top` 会带上文件系统缓存明细：

```bash
MiB Mem :   1950.0 total,    161.2 free,    570.3 used,   1218.6 buff/cache
MiB Swap:      0.0 total,      0.0 free,      0.0 used.   1185.9 avail Mem
```

`top` 的其余用法见 [[Chapter 6. CPUs]]。

### §8.6.4 vmstat

`vmstat` 也可能包含文件系统缓存明细：

```bash
$ vmstat 1
procs -----------memory---------- ---swap-- -----io---- -system-- ------cpu-----
 r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st
 0  0      0 167644  87032 1161112    0    0     7    14   14    1  4  2 90  0  5
 0  0      0 167636  87032 1161152    0    0     0     0  162  376  0  0 100  0  0
[...]
```

`buff` 列是 buffer cache、`cache` 列是页缓存，单位都是 KB。
`vmstat` 的其余用法见 [[Chapter 7. Memory]]。

### §8.6.5 sar

**系统活动报告器 `sar`** 提供多种文件系统统计，**可配置为周期性记录**。

```bash
# sar -v 1
Linux 5.3.0-1009-aws (ip-10-1-239-218)     02/08/20       _x86_64_  (2 CPU)

21:20:24    dentunusd   file-nr  inode-nr    pty-nr
21:20:25        27027      1344     52945         2
21:20:26        27012      1312     52922         2
21:20:27        26997      1248     52899         2
[...]
```

`-v` 的四列：**`dentunusd`** 目录项缓存**未使用**条目数（可用条目数）、
**`file-nr`** 在用文件句柄数、**`inode-nr`** 在用 inode 数、**`pty-nr`** 在用伪终端数。
另有 `-r` 选项打印 `kbbuffers` 与 `kbcached`（buffer cache 与页缓存大小，KB）。

### §8.6.6 slabtop

`slabtop` 打印内核 slab 缓存信息，其中一些就是文件系统缓存：

```bash
# slabtop -o
 Active / Total Objects (% used)    : 604675 / 684235 (88.4%)
 Active / Total Slabs (% used)      : 24040 / 24040 (100.0%)
 Active / Total Caches (% used)     : 99 / 159 (62.3%)
 Active / Total Size (% used)       : 140593.95K / 160692.10K (87.5%)
 Minimum / Average / Maximum Object : 0.01K / 0.23K / 12.00K

  OBJS ACTIVE  USE OBJ SIZE  SLABS OBJ/SLAB CACHE SIZE NAME
165945 149714  90%    0.10K   4255       39     17020K buffer_head
107898  66011  61%    0.19K   5138       21     20552K dentry
 67350  67350 100%    0.13K   2245       30      8980K kernfs_node_cache
 41472  40551  97%    0.03K    324      128      1296K kmalloc-32
 35940  31460  87%    1.05K   2396       15     38336K ext4_inode_cache
 33514  33126  98%    0.58K   2578       13     20624K inode_cache
 24576  24576 100%    0.01K     48      512       192K kmalloc-8
[...]
```

**与文件系统相关的 slab 缓存**：`dentry`、`ext4_inode_cache`、`inode_cache`。
不加 `-o`（一次性输出）时 `slabtop` 会刷新重绘屏幕。

原文列出的 slab 缓存含义：`buffer_head` → buffer cache；`dentry` → dentry cache；
`inode_cache` → inode cache；`ext3_inode_cache` / `ext4_inode_cache` → ext3/ext4 的 inode cache；
`xfs_inode` / `btrfs_inode` → 各自文件系统的 inode cache。
数据源是 `/proc/slabinfo`（需 `CONFIG_SLAB`）。

### §8.6.7 strace

在**系统调用接口**测延迟可以用追踪工具，Linux 上就是 `strace`：

```bash
$ strace -ttT -p 845
[...]
18:41:01.513110 read(9, "\334\260/\224\356k..."..., 65536) = 65536 <0.018225>
18:41:01.531646 read(9, "\371X\265|\244\317..."..., 65536) = 65536 <0.000056>
18:41:01.531984 read(9, "\357\311\347\1\241..."..., 65536) = 65536 <0.005760>
18:41:01.538151 read(9, "*\263\264\204|\370..."..., 65536) = 65536 <0.000033>
18:41:01.538549 read(9, "\205q\327\304f\370..."..., 65536) = 65536 <0.002033>
18:41:01.540923 read(9, "\6\2738>zw\321\353..."..., 65536) = 65536 <0.000032>
```

`-tt` 打印左侧相对时间戳，`-T` 打印右侧系统调用耗时。这里每次读 64 Kbytes：
**第一次 18 ms**，然后 **56 μs**（很可能是缓存命中），再 **5 ms**。读的 fd 是 9。
要确认这是文件系统（不是 socket），可以看更早输出里的 `open(2)`，或用 `lsof(8)`；
也可以查 `/proc/845/fd{,info}/9`。

**⚠️ 原文的强烈警告**：**当前基于 `ptrace(2)` 的 `strace` 实现会严重损害性能**，
**只应在开销可接受、且没有别的办法分析延迟时才用**。**测出的延迟可能被观察者效应扭曲**。
原文指向更现代的工具（`ext4slower(8)` 等），它们用**每 CPU 缓冲追踪 + BPF** 大幅降低开销，
**延迟测量更准确**。

### §8.6.8 fatrace

`fatrace` 是基于 Linux **fanotify API**（file access notify）的专用追踪器：

```bash
# fatrace
sar(25294): O /etc/ld.so.cache
sar(25294): RO /lib/x86_64-linux-gnu/libc-2.27.so
sar(25294): C /etc/ld.so.cache
sar(25294): O /usr/lib/locale/locale-archive
sar(25294): O /usr/share/zoneinfo/America/Los_Angeles
sar(25294): RC /usr/share/zoneinfo/America/Los_Angeles
sar(25294): RO /var/log/sysstat/sa09
sar(25294): R /var/log/sysstat/sa09
[...]
```

每行是**进程名、PID、事件类型、完整路径、可选状态**。事件类型：**O**（打开）、
**R**（读）、**W**（写）、**C**（关闭）。它适合**工作负载刻画**——弄清访问了哪些文件、
**找出可以消除的不必要工作**。

**代价**：繁忙的文件系统负载上，`fatrace` **每秒能产生数万行输出**、
消耗可观 CPU。按事件类型过滤能缓解；BPF 工具（如 `opensnoop(8)`）开销低得多。

### §8.6.9 LatencyTOP

LatencyTOP 报告**聚合的系统级与进程级延迟来源**，其中包括文件系统延迟：

```bash
Cause                                                Maximum     Percentage
Reading from file                                 209.6 msec         61.9 %
synchronous write                                  82.6 msec         24.0 %
Marking inode dirty                                 7.9 msec          2.2 %
Waiting for a process to die                        4.6 msec          1.5 %
Waiting for event (select)                          3.6 msec         10.1 %
Page fault                                          0.2 msec          0.2 %

Process gzip (10969)                       Total: 442.4 msec
Reading from file                                 209.6 msec         70.2 %
synchronous write                                  82.6 msec         27.2 %
Marking inode dirty                                 7.9 msec          2.5 %
```

上半是系统级汇总，下半是单个 `gzip(1)` 进程——它压缩文件，
**大部分延迟在 Reading from file（70.2%）**，**27.2% 在同步写**（写出压缩结果）。

**⚠️ 原文的现状说明**：LatencyTOP 由 Intel 开发，但**很久没更新了，官网也已下线**，
而且**需要并不常开启的内核选项**（`CONFIG_LATENCYTOP`、`CONFIG_HAVE_LATENCYTOP_SUPPORT`）。
原文建议：**不如直接用 BPF 追踪工具测文件系统延迟**（§8.6.13–§8.6.15）。


### §8.6.10 opensnoop

`opensnoop`（BCC 与 bpftrace 都有）追踪文件打开，用于**发现数据文件、日志文件与配置文件的位置**，
也能发现**频繁打开导致的性能问题**、或排查**文件缺失**问题：

```bash
# opensnoop -T
TIME(s)       PID    COMM               FD ERR PATH
0.000000000   26447  sshd                5   0 /var/log/btmp
[...]
1.961686000   25983  mysqld              4   0 /etc/mysql/my.cnf
1.961715000   25983  mysqld              5   0 /etc/mysql/conf.d/
1.961770000   25983  mysqld              5   0 /etc/mysql/conf.d/mysql.cnf
1.961799000   25983  mysqld              5   0 /etc/mysql/conf.d/mysqldump.cnf
1.961818000   25983  mysqld              5   0 /etc/mysql/mysql.conf.d/
1.961843000   25983  mysqld              5   0 /etc/mysql/mysql.conf.d/mysql.cnf
1.961862000   25983  mysqld              5   0 /etc/mysql/mysql.conf.d/mysqld.cnf
[...]
2.438417000   25983  mysqld              4   0 /var/log/mysql/error.log
[...]
2.816953000   25983  mysqld             30   0 ./binlog.000024
2.818827000   25983  mysqld             31   0 ./binlog.index_crash_safe
2.820621000   25983  mysqld              4   0 ./binlog.index
[...]
```

这段输出是一次 **MySQL 启动**，`opensnoop` 暴露了**配置文件、日志文件、数据文件（binlog）**。

**机制**：只追踪 `open(2)` 家族的变体——`open(2)` 与 `openat(2)`。
**因为打开通常不频繁，开销预期可忽略**。BCC 版选项：`-T` 加时间戳列、
`-x` 只显示失败的打开、`-p PID` 只追某进程、`-n NAME` 只追进程名含 NAME 的。

### §8.6.11 filetop

`filetop` 是 BCC 工具，像 `top(1)` 之于进程，只不过对象是**文件**：

```bash
# filetop
Tracing... Output every 1 secs. Hit Ctrl-C to end

19:16:22 loadavg: 0.11 0.04 0.01 3/189 23035

TID    COMM             READS  WRITES R_Kb    W_Kb    T FILE
23033  mysqld           481    0      7681    0       R sb1.ibd
23033  mysqld           3      0      48      0       R mysql.ibd
23032  oltp_read_only.  3      0      20      0       R oltp_common.lua
23031  oltp_read_only.  3      0      20      0       R oltp_common.lua
23032  oltp_read_only.  1      0      19      0       R Index.xml
23032  oltp_read_only.  4      0      16      0       R openssl.cnf
23035  systemd-udevd    4      0      16      0       R sys_vendor
[...]
```

默认显示**前二十个文件**，按读字节数排序。示例里 `mysqld` 对 `sb1.ibd` 读了 481 次、
共 7,681 Kbytes。**用法与 `top(1)` 发现意外吃 CPU 的进程完全平行**：
用它**发现意外 I/O 繁忙的文件**。

默认**只显示常规文件**；`-a` 显示全部类型（含 TCP socket 与设备节点）：

```bash
# filetop -a
[...]
TID    COMM             READS  WRITES R_Kb    W_Kb    T FILE
21701  sshd             1      0      16      0       O ptmx
23033  mysqld           1      0      16      0       R sbtest1.ibd
23335  sshd             1      0      8       0       S TCP
1      systemd          4      0      4       0       R comm
[...]
```

输出里出现了**其他类型（O）**与 **socket（S）**；`ptmx` 是 `/dev` 下的**字符特殊文件**。

选项：`-C` 不清屏（滚动输出）、`-a` 显示全部文件类型、`-r ROWS` 行数（默认 20）、
`-p PID` 只追某进程。默认每秒刷新（像 `top`），除非用 `-C`。
**作者偏好用 `-C`**，让输出留在终端回滚缓冲里，便于事后回看。

### §8.6.12 cachestat

`cachestat`（BCC）显示**页缓存命中与未命中统计**，用来检查命中率与效率：

```bash
$ cachestat -T 1
TIME         HITS   MISSES  DIRTIES HITRATIO   BUFFERS_MB  CACHED_MB
21:00:48      586        0     1870  100.00%          208        775
21:00:49      125        0     1775  100.00%          208        776
21:00:50      113        0     1644  100.00%          208        776
21:00:51       23        0     1389  100.00%          208        776
21:00:52      134        0     1906  100.00%          208        777
[...]
```

这展示的是**完全缓存命中的读负载**（HITS、命中率 100%）与**更高的写负载**（DIRTIES）。
**理想状态是命中率接近 100%**，这样应用读就不会阻塞在磁盘 I/O 上。

**调优线索**：命中率低且影响性能时，**可以调小应用的内存**，给页缓存腾出空间；
若配置了 swap 设备，还有 **swappiness** 可调项用来在「淘汰页缓存」与「换出」之间取舍。

**⚠️ 原文的定性**：它虽提供关键洞察，但**是实验性工具**——**用 kprobes 追踪特定内核函数**，
**换内核版本就需要维护**。原文说**如果将来有 tracepoint 或 `/proc` 统计可用，
这个工具就能改写成稳定版本**；**它今天的最佳用途也许只是证明「这样的工具是可能的」**。

### §8.6.13 ext4dist（xfs, zfs, btrfs, nfs）

`ext4dist` 插桩 ext4，把常用操作（读、写、打开、fsync）的**延迟分布**画成直方图：

```bash
# ext4dist 10 1
Tracing ext4 operation latency... Hit Ctrl-C to end.

21:09:46:

operation = read
     usecs               : count     distribution
         0 -> 1          : 783      |***********************                 |
         2 -> 3          : 88       |**                                      |
         4 -> 7          : 449      |*************                           |
         8 -> 15         : 1306     |****************************************|
        16 -> 31         : 48       |*                                       |
        32 -> 63         : 12       |                                        |
        64 -> 127        : 39       |*                                       |
       128 -> 255        : 11       |                                        |
       256 -> 511        : 158      |****                                    |
       512 -> 1023       : 110      |***                                     |
      1024 -> 2047       : 33       |*                                       |

operation = write
     usecs               : count     distribution
         0 -> 1          : 1073     |****************************            |
         2 -> 3          : 324      |********                                |
         4 -> 7          : 1378     |************************************    |
         8 -> 15         : 1505     |****************************************|
        16 -> 31         : 183      |****                                    |
        32 -> 63         : 37       |                                        |
        64 -> 127        : 11       |                                        |
       128 -> 255        : 9        |                                        |

operation = open
     usecs               : count     distribution
         0 -> 1          : 672      |****************************************|
         2 -> 3          : 10       |                                        |

operation = fsync
     usecs               : count     distribution
       256 -> 511        : 485      |**********                              |
       512 -> 1023       : 308      |******                                  |
      1024 -> 2047       : 1779     |****************************************|
      2048 -> 4095       : 79       |*                                       |
      4096 -> 8191       : 26       |                                        |
      8192 -> 16383      : 4        |                                        |
```

这里用了 10 秒间隔与 1 次计数，展示**单次 10 秒追踪**。原文的解读：
**读延迟呈双峰分布**——一个峰在 **0–15 μs**（很可能是**内存缓存命中**），
另一个峰在 **256–2048 μs**（很可能是**磁盘读**）。
**写很快**，很可能因为**写被缓冲了**，稍后用**更慢的 fsync** 刷盘。

**工具定位（原文）**：它与 `ext4slower(8)` 展示的是**应用能感受到的延迟**。
在磁盘层测延迟也是可以的（[[Chapter 9. Disks]]），但**应用可能并没有直接阻塞在磁盘 I/O 上**，
那样的测量**更难解释**。因此原文**优先用 `ext4dist(8)` / `ext4slower(8)`**，
再看磁盘 I/O 延迟工具。

选项：`-m` 以毫秒输出、`-p PID` 只追某进程。输出可视化成**延迟热力图**。

### §8.6.14 ext4slower（xfs, zfs, btrfs, nfs）

`ext4slower` 追踪常用 ext4 操作，**只打印慢于给定阈值**的逐事件细节：

```bash
# ext4slower
Tracing ext4 operations slower than 10 ms
TIME     COMM           PID    T BYTES   OFF_KB   LAT(ms) FILENAME
21:36:03 mysqld         22935  S 0       0          12.81 sbtest1.ibd
21:36:15 mysqld         22935  S 0       0          12.13 ib_logfile1
21:36:15 mysqld         22935  S 0       0          10.46 binlog.000026
21:36:15 mysqld         22935  S 0       0          13.66 ib_logfile1
21:36:15 mysqld         22935  S 0       0          11.79 ib_logfile1
[...]
```

列含义：时间（TIME）、进程名（COMM）、pid（PID）、操作类型（**T：R 读 / W 写 / O 打开 / S 同步**）、
KB 为单位的偏移（OFF_KB）、毫秒延迟（LAT(ms)）、文件名（FILENAME）。

输出显示**一批超过 10 毫秒（默认阈值）的同步操作**。阈值可作为参数给出；
**选 0 毫秒则显示所有操作**：

```bash
# ext4slower 0
Tracing ext4 operations
21:36:50 mysqld         22935  W 917504  2048        0.42 ibdata1
21:36:50 mysqld         22935  W 1024    14165       0.00 ib_logfile1
21:36:50 mysqld         22935  W 512     14166       0.00 ib_logfile1
21:36:50 mysqld         22935  S 0       0           3.21 ib_logfile1
21:36:50 mysqld         22935  W 1746    21714       0.02 binlog.000026
21:36:50 mysqld         22935  S 0       0           5.56 ibdata1
21:36:50 mysqld         22935  W 16384   4640        0.01 undo_001
21:36:50 mysqld         22935  W 16384   11504       0.01 sbtest1.ibd
21:36:50 mysqld         22935  W 16384   13248       0.01 sbtest1.ibd
21:36:50 mysqld         22935  W 16384   11808       0.01 sbtest1.ibd
21:36:50 mysqld         22935  W 16384   1328        0.01 undo_001
21:36:50 mysqld         22935  W 16384   6768        0.01 undo_002
[...]
```

**模式很清晰**：`mysqld` **先写文件，之后再做一次同步操作**。

**作者的操作纪律**：追踪**所有**操作会产生**大量输出与相应开销**，
**只做短时间**（例如十秒）以理解「其他汇总视图里看不到的文件系统操作模式」
（对照 `ext4dist(8)`）。选项：`-p PID` 只追某进程、`-j` 输出可解析的 CSV。

### §8.6.15 bpftrace

bpftrace 是基于 BPF 的追踪器，提供**高级编程语言**，能写**强大的一行式脚本与短脚本**。
它特别适合**基于其他工具给出的线索做定制文件系统分析**。

#### 一行式脚本（One-Liners）

原文给出的这些一行式脚本**照抄不改**，展示了 bpftrace 的不同能力。

追踪 openat(2)，带进程名

```bash
bpftrace -e 't:syscalls:sys_enter_openat { printf("%s %s\n", comm,
    str(args->filename)); }'
```

按系统调用类型统计 read 调用

```bash
bpftrace -e 'tracepoint:syscalls:sys_enter_*read* { @[probe] = count(); }'
```

按系统调用类型统计 write 调用

```bash
bpftrace -e 'tracepoint:syscalls:sys_enter_*write* { @[probe] = count(); }'
```

read() 请求大小的分布

```bash
bpftrace -e 'tracepoint:syscalls:sys_enter_read { @ = hist(args->count); }'
```

read() 读到的字节数（及错误）分布

```bash
bpftrace -e 'tracepoint:syscalls:sys_exit_read { @ = hist(args->ret); }'
```

按错误码统计 read() 错误

```bash
bpftrace -e 't:syscalls:sys_exit_read /args->ret < 0/ { @[- args->ret] = count(); }'
```

统计 VFS 调用

```bash
bpftrace -e 'kprobe:vfs_* { @[probe] = count(); }'
```

只统计 PID 181 的 VFS 调用

```bash
bpftrace -e 'kprobe:vfs_* /pid == 181/ { @[probe] = count(); }'
```

统计 ext4 tracepoint

```bash
bpftrace -e 'tracepoint:ext4:* { @[probe] = count(); }'
```

统计 xfs tracepoint

```bash
bpftrace -e 'tracepoint:xfs:* { @[probe] = count(); }'
```

按进程名与用户态栈统计 ext4 文件读

```bash
bpftrace -e 'kprobe:ext4_file_read_iter { @[ustack, comm] = count(); }'
```

追踪 ZFS spa_sync() 时刻

```bash
bpftrace -e 'kprobe:spa_sync { time("%H:%M:%S ZFS spa_sync()\n"); }'
```

按进程名与 PID 统计 dcache 引用

```bash
bpftrace -e 'kprobe:lookup_fast { @[comm, pid] = count(); }'
```

#### 系统调用追踪：read(2) 为什么难用

系统调用是很好的追踪目标，也是许多追踪工具的插桩来源。但有些系统调用缺少文件系统上下文，用起来容易混淆。原文用 `openat(2)`（好用）与 `read(2)`（不好用）做对照，并给出补救办法。

**openat(2)（能用的例子）**：

```bash
# bpftrace -e 't:syscalls:sys_enter_openat { printf("%s %s\n", comm,
    str(args->filename)); }'
Attaching 1 probe...
sa1 /etc/sysstat/sysstat
sadc /etc/ld.so.cache
sadc /lib/x86_64-linux-gnu/libsensors.so.5
sadc /lib/x86_64-linux-gnu/libc.so.6
sadc /lib/x86_64-linux-gnu/libm.so.6
sadc /sys/class/i2c-adapter
sadc /sys/bus/i2c/devices
sadc /sys/class/hwmon
sadc /etc/sensors3.conf
[...]
```

输出抓到了 `sar(1)` 归档统计的执行过程及它打开的文件。tracepoint 的参数可用 `-lv` 列出：

```bash
# bpftrace -lv t:syscalls:sys_enter_openat
tracepoint:syscalls:sys_enter_openat
    int __syscall_nr;
    int dfd;
    const char * filename;
    int flags;
    umode_t mode;
```

参数是系统调用号、文件描述符、文件名、打开标志、打开模式——**足够一行式脚本与
`opensnoop(8)` 这类工具使用**。

**read(2)（不好用的例子）**：

```bash
# bpftrace -lv t:syscalls:sys_enter_read
tracepoint:syscalls:sys_enter_read
    int __syscall_nr;
    unsigned int fd;
    char * buf;
    size_t count;
```

**参数无法区分**读的是文件系统、socket、`/proc` 还是别的。按进程名统计：

```bash
# bpftrace -e 't:syscalls:sys_enter_read { @[comm] = count(); }'
Attaching 1 probe...
^C

@[systemd-journal]: 13
@[sshd]: 141
@[java]: 3472
```

追踪期间 Java 做了 **3,472 次 `read(2)`**——**但它们来自文件系统、socket 还是别的东西？**
（`sshd` 的读很可能是 socket I/O。）

`read(2)` 能提供 **FD（整数）**，但**它只是个数字，不显示 FD 类型**
（而且 bpftrace 跑在受限的内核模式，**不能查 `/proc` 里的 FD 信息**）。
原文给了**至少四种解法**：

1. **从 bpftrace 打印 PID 与 FD**，事后用 `lsof(8)` 或 `/proc` 查这些 FD 是什么；
2. **即将到来的 BPF helper `get_fd_path()`** 可以返回 FD 的路径名，从而**区分有路径名的文件系统读**；
3. **改为从 VFS 追踪**，那里有更多数据结构可用；
4. **直接追踪文件系统函数**，排除其他 I/O 类型——**`ext4dist(8)` 与 `ext4slower(8)` 就是这么做的**。

#### VFS 追踪

**「VFS 抽象了所有文件系统（及其他设备），追踪它的调用提供了一个单一的观测点，
可以观察所有文件系统。」**

**VFS 调用计数**：

```bash
# bpftrace -e 'kprobe:vfs_* { @[func] = count(); }'
Attaching 65 probes...
^C
[...]
@[vfs_statfs]: 36
@[vfs_readlink]: 164
@[vfs_write]: 364
@[vfs_lock_file]: 516
@[vfs_iter_read]: 2551
@[vfs_statx]: 3141
@[vfs_statx_fd]: 4214
@[vfs_open]: 5271
@[vfs_read]: 5602
@[vfs_getattr_nosec]: 7794
@[vfs_getattr]: 7795
```

这展示了**全系统范围内出现的操作类型**（追踪期间有 7,795 次 `vfs_read()`）。

**VFS 延迟（按类型分桶）**——VFS 的读同样可能来自文件系统、socket 等，
于是要**从内核结构里取类型**（inode 的 superblock 名字）：

```bash
# vfsreadlat.bt
Tracing vfs_read() by type... Hit Ctrl-C to end.
^C
[...]
@us[sockfs]:
[0]                  141 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[1]                   91 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@                   |
[2, 4)                57 |@@@@@@@@@@@@@@@@@@@@@                               |
[4, 8)                53 |@@@@@@@@@@@@@@@@@@@                                 |
[8, 16)               86 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@                     |
[16, 32)               2 |                                                    |
[...]

@us[proc]:
[0]                  242 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[1]                   41 |@@@@@@@@                                            |
[2, 4)                40 |@@@@@@@@                                            |
[4, 8)                61 |@@@@@@@@@@@@@                                       |
[8, 16)               44 |@@@@@@@@@                                           |
[16, 32)              40 |@@@@@@@@                                            |
[32, 64)               6 |@                                                   |
[64, 128)              3 |                                                    |
@us[ext4]:
[0]                  653 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@         |
[1]                  447 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@                      |
[2, 4)                70 |@@@@                                                |
[4, 8)               774 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[8, 16)              417 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@                        |
[16, 32)              25 |@                                                   |
[32, 64)               7 |                                                    |
[64, 128)            170 |@@@@@@@@@@@                                         |
[128, 256)            55 |@@@                                                 |
[256, 512)            59 |@@@                                                 |
[512, 1K)            118 |@@@@@@@                                             |
[1K, 2K)               3 |@@                                                  |
```

**读法**：**ext4 的分布是双峰的**——0–8 μs 一峰（缓存命中），**64 μs 以上又是一峰**
（磁盘读）；**sockfs 则是典型的低延迟单峰**。原文指出输出还包含
sysfs、devpts、pipefs、devtmpfs、tmpfs、anon_inodefs 的直方图。

源码（照抄）：

```c
#!/usr/local/bin/bpftrace
#include <linux/fs.h>

BEGIN
{
        printf("Tracing vfs_read() by type... Hit Ctrl-C to end.\n");
}

kprobe:vfs_read
{
        @file[tid] = ((struct file *)arg0)->f_inode->i_sb->s_type->name;
        @ts[tid] = nsecs;
}

kretprobe:vfs_read
/@ts[tid]/
{
        @us[str(@file[tid])] = hist((nsecs - @ts[tid]) / 1000);
        delete(@file[tid]); delete(@ts[tid]);
}

END
{
        clear(@file); clear(@ts);
}
```

可以扩展到 `vfs_readv()`、`vfs_write()`、`vfs_writev()` 等。

**⚠️ 原文的限定**：**这个延迟未必直接影响应用性能**——取决于它发生在
**应用请求期间**，还是**异步后台任务**中。要回答这个问题，
**可以把用户态栈（`ustack`）作为额外的直方图键**，可能揭示 `vfs_read()` 是否发生在应用请求期间。

#### 文件系统内部

需要时可以开发自定义工具展示文件系统内部行为。**先试 tracepoint**：

```bash
# bpftrace -l 'tracepoint:ext4:*'
tracepoint:ext4:ext4_other_inode_update_time
tracepoint:ext4:ext4_free_inode
tracepoint:ext4:ext4_request_inode
tracepoint:ext4:ext4_allocate_inode
tracepoint:ext4:ext4_evict_inode
tracepoint:ext4:ext4_drop_inode
[...]
```

每个 tracepoint 的参数可用 `-lv` 列出。**tracepoint 不足（或该文件系统没有）时，
改用 kprobe 动态插桩**：

```bash
# bpftrace -lv 'kprobe:ext4_*'
kprobe:ext4_has_free_clusters
kprobe:ext4_validate_block_bitmap
kprobe:ext4_get_group_number
kprobe:ext4_get_group_no_and_offset
kprobe:ext4_get_group_desc
kprobe:ext4_wait_block_bitmap
[...]
```

**规模感（这个内核版本 5.3）**：**105 个 ext4 tracepoint、538 个可能的 ext4 kprobe**。

### §8.6.17 其他工具

原文列了一张表，把**本书其他章节**与《BPF Performance Tools》里的文件系统工具汇总（摘录）：

| 来源 | 工具 | 说明 |
|---|---|---|
| §5.5.6 | `syscount` | 统计系统调用（含文件系统相关） |
| [Gregg 19] | `statsnoop` | 追踪 `stat(2)` 各种变体 |
| [Gregg 19] | `syncsnoop` | 追踪 `sync(2)` 及其变体，带时间戳 |
| [Gregg 19] | `mmapfiles` | 统计 `mmap(2)` 的文件 |
| [Gregg 19] | `scread` | 统计 `read(2)` 的文件 |
| [Gregg 19] | `fmapfault` | 统计文件映射缺页 |
| [Gregg 19] | `filelife` | 追踪短命文件及其存活秒数 |
| [Gregg 19] | `vfsstat` | 常用 VFS 操作统计 |
| [Gregg 19] | `vfscount` | 统计所有 VFS 操作 |
| [Gregg 19] | `vfssize` | 显示 VFS 读/写尺寸 |
| [Gregg 19] | `fsrwstat` | 按文件系统类型显示 VFS 读写 |
| [Gregg 19] | `fileslower` | 显示慢的文件读写 |
| [Gregg 19] | `filetype` | 按文件类型与进程显示 VFS 读写 |
| [Gregg 19] | `ioprofile` | 统计 I/O 上的栈，显示代码路径 |
| [Gregg 19] | `writesync` | 按 sync 标志显示常规文件写 |
| [Gregg 19] | `writeback` | 显示回写事件与延迟 |
| [Gregg 19] | `dcstat` | 目录缓存命中统计 |
| [Gregg 19] | `dcsnoop` | 追踪目录缓存查找 |
| [Gregg 19] | `mountsnoop` | 全系统追踪 mount 与 umount |
| [Gregg 19] | `icstat` | inode 缓存命中统计 |
| [Gregg 19] | `bufgrow` | 按进程与字节显示 buffer cache 增长 |
| [Gregg 19] | `readahead` | 显示 read-ahead 命中与效率 |

其他 Linux 文件系统相关工具：`df(1)`（文件系统使用率与容量统计）、
`inotify`（Linux 上监控文件系统事件的框架）。

**ZFS 自带工具**：`zpool(1M)` 有 **iostat 子选项**，报告**池操作率（读、写）与吞吐**。
流行的附加工具 **`arcstat.pl`** 报告 **ARC 与 L2ARC 的大小与命中/未命中率**：

```bash
$ arcstat 1
    time  read  miss  miss%  dmis  dm%  pmis  pm%  mmis  mm%  arcsz     c
04:45:47     0     0      0     0    0     0    0     0    0    14G   14G
04:45:49   15K    10      0    10    0     0    0     1    0    14G   14G
04:45:50   23K    81      0    81    0     0    0     1    0    14G   14G
04:45:51   65K    25      0    25    0     0    0     4    0    14G   14G
[...]
```

列含义：**`read` / `miss`** = ARC 访问总数与未命中数；
**`miss%` / `dm%` / `pm%` / `mm%`** = ARC 未命中总比例、demand（按需）、prefetch（预取）、metadata（元数据）；
**`dmis` / `pmis` / `mmis`** = 上述三类的未命中数；
**`arcsz` / `c`** = ARC 当前大小 / ARC 目标大小。
`arcstat.pl` 是读 `kstat` 的 Perl 程序。

### §8.6.18 可视化

**随时间变化的负载**可以画成折线图，帮助识别**时间维度的使用模式**；
**读、写和其他操作分开画**会更有用。

**延迟分布的关键性质**：

> The distribution of file system latency is expected to be bimodal: one mode at low latency for file system cache hits, and another at high latency for cache misses (storage device I/O). For this reason, representing the distribution as a single value—such as a mean, mode, or median—is misleading.

**「双峰 ⇒ 用均值/众数/中位数概括会误导」**——这是本章最该带走的可视化原则。

解法是**展示完整分布的可视化**，例如**热力图**（热力图在 [[Chapter 2. Methodologies]] 介绍）。
原文的示例（Figure 8.13）展示了**启用 L2ARC 对 NFSv3 延迟的影响**：
系统有 **128 Gbytes 主存（DRAM）+ 600 Gbytes L2ARC**（读优化 SSD）；
**左半是禁用 L2ARC**，**右半是启用**。

**左半**：延迟要么低要么高，**中间有一段空隙**——低延迟是底部 0 毫秒附近的蓝线
（**很可能是主存缓存命中**），高延迟从 **3 毫秒左右**开始一直延伸到顶（**看起来像一团云，
很可能是旋转盘的延迟**）。这种**双峰延迟分布是旋转盘后端文件系统的典型形态**。

**右半**：启用 L2ARC 后，延迟**经常低于 3 毫秒**，**高延迟的次数也变少**——
**L2ARC 的延迟填补了原先空隙所在的区间**，整体文件系统延迟下降。

## §8.7 实验：主动测量

用这些工具时，**最好一直开着 `iostat(1)`** 来确认负载**确实到达了磁盘**——
**也可能根本没到**。例如测试一个「本该轻松装进文件系统缓存」的工作集大小，
读负载的期望是 **100% 缓存命中**，那么 `iostat(1)` **就不该显示大量磁盘 I/O**。
`iostat(1)` 见 [[Chapter 9. Disks]]。

### §8.7.1 即席测试

`dd(1)` 可用于**顺序文件系统性能的即席测试**：

```bash
write: dd if=/dev/zero of=file1 bs=1024k count=1k
read: dd if=file1 of=/dev/null bs=1024k
```

Linux 版 `dd` 完成后会打印统计：

```bash
$ dd if=/dev/zero of=file1 bs=1024k count=1k
1024+0 records in
1024+0 records out
1073741824 bytes (1.1 GB, 1.0 GiB) copied, 0.76729 s, 1.4 GB/s
```

**这显示的是 1.4 Gbytes/s 的文件系统写吞吐**——但原文提醒：**写回缓存在起作用，
所以这只弄脏了内存、稍后才会刷盘**（取决于 `vm.dirty_*` 可调项，见 [[Chapter 7. Memory]]）。

### §8.7.2 微基准工具

可选工具很多：Bonnie、Bonnie++、iozone、tiobench、SysBench、fio、FileBench。
原文按复杂度递增挑了几个介绍。**作者的个人推荐是用 `fio`**。

**Bonnie / Bonnie++**：简单的 C 程序，在**单个文件、单线程**上测多种负载；
最初由 Tim Bray 于 1989 年写。默认不需要参数：

```bash
$ ./Bonnie
File './Bonnie.9598', size: 104857600
[...]
              -------Sequential Output-------- ---Sequential Input-- --Random--
              -Per Char- --Block--- -Rewrite-- -Per Char- --Block--- --Seeks---
Machine    MB K/sec %CPU K/sec %CPU K/sec %CPU K/sec %CPU K/sec %CPU  /sec %CPU
          100 123396 100.0 1258402 100.0 996583 100.0 126781 100.0 2187052 100.0
164190.1 299.0
```

输出含**每个测试的 CPU 时间**——**100% 表明 Bonnie 从未阻塞在磁盘 I/O 上**，
**始终命中缓存并留在 CPU 上**。原因是**目标文件只有 100 Mbytes，在该系统上完全被缓存**。
可以用 `-s size` 改文件大小。另有 64 位版 **Bonnie-64**，以及 Russell Coker 写的
C++ 重写版 **Bonnie++**。

**⚠️ 原文的批评**：这类基准工具**可能误导**，**除非你清楚它在测什么**。
例如第一个 `putc(3)` 测试的结果**会随系统库实现而变**——那时被测的目标
**变成了系统库，而不是文件系统**。

**fio**：Jens Axboe 写的 **Flexible IO Tester**，可定制性强。原文给出**让他放弃其他工具、
改用 fio 的两个特性**：

1. **非均匀随机分布**——能更真实地模拟真实访问模式（例如 `-random_distribution=pareto:0.9`）；
2. **延迟百分位报告**，包括 **99.00、99.50、99.90、99.95、99.99**。

示例输出（随机读，8 Kbytes I/O，5 Gbyte 工作集，`pareto:0.9` 非均匀访问）：

```bash
# fio --runtime=60 --time_based --clocksource=clock_gettime --name=randread --
numjobs=1 --rw=randread --random_distribution=pareto:0.9 --bs=8k --size=5g --
filename=fio.tmp
randread: (g=0): rw=randread, bs=8K-8K/8K-8K/8K-8K, ioengine=sync, iodepth=1
fio-2.0.13-97-gdd8d
Starting 1 process
Jobs: 1 (f=1): [r] [100.0% done] [3208K/0K/0K /s] [401 /0 /0  iops] [eta 00m:00s]
randread: (groupid=0, jobs=1): err= 0: pid=2864: Tue Feb  5 00:13:17 2013
  read : io=247408KB, bw=4122.2KB/s, iops=515 , runt= 60007msec
    clat (usec): min=3 , max=67928 , avg=1933.15, stdev=4383.30
     lat (usec): min=4 , max=67929 , avg=1934.40, stdev=4383.31
    clat percentiles (usec):
     |  1.00th=[    5],  5.00th=[    5], 10.00th=[    5], 20.00th=[    6],
     | 30.00th=[    6], 40.00th=[    6], 50.00th=[    7], 60.00th=[  620],
     | 70.00th=[  692], 80.00th=[ 1688], 90.00th=[ 7648], 95.00th=[10304],
     | 99.00th=[19584], 99.50th=[24960], 99.90th=[39680], 99.95th=[51456],
     | 99.99th=[63744]
    bw (KB/s)  : min= 1663, max=71232, per=99.87%, avg=4116.58, stdev=6504.45
    lat (usec) : 4=0.01%, 10=55.62%, 20=1.27%, 50=0.28%, 100=0.13%
    lat (usec) : 500=0.01%, 750=15.21%, 1000=4.15%
    lat (msec) : 2=3.72%, 4=2.57%, 10=11.50%, 20=4.57%, 50=0.92%
    lat (msec) : 100=0.05%
  cpu          : usr=0.18%, sys=1.39%, ctx=13260, majf=0, minf=42
  IO depths    : 1=100.0%, 2=0.0%, 4=0.0%, 8=0.0%, 16=0.0%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued    : total=r=30926/w=0/d=0, short=r=0/w=0/d=0
```

**读法**：**50 百分位之前延迟极低**（5–7 μs），按这个延迟量级**可以假定是缓存命中**；
**其余百分位是缓存未命中的效果**，包含**队列尾部**——这里 **99.99 百分位是 63 ms**。

原文的评价很克制：**百分位**虽然**缺乏理解「很可能是多峰分布」所需的信息**，
**但它确实聚焦在最有意思的部分——较慢那一峰的尾部（磁盘 I/O）**。

更简单可试 **SysBench**（CPU 分析见 [[Chapter 6. CPUs]]）；要更多控制权则试 **FileBench**。

**FileBench**：可编程的文件系统基准工具，用**工作负载建模语言（Workload Model Language）**
描述应用负载以模拟之；可以模拟**不同行为的线程**并指定**同步线程行为**。
自带多种称为 **personalities** 的配置，**其中一个用来模拟 Oracle 数据库的 I/O 模型**。
**但它不易学易用**，可能只对全职做文件系统的人有价值。

### §8.7.3 缓存清空

Linux 提供**清空（丢弃条目）文件系统缓存**的方法，用于从**一致且「冷」的状态**
做基准（就像刚启动系统之后那样）。内核源码文档（`Documentation/sysctl/vm.txt`）
里的描述极简：

```bash
To free pagecache:
        echo 1 > /proc/sys/vm/drop_caches
To free reclaimable slab objects (includes dentries and inodes):
        echo 2 > /proc/sys/vm/drop_caches
To free slab objects and pagecache:
        echo 3 > /proc/sys/vm/drop_caches
```

**「其他基准跑之前先清空一切（3）」**——让系统从**一致状态（冷缓存）**开始，
从而**提供一致的基准结果**。

## §8.8 调优

原文说明：调优的具体选项与取值**取决于操作系统版本、文件系统类型与预期负载**。
这一节给**应用调用**与**两个示例文件系统**（ext4 与 ZFS）的参数。
**页缓存的调优见 [[Chapter 7. Memory]]**。

### §8.8.1 应用调用

**前提**：§8.3.7 已说明，**同步写负载**的性能可以用 `fsync(2)` **把一组逻辑写一起刷出**
来改善，而不是用 `O_DSYNC` / `O_RSYNC` 逐个同步。

另有两个提供**缓存资格提示（cache eligibility hints）**的调用：**`posix_fadvise()` 与 `madvise(2)`**。

**posix_fadvise()**（`fadvise64(2)` 的库封装）作用于**文件的一个区域**：

```c
int posix_fadvise(int fd, off_t offset, off_t len, int advice);
```

Linux 的 advice 取值（原文 Table 8.8 摘录）：

| Advice | 含义 |
|---|---|
| `POSIX_FADV_SEQUENTIAL` | 指定数据范围将被**顺序访问** |
| `POSIX_FADV_RANDOM` | 指定数据范围将被**随机访问** |
| `POSIX_FADV_NOREUSE` | 数据**不会被复用** |
| `POSIX_FADV_WILLNEED` | 数据**近期还会被用到** |
| `POSIX_FADV_DONTNEED` | 数据**近期不会被用到** |

**内核用这些信息决定何时预取、何时缓存**，从而**为高优先级数据提高缓存命中率**。

**madvise(2)** 作用于**内存映射**：

```c
int madvise(void *addr, size_t length, int advice);
```

| Advice | 含义 |
|---|---|
| `MADV_RANDOM` | 偏移将被**随机顺序访问** |
| `MADV_SEQUENTIAL` | 偏移将被**顺序访问** |
| `MADV_WILLNEED` | 数据**还会被需要**（请缓存） |
| `MADV_DONTNEED` | 数据**不会被需要**（不必缓存） |

与 `posix_fadvise()` 一样，**内核能用这些信息做出更好的缓存决策**。

**判据**：这两个调用是**成本最低的调优手段**——不改配置、不重启。
**读多且顺序 → `POSIX_FADV_SEQUENTIAL` + `MADV_SEQUENTIAL`（打开预取与预读）**；
**一次性扫过不再复用 → `POSIX_FADV_DONTNEED`（避免污染缓存）**。

### §8.8.2 ext4

Linux 的 ext2 / ext3 / ext4 有**四种调优方式**：**挂载选项**、**`tune2fs(8)` 命令**、
**`/sys/fs` 属性文件**、**`e2fsck(8)` 命令**。

**挂载选项与 tune2fs**：挂载选项可在挂载时手动用 `mount(8)` 设置，或在开机时写进
`/boot/grub/menu.lst` 与 `/etc/fstab`；可用选项见 `mount(8)` man 页。

```bash
# man mount
[...]
FILESYSTEM-INDEPENDENT MOUNT OPTIONS
[...]
       atime  Do not use the noatime feature, so the inode access time is con-
              trolled  by  kernel  defaults.  See also the descriptions of the
              relatime and strictatime mount options.

       noatime
              Do not update inode access times on this  filesystem  (e.g.  for
              faster access on the news spool to speed up news servers).  This
[...]
       relatime
              Update  inode  access  times  relative to modify or change time.
              Access time is only updated if the previous access time was ear-
              lier  than  the  current  modify  or  change  time.  (Similar to
              noatime, but it doesn't break mutt or  other  applications  that
              need  to know if a file has been read since the last time it was
              modified.)

              Since Linux 2.6.30, the kernel defaults to the behavior provided
              by   this   option  (unless  noatime  was  specified),  and  the
              strictatime option is required to obtain traditional  semantics.
              In  addition, since Linux 2.6.30, the file's last access time is
              always updated if it is more than 1 day old.
[...]
```

**`noatime` 历史上被用来提升性能**（避免访问时间戳更新及其磁盘 I/O）；
**现在 `relatime` 是默认**，同样减少了这些更新。
注意 `relatime` **不会像 `noatime` 那样破坏 mutt 这类需要知道「文件自上次修改后是否被读过」的应用**。

**ext4 的挂载选项有自己的 man 页：`ext4(5)`**（`mount(8)` 只覆盖通用选项与文件系统专属选项）：

```bash
# man ext4
[...]
Mount options for ext4
[...]
       The  options  journal_dev, journal_path, norecovery, noload, data, com-
       mit, orlov, oldalloc, [no]user_xattr, [no]acl, bsddf,  minixdf,  debug,
       errors,  data_err,  grpid,  bsdgroups, nogrpid, sysvgroups, resgid, re-
       suid, sb, quota, noquota, nouid32, grpquota, usrquota,  usrjquota,  gr-
       pjquota, and jqfmt are backwardly compatible with ext3 or ext2.

       journal_checksum | nojournal_checksum
              The  journal_checksum option enables checksumming of the journal
              transactions.  This will allow the recovery code in  e2fsck  and
[...]
```

当前挂载设置可以用 **`tune2fs -l device`** 与 **`mount`（不带选项）** 查看；
**`tune2fs(8)` 还能设置或清除各种挂载选项**。

**常用性能选项是 `noatime`**：避免文件访问时间戳更新——**如果文件系统用户不需要这些时间戳，
它就能减少后端 I/O**。

**/sys/fs 属性文件**：部分可调项可以通过 `/sys` 文件系统**在线设置**：

```bash
# cd /sys/fs/ext4/nvme0n1p1
# ls
delayed_allocation_blocks  last_error_time        msg_ratelimit_burst
err_ratelimit_burst        lifetime_write_kbytes  msg_ratelimit_interval_ms
err_ratelimit_interval_ms  max_writeback_mb_bump  reserved_clusters
errors_count               mb_group_prealloc      session_write_kbytes
extent_max_zeroout_kb      mb_max_to_scan         trigger_fs_error
first_error_time           mb_min_to_scan         warning_ratelimit_burst
inode_goal                 mb_order2_req          warning_ratelimit_interval_ms
inode_readahead_blks       mb_stats
journal_task               mb_stream_req
# cat inode_readahead_blks
32
```

这说明 **ext4 最多预读 32 个 inode 表块**。**注意并非所有这些文件都是可调项**——
有些只用于展示信息。文档在 Linux 源码的 `Documentation/admin-guide/ext4.rst`。

**e2fsck**：**`e2fsck -D -f /dev/hdX`** 可以**重建目录索引**，有助于改善性能。
（`e2fsck` 的其他选项与文件系统检查修复有关。）

### §8.8.3 ZFS

ZFS 支持**大量按文件系统的可调参数（properties）**，另有一小部分可**系统级**设置，
可用 `zfs(1)` 列出：

```bash
# zfs get all zones/var
NAME       PROPERTY              VALUE                  SOURCE
[...]
zones/var  recordsize            128K                   default
zones/var  mountpoint            legacy                 local
zones/var  sharenfs              off                    default
zones/var  checksum              on                     default
zones/var  compression           off                    inherited from zones
zones/var  atime                 off                    inherited from zones
[...]
```

输出列是**属性名、当前值、来源**。**来源说明它是怎么被设置的**：
从更高层的 ZFS dataset **继承**、**默认值**、还是**本地设置**。

关键性能参数（原文 Table 8.10 摘录）：

| 参数 | 可选值 | 说明 |
|---|---|---|
| `recordsize` | 512 到 128 K | 建议的文件块大小 |
| `compression` | `on` / `off` / `lzjb` / `gzip` / `gzip-[1–9]` / `zle` / `lz4` | 轻量算法（如 `lzjb`）**靠减轻后端 I/O 拥塞在某些场景提升性能** |
| `atime` | `on` / `off` | 访问时间戳更新（**读之后也会产生一些写**） |
| `primarycache` | `all` / `none` / `metadata` | ARC 策略；**低优先级文件系统（如归档）造成的缓存污染**可用 `none` 或仅 `metadata` 减少 |
| `secondarycache` | `all` / `none` / `metadata` | L2ARC 策略 |
| `logbias` | `latency` / `throughput` | 同步写的建议：`latency` **用日志设备**，`throughput` **用池设备** |
| `sync` | `standard` / `always` / `disabled` | 同步写行为 |

> The most important parameter to tune is usually record size, to match the application I/O. It usually defaults to 128 Kbytes, which can be inefficient for small, random I/O. Note that this does not apply to files that are smaller than the record size, which are saved using a dynamic record size equal to their file length. Disabling atime can also improve performance if those timestamps are not needed.

**要点**：① **最重要的参数通常是记录大小**，要**匹配应用的 I/O**；
默认 **128 Kbytes 对小的随机 I/O 效率很低**；② **比记录大小小的文件不适用这条**——
它们用**等于文件长度的动态记录大小**保存；③ **不需要时间戳就关掉 `atime`**。

**系统级可调项**包括：**事务组（TXG）同步时间**（`zfs_txg_synctime_ms`、`zfs_txg_timeout`）、
**metaslab 切换到空间优化还是时间优化分配**的阈值（`metaslab_df_free_pct`）。
**把 TXG 调小可以通过减少与其他 I/O 的争用与排队来提升性能**。

## 章末选型判据

| 场景 | 做什么 | 别做什么 |
|---|---|---|
| 报告「磁盘很忙」 | 先用**事务成本公式**算「应用事务里有多少 % 的时间在文件系统上」 | 只看设备利用率就断言「磁盘是瓶颈」——后台 flush 与其他租户的 I/O 会骗你 |
| 延迟只报一个平均值 | 报**分布**（直方图 / 热力图），并**按操作类型拆分** | 只报均值/中位数——文件系统延迟**天然双峰**，单值必然误导 |
| 读慢、怀疑是存储 | 先分清是**缓存未命中的磁盘延迟**还是**逻辑与物理的错配** | 直接上 mmap——省不掉毫秒级的磁盘延迟 |
| 读顺序且数据量大于缓存 | 用 `posix_fadvise(POSIX_FADV_SEQUENTIAL)` / `madvise(MADV_SEQUENTIAL)` **显式提示** | 指望文件系统自己猜——预取检测不佳反而污染缓存 |
| 一次性扫描、不再复用 | `POSIX_FADV_DONTNEED` / `MADV_DONTNEED` | 让这些数据占着缓存把热数据挤出去 |
| 需要不丢数据 | 用 **`fsync(2)` 分组提交** | 每次写都用 `O_SYNC`——慢且元数据更新次数多 |
| 数据库自管缓存 | 评估 **Direct I/O（`O_DIRECT`）** 或 **raw I/O** | 忘了 Direct I/O **仍受记录大小约束**，尺寸不对会 `EINVAL` |
| 直写盘之后没法观测 | 先想清楚 **raw I/O 会让常规文件系统工具链全部失效** | 直接切 raw 再发现没法备份、没法观测 |
| 负载重、读少写多 | 考虑**separate spindles**：日志与数据分盘（[[Chapter 9. Disks]]） | 让两类负载在同一块盘上互相制造随机 I/O |
| 缓存占内存让人慌 | **当作正常**（闲内存就该被用来做缓存） | 把它当泄漏去「清理」——应用要内存时内核本就会回收 |
| 怀疑某进程在狂开文件 | `opensnoop`（开销可忽略） | `fatrace` 在繁忙系统上每秒几万行 |
| 怀疑某文件是 I/O 热点 | `filetop` | `strace`——ptrace 开销会**扭曲测出的延迟** |
| 要看慢 I/O 的细节 | `ext4slower`（只打慢于阈值的）或 `ext4dist`（看分布） | `strace` 的时间戳 |
| 要给延迟分布做图 | 热力图（x 轴时间、y 轴延迟） | 折线图 + 均值——双峰分布看不出结构 |
| 做微基准 | **先定 WSS**（小于内存则全命中缓存，等于没测盘），**跑前 `echo 3 > /proc/sys/vm/drop_caches`** | 不清缓存就对比两次结果——量到的是缓存 |
| 基准只报平均延迟 | 用 **fio 并看百分位**（99.00–99.99） | 只看均值——尾部才是用户感受到的那部分 |
| 选 fio 的理由 | **非均匀随机分布**（如 `pareto:0.9`）+ **百分位报告** | 默认的均匀随机——真实访问是长尾的 |
| ZFS 上要调优 | **先调 `recordsize` 匹配应用 I/O**，再考虑 `atime`、`primarycache`、`logbias` | 沿用默认 128 K 跑小随机 I/O——效率很差 |
| ext4 上要调优 | 先确认 **`relatime`/`noatime`**、**记录大小**、**inode 预读块数**（`/sys/fs/ext4/*/inode_readahead_blks`） | 忘了 **e2fsck -D** 可以重建目录索引 |
| RAID 盘变慢 | 查**是否在 rebuild / resilver**（可能持续数小时到数天） | 只看平均吞吐——重建会持续抢占 I/O |
| 要找「谁在发这个 I/O」 | 在 bpftrace 里加 **`ustack`** 作为直方图键 | 只按进程名聚合——分不清是应用请求还是后台任务 |
