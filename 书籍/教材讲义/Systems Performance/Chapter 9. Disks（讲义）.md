---
title: Chapter 9. Disks（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - 磁盘IO
  - 延迟分析
  - 队列深度
  - 利用率与饱和度
  - 性能工程
source: "《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg，Addison-Wesley 2020，英文原版）第 9 章 Disks"
companion: "[[09-Chapter 9. Disks]]"
---

# 第 9 章 讲义：磁盘不是瓶颈，「等磁盘」才是

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[09-Chapter 9. Disks|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：本仓库**没有任何磁盘 IO**——`src/common/logger.cpp` 的唯一"落盘"是 `std::fprintf(stderr, ...)`，全仓 `src/` 下 grep 不到 `ofstream` / `fopen` / `fwrite` / `fsync`，也没有 yaml 加载器（`src/config/server_config.h` 明写"本仓库**没有 yaml 加载器**"）。于是本章的落点不是磁盘，而是三件别的东西：**时间口径**（`src/net/metrics.h` 的 `tick_drift_max_ms` 为什么必须写明起止事件）、**饱和度**（`src/proto/protocol.h` 的 `kSendBufCap = 128 KB` 有硬上限，却没有水位指标）、**分配器压力**（`src/room/room_mgr.cpp` 每 10 Hz 重建一次 `proto::SnapshotView`）。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原章。

---

## 0. 开局：一条 iostat 输出，盘没变慢，延迟却翻了三倍

原书 §9.8.4 有一张表：同一个盘，依次跑 1 个、2 个、3 个、4 个、5 个并发的随机读工具，每次抓一行 `iostat`。

```
Device:    rrqm/s      r/s    rkB/s  avgrq-sz   aqu-sz r_await  svctm  %util
sda        878.00   234.00  2224.00     19.01     1.00    4.27   4.27 100.00
sda       1233.00   311.00  3088.00     19.86     2.00    6.43   3.22 100.00
sda       1366.00   358.00  3448.00     19.26     3.00    8.44   2.79 100.00
sda       1775.00   413.00  4376.00     21.19     4.01    9.66   2.42 100.00
sda       1977.00   423.00  4800.00     22.70     5.04   12.08   2.36 100.00
```

四列要同时读：

| 列 | 读数怎么走 | 说明什么 |
|---|---|---|
| `%util` | 一直是 100.00 | 盘"一直忙" |
| `aqu-sz` | 1.00 → 2.00 → 3.00 → 4.01 → 5.04 | **队列在阶梯式加深** |
| `r_await` | 4.27 → 6.43 → 8.44 → 9.66 → 12.08 ms | 读延迟涨了近三倍 |
| `svctm` | 4.27 → 3.22 → 2.79 → 2.42 → 2.36 ms | **服务时间反而在下降** |

最后一行是整张表的关键：**盘本身变快了**（服务时间从 4.27 ms 降到 2.36 ms），是**排队**把用户感受到的延迟撑起来的。

于是本章的主线就一句话（本讲义自己的概括，不是原书原句）：

**磁盘的性能问题，几乎从不是"盘变慢了"，而是"等盘"的时间变长了。**

所以这一章反复做同一件事：**把时间拆开、把队列拆开、把"谁在动"和"谁在等"分开**。这一章讲的术语、模型、方法论、工具，全是为这一次拆分服务的。

（推演）把这张表当排障姿势读：**先问"排队的深度变了没有"，再问"服务变慢了没有"**。顺序反了，就会得出"盘变慢了，去换盘"这种昂贵而且经常错误的结论。

---

## 1. §9.1 Terminology：先把词钉死，因为这个词有五层

这一节全是定义，但有几个词**必须精确**——不是咬文嚼字，是**同一个词在不同层指的不是同一段时间**。

- **Virtual disk（虚拟磁盘）**：对存储设备的仿真。在系统看来是一块物理盘，实际可能由多块盘拼成，或只是一块盘的一部分。
- **Transport（传输通道）**：设备通信用的物理总线，承载数据与其它磁盘命令。
- **Sector（扇区）**：盘上的存储块，传统 512 字节，如今常见 4 KB。
- **I/O**：严格说只包括磁盘的读与写，**不包括**其它磁盘命令。
- **Throughput（吞吐）**：当前数据传输率，bytes/s。
- **Bandwidth（带宽）**：存储通道或控制器能达到的**最大**数据传输率，由硬件封顶。
- **Latency outliers（延迟离群点）**：延迟异常高的磁盘 I/O。

**最容易出事的是 "I/O latency" 这一个词。** 原文说得很直接：

> I/O latency alone may refer to the block I/O request time: the entire I/O time.

同一个词，在应用层、在内核层、在磁盘工程师嘴里，**指的不是同一段时间**。这就是下一节要展开的。

---

## 2. §9.2.1–9.2.3 Models：三个模型，三层队列

### §9.2.1 Simple Disk

现代磁盘内部**自己有一个 I/O 队列**（Figure 9.1）。被盘接收的 I/O，要么在队列里等，要么正在被处理。

> This simple model is similar to a grocery store checkout, where customers queue to be serviced.

（推演）**排队论**就是从这里进来的。原文说这个模型"很适合用排队论分析"——于是"利用率 60% 就不该再往上"这条经验值（第 2 章的 M/D/1）有了物理对应物：收费站前排了多长的队。

也提醒一句：磁盘控制器**不一定是先来先服务**。机械盘会用电梯算法，闪存盘可能给读和写分开的队列。

### §9.2.2 Caching Disk

盘上带一小块 DRAM 缓存（Figure 9.2）。命中 ⇒ 极快；未命中 ⇒ 回到盘的正常延迟。

写这边有两个词必须分清：

> The on-disk cache may also be used to improve write performance, by using it as a write-back cache. This signals writes as having completed after the data transfer to cache and before the slower transfer to persistent disk storage.

- **write-back（回写）**：数据进了缓存就算写完，**稍后**才慢慢刷到盘上。
- **write-through（直写）**：必须整块传到下一层才算写完。

⚠️ 实务上的提醒：存储的 write-back 缓存**通常配电池**，断电时能把缓存里的数据保住。电池可能在盘上，也可能在控制器上。

### §9.2.3 Controller

磁盘控制器（也叫 HBA）架在 CPU 的 I/O 通道和存储通道之间（Figure 9.3）。

> Performance may be limited by either of these buses, the disk controller, or the disks.

**四个候选嫌疑人**：两条总线、控制器、盘。这一句是本章后面所有"往上查还是往下查"的判断依据。

---

## 3. §9.3.1 Measuring Time：全章最重要的一节

这一节做的事情是：**把"I/O 花了多久"拆成可以分别观测的几段**。

### 三个通用词

| 词 | 定义 |
|---|---|
| **I/O request time**（I/O 响应时间） | 从发出 I/O 到它完成，**整段** |
| **I/O wait time** | **在队列上等**的那段 |
| **I/O service time** | 真正**被处理**的那段（不含等待） |

原文给了一张术语来源的说明：`service time` 这个词来自磁盘还是"简单设备"的年代——那时操作系统直接管着盘，知道它什么时候在干活。现在盘自己做内部排队，于是**内核看到的 service time 里已经包含了在内核队列上等的时间**。

### 从内核看

- **Block I/O wait time**（OS wait time）：从 I/O 被创建、塞进内核 I/O 队列，到它离开**最后一个**内核队列、被下发给设备为止。可能横跨多层内核队列（块 I/O 层队列 + 设备队列）。
- **Block I/O service time**：从把请求下发给设备，到设备的完成中断。
- **Block I/O request time** = 上面两者之和。

### 从盘看

- **Disk wait time**：在**盘自己的队列**上花的时间。
- **Disk service time**：出了盘队列之后，真正被处理的时间。
- **Disk request time**（= disk response time = disk I/O latency）= 上面两者之和，**等于 block I/O service time**。

原文对"从盘看"这组加了一句限定：

> The term service time originates from when disks were simpler devices managed directly by the operating system, which therefore knew when the disk was actively servicing I/O.

⚠️ 而且它提醒：**"block I/O service time 被当成当前磁盘性能的度量"是个简化**（这是老版 iostat 的做法）。因为在块设备接口之下可能还有三层驱动层，任何一层都可能有**自己的队列**或者**卡在 mutex 上**，这些延迟全被算进了 block I/O service time。

### 一个可以手算的公式

盘的服务时间通常不能直接从内核统计里读出来，但可以**反推**：

> disk service time = utilization/IOPS

原文的例子：利用率 60%、IOPS 300 ⇒ 平均服务时间 2 ms（600 ms / 300 IOPS）。

⚠️ 但这个公式**假设了设备一次只能处理一个 I/O**。现代盘通常能并行处理多个 I/O，所以这个算式**不准确**。要准确的服务时间，只能用事件追踪量"下发时刻"和"完成时刻"的高精度时间戳。

（推演）这个"用两个粗指标反推一个细指标"的思路值得记住——它在本仓库里有个**形状完全一样**的对应物：`src/net/metrics.h` 的 `tick_drift_max_ms` 就是用一个粗口径指标（tick 漂移）去近似"主循环实际落后多少"。但仓库作者在那里**明确写了它为什么不做反推**（因为漂移是有符号的，`v > cur` 会静默吞掉负半轴）。**同样是反推，一个敢用，一个不敢用——差别在于符号。**

---

## 4. §9.3.2–9.3.3 Time Scales 与 Caching：延迟有几个数量级

### §9.3.2 时间尺度

磁盘 I/O 的时间尺度**跨好几个数量级**——从几十微秒到几秒。

Table 9.1（原文给的是"示例时间尺度"，精确值要查厂商文档并自己测）：

| 事件 | 延迟 |
|---|---|
| 盘上缓存命中 | < 100 μs |
| 闪存读（小→大 I/O） | ~100 到 1,000 μs |
| 机械盘顺序读 | ~1 ms |
| 机械盘随机读（7,200 rpm） | ~8 ms |
| 机械盘随机读（慢，含排队） | > 10 ms |
| 机械盘随机读（队列里排了几十个） | > 100 ms |
| 虚拟盘最坏情况（硬件控制器 + RAID-5 + 排队 + 随机） | > 1,000 ms |

原文给了两个**行业阈值**的经验值，很值得记：

- 企业存储行业：**超过 10 ms 的磁盘 I/O 就算异常慢**，是潜在的性能问题源。
- 云计算行业：容忍度高得多——面向 web 的应用本来就预期"网络到浏览器"有高延迟，所以**磁盘 I/O 要超过 50 ms 才算问题**（单独算，或在一次应用请求内累计算）。

这张表还揭穿了一个常见错觉：

> Since a disk will return a mixture of these, expressing them together as an average latency (as iostat(1) does) can be misleading, as this is really a distribution with two modes.

**盘返回的是两种延迟的混合**（缓存命中的 < 100 μs 与未命中的 1–8 ms+），**平均成一个数是有害的**——它是**双峰分布**，平均值落在哪个峰之间都可能毫无意义。

### §9.3.3 缓存层

> The best disk I/O performance is none at all.

**最好的磁盘 I/O 性能，是没有磁盘 I/O。** 软件栈的每一层都在想办法**避免**磁盘 I/O：缓存读、缓冲写，一路到盘自己身上。

Table 9.2 列了盘这一侧的缓存：设备缓存（ZFS vdev）、块缓存（buffer cache）、磁盘控制器缓存（RAID 卡缓存）、存储阵列缓存（阵列缓存）、盘上缓存（DDC 挂的 DRAM）。

还有一层在盘**之下**：这些缓存对**随机 I/O 负载**的改善尤其大。

---

## 5. §9.3.4–9.3.8 工作负载特征：IOPS 这个数字为什么单独拎出来没有意义

### §9.3.4 随机 vs 顺序

- **顺序（sequential）**：也叫 **streaming**，通常在应用层用"流式读写"这个词。
- **随机（random）**：需要磁头寻道或等盘片旋转。

机械盘时代，随机 I/O 的额外延迟来自寻道 + 旋转（Figure 9.6）。调优手段包括：缓存、把随机 I/O 隔离到单独的盘、用磁盘布局缩短寻道距离。

**闪存盘基本没有这个区分**——随机读和顺序读表现差不多。⚠️ 但有个例外：**小于块大小的写会触发 read-modify-write**，随机写尤其吃亏。

⚠️ 还有一个陷阱：**操作系统看到的 offset 未必是物理盘上的 offset**。硬件虚拟盘可能把一段连续区间映射到多块盘上，盘自己（DDC）也可能重映射。所以**随机 I/O 往往不是看 offset 看出来的，而是靠"服务时间变高"推断出来的**。

### §9.3.5 读写比

读/写比例（按 IOPS 或按吞吐算），可以表达成时间上的比例："系统自启动以来 80% 是读"。

它的用处很实际：**高读负载加缓存最划算；高写负载加盘（提吞吐和 IOPS 上限）最划算**。而且读和写**各自还可能是不同的模式**——读随机、写顺序（写时复制文件系统尤其如此），I/O 尺寸也可能不同。

### §9.3.6 I/O 大小

I/O 越大，吞吐通常越高，但**单次延迟也更长**。而且尺寸可能在**应用层发出之后**被内核组件（文件系统、卷管理器、设备驱动）**膨胀或缩小**。

> a flash-based disk drive may perform optimally with 4 Kbyte reads and 1 Mbyte writes.

闪存盘的最佳读尺寸和最佳写尺寸**不是同一个数**（4 KB 读 vs 1 MB 写），因为 flash 的内部布局就是这样。

### §9.3.7 IOPS Are Not Equal

> Because of those last three characteristics, IOPS are not created equal and cannot be directly compared between different devices and workloads.

三个特征（随机/顺序、I/O 大小、读写方向）任意一个不同，**两个 IOPS 数字就不可比**。

> An IOPS value on its own doesn’t mean a lot.

而且 IOPS **对应用可能根本不重要**：随机请求组成的负载是**延迟敏感**的，高 IOPS 才好；流式负载是**吞吐敏感**的，低 IOPS 但 I/O 更大反而更想要。

> To make sense of IOPS, include the other details: random or sequential, I/O size, read/write, buffered/direct, and number of I/O in parallel.

**报 IOPS 必须带五个限定**：随机还是顺序、I/O 尺寸、读还是写、缓冲还是直写、并发几个 I/O。或者干脆改报**时间类指标**（利用率、服务时间），它们更容易横向比。

### §9.3.8 非数据传输类磁盘命令

盘还会收到**不是读写**的命令：

- **flush（刷缓存）**：带盘上缓存的盘可能被命令把缓存刷到盘上。⚠️ **这不是一次数据传输**——数据早就通过写命令送出去了。
- **TRIM / UNMAP**：告诉盘"这一段扇区不要了"。对 SSD 维护写性能很有用。

> These disk commands can affect performance and can cause a disk to be utilized while other I/O wait.

⚠️ **这类命令会让盘"显示为忙"，同时其它 I/O 在干等**——这就是为什么不能只看 `%util`。

---

## 6. §9.3.9–9.3.11 利用率、饱和度、I/O Wait：三个最容易被误读的指标

### §9.3.9 Utilization

利用率 = 区间内盘**忙着干活**的时间占比。0% 是空闲，100% 是连续不断。

> However, any rate of disk utilization can contribute to poor performance, as disk I/O is typically a slow activity.

**任何程度的利用率都可能带来糟糕的性能**——因为磁盘 I/O 本来就是慢活动。真正的问题不在"多少"，在"排队了没有"。

还有一个反直觉的中间点：

> There may also be a point between 0% and 100% (say, 60%) at which the disk’s performance is no longer satisfactory due to the increased likelihood of queueing

**60% 可能就够了**（具体值取决于盘、负载与延迟要求）。要确认是不是利用率的问题，得看**响应时间**以及**应用是不是真的阻塞在这个 I/O 上**——应用可能在异步做 I/O，慢 I/O 并没直接让应用等。

⚠️ **利用率是区间汇总**。磁盘 I/O 是**突发**的（尤其是写刷盘），用更长的区间汇总会把突发**藏起来**。

#### 虚拟盘的利用率会骗人

这是本节最实用的一段：

> A virtual disk that is 100% busy, and is built upon multiple physical disks, may be able to accept more work. In this case, 100% may mean that some disks were busy all the time, but not all the disks all the time, and therefore some disks were idle.

**虚拟盘 100% 忙 ≠ 底下每块盘都忙**。另外两种情况：

> Virtual disks that include a write-back cache may not appear very busy during write workloads, since the disk controller returns write completions immediately, even though the underlying disks are busy for some time afterward.

> Disks may be busy due to hardware RAID rebuild, with no corresponding I/O seen by the operating system.

**RAID 重建可以让盘一直忙，而操作系统一个 I/O 都看不见。**

### §9.3.10 Saturation

> Once a physical disk reaches 100% utilization and more I/O is requested, it becomes saturated.

**饱和 = 超出资源交付能力之外、排在队列里的工作量**。对磁盘设备来说，可以算成**操作系统里设备等待队列的平均长度**。

> A disk at 100% utilization may have no saturation (queueing), or it may have a lot, significantly affecting performance due to the queueing of I/O.

**100% 利用率既可能毫无排队，也可能排得很深**——两者对性能的影响天差地别。利用率给不出这个信息，**队列深度能给**。

⚠️ 别想当然地认为"不到 100% 就没有饱和"：这取决于**汇总区间**——50% 可能意味着"一半时间 100%、一半时间空闲"。

### §9.3.11 I/O Wait

I/O wait 是**每 CPU** 的指标：CPU 空闲、且**调度队列里有线程处于 sleep 状态阻塞在磁盘 I/O 上**的那部分时间。它把 CPU 空闲时间拆成"没事干"和"卡在 I/O 上"两半。

这一指标非常容易误导人：

> If another CPU-hungry process comes along, the I/O wait value can drop: the CPUs now have something to do, instead of being idle. However, the same disk I/O is still present and blocking threads, despite the drop in the I/O wait metric.

**来了个吃 CPU 的进程，I/O wait 就掉下来了**——但磁盘 I/O 还在、线程还在被阻塞。指标降了，故障没消失。

> The reverse has sometimes happened when system administrators have upgraded application software and the newer version is more efficient and uses fewer CPU cycles, revealing I/O wait.

反过来也发生过：升级应用后 CPU 省了，**原本被 CPU 忙碌掩盖的 I/O wait 露了出来**——运维以为"升级引入了磁盘问题"，其实磁盘一直那样，是 CPU 变快了。

**更可靠的指标是"应用线程被磁盘 I/O 阻塞的时间"**——它不管 CPU 在忙什么，只测"应用受了多少罪"。

但作者也没否定 I/O wait：它确实成功识别过一类瓶颈——**盘忙、CPU 闲**。用法是：**把任何 I/O wait 都当成系统瓶颈的信号，然后想办法把它消掉**。I/O 与 CPU 利用率**同时**发生时，多半是非阻塞 I/O；**不同时**发生的 I/O wait，更可能是应用阻塞型 I/O，是真瓶颈。

---

## 7. §9.3.12–9.3.13 同步 vs 异步、盘上的 I/O vs 应用的 I/O

### §9.3.12

**磁盘 I/O 的延迟未必直接影响应用性能**——如果应用 I/O 和磁盘 I/O 是异步的。这在 **write-back 缓存**下很常见：应用 I/O 早早完成，磁盘 I/O 是后来才发出去的。

读这边有 **read-ahead**（预读）可以把读变成异步；文件系统也会自己预取（prefetch）来暖缓存。

⚠️ 还有一层：**即使应用在同步等 I/O，这条代码路径也可能对客户端请求不重要**——它可能只是一个专门管 I/O 的工作线程，其它线程还在干活。

### §9.3.13 Disk vs. Application I/O

盘上的 I/O 是**内核一堆组件共同作用的结果**，它的速率和体量很可能跟应用发出的对不上：

| 差异来源 | 说明 |
|---|---|
| **文件系统的膨胀/缩小/无关 I/O** | 第 8 章 §8.3.12 |
| **内存不足引起的换页** | 第 7 章 §7.2.2 |
| **设备驱动的 I/O 尺寸调整** | 向上取整，或把 I/O 拆碎 |
| **RAID 写镜像/校验块，或读数据时校验** | 一次应用写变成多块盘上的写 |

> This mismatch can be confusing when unexpected. It can be understood by learning the architecture and performing analysis.

**对不上是常态，不是故障。** 搞不清的时候，去学架构 + 做分析，而不是去怪应用。

---

## 8. §9.4.1–9.4.4 Architecture：硬件与内核那一侧

### §9.4.1 盘的类型

**机械旋转盘（HDD）**：盘片（platters）涂着氧化铁颗粒，机械臂带着磁头读写。数据存在同心磁道（tracks）上，磁道再切成扇区（sectors）。

**慢的两个来源：seek（寻道）与 rotation（旋转）**，两者都可能花上毫秒。

减少这两个的办法：缓存、写时复制（把写变顺序，但**以后的读会变随机**）、把不同负载分到不同盘、把不同负载分到不同系统、盘自己的电梯算法、更高密度的盘、分区配置（short-stroking）。

顺带一串值得记的名词：

- **Short-stroking**：只用最外圈磁道。寻道距离被限制，头可以停在边缘。⚠️ **看公开的磁盘基准时要留意这个**——很多被测的盘是 short-stroked 的。
- **Sector zoning（multiple-zone recording）**：外圈磁道更长 ⇒ 每道扇区更多 ⇒ 外圈吞吐更高。
- **Advanced Format / 4 KB 扇区**：减少每扇区的元数据开销；但 512e 模拟可能引入 read-modify-write；**4 KB 非对齐 I/O 会跨两个扇区**，扇区 I/O 数被放大。
- **On-disk cache 里的命令队列**：SCSI 叫 TCQ，SATA 叫 NCQ。
- **Elevator seeking（电梯算法）**：按盘上位置重排 I/O。⚠️ 但它有代价——见下面的"饿死"。

**电梯算法的代价**（原文举的例子，值得整段记）：

> A disk has been sent a batch of I/O near offset 1,000, and a single I/O at offset 2,000.

盘头现在在 1,000。offset 2,000 那个 I/O 什么时候被服务？

> Now consider that, while servicing the I/O near 1,000, more arrive near 1,000, and more, and more—enough continual I/O to keep the disk busy near offset 1,000 for 10 seconds.

**如果 1,000 附近源源不断来 I/O，那个 2,000 的 I/O 就被饿死了**——它的最终延迟可能是 10 秒。**这就是"平均延迟好看、个别请求惨不忍睹"的来源。**

其它几项：

- **Data Integrity（ECC）**：每个扇区末尾存纠错码。读错时磁头会在**下一圈重试**（可能还微调位置）。**这可能是不寻常慢 I/O 的解释**——软错误率上升是盘要坏的信号。
- **Vibration（振动）**：作者 2008 年对着磁盘阵列**大喊**做写基准，引发一波极慢的 I/O，视频上了 YouTube，170 万次观看，顺带催生了"数据中心隔音"这行生意。
- **Sloth disks（懒盘）**：某些旋转盘会返回**超过一秒**的慢 I/O，**却不报任何错**。原文的评语很到位：这种盘**不如直接失败**，那样 OS 或控制器还能把它下线或上报。
- **SMR（瓦片式磁记录）**：磁道更窄，写头写不进去，靠部分重叠来写（像屋顶瓦片）。密度 +25%，代价是**写性能退化**（被覆盖的数据要重写）。适合"写一次读很多"的归档，**不适合写密集的 RAID**。
- **Flash / SSD**：读快写慢（因为 flash 要**整块擦除**才能重写）。控制器里的 **FTL（flash translation layer）** 相当于在闪存内部跑了一个日志结构文件系统。⚠️ **写放大（write amplification）**：写入尺寸小于块（可达 512 KB）时，块里剩下的内容得先被拷走再擦除。**TRIM/UNMAP 的作用就是让盘更容易拼出空闲块，从而减小写放大。** 延寿靠 **wear leveling（磨损均衡）** 与 **overprovisioning（预留空间）**：SLC 约 5 万–10 万次写擦，MLC 约 5 千–1 万次，TLC 约 3 千次，QLC 约 1 千次。
- **Pathologies（病态）**：老化导致的延迟离群点、碎片化导致的延迟上升（重格式化能治好）、盘内压缩导致的吞吐下降。

### §9.4.2 接口

SCSI（并行总线，**共享 ⇒ 会争用**）→ SAS（串行点对点，SAS-4 22.5 Gbit/s，8b/10b 编码 ⇒ **实际速率是标称的 80%**）→ SATA（6 Gbit/s）→ FC（光纤）→ **NVMe**（PCIe 直挂，**不经过控制器卡**）。

NVMe 的关键优势是**多硬件队列**：可以从同一个 CPU 发起（保住缓存温度），每条队列支持到 **64,000 条命令**，而 SAS 只有 256、SATA 只有 32。NVMe 预期 I/O 延迟 **< 20 μs**。

### §9.4.3 存储类型

四种架构：**磁盘设备**（JBOD，最容易分析，每块盘操作系统都认识）、**RAID**、**存储阵列**、**NAS**。

**RAID 性能要点（Table 9.3）**：

| 级别 | 性能特点 |
|---|---|
| 0（concat） | 一块一块填；多盘参与后随机读会变好 |
| 0（stripe） | **随机与顺序 I/O 最好**（取决于条带大小与负载形态） |
| 1（mirror） | 读可以多盘并行；**写受最慢那块盘限制**，且吞吐开销翻倍 |
| 10 | RAID-1 分组上做 RAID-0，兼得容量与冗余 |
| 5 | **写性能差**（read-modify-write + 校验计算） |
| 6 | 比 5 更差 |

- **硬件 RAID vs 软件 RAID**：硬件用专用电路算校验、还带电池；软件 RAID 现在因为 CPU 变快而重新流行（ZFS），**可观测性更好、坏了更好修**（想象一下 RAID 卡整个坏了）。
- **Read-Modify-Write**：RAID-5 的小块写要先读回整条带、算校验、再重写。**跨越整条带的写可以直接覆盖，不用先读**——所以调优要点是**让条带大小和平均写尺寸匹配**。
- **存储阵列的两个坑**：**巡检读（patrol read）**——每隔几天把所有块读一遍校验，和业务抢资源；**缓存刷盘间隔（cache flush interval）**——间隔长能减少磁盘 I/O（写取消 + 写聚合），但**刷盘那一刻的读延迟会更高**。
- **NAS**：走网络（NFS / SMB / iSCSI），**网络本身也成了性能因素**。

### §9.4.4 操作系统的磁盘 I/O 栈

**Linux 块 I/O 栈**（Figure 9.8）上多出来的三样东西：

1. **I/O merging（合并）**：Linux 可以把相邻请求**合并或聚合**（Figure 9.9），减少每 I/O 的内核开销，也减少盘的开销。合并前后的统计在 `iostat` 里能看到。
2. **I/O 调度器**：经典调度器（Noop / Deadline / CFQ）只存在于 5.0 之前的 Linux；**多队列调度器**是新的。`blk-mq`（Linux 3.13）给每个 CPU 一条提交队列、给设备多条派发队列。Linux 5.0 起**多队列调度器是默认**。其中 **Deadline** 用**三条队列**（读 FIFO、写 FIFO、排序队列）解决**饿死**问题；**Kyber** 只有两个可调项（`read_lat_nsec` / `write_lat_nsec`），按性能反馈调读/写派发队列长度，**Netflix 云默认用它**。
3. **卷管理器与 device mapper**（创建虚拟设备）。

调度完之后，请求被放到块设备队列上，下发给设备。

---

## 9. §9.5 Methodology：七种方法与它们的顺序

作者给了七种方法（Table 9.4），并给了**明确的先后顺序**：

> These methods may be followed individually or used in combination. When investigating disk issues, my suggestion is to use the following strategies, in this order: the USE method, performance monitoring, workload characterization, latency analysis, micro-benchmarking, static analysis, and event tracing.

**USE 方法 → 性能监控 → 工作负载刻画 → 延迟分析 → 微基准 → 静态分析 → 事件追踪。**

### §9.5.1 Tools Method（工具法）

> The tools method is a process of iterating over available tools, examining key metrics they provide.

简单直接，但**会漏掉工具看不见的问题**，而且很费时间。给磁盘的检查清单（Linux）：

| 工具 | 用来查什么 |
|---|---|
| `iostat` | 扩展模式找**忙盘**（> 60%）、高服务时间（> 10 ms）、高 IOPS |
| `iotop` / `biotop` | **哪个进程**在搞磁盘 I/O |
| `biolatency` | I/O 延迟的**直方图分布**，找多峰与离群点（> 100 ms） |
| `biosnoop` | **逐条**看 I/O |
| `perf(1)` / BCC / bpftrace | 自定义分析，包括**看是哪个用户态/内核栈发出了这个 I/O** |
| 厂商工具 | 控制器专属指标 |

### §9.5.2 USE Method

**对每个磁盘设备查三项**：Utilization（忙不忙）、Saturation（I/O 在队列里排的程度）、Errors（设备错误）。

⚠️ **错误可以先查**——它们常常被忽略，因为系统**还能正常工作，只是慢一点**：磁盘通常配在冗余池里，容忍部分故障。除了操作系统的标准错误计数器，设备还支持更多专用错误计数器（SMART 数据）。

**对每个磁盘控制器查三项**：Utilization（**当前 vs 最大**吞吐与操作率，⚠️ 这里不是时间口径）、Saturation、Errors。⚠️ 但控制器和传输通道**很少是瓶颈**——它们的容量通常超过挂在上面的盘。**一个诊断线索**：如果总吞吐或总 IOPS **无论什么负载都稳定卡在某个值**，那可能就是控制器或总线在限制。

### §9.5.3 性能监控

关键指标：**盘利用率**、**响应时间**。利用率连续多秒 100% 几乎肯定是问题；> 60% 也可能因为排队变差。指标要**按盘看**（找负载不均和单盘异常），响应时间可以**按秒平均**，也可以带**最大值和标准差**，理想情况是**看完整分布**（直方图或热力图）找离群点。

### §9.5.4 工作负载刻画

基本属性五个：**I/O 速率（IOPS）、I/O 吞吐、I/O 尺寸、读写比、随机 vs 顺序**。

原文给了一个把五项揉成一段话的**范例**（值得照抄这个句式）：

> The system disks have a light random read workload, averaging 350 IOPS with a throughput of 3 Mbytes/s, running at 96% reads. There are occasional short bursts of sequential writes, lasting between 2 and 5 seconds, which drive the disks to a maximum of 4,800 IOPS and 560 Mbytes/s. The reads are around 8 Kbytes in size, and the writes around 128 Kbytes.

⚠️ **这些特征会秒级变化**（尤其是会缓冲和定时刷写的应用与文件系统），所以**要抓峰值，也要抓平均值**，最好看完整的时间分布。

后面还有一长串进阶清单（谁在用盘、访问什么文件系统、有没有错误、各盘负载是否均衡、每条总线各多少 IOPS/吞吐、发了哪些非数据传输命令、I/O 是从哪条内核路径发的、I/O 有多同步、到达时间的分布）。

**性能刻画**（结果侧）问的是：每块盘多忙、饱和程度、平均服务时间、平均等待时间、有没有高延迟离群点、**完整的延迟分布**、有没有 I/O 节流在生效、非数据传输命令的延迟是多少。

**事件追踪**能记录全部文件系统操作：设备 ID、I/O 或命令类型、offset、尺寸、下发与完成时间戳、完成状态、发起进程。有下发与完成两个时间戳就能算延迟，还能看出**设备侧的重排序**。⚠️ 但捕获与保存本身**有明显开销**；更糟的是——

> If the disk writes for the event trace are included in the trace, it may not only pollute the trace, but also create a feedback loop and a performance problem.

**追踪用的写如果也被记进 trace，就会形成反馈回路**——追踪本身把系统写慢了。

### §9.5.5 延迟分析

> Latency analysis involves drilling deeper into the system to find the source of latency.

对磁盘来说，**这通常会终结在磁盘接口**：I/O 请求与完成中断之间的时间。**如果这一层的延迟与应用层的 I/O 延迟相同**，那基本可以断定延迟源自磁盘，可以把调查聚焦到盘上。**如果不同**，就要在操作系统栈的不同层分别测量，才能定位来源。

Figure 9.10 用两个离群点说明这件事：**A** 在每一层的延迟都相似 ⇒ 指向磁盘（或磁盘驱动）；**B** 看起来源自**文件系统层**（锁？排队？），下层贡献的时间少得多。⚠️ B 的例子可能是"下层只观察到了一个 10 ms 的 I/O，但没算上为同一个文件系统 I/O 服务的其它相关 I/O（比如元数据）"。

三个呈现粒度：**每区间平均**（OS 工具的常态）、**完整分布**（直方图/热力图）、**每条 I/O 的值**（事件追踪）。

### §9.5.6–9.5.10 静态调优、缓存调优、资源控制、微基准、扩容

- **静态性能调优**是一张检查表：几块盘、什么型号、固件版本、几个控制器、插在什么插槽、每个 HBA 接几块盘、**电池备电的电量**、RAID 怎么配（含条带宽度）、多路径、驱动版本、**主内存多大、有多少被页缓存和 buffer cache 占了**、有没有相关补丁、有没有 I/O 资源控制。
  - 原文的实战教训（Sun ZFS 时期）：**最常见的性能投诉是配置错了**——用了半个 JBOD（12 块盘）的 RAID-Z2，条带很宽，性能"和单盘差不多"。作者的结论：**先问配置，再登机器看延迟**。
- **缓存调优**：先查**有哪些缓存**、**它们在不在工作**、**工作得好不好**，然后**为缓存调负载、为负载调缓存**。
- **资源控制**：操作系统可以给进程/进程组设 I/O 资源的固定上限或份额（IOPS、吞吐，读写可分开设）。
- **微基准**（测的是**盘**，不是文件系统）：最好走**裸设备路径**，绕开所有文件系统行为（缓存、缓冲、I/O 拆分与合并、代码路径开销、offset 映射差异）。六个因子：**方向、offset 形态（随机/顺序）、offset 范围（全盘 or 只打 offset 0）、I/O 尺寸（512 B 到 1 MB）、并发数、盘的数量**。
  - 单盘要测的五项：最大吞吐（128 KB 或 1 MB 顺序读）、最大操作率（512 B 读、**offset 0 only**）、最大随机读 IOPS、读延迟剖面、随机 I/O 延迟剖面。
  - **"offset 0 only" 的用意是让数据待在盘上缓存里**，从而量到缓存访问时间。⚠️ 原文还带了个传闻：有些厂商固件会**加速 sector 0 的 I/O**，让这种测试虚高——**测 sector 0 vs 测你喜欢的那个数**。
  - 测控制器要**一块一块加盘**，"可能要十几块盘才能撞到控制器的上限"。
- **扩容**：**不要用每块盘的最大吞吐和 IOPS 来算**——

> Do not use the maximum throughput and IOPS values per disk, as this would result in driving disks at 100% utilization, leading to immediate performance issues due to saturation and queueing.

**定一个目标利用率（比如 50%），再按它折算**。然后算盘数（算上 RAID）、算控制器数、检查总线上限（不够就扩总线）、算每 I/O 需要多少 CPU 周期、需要几颗 CPU。

> We used to say, “Add more spindles.” We may now say, “Add more flash.”

---

## 10. §9.6 Observability Tools：Linux 上够用的那一套

### §9.6.1 iostat（第一个该用的命令）

内核默认开着这些统计，所以**开销可以忽略**。名字叫 "iostat" 是 "I/O statistics" 的缩写，⚠️ 但作者吐槽"叫 diskiostat 更合适"，于是经常有人困惑：**"应用明明在做 I/O，为什么 iostat 看不到"**——因为 iostat 报的是**盘**，不是文件系统。

默认输出是**自开机以来的汇总**（作者明说这个模式不用学，扩展模式才有用）。扩展短输出（`-sxz`）：

```
$ iostat -sxz 1
avg-cpu:  %user   %nice %system %iowait  %steal   %idle
          15.82    0.00   10.71   31.63    1.53   40.31

Device             tps      kB/s    rqm/s   await aqu-sz  areq-sz  %util
nvme0n1        1642.00   9064.00   664.00    0.44   0.00     5.52 100.00
```

各列：

| 列 | 含义 | 用途 |
|---|---|---|
| `tps` | 每秒事务数（IOPS） | 工作负载刻画 |
| `kB/s` | KB/s | 吞吐 |
| `rqm/s` | 每秒被**排队并合并**的请求数 | 合并程度，也提示顺序负载 |
| **`await`** | **平均 I/O 响应时间（含 OS 排队 + 设备响应）** | **交付性能的第一指标** |
| `aqu-sz` | 驱动请求队列 + 设备上活跃请求的**平均条数** | **饱和度（队列深度）** |
| `areq-sz` | 平均请求大小（KB） | 合并**之后**的尺寸 |
| `%util` | 设备忙的时间占比 | 利用率 |

> The most important metric for delivered performance is await , showing the average total wait time for I/O.

**`await` 是交付性能最重要的指标。** 好坏取决于需求——上例 0.44 ms 对那个数据库服务器是满意的。它会变大的原因有四类：**排队（负载）、更大的 I/O 尺寸、机械盘上的随机 I/O、设备错误**。

⚠️ `%util` 只是**忙闲**的度量，对**多盘虚拟设备**可能意味不大——那种设备更该看**施加的负载**（`tps` 与 `kB/s`）。

⚠️ `areq-sz` 是**合并之后**的尺寸：**小（≤ 8 KB）说明是合不掉的随机 I/O**；大可能是大 I/O，也可能是合并出来的顺序负载（看前面的列）。

完整 `-x` 还会把读和写**拆开**（`r/s`、`w/s`、`r_await`、`w_await`、`rareq-sz`…），并包含 **discard 与 flush** 统计（Linux 4.19 加 discard，5.5 加 flush）。

> Examining reads and writes separately is important.

**读写必须分开看**：应用和文件系统常用写回缓存来缓解写延迟，**应用很少被写阻塞**。合在一起的指标被"可能根本不相关的写"带偏了。拆开之后先看 `r_await`，它多半才是应用性能最重要的那个数。

### §9.6.2 sar

`sar -d 1` 给出与 `iostat -x` 相同的列，可以**存档做历史回看**。⚠️ 老版本里的 `svctm`（服务时间）列**已被移除**——因为那个"推断出来的服务时间"对现代并行处理的盘已经不准了（正好呼应 §9.3.1 里那个 `utilization/IOPS` 公式的不准）。

### §9.6.3 PSI

Linux 4.20 加的 **pressure stall information**，含 I/O 饱和度统计：

```
# cat /proc/pressure/io
some avg10=63.11 avg60=32.18 avg300=8.62 total=667212021
full avg10=60.76 avg60=31.13 avg300=8.35 total=622722632
```

> These averages are percentages of time that a task was I/O stalled.

`some` = 有**部分**任务被 I/O 卡住；`full` = **全部**可运行任务都被卡住。上例 10 秒均值（63.11）远高于 300 秒均值（8.62）⇒ **I/O 压力正在上升**。和负载均值一样，它适合**做高层告警指标**；发现问题了再换细粒度工具去查根因。

### §9.6.4 pidstat

`pidstat -d 1` 按进程给磁盘统计。列有 `kB_rd/s`、`kB_wr/s`、`kB_ccwr/s`（**被取消的写**——还没刷盘就被覆盖或删除了）、**`iodelay`**（进程被磁盘 I/O 阻塞的时间，时钟滴答）。

> The tar reads caused iodelay (5 clock ticks), whereas the gzip writes did not, due to write-back caching in the page cache.

原文的例子很有说服力：`tar` 读文件被卡了，`gzip` 写却没被卡——**因为页缓存的写回缓存**。过一会儿页缓存被刷盘，`kworker/u4:1-flush-259:0` 这个刷盘工作进程身上才出现 iodelay。

> iodelay is a recent addition and shows the magnitude of performance issues: how much the application waited.

### §9.6.5–9.6.15 其余工具（速览）

**perf(1)**（录块层 tracepoint）、**biolatency**（延迟分布直方图）、**biosnoop**（逐条 I/O）、**iotop / biotop**（谁在搞 I/O）、**biostacks**（I/O 与调用栈）、**blktrace**（块层事件全量追踪）、**bpftrace**（自定义追踪）、**MegaCli / smartctl**（厂商侧：RAID 卡状态、SMART 数据）、**SCSI Logging**。

---

## 11. §9.7 Visualizations：别只画折线

### §9.7.1 折线图

监控产品最常画 IOPS / 吞吐 / 利用率随时间的折线。但作者先泼了一盆冷水：

> Note the metric that is graphed. Average latency can hide multi-modal distributions, and outliers. Averages across all disk devices can hide unbalanced behavior, including single-device outliers. Averages across long time periods can also hide shorter-term fluctuations.

**三句"平均会骗人"**：平均延迟藏双峰分布和离群点；跨盘平均藏单盘异常；长时间平均藏短时波动。

### §9.7.2 延迟散点图

横轴完成时间、纵轴 I/O 响应时间，**每个事件一个点**。Figure 9.11 是生产 MySQL 的 1,400 个事件。

原文的诊断过程值得学：散点图上有几个读离群点（> 150 ms），**原因原先不知道**；看了多张类似图才发现**它们都出现在一阵写之后**——写延迟低是因为**RAID 控制器的写回缓存立刻返回了完成**，真正的落盘在后面，所以**读排在设备的写后面**。

⚠️ 散点图的规模上限：**多台机器或更长时间 ⇒ 事件多到糊成一片**。到那一步就换热力图。

### §9.7.3 延迟热力图

横轴时间、纵轴延迟、颜色表示该区间的 I/O 数量（越深越多）。**优势是能扛百万级事件**，散点图到那个量级就成"油漆"了。

Figure 9.12 是作者自己做的实验（顺序读一块块盘加上去，探索总线与控制器上限），结果形状像**翼龙（pterodactyl）**——每个细节都有技术原因："喙"停在 8 块盘 = SAS 端口数（两个 x4），"头"从第 9 块盘开始 = 端口开始争用。

### §9.7.4–9.7.5 另外两种

- **Offset 热力图**（Figure 9.13，1995 年 Richard McDougall 的 taztool）：纵轴是块地址、横轴是时间。**深色线 = 顺序 I/O，浅色云 = 随机 I/O**。
- **利用率热力图**：纵轴是每设备的利用率百分比，**用来找单块热盘和 sloth 盘**（它们是热力图顶部 100% 处的横线）。

---

## 12. §9.8 Experimentation：主动把盘压满

> When using these tools, it’s a good idea to leave iostat(1) continually running so that any result can be immediately double-checked.

**跑微基准时让 iostat 一直开着**——任何结果都能立刻交叉验证。⚠️ 有些微基准需要"直写"模式绕开文件系统缓存。

- **§9.8.1 dd(1)**：顺序测速。⚠️ `dd` 测到的可能是**缓存和盘**的混合吞吐，要只测盘得用裸设备（Linux 上可用 `raw(8)` 在 `/dev/raw` 下建字符设备）。更安全的做法是对**文件**用 `oflag=direct`（但会带上文件系统开销）。
- **§9.8.2 自写负载生成器**：一个短 C 程序打开设备路径施加负载；用 `O_DIRECT` 避免缓冲。⚠️ 用高级语言时也要**避开库的缓冲**（Perl 的 `sysread()`），最好连内核缓冲一起避开。
- **§9.8.3 hdparm(8)**：`-T` 测缓存读，`-t` 测设备读。原文示例里 **8367 MB/s vs 281 MB/s**——**这就是缓存命中与未命中的差别**（正对着 §9.3.2 的双峰分布）。
- **§9.8.4 随机读示例**：就是开头那五行。原文的点评是"注意 `aqu-sz` 的阶梯式上升和 `r_await` 的上升"。
- **§9.8.5 ioping(1)**：像 ping 一样，每次发一个 4 KB 读、打印微秒级延迟。

> The disk was driven to only 0.4% utilization.

**ioping 的最大优点是负载极轻**——别的微基准会把盘压到 100%，不适合在生产环境上跑；ioping 可以。

- **§9.8.6 fio**：文件系统基准，`--direct=true` 可以顺便照见盘设备性能。
- **§9.8.7 blkreplay**：回放 blktrace 抓下来的块 I/O 负载，适合**微基准搞不定的疑难问题**。⚠️ 但要小心第 12 章那个教训——**回放会骗人**（目标系统变了，缓存行为变了）。

---

## 13. §9.9 Tuning：三个层次

> While changing tunables can be easy to do, the default settings are usually reasonable and rarely need much adjusting.

**调参很容易改，但默认值通常已经合理，很少需要大动。** 三个层次：

- **§9.9.1 操作系统**：`ionice(1)`（调度类 0 none / 1 real-time / 2 best effort / 3 idle；`ionice -c 3 -p 1623` 把进程放进 idle 类，**适合长时间备份任务**）；**cgroups 的 blkio 子系统**（按 IOPS 或吞吐设份额/上限，读写可分开）；内核可调项 `/sys/block/*/queue/scheduler`、`nr_requests`（块层可分配的读写请求数）、`read_ahead_kb`（文件系统最多能请求多少预读）。
- **§9.9.2 磁盘设备**：`hdparm(8)` 能设电源管理与停转超时。⚠️ **小心**，手册里标了 "DANGEROUS" 的选项会导致数据丢失。
- **§9.9.3 磁盘控制器**：型号相关。原文列了一份 Dell PERC 6 的 `MegaCli` 输出，其中最值得注意的两项是 **`Cache Flush Interval`（默认 30s）**——缓存脏数据刷盘的间隔——和 **`Rebuild Rate`（默认 30%）**。

---

## 14. 实战锚点：otf-server 面对"磁盘"这件事

> **诚实前提（先说清楚，别被后面的话误导）**：**本仓库没有任何磁盘 IO。**
>
> `src/` 全目录 grep：`ofstream` / `fopen` / `fwrite` / `fsync` / `fdatasync` **零命中**。
> 唯一的"输出"是 `src/common/logger.cpp` 的两处 `std::fprintf(stderr, ...)`（第 100 行与第 140 行），
> 以及 `src/main.cpp` 的 `std::printf`（启动时的握手信息打印）。
> **连配置文件都不读**——`src/config/server_config.h` 的文件头明写"本仓库**没有 yaml 加载器**（零第三方依赖是硬约束，PyYAML 禁用）"，
> 所有参数是编译期常量。
>
> ⇒ **这一章的绝大多数内容在本仓库是空白的。** 下面写的是**能对应上的那一部分**，以及**对应不上但值得记下的缺口**。

### 14.1 §9.3.3 缓存 —— 本仓库把它做到了极致

> The best disk I/O performance is none at all.

本仓库的"缓存"不是磁盘缓存，是**进程内的几个复用缓冲**：

```cpp
// src/room/room_mgr.h:131
std::vector<std::uint8_t> snapshot_scratch_;
std::vector<std::uint8_t> event_scratch_;
```

`RoomManager::FlushSnapshots`（`src/room/room_mgr.cpp:265-267`）对每个房间 `snapshot_scratch_.clear()` 后复用同一个缓冲。代码注释把理由写清楚了：

> ★ 复用成员缓冲，而不是每轮 new 一个局部 vector。
>   旧写法是每房间每快照一次堆分配（10Hz × 房间数）+ 每事件一次 ——
>   32 间房就是每秒 320 次纯浪费。

**这就是 §9.3.3 那个"最好的 IO 性能是不做 IO"在内存侧的样子。**

⚠️ **但复用一个、漏了另一个**。`BuildSnapshotFor` 里：

```cpp
// src/room/room_mgr.cpp:239
proto::SnapshotView view;
```

`SnapshotView` 是**局部变量**，而它的 `players` 字段是 `std::vector<PlayerNetView>`（`src/proto/snapshot.h:60`）。⇒ **每次 `BuildSnapshotFor` 至少一次堆分配**（`players` 首次 `push_back` 时），而且因为 `view` 每轮新建，**这个 vector 的 capacity 也每轮重新申请**。

10 Hz × 32 房间 = **每秒 320 次 malloc/free**。字节缓冲复用了，**视图结构没复用**。（推演）这是很典型的"优化做了一半"：最贵的那次分配被省掉了，紧接着的第二次没有。

### 14.2 §9.3.1 时间口径 —— `tick_drift_max_ms` 为什么必须写清起止事件

原文的核心要求是：**说"延迟"必须说清"从哪个事件到哪个事件"**。本仓库在这个要求上比原文还严格。

`src/net/metrics.h` 的文件头与 `on_tick_drift` 注释里，作者（仓库作者）把口径、符号、以及"为什么这么算"逐条写了出来：

```cpp
///   ⚠️ 为什么按 |·| 取峰而不是按 `v > cur`：
///     漂移是**有符号**的（sim 超前 = 负 / 落后 = 正，两种都会导致双端不同步），
///     而初值 0 + `v > cur` 的写法会让负半轴**全部静默丢弃** ——
///     那不是「负漂移不重要」，是「恰恰最危险的那一半被吞掉，还显示成健康的 0」。
```

这与 §9.3.1 结尾那句"这个算式假设了设备一次只能处理一个 I/O，所以**不准确**"是同一个精神：**反推出来的指标必须写清它的假设，否则就是个骗人的数字。**

⚠️ 仓库还写了一条与 §9.3.9 完全同构的纪律（`src/net/metrics.h` 原文注释）：

> ⚠️ **nullopt = 一次都没记录过**，与「记录到的峰值恰好是 0」是两回事。

> ⚠️ 一次都没记录过的 `otf_tick_drift_max_ms` 会**整行缺席**（而不是输出 0）——
>   「没测过」与「测过，正好是 0」在告警上是两件事，输出 0 会把前者说成后者。

**这正是原文那句"‘没测过’和‘测过且刚好是 0’是两件事"的实现。**

### 14.3 §9.3.9–9.3.10 利用率与饱和度 —— 本仓库**没有**饱和度指标（诚实的缺口）

原文说：**100% 利用率既可能毫无排队，也可能排得很深；区分二者要靠队列深度。**

本仓库的硬上限都写成了具名常量（`src/proto/protocol.h`）：

| 常量 | 值 | 含义 |
|---|---|---|
| `kMaxPayloadSize` | 1400 | 单帧 payload 上限 |
| `kFrameQueueDepth` | 64 | 每连接帧队列深度 |
| `kSendBufCap` | 128 KB | 每连接待发缓冲上限 |
| `kMaxFd` | 1024 | fd 上限 |
| `kConnectionTimeoutMs` | 5000 | 5 秒无任何包判死 |

⚠️ **但没有任何一个"当前水位"的指标。** `src/net/metrics.h` 的 `MetricsSnapshot` 里有连接数、房间数、收发帧数、心跳超时数、超长帧拒绝数、tick 漂移峰值、快照发送延迟峰值、epoll_ctl 计数——**没有一个是"当前发送缓冲占了多少"或"帧队列现在多长"**。

⇒ **本仓库看得见"满了会怎样"（超限就断连），看不见"离满了还有多远"。** 这是 §9.3.10 意义上的饱和度缺失：**要等它真的爆了才知道自己快爆了。**

（推演）最小改法：`Metrics` 加一个 `send_buf_bytes` / `send_buf_high_water` 计数器，`Send()` 里顺手加一下。它不贵，却能把"要断连了"从**事后**变成**事前**——这正是 §9.3.9 说的"在 0% 和 100% 之间那个 60% 的点上你就要发现问题了"。

### 14.4 §9.3.11 I/O Wait —— 本仓库的对应物是 `tick_drift_max_ms`

原文警告：**I/O wait 会因为 CPU 变忙而下降，故障却还在。**

本仓库有一个**形状完全相同**的指标。主循环（`src/main.cpp`）：

```cpp
const std::int64_t now = SteadyNowMs();
transport.Poll(sched.PollTimeoutMs(now));            // ← 唯一的 IO 等待点
const std::int64_t post_poll = SteadyNowMs();        // ★ Poll 阻塞过，now 已过期
...
const std::int64_t drift = sched.DriftMs(post_poll);
metrics.on_tick_drift(drift);
```

★ **`post_poll` 存在的理由**（`main.cpp` 注释原文）：

> ★ 重新取时间：Poll 阻塞过，Poll 之前取的 now 已经过期。
>   拿旧 now 算补帧会少追帧 —— 症状是「逻辑帧率随负载下降」，
>   正是 §6.3 警告的那类漂移，只是从另一个方向来的。

**这是"等 IO 的时间"在本仓库的落点。** 和原文的 I/O wait 一样，它**会骗人**——如果主循环忙到 `Poll` 立刻返回，漂移就接近 0，但"客户端收不到帧"这件事并没有消失。

⚠️ 所以仓库的处置方式和原文一致：**不指望单个指标**，而是加一个**事件**——补帧上限生效时打一条 `log::warn_f`：

```cpp
if (r.dropped > 0) {
    log::warn_f("补帧上限生效：丢弃 " + std::to_string(r.dropped) +
                      " 帧（本次跑了 " + std::to_string(r.steps) + " 帧）",
                  log::Fields{.tick = static_cast<std::int64_t>(sched.tick_index())});
}
```

**指标给趋势，事件给现场。** 这与 §9.3.11 的结论一致：单看一个平均量不够，要看**分布与离群点**。

### 14.5 §9.3.12–9.3.13 同步 vs 异步、盘 IO vs 应用 IO —— 一个真实的"慢 IO 打死全场"

原文讲：**一次慢 I/O 可以毁掉整个应用响应**；而慢 I/O 的来源往往是"内核路径上的意外"。

本仓库里有一个**结构完全同构**的东西，但触发源不是磁盘而是网络：

`kConnectionTimeoutMs = 5000` —— **5 秒内没收到任何包就判死**。判死发生在 `SessionManager::Tick`（`src/session/session_mgr.cpp:506-514`），也就是**主循环的一拍里**。

⇒ （推演）**如果主循环卡住超过 5 秒**，那么**所有连接会在同一拍里被一起判死**——不是"某个慢客户端超时"，是"全场掉线"。这正是原文那句"at the slowest end of the scale, poor application response time can be caused by a single slow disk I/O"的镜像：**一个慢操作足以让整个系统的响应崩掉**。

⇒ 而本仓库**恰恰有一条现成的手段**能主动制造这个场景并验证观测是否有效：`OTF_DEBUG_STALL_MS` 环境变量会在主循环里做一次性 `nanosleep` 注入卡顿（`main.cpp`）。⚠️ 但注入的是 **10 ms 级别的卡顿**，**远不到 5 秒**，所以它验证的是**补帧上限**这条路径，**验证不了"全场判死"这条路径**。⚠️ **这是一个真实的观测缺口**：心跳判死这条最狠的路径，**没有故障注入覆盖**。

### 14.6 §9.3.8 非数据传输命令 & §9.4.3 刷盘间隔 —— 仓库唯一的落地点是 stderr

`std::fprintf(stderr, ...)` 写的是 **fd 2**，不是文件。**stderr 在 C 标准里是不完全缓冲的**（实际上通常是立即 write(2)），所以每条日志是一次系统调用。

- 如果 fd 2 被重定向到**文件**：这就是**一次 write(2) 进页缓存**，由内核按 write-back 节奏刷盘。⇒ **`fsync` 从不被调用** ⇒ 断电时最后一段日志丢失，**丢失多少取决于内核的脏页刷写节奏，而不是取决于你**。
- 如果 fd 2 接的是 **journald / 管道**：那就没有磁盘 IO 了（本仓库的默认部署形态）。

⇒ **本仓库与"磁盘"唯一的接触点，就是这条日志路径；而这条路径上，本仓库对落盘时机完全无知。** §9.4.3 里 Dell PERC 那个 `Cache Flush Interval`（默认 30 秒）的概念，在这里**没有对应物，也没有人需要它**——因为没人要求日志"此刻必须落盘"。

（推演）如果哪天真要给这个服务器加"日志必须落盘"的保证（比如审计要求），那么要加的就是 §9.2.2 里的 **write-through** 语义：`fflush` + `fsync(fd)`，**并且必须写清楚它在哪一拍执行**——因为 `fsync` 的延迟是**毫秒到秒级**的（原文 Table 9.1 的最坏情况 > 1,000 ms），在**单线程 reactor** 里做这件事，等于**让整个游戏停一拍**（原文：Under high load, disks become a bottleneck, leaving CPUs idle as the system waits for disk I/O to complete）。⇒ **正确的做法是把它挪出主线程，而不是在主循环里加一次 `fsync`。**

### 14.7 §9.6 整节 —— 本仓库在这一章的观测面是空的

| 原章工具 | 本仓库 |
|---|---|
| `iostat` / `sar` / `pidstat` / `iotop` | **无**（本仓库无磁盘 IO，但**同样没有 `iostat` 式的 IOPS/吞吐/利用率**） |
| PSI（`/proc/pressure/io`） | **无对应物**——但 `tick_drift_max_ms` 在功能上接近（"任务被卡住的时间占比"的代理） |
| `biolatency` / `biosnoop` / `blktrace` / `bpftrace` | **零**（全仓无 perf / eBPF / Ftrace 集成） |
| `MegaCli` / `smartctl` | **无**（无磁盘） |

本仓库有的只有**一个应用内 HTTP 指标端点**（`src/net/metrics.cpp` / `metrics_server.cpp`），暴露的是固定计数器与峰值。

⇒ 按 §9.5.1 的"工具法"清单打分：**本仓库的磁盘工具法是空的，按网络工具法（Ch10）打分也大半是空的。** 它只有**应用内埋点**这一层。

### 14.8 §9.5 的七步方法学 —— 本仓库的对照表

| 原章步骤 | 本仓库现状 |
|---|---|
| USE 方法 | **部分有**：Utilization ≈ `tick_drift_max_ms`（有）；Saturation ≈ **缺**；Errors ≈ 有（`heartbeat_timeouts` / `oversize_rejects`） |
| 性能监控 | 有（指标端点），但**只有快照值，没有历史序列**（`sar` 的归档能力没有） |
| 工作负载刻画 | **缺**——没有"读/写比、I/O 尺寸、随机/顺序"这套刻画。对应物是"上行 30 Hz 输入 / 下行 10 Hz 快照"，但**没有被刻画成指标** |
| 延迟分析 | **部分有**：`snapshot_send_ms_max`、`tick_drift_max_ms` 都是**峰值**，**没有分布、没有离群点**（原章明确说峰值会藏离群点） |
| 微基准 | 有（`tests/bench_data_path.cpp`，见第 12 章讲义） |
| 静态分析 | 有（`src/config/game_config.h` 是参数唯一产地） |
| 事件追踪 | **无**——但仓库有 `log::warn_f` 的**结构化字段**（fd / player_id / room_id / tick），**这是追踪的雏形** |

### 14.9 本章的一句话行动清单

1. **`BuildSnapshotFor` 里的 `proto::SnapshotView view;` 改成成员**（或复用一份 scratch view），消掉每秒 320 次 `players` vector 的堆分配——**这是 §9.3.3「最好的 IO 性能是不做 IO」在内存侧的最后一个漏口**。
2. **加一个发送缓冲水位指标**（`send_buf_bytes` + high-water mark）——把 §9.3.10 的饱和度从"看不见"变成"看得见"。
3. **心跳判死路径补一个故障注入用例**（把 `OTF_DEBUG_STALL_MS` 的量级覆盖到 > 5 秒，或新增一个独立开关）——验证"一次卡顿打死全场"这条最狠的路径有没有被观测到。
4. **`tick_drift_max_ms` 从"峰值"升级为"分布"**（至少给一个直方图）——§9.3.2 说过，**平均值是双峰分布的谎言**。
5. **明确写下"日志的落盘语义"**：stderr 是无缓冲的、**没有 fsync**、断电会丢多少——把这条写进运维文档，而不是留在读者的脑子里。

---

## 15. 收尾：本章金句清单

1. **Disk I/O can cause significant application latency, and is therefore an important target of systems performance analysis.** —— 全章的第一句话，也是全章存在的理由：磁盘 I/O 是性能分析的**主要目标物**。
2. **Under high load, disks become a bottleneck, leaving CPUs idle as the system waits for disk I/O to complete.** —— 瓶颈的形状是"**CPU 在等**"，不是"CPU 在算"。
3. **The best disk I/O performance is none at all.** —— **整章的价值观**：最好的优化是让那次 I/O **不必发生**。
4. **An IOPS value on its own doesn’t mean a lot.** —— IOPS 必须带五个限定才有意义（随机/顺序、尺寸、读写、缓冲/直写、并发）。
5. **Because of those last three characteristics, IOPS are not created equal and cannot be directly compared between different devices and workloads.** —— **"IOPS 不可比"**——这一章最容易被忽略、也最贵的常识。
6. **disk service time = utilization/IOPS** —— 一个能手算的反推公式，**但只在设备一次只能处理一个 I/O 时成立**。
7. **There may also be a point between 0% and 100% (say, 60%) at which the disk’s performance is no longer satisfactory due to the increased likelihood of queueing, either on-disk queues or in the operating system.** —— **60% 就该警惕**——问题不在占用，在**排队**。
8. **A disk at 100% utilization may have no saturation (queueing), or it may have a lot, significantly affecting performance due to the queueing of I/O.** —— **100% 利用率什么也说明不了**；能说明问题的是**队列深度**。
9. **Once a physical disk reaches 100% utilization and more I/O is requested, it becomes saturated.** —— 饱和的**定义点**在这里：不是"忙"，是"忙而且还有人在排"。
10. **I/O wait can be a very confusing metric.** —— 一个**会骗人**的指标：CPU 一忙它就降，故障却还在。
11. **These disk commands can affect performance and can cause a disk to be utilized while other I/O wait.** —— **非数据传输命令也会让盘"显示为忙"**——所以 `%util` 单独看会误判。
12. **Disks may be busy due to hardware RAID rebuild, with no corresponding I/O seen by the operating system.** —— **盘忙 ≠ 有 I/O**；看不见的那部分最可疑。
13. **This mismatch can be confusing when unexpected. It can be understood by learning the architecture and performing analysis.** —— "盘上的 I/O 和应用发的不一样"是**常态**，不是故障。
14. **Latency analysis involves drilling deeper into the system to find the source of latency.** —— 延迟分析的落点通常是**磁盘接口**；**各层延迟一致 ⇒ 指向盘，不一致 ⇒ 去分层测**。
15. **The most important metric for delivered performance is await , showing the average total wait time for I/O.** —— `await` 是交付性能的第一指标；**但平均值会藏双峰与离群点**。
16. **Note the metric that is graphed. Average latency can hide multi-modal distributions, and outliers.** —— 画图之前先问"我画的是什么指标"——**三句"平均会骗人"**。
17. **Do not use the maximum throughput and IOPS values per disk, as this would result in driving disks at 100% utilization, leading to immediate performance issues due to saturation and queueing.** —— 容量规划**不能按最大值算**，要按目标利用率（比如 50%）折算。
18. **We used to say, “Add more spindles.” We may now say, “Add more flash.”** —— 一句话讲完存储代际。
