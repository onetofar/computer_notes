---
title: Chapter 7. Memory（精读）
category: 性能工程
tags:
  - 虚拟内存
  - 缺页
  - NUMA
  - TLB
  - 大页
  - 内存分配器
difficulty: 进阶
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）第7章 Memory"
source_section: "精读 §7.1 / §7.2.1 / §7.2.2 / §7.2.3 / §7.2.4 / §7.2.6–7.2.7 / §7.2.9 / §7.2.10 / §7.3.1 / §7.3.2 / §7.3.3 / §7.4.2 / §7.4.3 / §7.4.6 / §7.4.7 / §7.4.10 / §7.5.1–7.5.13 / §7.6.1–7.6.5；§7.2.5 Process Swapping、§7.2.8 Allocators、§7.2.11 Word Size 不单独立篇（历史背景、概念引子与地址空间约束，机制已在相邻节展开）；§7.4.1 / §7.4.4 / §7.4.5 / §7.4.8 / §7.4.9 与 §7.5.14 并入方法论与工具两节，不逐条立篇；§7.7 Exercises 与 §7.8 References 不立篇（练习题与参考文献是全书共用的自测/索引材料，不承载机制陈述）"
skills:
  - obsidian-markdown
---

# Chapter 7. Memory

> **小节地图**：本章是**内存这条线的本体章**。第 3 章从操作系统角度只留了虚拟内存的入口（[[Chapter 3. Operating Systems]]），本章把它展开成「概念 → 硬件与软件架构 → 方法论 → 工具 → 调参」五段。
> 立篇：§7.1（术语口径）、§7.2.1（虚拟内存与超额订阅上限）、§7.2.2（文件分页 vs 匿名分页）、§7.2.3（需求分页、缺页与 RSS/VSZ）、§7.2.4（overcommit）、§7.2.6–7.2.7（文件系统缓存、利用率与饱和）、§7.2.9（共享内存与 PSS）、§7.2.10（WSS）、§7.3.1（NUMA 局部性 / TLB / 大页）、§7.3.2（内核腾内存的顺序与 kswapd）、§7.3.3（进程地址空间、堆增长与分配器）、§7.4.2–7.4.3（USE 与使用量刻画）、§7.4.6（泄漏 vs 增长）、§7.4.7（静态调优清单）、§7.4.10（内存收缩负向实验）、§7.5.1–7.5.13（可观测工具）、§7.6.1–7.6.5（调优）。
> **不立篇**：§7.2.5（整进程换出，Linux 早已不做）、§7.2.8（分配器是概念引子，机制在 §7.3.3）、§7.2.11（字长与地址空间保留）、§7.7 Exercises、§7.8 References。
> 方法论底座见 [[Chapter 2. Methodologies]]；磁盘侧的页缓存与 I/O 见 [[Chapter 9. Disks]]；书级索引见 [[Systems Performance]]。
> ⚠️ 精简层不写实战锚点（那是讲义层的事）。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §7.1 术语口径 | 07-Chapter 7. Memory.md L91 | Swapping : Linux uses the term swapping to refer to anonymous paging to the swap device (the transfer of swap pages ). In Unix and other operating systems, swapping is the transfer of entire processes between main memory and the swap devices. This book uses the Linux version of the term. |
| §7.2.1 虚拟内存 | 07-Chapter 7. Memory.md L113 | The kernel may impose a limit to oversubscription. A commonly used limit is the size of main memory plus the physical swap devices. The kernel can fail allocations that try to exceed this limit. Such “out of virtual memory” errors can be confusing at first glance, since virtual memory itself is an abstract resource. |
| §7.2.2 匿名分页 | 07-Chapter 7. Memory.md L143 | This is an anonymous page-in , which introduces synchronous latency to the application. Anonymous page-outs may not affect application performance directly, as they can be performed asynchronously by the kernel. |
| §7.2.3 需求分页 | 07-Chapter 7. Memory.md L161 | If the mapping can be satisfied from another page in memory, it is called a minor fault . This may occur for mapping a new page from available memory, during memory growth of the process (as pictured). It can also occur for mapping to another existing page, such as reading a page from a mapped shared library. |
| §7.2.3 需求分页 | 07-Chapter 7. Memory.md L163 | Page faults that require storage device access (not shown in this figure), such as accessing an uncached memory-mapped file, are called major faults . |
| §7.2.3 需求分页 | 07-Chapter 7. Memory.md L179 | Resident set size (RSS): The size of allocated main memory pages (C) |
| §7.2.3 需求分页 | 07-Chapter 7. Memory.md L181 | Virtual memory size : The size of all allocated areas (B + C + D) |
| §7.2.7 利用率与饱和 | 07-Chapter 7. Memory.md L211 | Main memory utilization can be calculated as used memory versus total memory. Memory used by the file system cache can be treated as unused, as it is available for reuse by applications. |
| §7.2.9 共享内存 | 07-Chapter 7. Memory.md L229 | One technique in use by Linux is to provide an additional measure, the proportional set size (PSS), which includes private memory (not shared) plus shared memory divided by the number of users. |
| §7.2.10 工作集大小 | 07-Chapter 7. Memory.md L233 | Working set size (WSS) is the amount of main memory a process frequently uses to perform work. It is a useful concept for memory performance tuning: performance should greatly improve if the WSS can fit into the CPU caches, rather than main memory. Also, performance will greatly degrade if the WSS exceeds the main memory size, and the application must swap to perform work. |
| §7.3.1 硬件 | 07-Chapter 7. Memory.md L285 | CPU 1 can perform I/O to DRAM A directly, via its memory bus. This is referred to as local memory . CPU 1 performs I/O to DRAM B via CPU 2 and the CPU interconnect (two hops). This is referred to as remote memory and has a higher access latency. |
| §7.3.1 硬件 | 07-Chapter 7. Memory.md L447 | Because the TLB has a limited number of entries for mappings, the use of larger page sizes increases the range of memory that can be translated from its cache (its reach ), which reduces TLB misses and improves system performance. The TLB may be further divided into separate caches for each of these page sizes, improving the probability of retaining larger mappings in cache. |
| §7.3.2 软件 | 07-Chapter 7. Memory.md L511 | The Linux swappiness parameter controls whether to favor freeing memory by paging applications or by reclaiming it from the page cache. It is a number between 0 and 100 (the default value is 60), where higher values favor freeing memory by paging. Controlling the balance between these memory freeing techniques allows system throughput to be improved by preserving warm file system cache while paging out cold application memory [Corbet 04] . |
| §7.3.2 软件 | 07-Chapter 7. Memory.md L567 | Once free memory has reached the lowest threshold, kswapd runs in the foreground, synchronously freeing pages of memory as they are requested, a method sometimes known as direct-reclaim [Gorman 04] . This lowest threshold is tunable (vm.min_free_kbytes), and the others are scaled based on it (by 2x for low, 3x for high). |
| §7.3.3 进程地址空间 | 07-Chapter 7. Memory.md L603 | A common source of confusion is the endless growth of heap. Is it a memory leak? For simple allocators, a free(3) does not return memory to the operating system; rather, memory is kept to serve future allocations. This means that the process resident memory can only grow, which is normal. |
| §7.4.2 USE 方法 | 07-Chapter 7. Memory.md L793 | Physical memory utilization can be reported differently by different tools, depending on whether they account for unreferenced file system cache pages or inactive pages. A system may report that it has only 10 Mbytes of available memory when it actually has 10 Gbytes of file system cache that can be reclaimed by applications immediately when needed. Check the tool documentation to see what is included. |
| §7.4.6 泄漏检测 | 07-Chapter 7. Memory.md L885 | Memory growth issues are often misidentified as memory leaks. The first question to ask is: Is it supposed to do that? Check the memory usage, the configuration of your application, and the behavior of its allocators. An application may be configured to populate a memory cache, and the observed growth may be cache warmup. |
| §7.5.4 sar | 07-Chapter 7. Memory.md L1450 | The %vmeff metric is a useful measure of page reclaim efficiency. High means pages are successfully stolen from the inactive list (healthy); low means the system is struggling. The man page describes near 100% as high, and less than 30% as low. |
| §7.5.11 drsnoop | 07-Chapter 7. Memory.md L1819 | This output shows some direct reclaims for Java, taking between one and seven milliseconds. The rates of these reclaims and their duration in milliseconds ( LAT(ms) ) can be considered in quantifying the application impact. |
| §7.6.2 大页 | 07-Chapter 7. Memory.md L2385 | Large page sizes can improve memory I/O performance by improving the hit ratio of the TLB cache (increasing its reach). Most modern processors support multiple page sizes, such as a 4 Kbyte default and a 2 Mbyte large page. |

> **书里没说的**（本篇的推论，原文没有，别当原文观点）：
> - **「超配把失败点推迟到 OOM」这条因果链是本篇的连线**：原文 §7.2.4 只讲 overcommit 依赖需求分页、
>   §7.3.2 只讲无 swap 时「要么报 OOM 要么被 OOM killer 杀」（L515）、§7.6.1 才讲
>   `vm.overcommit_memory=2` 可以避免它引发 OOM；三处是分开陈述的。
> - **「先缺页栈、后 malloc 栈」是本篇从原文两处拼出来的执行顺序**：原文 §7.4.1 说 malloc 追踪
>   「can cost considerable overhead」、§7.5.13 说「I first use CPU profiling … or page fault tracing」，
>   但没有把两者写成一条有序流程。
> - **「RSS 含共享段所以求和会超物理内存」原文在 §7.5.7 才讲**（L1540），本篇把它提前到
>   §7.2.9 的口径讨论里，因为那是记账口径问题，不是工具问题。
> - **「缺页火焰图比 malloc 火焰图便宜一个数量级」是本篇的量化说法**：原文只说 malloc 追踪
>   「can slow down the target by a factor of two or more」，没有给出倍数级的成本对比。

## §7.1 术语口径：先把三个词钉死

术语表在原文 L69–L93。性能分析里最贵的错误往往不是「算错」，而是**把两件事当成一件**——
本章最容易出事的是这三个词：

- **Swapping**：Linux 口径 = **匿名分页**到 swap 设备，不是整进程换出。

> Swapping : Linux uses the term swapping to refer to anonymous paging to the swap device (the transfer of swap pages ). In Unix and other operating systems, swapping is the transfer of entire processes between main memory and the swap devices. This book uses the Linux version of the term.

- **Page fault**：原文 L87 定义为「一次无效的内存访问」，并明说在使用按需虚拟内存时
  **这是正常现象**，不是错误。
- **Swap**：磁盘上一块放匿名数据的区域——可以是物理 swap 设备，也可以是文件系统里的 swap 文件；
  原文顺带警告有些工具拿 "swap" 指虚拟内存，那是**混淆且不正确**的（原文 L93）。

**判据**：跨人沟通「内存涨了」时，先问三句——**是 RSS 还是 VSZ？是匿名页还是文件页？是缺页在涨还是在换页？**
这三个维度答错，后面所有分析都建在流沙上。

## §7.2.1 虚拟内存：抽象层，以及它什么时候会拒绝你

**机制**：虚拟内存给每个进程和内核一份**自己的、大而线性的、私有的地址空间**（原文 L103）。
它把「物理内存放哪儿」这件事整个甩给操作系统，同时支持多任务（地址空间天然隔离）与
**超额订阅**（用起来的内存可以超出主存）。

但内核**可以给超额订阅设上限**：

> The kernel may impose a limit to oversubscription. A commonly used limit is the size of main memory plus the physical swap devices. The kernel can fail allocations that try to exceed this limit. Such “out of virtual memory” errors can be confusing at first glance, since virtual memory itself is an abstract resource.

⇒ 常见上限是「主存 + 物理 swap 设备」之和；超过就让分配**失败**，报 **"out of virtual memory"**。
这类错误看着费解，是因为虚拟内存本身是抽象资源——**分配失败不代表机器真的一格物理内存都没有**。

**容易错在哪**：把 "out of virtual memory" 直接读成「机器内存不够」→ 去加内存。
它也可能是**虚拟地址空间上限被打满**（overcommit 被关成 2，或地址空间被内核保留区吃掉一块）。

## §7.2.2 两种分页：文件分页 vs 匿名分页

**机制**：页缓存出现之后，分页分成两类（原文 L131–L139）：

| 类型 | 触发源 | 换出代价 | 原文评价 |
|---|---|---|---|
| **文件分页** | 内存映射文件的读写（`mmap(2)`）、走页缓存的文件系统 | 脏页要写回磁盘；**干净页磁盘上已有副本，直接释放即可** | “good” paging |
| **匿名分页**（Linux 说的 swapping） | 进程私有数据：堆与栈 | **必须落到 swap 设备或 swap 文件** | “bad” paging |

关键的性能区分在这一句：

> This is an anonymous page-in , which introduces synchronous latency to the application. Anonymous page-outs may not affect application performance directly, as they can be performed asynchronously by the kernel.

⇒ **page-in 是同步的**：应用被阻塞在读回主存的磁盘 I/O 上，延迟直接进请求路径。
**page-out 是异步的**：内核后台做，通常不直接砸在应用脸上。

**判据**：延迟尖刺同时伴随磁盘读 I/O ⇒ 先查 **swap-in**；swap 写出在涨但应用不慢 ⇒
那是 page-out，属于「正在腾空间」的中间态，**优先级低于 swap-in**。

补充：原文 §7.2.5 明确 **Linux 根本不换出整进程，只靠分页**（L201）——
看到「swap out 一个进程」的说法，那是 Unix 老口径，不是 Linux 现实。

## §7.2.3 需求分页、缺页，与 RSS / VSZ 两个量

**机制**：需求分页把「建映射」的开销推迟到第一次访问（原文 L151）。缺页分两种：

> If the mapping can be satisfied from another page in memory, it is called a minor fault . This may occur for mapping a new page from available memory, during memory growth of the process (as pictured). It can also occur for mapping to another existing page, such as reading a page from a mapped shared library.

> Page faults that require storage device access (not shown in this figure), such as accessing an uncached memory-mapped file, are called major faults .

⇒ **minor fault**：内存里已有的页就能满足映射（进程内存增长、读共享库页）——**不碰磁盘**。
⇒ **major fault**：必须访问存储设备（访问未缓存的内存映射文件）——**要等磁盘**。

**四态模型**（原文 L167–L177）：未分配 / 已分配未映射 / 已映射到主存 / 已映射到 swap。
状态 (B)→(C) 是一次缺页；要不要磁盘 I/O 决定它是 major 还是 minor。由此得到两个量：

> Resident set size (RSS): The size of allocated main memory pages (C)

> Virtual memory size : The size of all allocated areas (B + C + D)

⇒ **RSS 只算真正占着主存的页**；**虚拟内存大小 = 所有已分配区域**（含还没碰过的、也含已换到 swap 上的）。

**判据**：RSS 涨 = **主存压力**；VSZ 涨但 RSS 不涨 = 可能是预分配、大量 `mmap` 保留、
或线程栈预留（每线程栈都算进 VSZ），**不一定有问题**。
反过来 RSS 涨而 VSZ 不涨 = 异常，去看 §7.3.3 的堆增长与分配器。

## §7.2.4 超额订阅（overcommit）

**机制**：允许分配的内存**超过系统实际存得下的量**（超过主存 + swap 总和）。它成立靠两件事：
需求分页已经建立，且**应用通常只稀疏使用自己申请到的内存**（原文 L187）。
参数在 §7.6.1：`vm.overcommit_memory`（0 = 启发式允许合理超配，1 = 总是超配，2 = 不超配）。

**推演（标注）**：把 §7.2.4、§7.3.2、§7.6.1 串起来读的效果是——
超配让 `malloc()` 在早期就成功，代价是**失败点被推迟到内核真的缺内存时**：
那时要么应用拿到 ENOMEM，要么 OOM killer 杀进程。所以**没有 swap 的环境里，OOM 是唯一的失败信号**
（原文 L515–L519）。

原文顺带给了一个现成的工程决策：**Netflix 云上实例通常不配 swap**
（L517）——宁可 OOM 被杀后把流量切到别的实例，也不要一个实例因为换页而拖慢。

## §7.2.6–7.2.7 利用率与饱和

**机制**：启动后内存占用一路涨是**正常的**——内核拿空闲内存做文件缓存，
「有空闲就拿它做有用的事」（原文 L205）。所以 free 接近 0 本身不是故障。

> Main memory utilization can be calculated as used memory versus total memory. Memory used by the file system cache can be treated as unused, as it is available for reuse by applications.

⇒ **算利用率时，页缓存算「未使用」**（它随时可回收）。

**饱和（saturation）**：内存需求超过主存 ⇒ 主存饱和。内核随后可能分页、（已不支持的）
进程换出、以及在 Linux 上动用 OOM killer——**这三件事本身就是饱和的指标**（原文 L213）。

**判据**：**「free 低」不是指标，「饱和动作正在发生」才是指标**。
看 free 就下结论，是本章最常见的误判。

## §7.2.9 共享内存与 PSS

**机制**：系统库这类只读指令文本在进程间共享一份。麻烦随之而来：
「这个进程占了多少主存」这个问题，共享段算谁的？

> One technique in use by Linux is to provide an additional measure, the proportional set size (PSS), which includes private memory (not shared) plus shared memory divided by the number of users.

⇒ **PSS = 私有内存 + 共享内存 ÷ 使用者数量**。共享部分摊到每个使用者头上，求和不会重复计数。

**判据**：统计「全机一共用了多少进程内存」时**用 PSS 不用 RSS**——RSS 含共享段，
逐进程求和会超过物理内存总量（原文 §7.5.7 ps 一节点出了这个坑，L1540）。

## §7.2.10 工作集大小（WSS）

> Working set size (WSS) is the amount of main memory a process frequently uses to perform work. It is a useful concept for memory performance tuning: performance should greatly improve if the WSS can fit into the CPU caches, rather than main memory. Also, performance will greatly degrade if the WSS exceeds the main memory size, and the application must swap to perform work.

⇒ 三档性能区间，一句话记住：

| WSS 落点 | 结果 |
|---|---|
| 装得进 **CPU 缓存** | 性能**大幅提升** |
| 装得进**主存** | 正常 |
| **超过主存** | 必须换页 ⇒ 性能**大幅劣化** |

**难处**：**没有现成的 WSS 统计量**——观测工具报的是 RSS，不是 WSS（原文 L235）。
原文给了两条路：§7.4.10 的**负向实验**（逐步压缩可用内存）和 §7.5.12 的 `wss(8)`
（用 PTE 的 accessed bit 采样估算）。

## §7.3.1 硬件：NUMA 局部性、TLB 与大页

**NUMA（原文 §7.3.1）**：双路机器上，本地内存和远端内存不是一回事：

> CPU 1 can perform I/O to DRAM A directly, via its memory bus. This is referred to as local memory . CPU 1 performs I/O to DRAM B via CPU 2 and the CPU interconnect (two hops). This is referred to as remote memory and has a higher access latency.

⇒ **local memory** = 挂在本 socket 上的内存，直连内存总线。
**remote memory** = 要经过另一个 CPU 和 CPU 互连，**多一跳，延迟更高**。
内核按**内存节点**组织分配，优先就近（§7.5.6 的 `numastat` 统计的就是这件事）。

**TLB**：MMU 的地址转换缓存，**条目数有限**：

> Because the TLB has a limited number of entries for mappings, the use of larger page sizes increases the range of memory that can be translated from its cache (its reach ), which reduces TLB misses and improves system performance. The TLB may be further divided into separate caches for each of these page sizes, improving the probability of retaining larger mappings in cache.

⇒ **大页的价值 = TLB 的 reach 变大 = TLB miss 变少**。这是 §7.6.2 配大页的**全部**理由，
不是「大页少用几次分配」这种直觉。

其余硬件要点（原文 §7.3.1，本篇不立篇）：DRAM 的 CAS 延迟（DDR4 约 10–20 ns）、
DDR 带宽表（Table 7.1，DDR-200 → DDR5-6400）、多通道（Intel Core i7 最高四通道 DDR3-1600，
51.2 GB/s）、CPU 缓存分级（L1 常按虚拟地址索引，L2 起按物理地址）。

**判据**：TLB 压力的特征是**内存访问延迟高、CPU 利用率却不高**（在等地址转换）。
这时候大页是成本最低的解法（§7.6.2）。

## §7.3.2 软件：内核腾内存的顺序

原文 Figure 7.6 给出**可用内存下降时的使用顺序**（原文 L499–L509），这五级顺序是本章最该背下来的：

| 顺序 | 手段 | 机制 |
|---|---|---|
| 1 | **Free list** | 空闲页链表（idle memory），通常按 NUMA locality group 分组多份 |
| 2 | **Page cache** | 文件系统缓存；`swappiness` 决定「先动页缓存还是先换应用」 |
| 3 | **Swapping** | kswapd 把不常用的页（含应用内存）换出，需要配了 swap 才有 |
| 4 | **Reaping**（shrinking） | 跨过低内存阈值时，让内核模块和 slab 分配器**立刻**交还容易释放的内存 |
| 5 | **OOM killer** | `select_bad_process()` 选牺牲者、`oom_kill_process()` 杀掉，系统日志里是 "Out of memory: Kill process" |

**swappiness 是这一级的旋钮**：

> The Linux swappiness parameter controls whether to favor freeing memory by paging applications or by reclaiming it from the page cache. It is a number between 0 and 100 (the default value is 60), where higher values favor freeing memory by paging. Controlling the balance between these memory freeing techniques allows system throughput to be improved by preserving warm file system cache while paging out cold application memory [Corbet 04] .

⇒ 高值 = 优先换应用、**保住温热的页缓存**；低值 = 优先回收页缓存。
原文那句因果很重要：**保留温页缓存、换出冷应用内存，可以提升系统吞吐**。

**kswapd 与直接回收**：

> Once free memory has reached the lowest threshold, kswapd runs in the foreground, synchronously freeing pages of memory as they are requested, a method sometimes known as direct-reclaim [Gorman 04] . This lowest threshold is tunable (vm.min_free_kbytes), and the others are scaled based on it (by 2x for low, 3x for high).

⇒ **后台 kswapd 是异步的；直接回收是同步的**——应用申请内存时被内核叫去扫页，
这就是「分配突然变慢」的机制来源。三个水位：最低（`vm.min_free_kbytes`）、低（2×）、高（3×），
**按倍数派生而不是各自独立**，就是为了制造滞后、避免回收抖动。

kswapd 扫描顺序：**先 inactive 列表，需要时再扫 active**（原文 L575）；
扫到的页若被锁定或脏，则不可回收。

**页分配器**：Linux 用 **buddy 分配器**（原文 L535），按 2 的幂维护多级空闲链表，
并为每个 CPU 保留单页列表以减少锁竞争。层级是 node → zone → migration type → size。

**判据**：内核指标里最有用的两条是 **`pgscand/s` 高**（应用在直接回收上阻塞）与
**`%vmeff` 低**（回收效率差，见 §7.5.4）。这两条比「free 还剩多少」有信息量得多。

## §7.3.3 进程虚拟地址空间与分配器

**地址空间分段**（原文 §7.3.3）：**可执行文本**（只读 + 可执行，来自二进制的 text 段）、
**可执行数据**（来自 data 段，可读写，且带私有标志，改动不回写磁盘）、**堆**（匿名内存，
由 `malloc(3)` 分配、按需增长）、**栈**（各线程自己的栈，可读写）。
库同样有自己的 text / data 段：**text 段可跨进程共享，data 段每个进程私有**。

**堆一直涨 ≠ 内存泄漏**：

> A common source of confusion is the endless growth of heap. Is it a memory leak? For simple allocators, a free(3) does not return memory to the operating system; rather, memory is kept to serve future allocations. This means that the process resident memory can only grow, which is normal.

⇒ 简单分配器的 `free(3)` **不把内存还给操作系统**，只留着给下一次分配用。
所以 **RSS 只增不减是正常现象**。

真正能还给系统的手段（原文 L605–L609）：**re-exec**（`execve(2)` 从空地址空间重来）、
**mmap/munmap**（`munmap(2)` 真的会还给系统）、以及 glibc 的 **`malloc_trim(3)`**
（堆顶空闲超过 `M_TRIM_THRESHOLD`，默认 128 KB 时由 `free(3)` 自动调用，
用 `sbrk(2)` 归还）（原文 L611）。

**分配器家族**（原文 §7.3.3）：

- **内核 slab**：按**固定大小的对象**做缓存，避免每次都做页分配；
  对内核里那些高频、固定尺寸的结构体特别有效。Solaris 2.4 起源（Bonwick 94），
  后来加了 per-CPU 的 magazine（Bonwick 01）。ZFS 的例子（照抄原文）：

```cpp
        df = kmem_alloc(sizeof (l2arc_data_free_t), KM_SLEEP);
        head = kmem_cache_alloc(hdr_cache, KM_PUSHPAGE);
```

- **SLUB**：Linux 默认（2.6.23 起），去掉对象队列、保留 per-CPU 缓存，
  **NUMA 优化交给页分配器**（原文 L663）。
- **glibc**：基于 dlmalloc；小对象走 bins（用类 buddy 的算法合并）、大对象走树查找、
  超大对象切到 `mmap(2)`（原文 L669）。
- **TCMalloc**：per-thread 缓存服务小对象分配，降低锁竞争；周期性 GC 把内存迁回中心堆。
- **jemalloc**：多 arena + per-thread 缓存 + 小对象 slab；Facebook 在用。

**判据**：怀疑「分配器拖累性能」时，先看**分配频率 × 线程数**——
多线程 + 高频小对象分配正是 TCMalloc / jemalloc 的目标场景；
最省事的验证手段是 `LD_PRELOAD` 换一个分配器跑一遍（§7.6.3）。

## §7.4.2–7.4.3 方法论：USE 与使用量刻画

**USE 方法**（§7.4.2）三个维度：**Utilization**（用了多少、还剩多少可用，物理与虚拟都要看）、
**Saturation**（为了缓解压力付出了多少代价：扫描、分页、换页、OOM 牺牲）、**Errors**（软硬件错误）。

原文建议**先查饱和**（L791）：持续饱和本身就是内存问题的信号。

**利用率的口径陷阱**：

> Physical memory utilization can be reported differently by different tools, depending on whether they account for unreferenced file system cache pages or inactive pages. A system may report that it has only 10 Mbytes of available memory when it actually has 10 Gbytes of file system cache that can be reclaimed by applications immediately when needed. Check the tool documentation to see what is included.

⇒ **「free 只有 10 MB」可能等于「有 10 GB 页缓存可以立刻回收」**。
**不同工具口径不同，先对齐口径再比较**——否则两个工具的数字没有可比性。

**使用量刻画**（§7.4.3）要覆盖五件事（原文 L805–L815）：系统级物理与虚拟内存利用率、
饱和程度（换页与 OOM）、内核与文件系统缓存用量、每进程物理与虚拟内存用量、
以及资源控制（限额）的用量。

原文给的示范句式值得照抄这种写法（L819）：「系统有 256 GB 主存，进程占用 1%，
文件系统缓存占 30%；最大的进程是数据库，RSS 2 GB，这是它从上一台机器迁移过来时带的上限。」
——**把「哪台进程、为什么是这个量」写进结论里**，而不是只报一个百分比。

**§7.4.7 静态调优清单**（原文 L895–L925）是同一套思路的另一种形态：主存总量、应用自己配的用量、
用的哪个分配器、主存是什么类型（DDR5 还是 DDR4）、架构是 NUMA 还是 UMA、OS 是否 NUMA-aware、
内存是插在同一 socket 还是跨 socket、有几条内存总线、CPU 缓存与 TLB 的数量和大小、
BIOS 设置、大页配了没、overcommit 开了没、有没有软件层的内存限额。

**§7.4.10 内存收缩（负向实验）**：逐步压缩应用的可用主存，同时量性能与换页量，
**性能急剧劣化 + 换页大幅上升的那个点，就是 WSS 装不下主存的位置**。
原文明确说这个方法**故意损害性能，不建议在生产用**。

## §7.4.6 泄漏检测：先分清「泄漏」和「增长」

> Memory growth issues are often misidentified as memory leaks. The first question to ask is: Is it supposed to do that? Check the memory usage, the configuration of your application, and the behavior of its allocators. An application may be configured to populate a memory cache, and the observed growth may be cache warmup.

⇒ 原文把这类问题分成两类（原文 L881–L883）：

| 类型 | 性质 | 修法 |
|---|---|---|
| **内存泄漏** | 软件 bug：内存不再使用却从未释放 | **改代码**，或打补丁 / 升级（补丁本身就是改代码） |
| **内存增长** | 软件正常地吃内存，只是速率高于期望 | **改配置**，或让开发者改用法 |

工具：BCC 的 `memleak(8)` 追踪分配、标出区间内没释放的以及它的调用路径——
但**它分不清是泄漏还是正常增长**，得自己看调用路径；且高分配率下开销很大（原文 L889）。

**判据**：看到 RSS 曲线单调上升，**第一个问题永远是「它本来就该这样吗」**，
而不是「泄漏在哪」。缓存预热、预分配池、连接池扩容、页缓存占满——**全属于后者**。

## §7.5 可观测性工具

按「系统级 → 每进程 → 分配追踪」三段：

| 工具 | 原文节 | 看什么 |
|---|---|---|
| `vmstat(8)` | §7.5.1 | `free`（可用内存）、`cache`（页缓存）、`si`/`so`（换入/换出速率）。**si/so 持续非零 = 内存压力**（L1077）。另有 `-a`（拆 active/inactive）与 `-S M`（换单位） |
| `/proc/pressure/memory`（PSI） | §7.5.2 | Linux 4.20 起。`some` 与 `full` 两条线的 avg10/avg60/avg300 是**任务被内存阻塞的时间百分比**；`some` = 有任务受影响，`full` = 全部可运行任务都受影响（L1107） |
| `swapon(1)` | §7.5.3 | 有没有配 swap、各用了多少（没配就不输出） |
| `sar -B/-H/-r/-S/-W` | §7.5.4 | 分页、大页、利用率、swap 空间、换页速率的**历史**统计（Table 7.5） |
| `slabtop(1)` | §7.5.5 | 内核 slab 缓存排行；数据来自 `/proc/slabinfo`，也可用 `vmstat -m` 打印 |
| `numastat(8)` | §7.5.6 | `numa_hit` / `numa_miss` / `other_node`；命中率低就调 NUMA 参数、拆负载，或换 NUMA 节点更少的机器 |
| `ps(1)` / `top(1)` | §7.5.7–7.5.8 | RSS / VSZ / %MEM；**RSS 含共享段，求和会超物理内存** |
| `pmap(1)` | §7.5.9 | 进程映射明细（虚拟大小 / RSS / 脏页 / 权限 / 映射对象）；`-X` / `-XX` 里带 **PSS** |
| `perf(1)` | §7.5.10 | `perf record -e page-faults -a -g` 抓缺页栈，再画**缺页火焰图**；另有 brk、NUMA 迁移、kmem/vmscan/compaction 计数 |
| `drsnoop(8)` | §7.5.11 | 直接回收的**延迟**与受影响进程 |
| `wss(8)` | §7.5.12 | 用 PTE accessed bit 估算 WSS（**实验性，带开销告警**） |
| `bpftrace` | §7.5.13 | 追踪 `malloc(3)` / `brk(2)` / 缺页 / vmscan / NUMA 迁移，做分配火焰图 |

### 两条「先便宜后昂贵」的纪律

1. **先缺页栈，后 malloc 栈。** `malloc()` 每秒可以调用上百万次，uprobe 追踪会把目标拖慢
   一倍以上；原文 §7.5.13 自己给的做法是「先用 CPU profiling 找到分配路径的粗位置，
   或者用页缺追踪」。页缺是低频事件，成本低一个量级。
2. **drsnoop 用的是低频 tracepoint，开销可忽略**；而 `wss(8)` 会**重置 PTE 的 referenced 标志**，
   在有换页的系统上可能干扰内核对「哪些页该回收」的判断，原文在 WARNINGS 里点名了
   10% 的额外延迟与大进程下超过一秒的停顿（L1852）。

缺页火焰图的生成步骤（照抄原文）：

```bash
# perf record -e page-faults -a -g -- sleep 60
# perf script --header > out.stacks
$ git clone https://github.com/brendangregg/FlameGraph; cd FlameGraph
$ ./stackcollapse-perf.pl < ../out.stacks | ./flamegraph.pl --hash \
    --bgcolor=green --count=pages --title="Page Fault Flame Graph" > out.svg
```

**判据**：**缺页火焰图先做，malloc 火焰图后做**；内存访问画像用 `perf mem record` /
`perf mem report`（原文 §7.5.10）。

## §7.6 调优

**最重要的一条**（原文 L2265）：**最有效的内存调优，是让应用待在主存里，
不频繁分页、不频繁换页**。其余都是边角。

**§7.6.1 可调参数**（Table 7.7，5.3 内核 + Ubuntu 19.10 默认值）：

| 参数 | 默认 | 说明 |
|---|---|---|
| `vm.min_free_kbytes` | 动态 | 期望保留的空闲内存。**调低可让给应用更多内存，但压力下内核更容易被迫提前 OOM；调高可减少 OOM**（L2375） |
| `vm.swappiness` | 60 | 换页 vs 回收页缓存的偏好（§7.3.2） |
| `vm.overcommit_memory` | 0 | 0 = 启发式允许合理超配，1 = 总是超配，2 = 不超配 |
| `vm.watermark_scale_factor` | 10 | kswapd 三水位之间的间距，单位是万分之一（10 = 0.1% 内存） |
| `vm.watermark_boost_factor` | 5000 | 内存碎片化时，kswapd 扫描可以超过高水位多少（5000 = 最多 150%） |
| `vm.dirty_ratio` / `vm.dirty_background_ratio` | 20 / 10 | 脏页回写阈值（触发写入方回写 / 触发后台回写） |
| `vm.dirty_expire_centisecs` / `vm.dirty_writeback_centisecs` | 3,000 / 500 | 脏页可回写前的最短时间 / pdflush 唤醒间隔 |
| `vm.vfs_cache_pressure` | 100 | 回收目录与 inode 缓存的力度；**0 = 永不回收，容易 OOM** |
| `vm.percpu_pagelist_fraction` | 0 | 限制 per-CPU 页列表最多能占多少页 |
| `kernel.numa_balancing` | 1 | 自动 NUMA 均衡。**过激的 NUMA 扫描很吃 CPU**——Netflix 在 3.13 附近曾关成 0（L2381） |
| `kernel.numa_balancing_scan_size_mb` | 256 | 每次 NUMA 均衡扫描多少 MB 的页 |

**两条命名/语义规则**：参数名自带单位（`bytes` / `ratio` / `centisecs` / `kbytes`）；
`dirty_background_bytes` 与 `dirty_background_ratio` **互斥**，`dirty_bytes` 与
`dirty_ratio` 同理（设一个就覆盖另一个）。

**§7.6.2 大页**：

> Large page sizes can improve memory I/O performance by improving the hit ratio of the TLB cache (increasing its reach). Most modern processors support multiple page sizes, such as a 4 Kbyte default and a 2 Mbyte large page.

配置（照抄原文）：

```bash
# echo 50 > /proc/sys/vm/nr_hugepages
# grep Huge /proc/meminfo
# mkdir /mnt/hugetlbfs
# mount -t hugetlbfs none /mnt/hugetlbfs -o pagesize=2048K
```

应用侧消费大页的四条路：共享内存段的 `SHM_HUGETLBS`（`shmget(2)`）、hugetlbfs 文件系统、
`mmap(2)` 的 `MAP_ANONYMOUS|MAP_HUGETLB`、libhugetlbfs API；
另有**透明大页（THP）**——内核自动把普通页升/降级，应用什么都不用做。

**§7.6.3 换分配器**：编译期选，或运行期用 `LD_PRELOAD`：

```bash
export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libtcmalloc_minimal.so.4
```

**§7.6.4 NUMA 绑定**：

```bash
# numactl --membind=0 3161
```

原文建议**同时用 `--physcpubind` 把 CPU 也限制在同一 socket**，避免 CPU 互连访问的惩罚
（L2437）；`numastat(8)` 用来列可用节点。

**§7.6.5 资源控制**：`ulimit(1)` 可设主存与虚拟内存上限；cgroup 内存子系统另外给
`memory.limit_in_bytes`（含文件缓存）、`memory.memsw.limit_in_bytes`（内存 + swap）、
`memory.kmem.limit_in_bytes`、`memory.tcp.limit_in_bytes`、`memory.swappiness`、
`memory.oom_control`（0 = 允许 OOM killer，1 = 禁用）。

⚠️ **容器里的陷阱**（原文 L519）：**系统可能内存充裕，但某个容器打满了自己的 cgroup 上限，
于是开始换页、甚至触发 OOM**。所以「宿主机 free 还有很多」**推不出**「这个容器不缺内存」。

## 章末选型判据

| 场景 | 怎么做 | 别怎么做 |
|---|---|---|
| 有人报「内存涨了」 | 先问三句：RSS 还是 VSZ、匿名还是文件页、缺页在涨还是换页在涨 | 上来就加内存；或先看 `free` |
| 看到 `free` 接近 0 | 先确认是不是页缓存（**算利用率时页缓存算未使用**），再看饱和动作有没有发生 | 把「free 低」当成故障 |
| 延迟尖刺伴随磁盘读 | 查 **swap-in**（同步阻塞在应用路径上） | 只看 swap 写出量；page-out 是异步的 |
| 统计全机进程内存 | 用 **PSS** | 用 RSS 逐进程求和（共享段被重复计数） |
| 判断是不是内存泄漏 | 第一个问题：**「它本来就该这样吗」**（缓存预热 / 预分配 / 连接池扩容） | 直接上 `memleak` 找「没释放的块」 |
| 应用申请内存变慢 | 看 `pgscand/s`（直接回收）与 `%vmeff`（回收效率），再看 `vm.min_free_kbytes` | 只看 free 内存 |
| 想保住热页缓存 | 调 `vm.swappiness`（高值偏向换应用、保页缓存） | 反向调，把温缓存换掉 |
| TLB 是瓶颈（延迟高但 CPU 不忙） | 配大页（`vm.nr_hugepages` / hugetlbfs / THP） | 以为是缓存不够，去加 CPU 缓存 |
| 想压低多线程分配开销 | 换 TCMalloc / jemalloc（`LD_PRELOAD`）先验证 | 直接改应用逻辑 |
| 容器里「明明内存够却在换页」 | 查该容器的 cgroup 上限（`memory.limit_in_bytes`） | 只看宿主机 free |
| 想测 WSS | 生产上用 `wss(8)` 或估算法；**负向实验（§7.4.10）只在实验室做** | 在生产上跑内存收缩实验 |
| 追「谁在吃内存」 | 先抓**缺页栈**画火焰图，再考虑 malloc 栈 | 一上来就 uprobe `malloc()`（目标会被拖慢一倍以上） |
| 调 `vm.min_free_kbytes` | 调高可减少 OOM；调低能给应用更多内存但会更早被迫 OOM | 当成「空闲内存越大越好」的旋钮乱调 |
