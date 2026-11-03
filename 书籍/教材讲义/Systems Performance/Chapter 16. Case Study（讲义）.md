---
title: Chapter 16. Case Study（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - 案例研究
  - 性能定位
  - 方法论
  - USE方法
  - 性能工程
source: "《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg，Addison-Wesley 2020，英文原版）第 16 章"
companion: "[[16-Chapter 16. Case Study]]"
---

# 第 16 章 讲义：一次完整的性能定位长什么样

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[16-Chapter 16. Case Study|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：`/home/xier/dev/game-project/otf-server/` —— 本章把第 1–15 章的方法论**套回这个仓库**，走一遍完整的定位流程。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原章。

---

## 0. 开局：一次「说不清快在哪」的胜利

同一个二进制，同一份客户端负载，两台机器。

- **serverA**：共享的开发机。除了跑服务端，还跑着 IDE 的索引、别的服务，偶尔有人 ssh 进来编译东西。
- **serverB**：空闲的测试机。什么都没有。

跑同样的负载，serverB 端到端快了三倍。

三倍。这不是「快了一点」，这是**数量级上的差别**。按第 12 章的规矩，这种数字**默认是假的**——除非你能说清它是怎么来的。于是问题来了：

> **这 3 倍里，有多少是我的代码的性能，有多少是环境的？**

如果答案是「环境的」，那这个 3 倍**不可复制**——换台机器就没了，你的团队记住了一个假结论，下次还会按它做决策。如果答案是「我的代码的」，那就有 3 倍的真金白银躺在某个地方没人捡。

这一章没有新知识点。它是**第 1–15 章的合流**：把统计、配置、硬件计数器、软件事件、追踪这五种工具按顺序用一遍，看它们怎么**拼成一个结论**。

---

## 1. §16.1 An Unexplained Win：问题陈述与分析策略

### §16.1.1 Problem Statement：先问对问题

原文的场景：Netflix 一个算用户推荐的 Java 微服务，从 AWS EC2 的**虚拟机实例**搬到 Netflix 自研的容器平台 **Titus**（也跑在 EC2 上）。搬之前请求延迟 **3–4 秒**，搬之后 **1 秒**。

作者的第一反应是：

> This sounded too good to be true, and I was asked to investigate and explain how it happened.

原文把问题拆成了两个分支（这一步是整章的骨架）：

> The problem was to explain this performance difference. If it was simply due to the container move, the microservice can expect a permanent 3-4x win by moving. If it was due to some other factor, it would be worth understanding what it was and if it will be permanent. Perhaps it can also be applied elsewhere and to a greater degree.

**分支一**（是容器这个动作带来的）→ 搬家 = 永久赚 3–4 倍。
**分支二**（是别的因素）→ 值得搞清楚那是什么、会不会永久、能不能**复制到别处、放大**。

注意分支二的后半句——这才是性能工程的真正价值：**把一次偶然的胜利，变成一个可复制的原理。**

原文还列了两个**初始猜测**（这是好的习惯：**先写下猜测，再去证伪它**）：

1. **隔离运行的好处**：一个组件单独跑，能独占整个 CPU cache，不被另一个组件抢 → cache 命中率上升。
2. **容器平台上的 bursting**：容器能借用别的容器的空闲 CPU。

### §16.1.2 Analysis Strategy：两个决定性的选择

**选择一：造一个「可比」的现场。**

> This is an ideal situation for comparative analysis: I could run the same analysis command on both at the same time of day (same traffic mix and load) and compare the output immediately.

流量由 ELB 分流，所以 VM 和容器**同时在线、同时承压**。同一天、同一份流量、跑**同一条命令**、**立刻**对比输出。这叫**对照分析**——它把「环境差异」和「代码差异」从一开始就分开了。

> 选择二：确认自己的观测权限。

> If I had had only container access, analysis would have been much more time-consuming due to limited observability sources and kernel permissions, requiring much more inference from limited metrics rather than direct measurement.

作者有**容器宿主**的访问权（不只是容器内），所以任何工具都能跑、任何 syscall 都能发。如果只有容器内权限，就得**靠少量指标做大量推断**——结论会弱得多。（这正是第 11 章 §11.3.4.4 的 Tracing Tools 那节在讲的东西。）

**方法论的顺序**（原文原文）：

> For methodologies, I planned to start with the 60-second checklist ( Chapter 1 , Introduction , Section 1.10.1 , Linux Perf Analysis in 60s ) and the USE method ( Chapter 2 , Methodologies , Section 2.5.9 , The USE Method ), and based on their clues perform drill-down analysis ( Section 2.5.12 , Drill-Down Analysis ) and other methodologies.

**先便宜、后昂贵；先广、后窄。** 60 秒清单和 USE 方法给你**方向**，drill-down 给你**答案**。

### 对照回 otf-server：这个仓库能不能做同样的对照分析？

**一半能，一半不能**——诚实说清楚：

| 原文的做法 | otf-server 的现状 |
|---|---|
| 负载均衡器把流量劈两半，两边同时在线 | ❌ **没有负载生成器**。`tests/` 里是手写的定长脚本（`accept_k5_multi.sh` 之类），不是压测工具 |
| 对比「端到端请求延迟」 | ❌ 没有端到端延迟指标。仓库有 `otf_tick_drift_max_ms`（逻辑帧漂移），但它**不是**请求延迟 |
| 同一份流量做对照 | ⚠️ 只有 `tests/bench_data_path.cpp` 测**数据路径**（帧解码 + 帧编码），它用**同一份固定流量**做前后对比——**这是仓库版的「对照分析」，但只在数据路径这一层** |
| 有宿主权限，任何工具都能跑 | ✅ 本仓库是**裸进程**，perf / ftrace / bpftrace 都能直接挂 |

**结论**：仓库目前**做不了端到端的 A/B 对照**（没有负载生成器、没有请求延迟指标），但**做得成数据路径的 A/B**（`bench_data_path.cpp` 就是干这个的）。这个差距本身就是本章的一个产出。

---

## 2. §16.1.3 Statistics：60 秒清单

### 原文的四步

**第一步：`uptime`——看 load average，并且看趋势。**

> I checked the trends to see whether the problem was increasing, decreasing, or steady

> this is especially important in cloud environments that can automatically migrate load away from an unhealthy instance. More than once I’ve logged in to a problematic instance to find little activity, and a one-minute load average approaching zero.

**趋势比数值重要**：一个「负载在涨」的实例和一个「负载在跌」的实例，是两个完全不同的问题；而在云上，**负载可能被自动迁走**，你登上去可能看到一台空转的机器。

**第二步：`mpstat`——CPU，而且第一行告诉你有几颗 CPU。**

> mpstat(1) prints the number of CPUs as the first line.

原文的读法：VM 是 **48 CPU**，容器宿主是 **64 CPU**。这一行让 load average 变得可解释：

> the VM instance was running well into CPU saturation, because the load averages were roughly double the CPU count, whereas the container host was under-utilized.

**第三步：从 `mpstat` 的其余列里再挖出两条线索。**

- **超线程争抢**：
  > These processors have two hyperthreads per CPU core, so crossing the 50% utilization mark typically means hyperthread core contention, degrading performance.
  VM 的 %usr+%sys 是 **98%**（远超 50%），容器只有 **22%**。
- **`%sys` 高**：VM 是 **8%**，容器是 **0.38%**。原文的推断是「如果 VM 已经 CPU 饱和，这多出来的 %sys 可能包含内核上下文切换的代码路径 —— 用内核追踪或 profiling 可以确认」。**注意这个措辞：先猜，再用下一节的工具验。**

**第四步：跑完 60 秒清单剩下的命令。** `vmstat(8)` 显示 run queue 长度与 load average 相符（**确认了 load 是 CPU 型的**）；`iostat(1)` 磁盘 I/O 很少；`sar(1)` 网络 I/O 很少；`top(1)` 在容器宿主上只显示一个容器在跑。

> These commands provided statistics for the USE method, which also identified the issue of CPU load.

**然后是全章最漂亮的一段算术：**

> I’d found that the VM had a load average of 85 on a 48-CPU system, and that this load average was CPU-based.

> This meant that threads were waiting their turn roughly 77% of the time (85/48 – 1), and eliminating this time spent waiting would produce a roughly 4x speedup (1 / (1 – 0.77)).

**「负载是 CPU 型的」这个判断，把搜索空间从「整个系统」砍到「CPU」**——而且顺手算出了「如果把排队完全消掉，最多能快 4 倍」。这个 4 倍后来被 PMC 的数据交叉验证了（2.6 倍的 IPC 差），**这就叫结论互相咬合**。

### 对照回 otf-server：仓库版的 60 秒清单

**这里有一个必须点破的陷阱**：**本仓库是单线程 reactor，load average 在它身上是个坏指标。**

load average 统计的是「可运行 + 不可中断」的线程数。仓库的逻辑跑在**唯一的那个 IO 线程**里；逻辑卡住的时候，线程不是在 run queue 里排队，它是在 `epoll_wait` 里**睡着**。所以：

> **仓库逻辑卡死时，load average 可能是 0.00，而游戏已经完全卡住了。**

这不是仓库的缺陷，是**单线程模型的必然**：它用另一种方式表达了排队。

**仓库版的「run queue latency」是 `otf_tick_drift_max_ms`。**

| 原文的指标 | otf-server 的对应物 | 在哪看 |
|---|---|---|
| `uptime` 的 load average | ⚠️ **不可用**（单线程 reactor，见上） | — |
| `uptime` 的趋势 | `otf_tick_drift_max_ms` 的**变化趋势**（不是绝对值） | Prometheus 端点 `:7778` |
| `mpstat` 的 `%idle` | 进程级 CPU 占用（`pidstat -p <PID> 1`） | 外部工具 |
| `mpstat` 的 `%sys` | **syscall 速率**：`otf_epoll_ctl_add/del/mod` 的增量速率 | Prometheus 端点 |
| `vmstat` 的 run queue | `otf_tick_drift_max_ms`（**饱和度**） | Prometheus 端点 |
| `iostat` 磁盘 | ❌ **无** —— 仓库零磁盘 IO | — |
| `sar -n DEV,TCP` | ❌ **无** —— 需要 `ss -s` / `/proc/net/netstat` 手工凑 | 外部工具 |
| USE 的 utilization | `otf_frames_sent` / `otf_frames_recv` 的**速率** | Prometheus 端点 |
| USE 的 saturation | `otf_tick_drift_max_ms` | Prometheus 端点 |
| USE 的 errors | `otf_heartbeat_timeouts` / `otf_oversize_rejects` | Prometheus 端点 |

注意最后两行：`heartbeat_timeouts` 和 `oversize_rejects` 在 `src/net/metrics.h` 里被明确标注为「**攻击面指标**」。用 USE 方法的人习惯只盯 utilization 和 saturation——**而这两个 errors 恰好是这个仓库自己认出来的两类异常**。

**仓库版的「每 5 秒运行期摘要」**：`src/config/server_config.h` 的 `kRuntimeSummaryIntervalMs = 5000`，`main.cpp` 的 `LogRuntimeSummary` 每 5 秒打一次。它相当于原文的 `vmstat 1` 那种**滚动看趋势**的输出。

**本节结论**：仓库的 60 秒清单应该是——

```
1. 拉 Prometheus 端点，看 otf_tick_drift_max_ms 的趋势与 otf_frames_recv 的速率
2. pidstat -p <PID> 1  → %usr / %sys（%sys 高 = syscall 多 = IO 忙）
3. otf_epoll_ctl_add/del/mod 的速率（syscall 速率的自带版本）
4. otf_heartbeat_timeouts / otf_oversize_rejects 是否在涨（errors）
```

---

## 3. §16.1.4 Configuration：静态配置

原文查 `/proc/cpuinfo`，发现**两边的 CPU 型号不一样**：

| | VM（serverA） | 容器宿主（serverB） |
|---|---|---|
| 型号 | Intel Xeon Platinum 8175M @ 2.50GHz | Intel Xeon E5-2686 v4 @ 2.30GHz |
| **cache size** | **33792 KB（33 MB）** | **46080 KB（45 MB）** |
| cpu cores / siblings | 24 / 48 | 16 / 32 |

基频反而是 VM 略高，但 **LLC 大了 12 MB**：

> Depending on the workload, the larger cache size can make a significant difference to CPU performance.

原文的解释很克制：LLC 更大**可能**是重要因素，**但要下一节的 PMC 来验**。

### 对照回 otf-server：静态配置这一层，仓库要先查自己

**第一步，也是最容易被跳过的一步：确认两台机器跑的是同一个二进制、同一套参数。**

仓库在这一层**已经做了防线**，值得表扬：

- `CMakeLists.txt` 第 16–17 行：`if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES) set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "构建类型" FORCE)` —— **主动堵死了「忘了加 `-DCMAKE_BUILD_TYPE=Release` → 静默无优化」这个经典坑**。
- `CMakePresets.json` 只有一个 `dev` preset（RelWithDebInfo + Ninja），`dev.sh build` 强制走 `cmake --preset dev`。
- `CMakeLists.txt` 末尾 `message(STATUS "  构建类型  : ${CMAKE_BUILD_TYPE}")` —— **每次构建都会打印**。

**第二步：确认「参数」一致。** 这个仓库的静态配置散在四个地方：

| 文件 | 性质 | 关键值 |
|---|---|---|
| `src/proto/protocol.h` | **协议常量**（进协议哈希） | `kTickRateHz = 20`、`kSnapshotRateHz = 10`、`kMaxFd = 1024`、`kFrameQueueDepth = 64`、`kSendBufCap = 128 KB` |
| `src/config/game_config.h` | 玩法数值唯一产地 | `kTickIntervalMs = 50`、`kBodyPointCount = 69`、`kPartHp` … |
| `src/config/server_config.h` | **运行期参数的编译期版本** | `kListenPort = 7777`、`kMetricsPort = 7778`、`kMaxCatchupFrames = 5`、`kMaxPollTimeoutMs = 50`、`kMaxSessions = 64` |
| `config/server.yaml` | ⚠️ **给人看的副本，不是生效的那份** | 文件头明确写了这一点 |

**`server_config.h` 的文件头把「静态配置不一致」的标准形态写出来了**：

> **两处必须同时改**：单改一边不报错、不变红，只是服务器监听了另一个端口。

**这是原文那句「静态配置这一层藏着一个巨大的差异」的仓库版**——而且形态一模一样：**症状是「行为不对」，但没有任何一条测试会红**。

**第三步：仓库版的「/proc/cpuinfo」。** `main.cpp` 启动时打印：

```
tick_rate_hz=20 snapshot_rate_hz=10 max_players=4
```

**这一行的作用不是「看」，是「两台机器是不是同一套参数，一眼可证」。** 这就是仓库版的配置指纹。**凡是性能对比，第一步就是把这行输出和二进制指纹一起记下来。**

**第四步：真去查 `/proc/cpuinfo` 的 `cache size` 和 `siblings`。** 仓库**没有任何 PMC 采集设施**，但机器的 cache 大小是可以直接看的：

```
$ lscpu | grep -Ei 'cache|core|socket|thread'
```

而且 `src/proto/protocol.h` 里有一句值得记住的话（本仓库自己写的）：

> `kBodyPointCount = 69`

——**69 个轮廓点**。如果每 tick 都要对这 69 个点做距离/命中判定，那是**每帧几千次浮点运算 + 上千次分支**。这在 IPC 上是完全够得着「拖低 IPC」的量级，而它**不体现在任何一个应用层指标上**。

---

## 4. §16.1.5 PMCs：硬件计数器

原文用 `pmcarch`（Intel 架构计数器集）：

**VM：**

```
K_CYCLES  K_INSTR    IPC BR_RETIRED   BR_MISPRED  BMR% LLCREF      LLCMISS     LLC%
982412660 575706336 0.59 126424862460 2416880487  1.91 15724006692 10872315070 30.86
999621309 555043627 0.56 120449284756 2317302514  1.92 15378257714 11121882510 27.68
```

**容器：**

```
K_CYCLES  K_INSTR    IPC BR_RETIRED   BR_MISPRED  BMR% LLCREF      LLCMISS     LLC%
147523816 222396364 1.51 46053921119  641813770   1.39 8880477235  968809014  89.09
156634810 229801807 1.47 48236123575  653064504   1.35 9186609260  1183858023 87.11
```

**IPC 0.57 vs 1.52——2.6 倍。**

> The IPC was 2.6 times lower on the VM, which could explain 2.6x of this slowdown.

原文给出**两个独立原因**：

1. **超线程争抢**：
   > One reason for the lower IPC could be hyperthread contention, as the VM host was running at over 50% CPU utilization.
2. **LLC 命中率 30% vs 90%** —— 这个更致命，因为「instructions on the VM to frequently stall on main memory access, driving down IPC」。

**LLC 命中率低有三个可能原因**，原文一个不落地列了出来：

> A smaller LLC size on the VM: 33 Mbytes versus 45 Mbytes.
> A more complex workload on the VM: the full app, requiring more instruction text and data, versus the component run on the container.
> A high rate of context switches on the VM: around 2 million per second. These prevent threads from running on-CPU for long, interfering with cache warmup.

**第三个原因需要追踪工具才能验**——于是自然过渡到下一节。**这就是工具链的编排：每个工具验一件事，验完交给下一个。**

### 对照回 otf-server：仓库的 PMC 怎么看

**设施现状**：仓库**零 PMC 采集**。但本机（x86_64 Linux）可以用 `perf`：

```
perf stat -e cycles,instructions,cache-references,cache-misses,\
LLC-load-references,LLC-load-misses,branch-misses \
  -p <PID> -- sleep 10
```

读法与原文一致：

- **IPC** = `instructions / cycles`。**低于 0.7 就要警惕**（原文的 VM 就是 0.57）。
- **LLC 命中率** = 1 − `LLC-load-misses / LLC-load-references`。**低于 50% 就要警惕**（原文的 VM 是 30%）。
- **branch-misses** 直接对应原文的 `BR_MISPRED` 列。

**仓库里 IPC 会低在哪——两个具体的、可验证的怀疑点：**

**怀疑点 A：发送缓冲的 `std::vector` 反复重分配。**

`src/net/tcp_transport.h` 自己的注释写着这个缺口：

> ⚠️ **无环形缓冲区**（`c.out.resize(old + n)`）

`Conn::out` 是 `std::vector<uint8_t>`，`Send()` 往里追加时要扩容 → **realloc + memcpy**。每次扩容都是：一次分配（可能触发 page fault）→ 一次拷贝 → 旧的内存块还给分配器。**这是 cache miss 和 page fault 的稳定来源**，而且它**完全不出现在 `otf_frames_sent` 里**。

**对照之下，仓库的接收侧已经做对了**：`src/net/frame_codec.cpp` 的 `FrameDecoder` **是环形缓冲**（`RingPush` / `RingPopTo` / `RingSkip` / `RingPeekU16`，容量 `otf::kFrameQueueDepth = 64`）。

> **同一个仓库，接收侧环形化了，发送侧还没有。这是一个干净得不能再干净的「未解释的胜利」候选**——如果有人把 `Conn::out` 也改成环形缓冲，理论上能拿回一截 LLC 命中率和 page fault。而**这个假设完全可以用 `bench_data_path.cpp` 的堆分配计数器和 `perf stat` 的 page-faults 交叉验证**。

**怀疑点 B：每 tick 的轮廓点计算。** `kBodyPointCount = 69`，`src/sim/boss_anatomy.cpp` / `boss_brain.cpp` 每 tick 都要处理它们。**这部分开销不会体现在任何网络指标上**，只能靠 `perf record` 的火焰图定位。

**验证命令（第 13 章）**：

```
perf record -F 999 -g --call-graph dwarf -- ./build/dev/otf-server
# 跑一段负载后
perf report --stdio --sort symbol
perf annotate -s <热点函数>
```

---

## 5. §16.1.6 Software Events：软件事件

硬件计数器解释了「CPU 上发生了什么」，但解释不了「**为什么进程老是被赶下 CPU**」。原文用**软件事件**（`cs` = context switches）来数：

**VM：**

```
serverA# perf stat -e cs -a -I 1000
#           time             counts unit events
     1.000411740          2,063,105      cs
     2.000977435          2,065,354      cs
     3.001537756          1,527,297      cs
```

**容器：**

```
serverB# perf stat -e cs -p 1928219 -I 1000
#           time             counts unit events
     1.001931945              1,172      cs
     2.002664012              1,370      cs
     3.003441563              1,034      cs
```

**每 1000 个事件里差了三个数量级**（约 200 万/秒 vs 约 1000/秒）。

> A high rate of context switches can put more pressure on the CPU caches, which are switching between different code paths, including the kernel code to manage the context switch, and possibly different processes.

**这条链子是这样的**：上下文切换多 → 代码路径来回跳 → cache 来不及热 → IPC 掉 → 慢。**它把第 4 节（PMC）和第 6 节（追踪）接在了一起。**

### 对照回 otf-server：仓库的上下文切换从哪来

**结构性优势**：单线程 reactor，上下文切换**天然很低**——仓库不会到 200 万/秒那个量级。

**但有一个等价的、更要紧的问题**：仓库的上下文切换虽然少，**syscall 却可能不少**。每一次 `epoll_wait` 返回、每一次 `read`、每一次 `write`、每一次 `epoll_ctl` 都是一次进内核。

**仓库能自证的**：`otf_epoll_ctl_add` / `_del` / `_mod` 的**速率**就是 `epoll_ctl` syscall 的速率。`src/net/metrics.h` 里 `on_epoll_ctl(add, del, mod)` 记的是**本轮的增量**（调用方做差分），所以 Prometheus 上 `rate(otf_epoll_ctl_add[1m])` 就是现成的 syscall 速率指标。

**仓库测不到的**：`read` / `write` 的次数与字节数。**仓库现在没有这个指标。** 它需要第 15 章的 bpftrace：

```
bpftrace -e 'tracepoint:syscalls:sys_exit_read  { @reads = count(); }
             tracepoint:syscalls:sys_exit_write { @writes = count(); }' -c 100000
```

**本节最要紧的发现：一个写在自己注释里、却没有任何测试钉住的尾部延迟风险。**

`src/net/metrics_server.cpp` 的 `Serve()` 里，写侧短写的处理是：

```cpp
constexpr int kWriteAttempts = 8;
constexpr int kWriteWaitMs = 50;
...
if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
  pollfd p{fd, POLLOUT, 0};
  const int pr = ::poll(&p, 1, kWriteWaitMs);
```

**最坏情况：8 轮 × 50 ms = 400 ms 的阻塞，而它跑在游戏主循环线程上。**

仓库的注释对这件事**非常清楚**：

> ★★★ accept() **不继承** O_NONBLOCK …… 漏掉这次 fcntl 的话，下面那次 read 会在「客户端连上但还没发数据」时永久阻塞 —— 而 Poll() 跑在游戏主循环线程上，一卡就是整局卡死。

`metrics_server.h` 里也有一条硬约束：「Poll() 永不阻塞、永不抛异常、永不无限循环」。

**那 400 ms 算不算违反这条约束？算。** 它有界（不是无限），但它**就是阻塞**。

**更要命的是：现有测试盖不住它。** `tests/test_metrics.cpp` 的三条断言是：

- ① 无连接时 `Poll()` < 100 ms；
- ② 「只连不发」的客户端 → `waited < 200` ms（**盖住了读侧**）；
- ③ 多连接 → `< 500` ms，且单轮恰好服务 8 个（`kMaxServePerPoll`）。

**第 ③ 条的 500 ms 预算，足以容忍那个 400 ms 的最坏情况。** 而且触发它需要「客户端连上后不读、把接收窗口填满」——现有测试的客户端都**乖乖读数据**，永远触发不到写侧 EAGAIN。

**一旦发生，症状是什么？** 主循环被卡 400 ms → `otf_tick_drift_max_ms` 突然跳一下 → 补帧机制接管（`kMaxCatchupFrames = 5`，20 Hz 下是 **250 ms 的容忍窗口**）→ 400 ms > 250 ms → **丢帧**，客户端快照跳变（仓库的设计选择是「宁可快照跳变也不雪崩」）。

> **这就是一个「偶发、极难复现、看起来像玄学」的卡顿**——和第 11 章开头那类问题一模一样。而第 16 章教的方法在这里完全适用：**用 bpftrace 挂 `syscalls:sys_enter_poll2` / `sys_exit_poll2`，数主循环线程在 `poll` 上停了多久。**

---

## 6. §16.1.7 Tracing：追踪

最后一环：**为什么应用老是离开 CPU**。

原文用 BCC 的 `cpudist(8)`（on-CPU 时长分布）：

**VM：**

```
     usecs               : count     distribution
         0 -> 1          : 3618650  |****************************************|
         2 -> 3          : 2704935  |*****************************           |
         4 -> 7          : 421179   |****                                    |
         8 -> 15         : 99416    |*                                       |
        16 -> 31         : 16951    |                                        |
       128 -> 255        : 3400     |                                        |
      1024 -> 2047       : 8173     |                                        |
```

**容器：**

```
     usecs               : count     distribution
         0 -> 1          : 0        |                                        |
         2 -> 3          : 16       |                                        |
       512 -> 1023       : 156      |*                                       |
      2048 -> 4095       : 4511     |****************************************|
```

**VM 上典型 on-CPU 时间 < 7 μs；容器上典型 2–4 ms。** 差三个数量级。

再交叉验证一下「是谁把应用赶下去的」：

> Other tools (stackcount(8) of t:sched:sched_switch, and /proc/PID/status) showed that the application was usually leaving the CPU due to involuntary

（`/proc/PID/status` 里对应的字段叫 `nonvoluntary_ctxt_switches`。）

> Causing the application to leave the CPU after often less than 10 microseconds also does not give the CPU caches much time to warm up to the current code paths.

**闭环完成**：上下文切换多（第 5 节）→ on-CPU 时间被切碎（本节）→ cache 来不及热（第 4 节的 LLC 命中率低）→ IPC 掉 → 慢 4 倍。

### 对照回 otf-server：仓库版的「on-CPU 时间」是什么

仓库是单线程，所以「**谁把主线程赶下 CPU**」这个问题基本不存在（除了内核抢占）。但有一个**精确的等价物**：

> **每一拍（tick）里，逻辑真正跑了多久？IO 吃了多久？**

**仓库现在的答案是「不知道」。** 看 `main.cpp` 的主循环：

```cpp
transport.Poll(sched.PollTimeoutMs(now));   // IO 阶段
...
const auto post_poll = NowMs();
metrics.on_tick_drift(drift);
const TickScheduler::Result r = sched.Advance(post_poll, ...);  // 逻辑阶段
...
metrics_server.Poll();
```

**这三段之间没有任何计时。** `src/net/metrics.h` 里只有 `snapshot_send_ms_max`（**快照发送耗时**的峰值），**没有「每 tick 逻辑耗时」**。

**这就是仓库版的 `cpudist` 缺口**：想要「每一拍的时间都花在哪」，需要**先在应用里加计时**，或者**在函数边界上用 bpftrace 打点**。

**三个可落地的替代方案：**

**方案一（bpftrace，最快见效，第 15 章）——数每 tick 的工作量：**

```
bpftrace -e '
  tracepoint:syscalls:sys_exit_epoll_wait { @batches = count(); }
  interval:s:1 { printf("epoll_wait 返回 %d 次/秒\n", @batches); clear(@batches); }
'
```

「每秒 `epoll_wait` 返回多少次」= 「每秒处理了多少批连接」。这直接量化了**饱和度**。

**方案二（ftrace，第 14 章）——看 `epoll_wait` 到底睡了多久：**

```
# sys_enter / sys_exit 配对，才能算出单次 syscall 的时长
```

配对之后你会得到一张「每轮阻塞时长分布」——**这张表就是仓库版的 `cpudist`**：如果绝大多数轮是「睡满 50 ms」（说明没事干），偶尔有几轮「睡 0.01 ms 就返回」（说明有事件），那就是健康形状。**如果出现「睡 400 ms」，就是上面那个 `MetricsServer::Serve` 的写侧阻塞。**

**方案三（perf，第 13 章）——火焰图看 CPU 归属：**

```
perf record -F 999 -g --call-graph dwarf -- ./build/dev/otf-server
perf report --stdio
```

要看清楚这几个符号的占比：`TcpTransport::Poll`、`TcpTransport::OnReadable`、`TcpTransport::FlushWrite`、`FrameDecoder::Feed`、`SessionMgr` / `Room` 的 tick 路径、`sim` 层的 boss 逻辑。

**一个必须写进报告的提醒**：perf 的默认采样周期（默认 4000 Hz）在**函数很短**时会严重低估（一个 5 μs 的函数可能一次都采不到）。**要量短函数，perf 不可靠，bpftrace 的 `uretprobe` 才准**——这是第 13 章和第 15 章的分工。

---

## 7. §16.1.8 Conclusion：结论是「多条原因叠加」

原文的结论是**三条**，不是一条：

**第一条：No container neighbors（没有邻居容器）。**

> The container host was idle except for the one container. This allowed the container to have the entire CPU caches all to itself

但原文立刻给了这条结论的**保质期**：

> While this produced container-favorable results during the test, it is not the expected situation for long-term production use where neighboring containers will be the norm. The microservice may find that the 3-4x performance win vanishes when other tenants move in.

**这条不是「原因」，是「测试条件」。** 它让测试结果**不可外推**。

**第二条：LLC size and workload difference。** IPC 低 2.6 倍，三个原因（LLC 小 33 vs 45 MB、负载更复杂、上下文切换率高）。

**第三条：CPU load difference。** load average 85 / 48 CPU → 饱和 → 200 万次/秒上下文切换 + run queue latency → 「The run queue latency implied by the load averages showed the VM was running roughly 4x slower.」

> These issues explain the observed performance difference.

### 全章真正教的三件事（比结论更重要）

**（一）结论是「多条原因叠加」，不是「找到那一个原因」。**

三条原因分别解释 3-4 倍里的不同部分：IPC 差 2.6 倍解释了一部分，排队 4 倍解释了一部分，邻居条件让前两条**同时成立**。**单因归因在这里是不成立的**——这也是为什么原文说：

> Taking wrong turns, and identifying multiple contributing factors, is normal.

**走错路是正常的。** 案例研究里保留走错路的痕迹，是为了让你知道**排查该往哪个方向走**，而不是给你一个「照着做就行」的剧本。

**（二）每条结论都要标上「会不会持续」。** 原文对第一条（邻居）明确说了「不会持续」——**这一条会让整个「胜利」在生产环境消失**。这是性能报告里最容易被漏掉、也最致命的一栏。

**（三）结论要能和别的工具的结论「咬合」。** 排队算出来是 4 倍（第 3 节），IPC 差 2.6 倍（第 4 节），on-CPU 时间差 3 个数量级（第 6 节）——**三个独立工具指向同一个方向**，这才是可信的结论。单个工具的读数只是线索。

---

## 8. 实战锚点：otf-server 的一次完整定位流程

这一节是**整套讲义的合流点**：把第 1–15 章的方法论按原文的顺序，套回这个仓库。

### 8.0 先把问题写成一句可证伪的话（第 2 章 §2.5.5）

> **「在 64 个客户端同时在线时，serverB 的每 tick 处理连接数是 serverA 的 3 倍。」**

这句话有三个好处：可证伪、有数字、有对照组。**没有这三样的问题描述，不是性能问题，是抱怨。**

### 8.1 60 秒清单（第 1 章 §1.10.1）

```
1) curl -s http://127.0.0.1:7778/metrics | grep -E 'otf_(tick_drift_max_ms|frames_recv|connections)'
2) pidstat -p <PID> 1        # %usr / %sys
3) 记下 otf_epoll_ctl_add/del/mod 的增量速率
4) 看 otf_heartbeat_timeouts / otf_oversize_rejects 是否在涨
```

**判据**：`otf_tick_drift_max_ms` **稳定为 0 或缺席**（缺席 = 一次都没记录过，`metrics.h` 里明确区分了「没测过」和「测出来正好是 0」）；涨了 = 有饱和。

### 8.2 USE 三件套（第 2 章 §2.5.9）

| 维度 | 指标 | 判据 |
|---|---|---|
| **Utilization** | `otf_frames_recv` / `otf_frames_sent` 速率、`otf_connections` | 与基线比 |
| **Saturation** | `otf_tick_drift_max_ms` | 有符号峰值；负值 = sim 超前，同样是故障 |
| **Errors** | `otf_heartbeat_timeouts`、`otf_oversize_rejects` | 增长即异常 |

### 8.3 静态配置（第 2 章 §2.5.17）

```
1) 记下 main.cpp 启动时打的 tick_rate_hz / snapshot_rate_hz / max_players
2) 记下 CMakeLists 末尾打印的「构建类型」
3) ./dev.sh doctor          # 工具链版本
4) lscpu | grep -Ei 'cache|core|thread'
5) 对照 src/config/server_config.h 与 config/server.yaml（**两份必须一致**）
```

**第 5 条是仓库特有的雷**：yaml 是副本，`server_config.h` 是生效的那份，**改一边不报错**。

### 8.4 PMCs（第 6 章）

```
perf stat -e cycles,instructions,cache-misses,branch-misses,\
LLC-load-references,LLC-load-misses -p <PID> -- sleep 10
```

**判据**：IPC < 0.7 或 LLC 命中率 < 50% → 进 8.5。

### 8.5 软件事件（第 13 章 §13.5）

```
perf stat -e context-switches,cpu-migrations,task-clock,page-faults -p <PID> 1
```

**判据**：`page-faults` 持续增长 → 有人在反复分配内存 → **去查 `Conn::out` 的 `std::vector` 扩容**（这是本仓库最可疑的一处）。

### 8.6 追踪（第 14、15 章）

```
# bpftrace：每秒处理了多少批连接
bpftrace -e 'tracepoint:syscalls:sys_exit_epoll_wait { @b = count(); }
             interval:s:1 { printf("%d\n", @b); clear(@b); }'

# bpftrace：主循环线程在 poll 上停了多久（找那个 400 ms 的阻塞）
bpftrace -e 'tracepoint:syscalls:sys_enter_poll2 { @t = nsecs; }
             tracepoint:syscalls:sys_exit_poll2 /@t/ { @us = hist((nsecs - @t)/1000); }'

# perf：CPU 到底花在哪
perf record -F 999 -g --call-graph dwarf -- ./build/dev/otf-server
```

### 8.7 对照实验（第 12 章）

```
# 数据路径 A/B：同一份固定流量，只变实现
./build/dev/bench_data_path
```

`bench_data_path.cpp` 的文件头把方法论写得比很多书都清楚，值得逐字引用：

> ★ 为什么这个基准不测「QPS」而测**堆分配次数**：
>   计时器读数会被 CPU 频率、其它负载、容器调度搅动，差 10% 说明不了什么。
>   而「每帧堆分配几次」是**离散、可精确计数**的事实 —— 重载全局 operator new 把它数出来，改动前后的差异没有解释空间。

> ★ 它**不进 ctest**：
>   性能断言（"必须快过 X 倍"）在共享 CI 上必然 flaky，
>   而 flaky 的性能断言的归宿是被人加个 if 跳过 —— 那等于没有。

**这就是第 12 章「测了个寂寞」和第 16 章「完整定位」的交点**：性能断言不进 CI，但**性能计数器**可以进。

### 8.8 如果在云上：先查软件限额（第 11 章 §11.3.4.4）

> Since these limits, where present, are encountered long before the physical limits, they are more likely to be in effect and should be checked first.

**先查 cgroup 限额 / `st`（stolen），再查物理资源。** 顺序反了会浪费一整天。

### 8.9 这个仓库现在缺的六件东西

1. **没有负载生成器** → 做不了端到端的 A/B 对照（8.0 的对照组）。
2. **没有端到端延迟指标** → `otf_tick_drift_max_ms` 是逻辑漂移，不是请求延迟。
3. **没有「每 tick 逻辑耗时」指标** → `main.cpp` 的三段（`Poll` / `Tick` / `TickWorld`）之间没有计时，**仓库版的 `cpudist` 缺口**。
4. **没有 `read`/`write` 次数的计数器** → syscall 速率只覆盖了 `epoll_ctl`。
5. **`MetricsServer::Serve` 的写侧 400 ms 阻塞没有测试钉住** → 现有断言的 500 ms 预算足以容忍它，**且没有客户端触发写侧 EAGAIN**。这是本仓库最像「未解释的卡顿」的一处。
6. **零 BPF / ftrace / perf 资产** → 第 8.6 节的每一条命令都得自己校准（本机探针名、参数名、`this` 偏移）。

### 8.10 这个仓库已经做对的（值得保留的）

1. **构建类型强制 `RelWithDebInfo`**（`CMakeLists.txt` 第 16–17 行）——堵死了最常见的「静默无优化」。
2. **构建时打印构建类型**（`CMakeLists.txt` 末尾 `message(STATUS ...)`）——配置指纹。
3. **启动时打印关键参数**（`main.cpp` 的 `tick_rate_hz=... snapshot_rate_hz=... max_players=...`）——两台机器一眼可比。
4. **`metrics.h` 区分「没测过」和「测出来是 0」**（`tick_drift_max_ms` 用 `std::optional` + 独立标志位）——**这是 USE 方法里最容易被做错的一件事，仓库做对了**。
5. **指标分「累加」与「峰值」两类**（`metrics.h` 的注释：峰值指标累加起来没有运维意义）——告警语义正确。
6. **不变量单独设指标**（`epoll_ctl_add_with_out` 恒为 0，单独打点而不是混进差分调用）——**这是第 16 章「配置这一层藏着差异」的同构物，而仓库已经防住了**。
7. **纯逻辑层可注入 RNG、可重放**（`src/sim/`）——第 12 章「确定性基准」的物质基础。

---

## 9. 收尾：本章金句清单

1. **This chapter is a systems performance case study: the story of a real-world performance issue, from initial report to final resolution.** —— 案例研究的定义：从报障到收尾的完整故事。
2. **My intent in this chapter is not to introduce new technical content but to use storytelling to show how tools and methodologies may be applied in practice, in a real work environment.** —— 本章不教新知识，教**顺序**。
3. **This sounded too good to be true, and I was asked to investigate and explain how it happened.** —— 数字大到不像真的时，先怀疑数字，再怀疑世界。
4. **The problem was to explain this performance difference.** —— 性能问题不是「优化」，是**解释**。
5. **This is an ideal situation for comparative analysis: I could run the same analysis command on both at the same time of day (same traffic mix and load) and compare the output immediately.** —— 对照分析是性能工程里最强的武器：同一时刻、同一份负载、同一条命令。
6. **If I had had only container access, analysis would have been much more time-consuming due to limited observability sources and kernel permissions, requiring much more inference from limited metrics rather than direct measurement.** —— **观测权限决定结论强度**——这一点在第 11 章有更详细的展开。
7. **I checked the trends to see whether the problem was increasing, decreasing, or steady** —— 趋势比数值重要：涨、跌、平，是三个不同的问题。
8. **this is especially important in cloud environments that can automatically migrate load away from an unhealthy instance.** —— 云上「登上去发现机器空了」不是灵异事件，是负载被自动迁走了。
9. **More than once I’ve logged in to a problematic instance to find little activity, and a one-minute load average approaching zero.** —— **你在现场看到的，可能已经不是出问题的那个现场。**
10. **mpstat(1) prints the number of CPUs as the first line.** —— 一行输出，让 load average 变得可解释。
11. **the VM instance was running well into CPU saturation, because the load averages were roughly double the CPU count, whereas the container host was under-utilized.** —— 同一个数（load average），配上 CPU 数才有意义。
12. **These processors have two hyperthreads per CPU core, so crossing the 50% utilization mark typically means hyperthread core contention, degrading performance.** —— **50% 这条线**，是超线程争抢的分水岭。
13. **vmstat(8) showed run queue lengths similar to the load averages, confirming that the load averages were CPU-based.** —— 一次交叉验证：run queue 与 load average 吻合 → 负载是 CPU 型的 → 搜索空间砍掉一大半。
14. **These commands provided statistics for the USE method, which also identified the issue of CPU load.** —— 60 秒清单不是「随手敲几个命令」，它是 USE 方法的**数据采集阶段**。
15. **I’d found that the VM had a load average of 85 on a 48-CPU system, and that this load average was CPU-based.** —— 一句话结论，比一屏输出更有价值。
16. **This meant that threads were waiting their turn roughly 77% of the time (85/48 – 1), and eliminating this time spent waiting would produce a roughly 4x speedup (1 / (1 – 0.77)).** —— 把负载换算成**等待比例**，再换算成**理论上限**——这才是性能报告该给的东西。
17. **Depending on the workload, the larger cache size can make a significant difference to CPU performance.** —— 配置差异要看**负载相关**才算数；cache 大小对什么负载有影响，是要问的。
18. **One reason for the lower IPC could be hyperthread contention, as the VM host was running at over 50% CPU utilization.** —— 「could be」——作者在写假设，不是在写结论。
19. **The IPC was 2.6 times lower on the VM, which could explain 2.6x of this slowdown.** —— 把硬件计数器的比值直接换算成**性能倍数**，两个工具就此咬合。
20. **A smaller LLC size on the VM: 33 Mbytes versus 45 Mbytes.** —— 三个原因，一个一个列，不合并、不含糊。
21. **A more complex workload on the VM: the full app, requiring more instruction text and data, versus the component run on the container.** —— 「跑的负载不一样」这种差异，**比硬件差异更常见，也更容易被忽略**。
22. **A high rate of context switches on the VM: around 2 million per second. These prevent threads from running on-CPU for long, interfering with cache warmup.** —— 上下文切换不只是「切换本身贵」，它还**让 cache 热不起来**——它是一个跨层的放大器。
23. **To investigate context switches, I began with the perf(1) command to count the system-wide context switch rate.** —— 软件事件是最便宜的一步：从「数一下」开始。
24. **This output showed a rate of around two million context switches per second.** —— 一个数量级的差，比一个模糊的「慢一点」有用一万倍。
25. **cpudist(8) shows the on-CPU duration of threads.** —— **on-CPU 时长分布**：把「被抢走」这件事变成一张可读的直方图。
26. **This output shows that the application typically spent very little time on-CPU, often less than 7 microseconds.** —— 「典型 on-CPU 7 μs」——这个数字本身就是病。
27. **Other tools (stackcount(8) of t:sched:sched_switch, and /proc/PID/status) showed that the application was usually leaving the CPU due to involuntary** —— 两个独立工具指向同一件事，结论才立得住。
28. **Now the application was typically spending between 2 and 4 milliseconds on-CPU.** —— 对照组的数字必须**同样具体**，否则对比是空的。
29. **Causing the application to leave the CPU after often less than 10 microseconds also does not give the CPU caches much time to warm up to the current code paths.** —— **切碎的执行时间 = 永远热不起来的 cache = 掉下去的 IPC**——这是全章的机制闭环。
30. **While this produced container-favorable results during the test, it is not the expected situation for long-term production use where neighboring containers will be the norm.** —— **测试条件不等于生产条件**——每条结论都要标保质期。
31. **The microservice may find that the 3-4x performance win vanishes when other tenants move in.** —— 「3 倍的胜利会消失」——这种话**必须写进报告**，否则它会变成一个所有人都记得、没人更新的数字。
32. **These issues explain the observed performance difference.** —— 三条原因叠加起来，**才对得上观测到的差异**——单一原因归因在这个案例里是不成立的。
33. **Taking wrong turns, and identifying multiple contributing factors, is normal.** —— **走错路是正常的**；案例研究保留弯路，是为了告诉你该往哪走。
