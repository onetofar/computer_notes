---
title: 附录 D. Solutions to Selected Exercises（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - 习题解答
  - 定义
  - 延迟
  - 饱和
  - 背压
source: "《Systems Performance》（Brendan Gregg，2018，英文原版）附录 D：Solutions to Selected Exercises"
companion: "[[附录D-Appendix D： Solutions to Selected Exercises]]"
---

# 附录 D 讲义：七个问答，一份「作者亲手划的重点」

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[附录D-Appendix D： Solutions to Selected Exercises|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：`src/app/tick_scheduler.cpp`（`PollTimeoutMs` 的 `[0, 50] ms` 钳位 —— **饱和**的工程化）、`src/proto/protocol.h:224`（`kSendBufCap = 128 * 1024`）+ `src/net/tcp_transport.cpp:454`（超限**断开** —— 本仓版的**背压**）、`src/net/metrics.h:43`（`tick_drift_max_ms` —— 饱和的第一个信号）。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原附录。

> ⚠️ **这一篇是习题答案，不是章节。** 原文稿只有 77 行、
> **七个问答**（覆盖 9 个章里的 6 章）。
> 但它的信息密度出奇地高 —— **因为这些答案全是「作者认为这一章只需要记住的那一句」**。

---

## 0. 开局：十二章读完，你还是答不出一句话

你读完了《Systems Performance》的前九章，笔记记了两百页。
现在有人问你三个问题：

1. **延迟（latency）是什么？**
2. **利用率（utilization）和饱和（saturation）有什么区别？**
3. **磁盘过载了会发生什么？**

你翻遍自己的笔记，找不到一句**能直接说出口**的话。
每一章你都读了，但每一章的细节都压过了那一章的**主干**。

⇒ 这就是这份附录存在的理由。**它是作者替你压缩出来的答案。**

原文的开场只有一句半：

> The following are suggested solutions to selected exercises.

还有一条脚注：

> If you manage a training course where students receive a copy of this text, contact either the publisher or myself for a full list of exercise solutions.

⇒ **只有「选定习题」有答案**，不是全部。作者把「哪些值得出题」当成了筛选器。

---

## 1. §附录 D 这份附录的真正用途

### 1.1 它覆盖了哪些章 —— 这个名单本身就是信息

| 章 | 有答案？ | 问的是什么 |
|---|---|---|
| 2 Methodologies | ✅ | 延迟的定义 |
| 3 Operating Systems | ✅ | 线程离开 CPU 的原因 |
| 6 CPUs | ✅ | 负载平均怎么算 |
| 7 Memory | ✅ ✅ | 分页 vs 交换；利用率 vs 饱和 |
| 8 File Systems | ✅ ✅ | 逻辑 I/O vs 物理 I/O；COW 为什么快 |
| 9 Disks | ✅ | 磁盘过载会发生什么 |
| 11 Cloud Computing | ✅ | 从 guest 能看见宿主的什么 |
| 1 / 4 / 5 / 10 / 12 / 13 / 14 / 15 / 16 | ❌ | —— |

⇒ **规律很清楚**：有习题的章，都是**「有东西要算或有东西要辨」**的章
（负载平均要算、分页/交换要辨、过载的因果要推）。
没习题的章（观测工具、perf、BPF、案例研究）**是操作性的** ——
它们教的是「怎么做」，不是「是什么」，出不了问答题。

⇒ **推演（讲解性类比，非原文）**：
这份附录其实是一张**「概念主干图」** ——
它只画主干，不画枝叶。而 SP 全书十二章的枝叶极繁，
主干却只有这七八条。**背主干，再自己长枝叶**，比反过来省力得多。

### 1.2 全部是「定义题」和「因果题」，没有「操作题」

九道题里，**没有一道是「怎么做」**。
全是「**是什么**」和「**所以会怎样**」。

⇒ 这本书的作者在训练一种特定的读法：
**先问「这个东西的定义边界在哪」，再问「它超出边界会怎样」。**
你读完 SP 之后如果只会照着敲命令，那是**执行工**；
能说出「COW 为什么快」，那是**工程师**。

---

## 2. §附录 D 逐条精读

### 2.1 延迟（Ch2）：一个**承认自己有歧义**的定义

> A measure of time, usually time waiting for something to be done. In the IT industry, the term may be used differently depending on context.

两句，第二句比第一句重要。
**作者没有假装「延迟」这个词有唯一含义** —— 他明说「取决于上下文」。

⇒ 这正是 SP 第 2 章的核心主张：
**延迟不是一个标量，是「某个操作在某条路径上花了多久」的分布。**
你在书里看到的所有延迟指标（`await`、`svctm`、各种 percentile），
都是这个定义在**特定路径**上的投影。

### 2.2 线程为什么离开 CPU（Ch3）：一份**穷举清单**

> Blocked on I/O, blocked on a lock, call to yield, expired time slice, preempted by another thread, device interrupt, exiting.

七项，一个不多一个不少。这是全附录**最有实用价值的一条** ——
因为「CPU 利用率只有 50%，程序却慢」这个问题，
**答案只能从这七项里找**。

⇒ 推演：把「进程慢了」变成「填这七项里的一格」，
这是从「感觉」到「排查」的转换。**清单的价值在于它是穷举的** ——
不用你记得所有可能性，只要确认「不是这七项里的任何一个」，
就能排除一整类原因。

### 2.3 负载平均（Ch6）：答案被 epub 拍成了一个数字

原文这一节的答案是：

> A. 34

**推导过程在原文稿里丢了。** epub 抽取把一个多步计算压成了最终结果。
⇒ 这条属于**信息损失**：你从原文稿只能知道「答案是 34」，学不到算法。

⚠️ 但**书本身有**这一节（第 6 章讲负载平均的计算）。
本篇不重复正文，只标记这个缺口。

### 2.4 分页 vs 交换（Ch7）：Linux 术语的一次精确切分

> Paging is the movement of memory pages; swapping is the movement of pages to and from swap devices/files.

**关键词是 "to and from"** ——
交换是「**来回**搬」，分页只是「搬」。

⇒ 这条切分很重要，因为日常中文里「换页」和「交换」经常混用。
**「分页」可以完全不碰 swap**（进程自己的页换出、换入就是分页）；
「交换」专指**页在内存与 swap 设备之间来回搬**。

### 2.5 利用率 vs 饱和（Ch7）：全书最重要的一个区分

> For memory capacity, utilization is the amount that is in use and not available, measured against the total usable memory.

> Saturation is a measure of the demand for available memory beyond the size of memory, which usually invokes a kernel routine to free memory to satisfy this demand.

**利用率 = 已经被占掉、且拿不回来的比例。**
**饱和 = 「我还要更多」这个需求，超出了可用内存的规模。**

⇒ 推演：**利用率是「已经发生的事实」，饱和是「正在发生的需求」。**
一个系统可以**利用率只有 40% 却已经饱和** ——
因为那 60% 里有一部分是**碎片 / 不可回收 / 被缓存占着**。
**看利用率判断「要不要加内存」，会看错。**

⇒ 这条在第 12 章（基准测试）里有一个 CPU 版的孪生定义
（utilization vs saturation vs latency）。**本篇只记内存版。**

### 2.6 逻辑 I/O vs 物理 I/O（Ch8）：一次「接口层」切分

> Logical I/O is to the file system interface; physical I/O is to the storage devices (disks).

一句话，两个落点。

⇒ 这条切分的用处是**定位**：
`iostat` 看到的 `await` 是**物理**侧；
你的应用 `read()` 返回时看到的是**逻辑**侧。
**两者的差，就是文件系统自己花掉的时间。**

### 2.7 COW 为什么快（Ch8）：把「随机写」变成「顺序写」

> Since random writes can be written to a new location, they can be grouped (by increasing I/O size) and written out sequentially.

**因果链只有两步**：
随机写 → 改写到一个新位置 → 于是可以**按 I/O 大小分组** → **顺序写出**。

⇒ 关键在最后半句：**「能否顺序写」取决于「能不能改写到别处」**。
如果文件系统只能原地改（ext3 的老式方式），随机写就永远随机。
COW 换来了「可改写」⇒ 换来「可分组」⇒ 换来「可顺序」。

⇒ **推演（讲解性类比，非原文）**：这就像拼图 ——
在一个只能覆盖写、不能挪动的格子上拼，你只能反复擦同一个格子；
能挪的时候，你先把要挪的碎片堆到边上，最后一次性铺平。

### 2.8 磁盘过载（Ch9）：全附录最长的一段，因为它是一条**因果链**

> The disks run at a continual high utilization rate (up to 100%) with a high degree of saturation (queueing).

> Their I/O latency is increased due to the likelihood of queueing (which can be modeled).

> If the application is performing file system or disk I/O, the increased latency may hurt application performance, provided it is a synchronous I/O type: reads, or synchronous writes.

> It must also occur during a critical application code path, such as while a request is serviced, and not an asynchronous background task (which may only indirectly cause poor application performance).

> Usually back pressure from the increased I/O latency will keep the rate of I/O requests in check and not cause an unbounded increase in latency.

**五句话，一环扣一环**：

```
磁盘被塞满 → 利用率 100% + 饱和（排队）
          → 排队 ⇒ I/O 延迟上升
          → 延迟上升 ⇒ 「同步 I/O + 关键路径上」才会伤到应用
          → 而背压会把请求速率压住 ⇒ 延迟不会无界增长
```

⇒ 三个「必须同时成立」才伤到应用：
1. **是同步 I/O**（读，或同步写）—— 异步后台任务只是**间接**影响；
2. **在关键路径上**（比如正在服务一个请求）；
3. **磁盘确实饱和了**（不是别的瓶颈）。

⇒ 最后一句是**整段的安全阀**：
**背压把请求速率压住 ⇒ 延迟有界。**
没有背压的系统才会「延迟无界增长 ⇒ 请求堆 ⇒ 更慢」的正反馈。

### 2.9 从 guest 里能看见宿主的什么（Ch11）：可观测性的边界

> Depending on the host kernel implementation, the guest can see high-level metrics of all physical resources, including CPUs and disks, and notice when they are utilized by other tenants.

> Metrics that leak user data should be blocked by the kernel. For example, utilization for a CPU may be observable (say, 50%), but not the process IDs and process names from other tenants that are causing it.

⇒ **边界画得很清楚**：
**「聚合后的量」可以跨租户可见，「谁造成的」不可见。**
CPU 利用率 50% 能看见，**但造成这 50% 的那个 PID 和进程名不能看见**。

⇒ 这是**云上可观测性的一个根本约束**：
**你可以知道「邻居很忙」，但不能知道「邻居是谁、在干什么」。**
你在云上做的任何性能分析，都必须在这个边界内设计。

---

## 3. 实战锚点：这七条答案，在 `otf-server` 里长什么样

### 3.1 「利用率 vs 饱和」：本仓最实在的一处

`src/app/tick_scheduler.cpp` 的 `PollTimeoutMs`：

```cpp
std::int64_t t = next_tick_ms_ - now_ms;
if (t < 0) t = 0;                                  // 落后了 → 立即返回，别睡
if (t > config::kMaxPollTimeoutMs) t = config::kMaxPollTimeoutMs;
```

这就是**饱和的工程化处理**，而且两个方向都做了：

- **下界 0**：已经落后了（需求 > 供给），**立刻返回**而不是「按原计划睡到下个 tick」——
  睡了就等于**把饱和期的产能进一步砍掉**。
- **上界 50 ms**：再大就意味着 **tick 停摆**（一个 tick 周期），
  **睡眠时间本身就成了延迟来源** —— 一个「因为在等 IO 而等得比 IO 还久」的死循环。

⇒ **推演（讲解性类比，非原文）**：
利用率是「我今天用了多少台机器」，饱和是「我现在排队排了多久」；
**加机器治利用率，排队才是治饱和。**
本仓这行代码治的正是排队，不是利用率。

而**饱和的第一个信号**在 `src/net/metrics.h:43`：

```cpp
std::optional<std::int64_t> tick_drift_max_ms;
```

「tick 漂移」= 逻辑时间比墙钟慢了多少 = **饱和的直接度量**。
那个 `std::optional` 的来由（文件头注释里写着）：
取峰值用 `v > cur`、初值 0 ⇒ **所有负漂移被静默吞掉** ——
而「sim 跑得比墙钟快」正是会导致双端不同步的故障，**它恰好只在负半轴出现**。

⇒ 这是本仓**饱和度观测**的全部家当：一个峰值 + 一个符号方向 + 一个「负值不吞」的约定。

### 3.2 「磁盘过载」那一段的背压：本仓的版本是「断开」

`src/proto/protocol.h:224`：

```cpp
inline constexpr std::size_t kSendBufCap = 128 * 1024;
```

`src/net/tcp_transport.cpp:454`：

```cpp
const std::size_t queued = c.out.size() - c.out_sent;
if (queued + n > kSendBufCap) {
  // ★ 背压上限。契约只给了 true/false 两态，**没有「拒收」这一态**：
  //   若在这里返回 false 而不排入，调用方按契约去 EnableWrite，
  //   EPOLLOUT 触发时队列是空的 → 冲刷成 no-op → 这 n 字节**静默丢失**，
  //   而连接还活着。两端就此错位，且没有任何报错可查。
  //   所以超限按「对端消费不过来」处理：断开。宁可掉连接，不可静默丢数据。
  log::warn("发送队列超上限，断开该连接（背压保护）");
  Close(id);
  return false;
}
```

对照附录 D 那句：

> Usually back pressure from the increased I/O latency will keep the rate of I/O requests in check and not cause an unbounded increase in latency.

**本仓实现了这句话的同一个目标，但用的是另一种手段**：

| | 教科书式的背压 | 本仓 |
|---|---|---|
| 触发 | 对端慢 ⇒ 发送方**变慢** | 对端慢 ⇒ 发送队列超 128 KiB |
| 动作 | **阻塞 / 排队** | **断开连接** |
| 延迟 | 有界 | 有界（连接直接死） |
| 语义 | 不丢数据 | **不丢数据**（宁可掉连接） |

⇒ 注释里那三行推理（**「返回 false 而不排入 ⇒ 调用方 EnableWrite ⇒ 队列空 ⇒ 冲刷 no-op ⇒ 静默丢字节」）**
是这一段最值钱的东西：**背压做得不对，症状是「数据静默丢失、两端错位、日志里什么都没有」。**
**这比「延迟高」严重一个数量级。**

⇒ 而这正好呼应 2.8 那条：**背压的作用是「把速率压住」，让延迟有界。**
本仓的做法是**用连接的生命周期换延迟的有界**。

### 3.3 「线程为什么离开 CPU」那份清单：单线程 reactor 的答案

把 2.2 那七项套到本仓：

| 原因 | 本仓 |
|---|---|
| Blocked on I/O | ✅ **主要项** —— `transport.Poll` 里的 `epoll_wait` |
| Blocked on a lock | ❌ 零锁 |
| Call to yield | ❌ 单线程，无同优先级可让 |
| Expired time slice | ❌ 单线程，无抢占 |
| Preempted by another thread | ❌ 同上 |
| Device interrupt | ✅ 会发生，但**不在用户态** |
| Exiting | 进程结束 |

⇒ **本仓的 CPU 利用率低，只可能是一个原因：`epoll_wait` 在等。**
而「等得对不对」由 3.1 那个 `[0, 50] ms` 钳位保证。

⇒ 这条清单反过来给了一个**健康检查**：
**如果你在本仓看到 CPU 只有 20% 而延迟很高，那不像是「线程离开 CPU」的问题，
而像是「等待时间被算错了」或「`Poll` 的超时钳位被改坏了」。**
清单的价值就在这里：**它把「慢」缩小成一个可枚举的检查表。**

---

## 4. 收尾：本章金句清单

1. **The following are suggested solutions to selected exercises.** —— 只有「选定习题」有答案：**作者的出题本身就是重点筛选器**。
2. **If you manage a training course where students receive a copy of this text, contact either the publisher or myself for a full list of exercise solutions.** —— 完整答案要另找；这份是抽出来的**主干**。
3. **A measure of time, usually time waiting for something to be done. In the IT industry, the term may be used differently depending on context.** —— 延迟的定义，以及作者**主动承认它有歧义**。
4. **Blocked on I/O, blocked on a lock, call to yield, expired time slice, preempted by another thread, device interrupt, exiting.** —— 线程离开 CPU 的**穷举七项**：「CPU 只用 50% 却慢」的排查起点。
5. **Paging is the movement of memory pages; swapping is the movement of pages to and from swap devices/files.** —— 分页 vs 交换，分水岭在 **"to and from"**。
6. **For memory capacity, utilization is the amount that is in use and not available, measured against the total usable memory.** —— 利用率：**已经发生的事实**。
7. **Saturation is a measure of the demand for available memory beyond the size of memory, which usually invokes a kernel routine to free memory to satisfy this demand.** —— 饱和：**正在发生的需求**；利用率 40% 也可以是饱和的。
8. **Logical I/O is to the file system interface; physical I/O is to the storage devices (disks).** —— 两者的差 = 文件系统自己花掉的时间。
9. **Since random writes can be written to a new location, they can be grouped (by increasing I/O size) and written out sequentially.** —— COW 的因果链只有两步，但**「能改写」是「能顺序写」的前提**。
10. **The disks run at a continual high utilization rate (up to 100%) with a high degree of saturation (queueing).** —— 过载的第一现场：利用率顶满 **且** 饱和排队。
11. **Their I/O latency is increased due to the likelihood of queueing (which can be modeled).** —— 饱和 ⇒ 排队 ⇒ 延迟上升，**这条链是可以建模的**。
12. **Usually back pressure from the increased I/O latency will keep the rate of I/O requests in check and not cause an unbounded increase in latency.** —— **背压是安全阀**：速率被压住 ⇒ 延迟有界。
13. **Depending on the host kernel implementation, the guest can see high-level metrics of all physical resources, including CPUs and disks, and notice when they are utilized by other tenants.** —— 云上能看见「**量**」。
14. **Metrics that leak user data should be blocked by the kernel.** —— 云上看不见「**谁**」：聚合可见，身份不可见。
