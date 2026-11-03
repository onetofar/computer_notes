---
title: Chapter 7. Memory（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - 虚拟内存
  - 按需分页
  - 内存分配器
  - 缺页与换页
  - slab与伙伴分配器
source: "《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg，Addison-Wesley 2020，英文原版）第 7 章"
companion: "[[07-Chapter 7. Memory]]"
---

# 第 7 章 讲义：内存不够时，系统会一层一层地牺牲你

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[07-Chapter 7. Memory|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：`src/room/room_mgr.h` 的 `std::map<std::uint32_t, Room> rooms_` 是**红黑树**——每个房间一次独立堆分配，房间销毁后内存还给分配器、**不还给操作系统**；`src/net/frame_codec.h` 的 `std::deque<Frame> queue_` 每帧一次分配；`src/net/metrics.h` 里 12 个原子计数器**没有一个是内存指标**。这一章讲的就是这些。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原章。

---

## 0. 开局：p99 从 2ms 涨到 800ms，CPU 和内存都没变

某台跑着 4 个进程的机器，线上报警说 p99 延迟从 2ms 劣化到 800ms。排查的人第一反应查 CPU：使用率 35%，没跑满。查内存：`free -h` 看着还挺充裕。于是卡住了——**两个最容易被查的资源都正常**。

最后是翻 `vmstat 1` 的输出发现的：

```bash
$ vmstat 1
procs -----------memory---------- ---swap-- -----io---- -system-- ----cpu----
 r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa
 4  0      0 34454064 111516 13438596   0   0    0    5    2    0  0  0 100  0
 4  0      0 34455208 111516 13438596   0   0    0    0 2262 15303 16 12 73  0
 5  0      0 34455588 111516 13438596   0   0    0    0 1961 15221 15 11 74  0
 4  0      0 34456300 111516 13438596   0   0    0    0 2343 15294 15 11 73  0
[...]

```

这一屏里，`si`（swap in）和 `so`（swap out）**不再是 0**。原文说得很直接：

> If the si and so columns are continually nonzero, the system is under memory pressure and is swapping to a swap device or file (see swapon(8)).

翻译成人话：**有进程的内存页被换到了磁盘上，又被换回来**。换回来那一下是同步的磁盘 I/O——线程就卡在那儿等。这就是 800ms 的来源。

本章要回答的问题因此很具体：**内存是怎么被"用掉"的、系统在内存紧张时按什么顺序牺牲什么、以及你怎么在它动手之前看见它。**

---

## 1. §7.1 Terminology：先把名词钉死

几个词的差别是**实质性的**，混淆它们会导致误诊。**Main memory（主存 / physical memory）**是机器上真正存数据的地方，通常是 DRAM；**Virtual memory** 则是主存的一个抽象，原文一句话点破：

> Virtual memory : An abstraction of main memory that is (almost) infinite and non-contended. Virtual memory is not real memory.

**Resident memory** 是当前**真的待在主存里**的内存；**Anonymous memory** 没有文件系统路径——进程的堆、工作数据。 **Page fault** 的定义值得逐字记住，因为它直接推翻了"page fault = 错误"的直觉：

> Page fault : An invalid memory access. These are normal occurrences when using on-demand virtual memory.

**缺页是按需分页的正常产物，不是 bug。** 一个进程启动时触发几千次 page fault 完全健康。 **Paging vs Swapping**：Linux 用 *swapping* 指"匿名页换出到 swap 设备"，Unix 传统上用 *swapping* 指"整个进程换出"。**本书采用 Linux 的定义**——本书的 `swapon`、`vmstat` 都按这个口径。**Swap** 是磁盘上放匿名页的区域；原文提醒有些工具拿 swap 指代虚拟内存，**这是混淆且错误的**。

---

## 2. §7.2.1 Virtual Memory：抽象，以及它的账单

> Virtual memory is an abstraction that provides each process and the kernel with its own large, linear, and private address space.

每个进程（以及内核）拿到自己的**大、线性、私有**地址空间。好处有三层：**软件不用管物理位置**；**多任务天然成立**（地址空间在设计上就分离）；**超卖**——在用的内存可以超出主存总量。原文用 Figure 7.1 画了带 swap 设备时进程虚拟内存的角色：一页内存可以在主存和 swap 之间被内核搬来搬去，这就是 Linux 说的 swapping——**这正是让超卖成立的关键机制**。 但抽象有边界：

> The kernel may impose a limit to oversubscription. A commonly used limit is the size of main memory plus the physical swap devices. The kernel can fail allocations that try to exceed this limit.

于是你会遇到一种很难 debug 的失败：程序报告 "out of virtual memory"，而虚拟内存本身是个抽象资源——**你从来没真的"用掉"过它**。Linux 还有另一条路：完全不设上限，这叫 **overcommit**（§7.2.4）。

---

## 3. §7.2.2 Paging：两种分页，一种叫"好"，一种叫"坏"

分页是把页在主存和存储设备之间搬。1962 年 Atlas Computer 首次实现，让三件事成为可能：部分加载的程序能跑、比主存大的程序能跑、程序在主存与磁盘间高效搬运。后来加上了共享文件系统页的 page cache，于是分成两支。 **文件系统分页（file system paging）**——由内存映射文件的读写引起，是 `mmap(2)` 应用的正常行为，被称为**"好"分页**。搬出去时有个细节：如果页被改过（**dirty**），page-out 要写盘；如果没改过（**clean**），page-out 只是释放内存，因为磁盘上已有一份。所以 *page-out* 只意味着"页被移出内存"，**不一定**意味着发生了写 I/O。

**匿名分页（anonymous paging，即 swapping）**——涉及进程私有的数据：堆和栈。它是"匿名"的，因为操作系统里没有对应的文件名。它被称为**"坏"分页**：

> Anonymous paging hurts performance and has therefore been referred to as “bad” paging [McDougall 06a].

更精确地说，坏的是 **page-in**：

> When applications access memory pages that have been paged out, they block on the disk I/O required to read them back to main memory. 1 This is an anonymous page-in , which introduces synchronous latency to the application.

**换出是异步的**（内核可以慢慢来），**换入是同步的**（应用线程就卡在那儿）。这就是第 0 节那个 800ms 的机制。原文的结论：

> Performance is best when there is no anonymous paging (swapping).

---

## 4. §7.2.3 Demand Paging：缺页的四态机

按需分页的时序，原文用 Figure 7.2 画得很清楚：

```
malloc() → store → MMU 查表失败(page fault) → 内核建映射 → ... 稍后可能 page-out 到 swap
```

第 2 步也可能是 load 指令（针对已映射文件：数据本该在里面，只是还没映射进本进程地址空间）。**缺页分两种，代价差几个数量级**：**minor fault（次缺页）**是映射能由内存里的另一页满足——从空闲内存映射新页（图中情形），或映射到已有页（比如读一个共享库的页）；**major fault（主缺页）**必须访问存储设备才能满足，比如访问一个未缓存的内存映射文件。于是任何一页虚拟内存只有四种状态：(A) 未分配、(B) 已分配未映射、(C) 已分配并映射到主存、(D) 已分配并映射到 swap 设备。**状态迁移 (B)→(C) 就是一次 page fault**；需要磁盘 I/O 的是 major，否则是 minor。两个使用率指标也由此定义：

> Resident set size (RSS): The size of allocated main memory pages (C)

> Virtual memory size : The size of all allocated areas (B + C + D)

**RSS 只是 (C)，VSZ 是 (B+C+D)。** 两者差距巨大却被经常混着报——这是很多"内存用了 40G"式误判的源头。

---

## 5. §7.2.4–7.2.7 超卖、进程换出、文件缓存、饱和度

**§7.2.4 Overcommit**：Linux 允许分配的内存**超过物理内存 + swap 的总和**。它靠两件事成立：按需分页，加上"应用通常不会真的用完它要的内存"这个经验事实。于是程序员可以放心地大方申请内存，用的时候再稀疏地碰。代价在 §7.3 讲——内核扛不住时的处置方式。 **§7.2.5 Process Swapping**：把**整个进程**在主存和 swap 之间搬，是早期 Unix 管理内存的老技术，swap 这个词就是从这儿来的。搬出时要写堆、打开的文件表和各种元数据；来自文件系统且未被修改的数据可以直接丢掉，需要时再读回来。原文说它"严重损害性能"，因为被换出的进程要重新跑起来需要大量磁盘 I/O——这在 PDP-11 那种最大进程才 64 KB 的年代是合理的。今天：

> This description is provided for historical background. Linux systems do not swap processes at all and rely only on paging.

**§7.2.6 File System Cache Usage**：这块是很多人第一次看内存监控时的困惑来源。

> It is normal for memory usage to grow after system boot as the operating system uses available memory to cache the file system, improving performance. The principle is: If there is spare main memory, use it for something useful.

开机后 `free` 慢慢掉到接近 0 是**正常且健康**的——内核拿空闲内存缓存文件系统了，而且能在应用需要时**迅速**收回来。所以"可用内存快没了"本身不是问题。 **§7.2.7 Utilization and Saturation**：两个词要分清。**利用率**是已用 / 总量，关键细节是——**文件缓存占的那部分可以算作"未使用"**，因为应用随时能回收。**饱和度**才是出问题的信号：需求超过主存，主存就饱和了，内核会开始 page out、换出，或者（Linux 上）**OOM killer**——**这些动作本身就是饱和的指标**。虚拟内存也能算"容量利用率"：若内核设了上限，虚拟内存耗尽时分配直接失败（`malloc(3)` 返回 `ENOMEM`）；Linux 默认不设限（overcommit），所以这条通常不适用。原文最后提醒：目前可用的虚拟内存有时被（令人困惑地）称作 **available swap**。

---

## 6. §7.2.8–7.2.11 分配器、共享内存、WSS、字长

**§7.2.8 Allocators**：虚拟内存负责在多任务间分配物理内存，而**虚拟地址空间内部的具体分配和放置**，是分配器干的活。它们要么是用户态库，要么是内核例程，给程序员 `malloc(3)` / `free(3)` 这种简单接口。

> Allocators can have a significant effect on performance, and a system may provide multiple user-level allocator libraries to pick from. They can improve performance by use of techniques including per-thread object caching, but they can also hurt performance if allocation becomes fragmented and wasteful.

**分配器能显著影响性能**：靠 per-thread 对象缓存可以变快，也可能因为碎片化而变慢。具体实现见 §7.3.3。 **§7.2.9 Shared Memory**：进程之间可以共享内存，典型用途是让所有用到某个库的进程共享同一份只读指令文本。这给"每个进程各报一份内存用量"的工具出了难题——**共享内存该不该算进进程的总量？** Linux 的做法是提供一个额外指标 **PSS**（proportional set size）：私有内存全额计入，共享内存按使用者数量**均摊**。

**§7.2.10 Working Set Size（WSS）** 是进程为了干活而频繁使用的主存大小。

> It is a useful concept for memory performance tuning: performance should greatly improve if the WSS can fit into the CPU caches, rather than main memory.

> Also, performance will greatly degrade if the WSS exceeds the main memory size, and the application must swap to perform work.

**WSS 能装进 CPU 缓存 → 性能大幅提升；WSS 超过主存 → 必须换页 → 性能大幅劣化。** 这是本章最有用的一个"目标函数"。但它难测：

> While useful as a concept, it is difficult to measure in practice: there is no WSS statistic in observability tools (they commonly report RSS, not WSS).

工具通常报 RSS，不是 WSS。§7.4.10 讲一种估算方法，§7.5.12 讲作者自己写的实验工具 `wss(8)`。 **§7.2.11 Word Size**：32 位程序的可寻址范围上限是 4 GB，需要更多内存的应用必须编成 64 位。另外地址空间有一部分要留给内核：32 位 Windows 默认给内核留 2 GB（应用只剩 2 GB），Linux 留 1 GB。64 位下这点预留不成问题。

---

## 7. §7.3.1 Hardware：DRAM、NUMA 和 TLB

**主存**是 DRAM——易失，每位只用**一个电容加一个晶体管**实现，电容需要周期刷新。服务器通常配 1 GB 到 1 TB+，云实例小一些（512 MB 到 256 GB），但云的设计是把负载摊到一批实例上，**总 DRAM 量更大，一致性代价也更高**。 **延迟**：主存访问时间用 **CAS latency**（column address strobe latency）衡量——从发出地址到数据可读之间的时间，DDR4 约 10–20 ns。

> The access time of main memory can be measured as the column address strobe (CAS) latency: the time between sending a memory module the desired address (column) and when the data is available to be read.

搬一条 cache line（比如 64 字节）过 64 位宽的内存总线，这个延迟可能**发生多次**；读指令若命中 CPU 缓存就绕开了它，写指令在支持 write-back 时也能绕开。 **架构：UMA 与 NUMA**。Figure 7.3 是双处理器 UMA——各 CPU 通过共享系统总线访问所有内存，访问延迟一致，由单个内核实例统一管理，也就是 SMP。Figure 7.4 是 NUMA——内存互连本身成为架构的一部分，访问时间**取决于内存相对于 CPU 的位置**：

> CPU 1 can perform I/O to DRAM A directly, via its memory bus. This is referred to as local memory . CPU 1 performs I/O to DRAM B via CPU 2 and the CPU interconnect (two hops). This is referred to as remote memory and has a higher access latency.

连在每个 CPU 上的内存 banks 叫 **memory node**。内核若知道拓扑，就能按**内存局部性**分配内存和调度线程。**DDR SDRAM 与多通道**：双倍数据率指在时钟的上升沿和下降沿都传数据；常见双/三/四通道，Intel Core i7 支持到四通道 DDR3-1600，峰值带宽 51.2 GB/s。 **MMU 与 TLB**。MMU 负责虚拟地址到物理地址的翻译，**按页进行，页内偏移直接映射**。TLB 是翻译查找缓冲，作为地址翻译的第一级缓存，后面才是主存里的页表：

> Because the TLB has a limited number of entries for mappings, the use of larger page sizes increases the range of memory that can be translated from its cache (its reach ), which reduces TLB misses and improves system performance.

**TLB 的"射程"（reach）** 是本章最容易被忽略的物理事实：条目数有限，页越大，能被缓存覆盖的地址范围越大，TLB miss 越少。Table 7.2 给了一个典型 Intel Core i7 的四组 TLB：指令 4K 是 64 per thread / 128 per core、指令大页 7 per thread、数据 4K 是 64、数据大页 32。**现代处理器支持多种页大小**（4 KB / 2 MB / 1 GB），Linux 的大页特性用的就是后者——这直接接上了 §7.6.2 的调优。

---

## 8. §7.3.2 Software：内核腾内存的五级台阶

这是本章的骨架。图 7.6 按**可用内存下降的顺序**给出内核释放内存的手段，一个比一个激进。**第一级 · free list** 是空闲页的链表（也叫 idle memory），可立即分配，通常按 NUMA 局部性分组实现成多个链表。 **第二级 · page cache**：文件系统缓存。

> Page cache : The file system cache. A tunable parameter called swappiness sets the degree to which the system should favor freeing memory from the page cache instead of swapping.

**第三级 · swapping**：

> Swapping : This is paging by the page-out daemon, kswapd, which finds not recently used pages to add to the free list, including application memory.

注意"**包括应用内存**"——这是唯一会动你数据的一级。 **第四级 · reaping**：

> Reaping : When a low-memory threshold is crossed, kernel modules and the kernel slab allocator can be instructed to immediately free any memory that can easily be freed. This is also known as shrinking .

**第五级 · OOM killer**：

> OOM killer : The out-of-memory killer will free memory by finding and killing a sacrificial process, found using select_bad_process() and then killed by calling oom_kill_process().

**找出一个牺牲品，杀掉它。** 到这一步，已经不是"性能问题"而是"可用性问题"了。这五级的配比由 **swappiness** 控制（0–100，默认 60，数值越高越偏向换出应用内存）：

> Controlling the balance between these memory freeing techniques allows system throughput to be improved by preserving warm file system cache while paging out cold application memory [Corbet 04] .

**没有 swap 会怎样？** 虚拟内存上限被压低，分配更早失败，OOM killer 更早登场。对一个"内存无限增长"的应用，有 swap 时它会先变成**性能问题**（给你现场调试的机会），没 swap 时只有两种结局：应用报 "Out of memory"，或者被 OOM killer 杀掉——**调试窗口被压掉了**。Netflix 云上的选择是**不配 swap**：

> In the Netflix cloud, instances typically do not use swap, so applications are OOM killed if they exhaust memory.

理由是流量可以立刻切到别的健康实例，**宁可让一个实例死掉，也不要让它因为换页而变慢**。用 cgroup 时同理：宿主机可能内存充裕，但某个容器的 cgroup 上限被打满，于是照样在换页或被 OOM。 **free list 的结构**：原始 Unix 用内存图 + first-fit 扫描；BSD 引入分页虚拟内存时加上了 free list 和 page-out daemon。free list 的规矩是——**释放的内存放链表头**（优先复用），page-out daemon 释放的放链表尾（里面可能还藏着有用的缓存页，将来有人要了可以就地回收）：

> Memory freed is added to the head of the list for future allocations.

Linux 用 **buddy allocator** 管理页，按 2 的幂分级多个空闲链表；"buddy"指找到相邻的空闲页合并分配。层级自上而下是：**node**（NUMA 感知的内存 banks）→ **zone**（DMA / normal / highmem 等用途区间）→ **migration type**（unmovable / reclaimable / movable）→ **size**（2 的幂次页数）。单页这种最常见的分配，buddy 会为每个 CPU 各留一份链表，以减少锁争用。 **page scanning**：free list 的可用内存跌破阈值时，kswapd 开始扫描 LRU 的 inactive 列表，需要时再扫 active 列表。

> When available main memory in the free list drops below a threshold, the page-out daemon begins page scanning . Page scanning occurs only when needed.

> The page cache has separate lists for inactive pages and active pages .

> kswapd scans the inactive list first, and then the active list, if needed.

跌破最低阈值时，kswapd 前台同步地释放页，这叫 **direct reclaim**，延迟可以从 `drsnoop` 看到（毫秒级）。最低阈值可调（`vm.min_free_kbytes`），另两个按它缩放（low 2 倍、high 3 倍）。

---

## 9. §7.3.3 Process Virtual Address Space：堆、段，和五种分配器

地址空间按 **segment** 划分：**Executable text**（CPU 指令，只读 + 可执行）、**Executable data**（已初始化变量，可读可写，带 private 标记，改动不刷盘）、**Heap**（程序的**工作内存**，匿名内存，靠 `malloc(3)` 分配，**按需增长**）、**Stack**（运行中线程的栈）。库之间可以共享 text 段，各自有私有的 data 段副本。 **堆只涨不缩，这是常态而不是泄漏**：

> A common source of confusion is the endless growth of heap. Is it a memory leak? For simple allocators, a free(3) does not return memory to the operating system; rather, memory is kept to serve future allocations. This means that the process resident memory can only grow, which is normal.

**关键那句**：简单的分配器下 `free(3)` **不把内存还给操作系统**，只是留着给下一次分配用——所以 RSS **只增不减**，这是正常的。要真正还给系统只有两条路：

> Re-exec : Calling execve(2) to begin from an empty address space

> Memory mapping : Using mmap(2) and munmap(2), which will return memory to the system

glibc 作为进阶分配器还提供 `malloc_trim(3)` 主动归还（free 时若堆顶空闲块超过 `M_TRIM_THRESHOLD`（默认 128 KB）会自动调用，用 `sbrk(2)` 归还）。 **五种分配器**（Figure 7.11）：

| 分配器 | 层次 | 关键机制 |
|---|---|---|
| **slab** | 内核 | 固定大小对象的缓存，避免页分配开销 |
| **SLUB** | 内核 | slab 的简化版，Linux 2.6.23 起的默认 |
| **glibc** | 用户态 | 基于 dlmalloc |
| **TCMalloc** | 用户态 | per-thread 缓存 |
| **jemalloc** | 用户态 | 多 arena + per-thread 缓存 |

**slab** 管理固定大小对象的缓存，能快速回收而没有页分配的开销——对频繁分配的定长结构体特别有效。原文举了 ZFS 的两行内核代码：

```c
        df = kmem_alloc(sizeof (l2arc_data_free_t), KM_SLEEP);
        head = kmem_cache_alloc(hdr_cache, KM_PUSHPAGE);
```

第一行是传统风格的分配（大小作为参数），内核据此映射到对应大小的 slab cache；第二行直接操作自定义的 slab cache。后来 slab 加上了 per-CPU 的 **magazine** 缓存——每个 CPU 一个 M 元素的缓存，攒够 M 个才需要"换弹匣"。 **SLUB** 简化了 slab 的复杂度，去掉了对象队列和 per-CPU 缓存，把 NUMA 优化交给页分配器。**glibc** 基于 Doug Lea 的 dlmalloc：行为**随请求大小而变**——小分配走 bins（同类大小的单元，用类 buddy 算法合并），大分配走树查找找空间，**超大分配切到 `mmap(2)`**。**TCMalloc** 是线程缓存式的 malloc，per-thread 缓存服务小分配以减少锁争用；定期垃圾回收把内存迁回中央堆。**jemalloc** 源自 FreeBSD，用多 arena、per-thread 缓存和小对象 slab 来提升可扩展性、减少碎片；取系统内存时偏好 `mmap(2)`。Facebook 用它并加了 profiling。

> It uses techniques such as multiple arenas, per-thread caching, and small object slabs to improve scalability and reduce memory fragmentation.

分配器的**四个评价维度**（原文列出）：简单 API、高效内存使用（合并碎片）、性能（多线程下分配器会因同步原语争用而变慢，所以要少加锁、用 per-thread 或 per-CPU 缓存）、可观测性（提供统计和调试模式，能看出分配发生在哪些代码路径）。

---

## 10. §7.4 Methodology：怎么查

原文给的方法表（Table 7.3）分三类：观测分析、容量规划、实验分析。作者建议的排查顺序：

> When troubleshooting memory issues, my suggestion is to start with the following strategies, in this order: performance monitoring, the USE method, and characterizing usage.

**§7.4.1 Tools Method**：挨个工具过一遍。内存场景看：page scanning（`sar -B` 的 pgscan 列，**持续超过 10 秒**就是内存压力信号）、PSI、swapping（`vmstat` 的 si/so）、OOM killer（系统日志或 `dmesg` 里搜 "Out of memory"）、`top`（谁在吃物理内存和虚拟内存）、以及用 perf/BCC/bpftrace **带栈追踪内存分配**（开销可观，CPU profiling 是更便宜的粗筛）。 **§7.4.2 USE Method**：Utilization / Saturation / Errors。

> You may want to check saturation first, as continual saturation is a sign of a memory issue.

**先查饱和度**——持续饱和就是内存问题的信号。一个常见的观测陷阱：

> A system may report that it has only 10 Mbytes of available memory when it actually has 10 Gbytes of file system cache that can be reclaimed by applications immediately when needed.

所以不同工具报的"可用内存"口径不同（算不算未引用的文件缓存页、inactive 页），**必须看工具文档**。Errors 一侧：软件错误（分配失败、OOM killer）和硬件错误（ECC）都要看——

> These correctable errors can be used as a USE method error metric, and can be a sign that uncorrectable errors may soon occur.

不可纠正的内存错误表现为**无法复现的随机崩溃**（段错误、bus error），极难诊断。 **§7.4.3 Characterizing Usage**：回答"哪里用了多少"。原文给了一个范例描述句式：

> The system has 256 Gbytes of main memory, which has 1% in use (utilized) by processes and 30% in the file system cache. The largest process is a database, consuming 2 Gbytes of main memory (RSS), which is its configured limit from the previous system it was migrated from.

**注意最后半句**——这个 2 GB 不是用出来的，是**从上一台机器迁移时继承的配置**。这类"配置错了"是最大的性能收益来源之一：

> For example, a database cache may be configured too small and have low hit rates, or too large and cause system paging.

高级检查清单里几个关键的：WSS 是多少？内核内存用在哪些 slab？文件缓存里 active 与 inactive 各占多少？**进程为什么在分配内存（调用路径）？内核为什么在分配内存（调用路径）？** 有没有内存泄漏？NUMA 上内存分布均匀吗？IPC 和内存停滞周期是多少？本地 I/O 与远程 I/O 各占多少？ **§7.4.4 Cycle Analysis**：从 **IPC** 入手——它反映 CPU 负载有多依赖内存。**§7.4.5 Performance Monitoring**：关键指标是利用率（可由可用内存反推）和饱和度（换出、OOM）；**按进程监控内存随时间的变化，能发现内存泄漏及其速率**。

**§7.4.6 Leak Detection**：两种病——**memory leak**（内存不再使用却从不释放的软件 bug，改代码修）和 **memory growth**（软件正常消耗内存，但速率高得对系统不利，改配置修）。

> Memory growth issues are often misidentified as memory leaks.

**第一个该问的问题是：它本来就该这样吗？** 查内存用量、应用配置、分配器行为——**一个应用可能只是被配置成要预热某个缓存，你看到的"增长"是缓存预热**。Linux BCC 的 `memleak(8)` 能追踪分配并记录未释放的那些及其调用路径，但它**分不清泄漏和正常增长**，还是得自己分析调用路径。 **§7.4.7 Static Performance Tuning**：查静态配置——主存总量、应用被配置成用多少、用哪个分配器、主存是什么类型（DDR5？）、内存有没有被完整测试过（`memtester`）、架构是 NUMA 还是 UMA、内核 NUMA-aware 吗、大页配了吗、overcommit 怎么配的、有没有软件级的内存限制。

**§7.4.10 Memory Shrinking**：一种估算 WSS 的**反向实验**——逐步减少可用主存，同时测性能和换页量：

> This is a working set size (WSS) estimation method that uses a negative experiment, requiring swap devices to be configured to perform the experiment. Available main memory for an application is progressively reduced while measuring performance and swapping: the point where performance sharply degrades and swapping greatly increases shows when the WSS no longer fits into the available memory.

**性能急剧劣化、换页大幅增加的那个点，就是 WSS 装不下可用内存的时刻。** 原文明确说这个方法**不建议用于生产**（它故意伤害性能）。

---

## 11. §7.5 Observability Tools：够用的几个

**§7.5.1 vmstat**——第一屏已经在第 0 节见过了。要看的列：`swpd`（已换出的内存量）、`free`（空闲可用内存）、`buff`（buffer cache）、`cache`（page cache）、`si`（换入）、`so`（换出）。内存大的机器上列会错位，可改单位：`vmstat -Sm 1`（`m`=1000000，`M`=1048576），还有 `-a` 打印 page cache 的 active/inactive 拆分。 **§7.5.2 PSI（Pressure Stall Information）**——Linux 4.20 引入，是**内存饱和度的统计**：`# cat /proc/pressure/memory` 输出 `some avg10=2.84 avg60=1.23 avg300=0.32 total=1468344` 与 `full avg10=1.85 avg60=0.66 avg300=0.16 total=702578`。

> This output shows that memory pressure is increasing, with a higher 10-second average (2.84) than the 300-second average (0.32).

这些平均值是**任务处于内存停滞状态的时间百分比**。`some` 行表示"部分任务受影响"，`full` 行表示"**所有可运行任务都受影响**"。**10 秒均值 > 300 秒均值 = 压力正在上升**——这是单个 `vmstat` 快照给不出的信息。PSI 也按 cgroup2 统计。 **§7.5.3 swapon / §7.5.4 sar**——`swapon` 显示配了几个 swap 设备、用了多少；**很多现代系统根本不配 swap，这时它什么都不输出**。`sar -B` 提供历史统计（page scanning 等）。

**§7.5.5 slabtop**——内核 slab cache 的实时统计（数据来自 `/proc/slabinfo`，也可用 `vmstat -m` 打印）：`OBJS` 是对象数、`ACTIVE` 是活跃数、`USE` 是使用率、`OBJ SIZE` 是对象大小、`CACHE SIZE` 是整个 cache 的总大小。按 `-sc` 可以按 cache size 排序，最大的排最前。 **§7.5.6 numastat**——NUMA 系统的分配统计：

> numa_hit : Memory allocations on the intended NUMA node.

> numa_miss + numa_foreign : Memory allocations not on the preferred NUMA node.

命中率高说明 NUMA 分配策略工作良好；命中率低就该考虑调 sysctl 的 NUMA 参数、拆分负载或换更少 NUMA 节点的机器。 **§7.5.7 ps / §7.5.8 top / §7.5.9 pmap**——`ps` 的关键列是 `%MEM`（RSS 占全系统百分比）、`RSS`（驻留集大小，KB）、`VSZ`（虚拟内存大小，KB）。原文提醒：

> While RSS shows main memory usage, it includes shared memory segments such as system libraries, which may be mapped by dozens of processes. If you were to sum the RSS column, you might find that it exceeds the memory available in the system, due to overcounting of this shared memory.

**把 RSS 列加起来可能超过机器总内存**——因为共享内存被重复计了。**要看共享内存的构成就用 `pmap`**，它能显示 PSS。 **§7.5.10 perf：页错误采样与火焰图**——这是定位"内存为什么涨"的利器：`perf record -e page-faults -a -g -- sleep 60` 加 `perf script`。栈追踪能直接告诉你**哪条代码路径在触发页错误**，也就是哪条路径在推高 RSS。原文的分析结论：

> The Figure 7.12 flame graph shows that more than half of the memory growth in MySQL server was from the JOIN::optimize() code path (left large tower). A mouse-over of JOIN::optimize() shows that it and its child calls were responsible for 3,226 page faults; with 4 Kbyte pages, this amounts to around 12 Mbytes of main memory growth.

生成火焰图时注意 `--count=pages` 和绿色背景——**这是内存火焰图，不是 CPU 火焰图**。其他有用的 one-liners：`perf record -e syscalls:sys_enter_brk -a -g`（brk 追踪堆增长）、`perf stat -e 'kmem:*' -a -I 1000`、`perf record -e vmscan:mm_vmscan_wakeup_kswapd -ag`（kswapd 唤醒）。 **§7.5.11 drsnoop / §7.5.12 wss / §7.5.13 bpftrace**——**drsnoop** 追踪 direct reclaim，显示被影响的进程和**回收耗时**（毫秒），可用来量化内存受限系统对应用性能的影响。**wss** 是作者自己写的实验工具，用页表项的 "accessed" 位估算工作集：重置所有页的 accessed 位，等一段时间，再看哪些被置上了；因为是页粒度，分辨率就是页大小（通常 4 KB），数字应视为**向上取整到页**。**bpftrace** 则能直接追 `malloc(3)` 的调用栈和请求大小（`uprobe:...libc.so.6:malloc` + `@[ustack] = hist(arg0)`），但有明确警告：

> WARNING: user-level allocation requests can be a frequent activity, occurring many millions of times per second. While the instrumentation cost is small, when multiplied by a high rate, it can lead to significant CPU overhead while tracing, slowing down the target by a factor of two or more—use sparingly. Because it is low-cost, I first use CPU profiling of stack traces to get a handle on allocation paths, or page fault tracing shown in the next section.

**先便宜的，后昂贵的**——先做 CPU 采样 profiling 或页错误追踪，再上 malloc 追踪。

---

## 12. §7.6 Tuning：能调的旋钮

原文第一句就点明了最重要的那条：

> The most important memory tuning you can do is to ensure that applications remain in main memory, and that paging and swapping do not occur frequently.

**保证应用待在主存里**，别频繁换页。剩下的都是细活。 **§7.6.1 Tunable Parameters**——Table 7.7 列了一批（5.3 内核，Ubuntu 19.10 默认值）。挑关键的：

| 参数 | 默认 | 说明 |
|---|---|---|
| `vm.swappiness` | 60 | 偏向换出应用内存（而非回收 page cache）的程度 |
| `vm.overcommit_memory` | 0 | 0=启发式合理超卖；1=总是超卖；2=不超卖 |
| `vm.min_free_kbytes` | dynamic | 期望保留的空闲内存；调小能腾内存，但可能更早触发 OOM |
| `vm.watermark_scale_factor` | 10 | kswapd 三级水位线的间距 |
| `vm.watermark_boost_factor` | 5000 | 内存碎片时允许 kswapd 扫到 high 水位线的 150% |
| `kernel.numa_balancing` | 1 | 自动 NUMA 页平衡 |

`vm.min_free_kbytes` 是按主存的**比例动态**算的（算法不线性）。调大能避免 OOM，调小能腾内存但**压力大时内核更容易被压垮**。`vm.overcommit_memory` 设成 2 可以禁掉超卖，从源头避免"超卖到 OOM"。`vm.swappiness` 如果让应用内存过早被换出，性能会显著受影响；有时设成 0 是合意的（**尽量保住应用内存，代价是牺牲 page cache**），内存真不够时内核仍会换页。 **§7.6.2 Multiple Page Sizes**——

> Large page sizes can improve memory I/O performance by improving the hit ratio of the TLB cache (increasing its reach).

**大页的价值来自 TLB 射程**，不是来自"少几次缺页"。配置方式是先用 `echo 50 > /proc/sys/vm/nr_hugepages` 预留大页（用 `grep Huge /proc/meminfo` 确认 HugePages_Total / HugePages_Free / Hugepagesize），再让应用去用：`shmget(2)` 的 `SHM_HUGETLBS` 标志、`mmap(2)` 的 `MAP_ANONYMOUS|MAP_HUGETLB`，或挂 hugetlbfs 文件系统（`mount -t hugetlbfs none /mnt/hugetlbfs -o pagesize=2048K`）。还有**透明大页（THP）**：

> transparent huge pages (THP) is another mechanism that uses huge pages by automatically promoting and demoting normal pages to huge, without an application needing to specify huge pages [Corbet 11] .

应用什么都不用改，内核自动升降级。原文脚注提到 THP 历史上曾有性能问题而劝退用户，如今这些问题"应该已经修好了"。 **§7.6.3 Allocators / §7.6.4 NUMA Binding**——换分配器可以编译期选，也可以运行期用 `LD_PRELOAD`（如 `export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libtcmalloc_minimal.so.4`）。`numactl` 则把进程绑到 NUMA 节点：`# numactl --membind=0 3161`。绑到 node 0 后，**这个进程后续的内存分配如果无法从 node 0 满足就会失败**。原文建议搭配 `--physcpubind`，把进程限制在单个 socket 上，避免 CPU 互连的访问代价。

**§7.6.5 Resource Controls**——`ulimit(1)` 能设主存和虚拟内存上限；Linux 上更常用的是 cgroup 内存子系统：`memory.limit_in_bytes`（用户内存上限，含 file cache）、`memory.memsw.limit_in_bytes`（内存 + swap 上限）、`memory.kmem.limit_in_bytes`（内核内存上限）、`memory.tcp.limit_in_bytes`（TCP 缓冲上限）、`memory.swappiness`（cgroup 级）、`memory.oom_control`（设为 1 可禁用该 cgroup 的 OOM killer）。

---

## 13. 实战锚点：otf-server 怎么面对这件事

先说结论：**这个仓库在"内存"这件事上几乎什么都没做，而它做的唯一一件事（复用成员缓冲）恰好踩中了本章 §7.3.3 讲的核心机制。**

### 13.1 房间表：红黑树 + 值语义 Room = 一次大块堆分配

```cpp
// src/room/room_mgr.h:119
std::map<std::uint32_t, Room> rooms_;
```

`Room` 内联持有一个完整的 `world::World world_`（`src/room/room.h:125`），而 World 里装着 4 个 `Player`、每个玩家的输入缓冲、命中队列等。**这意味着每一次 `rooms_.emplace(room_id, Room(room_id))`（`src/room/room_mgr.cpp:114`）都是一次 sizeof(Room) 级别的堆分配。** 对照本章的分配器知识：**这种尺寸的分配落在 glibc 的 bins 里；更关键的是 `free` 不把内存还给操作系统**。房间销毁时 `rooms_.erase(id)`（`src/room/room_mgr.cpp:186`）把内存还给 glibc，但 glibc **留着不还给 OS**。原文那句在这里字面成立：

> For simple allocators, a free(3) does not return memory to the operating system; rather, memory is kept to serve future allocations.

于是**RSS 只增不减**。房间反复创建销毁，进程 RSS 会呈阶梯状往上爬——**这不是泄漏，是分配器的正常行为**，但它确实是你在 `top` 里看到的那条曲线。红黑树还额外多一层：每个节点一次独立分配、节点离散，遍历是指针跳跃（第 6 章的缓存内容）。 **能怎么改**：`room_id` 单调递增且永不复用（`src/room/room_mgr.h:124` 的 `next_room_id_`，`room_mgr.cpp:113` 递增），所以完全可以改成 `std::vector<Room>` + id 直接作下标——和 `src/session/session_mgr.h:206` 的 `std::vector<Session> sessions_`、`src/net/tcp_transport.h:133` 的 `std::vector<Conn> conns_` **完全同构**。上限是 `kMaxRooms = 32`（`src/proto/protocol.h:171`），数组完全装得下。（推演）32 个 `Room` 连续排布 vs 32 个离散堆块，前者的 WSS 更容易整体装进 CPU 缓存——按 §7.2.10 的说法，性能会因此改善。

### 13.2 做对的那一件事：复用成员缓冲

```cpp
// src/room/room_mgr.h:131-132
std::vector<std::uint8_t> snapshot_scratch_;
std::vector<std::uint8_t> event_scratch_;
```

`src/room/room_mgr.cpp:260` 的注释把理由写清楚了：

> ★ 复用成员缓冲，而不是每轮 new 一个局部 vector。

这个"每轮"到底有多贵，量化写在成员声明上方的注释里（`src/room/room_mgr.h:130`）：

> 32 间房 × 10Hz = 每秒 320 次纯浪费的堆分配。

**这正是本章 §7.3.3 讲的那个机制的正面用法**：`std::vector` 的 `capacity` 被保留，`clear()` 只改大小不改容量，于是**稳态下这块内存永远不还给分配器，也永远不再分配**。相比每轮 new 一个局部 vector：分配 → 用 → 销毁 → `free` → glibc 留着不还 —— 分配器的元数据开销、碎片化、缓存局部性全在白付钱。**这是本仓库目前唯一一处"分配器层面"的优化**，而它恰好避开了本章最核心的那个陷阱。

### 13.3 帧队列：每帧一次的分配，以及一个不可避免的分配

```cpp
// src/net/frame_codec.h:174
std::vector<uint8_t> ring_;
// src/net/frame_codec.h:177
std::deque<Frame> queue_;
```

接收侧的 `ring_` 是**预分配环形缓冲**（讲义对它用途的概括，仓库源码这一行本身没有注释），这是对的——符合 §7.2.3 的"按需分配"精神，也避免了每帧 resize 带来的缺页。但 `std::deque<Frame> queue_` 里每个 `Frame` 各自持有 payload，于是 `src/net/frame_codec.cpp:118` 的 `dst.resize(n)` 每帧可能分配一次。代码注释自己承认了（`src/net/frame_codec.cpp:115-117`）：

> ★ 同样一次 memcpy 取代逐字节循环（理由同 RingPush）。
>   dst.resize(n) 在 n>0 且容量不足时分配一次 —— 这是排队语义 unavoidable 的
>   代价（帧必须独立于环才能排队），但**拷贝方式**可以选：memcpy，不是逐字节。

**这个判断是对的**：队列语义下每帧必须有独立所有权，分配无法避免——除非上**对象池**。而对象池正是 §7.3.3 那几个分配器（TCMalloc 的 per-thread 缓存、jemalloc 的 small object slab）在做的事：**把"分配"变成"回收"**。这是本章没做、但可以补的一项。

### 13.4 本章的关键缺口：这个仓库**看不到自己的内存**

`src/net/metrics.h` 里有 12 个原子计数器：

```cpp
// src/net/metrics.h:136-139
std::atomic<std::int64_t> conns_{0}, rooms_{0}, sent_{0}, recv_{0};
std::atomic<std::int64_t> hb_timeout_{0}, oversize_{0};
std::atomic<std::int64_t> ctl_add_{0}, ctl_del_{0}, ctl_mod_{0}, ctl_add_out_{0};
std::atomic<std::int64_t> tick_drift_{0}, snap_ms_{0};
```

**没有一个是内存指标。** 对照本章的方法论，这是一个完整的观测盲区：

| 本章要求 | 仓库现状 |
|---|---|
| §7.4.5 监控内存使用随时间变化，发现泄漏 | ❌ 无 RSS / 无堆用量指标 |
| §7.2.10 WSS 概念 | ❌ 无任何相关指标 |
| §7.4.3 定位内存用在哪（哪个 slab / 哪段地址空间） | ❌ 无 |
| §7.5.5 slabtop 看内核 slab | ❌ 无（但可以上系统工具看） |

**后果是具体的**：§7.4.6 说的"memory growth 常被误认成 leak"这类误判，在这里**连判断的原料都没有**——运维看到进程 RSS 涨了，既不知道是泄漏还是正常增长，也不知道涨在哪。**最少成本的第一步**：在 `metrics.h` 里加一个进程 RSS 的采样（读 `/proc/self/statm` 即可，几乎零成本），按 §7.4.5 的思路**按时间记录**画成曲线。有了这条曲线，"§7.4.6 的第一个问题——它本来就该这样吗？"才有答案。

### 13.5 诚实对照：本章讲的东西，这个仓库**一项都没做**

| 本章的能力 | otf-server 现状 |
|---|---|
| 对象池 / arena / 内存池 | ❌ 无 |
| 自定义分配器（换 jemalloc / TCMalloc） | ❌ 无，用默认 glibc |
| 大页 / THP 显式配置 | ❌ 无 |
| 内存指标的观测 | ❌ 无 |
| 按进程分析内存用量（pmap / PSS 视角） | ❌ 无 |
| 单线程事件循环（IO 与逻辑未拆） | ⚠️ 单线程，所以**分配器锁争用暂时不存在**——但拆线程后 §7.2.8 说的"多线程下分配器因同步原语争用而变慢"立刻成立 |

最后一行值得展开：`src/app/reap_guard.h` 的 RAII 守卫（`= delete` 禁拷贝）和 `src/config/game_config.h` 的 `inline constexpr` 编译期常量，都是**"让问题在结构上不可能发生"**的思路——编译期常量根本不参与运行时分配。**这个仓库在"结构性预防"上的品味很好，只是这套思路还没延伸到内存分配这一层。** （推演）若按本章的顺序做，价值排序大概是：① 加 RSS 指标（有了度量才有其余）→ ② `rooms_` 换 `vector`（消除 32 次堆分配 + 指针跳跃 + 碎片）→ ③ 帧队列上对象池 → ④ 拆线程时同步上 per-thread 缓存的分配器（jemalloc）。

---

## 14. 收尾：本章金句清单

> 说明：以下英文均为原书原句逐字摘录。原书 epub 抽取保留了原排版的空格习惯（逗号/括号前的多余空格），引用时予以保留。

1. **Virtual memory : An abstraction of main memory that is (almost) infinite and non-contended. Virtual memory is not real memory.** —— 一句话戳破虚拟内存的幻觉：你手上的地址空间不是内存。
2. **Page fault : An invalid memory access. These are normal occurrences when using on-demand virtual memory.** —— 缺页不是错误，是按需分页的正常产物。
3. **Performance is best when there is no anonymous paging (swapping).** —— 性能目标就一句话：别换页。
4. **Anonymous paging hurts performance and has therefore been referred to as “bad” paging [McDougall 06a].** —— "坏"的不是换出，是换入。
5. **This is an anonymous page-in , which introduces synchronous latency to the application.** —— 换入是同步的：应用线程就卡在磁盘 I/O 上，这是 p99 炸掉的直接机制。
6. **Resident set size (RSS): The size of allocated main memory pages (C)** —— RSS 只算真正在主存里的那部分，别拿它当"我占了多少"。
7. **Virtual memory size : The size of all allocated areas (B + C + D)** —— VSZ 含已分配未映射的部分，所以 RSS 和 VSZ 差一大截是正常的。
8. **The principle is: If there is spare main memory, use it for something useful.** —— 开机后可用内存掉到近零是内核在拿它缓存文件，不是 bug。
9. **A system may report that it has only 10 Mbytes of available memory when it actually has 10 Gbytes of file system cache that can be reclaimed by applications immediately when needed.** —— 换台机器看"可用内存"数字可能差三个数量级，看工具口径。
10. **For simple allocators, a free(3) does not return memory to the operating system; rather, memory is kept to serve future allocations. This means that the process resident memory can only grow, which is normal.** —— 堆只涨不缩是常态，不是泄漏——排查内存问题的第一个问题应该是"它本来就该这样吗"。
11. **Memory growth issues are often misidentified as memory leaks.** —— 泄漏和正常增长的区别不在现象，在意图。
12. **It is a useful concept for memory performance tuning: performance should greatly improve if the WSS can fit into the CPU caches, rather than main memory.** —— WSS 是本章最值得盯的目标：装进 CPU 缓存，性能就赢了一大半。
13. **Because the TLB has a limited number of entries for mappings, the use of larger page sizes increases the range of memory that can be translated from its cache (its reach ), which reduces TLB misses and improves system performance.** —— 大页的真正价值在 TLB 射程，不在"少几次缺页"。
14. **CPU 1 can perform I/O to DRAM A directly, via its memory bus. This is referred to as local memory .** —— NUMA 的"本地"和"远程"是两次跳转的差别，性能差在这里。
15. **Controlling the balance between these memory freeing techniques allows system throughput to be improved by preserving warm file system cache while paging out cold application memory [Corbet 04] .** —— swappiness 调的不是开关，是"牺牲谁"的配比。
16. **The kernel slab allocator manages caches of objects of a specific size, allowing them to be recycled quickly without the overhead of page allocation.** —— slab 的本质是"固定大小对象的缓存"，对象池就是它的用户态版本。
17. **Memory allocations can be frequent, and on multithreaded environments they can perform poorly due to contention for synchronization primitives.** —— 单线程时分配器不疼；一旦拆线程，这句话立刻开始计息。
18. **The most important memory tuning you can do is to ensure that applications remain in main memory, and that paging and swapping do not occur frequently.** —— 内存调优的第一名永远是"别让应用被换出去"，其余都是细活。
