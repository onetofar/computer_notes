---
title: 附录D-Appendix D： Solutions to Selected Exercises（精读）
category: 性能工程
tags:
  - 习题解答
  - 延迟
  - 负载均值
  - 内存饱和
  - 磁盘队列
  - 背压
  - 虚拟化可观测性
difficulty: 进阶
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）附录D Solutions to Selected Exercises"
source_section: "精读 §附录 D"
skills:
  - obsidian-markdown
---

# 附录D-Appendix D： Solutions to Selected Exercises

> **小节地图**：本篇是**一份习题解答清单，不是一章正文**。原文只有一级标题（`# Appendix D: Solutions to Selected Exercises`），
> 下面是 9 组「Q/A」对答，按**正文的第几章**分组（Chapter 2—Methodology / 3—Operating Systems / 6—CPUs /
> 7—Memory / 8—File Systems / 9—Disks / 11—Cloud Computing）。
> 因为原文只有一级标题，**§-定位器只有 `§附录 D` 一种**，不细分 §D.1。
> 本篇按这 7 个章节组重排为 8 节：§附录 D 定位、§附录 D 第 2 章 Methodology、§附录 D 第 3 章 Operating Systems、
> §附录 D 第 6 章 CPUs、§附录 D 第 7 章 Memory、§附录 D 第 8 章 File Systems、§附录 D 第 9 章 Disks、
> §附录 D 第 11 章 Cloud Computing。
> **底层依赖**：本篇是「结论层」，机制本体在 [[Chapter 2. Methodologies]]、[[Chapter 3. Operating Systems]]、
> [[Chapter 6. CPUs]]、[[Chapter 7. Memory]]、[[Chapter 8. File Systems]]、[[Chapter 9. Disks]]、
> [[Chapter 11. Cloud Computing]]；USE 三问的清单版在 [[附录A-Appendix A： USE Method： Linux]]。
> 解释层（白话展开）见 [[附录 D. Solutions to Selected Exercises（讲义）]]。
> ⚠️ **注意**：本篇讲的是**答案**，不是**推导**。答案背后的推理链要去对应正文章找；
> 唯一例外是第 6 章那道题——它的推导被 epub 抽取压掉了（见该节）。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。
原文是一问一答的段落体（不是表格），所以引文多为「答句」的整句或分句。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §附录 D 定位 · 附录性质 | 附录D-Appendix D： Solutions to Selected Exercises.md L25 | The following are suggested solutions to selected exercises. |
| §附录 D 定位 · 完整答案另找 | 附录D-Appendix D： Solutions to Selected Exercises.md L27 | If you manage a training course where students receive a copy of this text, contact either the publisher or myself for a full list of exercise solutions. |
| §附录 D 第 2 章 Methodology · 延迟的定义 | 附录D-Appendix D： Solutions to Selected Exercises.md L33 | A measure of time, usually time waiting for something to be done. |
| §附录 D 第 2 章 Methodology · 口径随上下文而变 | 附录D-Appendix D： Solutions to Selected Exercises.md L33 | In the IT industry, the term may be used differently depending on context. |
| §附录 D 第 3 章 Operating Systems · 线程离开 CPU 的七种原因 | 附录D-Appendix D： Solutions to Selected Exercises.md L39 | Blocked on I/O, blocked on a lock, call to yield, expired time slice, preempted by another thread, device interrupt, exiting. |
| §附录 D 第 6 章 CPUs · 题目指向正文练习 | 附录D-Appendix D： Solutions to Selected Exercises.md L43 | Calculate the load average |
| §附录 D 第 6 章 CPUs · 答案只剩一个数字 | 附录D-Appendix D： Solutions to Selected Exercises.md L45 | A. 34 |
| §附录 D 第 7 章 Memory · paging 与 swapping 的分界 | 附录D-Appendix D： Solutions to Selected Exercises.md L51 | Paging is the movement of memory pages; swapping is the movement of pages to and from swap devices/files. |
| §附录 D 第 7 章 Memory · utilization 与 saturation 的定义 | 附录D-Appendix D： Solutions to Selected Exercises.md L55 | For memory capacity, utilization is the amount that is in use and not available, measured against the total usable memory. This can be presented as a percentage, similar to file system capacity. Saturation is a measure of the demand for available memory beyond the size of memory, which usually invokes a kernel routine to free memory to satisfy this demand. |
| §附录 D 第 8 章 File Systems · 逻辑 I/O 与物理 I/O 的落点 | 附录D-Appendix D： Solutions to Selected Exercises.md L61 | Logical I/O is to the file system interface; physical I/O is to the storage devices (disks). |
| §附录 D 第 8 章 File Systems · COW 把随机写变顺序写 | 附录D-Appendix D： Solutions to Selected Exercises.md L65 | Since random writes can be written to a new location, they can be grouped (by increasing I/O size) and written out sequentially. |
| §附录 D 第 8 章 File Systems · 收益取决于设备类型 | 附录D-Appendix D： Solutions to Selected Exercises.md L65 | Both of these factors usually improve performance, depending on the storage device type. |
| §附录 D 第 9 章 Disks · 过载的第一现场 | 附录D-Appendix D： Solutions to Selected Exercises.md L71 | The disks run at a continual high utilization rate (up to 100%) with a high degree of saturation (queueing). |
| §附录 D 第 9 章 Disks · 排队推高延迟 | 附录D-Appendix D： Solutions to Selected Exercises.md L71 | Their I/O latency is increased due to the likelihood of queueing (which can be modeled). |
| §附录 D 第 9 章 Disks · 同步 I/O 才伤到应用 | 附录D-Appendix D： Solutions to Selected Exercises.md L71 | If the application is performing file system or disk I/O, the increased latency may hurt application performance, provided it is a synchronous I/O type: reads, or synchronous writes. |
| §附录 D 第 9 章 Disks · 还必须在关键路径上 | 附录D-Appendix D： Solutions to Selected Exercises.md L71 | It must also occur during a critical application code path, such as while a request is serviced, and not an asynchronous background task (which may only indirectly cause poor application performance). |
| §附录 D 第 9 章 Disks · 背压让延迟有界 | 附录D-Appendix D： Solutions to Selected Exercises.md L71 | Usually back pressure from the increased I/O latency will keep the rate of I/O requests in check and not cause an unbounded increase in latency. |
| §附录 D 第 11 章 Cloud Computing · 客户机可见的高层指标 | 附录D-Appendix D： Solutions to Selected Exercises.md L77 | Depending on the host kernel implementation, the guest can see high-level metrics of all physical resources, including CPUs and disks, and notice when they are utilized by other tenants. |
| §附录 D 第 11 章 Cloud Computing · 聚合可见、身份不可见 | 附录D-Appendix D： Solutions to Selected Exercises.md L77 | Metrics that leak user data should be blocked by the kernel. For example, utilization for a CPU may be observable (say, 50%), but not the process IDs and process names from other tenants that are causing it. |

> **书里没说的**（本篇有几处推演，原文没有，别当成原文观点）：
> - **「答案只是主干」是本篇的读法**：原文只说这些是 "suggested solutions to selected exercises"（L25），
>   并且注明完整习题解答清单需要另行索取（L27）——「作者只公开了抽样」这一点是原文的；
>   但「作者把每一章压成一句话」是本篇对**为什么是这 9 道题**的解释，原文没说。
> - **第 6 章答案 "34" 的推导是本篇的推演**：附录只留下最终数字（见该节）。
>   本篇依据正文第 6 章的负载均值定义与该章练习题给定的系统参数复原了一遍，
>   **但附录本身没有这段推导**，读者应回 [[Chapter 6. CPUs]] 与原文 §6.10 核对。
> - **第 9 章「两个条件必须同时成立」是本篇的归纳**：原文说 "provided it is a synchronous I/O type"
>   与 "must also occur during a critical application code path"，两句都是条件句；
>   把它们并成一张清单是本篇的整理。
> - **第 11 章「聚合可见、身份不可见」是本篇的概括**：原文举的例子（CPU 利用率可见、
>   别的租户的 PID 与进程名不可见）确实支持这个概括，但原文没有用这个说法。

## §附录 D 定位：这份附录是什么

**它在考什么机制**：不考机制，考的是**全书习题的取舍**。9 道题里没有一道是新的知识点——
每一道都是「这一章最容易答错、最值得单独拎出来的一句话」。

> The following are suggested solutions to selected exercises.

注意三个限定词：**suggested**（是建议，不是唯一答案）、**selected**（是抽样，不是全集）、
**exercises**（是练习题，不是正文命题）。

**结论**：这是一份**答案抽样**，作者在脚注里把完整版指向了出版商/作者本人：

> If you manage a training course where students receive a copy of this text, contact either the publisher or myself for a full list of exercise solutions.

**判断依据 / 观察方法**：拿到这份附录时，正确用法是**当索引用**，不是当教材用。
9 道题覆盖第 2、3、6、7、8、9、11 章——恰好是全书里「概念最容易被误用」的那几章
（延迟、调度、负载均值、内存、文件系统、磁盘、云）。
**没被抽到的章不代表不重要**，只代表作者没给它出一道「一句话题」。

**推演**：本篇把 9 道题按章重排后可以看出作者的偏好——
凡是「一个名词有两种意思」的章，都会被抽一道辨析题
（latency 有歧义、paging 与 swapping 是两个词、logical 与 physical I/O 是两个层次、
utilization 与 saturation 是两个概念）。这解释了为什么 7 章里有 4 章抽到的都是辨析题。

## §附录 D 第 2 章 Methodology：latency 是什么

**它在考什么机制**：考的是**「延迟」这个词的口径**，不是延迟的机制。
这是全书用得最多的一个词，也是最容易被说得含糊的一个词。

**结论**：延迟是时间的度量，通常指「等某件事做完」的那段时间。
作者同时给了一句免责声明：

> A measure of time, usually time waiting for something to be done.

> In the IT industry, the term may be used differently depending on context.

**这句免责很重要**——它不是在补充定义，而是在**收回定义的权威性**：
「延迟」在行业里的用法随上下文变，所以讨论延迟时必须先说清**测的是哪一段**。

**判断依据 / 观察方法**：正文 §2.3.1 把这个歧义讲透了——同一次网页加载可以拆成
DNS latency、TCP connection latency、TCP data transfer time，也可以笼统叫「页面加载延迟」；
所以**报延迟必须带限定词**（request latency / TCP connection latency）。
正文还给了 latency 与 response time 的关系：后者跨越前者与操作时间本身。
正文还说 latency 可以用来**排序**性能问题（因为单位都是时间），而 IOPS 这类指标做不到——
这解释了为什么本书所有瓶颈结论都先落到延迟上。配套判据在 [[Chapter 2. Methodologies]]。

## §附录 D 第 3 章 Operating Systems：线程什么时候离开 CPU

**它在考什么机制**：考**调度器的完整出口清单**。这是排障第一个要问的问题：
「进程没在跑 CPU 上，它去哪儿了？」

**结论**：七种，一条不多：

> Blocked on I/O, blocked on a lock, call to yield, expired time slice, preempted by another thread, device interrupt, exiting.

**判断依据 / 观察方法**：这七项可以按「谁决定线程离开」分三组——
**主动让出或阻塞**（blocked on I/O、blocked on a lock、call to yield）、
**被调度器夺走**（expired time slice、preempted by another thread、device interrupt）、
**结束**（exiting）。
排障时的用法是：**CPU 利用率只有 50% 却慢，先确认线程卡在这七项里的哪一项**——
D 状态（不可中断睡眠）指向 I/O 阻塞，频繁的 voluntary context switch 指向锁或 yield，
involuntary context switch 指向抢占。机制本体见 [[Chapter 3. Operating Systems]]（§3.2.9 调度器、§3.2.15 抢占）。

**推演**：这七项也是「CPU 只用了 50%」这类现象的**排查起点**：
不是「为什么 CPU 没跑满」，而是「线程去哪了」。

## §附录 D 第 6 章 CPUs：负载均值的算法

**它在考什么机制**：考**负载均值（load average）怎么算**——
用第 6 章正文 §6.6.1 的定义和 §6.10 的练习题参数做一次实算。

**结论**：附录给出的答案只有一个数字：

> A. 34

**这里有一处必须说明的信息损失**：原文稿这一处**只剩最终结果**，题面（"Calculate the load average"）
与推导过程都没有留在附录里。推导在正文第 6 章的练习题里——那题给定的系统是
「64 个 CPU、系统级 CPU 利用率 50%、系统级 CPU 饱和度（可运行 + 排队线程数均值）2.0、
处于稳态且无显著磁盘/锁负载」。

**推演（本篇复原的算法，非原文附录所有）**：按正文 §6.6.1 的定义，
load average = **系统级需求**（正在占 CPU 的线程 + 排队等着的线程），不是 CPU 利用率。
所以：64 CPU × 50% 利用率 ⇒ 平均 **32** 个线程在跑；再加上饱和度里那 **2** 个排队线程 ⇒ **34**。
这正是 "A. 34" 的来路。
⚠️ 这里的读法有一处必须挑明：那个 2.0 只能读作**在跑的之外、额外排队的线程数**，
否则与 32 个在跑的线程对不上账；读成「runnable 与 queued 合计 2.0」就推不出 34。
换句话说，**「34」这个答案反过来锁定了作者对 saturation 一词的用法**——
而那个用法的正式定义在正文 §6.6.1，不在本附录。

**判断依据 / 观察方法**：这条最容易被误用的地方是——**load average 大于 CPU 数才需要紧张**，
因为它混入了「正在跑」和「排队等」两种线程；而且 Linux 的 load average 自 1993 年起
就包含了 I/O 阻塞（D 状态）的线程（正文 §6.6.1 及其脚注 9），所以**磁盘慢会把 load average 顶高**，
哪怕 CPU 很闲。判据与 PSI 的对照表在 [[Chapter 6. CPUs]]。

## §附录 D 第 7 章 Memory：paging 与 swapping、utilization 与 saturation

这一章抽到两题，都是**同义辨析**，答案都很短，但都是全书最容易说混的地方。

### paging 与 swapping 的分界

**结论**：分界在**是否离开内存**：

> Paging is the movement of memory pages; swapping is the movement of pages to and from swap devices/files.

关键词是 **"to and from"**——**往返**于内存与 swap 设备/文件之间才是 swapping；
仅仅是页在内存里的进出（哪怕换出到存储设备）仍属 paging。

**判断依据 / 观察方法**：正文 §7.2.2 把这件事分成两类 paging：
**file system paging**（内存映射文件的页进出）与 **anonymous paging (swapping)**（进程私有数据进 swap），
并明确说 **Linux 用 "swapping" 这个词专指后者**。
所以看到 swap 活动，要问的是「有没有匿名页在往返 swap 设备」——
这正是 `vmstat` 的 si/so 两列所测的东西。

### utilization 与 saturation 的定义

**结论**：两个词分别问两个不同的问题：

> For memory capacity, utilization is the amount that is in use and not available, measured against the total usable memory. This can be presented as a percentage, similar to file system capacity. Saturation is a measure of the demand for available memory beyond the size of memory, which usually invokes a kernel routine to free memory to satisfy this demand.

- **utilization = 已经占掉且拿不回来的量 / 可用总量**（可以像文件系统容量那样报百分比）。
- **saturation = 对可用内存的**需求**超出了内存规模**，并且这个需求**会触发内核例程去释放内存**。

**判断依据 / 观察方法**：原文这半句是最有信息量的——
「usually invokes a kernel routine to free memory to satisfy this demand」——
**saturation 是有代价的**：满足它要动内核回收例程（回收页缓存、换出匿名页）。
所以「利用率低」不等于「没饱和」：利用率只有 40% 也可能已饱和，
因为剩下 60% 里有一部分是**不可回收的驻留或碎片**，而需求已经超过了这个可用规模。
**判据**：加内存前看 saturation（回收活动、swap 动作），不要只看 utilization 百分比。
USE 三问的清单版在 [[附录A-Appendix A： USE Method： Linux]]，机制在 [[Chapter 7. Memory]]（§7.2.7）。

## §附录 D 第 8 章 File Systems：logical 与 physical I/O、COW 为什么更快

这一章也抽到两题：一道是**接口分层**，一道是**因果链**。

### logical I/O 与 physical I/O 的落点

**结论**：一句话，两个落点：

> Logical I/O is to the file system interface; physical I/O is to the storage devices (disks).

**判断依据 / 观察方法**：这条切分的用处是**定位**——
应用发的是 logical I/O，磁盘做的是 physical I/O，**两者对不上是常态**。
正文 §8.3.12 给了对不上的几种形态：unrelated（别的应用/别的租户/内核任务/备份）、
indirect、implicit、inflated、deflated。
所以「应用说没发多少 I/O，磁盘却忙」不一定是应用在撒谎——文件系统的
写回、预读、元数据 I/O 都会让物理侧大于逻辑侧。机制在 [[Chapter 8. File Systems]]（§8.3.12）。

### COW 为什么能提升性能

**结论**：因果链只有两步：

> Since random writes can be written to a new location, they can be grouped (by increasing I/O size) and written out sequentially.

**判断依据 / 观察方法**：关键是**「能改写到别处」这个前提**——
随机写之所以还能被救回来，是因为 COW 不原地覆盖，而是写到新位置、
更新引用、旧块进空闲表（正文 §8.4.4）。于是随机写就可以**按 I/O 大小分组**并**顺序写出**。
但作者立刻加了限定：

> Both of these factors usually improve performance, depending on the storage device type.

**"depending on the storage device type" 是承重的限定**——收益大小取决于设备类型
（顺序写对 HDD 的意义远大于对 SSD，因为 HDD 有寻道代价）。
而且 COW 逼近容量时会造成**碎片化**，正文明确说这会降性能（尤其 HDD），需要 defrag 补救。

**推演**：所以「上 COW 就更快」是错的说法。正确说法是
**「COW 把随机写转成可排序的顺序写，收益与设备寻道代价成正比」**。

## §附录 D 第 9 章 Disks：磁盘过载时到底发生什么

这是全附录最长的一段答案，因为它是一条**因果链**，每一句都是下一句的前提。

**它在考什么机制**：考**过载如何从设备侧传导到应用侧**，
以及**什么条件下才真的伤到用户**。

**结论**：五步链条。

**第一步 · 过载现场**——利用率顶满且饱和：

> The disks run at a continual high utilization rate (up to 100%) with a high degree of saturation (queueing).

**第二步 · 排队推高延迟**：

> Their I/O latency is increased due to the likelihood of queueing (which can be modeled).

括号里那句 "which can be modeled" 是提示：**排队论模型**在这里适用
（正文 §9.3.10 给出磁盘饱和度 = 设备等待队列的平均长度）。

**第三步 · 同步 I/O 才伤到应用**：

> If the application is performing file system or disk I/O, the increased latency may hurt application performance, provided it is a synchronous I/O type: reads, or synchronous writes.

**第四步 · 还必须在关键路径上**：

> It must also occur during a critical application code path, such as while a request is serviced, and not an asynchronous background task (which may only indirectly cause poor application performance).

**第五步 · 背压让延迟有界**：

> Usually back pressure from the increased I/O latency will keep the rate of I/O requests in check and not cause an unbounded increase in latency.

**判断依据 / 观察方法**：把第三、四步读成**两个独立条件的合取**——
**同步 I/O**（读、同步写）**且** **在关键代码路径上**，磁盘过载才会真的让用户感觉到。
少任何一个都不成立：异步后台任务把盘打满，用户往往毫无感觉（第四步的 "only indirectly"）。
第五步是整段的**安全阀**：延迟升高本身会形成背压，把请求速率压住，于是延迟**不会无界增长**。

**判据（这一条最实用）**：看到「磁盘 100%」先别急——
问三个问题：① 是同步还是异步 I/O？② 在不在关键路径上？③ 有没有背压把速率压住？
三个问题都能答上来，才知道这次过载会不会变成用户投诉。
机制与工具在 [[Chapter 9. Disks]]（§9.3.10 Saturation、§9.3.12 同步与异步、§9.3.13 磁盘 I/O 与应用 I/O）。

## §附录 D 第 11 章 Cloud Computing：客户机里能看见什么

**它在考什么机制**：考**虚拟化下的可观测性边界**——
客户机能看到多少物理世界，又必须看不到什么。

**结论**：能力与限制各一句。

**能力**：客户机可以看到所有物理资源的**高层指标**，并能察觉它们被别的租户使用：

> Depending on the host kernel implementation, the guest can see high-level metrics of all physical resources, including CPUs and disks, and notice when they are utilized by other tenants.

**限制**：会泄露用户数据的指标应当被内核挡住——

> Metrics that leak user data should be blocked by the kernel. For example, utilization for a CPU may be observable (say, 50%), but not the process IDs and process names from other tenants that are causing it.

**判断依据 / 观察方法**：作者举的例子划出了一条清晰的红线——
**聚合量（CPU 利用率 50%）可见，身份信息（别的租户的 PID 与进程名）不可见**。
所以在容器/客户机里做性能分析时：你能判断「邻居占了多少」，但**不能**去追「邻居是谁、在干什么」。

正文 §11.3.4.3 用一个实测印证了这条边界：在**完全空闲**的容器里跑 `iostat -sxz 1`，
输出里 `nvme0n1` 的 `%util` 是 **100.00**——因为工具默认显示的是**宿主统计**，
包含其他租户的活动。正文因此讨论了「容器感知」工具的两种目标：
隔离成只显示本容器，或显示「本容器 vs 宿主」的对比。硬件虚拟化的 guest 则是另一回事
（§11.2.4.2：只看得到分配给自己的虚拟设备）。机制在 [[Chapter 11. Cloud Computing]]。

## 章末选型判据

| 场景 | 用什么 | 别用什么 |
|---|---|---|
| 想知道某个延迟数字指哪一段 | 带限定词报（request latency / TCP connection latency） | 笼统说「延迟是 50ms」 |
| 进程没在跑 CPU、服务却慢 | 按七项出口清单定位（I/O / 锁 / yield / 时间片 / 抢占 / 中断 / 退出） | 直接加 CPU |
| 看到 load average 高 | 先分清是「正在跑」还是「排队等」，再查 D 状态 I/O 阻塞 | 当成 CPU 利用率来读 |
| 内存要不要加 | 看 saturation（是否触发内核回收、是否有 swap 动作） | 只看内存利用率百分比 |
| 应用说没发多少 I/O，磁盘却忙 | 比对 logical 与 physical I/O（unrelated / indirect / implicit / inflated / deflated） | 认定应用在撒谎 |
| 打算用 COW 换性能 | 先看设备类型与容量水位（碎片化 + defrag） | 认定「COW 一定更快」 |
| 磁盘 100% 利用率 | 问：同步还是异步？在不在关键路径上？有没有背压？ | 直接断定「用户受影响」 |
| 在客户机/容器里看到「很忙」 | 先判断是宿主统计还是本容器统计（工具是否 container-aware） | 把邻居的负载当成自己的负载 |

> **一句话收尾**：这份附录的 9 个答案不是「知识点」，是**9 个最容易说错的定义**。
> 遇到任何一个，先回到对应正文章找机制，再拿这里的答案当**核对项**——
> 答案对了不代表懂了，只能证明没答错。
