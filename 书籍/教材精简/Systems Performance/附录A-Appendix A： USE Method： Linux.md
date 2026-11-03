---
title: "附录A-Appendix A： USE Method： Linux（精读）"
category: 性能工程
tags:
  - USE方法
  - 性能检查清单
  - 饱和度
  - sar
  - vmstat
  - iostat
difficulty: 进阶
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）附录A USE Method： Linux"
source_section: "精读 §附录 A"
skills:
  - obsidian-markdown
---

# 附录A-Appendix A： USE Method： Linux

> **小节地图**：本篇是**一份清单，不是理论章**。原文只有一级标题（`# Appendix A: USE Method: Linux`），
> 正文是两张大表（Physical Resources / Software Resources），每行回答「某个资源的 USE 三问之一，
> 该用哪条命令、读哪个数字」。本篇按资源的递进顺序重组为十节，每节都按
> **Utilization → Saturation → Errors** 三问展开：
> §附录 A 清单定位、§附录 A 三列读法、§附录 A CPU、§附录 A 内存容量、§附录 A 网络接口、
> §附录 A 存储设备 I/O 与存储容量、§附录 A 控制器与互连、§附录 A 软件资源（锁）、
> §附录 A 软件资源（任务容量与文件描述符）、§附录 A 资源控制环境与术语。
> **不立篇**：§A.1 References（文献条目，不承载机制）。
> 底层依赖：USE 方法的原理在 [[Chapter 2. Methodologies]]；各资源的机制分别在
> [[Chapter 6. CPUs]]、[[Chapter 7. Memory]]、[[Chapter 8. File Systems]]、[[Chapter 9. Disks]]；
> 观测工具链见 [[Chapter 13. perf]] 与 [[Chapter 14. Ftrace]]（第 4 章 Observability Tools
> 与第 10 章、第 15 章在本库尚无同层笔记，故只作文字提及，不造链接）。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。
原文是压平的表格（单元格各占一段），因此引文多为「命令 + 读哪一项」的整段。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §附录 A 清单定位 | 附录A-Appendix A： USE Method： Linux.md L25 | This appendix contains a checklist for Linux derived from the USE method [Gregg 13d] . This is a method for checking system health, and identifying common resource bottlenecks and errors, introduced in Chapter 2 , Methodologies , Section 2.5.9 , The USE Method . |
| §附录 A 清单会过时 | 附录A-Appendix A： USE Method： Linux.md L27 | Performance tools are often enhanced, and new ones are developed, so you should treat this as a starting point that will need updates. New observability frameworks and tools can also be developed to specifically make following the USE method easier. |
| §附录 A CPU · Utilization | 附录A-Appendix A： USE Method： Linux.md L41 | Per CPU: mpstat -P ALL 1 , sum of CPU-consuming columns ( %usr , %nice , %sys , %irq , %soft , %guest , %gnice ) or inverse of idle columns ( %iowait , %steal , %idle ) |
| §附录 A CPU · Saturation | 附录A-Appendix A： USE Method： Linux.md L55 | Per process: /proc/PID/schedstat 2nd field (sched_info.run_delay); getdelays.c, CPU 2 ; perf sched latency (shows average and maximum delay per schedule) |
| §附录 A CPU · Errors | 附录A-Appendix A： USE Method： Linux.md L61 | Machine Check Exceptions (MCEs) seen in dmesg or rasdaemon and ras-mc-ctl --summary ; perf(1) if processor-specific error events (PMCs) are available; e.g., AMD64’s “04Ah Single-bit ECC Errors Recorded by Scrubber” 4 (which can also be classified as a memory device error); ipmtool sel list ; ipmitool sdr list |
| §附录 A 内存容量 · Utilization | 附录A-Appendix A： USE Method： Linux.md L67 | System-wide: free -m , Mem: (main memory), Swap: (virtual memory); vmstat 1 , free (main memory), swap (virtual memory); sar -r , %memused ; slabtop -s c for kmem slab usage |
| §附录 A 内存容量 · Saturation | 附录A-Appendix A： USE Method： Linux.md L75 | System-wide: vmstat 1 , si / so (swapping); sar -B , pgscank + pgscand (scanning); sar -W |
| §附录 A 内存容量 · Saturation（进程级） | 附录A-Appendix A： USE Method： Linux.md L77 | Per process: getdelays.c, SWAP 2; 10th field (min_flt) from /proc/PID/stat for minor fault rate, or dynamic instrumentation 5 |
| §附录 A 内存容量 · Errors | 附录A-Appendix A： USE Method： Linux.md L83 | dmesg for physical failures or rasdaemon and ras-mc-ctl --summary or edac-util ; dmidecode may also show physical failures; ipmtool sel list ; ipmitool sdr list ; dynamic instrumentation, e.g., uretprobes for failed malloc()s (bpftrace) |
| §附录 A 网络接口 · Utilization | 附录A-Appendix A： USE Method： Linux.md L89 | ip -s link , RX/TX tput / max bandwidth; sar -n DEV , rx/tx kB/s / max bandwidth; /proc/net/dev, bytes RX/TX tput/max |
| §附录 A 网络接口 · Saturation | 附录A-Appendix A： USE Method： Linux.md L95 | nstat , TcpRetransSegs ; sar -n EDEV , *drop/s , *fifo/s 6 ; /proc/net/dev, RX/TX drop ; dynamic instrumentation of other TCP/IP stack queueing (bpftrace) |
| §附录 A 存储设备 I/O · Utilization | 附录A-Appendix A： USE Method： Linux.md L107 | System-wide: iostat -xz 1 , %util ; sar -d , %util ; per process: iotop , biotop ; /proc/PID/sched se.statistics.iowait_sum |
| §附录 A 存储设备 I/O · Saturation | 附录A-Appendix A： USE Method： Linux.md L113 | iostat -xnz 1 , avgqu-sz |
| §附录 A 存储设备 I/O · Errors | 附录A-Appendix A： USE Method： Linux.md L119 | /sys/devices/ . . . /ioerr_cnt; smartctl ; bioerr ; dynamic/static instrumentation of I/O subsystem response codes 7 |
| §附录 A 存储容量 · Saturation | 附录A-Appendix A： USE Method： Linux.md L131 | Not sure this one makes sense—once it’s full, ENOSPC (although when close to full, performance may be degraded depending on the file system free block algorithm) |
| §附录 A 内存互连 · Utilization | 附录A-Appendix A： USE Method： Linux.md L197 | perf stat with PMCs for memory buses, tput/max; e.g. Intel uncore_imc/data_reads/,uncore_imc/data_writes/; or IPC less than, say, 0.2; PMCs may also have local versus remote counters |
| §附录 A load average 说明 | 附录A-Appendix A： USE Method： Linux.md L229 | General notes: uptime “load average” (or /proc/loadavg) wasn’t included for CPU metrics since Linux load averages include tasks in the uninterruptible I/O state. |
| §附录 A 资源控制环境 | 附录A-Appendix A： USE Method： Linux.md L239 | For any environment that imposes resource controls (e.g., cloud computing), check USE for each resource control. These may be encountered—and limit usage—before the hardware resource is fully utilized. |
| §附录 A 内核互斥锁 · Saturation | 附录A-Appendix A： USE Method： Linux.md L273 | With CONFIG_LOCK_STATS=y, /proc/lock_stat waittime-total / contentions (also see waittime-min , waittime-max ); dynamic instrumentation of lock functions, e.g., mlock.bt [Gregg 19]; spinning shows up with profiling perf record -a -g -F 99 ... |
| §附录 A 文件描述符 · Utilization | 附录A-Appendix A： USE Method： Linux.md L321 | System-wide: sar -v , file-nr versus /proc/sys/fs/file-max; or just /proc/sys/fs/file-nr |

> **书里没说的**（本篇有几处推论，原文没有，别当成原文观点）：
> - **「三问之间怎么联动」是本篇的说法**。原文把 Utilization / Saturation / Errors 排成三行，
>   没有讨论彼此关系；「利用率高但饱和度低不算有问题」是本篇从清单结构里推出来的。
> - **「`%util` 不等于吞吐」「`await` 高而 `%util` 低 ⇒ 不是排队」是本篇给的组合判据**，
>   原文只并列给出 `%util` 与 `avgqu-sz`/`await` 两个信号，没有做组合解读。
> - **「`free` 里 used 高但 available 多 ⇒ 多半是可回收缓存」是本篇推演**，原文只把
>   `Mem:`（主存）与 `Swap:`（虚拟内存）区分开，没讨论 page cache。
> - **「自旋不算等待时间」这层因果是本篇的推演**。原文只说「spinning shows up with profiling」，
>   没说自旋为何不进 `waittime`。
> - **「文件描述符是计数型硬上限，满了直接 EMFILE、不排队」是本篇推演**；原文只写
>   「This one may not make sense」。
> - **「容器里看到的是宿主全局状态」这层因果是本篇的推演**：原文只说资源控制环境（云）
>   要对每个 resource control 各问一遍，没讨论容器内 `/proc` 视图与宿主的关系。
> - **「线程数不够时卡在内存分配而非 CPU」是原文陈述**（原文写的是「Threads blocking on
>   memory allocation」），本篇把它与内存饱和度那一格连起来是**本篇的连线**。

## §附录 A 清单定位：USE Method 的 Linux 落地版

**机制**：这份附录是 **Chapter 2 §2.5.9 引入的 USE Method 的 Linux 版清单**。USE Method 的原意是：
对每一个资源（component）问三个问题——**利用率（Utilization）**、**饱和度（Saturation）**、
**错误（Errors）**——从而把「系统健康检查」从拍脑袋变成可逐条执行的检查项。

> This appendix contains a checklist for Linux derived from the USE method [Gregg 13d] . This is a method for checking system health, and identifying common resource bottlenecks and errors, introduced in Chapter 2 , Methodologies , Section 2.5.9 , The USE Method .

后面的章节（第 5、6、7、9、10 章）在各自语境里展开 USE Method 并引入配套工具，
附录把它们**收拢成一张可打印的检查表**。

清单自己声明了有效期：

> Performance tools are often enhanced, and new ones are developed, so you should treat this as a starting point that will need updates. New observability frameworks and tools can also be developed to specifically make following the USE method easier.

**（推演）** 这句话决定了这份清单该怎么用：**要学的是「三问」这个框架、以及「每个资源对应哪条命令」的映射，
不是背命令**。命令会变（原文自己说工具会被增强），框架不会。

**容易错在哪**：把 USE Method 当成「一次跑完的巡检脚本」。它是**按需发问**的方法——
怀疑 CPU 就问 CPU 的三件事，怀疑内存就问内存的三件事。（推演）

## §附录 A 三列读法：Component / Type / Metric

原文的表头就是三列，每行是一个 (组件 × 三问之一) 的组合：

| Component | Type | Metric |
|---|---|---|
| CPU | Utilization | 逐核 / 系统级 / 进程级 / 内核线程各一条命令与读数 |
| CPU | Saturation | … |
| CPU | Errors | … |

- **Component（组件）**：被问的那个资源。
- **Type（类型）**：三问中的哪一问。
- **Metric（指标）**：**具体用什么命令、读哪个数字**。

（推演）读法可以固化成一句话：**「这个组件忙不忙 → 有没有在排队 → 有没有在失败」**。
三问的顺序不是随意的：利用率高**不等于**有问题（一个跑满 CPU 但不排队的进程是健康的）；
饱和度高才是排队（跑不满却排队 = 瓶颈在别处）；错误则是独立的一条轴。

⚠️ 原文每一问独立成行、**互不交叉引用**，所以「利用率正常但饱和度高」这种最常见的情况，
读清单的人必须自己把两行对着看。（推演）

### ⚠️ 一处原文级的坑：load average 故意不在表里

Linux 的 load average 看着最像「CPU 利用率」，但原文**明确没有把它放进 CPU 指标**：

> General notes: uptime “load average” (or /proc/loadavg) wasn’t included for CPU metrics since Linux load averages include tasks in the uninterruptible I/O state.

**机制**：load average 把**不可中断 I/O 状态（D 状态）**的任务也算进去 ⇒ 一个卡在磁盘 I/O 上的
进程会抬高 load average，但**它没有在烧 CPU**。所以 load 高可能是内存/磁盘问题，不是 CPU 问题。

**判据**：load average 高，先看 `vmstat` 的 `r` 列里有多少是真在 CPU 上跑的、多少在等 I/O，
再决定是不是 CPU 问题。

## §附录 A CPU

### Utilization（利用率）

> Per CPU: mpstat -P ALL 1 , sum of CPU-consuming columns ( %usr , %nice , %sys , %irq , %soft , %guest , %gnice ) or inverse of idle columns ( %iowait , %steal , %idle )

原文并列给出**两套等价口径**：

- **加消耗列**：`%usr`、`%nice`、`%sys`、`%irq`、`%soft`、`%guest`、`%gnice`；
- **加空闲列取反**：`%iowait`、`%steal`、`%idle`。

（推演）两套口径互为交叉验证：如果「消耗列之和」与「空闲列取反」对不上，多半是你看的不是同一个
时间窗（`1` 秒采样 vs 上一次报告以来的平均值），而不是数据错。

```bash
mpstat -P ALL 1
```

| 层级 | 命令 | 读什么 |
|---|---|---|
| 逐核 | `mpstat -P ALL 1` | 上面两套口径 |
| 系统级 | `vmstat 1` | `us + sy` |
| 系统级 | `sar -u` | `%user + %nice + %system` |
| 进程级 | `top` / `htop` / `ps -o pcpu` / `pidstat 1` | `%CPU` / `CPU%` |
| 内核线程 | `top`/`htop`（按 `K` 切换） | `VIRT == 0` 的条目（原文标注为 heuristic） |

**容易错在哪**：逐核 `mpstat -P ALL` 与系统级 `vmstat` 的数字**经常不一致**——
这不是矛盾，是因为前者是**每个核**、后者是**全机**。系统级 60% 而逐核里有一个核 100%，
说明有单核热点。（推演）

**往哪查**：系统级高 → 逐核看分布 → 进程级（`pidstat 1` / `top`）排序 → 找到那个进程，
再问它的 Saturation 与 Errors。

### Saturation（饱和度）

系统级（原文 L53）：`vmstat 1` 的 `r` 列 > CPU 数；`sar -q` 的 `runq-sz` > CPU 数；
以及 `runqlat`、`runqlen`。

```bash
vmstat 1
sar -q
```

进程级走**延迟核算**：

> Per process: /proc/PID/schedstat 2nd field (sched_info.run_delay); getdelays.c, CPU 2 ; perf sched latency (shows average and maximum delay per schedule)

```bash
perf sched latency
```

**承重的两个细节**：

1. `r` 列 = **等待中的线程 + 正在 CPU 上跑的线程**（原文脚注 1：「The r column reports those
   threads that are waiting and threads that are running on-CPU.」）。所以 `r` 大于 CPU 数
   **不等于**「CPU 不够」——它也可能是**很多线程在等 I/O**。这与上一节 load average 那条警告是
   同一个机制。
2. 进程级用 `sched_info.run_delay` / `perf sched latency` 是**延迟核算**（原文脚注 2：
   「Uses delay accounting」）——量的是「等了多久」而不是「有多少在等」。这是饱和度的正确量纲。

**往哪查**：调度延迟高 ⇒ 优先级 / 时间片 / cgroup CPU 配额；`perf sched` 直接告诉你
**哪个任务在等、等多久**。

### Errors（错误）

> Machine Check Exceptions (MCEs) seen in dmesg or rasdaemon and ras-mc-ctl --summary ; perf(1) if processor-specific error events (PMCs) are available; e.g., AMD64’s “04Ah Single-bit ECC Errors Recorded by Scrubber” 4 (which can also be classified as a memory device error); ipmtool sel list ; ipmitool sdr list

**承重的一个交叉点**：原文括号里明说，单比特 ECC 错误**也可以归类为内存设备错误**。
也就是说 **CPU 的 Errors 行与内存的 Errors 行会重叠**——同一条 MCE 既是 CPU 错误也是内存错误。
查内存 ECC 错误时不要只查内存那一行。

**脚注 4 的现实提醒**：近年 Intel/AMD 处理器手册里**错误类事件本来就不多**，
所以 PMC 覆盖不了所有错误——不能指望「perf 能查到所有硬件错误」。

```bash
dmesg | grep -i mce
ras-mc-ctl --summary
ipmitool sel list
```

## §附录 A 内存容量

### Utilization（利用率）

> System-wide: free -m , Mem: (main memory), Swap: (virtual memory); vmstat 1 , free (main memory), swap (virtual memory); sar -r , %memused ; slabtop -s c for kmem slab usage

**承重的区分**：`free -m` 里的 `Mem:` 是**主存**、`Swap:` 是**虚拟内存**（交换区）；
`vmstat` 的 `free` / `swap` 同理；`sar -r` 的 `%memused` 是**主存占用率**。
进程级则用 `top`/`htop` 的 `RES`（驻留主存）与 `VIRT`（虚拟内存）——
**`VIRT` 不是占用量**，它含大量未触碰的映射。

```bash
free -m
vmstat 1
sar -r
slabtop -s c
```

**容易错在哪**：把 `%memused` 高直接判成「内存不够」。Linux 会拿空闲内存做 page cache，
`free` 显示 used 高但 available 多时，**可回收缓存**占了大部分。（推演）

**`slabtop -s c` 是不可替代的一格**（推演）：内核自己的内存（slab）**不会在 `free` 里完整呈现**，
内核侧分配不释放（kmalloc 泄漏）只会在这里露出来。

### Saturation（饱和度）

> System-wide: vmstat 1 , si / so (swapping); sar -B , pgscank + pgscand (scanning); sar -W

进程级：

> Per process: getdelays.c, SWAP 2; 10th field (min_flt) from /proc/PID/stat for minor fault rate, or dynamic instrumentation 5

**四个信号，含义各不相同**——这是内存这一格最该记住的一段：

| 信号 | 工具 | 含义 |
|---|---|---|
| `si` / `so` | `vmstat 1` | **真的在换页**：si 是换入、so 是换出 |
| `pgscank` + `pgscand` | `sar -B` | **页扫描器在跑**：回收压力大，但还没到非换不可 |
| `min_flt`（第 10 字段） | `/proc/PID/stat` | **minor fault 速率**：分配压力 |
| OOM killer | `dmesg` 里的 killed 记录 | **最终结果**，不是起点 |

**minor fault 是分水岭**（原文脚注 5：「This can be used to show what is consuming memory and
leading to saturation, by seeing what is causing minor faults. This should be available in
htop(1) as MINFLT.」）——这是**从「饱和」反查「元凶」的入口**。

**OOM killer 是终点不是起点**：看到 OOM 记录时，真正的问题在上游（谁的 RSS 涨爆了），
OOM killer 只是最后一道闸。

### Errors（错误）

> dmesg for physical failures or rasdaemon and ras-mc-ctl --summary or edac-util ; dmidecode may also show physical failures; ipmtool sel list ; ipmitool sdr list ; dynamic instrumentation, e.g., uretprobes for failed malloc()s (bpftrace)

**两类错误要分开**（推演）：**硬件错误**（DIMM 坏了、EDAC 报错）走
`rasdaemon`/`edac-util`/`dmidecode`；**软件错误**（`malloc()` 失败）用 uretprobes 挂钩。
原文把两者并列在同一条里，但它们的排查路径完全不同。

## §附录 A 网络接口

### Utilization（利用率）

> ip -s link , RX/TX tput / max bandwidth; sar -n DEV , rx/tx kB/s / max bandwidth; /proc/net/dev, bytes RX/TX tput/max

**承重的是那个分母**：原文每一处都是 **tput / max bandwidth**——利用率必须相对**链路带宽**算，
没有 max 的百分比没有意义。`sar -n DEV` 给的是 `rx/tx kB/s`，除以网卡标称速率才是利用率。

### Saturation（饱和度）

> nstat , TcpRetransSegs ; sar -n EDEV , *drop/s , *fifo/s 6 ; /proc/net/dev, RX/TX drop ; dynamic instrumentation of other TCP/IP stack queueing (bpftrace)

**一个信号，两个归属**（原文脚注 6：「Dropped packets are included as both saturation and error
indicators, since they can occur due to both types of events.」）——丢包既算饱和（队列满）
也算错误（链路/驱动出错），所以三问在这里**不是互斥的**。

**两个指标指向不同方向**（推演）：`TcpRetransSegs`（重传）指向**路径**问题
（丢包、拥塞、链路质量）；drop 计数指向**本机接收侧处理不过来**（队列满）。

### Errors（错误）

原文 L101 给的路径：`ip -s link` 的 errors；`sar -n EDEV all`；`/proc/net/dev` 的 `errs`、
`drop`；额外的错误计数器可能在 `/sys/class/net/*/statistics/*error*`；
以及**驱动函数返回值**的动态插桩。

**往哪查**：错误往**驱动层**找——`/sys/class/net/.../statistics/` 下的本机计数器是最靠近
故障点的一层。

## §附录 A 存储设备 I/O 与存储容量

### Storage device I/O · Utilization

> System-wide: iostat -xz 1 , %util ; sar -d , %util ; per process: iotop , biotop ; /proc/PID/sched se.statistics.iowait_sum

**`%util` 不是吞吐也不是延迟**（推演）：它是「设备处于忙状态的时间占比」。队列加深时
`%util` 早已 100%，而吞吐可能纹丝不动——所以必须配合 Saturation 那一行看。

进程级：`iotop` / `biotop`；内核还给了精确到进程的字段
`/proc/PID/sched` 的 `se.statistics.iowait_sum`（**任务在 I/O 上累计等待的时间**）。

### Storage device I/O · Saturation

> iostat -xnz 1 , avgqu-sz

**判据是队列深度**。原文给的两个信号是 `avgqu-sz` 大于 1（平均队列长度大于 1 ⇒ 请求在排队）
与 `await` 高（平均等待时间高）。

```bash
iostat -xnz 1
```

（推演）两个信号组合起来能分出两种病：

- `%util` 高 + `await` 高 + 队列深 ⇒ **设备被打满**（加设备或减负载）；
- `%util` 低 + `await` 高 ⇒ **设备不忙但延迟高**（介质/控制器/线缆问题，而不是排队）。

**往哪查**：`perf(1)` 的 block tracepoint 拿队列长度与延迟，`biolatency` 是现成的探针。

### Storage device I/O · Errors

> /sys/devices/ . . . /ioerr_cnt; smartctl ; bioerr ; dynamic/static instrumentation of I/O subsystem response codes 7

**脚注 7 的关键**：I/O 子系统有**多层**——block device、SCSI、SATA、IDE……（原文：
「This includes tracing functions from different layers of the I/O subsystem: block device, SCSI,
SATA, IDE...」）。有些层有现成的静态探针（`perf(1)` 的 scsi 与 block tracepoint 事件），
其余才需要动态追踪。**报 I/O 错要先确定错在哪一层。**

### Storage capacity

- **Utilization**（原文 L125）：交换区用 `swapon -s`、`free`、`/proc/meminfo` 的
  `SwapFree` 与 `SwapTotal`；文件系统用 `df -h`。
- **Saturation**：原文自己标了存疑：

> Not sure this one makes sense—once it’s full, ENOSPC (although when close to full, performance may be degraded depending on the file system free block algorithm)

**这是整份清单里最诚实的一句**：容量型资源**没有「排队」这回事**——满了就是 ENOSPC。
但原文紧接着给了一个真实机制：**接近满时性能也会退化，退化程度取决于文件系统的空闲块算法**
（不同文件系统查找空闲块的成本不同）。这一句是原文明确说的，不是推演。

- **Errors**（原文 L137）：`strace` 找 ENOSPC；动态插桩找 ENOSPC；
  `/var/log/messages` 里按文件系统找 errs；以及应用日志。

## §附录 A 控制器与互连：PMC 层

这一层几乎没有「一条命令搞定」的工具，全靠 `perf stat` 读 PMC（原文术语表：PMCs =
Performance monitoring counters，见 [[Chapter 6. CPUs]]）。

| 组件 | Utilization | Saturation | Errors |
|---|---|---|---|
| Storage controller | `iostat -sxz 1` 把各设备汇总，与该卡已知的 IOPS/tput 上限比 | 见 storage device 那一行 | 见 storage device 那一行 |
| Network controller | 由 `ip -s link`（或 `sar`、`/proc/net/dev`）的 tput 与该控制器的已知上限反推 | 见 network interfaces 那一行 | 见 network interfaces 那一行 |
| CPU interconnect | PMC 读 CPU 互连端口的 tput/max | PMC 读 stall cycles | PMC 读「有的都读」 |
| Memory interconnect | PMC 读内存总线的 tput/max | PMC 读 stall cycles | PMC 读「有的都读」 |
| I/O interconnect | PMC 读 tput/max（没有就由 `iostat`/`ip` 的已知 tput 反推） | PMC 读 stall cycles | PMC 读「有的都读」 |

**内存互连那一格含一个可以直接用的间接判据**：

> perf stat with PMCs for memory buses, tput/max; e.g. Intel uncore_imc/data_reads/,uncore_imc/data_writes/; or IPC less than, say, 0.2; PMCs may also have local versus remote counters

**IPC 低于约 0.2** ⇒ CPU 大部分周期在等内存 ⇒ 内存互连（或内存本身）是瓶颈。
这是一个**没有 PMC 也能用**的间接判据。

**`local versus remote counters`** ⇒ PMC 能区分本地访问与远端访问 ⇒ **NUMA 失衡可以直接量出来**
（远端访问计数高 ⇒ 线程绑错了核）。

**互连层的 Errors 一律是「whatever is available」**——这一层的可观测性最弱，这是原文自己透露的，
不是本篇的判断。

**术语**：原文把「I/O interconnect」定义为 CPU 到 I/O 控制器的总线、I/O 控制器本身，
以及设备总线（如 PCIe）。

## §附录 A 软件资源（一）：锁

### 内核互斥锁

- **Utilization**（原文 L267）：需内核开启 `CONFIG_LOCK_STATS=y`，读 `/proc/lock_stat` 的
  `holdtime-total` 除以 `acquisitions`（另有 `holdtime-min`、`holdtime-max`）；
  或动态插桩锁函数（原文注明「maybe」）。
- **Saturation**：

> With CONFIG_LOCK_STATS=y, /proc/lock_stat waittime-total / contentions (also see waittime-min , waittime-max ); dynamic instrumentation of lock functions, e.g., mlock.bt [Gregg 19]; spinning shows up with profiling perf record -a -g -F 99 ...

**两个比值不是一回事**——这是内核锁这一格的结构性要点：

- Utilization = **持有时间 ÷ 获取次数** → 这把锁有多「黏」；
- Saturation = **等待时间 ÷ 竞争次数** → 有多少人在抢。

**自旋（spinning）不会出现在 `waittime` 里**——自旋的线程**没有睡**，它占着 CPU 空转。
原文给的发现手段是 profiling：`perf record -a -g -F 99 ...`。（推演）这就是
「CPU 利用率 100% 但有效工作很少」这类问题的入口。

- **Errors**（原文 L279）：动态插桩（例如**递归进入同一把 mutex**）；其他错误会导致
  kernel lockup / panic，用 kdump / crash 调试。

**脚注 8**：内核锁分析过去靠 `lockmeter`（它有个叫 `lockstat` 的接口），
`/proc/lock_stat` 是它的替代。

### 用户态互斥锁

- **Utilization**（原文 L285）：`valgrind --tool=drd --exclusive-threshold= ...`（持有时间）；
  或动态插桩 lock-to-unlock 的函数耗时。
- **Saturation**（原文 L291）：用 `valgrind --tool=drd` **从持有时间反推竞争**；
  动态插桩同步函数的等待时间（如 `pmlock.bt`）；`perf(1)` 采用户栈看自旋。
- **Errors**（原文 L297）：`valgrind --tool=drd` 的各类错误；动态插桩
  `pthread_mutex_lock()` 抓 `EAGAIN`、`EINVAL`、`EPERM`、`EDEADLK`、`ENOMEM`、
  `EOWNERDEAD`。

**结构差异（推演）**：内核锁有 `lock_stat` 这个现成的计数器，用户态**没有**——
用户态的持有时间只能靠 `drd` 或插桩，而 `drd` 是**离线**分析（会把程序放慢几个数量级），
插桩则要改代码。

**脚注 9 是这两格的使用前提**：

> Since these functions can be very frequent, beware of the performance overhead of tracing every call: an application could slow by 2x or more.

⇒ **不要对每把锁都插桩**，先用采样 / profile 定位嫌疑锁。

## §附录 A 软件资源（二）：任务容量与文件描述符

### 任务容量

- **Utilization**（原文 L303）：`top`/`htop` 的 `Tasks`（当前数）；
  上限是 `sysctl kernel.threads-max` 或 `/proc/sys/kernel/threads-max`。
- **Saturation**（原文 L309）：线程**阻塞在内存分配上**；到这一步**页扫描器应该已经在跑**
  （`sar -B` 的 `pgscan*`），否则用动态追踪继续查。
- **Errors**（原文 L315）：「can't fork()」错误；用户级线程 `pthread_create()` 失败
  （`EAGAIN`、`EINVAL` 等）；内核级动态追踪 `kernel_thread()` 的 `ENOMEM`。

**这一格是整份清单里跨资源耦合最强的一处**（原文直说）：**线程数不够时，线程卡住的地方不是 CPU，
而是内存分配**——而且此时页扫描器已经在跑（这与内存容量的 Saturation 是同一件事）。
（推演）推论：看到 `Tasks` 数逼近 `threads-max`，要同时看内存饱和度，不要先怀疑调度。

### 文件描述符

- **Utilization**（原文 L321）：

> System-wide: sar -v , file-nr versus /proc/sys/fs/file-max; or just /proc/sys/fs/file-nr

  进程级（原文 L323）：数 `/proc/PID/fd/` 下有多少项，与 `ulimit -n` 比。

  ```bash
  echo /proc/PID/fd/* | wc -w
  ```

- **Saturation**（原文 L329）：**「This one may not make sense」**——和存储容量一样，
  作者明确说这一问不适用。
- **Errors**（原文 L335）：`strace` 抓返回文件描述符的系统调用上的 `errno == EMFILE`
  （如 `open(2)`、`accept(2)`）；`opensnoop -x`。

**为什么 FD 的饱和「不适用」**（推演）：文件描述符是**计数型硬上限**，满了直接 `EMFILE` 失败——
**没有排队、没有降速**。饱和度衡量的是「等待」，而 FD 满的表现是「失败」。
所以这里 Saturation 行是空的，Errors 行才是真正的那一问。

**两处「作者说不适用」合起来看**（推演）：存储容量与文件描述符是清单里唯二被标注
「可能不适用」的格子。USE 三问**不是能机械套到每个资源上的公式**——遇到计数型 / 容量型资源，
要先判断它到底有没有「排队」这一层。

## §附录 A 资源控制环境、通用说明与术语

### 资源控制（云）：USE 之前先问限额

> For any environment that imposes resource controls (e.g., cloud computing), check USE for each resource control. These may be encountered—and limit usage—before the hardware resource is fully utilized.

**在云上，资源控制（cgroup / 配额）会先于硬件打满而限制你**：所以 USE 要**对每一个资源控制各问一遍**，
不能只对着物理硬件问。（推演）这条对容器环境尤其致命：宿主机的 `mpstat`/`free` 显示一切正常，
容器里的 cgroup 限额却已经触顶。

### 术语

- **perf(1)**：原文称之为 powerful observability toolkit，能读 PMC，也能做动态与静态插桩
  （见 [[Chapter 13. perf]]）。
- **PMCs**：Performance monitoring counters，硬件性能计数器（见 [[Chapter 6. CPUs]]）。
- **Dynamic instrumentation**（动态插桩）：允许**自定义指标**被开发出来（见第 4 章
  Observability Tools 与后续章节）；Linux 上的动态追踪工具包括 `perf(1)`（第 13 章）、
  Ftrace（第 14 章，见 [[Chapter 14. Ftrace]]）、BCC 与 bpftrace（第 15 章）。
- **I/O interconnect**：CPU 到 I/O 控制器的总线、I/O 控制器本身，以及设备总线（如 PCIe）。

### 脚注里三条容易被漏掉的机制

1. **脚注 1**：`r` 列报告的是「等待中的线程 + 正在 CPU 上跑的线程」。
2. **脚注 5**：minor fault 可以显示**是什么在消耗内存并导致饱和**，htop 里对应 `MINFLT`。
3. **脚注 6**：丢包同时算饱和与错误两种指标。

## 章末选型判据

| 场景 | 先问什么 | 判据 | 别用什么 |
|---|---|---|---|
| 「机器很慢」，还没定位到资源 | 逐个资源各问三遍；先看 CPU 的利用率与饱和度 | — | 别拿 load average 当 CPU 利用率（它把 D 状态算进去） |
| CPU 利用率高 | 逐核 `mpstat -P ALL 1` 看分布 | 系统级高但逐核不均 ⇒ 单核热点 | 别只看 `vmstat` 的 `us + sy` |
| 进程「拿到了 CPU 但很慢」 | 饱和度：`sar -q` 的 `runq-sz`、`perf sched latency` | 调度延迟高 ⇒ 优先级 / 时间片 / cgroup 配额 | 别用 load average 代替 |
| 内存「不够用」 | `free -m` 的 `Mem:` 与 `Swap:`、`sar -r` 的 `%memused`、`slabtop -s c` | used 高但 available 多 ⇒ 多半是可回收缓存 | 别把 `VIRT` 当占用量 |
| 内存饱和 | `vmstat 1` 的 `si`/`so`、`sar -B` 的 `pgscan*` | 有换页或扫描在跑才算饱和 | 别用 `%memused` 高直接判饱和 |
| 想找「谁在吃内存」 | 进程级 `min_flt` 速率（`/proc/PID/stat` 第 10 字段） | minor fault 速率高者即是嫌疑 | 别等 OOM killer 告诉你 |
| 网络「带宽不够」 | `sar -n DEV` 的 rx/tx kB/s 除以链路带宽 | **必须除以 max** | 别报没有分母的百分比 |
| 网络丢包 | `nstat` 的 `TcpRetransSegs` vs `sar -n EDEV` 的 drop | 重传 ⇒ 路径问题；drop ⇒ 本机队列满 | 别把两者混为一谈 |
| 磁盘「慢」 | `iostat -xz 1` 的 `%util` + `avgqu-sz` + `await` | 队列深 ⇒ 排队；`%util` 低而 `await` 高 ⇒ 不是排队 | 别单看 `%util`（100% 不等于满吞吐） |
| 容器里「资源不够」 | 先查 cgroup 限额 | 限额先于硬件触顶 | 别只查宿主机的 `free`/`mpstat` |
| 怀疑锁竞争 | 内核 `/proc/lock_stat` 的 `waittime-total`/`contentions`；用户态 `drd` 或插桩 | 自旋看 `perf record -a -g -F 99` | 别对每把锁都插桩（原文脚注 9：可能慢 2 倍以上） |
| 「线程数不够」 | 同时看内存饱和度 | 卡在内存分配 ⇒ 页扫描器已在跑 | 别先怀疑调度 |
| 要写巡检脚本 | 把「三问 + 分母 + 元凶反查」固化成流程 | 每个资源三行 | 别把命令列表当成 USE Method |

**三条「什么时候别用这份清单」**（推演）：

1. **你要的是原理，不是命令** —— USE 的原理在 [[Chapter 2. Methodologies]]，
   附录只给落地表。
2. **资源不在表里** —— 表里的组件是作者当时 Linux 工具集所覆盖的，
   容器与 eBPF 时代的新指标未必在列（原文自己也说清单需要更新）。
3. **只看一个数就下结论** —— 三问是**联动的**：利用率高但饱和度低 ≠ 有问题；
   饱和度高但利用率低 ⇒ 瓶颈在别处。
