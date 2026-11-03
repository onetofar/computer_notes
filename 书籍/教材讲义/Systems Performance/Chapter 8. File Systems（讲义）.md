---
title: Chapter 8. File Systems（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - 文件系统
  - 页缓存
  - 逻辑IO与物理IO
  - 延迟分析
  - 性能工程
source: "《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg，Addison-Wesley 2020，英文原版）第 8 章"
companion: "[[08-Chapter 8. File Systems]]"
---

# 第 8 章 讲义：一次 read() 背后有多少次 IO

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[08-Chapter 8. File Systems|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：`otf-server` 全仓**不碰文件系统**（无 `fopen`/`ifstream`/`ofstream`），唯一的"落盘"是 `src/common/logger.cpp` 的 `std::fprintf(stderr, ...)`；这既是本章的对照基准，也把 Ch7 的 swap 话题提前排除了。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原章。

---

## 0. 开局：写 1 个字节，磁盘动了 128 KB

先做一件小事。在一个已经存在的文件里，**改写 1 个字节**：

```c
pwrite(fd, "X", 1, offset);   /* 应用的全部动作 */
```

应用侧只有这一行。按理说磁盘上也就该动 1 个字节。但作者在 §8.3.12 里把真实发生的事一步步列了出来：

1. 文件系统认出这个位置属于一个 **128 KB 的记录**（record），这个记录**不在缓存里**（但指向它的元数据在）。
2. 文件系统请求把这条 128 KB 记录**从磁盘读上来**。
3. 磁盘设备层把这 128 KB 拆成设备能接受的小读。
4. 磁盘实际读了 **128 KB**。
5. 文件系统把新字节替换进内存里那条记录。
6. 稍后（可能几秒后），那条 128 KB 的脏记录被**写回磁盘**。
7. 文件系统还要写**新的元数据**（比如访问时间戳，或 copy-on-write 的引用更新）。
8. 磁盘又写了一次。

于是：

> So, while the application performed only a single 1-byte write, the disks performed multiple reads (128 Kbytes in total) and more writes (over 128 Kbytes).

**应用写了 1 字节，磁盘读了 128 KB、写了 128 KB 以上。**

这就是全章的种子问题：**逻辑 I/O（应用要的）≠ 物理 I/O（磁盘做的）**，而且两者的比例可以是 1 : 262144。

（推演）把它想成寄一封信要填一张表：你要寄的东西是 1 字节，但快递系统按"一整个信封"搬运——因为它只认得整块信封，不认得里面的一个字。

---

## 1. §8.1 术语：先把"IO"这个词钉死

这一节看着像术语表，但里面藏着一个后面反复用到的切分。

**逻辑 I/O（logical I/O）**：应用向文件系统发起的 I/O。
**物理 I/O（physical I/O）**：文件系统直接发给磁盘的 I/O。

而"I/O"这个词本身有更严的定义：

> I/O does not include open(2) and close(2) (although those calls update metadata and can cause indirect disk I/O).

`open()` / `close()` **不算 I/O**，但它们会改元数据，从而**间接**引起磁盘 I/O。这条区分在第 9 章还会出现一次（"非数据传输类磁盘命令"），到那时你会发现它其实是一回事。

另外几个后面会反复出现的名词：

| 术语 | 一句话 |
|---|---|
| **inode** | 一个索引节点，装着文件对象的元数据：权限、时间戳、数据指针 |
| **block size / record size** | 文件系统在磁盘上组织数据的块大小 |
| **throughput** | 应用与文件系统之间的当前数据传输率（字节/秒） |
| **VFS** | 虚拟文件系统，内核里抽象不同文件系统类型的统一接口 |
| **volume** | 一个存储实例，可能是整块盘的一部分，也可能横跨多块盘 |

---

## 2. §8.2.1–8.2.3 模型：三层就够了

作者说这一章的三个模型都是"示意性的"（simple models），但它们已经把后面所有内容的骨架搭好了。

**第一层：接口**（Figure 8.1）。应用看到的是"对象操作"——open/read/write/stat/mkdir。Linux 还提供 readv(2)、writev(2)、openat(2) 等变体。图里同时标出了**逻辑操作发生在哪里**、**物理操作发生在哪里**——这是本章所有分析的基础。

**第二层：内存缓存**（Figure 8.2）。读请求要么从缓存命中返回，要么从磁盘取回并**顺便把缓存填热**（warming it up）。写也会先进缓存的缓冲区，稍后再刷盘。

> The file system cache may also buffer writes to be written (flushed) later.

**第三层：二级缓存**（Figure 8.3）。可以是任意介质的内存，作者这里画的是闪存——这一层最早是他 2007 年为 ZFS 做的（L2ARC）。

---

## 3. §8.3.1 延迟：文件系统性能的头号指标

> File system latency is the primary metric of file system performance, measured as the time from a logical file system request to its completion.

关键在"**应用线程会在这段时间里阻塞**"：文件系统的延迟**成比例地**传导成应用的延迟。

但作者立刻给了一个反例——不是所有文件系统延迟都会伤到应用：

- **非阻塞 I/O**：应用不等；
- **预取**：后台干的；
- **异步线程发起的 I/O**：比如后台 flush 线程。

麻烦的是操作系统历来只给**设备级**统计，不给文件系统延迟统计。而设备级统计常常**既不相关又误导人**：

> An example of this is where file systems perform background flushing of written data, which may appear as bursts of high-latency disk I/O. From the disk device-level statistics, this looks alarming; however, no application is waiting on these to complete.

（推演）设备级统计像"急诊室的进车数"：救护车进进出出看着很忙，但候诊椅上可能一个病人也没有。

---

## 4. §8.3.2 缓存：内存"变少"不是 bug

> The principle is: If there is spare main memory, do something useful with it. When applications need more memory, the kernel should quickly free it from the file system cache for use.

所以看到 `free` 显示内存被吃光、缓存占了大头时，**新用户会慌，但完全正常**。

Linux 上常见的缓存类型（Table 8.1）：

| 缓存 | 例子 |
|---|---|
| page cache（页缓存） | 操作系统页缓存 |
| 文件系统主缓存 | ZFS ARC |
| 文件系统二级缓存 | ZFS L2ARC |
| 目录缓存 | dentry cache |
| inode 缓存 | inode cache |
| 设备缓存 | ZFS vdev |
| 块设备缓存 | buffer cache |

这张表在第 9 章会再次出现（磁盘侧的缓存清单），到时候你会发现**很多层是重叠的**。

---

## 5. §8.3.3 随机 vs 顺序：碎片化是这么来的

**顺序 I/O**：每次 I/O 的偏移量接着上一次的结束位置。
**随机 I/O**：偏移量之间没有明显关系。

因为存储设备的物理特性（Ch9 主题），文件系统历史上一直在努力**减少随机 I/O**——把文件数据顺序、连续地放上磁盘。做不到的时候，就叫**碎片化（fragmentation）**：

> The term fragmentation describes what happens file systems do this poorly, causing files to be scattered over a drive, so that sequential logical I/O yields random physical I/O.

**注意这半句**：碎片化会让**顺序的逻辑 I/O 变成随机的物理 I/O**。这是第 8 章和第 9 章的接口——逻辑层的"顺序"不等于物理层的"顺序"。

---

## 6. §8.3.4–8.3.5 预取与 read-ahead：赌下一次读

典型场景：备份要顺序读一大块文件数据。数据**装不进缓存**，或者读一次就丢（取决于淘汰策略）。缓存命中率低 → 性能差。

**预取（prefetch）**的做法：根据当前和上一次的偏移量判断出"这是顺序读"，于是**在应用还没要之前**就把后面的数据读进缓存。流程是：

1. 应用发 `read(2)`，进内核；
2. 数据没缓存 → 文件系统向磁盘发读；
3. 比较上一次偏移和当前位置，发现是顺序 → **追加发出预取读**；
4. 第一次读完成，数据和控制权交回应用；
5. 预取读陆续完成，把缓存填热；
6. 后续顺序读**从 RAM 命中**，很快。

赌对了，应用看到顺序读性能显著提升（磁盘跑在应用前面，前提是带宽够）；赌错了：

> When prefetch detection works poorly, unnecessary I/O is issued that the application does not need, polluting the cache and consuming disk and I/O transport resources.

注意 read-ahead 这个词的历史：Linux 用 `readahead(2)` 这个系统调用名指"让应用显式把文件缓存预热"。

---

## 7. §8.3.6–8.3.7 写回缓存与同步写：可靠性的账

**写回缓存（write-back caching）**：写一旦**进了内存**就算完成，落盘推迟到以后异步做（这一步叫 flushing）。

代价是可靠性：

> The trade-off is reliability. DRAM-based main memory is volatile, and dirty data can be lost in the event of a power failure.

更糟的是"**写了一半**"——落盘状态不完整。如果坏的是**元数据**，文件系统可能整个加载不了；如果坏的是应用读的数据内容，**业务就危险了**。

**同步写（synchronous writes）**：只有真正写进持久化存储（含必要的元数据变更）才算完成。慢得多，因为要真吃磁盘延迟，有时还要多次 I/O。作者举了数据库日志写入这个例子：异步写导致数据损坏的风险不可接受。

同步写有两种形态：

- **单次同步写**：`open()` 时带 `O_SYNC`（或 `O_DSYNC` / `O_RSYNC`，glibc 在 Linux 2.6.31 起把后两个映射成 `O_SYNC`）。有些文件系统还有挂载选项强制所有写同步。
- **把之前的写同步提交**：在代码里的检查点用 `fsync(2)` 一次性提交前面积压的写。这样做**能提升性能**——把多次写打包，并且避免多次元数据更新。

另外，关闭文件句柄、未提交的缓冲太多时，也会触发一次提交。前者很容易被察觉：解一个装了很多文件的压缩包时**长时间卡顿**，NFS 上尤其明显。

---

## 8. §8.3.8–8.3.9 Raw、Direct 与非阻塞 I/O

**Raw I/O**：直接对磁盘偏移发 I/O，**完全绕开文件系统**。数据库常用——它自己能管好缓存。代价是软件复杂、运维困难（备份/恢复和观测工具都用不了）。

**Direct I/O**：仍然用文件系统，但**绕过文件系统缓存**（Linux 上是 `O_DIRECT` 打开标志）。它和同步写类似（但没有 `O_SYNC` 的保证），读写都适用。它没那么"直接"——文件偏移到磁盘偏移的映射还得文件系统做；I/O 也可能被**调整大小**以对齐文件系统的记录大小，否则直接报错 `EINVAL`。具体文件系统可能连预取也一起关掉。

**非阻塞 I/O**：`O_NONBLOCK` / `O_NDELAY` 让读写**不等待**，而是返回 `EAGAIN` 让应用稍后再试。Linux 5.1 还加了 `io_uring` 这个新的异步 I/O 接口。

---

## 9. §8.3.10 内存映射文件：mmap 的收益与陷阱

`mmap(2)` 把文件映射到进程地址空间，直接按内存偏移访问。好处有两条：

- 省掉 `read(2)`/`write(2)` 的**系统调用执行和上下文切换**开销；
- 如果内核支持直接映射文件数据缓冲区到进程地址空间，还能省掉**一次数据拷贝**。

但作者对一种常见做法明确泼了冷水：

> I've noticed a tendency to try using mmap(2) to solve file system performance issues without first analyzing them. If the issue is high I/O latency from disk devices, avoiding the small syscall overheads with mmap(2) may accomplish very little, when the high disk I/O latency is still present and dominant.

**先量，再改**——这句话在第 9 章还会再出现一次。

多核系统上 mmap 也有自己的开销：要让每个 CPU 的 MMU 保持一致，需要 **TLB shootdown**（CPU 间互相通知撤销映射）。内核和映射方式不同，可以用**延迟 shootdown（lazy shootdowns）**把开销摊薄。

---

## 10. §8.3.11 元数据：一次 stat 也能惹出磁盘 I/O

**逻辑元数据**：能从 POSIX 接口读到或写进去的元数据。

- *显式*：`stat(2)` 读文件统计、`creat(2)`/`unlink(2)` 建删文件、`mkdir(2)`/`rmdir(2)`、`chown(2)`/`chmod(2)`。
- *隐式*：访问时间戳更新、目录修改时间戳更新、已用块位图更新、空闲空间统计。

**"元数据密集（metadata-heavy）"** 通常说的就是逻辑元数据。典型例子：web 服务器用 `stat(2)` 检查文件自缓存以来有没有变——这个频率**远高于**读文件内容本身。

**物理元数据**：实现磁盘布局所需的元数据——superblock、inode、数据指针块、空闲链表等。

作者顺手把话递给了下一节：**逻辑元数据和物理元数据，是逻辑 I/O 与物理 I/O 有差别的原因之一**。

---

## 11. §8.3.12 逻辑 vs 物理 I/O：五种扭曲

这一节是全章的核心。磁盘 I/O 相对于应用 I/O，可以是**无关的、间接的、隐式的、缩小的、或膨胀的**。

### 无关（Unrelated）

与应用完全无关的磁盘 I/O，来自：别的应用、别的租户（云环境下可能是另一个客户）、内核任务（比如重建软件 RAID、异步校验文件系统）、管理任务（比如备份）。

### 间接（Indirect）

由应用引起，但**没有对应的即时应用 I/O**：

- **预取**：多发了应用可能用也可能不用的 I/O；
- **写缓冲**：写回缓存把写推迟并合并，之后成批刷下去——有的系统能压几十秒，于是磁盘上表现为**大而不频繁的尖峰**。

### 隐式（Implicit）

由应用的**指令**（而非系统调用）触发的磁盘 I/O。典型是 mmap 文件的 load/store 指令触发的读或写。这在排查时特别难受：

> This can be confusing when analyzing file system operations (read(2), write(2)) and failing to find the source of the I/O (since it is triggered by instructions and not syscalls).

### 缩小（Deflated）

磁盘 I/O 比应用 I/O 小，甚至根本没有：

- **缓存命中**：读直接从内存满足；
- **写取消（write cancellation）**：同一段偏移在刷盘前被改了好几次，最后只写一次；
- **压缩**；
- **合并（coalescing）**：把顺序 I/O 合并后再发（减少 I/O **次数**，但总量不变）；
- **内存文件系统**（tmpfs）：内容可能永远不落盘。

### 膨胀（Inflated）

磁盘 I/O 比应用 I/O 大，这是常态：

- **文件系统元数据**：额外加的 I/O；
- **记录大小**：I/O 大小向上取整（膨胀字节数），或被切碎（膨胀次数）；
- **日志（journaling）**：启用的话磁盘写能**翻倍**——一份写日志，一份写最终目的地；
- **卷管理器 parity**：读-改-写循环，额外 I/O；
- **RAID 膨胀**：多写一份 parity，或写到镜像卷。

### 五种扭曲合起来的算术

回到开局那个 1 字节的写，套上这套分类：

- 应用 I/O：1 字节写。
- 记录 128 KB 未缓存 → 读 **128 KB**（间接/膨胀的混合）。
- 脏记录刷回 → 写 **128 KB**。
- 元数据更新 → 额外写。

于是逻辑 I/O（1 B）与物理 I/O（> 256 KB）差了两个数量级。**这就是为什么"磁盘忙"和"应用慢"之间不能划等号**，也是为什么 §8.5.1 批评"只盯着磁盘看"的排查方式：

> A common troubleshooting strategy has been to ignore the file system and focus on disk performance instead. This assumes that the worst I/O is disk I/O, so by analyzing only the disks you have conveniently focused on the expected source of problems.

---

## 12. §8.3.13–8.3.16 「每秒 500 次操作」这句话几乎没信息量

> You can't tell much about a workload of "500 operations/s" from the rate alone.

同样 500 次/秒，可能是**全部命中缓存**（主存速度），也可能是**全部打到磁盘**（慢好几个数量级）。决定因素还有：随机还是顺序、读还是写、同步还是异步写、I/O 多大、是否连带其他操作类型、CPU 执行成本（以及系统有多 CPU 忙）、存储设备 characteristics。

作者在 ZFS 上测了一组（Table 8.2，空闲的 Xeon 2.4 GHz 上）：

| 操作 | 平均延迟 (μs) |
|---|---|
| `open(2)`（inode 已缓存） | 2.2 |
| `close(2)`（无脏数据） | 0.7 |
| `read(2)` 4 KB（缓存命中） | 3.3 |
| `read(2)` 128 KB（缓存命中） | 13.9 |
| `write(2)` 4 KB（异步） | 9.3 |
| `write(2)` 128 KB（异步） | 55.2 |

**这些测试根本没有碰到存储设备**——测的是文件系统软件和 CPU 的速度。有些特殊文件系统（`/proc`、`/sys`、`/dev`）**永远不访问持久化设备**。

同一章的另外三节：

- **§8.3.14 特殊文件系统**：`/tmp`、`/dev`、`/proc`、`/sys` 各有各的用途。有一行很实用的脚注：`grep '^nodev'/proc/filesystems` 可以列出**不使用存储设备的**文件系统类型。
- **§8.3.15 访问时间戳**：很多文件系统记录访问时间，**每次读文件都要改元数据、产生一次写**，消耗磁盘 I/O。关掉它就是一次实实在在的调优。
- **§8.3.16 容量**：文件系统快满时性能会退化——找空闲块要花更多 CPU 和 I/O；而且空闲区变小、变稀疏，I/O 变小或变随机。

---

## 13. §8.4.1–8.4.6 架构：VFS 与四层缓存

**§8.4.1 I/O 栈**（Figure 8.6）：从应用和系统库 → 系统调用 → 内核。系统调用**直接**到磁盘设备子系统那条路是 **raw I/O**；经过 VFS 和文件系统的那条是**文件系统 I/O**（含跳过文件系统缓存的 direct I/O）。

**§8.4.2 VFS**（Figure 8.7）：给不同文件系统类型提供统一接口。它起源于 SunOS，现在已是文件系统的事实标准抽象。有个易混点：Linux VFS **复用了 inode 和 superblock 这两个词**来指 VFS 对象——而磁盘上真正的布局结构通常带文件系统前缀（`ext4_inode`、`ext4_super_block`）。**VFS 的 inode 和 superblock 只在内存里**。

VFS 还能当**统一的测量点**：不管底下是哪个文件系统，都能在同一层量。

**§8.4.3 四层缓存**（Figure 8.8）：

- **buffer cache**：Unix 当年只有它，固定大小，在块设备接口层。后来加了页缓存，两者并存带来调优难题、双重缓存和同步开销。Linux 2.4 起把 buffer cache **也存进页缓存**（统一缓冲区缓存），避开这些开销。功能还在，只是现在主要给块设备 I/O 提速。
- **page cache**：1985 年 SunOS 虚拟内存重写时引入，缓存虚拟内存页（含映射的文件系统页）。**比 buffer cache 高效**——后者每次查找都要把文件偏移翻译成磁盘偏移。大小是动态的，会涨到吃满可用内存，应用需要时再释放。Linux 有一个可调参数平衡"从页缓存换出"和"换出到 swap"（swappiness，Ch7 主题）。脏页由内核线程刷盘（2.6.32 前是 pdflush 线程池，之后换成每设备一个 `flush` 线程）。刷盘的触发条件：每隔一段时间（30 秒）、`sync(2)`/`fsync(2)`/`msync(2)`、脏页太多（`dirty_ratio`/`dirty_bytes`）、页缓存里没有可用页。内存紧张时 `kswapd` 也会参与。
- **dentry cache**：记住"目录项 → VFS inode"的映射。路径查找时每一步都能直接命中 inode 映射，而不用翻目录内容。条目存在哈希表里。后来加了 **RCU-walk**：走路径时不更新 dentry 引用计数（多核高并发查找时，引用计数与缓存一致性的开销是瓶颈）。碰到不在缓存里的 dentry，就退回慢速的 ref-walk。Dcache 还做**负缓存**——记住"查过且不存在"，加速失败的查找（比如找共享库）。
- **inode cache**：缓存 VFS inode，`stat(2)` 返回的大部分属性来自这里。它的查找大多通过 Dcache 间接完成。

**§8.4.4 关键特性**：

- **Block vs. Extent**：基于块的文件系统用固定大小块，元数据块里存指针；文件一大就需要很多指针，块的位置还可能散开 → 随机 I/O。用**可变块大小**能缓解。**基于 extent 的文件系统**预分配连续空间（extent，长度可变），随文件增长。好处：流式性能更好、随机 I/O 也更好（数据局部化）、元数据对象更少。
- **Journaling**：日志记录文件系统的变更，崩溃后可以**原子地重放**——要么全成功要么全失败，文件系统能快速回到一致状态。没有日志的文件系统崩溃时可能损坏，恢复要遍历所有结构，TB 级可能要几个小时。日志是**同步写盘**的，有的文件系统能配独立设备。只记元数据的日志配合 copy-on-write 来保证数据完整性。
- **Copy-on-Write**：不覆盖已有块，而是①写新位置 ②更新引用 ③把旧块加进空闲链表。既保完整性，又**把随机写变成顺序写**。但接近容量上限时 COW 会让文件在盘上的布局变碎（尤其 HDD），需要 defrag。
- **Scrubbing**：异步读所有数据块并校验 checksum，尽早发现坏盘。**但会伤性能**，应该低优先级或低负载时做。

**§8.4.5 文件系统类型**：FFS（把盘分成 cylinder group，文件 inode 和数据尽量放在同一组里；块大小提到 4 KB；**块交错**给内核留出发起下一个读的时间）、ext3/ext4（journaling、extents、预分配、延迟分配、更快的 fsck）、XFS（allocation group 并行、在线 defrag）、ZFS（池化存储、COW、ARC、L2ARC、SLOG、快照）、btrfs。

其中 ext4 的一个设计值得单拎出来——**延迟分配（delayed allocation）**：块分配推迟到刷盘时才做，好让写入能通过 multiblock allocator **合并成一组**，减少碎片。

**§8.4.6 卷与池**：卷（volume）把多块盘呈现为一块虚拟盘；池化存储（pooled storage，ZFS/btrfs 用的）把多块盘放进一个池，从池里切出多个文件系统，**可以任意伸缩**。附加的性能注意点：stripe 宽度要匹配负载；**虚拟设备的利用率容易让人困惑**（要去看底层物理设备）；CPU 开销（尤其是 RAID parity 计算）；**重建（rebuilding/resilvering）**——加一块空盘进 RAID 组并灌满数据，这会大量吃 I/O，可能持续数小时甚至数天。

---

## 14. §8.5.2 延迟分析：先算"时间花在哪儿"

方法论一节给了八条路（Table 8.3），作者建议的起步顺序是：**延迟分析 → 性能监控 → 工作负载刻画 → 微基准 → 静态调优**。

**延迟分析**先量每个文件操作的延迟，公式就一行：

```
operation latency = time (operation completion) - time (operation request)
```

关键是**从哪一层量**（Table 8.4）：

| 层 | 优点 | 缺点 |
|---|---|---|
| 应用 | 最贴近应用受影响的真实度量；还能看出这是主流程还是异步路径 | 手段随应用和版本而变 |
| 系统调用接口 | 文档齐全；OS 工具和静态追踪都能看 | **会把所有文件系统类型都算进来**（统计信息、socket），不筛会乱；同一个功能可能有多个系统调用（`read(2)`、`pread64(2)`、`preadv(2)`、`preadv2(2)`…）都得量 |
| VFS | 所有文件系统的统一接口，一次调用覆盖（`vfs_write()`） | 同样会混入非存储文件系统 |
| 文件系统顶层 | 只追特定文件系统类型，还能拿到内部上下文 | 强相关于具体文件系统实现 |

> For file systems that have a high cache hit rate (over 99%), per-interval averages can become dominated by cache hit latency.

命中率超过 99% 时，**区间平均值会被缓存命中的延迟主导**，那些真正要紧的**离群点（outliers）**就看不见了。所以要看**全分布**（直方图、热力图）或**逐次操作的延迟列表**。

### 事务成本：把文件系统换算成"占了应用多少时间"

另一个呈现方式——用**应用一次事务**里花在文件系统上的阻塞时间来表示：

```
percent time in file system = 100 * total blocking file system latency / application transaction time
```

作者给了两个例子：

- 事务 200 ms，其中等文件系统 180 ms → **90%**。**消除文件系统延迟最多能让性能提升 10 倍**。
- 事务 200 ms，其中只有 2 ms 花在文件系统 → **1%**。

> This result is incredibly useful, as it can steer the performance investigation to the real source of latency.

第二个例子的价值在于：**1% 这个数字直接告诉你别再查磁盘了**，去查 CPU、查锁、查别的。

---

## 15. §8.5.3–8.5.4 工作负载刻画与监控

刻画文件系统负载的**基本属性**：

- 操作速率与操作类型
- 文件 I/O 吞吐
- 文件 I/O 大小
- 读写比
- 同步写比例
- 随机 vs 顺序的文件偏移访问

作者给了一个把属性合起来说的例子：

> On a financial trading database, the file system has a random read workload, averaging 18,000 reads/s with an average read size of 4 Kbytes. The total operation rate is 21,000 ops/s, which includes reads, stats, opens, closes, and around 200 synchronous writes/s. The write rate is steady while the read rate varies, up to a peak of 39,000 reads/s.

**进阶清单**（当作 checklist 用）里最该记住的一条：

> Why is file system I/O issued (user-level call path)?

以及性能侧要问的：平均延迟是多少、有没有高延迟离群点、完整分布长什么样、有没有资源控制（throttling）在起作用。

**监控**的两个核心指标就是**操作速率**和**操作延迟**。作者提醒了一句很实际的：

> Unfortunately, in Linux there are usually no readily available statistics for file system operations (exceptions include, for NFS, via nfsstat(8)).

——所以很多情况下你得**自己造**工具（这就是第 13、14、15 章那三把尺子的用武之地）。

---

## 16. §8.6–8.8 观测、实验与调优

**§8.6 观测工具**（Table 8.6）：传统的 `mount(8)`、`free(1)`、`top(1)`、`vmstat(8)`、`sar(1)`；BPF 系的 `opensnoop(8)`、`filetop(8)`、`cachestat(8)`、`ext4dist(8)`、`ext4slower(8)`；以及 `strace(8)`、`fatrace`、`latencytop` 和定制化的 `bpftrace`。

其中几个值得记住的用途：

- **`ext4dist`**：把 ext4 操作的延迟画成分布图；
- **`ext4slower`**：**只记录慢 I/O**——这是"全量事件追踪开销太大"时的实用折中；
- **`filetop`**：按 IOPS 和字节数排出最忙的文件；
- **`bpftrace`**：自己写。`t:syscalls:sys_enter_openat` 之类可以数调用、看内核栈。

**§8.7 实验**：

- **ad hoc**：`dd` 就能测顺序吞吐。但作者提醒，`dd` 测出来的 1.4 GB/s **只是把内存弄脏了**（写回缓存生效），真正落盘多少要看 `vm.dirty_*`。
- **微基准工具**：Bonnie、Bonnie++、iozone、tiobench、SysBench、fio、FileBench。作者的推荐是 **fio**，理由是两点：支持**非均匀随机分布**（`-random_distribution=pareto:0.9`，比均匀分布更接近真实访问），以及**报告延迟分位数**（99.00 / 99.50 / 99.90 / 99.95 / 99.99）。

fio 输出的分位数读法值得学：50 分位是 7 μs（几乎肯定是缓存命中），但 **99.99 分位是 63 ms**——平均值把这一切全藏起来了。

- **缓存清空**：`echo 3 > /proc/sys/vm/drop_caches` 把页缓存和 slab 对象（dentry、inode）一起丢掉，得到一个"冷缓存"的干净起点。

**§8.8 调优**：

- **应用侧调用**：同步写场景下用 `fsync(2)` 批量提交，比逐个 `O_DSYNC` 快；`posix_fadvise()` 给内核"这段是顺序的/这段马上要用/这段不会再用"的提示；`madvise(2)` 对映射区域做同样的事。内核可以用这些提示决定**什么时候预取、什么时候缓存**。
- **ext4**：`noatime` 关掉访问时间戳（省一次元数据写）、`relatime` 折中方案；`/sys/fs/ext4/features/` 下能看到特性支持状态（`batched_discard`、`casefold`、`encryption`、`journal` checksum 等）。
- **ZFS**：各条 `zfs get` 参数。

---

## 17. 实战锚点：`otf-server` 怎么面对文件系统这一章

先说结论：**这个仓库几乎不碰文件系统**。这不是"还没来得及做"，而是**有意的取舍**——所以这一章对它是**对照基准**，不是待办清单。

### 事实（grep 核对过）

- 全仓**没有** `fopen` / `ifstream` / `ofstream` / `fstream` / 路径形式的 `open()`。
- 唯一"往设备写"的地方：
  - `src/common/logger.cpp:100` —— `std::fprintf(stderr, "[%8lld ms] %-5s %.*s\n", ...)`；`src/common/logger.cpp:140` —— JSON 行格式的 `std::fprintf(stderr, "%s\n", line.c_str())`。**写的是 stderr，不是文件**（要落盘得靠 shell 重定向，那是部署的事，不是代码的事）。
  - `src/net/metrics_server.cpp:159` —— `::write(fd, resp.data() + off, resp.size() - off)`。**这是 socket，不是文件**。
  - `src/cli/cli_client.cpp:618` 同理（客户端的 socket 写）。
- `src/main.cpp:388` 的 `int main()` **没有命令行参数**；真正解析参数的是 `src/cli/cli_client.cpp:1176` 的 `int main(int argc, char** argv)`（`--port` / `--hash` / `--demo` 等）。
- `src/config/game_config.h` 与 `src/config/server_config.h` 里的参数**全是编译期常量**（`inline constexpr` / `const`）。仓库根的 `config/server.yaml` 存在，但**服务端不读它**——`server_config.h` 的文件头注释把这件事写得很明白："本仓库**没有 yaml 加载器**（零第三方依赖是硬约束）"，那份 yaml 是**给人看的运行期副本**，唯一真正生效的是头文件里的常量。

### 于是，本章的哪些机制在这里**根本不存在**

| 本章概念 | 在本仓库的对应物 |
|---|---|
| 文件系统缓存 / page cache / dentry / inode cache | **不存在**——没有文件被打开 |
| 逻辑 I/O vs 物理 I/O 的分叉 | **不存在**——没有中间层 |
| 预取 / read-ahead | **不存在** |
| 写回缓存的可靠性权衡 | **不存在**——没有"落盘"这个概念 |
| `mmap` vs `read()` 的拷贝账 | **存在，但是网络侧的**（见下） |
| 元数据密集负载（大量 `stat()`） | **不存在** |

### 那 §8.3.10 的"拷贝账"在哪儿？在 socket 那边

本章说 mmap 的一个收益是"避免双重拷贝"。这个仓库的**网络路径上**恰好有同一笔账，只是介质从磁盘换成了网卡：

- **收件侧**：`src/net/frame_codec.cpp` 的 `FrameDecoder::RingPush()` / `RingPopTo()` 用 `std::memcpy` 把字节从内核读到的 buffer 搬进环形缓冲 `ring_`，再从 `ring_` 搬进每帧的 `payload`——**两跳拷贝**。代码注释里明确记着这次重构的收益："一次 memcpy 取代逐字节循环 —— 这是本层最大的一笔 CPU 账"，实测 6556 ns/帧 → 约 100 ns/帧。
- **发件侧**：`src/net/tcp_transport.cpp:467-469`，`c.out.resize(old + n)` + `std::memcpy`——把待发数据追加进 `Conn::out`（这是 **socket 发送缓冲，不是文件缓冲**），再由 `FlushWrite()` 用 `::write()` 交出去。

**缺口要说清楚**：发件侧**没有环形缓冲**。`c.out` 每次追加都 `resize`，发完 `clear()`——它靠"resize 保留 capacity"来避免稳态重新分配，但**不是**环。

（推演）把 socket 发送缓冲类比成第 8 章的**写回缓存**：应用调 `Send()` 时数据只是进了内核的发送队列，还没上网；`FlushWrite()` 写不完才挂 `EPOLLOUT` 等可写事件。这套"先收下、稍后交付"的语义，和文件系统把写交给页缓存是同构的——**区别是 socket 这条路上没有掉电丢数据的问题**（进程一死就没了，不需要持久性保证）。

顺带一提，这也让**背压**变成了一个显式设计：发件缓冲有硬上限 `kSendBufCap = 128 * 1024`（`src/proto/protocol.h:224`），超过就**断开连接**并打警告，而不是无限缓冲。作者在代码注释里解释了为什么不能"拒收"：那会造成字节静默丢失、连接还活着、两端就此错位——**宁可掉连接，不可静默丢数据**。

### 顺带为 Ch7 排掉一个雷

因为本仓库**不碰磁盘**，内存分配与 **swap 这件事在这里几乎不会发生**：

- 全部状态在堆上（`std::vector` / `std::deque`），工作集**有界**（4 人一房、快照定长、发送缓冲 128 KB 封顶）；
- 没有任何"读一个 1 GB 文件做索引"这类操作，**没有让内存占用随时间增长的路径**；
- 没有 `mmap` 出去的文件页，也就没有**可回收的文件页**去挤占 swap。

所以 Ch7 讲的页缓存回收、swap 抖动、`kswapd`，在这个仓库里**基本是空集**。这不是"学了就用不上"——而是**正因为不碰磁盘，才让内存这件事变得可预测**。这条取舍的代价在别处：一旦将来要加持久化（日志落盘、快照存盘），第 8 章这一整套（写回 vs 同步写、fsync 批提交、noatime）就会**同时变成必修课**。

---

## 18. 收尾：本章金句清单

1. **"File system performance often matters more to the application than disk or storage device performance, because it is the file system that applications interact with and wait for."** —— 应用等的是文件系统，不是磁盘；这是全章的立论。
2. **"System performance analysis and monitoring tools have historically focused on disk performance, leaving file system performance as a blind spot."** —— 几十年来我们一直在量错的那一层。
3. **"I/O does not include open(2) and close(2) (although those calls update metadata and can cause indirect disk I/O)."** —— 定义要抠字眼：不是 I/O 的调用，照样能引起磁盘 I/O。
4. **"The principle is: If there is spare main memory, do something useful with it."** —— 缓存把内存吃掉不是故障，是设计。
5. **"Over time, the cache grows, while free memory for the operating system shrinks. This can alarm new users, but is perfectly normal."** —— 新用户的第一课。
6. **"The term fragmentation describes what happens file systems do this poorly, causing files to be scattered over a drive, so that sequential logical I/O yields random physical I/O."** —— 碎片化的真正伤害在物理层。
7. **"When prefetch detection works poorly, unnecessary I/O is issued that the application does not need, polluting the cache and consuming disk and I/O transport resources."** —— 预取是赌，赌输了要赔。
8. **"The trade-off is reliability."** —— 写回缓存四个字，代价全在这五个词里。
9. **"I've noticed a tendency to try using mmap(2) to solve file system performance issues without first analyzing them."** —— 先量，再改；这条纪律在第 9 章还会再考一次。
10. **"This can be confusing when analyzing file system operations (read(2), write(2)) and failing to find the source of the I/O (since it is triggered by instructions and not syscalls)."** —— mmap 的 I/O 没有系统调用痕迹，最难查。
11. **"So, while the application performed only a single 1-byte write, the disks performed multiple reads (128 Kbytes in total) and more writes (over 128 Kbytes)."** —— 开局那个算术，数字本身就是最狠的论据。
12. **"A common troubleshooting strategy has been to ignore the file system and focus on disk performance instead."** —— 只看磁盘，等于只看了你预期出问题的地方。
13. **"You can't tell much about a workload of "500 operations/s" from the rate alone."** —— 光有速率，等于没有信息。
14. **"Eliminating file system latency may improve performance by up to 10x."** —— 值得把文件系统当头号嫌疑犯来查。
15. **"This result is incredibly useful, as it can steer the performance investigation to the real source of latency."** —— "只占 1%" 这种结论，能省掉一整天的排查。
16. **"Characterizing the load applied is an important exercise when capacity planning, benchmarking, and simulating workloads."** —— 不知道负载，就不知道该优化什么。
17. **"It can also lead to some of the largest performance gains by identifying unnecessary work that can be eliminated."** —— 性能问题里最大的一类收益，来自**不做**某件事。
18. **"Some file system benchmark tools do not make clear what they are testing, and may imply a disk benchmark but use a small total file size, which returns entirely from cache and so does not test the disks."** —— 基准测试最大的自欺：测了个寂寞。
19. **"My personal recommendation is to use fio."** —— 工具推荐只有一句，因为它只需要一个。
