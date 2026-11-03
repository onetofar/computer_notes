---
title: Chapter 9. Disks（精读）
category: 性能工程
tags:
  - 磁盘性能
  - IOPS
  - 利用率
  - 饱和
  - USE 方法
  - Linux I/O 栈
difficulty: 进阶
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）第9章 Disks"
source_section: "精读 §9.1 / §9.2 / §9.3.1–9.3.13 / §9.4.1–9.4.4 / §9.5.1–9.5.10 / §9.6 / §9.7 / §9.8 / §9.9；§9.10 Exercises 与 §9.11 References 不立篇（练习题与参考文献是全书共用的自测/索引材料，不承载机制陈述）"
skills:
  - obsidian-markdown
---

# Chapter 9. Disks

> **小节地图**：本章按「术语 → 模型 → 概念 → 架构 → 方法论 → 工具 → 可视化 → 实验 → 调优」九段展开，全部立篇。
> §9.3 是全章的概念核心（时间口径、工作负载四属性、利用率/饱和/I/O Wait），单独拆成多节讲。
> §9.4.1 下的 `###### Seek and Rotation` / `###### Flash Memory` / `###### Controller` 等是**无编号子标题**，按原文文字引用，不编 § 号。
> §9.6 的工具小节（iostat/sar/PSI/pidstat/perf/biolatency/biosnoop/biotop/blktrace/bpftrace/MegaCli/smartctl）并入 §9.6 一节讲，只点名不逐条立篇。
> 底层依赖：队列论与时间尺度见第2章；文件系统的逻辑/物理 I/O 见第8章；CPU 侧的调度器与 perf 见第6章、第13章。
> 本篇的实践面：第12章（基准测试）、第13章（perf）、第14章（Ftrace）、第15章（BPF）、附录 A/B/C 分别是本章工具的展开。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §9.1 术语 | 09-Chapter 9. Disks.md L89 | Be aware that networking uses the term latency to refer to the time needed to initiate an I/O, followed by data transfer time. |
| §9.2 模型 | 09-Chapter 9. Disks.md L121 | The on-disk cache may also be used to improve write performance, by using it as a write-back cache. This signals writes as having completed after the data transfer to cache and before the slower transfer to persistent disk storage. The counter-term is the write-through cache, which completes writes only after the full transfer to the next level. |
| §9.3.1 时间口径 | 09-Chapter 9. Disks.md L161 | Block I/O wait time (also called OS wait time ) is the time spent from when a new I/O was created and inserted into a kernel I/O queue to when it left the final kernel queue and was issued to the disk device. This may span multiple kernel-level queues, including a block I/O layer queue and a disk device queue. |
| §9.3.1 时间口径 | 09-Chapter 9. Disks.md L173 | Disk request time (also called disk response time and disk I/O latency ) is both the disk wait time and disk service time, and is equal to the block I/O service time. |
| §9.3.2 时间尺度 | 09-Chapter 9. Disks.md L257 | Since a disk will return a mixture of these, expressing them together as an average latency (as iostat(1) does) can be misleading, as this is really a distribution with two modes. |
| §9.3.3 缓存 | 09-Chapter 9. Disks.md L261 | The best disk I/O performance is none at all. |
| §9.3.7 IOPS 不可比 | 09-Chapter 9. Disks.md L325 | Because of those last three characteristics, IOPS are not created equal and cannot be directly compared between different devices and workloads. An IOPS value on its own doesn’t mean a lot. |
| §9.3.7 IOPS 不可比 | 09-Chapter 9. Disks.md L331 | To make sense of IOPS, include the other details: random or sequential, I/O size, read/write, buffered/direct, and number of I/O in parallel. Also consider using time-based metrics, such as utilization and service time, which reflect resulting performance and can be more easily compared. |
| §9.3.9 利用率 | 09-Chapter 9. Disks.md L345 | Disks at 100% utilization are a likely source of performance issues, especially if they remain at 100% for some time. However, any rate of disk utilization can contribute to poor performance, as disk I/O is typically a slow activity. |
| §9.3.10 饱和 | 09-Chapter 9. Disks.md L373 | You might assume that disks at less than 100% utilization have no saturation, but this actually depends on the utilization interval: 50% disk utilization during an interval may mean 100% utilized for half that time and idle for the rest. Any interval summary can suffer from similar issues. When it is important to know exactly what occurred, tracing tools can be used to examine I/O events. |
| §9.3.11 I/O Wait | 09-Chapter 9. Disks.md L379 | If another CPU-hungry process comes along, the I/O wait value can drop: the CPUs now have something to do, instead of being idle. However, the same disk I/O is still present and blocking threads, despite the drop in the I/O wait metric. |
| §9.3.12 异步 I/O | 09-Chapter 9. Disks.md L387 | It can be important to understand that disk I/O latency may not directly affect application performance, if the application I/O and disk I/O operate asynchronously. |
| §9.4.1 机械盘 | 09-Chapter 9. Disks.md L429 | Slow I/O for magnetic rotational disks is usually caused by the seek time for the disk heads and the rotation time of the disk platter, both of which may take milliseconds. |
| §9.4.1 SSD | 09-Chapter 9. Disks.md L547 | Translating between input and output is performed by the controller’s flash translation layer (FTL), which must also track free blocks. It essentially uses its own file system to do this, such as a log-structured file system. |
| §9.4.1 SSD | 09-Chapter 9. Disks.md L549 | This can cause write amplification , where the remainder of the block is copied elsewhere before erasure, and also latency for at least the erase-write cycle. |
| §9.4.3 存储类型 | 09-Chapter 9. Disks.md L635 | However, advances in processors have produced CPUs with a surplus of cycles and cores, reducing the need to offload parity calculations. |
| §9.4.4 OS I/O 栈 | 09-Chapter 9. Disks.md L773 | The multi-queue driver (blk-mq, added in Linux 3.13) solves this by using separate submission queues for each CPU, and multiple dispatch queues for the devices. This delivers better performance and lower latency for I/O versus classic schedulers, as requests can be processed in parallel and on the same CPU where the I/O was initiated. |
| §9.5.1 USE 方法 | 09-Chapter 9. Disks.md L891 | The USE method is for identifying bottlenecks and errors across all components, early in a performance investigation. |
| §9.5.5 延迟分析 | 09-Chapter 9. Disks.md L1023 | If this matches the I/O latency at the application level, it’s usually safe to assume that the I/O latency originates from the disks, allowing you to focus your investigation on them. If the latency differs, measuring it at different levels of the operating system stack will identify the origin. |
| §9.5.10 扩展 | 09-Chapter 9. Disks.md L1147 | Do not use the maximum throughput and IOPS values per disk, as this would result in driving disks at 100% utilization, leading to immediate performance issues due to saturation and queueing. Pick a target utilization (say, 50%) and scale values accordingly. |

> **书里没说的**（本篇的推论，原文没有，别当原文观点）：
> - 「延迟这个词在磁盘语境下有至少四套口径」这个**归纳**是本篇做的：原文 L89/L161/L173/L181 分别给出
>   networking 口径、kernel 口径（wait/service/request）、disk 口径（wait/service/request）三套，
>   原文并没有把它们并列成一张「同名不同义」的表。
> - 「§9.4.2 的接口演进史可以压成一条主线：**共享总线 → 点对点 → 直连 PCIe**」是本篇的串法，
>   原文是 SCSI/SAS/SATA/FC/NVMe 五节平行叙述，没有给出这条主线。
> - 「§9.7 的五种可视化其实是在回答同一个问题的五种切面（这次 I/O 长什么样）」是本篇的归并说法；
>   原文五节互不引用。
> - 本篇把 §9.5.1「Tools 方法」当作「USE 方法的退化形态」来对比，是**本篇的判断**：
>   原文 L871 确实说 tools method 「can overlook issues for which the tools provide poor or no visibility」，
>   但没有把两者排成优劣序。

## §9.1 术语

这一节是全章的**口径表**，作用是消灭「延迟」这个词的歧义。原文给的定义里最容易混淆的一组是
**latency（磁盘语境）** 与 **latency（网络语境）**：

> Be aware that networking uses the term latency to refer to the time needed to initiate an I/O, followed by data transfer time.

也就是说：网络里的 latency = **发起 I/O 的时间 + 数据传输时间**（不含排队）；
磁盘里的 latency 往往指**整段在设备上的时间**（含设备内部排队）。
同一份 `iostat` 数字，和一份网络 RTT，放进同一张对照表会直接对不上。

另外两个必须分开的量（原文 L85/L87）：

| 量 | 含义 | 是否受硬件上限约束 |
|---|---|---|
| **Throughput** | 当前**实际**数据传输率（bytes/s） | 否，随负载浮动 |
| **Bandwidth** | 传输/控制器能达到的**最大**数据传输率 | 是，由硬件上限决定 |

**容易错在哪**：把「带宽」当成「当前吞吐」写进报告，等于把上限当成实测值。
原文对 Bandwidth 的定义是 "the maximum possible data transfer rate for storage transports or controllers"，
对 Throughput 是 "the current data transfer rate"——一个天花板，一个读数。

## §9.2 模型：队列、缓存、控制器

三个简化模型，是后面所有分析的心智脚手架。

### 简单磁盘：一个队列 + 一个服务台

原文把磁盘建模成**排队论**里的「客户在收银台前排队」：I/O 要么在设备内排队，要么正在被服务。
关键是——**这个队列不一定是 FIFO**。原文明确说设备控制器可以用别的算法：

> the on-disk controller can apply other algorithms to optimize performance

（电梯寻道、读写分队列等，见 §9.4.1。）**这是推论（推演）**：这正是 §9.5.2 里「饱和」会脱离 FIFO
直觉的原因——一个 LIFO 或非公平队列，在高利用率下的排队延迟曲线与 M/D/1 不一样。

### 缓存磁盘：加一层 DRAM，延迟分布变成**双峰**

> The on-disk cache may also be used to improve write performance, by using it as a write-back cache. This signals writes as having completed after the data transfer to cache and before the slower transfer to persistent disk storage. The counter-term is the write-through cache, which completes writes only after the full transfer to the next level.

两个术语必须钉死：

| 缓存模式 | 写入「完成」的判定点 | 代价 |
|---|---|---|
| **write-back**（回写） | 数据进缓存即返回 | 掉电可能丢数据 ⇒ 生产上必须**配电池** |
| **write-through**（直写） | 数据落到下层持久存储才返回 | 慢，但掉电安全 |

原文紧接着说：storage 的回写缓存**通常配电池**，电池在盘上或在控制器上。
这就是 §9.4.3 里「电池失效 ⇒ 切 write-through ⇒ 写性能突然塌陷」的机制来源。

**关键推论**：加了缓存之后，一次 I/O 的延迟不再是「一个数」，而是**两个模式的混合**——
命中是几十微秒，未命中是几毫秒。**这正是 §9.3.2 里「平均延迟会骗人」的根因。**

### 控制器：CPU 总线 ↔ 存储总线之间的桥

磁盘控制器（原文叫 host bus adapters, HBA）在 CPU I/O 传输与存储传输之间架桥。
性能可能被**四样东西**中的任意一个卡住：CPU 侧总线、控制器、存储侧总线、磁盘本身。

**容易错在哪**：默认「磁盘慢 ⇒ 是盘的问题」。但 §9.5.2 给了一条很好的判据：
如果**总吞吐在各种负载下都卡在同一个数**，那更可能是控制器或传输总线到顶了，而不是盘。

## §9.3 概念

### §9.3.1 时间口径：同一件事有六种叫法

原文把 I/O 时间拆成三层、每层两三个名字。**这是全章最容易被工具输出误导的地方**——
`iostat` 的 `r_await`、`aqu-sz`、`%util` 对应的是哪一层的量，取决于版本和字段。

**内核侧**（Block I/O 层）：

> Block I/O wait time (also called OS wait time ) is the time spent from when a new I/O was created and inserted into a kernel I/O queue to when it left the final kernel queue and was issued to the disk device. This may span multiple kernel-level queues, including a block I/O layer queue and a disk device queue.

- **Block I/O wait time（OS wait time）**：从创建 I/O 进内核队列，到**离开最后一个内核队列**、发往设备为止。
  注意它**可能横跨多个内核队列**（块层队列 + 设备队列）。
- **Block I/O service time**：从把请求发给设备，到设备的完成中断。
- **Block I/O request time** = wait + service = **从创建到完成的全程**。

**磁盘侧**（设备内部）：

> Disk request time (also called disk response time and disk I/O latency ) is both the disk wait time and disk service time, and is equal to the block I/O service time.

于是有一个关键等式：

```
block I/O service time  ==  disk request time
```

**推论**：磁盘的「响应时间」和内核的「服务时间」是**同一个数的两个名字**；
而内核的「等待时间」是磁盘侧看不见的那一段（设备内部的 on-disk queue 等待算在 disk wait time 里，
但 disk wait time 又是 block I/O service time 的一部分）。

原文还提醒：**block I/O service time 不完全等于「磁盘性能」**——块设备接口之下的驱动层
（原文说三层驱动）各自可能有队列或阻塞在互斥锁上，这部分延迟也计在 block I/O service time 里。

### §9.3.1 的推算式：utilization/IOPS，以及它为什么常常不准

磁盘服务时间通常拿不到内核统计，只能推算：

```
disk service time = utilization / IOPS
```

**但原文紧接着给了限定条件**：公式假设利用率反映的是**单个设备（服务台）**，
它同一时刻只能处理一个 I/O；而**盘通常可以并行处理多个 I/O**（多个队列、多条通道），
所以这个推算常常不准——分母被低估、推算值偏高。

> **判据**：看到 `utilization/IOPS` 推出的服务时间明显偏大，先怀疑「并行处理」而不是「盘真的慢」。
> 要准确的 disk service time，只能上事件追踪（issue/completion 高精度时间戳）。

### §9.3.2 时间尺度：微秒到毫秒，跨三个数量级

> Since a disk will return a mixture of these, expressing them together as an average latency (as iostat(1) does) can be misleading, as this is really a distribution with two modes.

原文的 Table 9.1 把磁盘延迟排成阶梯（on-disk cache 命中 < 100 μs → 闪存读 ~100 μs–1 ms →
机械盘顺序读 ~1 ms → 机械盘随机读 ~8 ms → 排队后 > 10 ms → 最差虚拟盘 > 1,000 ms）。

**三个可直接用的判据**：

1. **一个慢 I/O 就能毁掉一次响应**：慢端（> 100 ms 那一档）单个就能造成用户可感知的卡顿。
2. **快端要靠数量堆**：快速 I/O 只有在**数量极大**时才成为问题（很多个快 I/O 加起来等于一个慢 I/O）。
3. **平均值是双峰分布的塌缩**：`iostat` 报的 `r_await` 是平均值，而真实分布有两个峰
   （缓存命中 / 未命中），所以平均值既不像命中也不像未命中，**两边都不像**。

原文还给了一个行业口径差异：企业存储环境里 > 10 ms 就算异常；云环境容忍度高得多，
可能要到 50 ms 才成为问题。**判据（推演）**：阈值不是常数，是**环境契约**的一部分，
报告里必须同时给出「延迟分布」和「该环境的可接受阈值」。

### §9.3.3 缓存：磁盘这一层的缓存清单

> The best disk I/O performance is none at all.

原文明说：软件栈的每一层都在试图**避免**磁盘 I/O——缓存读、缓冲写，一路一直到盘本身。
设备驱动层及以下（原文 Table 9.2）的缓存清单：

| 缓存 | 例子 |
|---|---|
| Device cache | ZFS vdev |
| Block cache | Buffer cache |
| Disk controller cache | RAID 卡缓存 |
| Storage array cache | 阵列缓存 |
| On-disk cache | 磁盘数据控制器（DDC）外挂的 DRAM |

**判据**：这五层**各有各的调优手段**（见 §9.5.7 与 §9.9）。看到「磁盘慢」时，
先问「这次 I/O 是被哪一层放过的」——很多「磁盘问题」其实是缓存没命中或没配好。

### §9.3.7 IOPS 不可比：一个数字不带上下文就没有意义

> Because of those last three characteristics, IOPS are not created equal and cannot be directly compared between different devices and workloads. An IOPS value on its own doesn’t mean a lot.

原文点出的三个「另三个特性」是 §9.3.4/9.3.5/9.3.6 的**随机 vs 顺序**、**读写比**、**I/O 大小**。

> To make sense of IOPS, include the other details: random or sequential, I/O size, read/write, buffered/direct, and number of I/O in parallel. Also consider using time-based metrics, such as utilization and service time, which reflect resulting performance and can be more easily compared.

**IOPS 的完整上下文清单**（本篇按原文这句整理成表）：

| 维度 | 为什么必须带上 |
|---|---|
| random / sequential | 机械盘上随机 IOPS 可能比顺序 IOPS 差一个数量级 |
| I/O size | SSD 的 IOPS 与 I/O 大小强相关（4 KB 读 / 1 MB 写是常见最优点） |
| read / write | SSD 读写不对称；写还要看是否 buffered |
| buffered / direct | 命中页缓存的 IOPS 测的是内存不是磁盘 |
| 并发 I/O 数 | 并发度直接改变排队与延迟 |
| 额外维度（原文同章给出） | utilization、service time 这类**时间型**指标更容易跨设备比较 |

> **判据**：看到一份只有「IOPS: 50,000」的对比结论，**默认它不可信**。
> 要么补齐上表，要么换成 utilization / service time 这类时间型指标。

### §9.3.8 非数据传输命令：不是 I/O，但会占着盘

> Another example command is used to discard data: the ATA TRIM command, or SCSI UNMAP command. This tells the drive that a sector range is no longer needed, and can help SSD drives maintain write performance.

除了读写，磁盘还接受**非数据传输命令**：刷缓存（flush）、TRIM/UNMAP（丢弃）、
巡检读（patrol read）、一致性检查。
其中 TRIM（ATA）/ UNMAP（SCSI）告诉驱动器「某段扇区已不再需要」，可帮助 SSD 维持写性能。

**机制要点**：这类命令**不是数据 I/O**，但**会占用设备**。原文 §9.3.8 明说：
这些命令可以在别的 I/O 排队时让盘处于「busy」状态。

> **判据**：`%util` 高但**没有对应的读写 IOPS** ⇒ 大概率是非数据传输命令（刷缓存 / 巡检 / 重建）。
> 硬件 RAID 卡的 patrol read 与 cache flush interval 就是两个常见的这类元凶（§9.4.3）。

### §9.3.9 利用率：不是越低越好，也不是只有 100% 才算问题

> Disks at 100% utilization are a likely source of performance issues, especially if they remain at 100% for some time. However, any rate of disk utilization can contribute to poor performance, as disk I/O is typically a slow activity.

**两层结论**：
1. 100% 持续 ⇒ 几乎必然是问题源；
2. **但任何利用率都可能造成问题**——因为磁盘 I/O 本身就是个慢操作，50% 也可能让应用等。

**虚拟盘利用率与物理盘利用率会严重背离**（§9.3.9 的 "Virtual Disk Utilization"）：
- 虚拟盘 100% busy，但它由多块物理盘组成 ⇒ 可能还有物理盘闲着；
- 带 write-back 缓存的虚拟盘，写负载下**看起来不忙**（控制器立即返回完成），而物理盘还在忙；
- 硬件 RAID 重建会让盘忙，但**操作系统看不到对应的 I/O**。

**判据**：分析虚拟盘时，**必须同时看物理盘的利用率**（§9.5.2 明确说操作系统应当也暴露物理盘的利用率）。

### §9.3.10 饱和：超过 100% 之后的那一段

饱和度量的是「**超出设备交付能力之外排队的工作**」，对磁盘来说是**操作系统里设备等待队列的平均长度**。

> You might assume that disks at less than 100% utilization have no saturation, but this actually depends on the utilization interval: 50% disk utilization during an interval may mean 100% utilized for half that time and idle for the rest.

**区间摘要的通病**（这一条同样适用于 %util、IOPS、平均延迟）：
50% 的区间利用率可能是「半段时间打满 + 半段全闲」。想看真相，只能上追踪工具。

**判据**：**利用率是上限的代理，饱和才是排队的事实**。两者必须一起看：
利用率低 + 饱和高 ⇒ 突发负载把队列顶起来了；利用率高 + 饱和低 ⇒ 稳定高负载，可能还能撑。

### §9.3.11 I/O Wait：一个会骗人的指标

I/O wait 是 per-CPU 指标：CPU 调度队列上有线程**在 sleep 状态阻塞于磁盘 I/O** 时，计入 I/O wait。

> If another CPU-hungry process comes along, the I/O wait value can drop: the CPUs now have something to do, instead of being idle. However, the same disk I/O is still present and blocking threads, despite the drop in the I/O wait metric.

**反直觉的两端**（原文都给了实例）：
- 来一个吃 CPU 的进程 ⇒ **I/O wait 反而下降**（CPU 有活干了），但磁盘 I/O 一分没少；
- 升级一个**更高效**的应用版本 ⇒ CPU 变闲 ⇒ **I/O wait 反而上升**，
  管理员会误以为「升级引入了磁盘问题」，其实磁盘性能一模一样，**变好的是 CPU**。

**更可靠的替代指标**：**应用线程被磁盘 I/O 阻塞的时间**——它衡量的是「应用线程受的苦」，
不受 CPU 忙闲影响。

**但 I/O wait 并非无用**：原文说它被广泛使用是因为能识别**一种特定的瓶颈形态**——
「磁盘忙、CPU 闲」。判据：把任何 I/O wait 都当成「系统有瓶颈」的信号去压；
**并发 I/O 更可能是非阻塞 I/O，不直接构成问题；非并发 I/O（I/O wait 暴露的那种）更可能是应用真在等。**

### §9.3.12 / §9.3.13 异步 I/O 与「磁盘 I/O ≠ 应用 I/O」

> It can be important to understand that disk I/O latency may not directly affect application performance, if the application I/O and disk I/O operate asynchronously.

**异步的三条路径**（原文 §9.3.12）：
1. write-back 缓存：应用 I/O 早早返回，真正的磁盘 I/O 后面才发；
2. 预读（read-ahead / prefetch）：应用不阻塞等盘，文件系统自己预取；
3. 应用自己的 I/O 工作线程：即便这段代码是同步的，它相对于**用户请求**也是异步的。

§9.3.13 列出了「磁盘 I/O 的速率/体积 ≠ 应用发起的 I/O」的四个原因：
文件系统的膨胀/回缩、缺内存导致的换页、驱动对 I/O 尺寸的取整或拆分、
RAID 写镜像/校验块。

> **判据**：应用层看到 I/O 慢但盘不慢，先查 §9.3.9 的虚拟盘口径与 §9.3.12 的异步性——
> 这两个「看起来矛盾」的现象，是磁盘分析里最常见的误判来源。

## §9.4 架构

### §9.4.1 磁盘类型

**机械旋转盘（HDD）**：慢在寻道与旋转：

> Slow I/O for magnetic rotational disks is usually caused by the seek time for the disk heads and the rotation time of the disk platter, both of which may take milliseconds.

两种消除随机 I/O 的经典手段：**把负载变顺序**（CoW 文件系统、电梯寻道）、
**把随机 I/O 隔离到别的盘**（原文列为七种减少寻道/旋转等待的策略之一）。

原文这一节还点了几个**容易在报告里被漏掉**的机制：

| 子标题（无编号） | 机制要点 |
|---|---|
| Sector Zoning | 外圈轨道更长 ⇒ **外圈吞吐更高**（所以 short-stroking 顺带提升吞吐） |
| Sector Size / Advanced Format | 4 KB 扇区；512e 仿真会引入 read-modify-write |
| On-Disk Cache | SCSI 的 TCQ / SATA 的 NCQ：设备侧命令重排 |
| Elevator Seeking | 重排 I/O；**副作用是可能饿死某个远处的 I/O**（原文举例：offset 2000 的请求被 offset 1000 的连续流堵住） |
| Data Integrity | ECC 在每扇区尾部；**读错会在下一转重试** ⇒ 这是「异常慢 I/O」的一个成因 |
| SMR | 窄道重叠写：密度 +25%，**写性能退化**；适合一次写多次读的归档，不适合写密集的 RAID |

**SMR 那条值得单独记**（原文 L501）：用 SMR 盘做 RAID 写密集负载是**已知反模式**。

**SSD / 闪存**：机制在「闪存控制器」这一层。
- 写只能写到**已擦除**的页；页（通常 8 KB）按 32–64 页（256–512 KB）的块擦除；
- 控制器用 **FTL**（闪存转换层）在「按页的原生操作」与「按扇区的块设备接口」之间翻译，
  **它内部相当于跑了一个文件系统**（原文说是 log-structured）：

> Translating between input and output is performed by the controller’s flash translation layer (FTL), which must also track free blocks. It essentially uses its own file system to do this, such as a log-structured file system.

- **写放大**由此而来：

> This can cause write amplification , where the remainder of the block is copied elsewhere before erasure, and also latency for at least the erase-write cycle.

写 I/O 尺寸小于闪存块（可大到 512 KB）时，块里没写满的部分必须先被搬到别处再擦除。

**寿命**（§9.4.1 的 Lifespan）：控制器用 wear leveling（把写摊到不同块）和
memory overprovisioning（预留额外空间）延长寿命。企业级 SLC 可达 100 万次写循环，
消费级 MLC 可能只有 1,000 次。

**判据**：SSD 的**最优 I/O 尺寸**由闪存布局决定（原文实例：4 KB 读 / 1 MB 写最优），
不同盘不同 ⇒ **必须微基准测出来，不要照抄别人的数字**。

### §9.4.2–9.4.3 接口与存储形态

**接口**（§9.4.2）：SCSI（并行、共享总线）→ SAS（串行点对点）→ SATA（消费级）→
FC（光纤 SAN）→ NVMe（**PCIe 直连，无控制器卡**）。

两条值得记的数字：

| 量 | 值 |
|---|---|
| SAS / SATA 的实际数据率 | 标称带宽的 **80%**（8b/10b 编码开销） |
| NVMe 单队列命令缓冲 | **64,000** 条；SAS 256 条、SATA 32 条 |

NVMe 的优势是**多条硬件队列**（同 CPU 可用、Linux multi-queue 下还避免共享内核锁）。

**存储形态**（§9.4.3）四种：磁盘设备（每盘独立可见，**最好分析**）、RAID、存储阵列、NAS。

**硬件 RAID vs 软件 RAID** 的取舍（原文 L635）：

> However, advances in processors have produced CPUs with a surplus of cycles and cores, reducing the need to offload parity calculations.

硬件 RAID 当年的优势（专用硬件算校验和、更快、带 BBU 电池）被**CPU 性能过剩**抹平了一部分；
于是又有一批方案回到软件 RAID（如 ZFS），换来**更低的复杂度、更低的硬件成本、
以及更好的可观测性**（软件 RAID 的单盘可被操作系统直接观测），
而且**坏了更好修**（想象一下 RAID 卡整个坏了）。

**判据**：**选软件 RAID 的真实理由是「可观测性」，不是「省硬件」**——
硬件 RAID 的盘在操作系统眼里是一个黑盒虚拟盘，出问题时你什么都查不到。

### §9.4.4 操作系统的磁盘 I/O 栈

块设备接口之下（原文 Figure 9.7 的通用模型）有三层驱动，**每层都可能有自己的队列或锁**。
Linux 的实际栈（Figure 9.8）在此之上加了 I/O 合并、I/O 调度器、卷管理器、device mapper。

**I/O 合并（merging / coalescing）**：把相邻请求合并，降低内核栈与磁盘的**每次 I/O 开销**。
`iostat` 的 `rrqm/s`（read merge per second）就是它的统计。

**I/O 调度器**是这一节的承重机制：

| 分类 | 代表 | 机制 |
|---|---|---|
| 经典（Linux 5.0 前） | noop / deadline / CFQ | 单一请求队列 + **单一锁** |
| 多队列（5.0 起为默认） | none / BFQ / mq-deadline / Kyber | blk-mq：每 CPU 独立提交队列 + 设备多派发队列 |

> The multi-queue driver (blk-mq, added in Linux 3.13) solves this by using separate submission queues for each CPU, and multiple dispatch queues for the devices. This delivers better performance and lower latency for I/O versus classic schedulers, as requests can be processed in parallel and on the same CPU where the I/O was initiated.

**为什么必须换掉经典调度器**：单一队列 + 单一锁在高 I/O 率下成了瓶颈；
而 blk-mq 支持**百万级 IOPS 的闪存设备**是前提（原文明确说了动机）。

**Kyber** 值得单记：只有两个可调项（`read_lat_nsec` / `write_lat_nsec`），
按**性能反馈自动调读写派发队列长度**以达成目标延迟。Netflix 云里默认用它。

**判据**：**Linux 5.0+ 看到 `cfq` / `deadline` 就该怀疑是过时配置**——
原文明说经典调度器「no longer included」。

## §9.5 方法论

### §9.5.1 Tools 方法与 §9.5.2 USE 方法

**Tools 方法**（§9.5.1）：迭代可用工具、看关键指标。原文列的磁盘工具清单是
`iostat`（找 busy 盘、高服务时间、高 IOPS）→ `iotop`/`biotop`（谁在发 I/O）→
`biolatency`（延迟直方图、找多峰与离群）→ `biosnoop`（逐条看）→
`perf`/BCC/bpftrace（自定义分析、用户态与内核态栈）。

**USE 方法**（§9.5.2）是升级版：

> The USE method is for identifying bottlenecks and errors across all components, early in a performance investigation.

对**每块磁盘**查三件事：**Utilization（忙不忙）/ Saturation（I/O 在不在排队）/ Errors（有没有错）**。
原文特别提醒**先查 Errors**——因为冗余池设计成「坏一块照样跑」，磁盘坏了系统只是变慢，
错误常被忽略。

对**每个控制器**查三件事，但**利用率的定义变了**：

| 层 | Utilization 的定义 |
|---|---|
| 磁盘设备 | 时间维度：设备忙的时间占比 |
| 磁盘控制器 | **能力维度**：当前吞吐 vs 最大吞吐、当前操作率 vs 最大操作率 |
| 传输总线 | 同控制器（也要单独查：errors / utilization / saturation） |

**这是 §9.5 全节最有价值的一条**：**「控制器利用率」不是时间百分比，是「你用了它多少能力」。**
原文 L921 还给了控制器指标缺失时的土办法：单控制器就把所有盘的 IOPS 与吞吐加起来；
多控制器要先弄清盘到控制器的归属再分别求和。

**判据**：**「总吞吐/总 IOPS 在各种负载下都卡在同一个数」⇒ 该查控制器与传输总线，而不是盘。**

### §9.5.3–9.5.4 性能监控与工作负载刻画

**要监控的关键指标**（原文）：磁盘利用率、响应时间。判据：
100% 持续数秒「very likely an issue」；**超过 60% 因排队增多也可能变差**。

监控粒度：**按盘看**（找负载不均与个别坏盘）；响应时间**按秒平均**，
但理想上要看**完整分布**（直方图 / 热力图）来发现离群点。

**工作负载刻画的六个基本属性**（原文 §9.5.4）：I/O 速率（IOPS）、I/O 吞吐、
I/O 大小、读写比、随机 vs 顺序。

原文给了一个**完整的示例描述句**（值得当模板抄）：

> The system disks have a light random read workload, averaging 350 IOPS with a throughput of 3 Mbytes/s, running at 96% reads.

（后接突发顺序写、读写各自的尺寸。）

**推论（推演）**：这六属性是**容量规划与基准测试的共同输入**——
§9.5.10 的扩展计算、§9.9 的调优目标，全都要落到这六个数上。

原文还附了「进阶刻画清单」（§9.5.4 的 Advanced Workload Characterization/Checklist），
本质是**一组该问的问题**：系统级/每盘/每控制器的 IOPS 与吞吐各是多少？
哪些应用在用？访问的是什么文件系统？I/O 在各盘间**分布是否均衡**？
非数据传输命令有哪些？I/O 的**到达时间分布**？I/O 在多大程度上是**应用同步**的？

**最后一个问题（应用同步性）最容易被跳过，但它是 §9.3.12 的落地**：异步 I/O 的慢，
用户是感觉不到的。

### §9.5.5 延迟分析：逐层对比定位

思路是**在栈的每一层测同一个 I/O 的延迟**，看哪一层开始变慢：

> If this matches the I/O latency at the application level, it’s usually safe to assume that the I/O latency originates from the disks, allowing you to focus your investigation on them. If the latency differs, measuring it at different levels of the operating system stack will identify the origin.

原文用两个离群 I/O 举例：
- **I/O A**：各层延迟都差不多 ⇒ 指向**磁盘（或磁盘驱动）**；
- **I/O B**：延迟出现在**文件系统层**（锁？排队？），下层只占很小一部分。

**B 案例的一个陷阱**（原文明说）：文件系统层的一次逻辑 I/O 可能在底层引发**多次** I/O
（例如元数据），只看到下层那一次的 10 ms 会误判。

每层的延迟呈现方式有三种：区间平均、完整分布（直方图/热力图）、**逐条 I/O 值**（事件追踪）。
后两种才追得到离群点的来源。

### §9.5.6–9.5.8 静态调优、缓存调优、资源控制

**§9.5.6 静态调优**问的是「配置本身有没有问题」，原文列了一长串静态问题
（磁盘数量与类型、固件版本、控制器与接口类型、PCIe 插槽速率、每 HBA 挂几块盘、
BBU 电量、RAID stripe 宽度、多路径是否配置、驱动版本、内存大小与缓存占用、有无资源控制）。

原文讲了一个真实的教训（§9.5.6 结尾）：

> I learned to ask for configuration details first (usually over the phone) before spending time logging in to the system and examining I/O latency.

**判据**：**先问配置，再登录看延迟**。原文说这是他做 ZFS 存储性能负责人时收到的
**最常见的性能投诉**的原因——配置是「半个 JBOD（12 块盘）做 RAID-Z2 宽条带」，
可靠性好但性能跟单盘差不多。

**§9.5.7 缓存调优**：四步——**查清有哪些缓存 → 查它们是否在工作 → 查工作得好不好
→ 按缓存调负载、按负载调缓存**。

**§9.5.8 资源控制**：操作系统可以对进程/进程组设 IOPS 与吞吐的**固定上限**或**份额**。
**关键判据**：磁盘成为瓶颈，可能是因为**施加的限制**，而不是磁盘本身的活动——
所以排查时一定要确认「资源控制是否在生效」。

### §9.5.9–9.5.10 微基准与扩展

**微基准要测的是「盘」，不是「文件系统」**：走 raw 设备路径，绕开
缓存、缓冲、I/O 拆分/合并、代码路径开销、偏移映射差异。

**六个微基准因子**：方向（读/写）、偏移模式（随机/顺序）、偏移范围（全盘 or 只打 offset 0）、
I/O 大小（512 B → 1 MB）、**并发度**、设备数量。

「**offset 0 only**」是有意为之的技巧：它把数据打进**盘上缓存**，从而单独测出缓存命中时间
（原文还提醒：有些厂商固件会加速 sector 0，别被骗，要拿别的扇位对照测）。

**扩展（§9.5.10）的五步计算**里，最关键的一条是**别按峰值算**：

> Do not use the maximum throughput and IOPS values per disk, as this would result in driving disks at 100% utilization, leading to immediate performance issues due to saturation and queueing. Pick a target utilization (say, 50%) and scale values accordingly.

另外四步：定目标负载（按用户数换算吞吐与 IOPS，注意**缓存不同步扩展会让盘负载上升**）→
算盘数（**按 50% 目标利用率折算**）→ 算控制器数 → 检查传输总线是否超限 → 算每 I/O 的 CPU 周期与所需 CPU 数。

**判据**：**扩展存储时，「除以 2」不是经验法则，是队列论的直接结论**——
按峰值配盘，等于把系统长期停在饱和区。

## §9.6 可观测性工具 / §9.7 可视化

**§9.6** 逐个介绍 Linux 侧工具（iostat、sar、PSI、pidstat、perf、biolatency、biosnoop、
iotop/biotop、biostacks、blktrace、bpftrace、MegaCli、smartctl、SCSI logging）。本篇不逐条展开，
只标注它们各自回答哪个问题（工具的详细用法见附录 A/B/C 与第13–15章）：

| 工具 | 回答的问题 |
|---|---|
| `iostat` | 盘级聚合：IOPS、吞吐、`%util`、`aqu-sz`、`r_await` |
| `sar` | 历史采样与长期趋势（附录 B） |
| **PSI**（`/proc/pressure/io`） | **一段时间内有多少任务因 I/O 而卡住**——比 `%util` 更贴近「用户感受」 |
| `pidstat` | 哪个进程在发 I/O（`-d`） |
| `perf` / `bpftrace` | 自定义：发起 I/O 的用户态+内核态栈（附录 C） |
| `biolatency` | 延迟**直方图**（找多峰与离群） |
| `biosnoop` | **逐条 I/O**（含排队时间 `Q`）与离群分析 |
| `biotop` | 按进程聚合的 I/O 热点 |
| `blktrace` / `btt` | 块层**事件序列**，看设备侧重排 |
| `MegaCli` / `smartctl` | RAID 卡与盘的**错误计数器**（USE 方法里的 Errors 项） |

**§9.7** 的五种可视化各有其擅长：

| 可视化 | 擅长回答 |
|---|---|
| 折线图 | 随时间的变化模式（白天负载波动、周期性刷盘） |
| 延迟散点图 | **逐事件**的延迟分布，找离群点 |
| 延迟热力图 | 上百万事件的延迟 × 时间（散点图到量级就成「paint」了） |
| 偏移热力图 | I/O 在盘上的**空间分布**（顺序是暗线，随机是亮云） |
| 利用率热力图 | 找出**单块热盘**（顶部的 100% 线） |

**贯穿全节的一条警告**（原文在折线图一节说，后面几节反复强化）：
**平均值会隐藏多峰分布、离群点、盘间不均、短时波动**。
§9.6 里 `iostat` 报的是 `r_await`（平均值），§9.7 里所有图都是为了让平均值露馅。

## §9.8–9.9 实验与调优

**§9.8 实验**（主动压测）：

- **`dd`** 做临时顺序测：注意「内核会缓存和缓冲，测出来的可能是缓存+盘」；
  要单独测盘用 raw 设备；要更安全就用 `oflag=direct` 打文件（会带一点文件系统开销）。
- **自定义负载生成器**：用 C 直接打开块设备、带上 `O_DIRECT`。
- **微基准工具**：`hdparm -Tt`（`-T` 测缓存读、`-t` 测设备读，**两者差值就是缓存效果**）。
- **`ioping`**：负载极轻（默认每秒 4 KB），把盘只驱动到 **0.4% 利用率**——
  原文明说它**可以在生产环境调试**，而别的微基准会把盘打到 100%。

**一条通用纪律**（原文 §9.8 开头）：

> When using these tools, it’s a good idea to leave iostat(1) continually running so that any result can be immediately double-checked.

**§9.9 调优**：

- **OS 层**：`ionice`（调度类 0 none / 1 real-time / 2 best effort / 3 idle）、
  cgroups 的 `blkio` 资源控制、内核参数（`/sys/block/*/queue/scheduler`、`nr_requests`、`read_ahead_kb`）。
- **盘层**：`hdparm`（电源管理、spindown 超时）——**原文警告多个选项标着 "DANGEROUS"**。
- **控制器层**：Dell PERC 的 MegaCli 参数（Cache Flush Interval、Rebuild Rate、Interrupt Throttle…）。

**收尾的判断**（原文 §9.9 开头）：

> While changing tunables can be easy to do, the default settings are usually reasonable and rarely need much adjusting.

> **判据**：**调参数是最后手段**。原文的顺序是：先看方法论（§9.5）能不能省掉不必要的工作 →
> 再看架构与配置（§9.5.6 静态调优）有没有配错 → 最后才动 tunable。

## 章末选型判据

| 场景 | 用什么 | 别用什么 |
|---|---|---|
| 报告里要写「磁盘延迟」 | 写清口径：block I/O request / disk request / disk service，并给完整分布 | 只写一个 `r_await` 平均值（双峰分布会被塌缩成两边都不像的数） |
| 要解释「IOPS 凭什么比」 | 补齐随机/顺序、I/O 大小、读/写、buffered/direct、并发度 | 单独引用一个 IOPS 数字 |
| 怀疑盘是瓶颈 | 按 USE 查 Utilization / Saturation / **Errors**（错误先查） | 只看 `%util` |
| 虚拟盘利用率异常 | 同时查物理盘利用率与控制器能力利用率 | 把虚拟盘 `%util` 当成物理盘实况 |
| 「I/O wait 忽高忽低」 | 改用「应用线程被 I/O 阻塞的时间」 | 把 I/O wait 当成磁盘负载的唯一指标 |
| 吞吐/IOPS 在不同负载下卡在同一个数 | 查控制器与传输总线的**能力**利用率 | 继续加盘 |
| 磁盘 I/O 慢但应用不慢 | 先查 §9.3.12 的异步性与 §9.3.13 的膨胀/回缩 | 立刻加盘 |
| 要做容量扩展 | 按**目标利用率 50%** 折算盘数，再算控制器与总线 | 按单盘峰值吞吐/IOPS 配盘 |
| 要测盘的裸性能 | raw 设备路径 + `O_DIRECT`，覆盖方向/偏移/尺寸/并发 | `dd` 打普通文件（测的是缓存+文件系统） |
| 生产环境想试一下盘的延迟 | `ioping`（只驱动到 0.4% 利用率） | 其他会把盘打到 100% 的微基准 |
| 排查「盘时快时慢」 | 热力图/散点图 + 事件追踪 | 折线图上的平均值曲线 |
| 想调优 | 先排查不必要的工作与静态配置 | 直接改 tunable（默认值通常已合理） |
| Linux 5.0+ 选 I/O 调度器 | 多队列系（BFQ / mq-deadline / Kyber），要延迟目标用 Kyber | `cfq` / `deadline`（已不在内核里） |
