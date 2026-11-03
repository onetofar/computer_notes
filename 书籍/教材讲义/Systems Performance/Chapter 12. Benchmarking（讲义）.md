---
title: Chapter 12. Benchmarking（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - 基准测试
  - 性能工程
  - 微基准
  - 主动基准测试
  - sanity check
source: "《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg，Addison-Wesley 2020，英文原版）第 12 章"
companion: "[[12-Chapter 12. Benchmarking]]"
---

# 第 12 章 讲义：基准测试最大的坑：测了个寂寞

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[12-Chapter 12. Benchmarking|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：`tests/bench_data_path.cpp`（刻意不进 ctest 的数据路径基准，重载全局 `operator new` 数「每帧堆分配次数」）、`CMakeLists.txt` 里 19 个 `add_test()` 注册、`src/sim/`（纯逻辑层，可做确定性基准）、`src/net/tcp_transport.cpp` 的 `Conn::out` 线性增长发送缓冲。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原章。

---

## 0. 开局：一块每秒写 739 KB 的"硬盘"

作者挑了一个最有名的基准工具开刀——`bonnie++`。在 Ubuntu Linux 上跑，第一行输出：

```
# bonnie++
[...]
Version  1.97       ------Sequential Output------ --Sequential Input- --Random-
Concurrency   1     -Per Chr- --Block-- -Rewrite- -Per Chr- --Block-- --Seeks--
Machine        Size K/sec %CP K/sec %CP K/sec %CP K/sec %CP K/sec  /sec %CP
ip-10-1-239-21   4G   739  99 549247  46 308024  37  1845  99 1156838  38 +++++ +++
Latency             18699us     983ms     280ms   11065us    4505us    7762us
[...]
```

第一列测试是 "Sequential Output" 和 "Per Chr"，bonnie++ 报出 **739 Kbytes/sec**。

作者做的第一件事不是相信，是**心算**：

> Sanity check: if this really was I/O per character, it would mean the system was achieving 739,000 I/O per second. The benchmark is described as testing hard drive performance, but I doubt this system can achieve that many disk IOPS.

如果真是"每字符一次 I/O"，那就意味着每秒 **73.9 万次**磁盘 IOPS。一块普通硬盘做不到。

于是他在基准**还在跑**的时候，去看了系统到底在干什么：

```
$ iostat -sxz 1
[...]
avg-cpu:  %user   %nice %system  %iowait  %steal   %idle
          11.44    0.00   38.81    0.00    0.00   49.75

Device             tps      kB/s    rqm/s   await aqu-sz  areq-sz  %util

[...]
```

**没有磁盘 I/O。** 一个 tps 都没有。

再往下，用 bpftrace 数块层事件：

```
# bpftrace -e 'tracepoint:block:* { @[probe] = count(); }'
Attaching 18 probes...
^C

@[tracepoint:block:block_dirty_buffer]: 808225
@[tracepoint:block:block_touch_buffer]: 1025678
```

依然**没有 `block:block_rq_issue`、没有 `block:block_rq_complete`**——只有缓冲区被弄脏了。

再往上看 VFS 层：

```
# bpftrace -e 'kprobe:vfs_* /comm == "bonnie++"/ { @[probe] = count(); }'
Attaching 65 probes...
^C

@[kprobe:vfs_fsync_range]: 2
@[kprobe:vfs_statx_fd]: 6
@[kprobe:vfs_open]: 7
@[kprobe:vfs_read]: 13
@[kprobe:vfs_write]: 1176936
```

`vfs_write` 被调了 **117 万次**。写多少字节？看直方图：

```
# bpftrace -e 'k:vfs_write /comm == "bonnie++"/ { @bytes = hist(arg2); }'
Attaching 1 probe...
^C

@bytes:
[1]               668839 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[2, 4)                 0 |                                                    |
[4, 8)                 0 |                                                    |
[8, 16)                0 |                                                    |
[16, 32)               1 |                                                    |
```

**全是 1 字节写。**

结论一句话：bonnie++ 的第一个测试是"往文件系统缓存里写 1 字节"，**它一次磁盘 I/O 都没产生**。而它的手册页第一行写的是 "program to test hard drive performance"。

> By analyzing the benchmark while it is running (active benchmarking), we’ve learned that the first bonnie++ test is a 1-byte file system write, which buffers in the file system cache. It does not test disk I/O, as is implied by the descriptions of bonnie++.

**这一章讲的就是这件事**：基准测试给你一个数字，你信了它，结论就建在沙子上。而这类事在业界不是个案——有一篇统计了 106 篇论文、415 个文件系统与存储基准的研究，结论是：

> In this article we survey 415 file system and storage benchmarks from 106 recent papers. We found that most popular benchmarks are flawed and many research papers do not provide a clear indication of true performance.

**"最流行"和"最可信"是反义词。** 本章的十六种失败，绝大多数不是"测错数"，是"测的根本不是你想的那个东西"。

---

## 1. §12.1.1 为什么要做基准测试

先说动机。作者列了七种理由，每一种都对应一类真实决策：

| 理由 | 典型场景 |
|---|---|
| **System design** | 比较不同系统/组件/应用，给采购决策提供价格性能比的依据 |
| **Proofs of concept** | 采购前、上生产前，测软件硬件扛不扛得住负载 |
| **Tuning** | 扫可调参数与配置项，挑出值得拿生产负载再验的那些 |
| **Development** | 非回归测试 + 极限探测：把产品推到极限，看工程 effort 该花在哪 |
| **Capacity planning** | 定系统与应用的容量上限，给建模供数，或直接找容量天花板 |
| **Troubleshooting** | 验各组件是否还能跑在最高性能上（比如测两台机之间的最大网络吞吐） |
| **Marketing** | 测产品最高性能交给市场部——作者管这叫 **benchmarketing** |

原文还有一句很实在的补充：企业内购硬件的 POC 常常要花**几周到几个月**（发货、上架、布线、装系统）；而云上"分钟级搭一个大环境、跑一轮、拆掉"，成本几乎为零，**但语言/操作系统/数据库/负载均衡器这类长周期选择依然存在**，所以基准测试并没有被云取代。

> Note that a fault-tolerant and distributed cloud environment also makes experimentation easy: when new instance types are made available, the environment may allow testing them immediately with production workloads, skipping a traditional benchmarking evaluation.

（推演）把"要不要做基准"当成"要不要花一周"是错的判断维度：真正贵的是**做完之后没人分析**——那不是一周的活，是半年后才发现结论是错的。

---

## 2. §12.1.2 有效基准测试长什么样

作者引了 [Smaalders 06]，给出一个好基准的六条：

> Repeatable : To facilitate comparisons
> Observable : So that performance can be analyzed and understood
> Portable : To allow benchmarking on competitors and across different product releases
> Easily presented : So that everyone can understand the results
> Realistic : So that measurements reflect customer-experienced realities
> Runnable : So that developers can quickly test changes

要**比较不同系统以便采购**，还得加上第七条：价格性能比（用五年资本成本来量化价格）。

但六条里最要紧的是 **Observable**。作者接着给出使用基准时你**必须**搞懂的四件事：

> What is being tested
> What the limiting factor or factors are
> Any perturbations that might affect the results
> What conclusions may be drawn from the results

并给了一条极其实用的组织建议：

> A common mistake is to have junior staff execute the benchmarks, then to bring in performance experts to explain the results after the benchmark has completed. It is best to engage the performance experts during the benchmark so they can analyze the system while it is still running.

（推演）这条其实是在说：**看现场的人要和跑现场的人同时在场**。事后解说等于看录像破案——证据早就冷了、也早被人挪过位置。

作者还举了个 1981 年的例子说明"分析"长什么样（Bill Joy 描述原始 BSD TCP/IP 栈）：

> Sending the data from our 11/750 to our 11/780 through TCP/IP takes 28 seconds. This includes all the time needed to set up and tear down the connections, for a user-user throughput of 1.2 Megabaud. During this time the 11/750 is CPU saturated, but the 11/780 has about 30% idle time. The time spent in the system processing the data is spread out among handling for the Ethernet (20%), IP packet processing (10%), TCP processing (30%), checksumming (25%), and user system call handling (15%), with no single part of the handling dominating the time in the system.

这段话的结构值得学：**先点出限制因素（CPU 饱和 vs 30% 空闲），再把时间拆成百分比，落到各组件上**。作者补了一句：能把内核 CPU 时间这样高一层地总结出来，**直到火焰图普及后才刚变得容易**——1981 年的人靠的是另一种本事。

---

## 3. §12.1.3 失败清单（上）：你测的不是你以为的东西

这一节是全章的干货：十六条清单。作者原话：

> The following sections provide a checklist of various benchmarking failures—mistakes, fallacies, and misdeeds—and how to avoid them.

### 3.1 Casual Benchmarking：测 A，量到 B，结论说 C

> Casual benchmarking: you benchmark A, but actually measure B, and conclude you’ve measured C.

作者的解释是：基准工具会给数字，但那些数字**未必反映你以为它测的东西**，于是你的结论可能是假的。

最典型的形态：很多工具声称或暗示在测**磁盘**性能，实际测的是**文件系统**性能。差别可以是**数量级**——文件系统用缓存和缓冲把磁盘 I/O 换成了内存 I/O。

作者还给了一个很到位的直觉模型：

> If you bought a thermometer that showed the temperature of the room you’re in as 1,000 degrees Fahrenheit (or Celsius), you’d immediately know that something was amiss. The same isn’t true of benchmarks, which produce numbers that are probably unfamiliar to you.

（推演）温度计的好处是**有一个你早就知道的参照系**。基准测试没有——它输出的数字对你来说是全新的，所以**荒谬值不会自己跳出来**。这解释了为什么 bonnie++ 那个 739 KB/s 数字能堂而皇之发布好几年。

### 3.2 Blind Faith：popularity ≠ validity

> The misconception that popularity equals validity is known as argumentum ad populum (Latin for “appeal to the people”).

作者说：问题甚至未必出在基准软件本身（虽然确实有 bug），**而出在对结果的解读**。

### 3.3 Numbers Without Analysis：裸数字

> Bare benchmark results, provided with no analytical details, can be a sign that the author is inexperienced and has assumed that the benchmark results are trustworthy and final.

作者的一句狠话：

> If you’ve spent less than a week studying a benchmark result, it’s probably wrong.

没时间仔细分析时，作者建议**至少把假设写出来**，跟结果一起发：

> Assuming the benchmark tool isn’t buggy
> Assuming the disk I/O test actually measures disk I/O
> Assuming the benchmark tool drove disk I/O to its limit, as intended
> Assuming this type of disk I/O is relevant for this application

这四条可以变成一张待办清单，等结果真被看上了再回去核。

### 3.4 Complex Benchmark Tools：工具本身成了瓶颈

> A common problem is one of benchmarking the benchmark —where the result reported is limited by the benchmark software itself. A common cause of this is single-threaded benchmark software.

两条选型建议：微基准**优先挑用 C 写的**；客户端模拟基准**用和真实客户端相同的语言**，以免语言差异混进来。

### 3.5 Testing the Wrong Thing：测的东西跟客户无关

> If it doesn’t actually resemble customer workloads, however, the engineering effort will optimize for the wrong behavior [Smaalders 06].

作者提醒：**基准会过时**——

> A benchmark may have tested an appropriate workload once upon a time but now hasn’t been updated for years and is testing the wrong thing.

---

## 4. §12.1.3 失败清单（中）：环境、错误、方差、扰动

### 4.6 Ignoring the Environment

> You configure a test server and run a database benchmark, only to later learn that you missed an important step: your test server is a default setup with default tunables parameters, default file system, etc.

生产机为了高 IOPS 调过参、你在默认配置上测——测的不是同一个东西。

### 4.7 Ignoring Errors（作者单独拎出来的一条）

作者举了个亲身经历：一次 web 服务器性能测试，测出平均延迟**超过一秒**。分析后发现：

> the web server did nothing at all during the test, as all requests were blocked by a firewall. All requests. The latency shown was the time it took for the benchmark client to time out and error!

**服务器全程什么都没干**。那个"一秒延迟"是客户端超时的时间。基准工具照样吐出了一个漂亮的数字。

### 4.8 Ignoring Variance

基准工具常常把真实负载**平均化**成恒定速率（因为真实世界的平均是 500 读/s、50 写/s）。作者说：

> This approach ignores variance : the rate of operations may be variable. The types of operations may also vary, and some types may occur orthogonally. For example, writes may be applied in bursts every 10 seconds (asynchronous write-back data flushing), whereas synchronous reads are steady.

突发写会在生产里把读**排队**，而恒定速率的基准**完全模拟不出来**。作者给的解法是 **Markov 模型**。

### 4.9 Ignoring Perturbations

> Will a timed system activity, such as a system backup, execute during the benchmark run? Do monitoring agents collect statistics once per minute? For the cloud, a perturbation may be caused by unseen tenants on the same host system.

作者的抗扰动策略：

> As a rule, the duration of a benchmark should not be shorter than one second. Short tests might be unusually perturbed by device interrupts (pinning the thread while performing interrupt service routines), kernel CPU scheduling decisions (waiting before migrating queued threads to preserve CPU affinity), and CPU cache warmth effects. Try running the benchmark test several times and examining the standard deviation—this should be as small as possible.

还有一条常被忽略的：**别只记总时长，要记操作的延迟分布**，好让离群点能被看见、被记录。

### 4.10 Changing Multiple Factors

> When comparing benchmark results from two tests, be careful to understand all the factors that are different between the two.

云上尤其危险——每次开实例可能落在快机器上、慢机器上、或别的租户正抢资源的机器上。作者的解法：**多开几个实例，取中位数**（或干脆记全分布）。

---

## 5. §12.1.3 失败清单（下）：数字游戏与作弊

### 5.11 Benchmark Paradox

> “If your product’s chances of winning a benchmark are 50/50, you’ll usually lose.” [Gregg 14c]

原因是**连乘**：客户买性能类产品往往要"稳赢"，于是跑好几个基准，要求**全赢**。

> The probability of winning three benchmarks = 0.5 × 0.5 × 0.5 = 0.125 = 12.5%
> The more benchmarks—with the requirement of winning them all—the worse the chances.

（推演）这条对**做基准工具的人**同样成立：你的工具越"权威"，越可能被人拿去做多轮横评——而横评里"没输"比"赢得漂亮"更要命。

### 5.12 Benchmarking the Competition

> When customers pick a product, they don’t use it for five minutes; they use it for months. During that time, they analyze and tune the product for performance, perhaps shaking out the worst issues in the first few weeks.
> You don’t have a few weeks to spend analyzing and tuning your competitor .

结果就是：你发表的是**未调优的**对手数据，而对手的客户一看就知道——**你在最想打动的那群人面前丢了信誉**。

### 5.13 Friendly Fire（友军误伤）

> In one case, I saw an engineering team had worked hard to develop a high-performing product. Key to its performance was a new technology that had yet to be documented. For the product launch, a benchmark team had been asked to provide the numbers. They didn’t understand the new technology (it wasn’t documented), they misconfigured it, and then they published numbers that undersold the product.

另一种友军误伤：拿**有性能问题的老版本**、或者**手边仅有的次优设备**去测，发表的数字不是最好成绩——而潜在客户默认你发的就是最好成绩。

作者给的检查问题：

> What is the bottleneck for this benchmark? This may be a physical resource (such as CPUs, disks, or an interconnect) that has been driven to 100% utilization

### 5.14 Misleading Benchmarks：数字没造假，是**省略**造假

> While it may not be cheating—the numbers are not fake—it is lying by omission .

作者建议：厂商基准**可以当性能上界看**——你不该指望超过它（友军误伤情况除外）。

### 5.15 Benchmark Specials：为了刷分而做的特殊优化

1993 年 TPC-A 时代的旧账：Oracle 被指控加了一个"离散事务"选项，唯一作用就是**把 TPC-A 分数吹上去**，而真实客户不会用这个选项。TPC 后来的反条款：

> All “benchmark special” implementations that improve benchmark results but not real-world performance or pricing, are prohibited.

另一种刷分方式是**按特殊定价报价**（客户根本拿不到的深度折扣），TPC 因此规定：

> TPC specifications require that the total price must be within 2% of the price a customer would pay for the configuration.

### 5.16 Cheating

编造数字。作者的结论带点黑色幽默：

> Fortunately, this is either rare or nonexistent; I’ve not seen a case of purely made-up numbers being shared, even in the most bloodthirsty of benchmarking battles.

---

## 6. §12.2–12.2.4 基准测试的类型：微基准、模拟、回放、行业标准

原文给了张谱系图（Figure 12.1），把生产负载也放进去比。三种类型：

### 6.1 Micro-Benchmarking（微基准）

用人工负载测**某一种**操作：一次文件系统 I/O、一条数据库查询、一条 CPU 指令、一次系统调用。

优点：简单——涉及的组件与代码路径少，**差异容易做根因定位**；可重复（其它组件的变异被尽可能排除）；换系统上跑得快；而且因为是刻意人工的，**不容易跟真实负载模拟搞混**。

书里按资源类型列了几个常见工具：

```
CPU : SysBench

Memory I/O : lmbench (in Chapter 6 , CPUs )

File system : fio

Disk : hdparm, dd or fio with direct I/O

Network : iperf
```

但紧跟一句警告：

> However, remember the warning from [Traeger 08]: “Most popular benchmarks are flawed.”

作者还给了一个**微基准设计矩阵**的例子（文件系统）：三个维度——顺序/随机、I/O 大小、读写方向；再乘两个因子——**工作集大小**（远小于内存 ⇒ 全缓存；远大于内存 ⇒ 把基准推向磁盘）和**线程数**（单线程测当前时钟频率下的文件系统性能；多线程打满所有 CPU ⇒ 测系统极限）。

作者提醒那个"Solaris 在 128 KB 处最好"的例子：I/O 越大不一定越好，**存在 sweet spot**（文件系统的 slab cache、内核分配器、内存页都会影响）。

还有一个容易被忽略的方向：

> Creating benchmarks that focus on top speeds has been called sunny day performance testing. So that issues are not overlooked, you also want to consider cloudy day or rainy day performance testing, which involves testing non-ideal situations, including contention, perturbations, and workload variance.

### 6.2 Simulation（模拟 / macro-benchmark）

按测出来的真实负载特征**模拟客户负载**。作者举的 NFS 例子：读 40%、写 7%、getattr 19%、readdir 1%……

模拟可以分两种：

- **stateless**：每个请求与上一个无关，按测出来的概率随机选操作类型；
- **stateful**：请求与客户端状态相关（NFS 的读写常成组出现，写后跟写的概率远高于写后跟读）——这种该用 **Markov 模型**表示状态转移。

作者也点出模拟的软肋：**会忽略方差**（同 §12.1.3 第 8 条），而且**客户用法会随时间变**，模拟得跟着改；改了又可能导致"以前发表的结果没法和新版本比了"。

### 6.3 Replay（回放）

> This sounds ideal—as good as testing in production, right? It is, however, problematic: when characteristics and delivered latency change on the server, the captured client workload is unlikely to respond naturally to these differences, which may prove no better than a simulated customer workload.

作者举的例子很妙：客户想升级存储，把**生产 trace 回放到新硬件**，结果更慢了，**单子丢了**。原因是 trace 是在**磁盘 I/O 层**抓的：老机器是 10K rpm 盘，新机器是 7200 rpm 盘但**缓存大 16 倍、CPU 更快**。真实生产负载本该**大部分从缓存返回**，而回放磁盘事件**根本没模拟出这一点**。

> As with all benchmarks, it is crucial to analyze and understand what’s going on.

### 6.4 Industry Standards

行业标准基准由独立机构维护，要求**披露配置**、**可被审计**。常见的有 TPC 系列（TPC-C / TPC-DS / TPC-E / TPC-H / TPC-VMS / TPCx-HS / TPCx-V）和 SPEC 系列（SPEC CPU 2017、SPEC Cloud IaaS 2018、SPECsfs2014、SPECvirt_sc2013 等）。

作者提醒一句很实在的：

> Industry benchmarks typically measure a custom metric based on the benchmark, which serves only for comparisons with itself.

——**它只跟自己可比**。用之前先找到那个"最接近你未来生产负载"的基准。

---

## 7. §12.3.1–12.3.2 被动基准测试 vs 主动基准测试

这是全章的**方法论主干**。

### 7.1 被动（Passive）：fire-and-forget

> This is the fire-and-forget strategy of benchmarking—where the benchmark is executed and then ignored until it has completed. The main objective is the collection of benchmark data.

它的完整步骤短得刺眼：

> Pick a benchmark tool.
> Run it with a variety of options.
> Make a slide deck of the results.
> Hand the slides to management.

被动基准的结果可能是这六种坏东西中的任何一种：

> Invalid due to benchmark software bugs
> Limited by the benchmark software (e.g., single-threaded)
> Limited by a component that is unrelated to the benchmark target (e.g., a congested network)
> Limited by configuration (performance features not enabled, not a maximum configuration)
> Subject to perturbations (and not repeatable)
> Benchmarking the wrong thing entirely

> Passive benchmarking is easy to perform but prone to errors. When performed by the vendor, it can create false alarms that waste engineering resources or cause lost sales. When performed by the customer, it can result in poor product choices that haunt the company later on.

### 7.2 主动（Active）：跑着的时候就在看

> With active benchmarking, you analyze performance while the benchmark is running—not just after it’s done—using observability tools [Gregg 14d] .

主动基准能**确认这个基准真的在测它声称要测的东西**，也能找出**系统或基准自身的真正限制因素**。另外还有两个好处：

> As a bonus, this can be a good time to develop your skills with performance observability tools. In theory, you are examining a known load and can see how it appears from these tools.

> Ideally, the benchmark can be configured and left running in steady state, so that analysis can be performed over a period of hours or days.

**bonnie++ 那一整段（iostat → bpftrace → cachestat → VFS 直方图 → strace）就是一次标准的主动基准测试。** 注意作者的顺序：**先做 sanity check（数字荒谬吗）→ 再用块层证据确认（真的没有磁盘 I/O）→ 再用 VFS 层证据确认（确实在写文件）→ 再用直方图确认写多大 → 最后用 strace 确认没有 fsync**。每一层都在回答"它到底在干什么"。

作者也提醒 strace 的代价：

> strace(1) also costs high overhead, so the benchmark results while using strace(1) should be discarded.

**——用 strace 看到的数字不能当结论。** 这是本节最实用的一条纪律之一：观测手段本身会改变被观测对象。

---

## 8. §12.3.3–12.3.5 CPU profiling、USE 方法、工作负载刻画

### 8.1 CPU Profiling

> CPU profiling of both the benchmark target and the benchmark software is worth singling out as a methodology, because it can lead to some quick discoveries.

作者的案例：一个新系统上的磁盘微基准**吞吐比老系统还差**，大家怀疑是盘或控制器不行。作者用 USE 方法一看，**盘根本不忙**，倒是内核态 CPU 有占用——那就值得看一眼火焰图。

> Browsing the stack frames showed that 62.17% of CPU samples included a function called zfs_zone_io_throttle(). I didn’t need to read the code for this function, as its name was enough of a clue: a resource control, ZFS I/O throttling, was active and artificially throttling the benchmark! This was a default setting on the new system (but not the older system) that had been overlooked when the benchmark was performed.

**函数名本身就是线索**——`zfs_zone_io_throttle`。这个例子说明：profiling 不只是"找热点"，更是"**发现一个你没设置过的限制器**"。

### 8.2 USE Method

> Applying the USE method during benchmarking can ensure that a limit is found. Either some component (hardware or software) has reached 100% utilization, or you are not driving the system to its limit.

这句话可以当**判据**用：跑完一轮，问"有哪个组件到 100% 了吗？"没有 ⇒ **你的基准没把系统压到极限**，那个数字没有说服力。

### 8.3 Workload Characterization

工作负载刻画（第 2 章的方法论）用来判断"这个基准跟你现在的生产环境像不像"。作者的顺序是：**先刻画生产负载 → 再据此选基准**。没有生产环境可分析时，就建模拟或建模。

---

## 9. §12.3.6–12.3.9 自定义基准、爬坡负载、Sanity Check、统计分析

### 9.1 Custom Benchmarks

> For simple benchmarks, it may be desirable to code the software yourself. Try to keep the program as short as possible, to avoid complexity that hinders analysis.

语言选择的警告：

> Languages that involve virtual machines, asynchronous garbage collection, and dynamic runtime compilation can be much more difficult to debug and control with reliable precision.

以及编译器优化这个坑：

> the compiler may elide simple benchmark routines if it thinks the output is unused and therefore unnecessary to calculate. Always check using other tools while the benchmark is running to confirm its operation. It may also be worth disassembling the compiled binary to see what will actually be executed.

（推演）这条在今天的语言生态里更尖锐了：JIT 会在运行中改写代码路径，GC 会在你没预期的时刻停一下——同一份基准跑两遍，执行的机器码都可能不一样。

还有个容易忽略的收益：

> Writing custom benchmarks can also reveal subtle details about the target that can prove useful later on. For example, when developing a database benchmark, you may discover that the API supports various options for improving performance that are not currently in use in the production environment,

以及一种分工方式：

> Your software may simply generate load (a load generator ) and leave the measurements for other tools. One way to perform this is to ramp load .

### 9.2 Ramping Load（爬坡负载）

> It involves adding load in small increments and measuring the delivered throughput until a limit is reached. The results can be graphed, showing a scalability profile.

原文给了一个 Perl 写的随机读负载生成器（很朴素，作者特意说"短到能整段贴出来"）：

```perl
#!/usr/bin/perl -w
#
# randread.pl - randomly read over specified file.

use strict;

my $IOSIZE = 8192;                      # size of I/O, bytes
my $QUANTA = $IOSIZE;                   # seek granularity, bytes

die "USAGE: randread.pl filename\n" if @ARGV != 1 or not -e $ARGV[0];

my $file = $ARGV[0];
my $span = -s $file;                    # span to randomly read, bytes
my $junk;

open FILE, "$file" or die "ERROR: reading $file: $!\n";

while (1) {
        seek(FILE, int(rand($span / $QUANTA)) * $QUANTA, 0);
        sysread(FILE, $junk, $IOSIZE);
}

close FILE;
```

> To avoid buffering, this uses sysread() to call the read(2) syscall directly.

作者说：为了测 Sun ZFS Storage Appliance 的极限，用的正是这套 Perl 工具；也试过更复杂的基准和别的语言，**都没能超过它**——因为瓶颈（服务端 CPU）没变。

**但爬坡必须同时量延迟**：

> When following this approach, measure latency as well as the throughput, especially the latency distribution. Once the system approaches its limit, queueing delays may become significant, causing latency to increase. If you push load too high, latency may become so high that it is no longer reasonable to consider the result as valid. Ask yourself if the delivered latency would be acceptable to a customer.

作者给的收尾例子值得抄下来：

> You use a large array of clients to drive a target system to 990,000 IOPS, which responds with an average I/O latency of 5 ms. You’d really like it to break 1 million IOPS, but the system is already reaching saturation. By adding more and more clients, you manage to scrape past 1 million IOPS; however, all operations are now heavily queued, with average latency of over 50 ms, which is not acceptable! Which result do you give marketing? (Answer: 990,000 IOPS.)

### 9.3 Sanity Check

> It includes checking whether the result would have required some component to exceed its known limits, such as network bandwidth, controller bandwidth, interconnect bandwidth, or disk IOPS. If any limit has been exceeded, it is worth investigating in more detail. In most cases, this exercise ultimately discovers that the benchmark result is bogus.

作者的算术题：NFS 服务器用 8 KB 读，报 50,000 IOPS，接的是**单个 1 Gbit/s 网口**。

> 50,000 IOPS × 8 Kbytes = 400,000 Kbytes/s, plus protocol headers. This is over 3.2 Gbits/s—well in excess of the 1 Gbit/s known limit. Something is wrong!

作者用这个算式识破过的一串"单 1 Gbit/s 口"的吞吐：

```
120 Mbytes/s (0.96 Gbit/s)

200 Mbytes/s (1.6 Gbit/s)

350 Mbytes/s (2.8 Gbit/s)

800 Mbytes/s (6.4 Gbit/s)

1.15 Gbytes/s (9.2 Gbit/s)
```

> The 120 Mbyte/s result may be fine—a 1 Gbit/s interface should reach around 119 Mbyte/s, in practice. The 200 Mbyte/s result is possible only if there was heavy traffic in both directions and this was summed; however, these are single-direction results. The 350 Mbyte/s and beyond results are clearly bogus.

方法论总结成一句：

> When you’re given a benchmark result to check, look for any simple sums you can perform on the provided numbers to discover such limits.

### 9.4 Statistical Analysis

三个阶段：**选工具与要采的指标 → 跑，采一大批数据 → 用统计方法解释、出报告**。

作者区分得很清楚：

> Unlike active benchmarking, which focuses on analysis of the system while the benchmark is running, statistical analysis focuses on analyzing the results. It is also different from passive benchmarking, in which no analysis is performed at all.

这套方法存在的理由是**只有一台"最高配"机器**，但很多团队都要用（销售 POC、市场、支持、工程、质量各来一路）。作者因此强调**采集的可靠性**（采错了后面全得重来），以及**多采统计属性**（变异、完整分布、误差范围）。

还有一条很实在的现场取证建议：

> Also collect as much performance data as possible from the running system (without harming the result due to the collection overhead) so that forensic analysis can be performed afterward on this data.

（脚注提醒：拷 `/proc` 计数器**不要用 `tar(1)`**——它会被零长度的 `/proc` 文件搞乱。）

---

## 10. §12.3.10 检查清单：六个问题

作者给了一张极短的表（ inspiration 来自第 2 章的 Performance Mantras）：

> Why not double?
> Did it break limits?
> Did it error?
> Does it reproduce?
> Does it matter?
> Did it even happen?

逐条展开：

- **Why not double?** —— 其实是在问"限制因素是谁"。回答它常常能解决整个基准问题——**因为限制因素不是你打算测的那个东西**。
- **Did it break limits?** —— 就是 §12.3.8 的 sanity check。
- **Did it error?** —— 错误路径和正常路径表现不同，高错误率会把结果带歪。
- **Does it reproduce?** —— 结果有多一致？
- **Does it matter?** —— 这个基准测的负载跟你生产需要的是否相关。> Some micro-benchmarks test individual syscalls and library calls, but your application may not even be using them.
- **Did it even happen?** —— 就是 §12.1.3 第 7 条那个防火墙故事：测出来的时间是**超时**，不是服务。

这六问的妙处在于**它们互不依赖、且几乎都能用现有数字回答**。作者随后还给了 §12.4 那一长串问题，专门用于**你手头没有目标系统**、只能看别人给的数字的时候。

---

## 11. §12.4 拿到别人的数字：先问这些

§12.4 的问题清单分四组。**通用组**（节选）：

> Does the benchmark relate to my production workload?
> What was the configuration of the system under test?
> Was a single system tested, or is this the result of a cluster of systems?
> What was the cost of the system under test?
> What was the configuration of the benchmark clients?
> What was the duration of the test? How many results were collected?
> Is the result an average or a peak? What is the average?
> What are other distribution details (standard deviation, percentiles, or full distribution details)?
> What was the limiting factor of the benchmark?
> What was the operation success/fail ratio?
> What were the operation attributes?
> Does the benchmark simulate variance, or an average workload?
> Was the benchmark result confirmed using other analysis tools? (Provide screenshots.)
> Can an error margin be expressed with the benchmark result?
> Is the benchmark result reproducible?

**CPU/内存组**问的是：什么处理器？超频没有？水冷没有？内存条几个、怎么插的？有没有关核？系统整体 CPU 利用率多少（"Lightly loaded systems can perform faster due to higher levels of turbo boosting"）？测的是真核还是超线程？内存多大、什么类型？BIOS 改过什么？

**存储组**问的是：盘几块、什么型号、什么协议、RAID 怎么组、缓存多大、写回还是直写？文件系统是什么、几个、怎么调优？**工作集多大？命中到哪一级缓存？**访问了多少个文件？

**网络组**问的是：几个网卡、什么型号、怎么配？拓扑？协议与 socket 选项？网络栈与 TCP/UDP 可调参数改过什么？

（推演）这四组问题的共同形状是：**先问"你测的环境长什么样"，再问"你测的负载长什么样"，最后才问"数字是多少"**。顺序反过来——先看数字再找理由——就正好落进 §12.1.3 第 2 条"盲目相信"。

---

## 12. §12.5 练习

原文的思考题与实作题（保留原意）：

> What is a micro-benchmark?
> What is working set size, and how might it affect the results of storage benchmarks?
> What is the reason for studying the price/performance ratio?

选一个微基准，做三件事：

> Scale a dimension (threads, I/O size...) and measure performance.
> Graph the results (scalability).
> Use the micro-benchmark to drive the target to peak performance, and analyze the limiting factor.

这三步正好对应 §12.3.7（爬坡 + 画 scalability profile）与 §12.3.4（USE 找限制因素）。

---

## 13. 实战锚点：otf-server 怎么面对"测了个寂寞"

> **诚实前提**：本仓库跑在**本地/自建**环境，不在云上——云的那部分（§12.1.1 末尾、§12.1.3 第 10 条）本章不适用。但"怎么让基准有意义"这件事，这个仓库恰好有一个**教科书级的正面对照物**。

### 13.1 仓库里有一个**刻意不进 ctest** 的基准

`tests/bench_data_path.cpp`（249 行）测的是**数据路径**：帧解码 + 帧编码。它的文件头注释把本章的核心讲了一遍（照抄）：

> ★ 为什么这个基准不测「QPS」而测**堆分配次数**：
>   计时器读数会被 CPU 频率、其它负载、容器调度搅动，差 10% 说明不了什么。
>   而「每帧堆分配几次」是**离散、可精确计数**的事实 ——
>   重载全局 operator new 把它数出来，改动前后的差异没有解释空间。
>   计时读数只作参考（每帧多少纳秒），不作为结论依据。
>
> ★ 为什么用「同一份流量」做前后对比：
>   帧的字节构成、条数、分片方式全部固定，唯一变量是实现。
>   这样「变快了」才有意义，而不是「换了个更容易的用例」。
>
> ★ 它**不进 ctest**：
>   性能断言（"必须快过 X 倍"）在共享 CI 上必然 flaky，
>   而 flaky 的性能断言的归宿是被人加个 if 跳过 —— 那等于没有。

`CMakeLists.txt` 里对应的注册（第 70–76 行）也把这套理由写在了工程文件里：

```cmake
## 数据路径基准（帧解码 / 帧编码）。**不是 ctest** —— 性能断言在共享 CI 上必然 flaky，
##   而 flaky 的性能断言的归宿是被人加个 if 跳过，那等于没有。
##   它是手工跑的工具：读数口径见 tests/bench_data_path.cpp 的文件头
##   （主读数是**堆分配次数**，不是计时器 —— 分配次数是离散可精确计数的事实，
##   计时器会被 CPU 频率与调度搅动，差 10% 说明不了什么）。
add_executable(bench_data_path tests/bench_data_path.cpp)
target_link_libraries(bench_data_path PRIVATE otf_core)
```

**对照本章，这三段注释分别命中了三条：**

| 仓库的做法 | 对应原章 |
|---|---|
| 主读数是"每帧堆分配次数"（离散、可精确计数），不是计时器 | §12.1.3 第 8 条 Ignoring Variance + §12.3.8 Sanity Check（数出来的 3 → 0 不会有"解释空间"） |
| 用**同一份流量**做前后对比，唯一变量是实现 | §12.1.3 第 10 条 Changing Multiple Factors |
| **不进 ctest**，因为性能断言在共享 CI 上必然 flaky | §12.1.3 第 3 条 Numbers Without Analysis（宁可不测，也不要一个没人敢看的红灯） |

反过来看，仓库**没有**做原章建议的那几件事，这也是诚实的缺口：

- **没有"主动基准测试"的观测面**：跑基准时没有同步抓 perf / 火焰图 / 计数器快照。原章的 `iostat` → `bpftrace` → `cachestat` → VFS 直方图 那一串，本仓库目前是空缺。
- **没有 ramping load**：没有"加压 → 找拐点 → 画 scalability profile"这个动作。
- **没有统计分布**：只有单点读数，没有多轮跑的中位数/标准差。
- **没有端到端的数字阈值**：19 个 ctest 全是**正确性**断言，没有一个卡性能。

### 13.2 微基准 vs 端到端：仓库两侧各有一个，正好能对照

**微基准侧 —— `src/sim/` 纯逻辑层**：`boss_brain` / `boss_aggro` / `boss_anatomy` / `boss_rewind` / `player_sim` / `player_hp` / `loot_roll` / `sim_types`。

它的价值在于**确定性**：`test/check_sim_purity.sh` 这条机器检查禁止 `sim/` 里出现时钟读取与全局随机（`randf()` / `randi()` / `randomize()` 一律不允许），随机只能**由调用方注入种子**——最直接的证据是 `src/sim/loot_roll.h` 里的签名：

```cpp
std::uint32_t NormalizeSeed(std::uint64_t seed);
RollResult Roll(std::uint64_t seed, const DropTable& table, ...);
```

**种子是参数，不是全局状态。** 于是纯逻辑层能做**可重放**的基准：固定种子 → 固定输出 → 测量工具的噪声被隔离在测量之外。`tests/test_world.cpp` 的文件头把这件事说得很清楚——它验三件事，其中第三件是"同一串输入 → 同一串快照字节"。

**端到端侧 —— 真 socket 真 epoll**：`tests/test_tcp_transport.cpp` 的文件头第一句就是：

> TcpTransport 集成测试 —— 真起 socket、真 accept、真收发。
> 这层测不了单测：epoll 的行为只有真内核能验。

它覆盖的是单测**覆盖不了**的东西（内核行为），代价是噪声大、依赖时序。反过来 `tests/test_session_room.cpp` 走的是另一条路——`CMakeLists.txt` 里写明"这套测试**不碰 socket**"，因为 `SessionManager` 依赖的是 `FrameSink` 接口，房间生命周期能**毫秒级**跑完。

（推演）这三档正好是 §12.2 的三层：**`src/sim/` ≈ 微基准（组件单一、变量可控）→ `test_session_room` ≈ 模拟（业务负载、无内核噪声）→ `test_tcp_transport` ≈ 端到端（真系统、噪声大）**。本仓库缺的是**中间那层的负载生成器**——`src/cli/cli_client.cpp`（1454 行）虽然有 `--demo` / `--fight` 编排，但它定位是**单客户端验收工具**（CMakeLists.txt 第 59 行的原话："刻意**不在** add_test() 里：它依赖「服务器正在监听」"），不是压测器；`test/accept_k5.sh` 那一批脚本同理。

### 13.3 拿 §12.3.10 那六个问题去问本仓库

这一节是本章最实用的部分——六个问题不需要新工具，只需要**对着已有代码和已有指标问一遍**。

**Q1 · Why not double?** —— ❓ **目前答不出来**。仓库里没有任何端到端基准能问"为什么不是两倍"。这恰好是 §12.3.7 里"问限制因素"那一步的缺失。

**Q2 · Did it break limits?** —— 仓库能自答，因为**上限都是具名常量**：
- `src/proto/protocol.h`：`kMaxPayloadSize = 1400`（单帧不触发 IP 分片）、`kFrameQueueDepth = 64`（每连接帧队列深度，满则拒连）、`kSendBufCap = 128 * 1024`（每连接待发缓冲上限）、`kMaxFd = 1024`。
- `src/net/tcp_transport.cpp` 的 `Send()` 里，`queued + n > kSendBufCap` 就**断开**——作者的注释说得很直白："宁可掉连接，不可静默丢数据。"

**这正是 §12.3.8 Sanity Check 的做法**：先算出物理上限，再问"这个数字合理吗"。仓库把上限写成了常量，于是 sanity check 可以被机器做，而不是靠人记得。

**Q3 · Did it error?** —— ✅ 仓库有现成答案。`src/net/metrics.h` 里 `on_oversize_reject()` / `on_heartbeat_timeout()` / `on_frame_sent()` / `on_frame_recv()` 都是累加器，`src/net/metrics_server.cpp` 把它渲染成独立的观测端点（与游戏端口分开，避免健康检查拖垮游戏进程）。**"测的时候有没有报错"在本仓库是可读的，不是要靠人回忆的。**

**Q4 · Does it reproduce?** —— ✅ `bench_data_path.cpp` 的答案是"**我不拿计时读数当结论**"。这比"跑三次取中位数"更保守，但也更诚实：分配次数是离散事实，计时是连续量且会被搅动。

**Q5 · Does it matter?** —— 部分能答。基准测的**帧解码 + 帧编码**是真实热路径（上行 30 Hz 输入、下行 10 Hz 快照），但它**不覆盖**房间逻辑、`World::Tick` 结算与快照产出。**"这个基准像不像我的生产负载"这个问题，在本仓库目前只回答了一半。**

**Q6 · Did it even happen?** —— ✅ 仓库有一个非常漂亮的答案。`src/net/metrics.h` 里：

> `epoll_ctl_add_with_out` —— 累计 EPOLL_CTL_ADD（含监听 socket）

它带 `EPOLLOUT` 的注册次数**应恒为 0**，因为 §6.4.1 要求注册时只监听 `EPOLLIN`。`src/net/metrics.h` 的注释写明：

> 这个指标是它在**生产环境**唯一可读的证据。变异 M2c 只有它能抓。

这正是 §12.1.3 第 7 条那个防火墙故事的**镜像版**：本仓库的 "Did it even happen?" 问的是"该发生的那次注册，发生了没有"——**用计数器回答，而不是靠相信**。

### 13.4 仓库的性能债（自己 grep 确认过的）

1. **发送侧没有环形缓冲。** `src/net/tcp_transport.h:110`：`std::vector<uint8_t> out;`（待发缓冲，`out[0..out_sent)` 已交给内核）。追加走 `src/net/tcp_transport.cpp:467-469`：

   ```cpp
   const std::size_t old = c.out.size();
   c.out.resize(old + n);
   std::memcpy(c.out.data() + old, buf, n);
   ```

   注释说得很准：`resize` **保留已有 capacity**，所以稳态下（缓冲撑到过峰值之后）这里**零分配**。但它仍然是一条**线性增长的 `std::vector`**，不是环形缓冲——没有"消费即回收头部"那类结构。

2. **解码侧有环形缓冲**（这一条与常见的"没有环形缓冲"说法相反，实际代码为准）。`src/net/frame_codec.cpp` 里有 `RingPush`(:92) / `RingPopTo`(:114) / `RingSkip`(:133) / `RingPeekU16`(:137)，默认深度引 `otf::kFrameQueueDepth`（`src/proto/protocol.h:221`，= 64）。

3. **无对象池、无零拷贝解析、无无锁队列、单线程事件循环。** `src/` 下 grep 不到任何 `std::thread` / `pthread_create`——IO 与逻辑确实没拆。

4. **零三方依赖是硬约束。** `CMakeLists.txt` 里没有 `FetchContent` / `find_package` / `ExternalProject`；`技术参考.md` §16 的决策记录 D1 行明确写着引入 C 第三方库会"违背零依赖"的决策。**这意味着 google-benchmark 这类框架在本仓库根本进不来**——`bench_data_path.cpp` 那 249 行手写工具不是"懒得用框架"，是被架构决策逼出来的。

### 13.5 一句话总结这个仓库的立场

（推演）`tests/bench_data_path.cpp` 犯的"错"——不测 QPS、不进 CI——恰恰是本章 §12.1.3 第 3 条与第 8 条的正面执行：**宁可给一个离散、可复现、不需要解释空间的事实，也不要给一个漂亮但没法信的数字**。原章的 bonnie++ 问题是"它测的不是你以为的东西还装作是"；本仓库的对策是"**只测我明确知道在测的东西，并且把它测成事实而不是读数**"。这两条其实是同一条纪律的两面。

---

## 14. 收尾：本章金句清单

1. **There are lies, damn lies and then there are performance measures.** —— 本章开篇引的题词（出自 [Anon 85]）。整本书的立场一句话说完。

2. **Benchmarks tell you only how fast the system can run the benchmark; it’s up to you to understand the result and determine how it applies to your environment.** —— 基准只回答"跑这个基准能多快"，**不回答"这对你意味着什么"**。后半句是你的活。

3. **Benchmarking is surprisingly difficult to do well, with many opportunities for mistakes and oversights.** —— "做得好很难"不是谦辞，是这一整章存在的理由。

4. **Casual benchmarking: you benchmark A, but actually measure B, and conclude you’ve measured C.** —— **三段错位**是所有基准事故的通用结构：A 是你以为在测的，B 是真在测的，C 是你写进结论的。

5. **Most popular benchmarks are flawed.** —— Traeger 等人统计 415 个基准后的结论。**流行度不是质量指标**，甚至可能是反指标（用的人越多，误用的伤害越大）。

6. **If you’ve spent less than a week studying a benchmark result, it’s probably wrong.** —— "发布前没花一周看结果"是最省事也最致命的门槛。

7. **As a rule, the duration of a benchmark should not be shorter than one second.** —— 短到会被设备中断、CPU 调度、缓存冷热搅动的测试，读数没有意义。

8. **one of benchmarking the benchmark** —— 测基准本身成了瓶颈。最常见的元凶是**单线程的基准软件**：你测的是它自己的天花板。

9. **With active benchmarking, you analyze performance while the benchmark is running—not just after it’s done** —— 事后解说等于看录像破案。**专家必须在跑的时候在场。**

10. **Either some component (hardware or software) has reached 100% utilization, or you are not driving the system to its limit.** —— USE 方法用在基准上的判据：**没有一个组件到 100%，那这个数字就没有说服力**。

11. **In most cases, this exercise ultimately discovers that the benchmark result is bogus.** —— Sanity check（用已知物理上限做算术）的结论。**会算术就能抓出一大批假数字。**

12. **Try to keep the program as short as possible, to avoid complexity that hinders analysis.** —— 基准工具越短越好读。**复杂不是严谨，是遮挡。**

13. **The more benchmarks—with the requirement of winning them all—the worse the chances.** —— 基准悖论。**"多跑几个"不是求稳，是把 50% 的赢率连乘成 12.5%。**

14. **All “benchmark special” implementations that improve benchmark results but not real-world performance or pricing, are prohibited.** —— TPC 的反条款：只涨分不涨真实体验的优化，一律禁止。**这是行业给自己立的规矩，也是唯一的兜底。**

15. **Ask yourself if the delivered latency would be acceptable to a customer.** —— 爬坡时的收尾问题。**吞吐的最后一个百分点往往是用延迟换的**，而客户买的是延迟。

16. **When you’re given a benchmark result to check, look for any simple sums you can perform on the provided numbers to discover such limits.** —— 拿到别人的数字，先做加减乘除。**这是投入产出比最高的一步审计。**
