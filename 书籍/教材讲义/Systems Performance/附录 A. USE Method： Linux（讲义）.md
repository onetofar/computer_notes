---
title: 附录 A. USE Method： Linux（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - USE方法
  - 性能排查
  - 可观测性
  - 指标
  - Systems Performance
source: "《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg，Addison-Wesley 2020，英文原版）附录 A"
companion: "[[附录A-Appendix A： USE Method： Linux]]"
---

# 附录 A 讲义：USE 方法：怎么系统地排查

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[附录A-Appendix A： USE Method： Linux|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：`otf-server` 的 `src/net/tcp_transport.cpp`（epoll 事件循环，528 行）、`src/net/metrics.cpp` / `metrics.h` / `metrics_server.cpp`（观测子系统，172 + 145 + 181 行）。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原章。

---

## 0. 开局：「服务器变慢了」，你的第一句话是什么？

线上告警响了：**「游戏服务器延迟涨了」**。你现在要回答一个问题——**慢在哪？**

大多数人脑子里蹦出来的第一句是「看 CPU」。于是 `top` 一开，`%CPU` 是 40%，心里一块石头落地：「CPU 没问题」。

然后呢？

（推演）40% 的 CPU 说明**CPU 有一半时间在闲着**。可服务器确实慢了。这两件事同时成立，只可能是**别的什么东西**把时间吃掉了——磁盘队列在等、网络重传在等、某个锁在等。于是你重新开始查，每换一个怀疑对象就要换一套命令、记一套指标名。**这就是没有方法论时的排查现场：靠临时记忆 + 逐个试错。**

 Gregg 在这一章给的就是那份方法论。它不是「性能工具清单」，它是**一张体检表**：每个资源都问同样三个问题，答案就有了固定的形状。

---

## 1. §附录 A 方法本身：Utilization / Saturation / Errors

原文稿的第一段就交代了这份清单的出身：

> This appendix contains a checklist for Linux derived from the USE method [Gregg 13d] . This is a method for checking system health, and identifying common resource bottlenecks and errors, introduced in Chapter 2 , Methodologies , Section 2.5.9 , The USE Method .

要点有三个：**它是一份 checklist**（清单，不是论述）；**它派生自 USE 方法**（方法在第 2 章 §2.5.9，这里是 Linux 上的落地版）；**它服务于 health check 与 bottleneck / error 识别**（不是「性能调优」）。

### 三个词，各自该怎么问

USE 三个字母看起来像三个名词，但它们实际是**三个问题的缩写**。用本仓库当尺子，一个一个展开：

| 词 | 怎么问 | 怎么量 | 量到什么算超限 |
|---|---|---|---|
| **Utilization**（利用率） | 这个资源**被吃掉多少**？ | 一个「已用 / 总量」的**比值** | 接近 100% 通常就是超限（但**不总是**，见下） |
| **Saturation**（饱和度） | 有没有东西在**排队等**它？ | 排队**长度**或**等待时间** | 排队长度 > 1、或等待时间持续增长 ⇒ **超限**（哪怕利用率才 60%） |
| **Errors**（错误） | 它自己**报错过几次**？ | 错误**计数** | 任何一次非预期错误都值得看；速率持续上涨 ⇒ 超限 |

### 关键直觉：饱和度不是「利用率到顶」

这是整章最容易讲错、也最有价值的一点。

**排队的阈值不一定是 100%。** 原文稿的表里给了一堆现成的判据，看它们长什么样：

- CPU 饱和：`vmstat 1 , r > CPU count 1`（可运行队列长度超过 CPU 数）；
- 磁盘饱和：`iostat -xnz 1 , avgqu-sz > 1, or high await`（平均队列长度 > 1，或 await 很高）；
- 网络饱和：`nstat , TcpRetransSegs`（重传计数）、`sar -n EDEV , *drop/s , *fifo/s`。

（推演）`avgqu-sz > 1` 这个判据最能说明问题。**队列长度超过 1，意味着「平均每有一个请求在服务，就还有一个在等」**——这时候设备可能才跑在 50% 的带宽上，但**延迟已经开始爆炸**。利用率告诉你「资源还够用」，饱和度告诉你「**排队已经开始了**」。前者是事后确认，后者是提前预警。

### 为什么要按 U → S → E 的顺序走

（推演）这是个**成本递增**的顺序：U 最便宜（`top` / `free` 一把就出来），S 要动真格（`vmstat` / `iostat` / `perf`），E 最贵（要硬件计数器 MCE、要 `rasdaemon`、要 `dmesg`）。而排查的价值密度恰好相反——**U 命中率最高**（大部分瓶颈确实是某个资源用满了），S 次之（排队型瓶颈更隐蔽），E 最后（错误通常不是「慢」的原因，而是「已经坏了」的证据）。

### 「这份清单会过期」

> Performance tools are often enhanced, and new ones are developed, so you should treat this as a starting point that will need updates. New observability frameworks and tools can also be developed to specifically make following the USE method easier.

作者在第一段就提醒：**这是起点，不是终点**。工具会变、新的可观测框架会变。这条提醒在 2026 年读更有分量——今天 `eBPF` 系列工具（bpftrace / BCC）已经比原文稿写作时成熟得多，而原文稿里 `dynamic instrumentation` 那一栏写的还是「e.g., uretprobes for failed malloc()s (bpftrace)」。

---

## 2. §附录 A 硬件资源：这张表怎么读

原文稿这一节的骨架是一张三列大表：**Component（组件）/ Type（Utilization / Saturation / Errors）/ Metric（怎么量）**。

> ⚠️ **抽取瑕疵提醒**： epub 直抽把这张表的**表格结构整个压平了**——每个单元格变成单独一行，行列关系丢失。所以下面这张表是**按原文逐格重建**的，不是原文稿的排版。

### 重建后的硬件资源表

| 组件 | Utilization | Saturation | Errors |
|---|---|---|---|
| **CPU** | 每核：`mpstat -P ALL 1`（求和 `%usr %nice %sys %irq %soft %guest %gnice`，或反过来求 `%iowait %steal %idle` 的补）；`sar -P ALL`。整机：`vmstat 1` 的 `us + sy`；`sar -u` 的 `%user + %nice + %system`。每进程：`top` 的 `%CPU`、`htop` 的 `CPU%`、`ps -o pcpu`、`pidstat 1 , %CPU`。每内核线程：`top/htop`（K 切换），`VIRT == 0` 那个启发式 | 整机：`vmstat 1 , r > CPU count`；`sar -q , runq-sz > CPU count`；`runqlat`；`runqlen`。每进程：`/proc/PID/schedstat` 第 2 个字段（`sched_info.run_delay`）；`getdelays.c` 的 `CPU`；`perf sched latency`（给出每次调度的平均与最大延迟） | `dmesg` 或 `rasdaemon` / `ras-mc-ctl --summary` 里的 Machine Check Exceptions（MCE）；`perf(1)`（若处理器有 PMCs）；如 AMD64 的「04Ah Single-bit ECC Errors Recorded by Scrubber」（也可归类为内存设备错误）；`ipmtool sel list`；`ipmitool sdr list` |
| **Memory capacity**（内存容量） | 整机：`free -m` 的 `Mem:`（物理内存）与 `Swap:`（虚拟内存）；`vmstat 1` 的 `free` 与 `swap`；`sar -r , %memused`；`slabtop -s c` 看 kmem slab。每进程：`top`/`htop` 的 `RES`（常驻物理内存）、`VIRT`（虚拟内存） | 整机：`vmstat 1 , si / so`（换页）；`sar -B , pgscank + pgscand`（页面扫描）；`sar -W`。每进程：`getdelays.c` 的 `SWAP`；`/proc/PID/stat` 第 10 个字段（`min_flt`）看缺页率，或动态插桩；`dmesg | grep killed`（OOM killer） | `dmesg` 里的物理故障；`rasdaemon` / `ras-mc-ctl --summary` / `edac-util`；`dmidecode` 也可能显示物理故障；`ipmtool sel list`；`ipmitool sdr list`；动态插桩，如用 uretprobes 追 `malloc()` 失败（bpftrace） |
| **Network interfaces** | `ip -s link` 的 RX/TX 吞吐 ÷ 最大带宽；`sar -n DEV` 的 rx/tx kB/s ÷ 最大带宽；`/proc/net/dev` 的 bytes RX/TX 吞吐 ÷ 最大 | `nstat , TcpRetransSegs`；`sar -n EDEV , *drop/s , *fifo/s`；`/proc/net/dev` 的 RX/TX drop；TCP/IP 协议栈其他队列的动态插桩（bpftrace） | `ip -s link` 的 errors；`sar -n EDEV` 全量；`/proc/net/dev` 的 `errs`、`drop`；额外的错误计数器在 `/sys/class/net/*/statistics/*error*`；驱动函数返回值的动态插桩 |
| **Storage device I/O** | 整机：`iostat -xz 1 , %util`；`sar -d , %util`。每进程：`iotop`、`biotop`；`/proc/PID/sched` 的 `se.statistics.iowait_sum` | `iostat -xnz 1 , avgqu-sz > 1, or high await`；`sar -d` 同上；`perf(1)` 的 block tracepoint 看队列长度/延迟；`biolatency` | `/sys/devices/.../ioerr_cnt`；`smartctl`；`bioerr`；对 I/O 子系统响应码做动态/静态插桩 |
| **Storage capacity** | Swap：`swapon -s`；`free`；`/proc/meminfo` 的 `SwapFree` / `SwapTotal`。文件系统：`df -h` | ⚠️ 原文自己都不确定：`Not sure this one makes sense`——一旦满了就是 ENOSPC（不过接近满时性能可能已经退化，取决于文件系统的空闲块算法） | 文件系统错误：`strace` 抓 ENOSPC；动态插桩抓 ENOSPC；`/var/log/messages` 里的错误（取决于文件系统）；应用日志错误 |
| **Storage controller** | `iostat -sxz 1`，把各设备求和，再与该卡已知的 IOPS/吞吐上限比 | 同「Storage device I/O」的 Saturation | 同「Storage device I/O」的 Errors |
| **Network controller** | 由 `ip -s link`（或 `sar`、`/proc/net/dev`）推断，与控制器已知吞吐上限比 | 同「Network interfaces」的 Saturation | 同「Network interfaces」的 Errors |
| **CPU interconnect** | `perf stat` + CPU 互连端口的 PMCs，吞吐 ÷ 上限 | `perf stat` + stall cycles 的 PMCs | `perf stat` + 任何可用的 PMCs |
| **Memory interconnect** | `perf stat` + 内存总线的 PMCs，吞吐 ÷ 上限；如 Intel 的 `uncore_imc/data_reads/`、`uncore_imc/data_writes/`；或 **IPC 小于约 0.2**；PMCs 可能分本地/远端计数 | `perf stat` + stall cycles 的 PMCs | `perf stat` + 任何可用的 PMCs；`dmidecode` 或许能看出点什么 |
| **I/O interconnect** | `perf stat` + 吞吐 ÷ 上限的 PMCs（若可用）；否则由 `iostat` / `ip` 等已知的吞吐推断 | `perf stat` + stall cycles 的 PMCs | `perf stat` + 任何可用的 PMCs |

### 从这张表里读出三条「作者的态度」

**（一）作者很诚实：有些格子填不出东西。** Storage capacity 的 Saturation 写的是「Not sure this one makes sense」，File descriptors 的 Saturation 写的是「This one may not make sense」。**留白比编一个假判据好**——USE 方法的价值在于「问对了问题」，问不出答案的问题标出来，比给一个似是而非的阈值强。

**（二）重复引用是刻意的。** Storage controller / Network controller 的 Saturation 与 Errors 三行都写「See storage device saturation, ...」「See network interfaces, errors, ...」。**控制器与设备是同一条瓶颈链上的两级**，量设备就够，控制器那栏只是提醒你「别忘了卡也有上限」。

**（三）`IPC < 0.2` 是一个非常好用的经验判据。** CPU 利用率不高但 IPC 极低，通常意味着 CPU 在**等内存**——等 cache miss、等 TLB miss。这是一条「用别的资源解释 CPU 的低利用率」的交叉验证。

---

## 3. §附录 A 软件资源

硬件之外，原文稿还有一节软件资源。这一节的判据明显更「Linux 内部」：

| 组件 | Utilization | Saturation | Errors |
|---|---|---|---|
| **Kernel mutex**（内核锁） | 需 `CONFIG_LOCK_STATS=y`：`/proc/lock_stat` 的 `holdtime-total / acquisitions`（另见 `holdtime-min`、`holdtime-max`）；对锁函数/指令做动态插桩（也许可行） | 需 `CONFIG_LOCK_STATS=y`：`/proc/lock_stat` 的 `waittime-total / contentions`（另见 `waittime-min`、`waittime-max`）；动态插桩锁函数，如 `mlock.bt`；自旋用 `perf record -a -g -F 99` profiling 看 | 动态插桩（如递归 mutex 的进入）；其他错误可导致内核 lockup/panic，用 `kdump` / `crash` 调试 |
| **User mutex**（用户态锁） | `valgrind --tool=drd --exclusive-threshold= ...`（持有时间）；动态插桩「加锁到解锁」的函数耗时 | `valgrind --tool=drd` 从持有时间推断争用；动态插桩同步函数的等待时间，如 `pmlock.bt`；用 `perf(1)` profiling 采用户栈看自旋 | `valgrind --tool=drd` 的各类错误；动态插桩 `pthread_mutex_lock()` 的 `EAGAIN`、`EINVAL`、`EPERM`、`EDEADLK`、`ENOMEM`、`EOWNERDEAD` 等 |
| **Task capacity**（任务容量） | `top`/`htop` 的 Tasks（当前数）；`sysctl kernel.threads-max`、`/proc/sys/kernel/threads-max`（上限） | 卡在内存分配上的线程；到这一步页面扫描器应当已经跑起来了（`sar -B , pgscan*`），否则用动态 tracing 查 | 「can't fork()」错误；用户态线程：`pthread_create()` 失败并返回 `EAGAIN`、`EINVAL` 等；内核：动态 trace `kernel_thread()` 的 `ENOMEM` |
| **File descriptors** | 整机：`sar -v , file-nr` 对 `/proc/sys/fs/file-max`；或直接读 `/proc/sys/fs/file-nr`。每进程：`echo /proc/PID/fd/* | wc -w` 对 `ulimit -n` | ⚠️ `This one may not make sense` | `strace` 抓返回文件描述符的系统调用上的 `errno == EMFILE`（如 `open(2)`、`accept(2)` 等）；`opensnoop -x` |

### 两条只有软件资源才有的读法

**（一）用户态锁的「饱和」是从「持有时间」推出来的。** 表里写的是 `valgrind --tool=drd to infer contention from held time`——因为用户态一般拿不到「等锁时间」的现成计数器，只能拿持有时间去**反推**。这比内核的 `/proc/lock_stat` 差一档精度，但也是唯一的路子。

**（二）文件描述符这一行是「整机 vs 每进程」的双视角。** 整机看 `/proc/sys/fs/file-nr`（已分配 / 上限），每进程看 `echo /proc/PID/fd/* | wc -w` 对 `ulimit -n`。**两个上限不一样**：整机上限通常远大于单进程上限，所以「单个进程 fd 用光」完全可能在整机数字看起来很健康时发生。

---

## 4. §附录 A 贯穿全表的四条规矩

这一节没有新表格，只有四条「读表时的注意事项」，但每一条都会改变你的结论。

### 规矩一：load average **不是** CPU 指标

> General notes: uptime “load average” (or /proc/loadavg) wasn’t included for CPU metrics since Linux load averages include tasks in the uninterruptible I/O state.

翻译过来：**load average 把「不可中断 I/O 状态（D 状态）」的任务也算进去了**。所以一个 load 很高的机器，可能根本不是 CPU 忙，而是**一堆进程卡在磁盘 I/O 上**。把 load average 当 CPU 利用率读，会把你直接引到错误的资源上去——**这正是 USE 方法要防的那类错误**。

（推演）这一条几乎可以当成整篇附录的题眼：**「问对问题」比「拿到数字」重要。** load average 是个极其顺手的数字，正因为顺手，它最容易被当成答案。

### 规矩二：工具分三层，用途不同

> perf(1): is a powerful observability toolkit that reads PMCs and can also use dynamic and static instrumentation. Its interface is the perf(1) command. See Chapter 13 , perf .

> PMCs: Performance monitoring counters. See Chapter 6 , CPUs , and their usage with perf(1).

> Dynamic instrumentation: allows custom metrics to be developed. See Chapter 4 , Observability Tools , and the examples in later chapters. Dynamic tracing tools for Linux include perf(1) ( Chapter 13 ), Ftrace ( Chapter 14 ), BCC and bpftrace ( Chapter 15 ).

- **PMCs**（性能监控计数器）：硬件级，测微架构停顿（stall cycles）、内存总线吞吐。**要硬件支持、要权限、要看型号**。
- **静态/动态插桩**：在代码路径上挂钩子，能造出**任何**自定义指标——但**开销会进你的数字**。

### 规矩三：插桩有代价，脚注明确警告

> Since these functions can be very frequent, beware of the performance overhead of tracing every call: an application could slow by 2x or more.

**「应用可能慢一倍以上」**——所以插桩出来的数字**不能直接当生产基线**。这一条也解释了为什么很多判据给的是「两个来源二选一」：能拿现成计数器就别插桩。

### 规矩四：I/O interconnect 的定义

> I/O interconnect: This includes the CPU-to-I/O controller buses, the I/O controller(s), and device buses (e.g., PCIe).

这一条本身不起眼，但它解释了为什么表里会出现「**I/O 控制器**」这种平时没人提的组件——因为它被算作互连的一部分。

### 规矩五（补）：云环境下，**每一层**限制都要查

> For any environment that imposes resource controls (e.g., cloud computing), check USE for each resource control. These may be encountered—and limit usage—before the hardware resource is fully utilized.

**这句话放在 2026 年读格外有分量。** 容器有 CPU quota、内存 limit、cgroup 的 io.max；虚机有 hypervisor 的 ballooning。**这些限制会先于硬件资源饱和**——你以为在查磁盘，其实早就撞上了 cgroup 的 `io.max`。所以：**查 USE 要按「限制层」逐层查，而不是按「资源名」查一遍就完。**

---

## 5. 实战锚点：otf-server 逐项填表

现在把这一章的方法用在本仓库 `/home/xier/dev/game-project/otf-server/` 上。**先说清楚这个服务器是什么形状**（下面所有判断都基于此）：

- **单线程 reactor**：`src/net/tcp_transport.cpp`（528 行）的 `TcpTransport::Poll()` 在 `epoll_wait` 上阻塞，回来后按就绪 fd 分派；`src/main.cpp` 的 `Run()` 在同一个线程里驱动 `Poll()` → 固定步长 tick → 尾部 `ReapClosed()`。
- **零第三方依赖**、零磁盘 I/O（日志走 `stderr`，`src/common/logger.cpp` 里只有 `std::fprintf(stderr, ...)`）。
- **连接表用 fd 直接做下标**：`kMaxFd = 1024`（`src/proto/protocol.h:214`），`conns_` 是个 `std::vector<Conn>`。

### 逐项填表

| 资源 | 怎么量（Linux） | 这个仓库的现状 | 缺什么 |
|---|---|---|---|
| **CPU** | `pidstat -u -p <pid> 1` 看单进程；`top -H` 看每线程；整机 `mpstat -P ALL 1` | **单线程**，所以进程 CPU ≈ 线程 CPU ≈ reactor 那一根线程。`main.cpp` 的主循环是「epoll_wait 超时 → tick」，tick 是固定步长（`kTickRateHz`），所以 CPU 占用随房间数线性上升 | **指标里一个 CPU 数都没有**。没有「tick 耗时」「每 tick CPU 时间」这类指标——现在只能靠外部 `pidstat` 补 |
| **Memory capacity** | `free -m`；`vmstat 1`（看 `si`/`so`/`free`）；`pmap -x <pid>` / `/proc/<pid>/status` 的 `VmRSS`；`smaps_rollup` 看 malloc arena | ① 发送缓冲 `Conn::out` 是 `std::vector<uint8_t>`，`FlushWrite()` 写完后只 `clear()`——**capacity 不还给系统**，`clear()` 只把 size 归零。连接的**历史峰值容量会一直占着**，直到 `Destroy()` 里 `c = Conn{}` 才释放。1024 条连接 × 每条曾达 128KB（`kSendBufCap`）≈ **128MB 的理论驻留上限**。② `conns_` 本身是 1024 个 `Conn`（约 48KB），可忽略。③ `FrameDecoder::ring_` 是每连接一个，容量受 `kFrameQueueDepth` 约束 | **没有任何内存指标**。`MetricsSnapshot` 里没有 RSS、没有堆分配总量、没有「哪些连接占了大头」 |
| **Storage device I/O** | `iostat -xz 1`；`pidstat -d 1` | **运行时零磁盘 I/O**（无持久化、无日志文件、无存档）。这一行对本仓库**整体不适用** | 不需要补（但要注意：一旦加存档/日志落盘，这行立刻变成第一嫌疑） |
| **Storage capacity** | `df -h` | 同上，运行时不占文件系统 | 无 |
| **Network interfaces** | `ip -s link`；`sar -n DEV 1`；`/proc/net/dev`；本进程侧 `ss -tin` 看每条连接的发送队列 `Send-Q` | ★ **发送队列长度是本仓库最关键的饱和度指标**：`TcpTransport::queued_bytes(id)` 已经把「用户态还没写出去的字节数」算出来了（`out.size() - out_sent`），单条连接的上限是 `kSendBufCap = 128KB`。**超限即断连**（`Send()` 里 `log::warn("发送队列超上限，断开该连接（背压保护）")`）——**这已经是饱和的硬证据，但只进了日志，没进指标** | **`otf_queued_bytes` 之类的聚合指标缺失**。现有指标只有 `otf_frames_sent` / `otf_frames_recv`（条数，不是字节），看不到「积压」 |
| **Network interfaces（饱和）** | `nstat , TcpRetransSegs`；`ss -tin` 的 `unacked`/`rto` | 建连期设了 `TCP_NODELAY`（`AcceptNew()`），**刻意不调** `SO_RCVBUF`/`SO_SNDBUF`（注释理由是「带宽余量 20 倍以上」） | 无重传计数。`TCP_NODELAY` 设失败只 `warn` 不关连接（降级可用），这类降级**没有指标**，只在日志里 |
| **Network interfaces（错误）** | `ip -s link` 的 errors；`dmesg` | `otf_heartbeat_timeouts`（心跳超时）与 `otf_oversize_rejects`（超长帧拒收）是**两个真正的错误类指标**——都在 `MetricsSnapshot` 里 | 缺「每连接错误」「RST 次数」「accept 失败次数」 |
| **Task capacity** | `top`/`htop` 的 Tasks；`/proc/sys/kernel/threads-max` | **单线程**，Tasks 恒为 1（加主线程）。这条对本仓库**当前不适用**——但它是 `目标.md` roadmap 第 1 步（拆 IO 线程 + 逻辑线程）之后**第一个会变**的条目 | 拆多线程后必须补「线程数」指标 |
| **File descriptors** | 整机 `file-nr`；每进程 `echo /proc/<pid>/fd/* \| wc -w` 对 `ulimit -n` | fd 表大小 `kMaxFd = 1024`。`AcceptNew()` 里 `if (!Valid(fd)) { ::close(fd); continue; }`——**超过上限的连接直接被静默关掉，没有任何日志、没有任何指标** | ★ **这是本仓库最刺眼的一个 USE 缺口**：USE 的「任务/资源容量」类里，「fd 用满」是典型的饱和事件，而现在它**完全静默** |

### 「Errors 里最容易被忽略的一项」：不变量

`otf_epoll_ctl_add_with_out` 是本仓库独有的一个指标，它不属于任何一条标准 USE 项——但它做的事正是 USE 的 Errors 逻辑：

`src/net/tcp_transport.cpp` 的 `Ctl()` 只在 `op == EPOLL_CTL_ADD` 时检查 `has_out` 并计数。规格是「注册时只监听 `EPOLLIN`」（§6.4.1），所以**这个数必须恒为 0**。非 0 就意味着有 fd 带着 `EPOLLOUT` 注册，症状是 LT 模式下被反复唤醒、CPU 凭空上涨。

（推演）这是一个**用不变量当错误指标**的例子：它的价值不在于「数有多大」，而在于「**它永远应该是 0**」。USE 的 Errors 一栏问「错了几次」，这个指标问的是「**有没有一次本不该发生的注册**」。

---

## 6. 实战锚点：给这个服务器补一套 USE 指标

下面是**一张具体的清单**：指标名、从哪采、在哪采。每一条都标出它在现有 `src/net/metrics.cpp` / `metrics.h` / `metrics_server.cpp` 里**有没有**。

| # | 指标名（建议） | 类型 | 怎么采 | 在哪采 | 现状 |
|---|---|---|---|---|---|
| 1 | `otf_epoll_wait_blocked_ratio` | gauge | `epoll_wait` 返回的 `n == 0`（超时）次数 ÷ 总次数 | `TcpTransport::Poll()` | ❌ **缺**。这是本仓库的「CPU 利用率」——**没在等 I/O 的时间比例 = 逻辑在跑的比例** |
| 2 | `otf_epoll_events_per_wait` | gauge | 每次 `epoll_wait` 返回的事件数（累加 ÷ 次数） | 同上 | ❌ 缺。它是「每次唤醒处理多少连接」的效率指标 |
| 3 | `otf_tick_cost_us_max` / `_avg` | gauge / counter | 一整个 tick 的耗时（µs） | `src/app/tick_scheduler.cpp` | ❌ **缺**。`TickScheduler` 只记 `tick_index()`，不记耗时——**「tick 跑不完」是本仓库最现实的风险** |
| 4 | `otf_tick_budget_overruns` | counter | 补帧（catch-up）真的发生过的次数 | `TickScheduler` | ❌ 缺。`kMaxCatchupFrames = 5` 是个上限，但**用没用满**没有指标 |
| 5 | `otf_queued_bytes` / `otf_queued_bytes_max` | gauge / gauge | `Σ queued_bytes(id)` over live conns；峰值取 `Conn::out` 的历史峰值 | `TcpTransport`（`queued_bytes()` 已存在，**聚合层缺**） | ❌ 缺。**这是网络 Saturation 的正解**，且底层数据已经在手 |
| 6 | `otf_conn_rejected_maxfd` | counter | `AcceptNew()` 里 `!Valid(fd)` 的次数 | `TcpTransport::AcceptNew()` | ❌ **缺**。现在超 fd 上限的连接被**静默关闭** |
| 7 | `otf_fd_used` / `otf_fd_max` | gauge | `live_` / `kMaxFd` | `TcpTransport`（`live_conns()` 已存在） | ❌ 缺。**这是 File Descriptors 那一行的正解** |
| 8 | `otf_bytes_sent` / `otf_bytes_recv` | counter | 字节数（不是条数） | `main.cpp` 已有 `bytes_recv`，`Send` 侧没有 | ⚠️ **半有**。`bytes_recv` 只进 debug 日志（`LogRuntimeSummary`），没进指标 |
| 9 | `otf_resident_bytes` | gauge | 读 `/proc/self/statm` 或 `mallinfo` | `MetricsServer::Poll()` 或独立采集 | ❌ 缺。**这是 Memory capacity 的正解**；`MetricsServer` 每轮 `Poll()` 都跑在主循环线程上，**读一次 `/proc/self/statm` 的开销可以忽略** |
| 10 | `otf_alloc_failures` | counter | `malloc`/`new` 失败的次数 | 动态插桩（bpftrace / `LD_PRELOAD`）或内存耗尽时的告警 | ❌ 缺。**这是 Memory Errors 的正解**（原文稿推荐 uretprobes 追 `malloc()` 失败） |
| 11 | `otf_tcp_nodelay_failures` | counter | `setsockopt(TCP_NODELAY)` 失败的次数 | `AcceptNew()` | ❌ 缺。现在只 `warn`（**降级可用**，连接仍活着但会有 0.5s 小包延迟） |
| 12 | `otf_tcp_rst_in` | counter | `EPOLLRDHUP` / `EPOLLERR` 触发关闭的次数 | `TcpTransport::Poll()` | ❌ 缺。**这是 Network Errors 的正解** |
| 13 | `otf_live_conns` | gauge | `live_conns()` | 已在用（`otf_connections`） | ✅ **有**（`Metrics::on_connect`） |
| 14 | `otf_frames_sent` / `_recv` | counter | 帧条数 | 已在用 | ✅ 有 |
| 15 | `otf_heartbeat_timeouts` | counter | 心跳超时 | 已在用 | ✅ 有（**典型 Errors 指标**） |
| 16 | `otf_oversize_rejects` | counter | 超长帧拒收 | 已在用 | ✅ 有（**典型 Errors 指标**） |
| 17 | `otf_snapshot_send_ms_max` | gauge（峰值） | 快照发送耗时 | 已在用 | ✅ 有（**延迟类饱和的近似**） |
| 18 | `otf_tick_drift_max_ms` | gauge（峰值，带符号） | sim 与墙钟的漂移 | 已在用 | ✅ 有（**保留符号**的设计是对的——超前/落后是两种故障） |
| 19 | `otf_epoll_ctl_add/del/mod` | counter | `epoll_ctl` 调用数 | 已在用 | ✅ 有（`add`−`del` 是 fd 泄漏的早期信号） |
| 20 | `otf_epoll_ctl_add_with_out` | counter（**不变量，应恒为 0**） | 注册时带 `EPOLLOUT` 的次数 | 已在用 | ✅ 有（`main.cpp` 里非 0 还会 `log::warn`） |

### 覆盖情况小结

- **已覆盖（10 项）**：连接数、帧收发、心跳超时、超长帧、快照发送耗时、tick 漂移、epoll_ctl 三件套 + 那条不变量。**这批指标的性质很清楚——全是「应用层语义」**（这一局发生了几件事），**没有一条是「资源」指标**（这进程用了多少 CPU、多少内存、积压了多少字节）。
- **完全缺失的整块**：
  1. **CPU**：一条没有。本仓库最该有的「CPU 利用率」其实不是 `%CPU`——**是「epoll_wait 空转的比例」**（第 1 项），因为单线程 reactor 的 CPU 几乎全在 tick 上，而 tick 的耗时（第 3 项）才是真正该盯的数。
  2. **内存**：一条没有。第 9 项 `otf_resident_bytes` 的采集成本几乎为零（读一次 `/proc/self/statm`），是**投入产出比最高的一条**。
  3. **网络饱和**：第 5 项。底层数据（`queued_bytes()`）**已经在代码里了**，只差一个聚合——**这是本清单里最「白捡」的一条**。
  4. **网络错误**：第 12 项。
  5. **容量类**：第 6、7 项。**现在 fd 用满是静默的**，而 USE 方法的存在意义就是让这类事**有名字、有数字**。
- **一条不该补的**：存储。运行时零磁盘 I/O，补了就是**造一个永远为 0 的指标**——按 `metrics.h` 里那条「宁可显示怪值也不要永远健康的指标」的同款精神，**永远为 0 的指标是负资产**（它会让人误以为「磁盘这条线已经有人管了」）。

### 三条采集位置的原则（照抄本仓库已有的纪律）

1. **能差分就不要在回调里打点。** `main.cpp` 的 `SyncTransportMetrics()` 已经写明了理由：回调打点要求**每一个**增减 `live_conns()` 的地方都记得回调——那是「靠自觉」这种失败模式。改成每轮从 `live_conns()` 反推增量，`otf_connections` 就**恒等于** transport 的真实状态。
2. **采集点在主循环里，不要另开线程。** 本仓库是单线程 reactor，**另开采集线程就等于引入了并发**——而 `metrics.h` 头部那段注释明说了「原子量只是为了不让 `snapshot()` 与 `on_*()` 的调用点严格同线程，**不代表** Metrics 是多线程安全的计数方案」。所以第 9 项 `/proc/self/statm` 必须读在主循环里。
3. **峰值类指标要带符号、要带「没测过」的区分。** `metrics.h` 里 `tick_drift_max_ms` 的两条设计（用 `std::optional` 而不是哨兵值；按 `|·|` 取峰而不是 `v > cur`）**正是 USE 里 Saturation 一栏的通用要求**——饱和度经常表现为「一个方向的单调偏移」，只看正值会把最危险的一半静默吞掉。

---

## 7. 收尾：本章金句清单

1. **"This appendix contains a checklist for Linux derived from the USE method"** —— 这一章是**清单**不是论述。清单的价值是「照着做」，不是「读完懂了」。
2. **"This is a method for checking system health, and identifying common resource bottlenecks and errors"** —— USE 服务的三个目标：健康、瓶颈、错误。**它不是为了「优化」，是为了「定位」**。
3. **"Performance tools are often enhanced, and new ones are developed, so you should treat this as a starting point that will need updates."** —— 任何工具清单都会过期，**包括这一份**。别把清单刻成教条。
4. **"New observability frameworks and tools can also be developed to specifically make following the USE method easier."** —— 工具是为方法服务的；**方法稳定，工具常换**。
5. **"General notes: uptime “load average” (or /proc/loadavg) wasn’t included for CPU metrics since Linux load averages include tasks in the uninterruptible I/O state."** —— ★ **题眼级的一条**：最顺手的数字最容易被当成答案。load average 高**不等于** CPU 忙，它可能是在告诉你「磁盘堵了」。
6. **"Dropped packets are included as both saturation and error indicators, since they can occur due to both types of events."** —— ★ **同一个指标可以同时算饱和和错误**：丢包既可能是「设备忙不过来」（饱和），也可能是「链路坏了」（错误）。**分类不是恒等的**。
7. **"Not sure this one makes sense—once it’s full, ENOSPC"** —— ★ **作者在清单里留白**：查不出的格子就写「查不出」。**一个诚实的空格，胜过十个似是而非的阈值**。
8. **"This one may not make sense"** —— 文件描述符的饱和度一栏。**USE 不是用来填满表格的**。
9. **"See storage device saturation, . . ."** —— 控制器与设备是同一条链上的两级。**重复引用不是偷懒，是「瓶颈要按链查」**。
10. **"perf(1): is a powerful observability toolkit that reads PMCs and can also use dynamic and static instrumentation."** —— 工具分两层：**读硬件计数器**（PMCs）与**挂钩子**（插桩）。
11. **"PMCs: Performance monitoring counters."** —— 只有三个字母，但它指的是一整套需要硬件支持、需要权限、需要知道 CPU 型号的计数器。
12. **"I/O interconnect: This includes the CPU-to-I/O controller buses, the I/O controller(s), and device buses (e.g., PCIe)."** —— 连「I/O 控制器」都算互连的一部分。**互连是常被忽略的第四类瓶颈**。
13. **"an application could slow by 2x or more"** —— 插桩会改变被测系统。**插桩出来的数字不是生产基线**。
14. **"For any environment that imposes resource controls (e.g., cloud computing), check USE for each resource control."** —— ★ **云时代的必修课**：cgroup / quota / limit 会**先于**硬件饱和。USE 要**按限制层逐层查**。
15. **"These may be encountered—and limit usage—before the hardware resource is fully utilized."** —— 「资源还没用满，但你已经撞墙了」——**这就是饱和度与利用率必须分开量的原因**。
