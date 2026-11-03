---
title: Chapter 11. Cloud Computing（精读）
category: 性能工程
tags:
  - 云计算
  - 虚拟化
  - hypervisor
  - 容器
  - cgroups
  - 多租户
difficulty: 进阶
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）第11章 Cloud Computing"
source_section: "精读 §11.1 / §11.1.1 / §11.1.3 / §11.1.5 / §11.2 / §11.2.1 / §11.2.2 / §11.2.3 / §11.2.4 / §11.3 / §11.3.1 / §11.3.2 / §11.3.3 / §11.3.4 / §11.4 / §11.5 / §11.6；§11.1.2 / §11.1.4 / §11.1.6 / §11.2.4.1 / §11.2.4.2 / §11.2.4.3 / §11.3.4.1 / §11.3.4.2 / §11.3.4.3 / §11.4.1 / §11.4.2 / §11.4.3 / §11.4.4 的内容随所属父节一并精读，不单独成节；§11.7 Exercises 与 §11.8 References 不立篇（练习题与参考文献是全书共用的自测/索引材料，不承载机制陈述）"
skills:
  - obsidian-markdown
---

# Chapter 11. Cloud Computing

> **小节地图**：本章讲的不是「云是什么」，而是**云里的性能分析怎么做**。三条主线贯穿全章：
> ① 每种虚拟化技术的**开销来自哪里**；② **资源控制**能限到什么粒度；③ **从哪一侧能看到什么**（可观测性边界）。
> 立篇：§11.1（背景）、§11.1.1（实例类型）、§11.1.3（容量规划与自动扩缩容）、§11.1.5（多租户）、
> §11.2（硬件虚拟化）、§11.2.1（实现）、§11.2.2（开销）、§11.2.3（资源控制）、§11.2.4（可观测性）、
> §11.3（OS 虚拟化）、§11.3.1（实现）、§11.3.2（开销）、§11.3.3（资源控制）、§11.3.4（可观测性）、
> §11.4（轻量虚拟化）、§11.5（其他类型）、§11.6（三种技术比较）。
> **不立篇**：§11.7 Exercises、§11.8 References（自测与文献，不承载机制）。
> 底层依赖：[[Chapter 3. Operating Systems]]（特权环、内核态/用户态、cgroups 的由来）、[[Chapter 2. Methodologies]]（USE 方法等分析范式）、
> [[Chapter 9. Disks]]（设备延迟与 I/O 栈）、[[Chapter 1. Introduction]]（方法论起点）。
> 同层对照：[[Chapter 3. Operating Systems]] 讲「被测对象本身」，本篇讲「被测对象被虚拟化之后怎么测」。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §11.1.1 实例类型是可调参数 | 11-Chapter 11. Cloud Computing.md L95 | With so many options and the ease of redeploying instances, the instance type has become like a tunable parameter that can be modified as needed. This is a great improvement over the traditional enterprise model of selecting and ordering physical hardware that the company might be unable to change for years. |
| §11.1.3 容量规划与自动扩缩容 | 11-Chapter 11. Cloud Computing.md L119 | On-premises servers, and data centers, were the norm for enterprise environments. Cloud computing is very different. Server instances are inexpensive, and can be created and destroyed almost instantly. Instead of spending time planning what may be needed, companies can increase the number of server instances they use as needed , in reaction to real load. This can be done automatically via the cloud API, based on metrics from performance monitoring software. A small business or startup can grow from a single small instance to thousands, without a detailed capacity planning study as would be expected in enterprise environments. |
| §11.1.3 容量规划与自动扩缩容 | 11-Chapter 11. Cloud Computing.md L137 | Auto scaling can solve the need to quickly respond to changes in load, but it also risks overprovisioning , as pictured in Figure 11.2 . For example, a DoS attack may appear as an increase in load, triggering an expensive increase in server instances. There is a similar risk with application changes that regress performance, requiring more instances to handle the same load. Monitoring is important to verify that these increases make sense. |
| §11.1.5 多租户的邻居干扰 | 11-Chapter 11. Cloud Computing.md L189 | Since resources are shared among tenants, performance issues may be caused by noisy neighbors . For example, another guest on the same host might perform a full database dump during your peak load, interfering with your disk and network I/O. Worse, a neighbor could be evaluating the cloud provider by executing micro-benchmarks that deliberately saturate resources in order to find their limit. |
| §11.2 I/O 代理带来的开销 | 11-Chapter 11. Cloud Computing.md L227 | Both configurations may involve running an I/O proxy (e.g., using the QEMU software) in domain 0 (Xen) or the host OS (KVM), for serving guest I/O. This adds overhead to I/O, and over the years has been optimized by adding shared memory transports and other techniques. |
| §11.2.1 Nitro：没有 QEMU 代理 | 11-Chapter 11. Cloud Computing.md L253 | Nitro : Launched by AWS in 2017, this hypervisor uses parts based on KVM with hardware support for all main resources: processors, network, storage, interrupts, and timers [Gregg 17e] . No QEMU proxy is used. Nitro provides near bare-metal performance to guest VMs. |
| §11.2.2 CPU 开销与 guest exit | 11-Chapter 11. Cloud Computing.md L273 | Hardware-assisted virtualization is generally preferred, depending on the implementation and workload, while paravirtualization is used to improve the performance of some workloads (especially I/O) if the guest OS supports it. |
| §11.2.2 CPU 开销与 guest exit | 11-Chapter 11. Cloud Computing.md L279 | The rate of transitions between the guest and hypervisor, as well as the time spent in the hypervisor, can be studied as a metric of CPU overhead. These events are commonly referred to as guest exits , as the virtual CPU must stop executing inside the guest when this happens. Figure 11.6 shows CPU overhead related to guest exits inside KVM. |
| §11.2.2 内存映射：EPT/NPT | 11-Chapter 11. Cloud Computing.md L355 | The mapping, from guest virtual to host physical, can then be cached in the TLB, so that subsequent accesses can operate at normal speed—not requiring additional translation. Modern processors support MMU virtualization, so that mappings that have left the TLB can be recalled more quickly in hardware alone (page walk), without calling in to the hypervisor. The feature that supports this is called extended page tables (EPT) on Intel and nested page tables (NPT) on AMD [Milewski 11] . |
| §11.2.2 I/O 路径步数 | 11-Chapter 11. Cloud Computing.md L385 | The number of steps in the I/O path, both control and data, is critical for performance: the fewer, the better. In 2006, the KVM developers compared a privileged-guest system like Xen with KVM and found that KVM could perform I/O using half as many steps (five versus ten, although the test was performed without paravirtualization so does not reflect most modern configurations) [Qumranet 06] . |
| §11.2.3 资源控制的盲区 | 11-Chapter 11. Cloud Computing.md L417 | There are limitations on how either technology can respect guest priorities . A guest’s CPU usage is typically opaque to the hypervisor, and guest kernel thread priorities cannot typically be seen or respected. For example, a low-priority log rotation daemon in one guest may have the same hypervisor priority as a critical application server in another guest. |
| §11.2.4 可观测性边界 | 11-Chapter 11. Cloud Computing.md L473 | When pass-through or SR-IOV is used, the guest may be making I/O calls directly to hardware. This may bypass I/O paths in the hypervisor, and the statistics they typically collect. The result is that I/O can become invisible to the hypervisor, and not appear in iostat(1) or other tools. A possible workaround is to use PMCs to examine I/O-related counters, and infer I/O that way. |
| §11.3 OS 虚拟化：只有一个内核 | 11-Chapter 11. Cloud Computing.md L789 | A key difference from hardware virtualization technologies is that only one kernel is running. The following are the performance advantages of containers over hardware VMs ( Section 11.2 , Hardware Virtualization ): |
| §11.3.2 多租户争抢内核资源 | 11-Chapter 11. Cloud Computing.md L1055 | Kernel execution can encounter additional contention for buffers, caches, queues, and locks, because a multi-tenant container system can increase their load by an order of magnitude or more. Such contention can slightly degrade application performance, depending on the kernel resource and its scalability characteristics. |
| §11.3.3 CPU shares 与带宽上限 | 11-Chapter 11. Cloud Computing.md L1149 | container CPU = (all CPUs × container shares) / total busy shares on system |
| §11.3.3 CPU shares 与带宽上限 | 11-Chapter 11. Cloud Computing.md L1159 | A problem with shares is that bursting can confuse capacity planning, especially since many monitoring systems do not show bursting statistics (they should). An end user testing a container may be satisfied with its performance, unaware that this performance is only made possible by bursting. Later on, when other tenants move in, their container can no longer burst and will suffer lower performance. Imagine container A initially testing on an idle system and getting 100% CPU, but later on only getting 10% as other containers have since been added. I’ve seen this scenario happen in real life on numerous occasions, where the end user thinks that there must be a system performance issue and asks me to help debug it. They are then disappointed to learn that the system is working as intended, and that ten-times slower is the new norm because other containers have moved in. To the customer, this can feel like bait and switch. |
| §11.3.4 可观测性：容器内的视角 | 11-Chapter 11. Cloud Computing.md L1271 | From the guests : The container can typically only see its own processes, file systems, network interfaces, and TCP sessions. A major exception is system-wide statistics, such as for CPUs and disks: these often show the host rather than just the container. The status of these statistics is typically undocumented (I have written my own documentation in the following section, Traditional Tools ). Kernel internals typically cannot be inspected, so performance tools that use kernel tracing frameworks ( Chapters 13 to 15 ) usually do not work. |
| §11.3.4 可观测性：容器内的视角 | 11-Chapter 11. Cloud Computing.md L1277 | Many monitoring tools written for stand-alone systems have yet to develop support for OS virtualization (containers), and are blind to cgroup and other software limits. Customers attempting to use these in the containers may find that they appear to work, but are in fact only showing physical system resources. Without support for observing cloud resource controls, these tools may falsely report that systems have headroom when in fact they have hit software limits. They may also show high resource usage that is actually due to other tenants. |
| §11.4 轻量虚拟化 | 11-Chapter 11. Cloud Computing.md L1794 | To describe the difference this makes: quick emulator (QEMU) is a full-machine hardware hypervisor used for KVM, and has over 1.4 million lines of code (QEMU version 4.2). Amazon Firecracker is a lightweight hypervisor and has only 50 thousand lines of code [Agache 20] . |
| §11.6 三种技术的比较 | 11-Chapter 11. Cloud Computing.md L2036 | Virtualization technologies are often compared using micro-benchmarking to see which performs the best. Unfortunately, this overlooks the importance of being able to observe the system, which can lead to the largest performance gains of all. Observability often makes it possible to identify and eliminate unnecessary work, making possible performance wins far greater than minor hypervisor differences. |

> **书里没说的**（本篇的推论与整理，原文没有，别当成原文观点）：
> - **「三种虚拟化 = 同一套四问（实现 / 开销 / 资源控制 / 可观测性）」这个平行结构是本篇的归纳**：
>   原文对硬件虚拟化（§11.2）、OS 虚拟化（§11.3）、轻量虚拟化（§11.4）是分别叙述的，
>   但每一类内部确实都按「实现 → 开销 → 资源控制 → 可观测性」四段展开，本篇沿用这个骨架以便对照。
> - **「I/O 路径步数越少越好」这条规律同时出现在 §11.2.2 与 §11.3.2 / §11.1.6**：这是本篇的连线，
>   原文没有把「hypervisor 的 I/O 路径层数」与「容器的 overlayfs / bridge / iptables 层数」并列成同一条原理。
> - **「软件限制比物理限制更早触发，所以应该先查」**是原文自己在 §11.3.4.4 说的（不是推演）；
>   但把它当作一条**通用排查次序**推广到本章三种技术，是本篇的推广。
> - 原文**没有**给出「硬件虚拟化 vs 容器」的量化开销对比数字（唯一带数字的是 2006 年那次 I/O 步数对比，
>   且原文自己注明该测试未启用半虚拟化、不代表现代配置）。本篇因此不给任何开销倍数。
> - 原文明说 Amazon 在 Nitro 上「实际怎么做 hypervisor 可观测性不是公开知识」，因此本篇对 Nitro 的
>   可观测性只写「SR-IOV 让设备 I/O 更难从 hypervisor 观察」这一层，不再推测。
> - **§11.3.4.4 的 CPU 节流判定流程（五个统计量）是作者本人的经验产物**，原文引用了它自己的出处；
>   本篇把它当成「判定顺序」来记，而不是当成内核保证的接口。

## §11.1 背景：云改变了什么，没改变什么

**机制**：云不是一种新技术，而是**把「容量」从一次性采购变成随时可调的运行时决策**。
原文一句话点明代价与收益：

> The rise of cloud computing solves some old problems in the field of performance while creating new ones. A cloud environment can be created instantly and scaled on demand, without the typical overheads of building and managing an on-premises data center. Clouds also allow better granularity for deployments—fractions of a server can be used by different customers as needed. However, this brings its own challenges: the performance overhead of virtualization technologies, and resource contention with neighboring tenants.

（这一段没有单独立锚点，它统领全章：新问题只有两个——**虚拟化开销**与**邻居争抢**。）

云的基本单位是**实例（instance）**，两种形态：**hardware instance**（IaaS，底层是硬件虚拟化，每实例一台 VM）
和 **OS instance**（轻量，通常 OS 虚拟化）。一个或多个实例作为物理宿主的 guest 运行，
guest 也叫 **tenant**，跑在同一宿主上就叫 **multitenancy**；宿主可以由云厂商运营（public cloud）、
公司自用（private cloud），或两者混合（hybrid cloud）。

### §11.1.1 实例类型：硬件选型变成了可调参数

云厂商按「family（字母）+ generation（数字）」组织类型并给出多个 size：
m5 通用、c5 计算、i3/d2 存储、r4/x1 内存、p1/g3/f1 加速计算；同一 family 内 size 从
m5.large（2 vCPU / 8 GB）到 m5.24xlarge（96 vCPU / 384 GB）。同一 family 内**价格/性能比相当一致**，
所以选 size 变成纯粹的工作负载匹配问题；GCP 还提供资源量自选的自定义机型。

**机制上的关键变化**是这一句：

> With so many options and the ease of redeploying instances, the instance type has become like a tunable parameter that can be modified as needed. This is a great improvement over the traditional enterprise model of selecting and ordering physical hardware that the company might be unable to change for years.

**判据**：把「实例类型」当成**运行时参数**而不是采购件——这是云性能工作的前提。
用同一个负载在一两个 size 之间实测（而不是照抄别人的推荐），通常比调任何云内参数收益都大。

### §11.1.2 水平扩展：为什么不是「买更大的机器」

垂直扩展有物理上限：机器尺寸受电梯门/集装箱限制，CPU 数量上去后 cache 一致性变难，功耗与散热也变难。
所以负载被横向摊到很多（往往很小）的系统上，这就是**水平扩展**（HPC 早于云就在用）。
云里的架构分层（负载均衡器 / web / 应用 / 数据库），每层由一个或多个并行实例组成，
也可以整组加——即**垂直切分**（Shopify 称之为 pods）。

**数据库是这套模式的硬骨头**：传统数据库必须有主库，所以要么逻辑切分成 **shards**，
要么用能动态并行执行的分布式架构（Riak），要么用为云设计的 **cloud-native 数据库**
（Cassandra、CockroachDB、Aurora、DynamoDB）。

**判据**：单实例 size 小（典型 8 GB，宿主 512 GB+ DRAM）是**细粒度扩容**的前提——
用接近满载的小实例换价格/性能比，而不是买一套常年闲置的大机器。

### §11.1.3 容量规划：被「按需扩容」替代

本地服务器的成本不只是硬件，还有**长达数年的服务合同**，而且从审批、备件到货、上架、安装、测试
上线**要好几个月**。云里实例几乎瞬间可建可销，于是容量规划的做法整个反过来：

> On-premises servers, and data centers, were the norm for enterprise environments. Cloud computing is very different. Server instances are inexpensive, and can be created and destroyed almost instantly. Instead of spending time planning what may be needed, companies can increase the number of server instances they use as needed , in reaction to real load.

自动扩缩容（AWS 的 auto scaling group，ASG）也让微服务架构成立——每个服务可独立扩缩。
但**扩缩容本身是新的故障源**：

> Auto scaling can solve the need to quickly respond to changes in load, but it also risks overprovisioning , as pictured in Figure 11.2 . For example, a DoS attack may appear as an increase in load, triggering an expensive increase in server instances.

同类风险还有**性能回归**：同样负载突然需要更多实例。
所以原文的结论是「**监控很重要，要确认这些扩容是合理的**」。

两个量级参考：Netflix 每天增删**数万个实例**去匹配它的每日并发曲线；
Pinterest 在 2012 年 12 月报告靠夜间自动关机把成本从 $54/小时降到 $20/小时；
Shopify 2018 年上云后平均空闲率从 61% 降到 19%。
**「立刻省下的钱」也可能来自性能调优**——需要的实例数变少了，成本自然下来。

还有一个容易被忽略的时间维度：**代码变更节奏**。网站往往每周、每天甚至一天多次更新生产代码，
而一次容量研究要几周、且基于的是**某一时刻的性能快照**，等做完已经过时；
这与企业跑商业软件、一年改几次的节奏完全不同。

云上的容量规划活动只剩三件：**动态调整实例数**、**短时购买大规模环境做可伸缩性测试**（本质是基准测试）、
以及在时间不允许时做**建模**估计实际可伸缩性与理论值的差距。

### §11.1.4 存储：本地盘 vs 网络存储

实例的 root volume 及其他卷（ephemeral drives）随实例销毁而消失。
持久化存储有三种形态：**file store**（如 NFS 上的文件）、**block store**（如 iSCSI 上的块）、
**object store**（通常是 HTTP API）。它们都跑在网络上，而**网络设施与存储设备都与别的租户共享**，
所以性能可预测性远不如本地盘；云厂商用资源控制来改善一致性。
缓解手段是**用内存缓存挡掉网络存储的延迟**；需要稳定性能时可直接**购买 IOPS 配额**
（如 EBS Provisioned IOPS volume）。

### §11.1.5 多租户：邻居（noisy neighbors）

Unix 本身就是多用户多任务系统，Linux 后来补上了资源限制与控制以更公平地共享资源。
云的区别是：**整个操作系统实例可以共存于同一物理系统**。每个 guest 是自己隔离的操作系统，
彼此**看不到对方的用户和进程**（那算信息泄露），**但共享同一批物理资源**。

> Since resources are shared among tenants, performance issues may be caused by noisy neighbors . For example, another guest on the same host might perform a full database dump during your peak load, interfering with your disk and network I/O.

解法是 **resource management（= resource isolation）**：对 CPU、内存、磁盘或文件系统 I/O、
网络吞吐施加 per-tenant 的**限值或优先级**。而**能观察多租户争抢的程度，取决于虚拟化类型**——
这句话是本章后面 §11.2.4 与 §11.3.4 的引子。

### §11.1.6 编排（Kubernetes）

Kubernetes 把容器部署成共置的 **Pod**：容器共享资源、可在 localhost 上互通、每个 Pod 有独立 IP；
**Service** 是对一组 Pod endpoint 的**持久稳定抽象**（Pod 可随时增删、可当一次性资源对待），
这支撑了微服务架构；**Horizontal Pod Autoscaler** 按目标利用率或其他指标扩缩副本；
物理机叫 **Node**，连到同一个 API server 的 Node 组成一个 cluster。

**两个性能战场**（原文明确列出）：**调度**（容器放哪台机）与**网络**（为了实现容器网络与负载均衡，
额外引入了组件）。调度会考虑 CPU/内存的 requests 与 limits、以及 node taints 与 label selectors；
**当时 Kubernetes 不限制 block I/O**（blkio cgroup 支持还在未来），所以**磁盘争抢是可能的性能问题源**。
网络侧由 CNI 插件实现（Calico 基于 netfilter/iptables，Cilium 基于 BPF），
Cilium 还提供用 BPF 替代 kube-proxy 的方案。

**判据**：上 K8s 之后性能退化，**先怀疑调度放置与 CNI/kube-proxy 这两段路径**，
而不是先怀疑应用本身。

## §11.2 硬件虚拟化：每个 guest 一套内核

hypervisor（VMM）创建并管理虚拟机，guest 看起来像一台完整的电脑，可以装**整个操作系统和自己的内核**。
经典分类是 Type 1（直接跑在处理器上，如 Xen）与 Type 2（跑在宿主 OS 里，guest 表现为宿主上的进程，如 KVM）；
但作者认为**这个分类已不再严格成立**（Type 2 用内核模块拿到了部分硬件直通能力），
于是改用两个更实用的配置：

| 配置 | 形态 | hypervisor 跑在哪 | 谁来调度 guest vCPU | 代表 |
|---|---|---|---|---|
| Config A | native / bare-metal hypervisor | 直接在处理器上 | hypervisor 自带的调度器 | Xen |
| Config B | 宿主 OS 内 | 宿主 OS 内核（内核模块 + 用户态进程） | 宿主内核调度器 | KVM |

### §11.2 I/O 代理是硬件虚拟化的主要开销源

两种配置**都可能**在 dom0（Xen）或宿主 OS（KVM）里跑一个 **I/O proxy**（通常是 QEMU）来服务 guest 的 I/O：

> Both configurations may involve running an I/O proxy (e.g., using the QEMU software) in domain 0 (Xen) or the host OS (KVM), for serving guest I/O. This adds overhead to I/O, and over the years has been optimized by adding shared memory transports and other techniques.

历史上的演进路线（本节最重要的机制链）：

1. **二进制翻译**（VMware 1998）：把特权指令（系统调用、页表操作）改写后再执行；非特权指令直接跑。
   开销高，但被服务器整合带来的节省所抵消。
2. **处理器虚拟化扩展**：AMD-V 与 Intel VT-x（2005–2006）让虚拟化特权指令与 MMU 快得多。
3. **半虚拟化（paravirt / PV）**：给 guest OS 一个高效使用宿主资源的接口（**hypercalls**），
   而不必把所有部件都完整虚拟化。例如「装定时器」通常需要多条特权指令逐条被模拟，
   PV 可以简化成**一条 hypercall**；Xen 还能把这些 hypercall 批量成 **multicall**。
   PV 也可以包含**半虚拟网络设备驱动**，让包更高效地交给宿主物理网卡。
   代价：**依赖 guest OS 支持**（Windows 历史上不提供）。
4. **设备硬件支持**：**SR-IOV** 让 guest 直接访问网络与存储设备，需要驱动支持
   （ixgbe、ena、hv_netvsc、nvme）。

现代 Xen 常用 **PVHVM**：以 HVM 模式启动（可装未修改的 OS），再用带 HVM 支持的 PV 驱动提速。

### §11.2.1 实现：五种 hypervisor 的定位

| 实现 | 时间/背景 | 形态 | 记忆点 |
|---|---|---|---|
| VMware ESX | 2001 | 裸金属微内核，首台虚拟机叫 **service console** | 服务器整合的起点 |
| Xen | 2003（剑桥研究项目，后被 Citrix 收购） | Type 1；虚拟机叫 **domains**，最特权的是 **dom0** | EC2 早期基于 Xen |
| Hyper-V | 随 Windows Server 2008 发布 | Type 1；创建 **partitions** | Azure 可能跑定制版 |
| KVM | Qumranet 开发，2008 被 Red Hat 收购 | Type 2，**内核模块**；与用户态 **QEMU** 配对才构成完整 VM | GCE 在用 |
| Nitro | AWS 2017 | 基于 KVM 部件，但**处理器、网络、存储、中断、定时器全部硬件支持** | **不用 QEMU proxy** |

Nitro 那一行值得单独抄下来：

> Nitro : Launched by AWS in 2017, this hypervisor uses parts based on KVM with hardware support for all main resources: processors, network, storage, interrupts, and timers [Gregg 17e] . No QEMU proxy is used. Nitro provides near bare-metal performance to guest VMs.

**判据**：判断一套云实例「为什么比裸机快/慢」，**先看它的 hypervisor 有没有 I/O proxy**。
有 proxy（Xen dom0、QEMU）就意味着 I/O 路径多两层；没有（Nitro）就意味着少两层。

### §11.2.2 开销：五类开销各自来自哪里

原文把硬件虚拟化的开销拆成五块：CPU 执行、内存映射、内存大小、I/O、多租户争抢。

**CPU**：guest 应用通常**直接跑在处理器上**，CPU-bound 应用性能可以与裸机几乎相同。
开销出现在**特权处理器调用、访问硬件、映射内存**时。三种指令处理方式：

| 方式 | 做法 | 代价 |
|---|---|---|
| 二进制翻译 | VMM 占 ring 0，guest kernel 移到 ring 1；guest 内核假设自己在 ring 0，指令须被翻译并回调 VMM | **运行时翻译，CPU 开销显著** |
| 半虚拟化 | 必须虚拟化的指令换成 hypercall | guest OS 改一下就能更省 |
| 硬件辅助 | guest 的特权指令**陷阱**到 ring 更低的 VMM，由 VMM 模拟特权 | 不改指令，但每次都要 trap |

结论口径：

> Hardware-assisted virtualization is generally preferred, depending on the implementation and workload, while paravirtualization is used to improve the performance of some workloads (especially I/O) if the guest OS supports it.

（VMware 2007 年自己的说法也很坦率：他们的二进制翻译在多数情况下**优于第一代硬件辅助实现**，
因为第一代硬件辅助的编程模型僵化，没有多少余地调整 hypervisor↔guest 转换的频率与成本。）

**CPU 开销怎么量**：用 **guest exit**。

> The rate of transitions between the guest and hypervisor, as well as the time spent in the hypervisor, can be studied as a metric of CPU overhead. These events are commonly referred to as guest exits , as the virtual CPU must stop executing inside the guest when this happens.

**开销量级取决于 exit 在哪里被处理**：一部分 exit 可以在内核里直接处理掉；
处理不了的必须**离开内核、回到用户态**——那会引入更大的开销。

KVM 的 exit 处理函数数组（原文照抄，Linux 5.2，arch/x86/kvm/vmx/vmx.c，已截断）：

```cpp
/*
 * The exit handlers return 1 if the exit was handled fully and guest execution
 * may resume.  Otherwise they set the kvm_run parameter to indicate what needs
 * to be done to userspace and return 0.
 */
static int (*kvm_vmx_exit_handlers[])(struct kvm_vcpu *vcpu) = {
        [EXIT_REASON_EXCEPTION_NMI]           = handle_exception,
        [EXIT_REASON_EXTERNAL_INTERRUPT]      = handle_external_interrupt,
        [EXIT_REASON_TRIPLE_FAULT]            = handle_triple_fault,
        [EXIT_REASON_NMI_WINDOW]              = handle_nmi_window,
        [EXIT_REASON_IO_INSTRUCTION]          = handle_io,
        [EXIT_REASON_CR_ACCESS]               = handle_cr,
        [EXIT_REASON_DR_ACCESS]               = handle_dr,
        [EXIT_REASON_CPUID]                   = handle_cpuid,
        [EXIT_REASON_MSR_READ]                = handle_rdmsr,
        [EXIT_REASON_MSR_WRITE]               = handle_wrmsr,
        [EXIT_REASON_PENDING_INTERRUPT]       = handle_interrupt_window,
        [EXIT_REASON_HLT]                     = handle_halt,
        [EXIT_REASON_INVD]                    = handle_invd,
        [EXIT_REASON_INVLPG]                  = handle_invlpg,
        [EXIT_REASON_RDPMC]                   = handle_rdpmc,
        [EXIT_REASON_VMCALL]                  = handle_vmcall,
   [...]
        [EXIT_REASON_XSAVES]                  = handle_xsaves,
        [EXIT_REASON_XRSTORS]                 = handle_xrstors,
        [EXIT_REASON_PML_FULL]                = handle_pml_full,
        [EXIT_REASON_INVPCID]                 = handle_invpcid,
        [EXIT_REASON_VMFUNC]                  = handle_vmx_instruction,
        [EXIT_REASON_PREEMPTION_TIMER]        = handle_preemption_timer,
        [EXIT_REASON_ENCLS]                   = handle_encls,
};
```

这些名字虽短，却直接列出了**guest 会因为什么原因回调 hypervisor**。
其中最常见的一类是 **HLT（halt）**：内核找不到活干时让处理器进入低功耗模式直到被打断。
KVM 的 `kvm_vcpu_halt()`（arch/x86/kvm/x86.c）示范了两件事：**先累加统计量**（`vcpu->stat.halt_exits`），
再决定是内核内直接模拟（返回 1）还是退回用户态（返回 0）：

```cpp
int kvm_vcpu_halt(struct kvm_vcpu *vcpu)
{
        ++vcpu->stat.halt_exits;
        if (lapic_in_kernel(vcpu)) {
                vcpu->arch.mp_state = KVM_MP_STATE_HALTED;
                return 1;
        } else {
                vcpu->run->exit_reason = KVM_EXIT_HLT;
                return 0;
        }
}
```

函数体写得极短，就是为了压低每次 exit 的 CPU 开销。这类函数可以在宿主上用 **kprobes** 插桩，
统计 exit 的类型与时长；也可以用 **kvm:kvm_exit** 这个 tracepoint 统计。

**内存映射**：guest 的缺页要经过**两级翻译**——guest 内核做「guest 虚拟 → guest 物理」，
hypervisor 的 VMM 做「guest 物理 → host 物理（真实）」。合成的映射可以缓存进 TLB，
后续访问就是正常速度。现代处理器支持 MMU 虚拟化：

> The mapping, from guest virtual to host physical, can then be cached in the TLB, so that subsequent accesses can operate at normal speed—not requiring additional translation. Modern processors support MMU virtualization, so that mappings that have left the TLB can be recalled more quickly in hardware alone (page walk), without calling in to the hypervisor. The feature that supports this is called extended page tables (EPT) on Intel and nested page tables (NPT) on AMD [Milewski 11] .

没有 EPT/NPT 时，另一条路是**影子页表**：guest 照常维护自己的页表，hypervisor 截获页表变更并同步一份到 host 物理，
guest 执行时由 hypervisor **改写 CR3 寄存器**指向影子页表。

**内存大小**：比 OS 虚拟化多三笔——**每个 guest 自己的内核**占一点；
**存储架构可能导致双重缓存**（guest 与 host 缓存同一份数据）；**KVM 式 hypervisor 每台 VM 还跑一个 VMM 进程**（如 QEMU），它自己也要吃内存。

**I/O**：历史上这是硬件虚拟化**最大的开销来源**，因为每次设备 I/O 都要被 hypervisor 翻译；
高频 I/O（如 10 Gbit/s 网络）下，每次 I/O 多一点点开销，乘起来就很可观。
缓解手段：I/O MMU 虚拟化（AMD-Vi、Intel VT-d）、**半虚拟驱动**（合并 I/O、减少设备中断从而减少 hypervisor 开销）、
**PCI pass-through**（把设备直接指派给 guest，性能最好，但设备不再共享、降低多租户灵活性，还会复杂化 live migration）、
以及 **SR-IOV / MR-IOV**（暴露不同数量的 root complex PCI 拓扑）。
Xen 用 **device channel**——dom0 与 domU 之间的**异步共享内存传输**，避免在域之间传递 I/O 数据时多做一次拷贝。

原文给出的判据句是这一节的核心：

> The number of steps in the I/O path, both control and data, is critical for performance: the fewer, the better.

（2006 年 KVM 开发者对比 Xen 与 KVM：KVM 用**一半的步数**完成 I/O——五步对十步；
但原文注明该测试**未启用半虚拟化**，不代表现代配置。）

**多租户争抢**：取决于 hypervisor 配置与 CPU/cache 在租户间的共享程度，
可能出现 **CPU stolen time** 与**别的租户造成的 CPU cache 污染**；
原文指出**这通常在容器上比在 VM 上更严重**（容器为了支持 bursting 本来就鼓励共享）。
别的租户做 I/O 引发的中断也会打断当前执行。

### §11.2.3 资源控制：vCPU 是粗粒度的，细粒度要靠别的机制

CPU 与内存通常在 guest 配置阶段就被设限；hypervisor 还可能为网络与磁盘 I/O 提供控制。

**vCPU 数量是粗粒度上限**。细粒度 CPU 配额有两条路：

- **Xen 侧**：hypervisor 自己的 CPU 调度器——**BVT**（借用虚拟时间的公平共享调度器，
  可预借虚拟时间以给实时/交互应用低延迟）、**SEDF**（实时调度器，可配置运行时保证，按最早截止时间优先）、
  **credit-based**（支持权重与上限，并跨多 CPU 做负载均衡）。
- **KVM 侧**：由**宿主 OS** 提供，例如宿主内核的公平共享调度器，Linux 上就是 **cgroup 的 CPU 带宽控制**。

**但两条路都有一个硬限制**：

> There are limitations on how either technology can respect guest priorities . A guest’s CPU usage is typically opaque to the hypervisor, and guest kernel thread priorities cannot typically be seen or respected.

即：**hypervisor 看不见 guest 内部线程的优先级**。
一个 guest 里的低优先级日志轮转 daemon，与另一个 guest 里的关键应用服务器，在 hypervisor 眼里优先级相同。
**推论**：租户内部的应用级优先级只能靠 guest 自己实现，hypervisor 层的配额管不了。

**Xen 还有一层账要单独算**：高 I/O 负载会在 dom0 消耗额外 CPU，
而 **guest domain 里的后端驱动与 I/O proxy 消耗的 CPU 可能超出其配额却不计入账**。
解法是 **isolated driver domains（IDD）**：把 I/O 服务分离到独立域，做安全、性能隔离与**计费**（SEDF-DC 调度器按 IDD 反馈回收 CPU 配额）；
更新的做法是 **stub domains**（跑一个 mini-OS）。

**CPU 缓存**：用 **Intel CAT** 在 guest 之间划分 LLC（分区可共享）。能防止一个 guest 污染另一个 guest 的缓存，
但**限制缓存使用本身也会伤性能**。

**内存容量**：限额在 guest 配置阶段确定，**guest 内核照常执行自己的分页/换出**来待在限额内。
为了比静态配置更灵活，VMware 发明了 **balloon driver**：在 guest 里「充气」一个占内存的模块，
被占掉的内存由 hypervisor 回收给别的 guest；也可以放气还给 guest 内核。
VMware、Xen、KVM 都支持。**检查方法**：在 guest 里 `dmesg` 搜 balloon。
作者提醒：**用了 balloon 就要留意它带来的性能问题**。

**文件系统容量**：guest 拿到的是宿主提供的虚拟磁盘卷；KVM 式 hypervisor 里这些卷通常由 OS 创建并定容
（例如 ZFS 文件系统可以创建指定大小的卷）。

**设备 I/O**：hypervisor 的资源控制历来聚焦 CPU（间接控制 I/O）。
网络吞吐可以由**外部专用设备**限速，或在 KVM 式 hypervisor 里由**宿主内核特性**限速
（Linux 的 cgroup 网络带宽控制与各类 qdisc 都可以作用到 guest 网卡）。
Xen 的网络性能隔离被指出是弱项（原文引用的研究结论：Xen 缺乏 proper performance isolation），
该研究也提出了给网络 I/O 调度加优先级与速率可调参数的建议。
像 Nitro 这样**全硬件支持**的 hypervisor，I/O 限制由硬件或外部设备承担——
在 EC2 上，网络 I/O 与到网络设备的磁盘 I/O 由外部系统限流到配额。

### §11.2.4 可观测性：谁能看见什么

原文把「可见性」拆成**位置**（宿主/特权 guest 侧 vs guest 侧）而不是工具，规则如下：

| 位置 | 能看到 | 看不到 |
|---|---|---|
| 特权 guest / 宿主（Xen dom0、KVM 宿主） | 全部物理资源可用前面各章的标准 OS 工具观察；guest I/O 可通过分析 I/O proxy 观察；hypervisor 应提供 per-guest 资源统计 | **guest 内部（进程等）无法直接观察**；pass-through / SR-IOV 的 I/O 可能观察不到 |
| 硬件支持的宿主（Nitro） | 同上 | SR-IOV 让设备 I/O 更难从 hypervisor 观察（guest 直连硬件，不走 proxy 也不走宿主内核） |
| guest | 虚拟资源及自己的使用情况；可反推物理问题；**有自己的内核 ⇒ 内核内部可分析，BPF 类工具全都能用** | 物理资源本身通常看不到 |

**能聚合到什么粒度**：物理资源使用通常可在高层观察——利用率、饱和、错误、IOPS、吞吐、I/O 类型，
而且**通常可以按 guest 表达**，便于快速找出大户。
但**「guest 里哪个进程在做 I/O、它的应用调用栈是什么」无法直接观察**，
只能登录进 guest（需授权，如 SSH）用 guest 自己的工具看。

**pass-through / SR-IOV 的可观测性盲区**是最需要记住的一条：

> When pass-through or SR-IOV is used, the guest may be making I/O calls directly to hardware. This may bypass I/O paths in the hypervisor, and the statistics they typically collect. The result is that I/O can become invisible to the hypervisor, and not appear in iostat(1) or other tools.

变通办法是用 **PMC** 读 I/O 相关计数器再反推。

**根因分析要两侧都登**。原文明确：为了定位 guest 的性能问题，
云运维者**可能需要同时登录 hypervisor 与 guest 两侧**执行观测工具；
I/O 路径本来就步骤多，追踪起来复杂，还可能涉及 hypervisor 内部与 I/O proxy 的分析。

**guest 侧看不到物理资源**的副作用：客户容易把「说不清的性能问题」归咎于 noisy neighbors。
缓解手段是云厂商通过 **SNMP 或云 API** 提供（脱敏的）物理资源信息，
既让客户安心，也减少支持工单。另有现成的容器监控方案：**cAdvisor**、**Cilium Hubble**。

#### §11.2.4.1 特权 guest / 宿主侧的工具

**Xen**：guest 的 vCPU 存在于 hypervisor 中，**在 dom0 里用标准 OS 工具看不到**，要用 `xentop(1)`：

```
# xentop
xentop - 02:01:05   Xen 3.3.2-rc1-xvm
2 domains: 1 running, 1 blocked, 0 paused, 0 crashed, 0 dying, 0 shutdown
Mem: 50321636k total, 12498976k used, 37822660k free    CPUs: 16 @ 2394MHz
      NAME  STATE   CPU(sec) CPU(%)     MEM(k) MEM(%)  MAXMEM(k) MAXMEM(%) VCPUS NETS
NETTX(k) NETRX(k) VBDS   VBD_OO   VBD_RD   VBD_WR SSID
  Domain-0 -----r    6087972    2.6    9692160   19.3   no limit       n/a    16    0
0        0    0        0        0        0    0
Doogle_Win --b---     172137    2.0    2105212    4.2    2105344       4.2     1    2
0        0    2        0        0        0    0
[...]
```

字段含义（原文逐项列出）：`CPU(%)`（多 CPU 求和的使用率）、`MEM(k)`/`MEM(%)`（主存用量与占比）、
`MAXMEM(k)`/`MAXMEM(%)`（主存上限与占比）、`VCPUS`（分配的 vCPU 数）、`NETS`（虚拟网卡数）、
`NETTX(k)`/`NETRX(k)`（网络收发）、`VBDS`（虚拟块设备数）、
`VBD_OO`（**被阻塞排队的虚拟块设备请求 = 饱和度**）、`VBD_RD`/`VBD_WR`（读/写请求数）。
默认每 3 秒刷新，可用 `-d delay_secs` 调整。进阶用 `xentrace(8)` 取 hypervisor 事件日志，
配 `xenanalyze` 分析调度问题；`xenoprof` 是 Xen 的系统级 profiler。

**KVM**：guest 实例在宿主 OS 里就是进程（`top` 里那个 `qemu-system-x86`）。
per-vCPU 的用量用 `pidstat -tp` 看（`CPU 0/KVM`、`CPU 1/KVM` 这样的线程）；
把 QEMU 进程对应回 guest 实例名，通常看进程参数里的 `-name`（`ps -wwfp PID`）。

**guest vCPU exit 是 KVM 分析的重心**。`perf kvm stat live` 给出 exit 类型分布：

```
host# perf kvm stat live
11:12:07.687968

Analyze events for all VMs, all VCPUs:

           VM-EXIT Samples Samples%   Time%  Min Time    Max Time       Avg time

         MSR_WRITE    1668   68.90%   0.28%    0.67us     31.74us    3.25us ( +-  2.20% )
               HLT     466   19.25%  99.63%    2.61us 100512.98us 4160.68us ( +- 14.77% )
  PREEMPTION_TIMER     112    4.63%   0.03%    2.53us     10.42us    4.71us ( +-  2.68% )
 PENDING_INTERRUPT      82    3.39%   0.01%    0.92us     18.95us    3.44us ( +-  6.23% )
EXTERNAL_INTERRUPT      53    2.19%   0.01%    0.82us      7.46us    3.22us ( +-  6.57% )
    IO_INSTRUCTION      37    1.53%   0.04%    5.36us     84.88us    19.97us ( +- 11.87% )
          MSR_READ       2    0.08%   0.00%    3.33us      4.80us    4.07us ( +- 18.05% )
     EPT_MISCONFIG       1    0.04%   0.00%    19.94us    19.94us    19.94us ( +-  0.00% )

Total Samples:2421, Total events handled time:1946040.48us.
[...]
```

列的含义：`VM-EXIT`（exit 类型）、`Samples`（采样到的次数）、`Samples%`（占比）、
`Time%`（在 exit 里花掉的时间占比）、`Min/Max/Avg time`。

**这张表怎么读**（原文给的判据）：exit 数量少且其中 `HLT` 占比高 ⇒ guest CPU 相当空闲；
反之，若 I/O 操作很多、且中断既被生成又被注入 guest ⇒ guest 极可能正在**通过虚拟网卡和虚拟磁盘做 I/O**。

进阶：`perf list | grep kvm` 有一长串 tracepoint；用 bpftrace 取 `kvm:kvm_exit` 的参数
（`exit_reason`、`guest_rip` 等），配合 `kvm:kvm_entry` 就能量出**每次 exit 的时长**；
作者发布的 `kvmexits.bt` 直接给出 exit 原因的直方图。
还有一个技巧：**采样 guest 的 CR3 寄存器**——CR3 是页表根，
从宿主采样 CR3 可以判断 guest 里是**单个进程在跑**（CR3 值相同）还是在**进程间切换**（CR3 值不同）。
最后，**要看到 guest 内部细节，就必须登录进 guest**。

#### §11.2.4.2 guest 侧

guest 里只看得到**虚拟设备**（除非用了 pass-through/SR-IOV）。CPU 就是分配给 guest 的 vCPU
（`mpstat -P ALL` 里就那么几行）。

**stolen time** 是少见的「虚拟化感知」统计量：`vmstat` 的 `st` 列表示
**不属于 guest 的 CPU 时间**——可能被别的租户吃掉，也可能是 hypervisor 自身功能
（处理 I/O、或因实例类型而限流）造成的。

**PMC 的可用性取决于 hypervisor 配置**：Xen 提供虚拟性能监控单元（vpmu）支持 guest 使用 PMC，
并可调优允许哪些 PMC。

**磁盘与网络都是虚拟设备**，所以要重点分析的是**延迟**——设备在虚拟化、限额与邻居干扰下的响应；
而 `%busy` 这类指标**在不知道底层设备是什么时难以解释**。

内核 tracing 工具（perf、Ftrace、BPF）在 guest 里**都能用**（有独立内核、root 有完整内核访问权），
例如在 KVM guest 里跑 BPF 版 `biosnoop` 就能看到虚拟磁盘的逐次延迟。
**注意对比：容器里这些内核 tracing 工具往往不能用**（见 §11.3.4）。

#### §11.2.4.3 分析策略

**管理员侧**能做的：按前面各章的方法分析物理系统资源找瓶颈与错误；
再检查**施加在 guest 上的资源控制**——如果 guest 长期顶在限额上，
就该通知客户并鼓励升级规格。**但不做更多了**，认真的性能调查必须登录进 guest。

**guest 侧**也能用前面各章的方法，但要记住**资源是虚拟的**：
有些资源没跑满可能是因为**看不见的 hypervisor 资源控制**，也可能是因为**别的租户在争抢**。
理想情况是云软件/厂商提供**脱敏后的物理资源用量**让客户自查；
否则只能**从 I/O 与 CPU 调度延迟的异常上升去反推**争抢与限额（延迟可在系统调用层测，也可在 guest 内核里测）。

**作者自己的做法**（这一段是可复用的方法论）：从 guest 侧识别磁盘与网络争抢，靠**仔细分析 I/O 模式**——
把 `biosnoop` 的输出记录下来，**看 I/O 的先后序列**，
判断延迟离群点是否由 **I/O 大小**（大 I/O 更慢）、**访问模式**（读请求排在写 flush 后面）造成；
**如果两者都不是**，那就很可能是**物理争抢或设备问题**。

## §11.3 OS 虚拟化：容器

容器把操作系统切成一个个实例，**可以像独立服务器一样单独管理与重启**，给客户小、快启动的实例，
给运维方高密度。

**血统**：`chroot(8)`（把进程隔离到文件系统的子树）→ **FreeBSD jails**（1998，安全隔舱）→
**Solaris Zones**（Solaris 10，2005，带多种资源控制）→
Linux：**namespaces**（2002，2.4.19 引入）与 **cgroups**（2008，2.6.24 引入），
两者组合成容器，通常还用 **seccomp-bpf** 限制系统调用。

**最关键的一句机制差异**：

> A key difference from hardware virtualization technologies is that only one kernel is running.

### §11.3.1 实现：内核里没有「容器」这个概念

Linux 内核**没有容器的概念**，只有 namespaces 与 cgroups，是**用户态软件**（如 Docker）把它们组装成「容器」。
内核里最接近容器的抽象是 `struct nsproxy`（把进程链接到它所在的各个 namespace）。

**Namespaces** 过滤系统视图，让容器只能看见并管理自己的进程、挂载点与其他资源——
这是容器间隔离的主要机制。原文列出：cgroup、ipc、mnt、net、pid、time、user、uts。
用 `lsns(8)` 可以列出当前系统的 namespace（示例里 init 进程同时挂着 6 种 namespace，被 100+ 个进程使用）。

**Control groups** 限制资源用量。v1 包含原文列出的：blkio（块 I/O 的字节与 IOPS 限额）、
cpu（按 shares 限 CPU）、cpuacct（进程组 CPU 记账）、cpuset（分配 CPU 与内存节点）、
devices（设备访问控制）、hugetlb（huge page 限额）、memory（进程内存、内核内存与 swap 限额）、
net_cls（给包打 classid 给 qdisc 与防火墙用）、net_prio（网卡优先级）、
perf_event（允许 perf 监控 cgroup 内进程）、pids（限制可创建的进程数）、rdma（RDMA/InfiniBand 限额）。

cgroups **v2** 是层级化的，修掉 v1 的若干问题；Kubernetes 等项目当时仍在用 v1，v2 在推进中，
**Fedora 31（2019）已经切到 v2**。

### §11.3.2 开销：用户态没有开销，代价在争抢

**CPU**：容器线程在用户态运行时**没有直接的 CPU 开销**——线程直接跑在 CPU 上，直到让出或被抢占。
Linux 上跑在 namespace 与 cgroup 里也**没有额外 CPU 开销**，因为
**所有进程本来就在一套默认 namespace 与 cgroup 里**，用不用容器都一样。
所以 **CPU 性能下降最可能来自与其他租户的争抢**。

**编排器的额外开销**：Kubernetes 之类会引入额外网络组件处理数据包；
例如服务数量很多时，**kube-proxy 面对庞大的 iptables 规则集会有首包开销**——
可以用 **BPF 替换 kube-proxy** 来消除。

**内存映射 / 加载 / 存储**：无开销。**内存大小**：容器可用**全部**分配到的内存（VM 则是每租户一个内核，各占一点）；
**OverlayFS** 还能让访问同一文件的容器**共享 page cache**，省下 VM 重复存放公共文件（如系统库）的内存。

**I/O**：开销取决于配置，可能多了隔离层——**文件系统层**（overlayfs）与**网络层**（bridge 联网）。
overlayfs 在内核栈里表现为 `ovl_write_iter()`：

```
    blk_mq_make_request+1
    generic_make_request+420
    submit_bio+108
    _xfs_buf_ioapply+798
    __xfs_buf_submit+226
    xlog_bdstrat+48
    xlog_sync+703
    __xfs_log_force_lsn+469
    xfs_log_force_lsn+143
    xfs_file_fsync+244
    xfs_file_buffered_aio_write+629
    do_iter_readv_writev+316
    do_iter_write+128
    ovl_write_iter+376
    __vfs_write+274
    vfs_write+173
    ksys_write+90
    do_syscall_64+85
    entry_SYSCALL_64_after_hwframe+68
```

**代价大小取决于负载的 IOPS 率**——原文给的量化口径是：**IOPS 低于约 1000 的服务器，开销可以忽略**。

**多租户争抢**（容器最核心的性能问题，原文逐条列）：

| 争抢面 | 机制 |
|---|---|
| CPU cache | 命中率下降（别的租户在消费并驱逐条目）；某些处理器与内核配置下，切到别的容器线程**甚至会 flush L1** |
| TLB | 命中率下降，另外上下文切换时会 flush（**用 PCID 可以避免**） |
| 中断 | 其他租户设备（如网络 I/O）的 ISR 会打断当前执行 |
| 内核资源 | **buffers、caches、queues、locks** 的争抢，多租户容器系统能把它们的负载**抬高一个数量级甚至更多** |
| 网络 | iptables 实现的容器网络带来 CPU 开销 |
| 整机资源 | CPU、磁盘、网卡的争抢 |

其中**内核资源争抢**是原文特别强调的一条：

> Kernel execution can encounter additional contention for buffers, caches, queues, and locks, because a multi-tenant container system can increase their load by an order of magnitude or more.

两个真实案例（原文给出，值得记）：
**Gianluca Borello** 发现某个容器的性能会随时间缓慢劣化，定位到是
**另一个容器的工作负载影响了 dcache，导致 `lstat(2)` 延迟升高**；
**Maxim Leonovich** 报告从单租户 VM 迁到多租户容器后，**内核的 `posix_fadvise()` 调用率上升**并形成瓶颈。

### §11.3.3 资源控制：priorities 与 limits

原文把资源控制分成两类：**priorities**（按重要性在邻居之间分配资源）与 **limits**（消耗上限），
有些资源两者都用。按资源对照：

| 资源 | Priority | Limit |
|---|---|---|
| CPU | CFS shares | cpusets（整 CPU）、CFS bandwidth（分数 CPU） |
| 内存容量 | 内存软限额 | 内存限额 |
| Swap 容量 | — | Swap 限额 |
| 文件系统容量 | — | 文件系统配额/限额 |
| 文件系统缓存 | — | 内核内存限额 |
| 磁盘 I/O | blkio weights | blkio IOPS 限额、blkio 吞吐限额 |
| 网络 I/O | net_prio 优先级、qdiscs（fq 等）、自定义 BPF | 同左 |

**cpusets**：把**整个 CPU** 分配给特定容器。好处是这些容器**跑起来不会被别人打断**，
且可用 CPU 容量稳定；代价是**空闲的 CPU 容量无法被其他容器使用**。

**CPU shares（CFS）**：另一种分配方式，**允许容器共享空闲 CPU**，并支持 **bursting**。
shares 是分配单位，用于计算繁忙容器在某时刻能拿到多少 CPU：

> container CPU = (all CPUs × container shares) / total busy shares on system

原文给的算例：系统共分配 100 shares；某一时刻只有 A（10 shares）与 B（30 shares）要 CPU，
则 A 可用 **25%** 的总 CPU（全部 CPU × 10/(10+30)）；
若所有容器都忙，则 A 只能拿 **10%**（全部 CPU × 10/100）。
**结论：shares 给的是「最低保证」，bursting 可以让它更高——A 实际可用 10% 到 100% 之间的任何值。**

**shares 的陷阱是本节最该记住的一条**：

> A problem with shares is that bursting can confuse capacity planning, especially since many monitoring systems do not show bursting statistics (they should).

原文描述的场景值得完整记住：用户在一台空闲机器上测试容器 A，拿到 100% CPU，满意；
后来别的容器搬进来，A 不能再 burst，只剩 10%——**性能掉了十倍，而系统「工作完全正常」**。
作者说这种情况现实中见过很多次，用户以为系统有性能问题请他 debug，结果被告知一切按设计工作，
「对客户来说这感觉像是 bait and switch」。

**两个对策**：
① 用 **CFS bandwidth control** 给 burst 设上限（例如 A 的带宽设为全系统 CPU 的 20%），
于是 A 的实际区间变成 **10%–20%**——**性能上限被削平了，但性能下降也没那么剧烈**；
② **运维方主动通知**用户「你正在 burst，已经持续几天」，避免用户形成错误的性能预期，
并引导用户升级容器规格以获得更多 shares 与更高的最低保证。

**CPU 缓存**：同样是 **Intel CAT**（告警同 §11.2.3：限制缓存访问本身也伤性能）。

**内存 cgroup** 提供四种机制：`memory.limit_in_bytes`（硬限额，超了就换页或被 OOM killer 干掉）、
`memory.soft_limit_in_bytes`（尽力而为，回收内存把容器推向软限）、
`memory.kmem.limit_in_bytes`（内核内存限额）、`memory.kmem.tcp.limit_in_bytes`（TCP buffer 内存限额）。
另有 `memory.pressure_level` 这个低内存通知器（经 `eventfd(2)` 使用，需要应用配合）；
`memory.pressure_level` 与 `memory.oom_control` 都是通知机制，同样需要配 eventfd。
**注意**：容器未使用的内存**可以被别的容器用在内核 page cache 里**（内存版的 bursting）。

**Swap**：`memory.memsw.limit_in_bytes`（内存 + swap 合计）。

**文件系统容量**：通常交给文件系统——XFS 对用户/组/项目都支持软硬配额（软限允许在硬限下临时超额），
ZFS 与 btrfs 也有配额。

**文件系统缓存**：容器用掉的 page cache **记在容器的 memory cgroup 里**，不需要额外设置；
若配了 swap，`memory.swappiness` 控制「偏换页」还是「偏驱逐 page cache」的取舍（同系统级 vm.swappiness）。

**磁盘 I/O**：`blkio.weight`（类似 CPU shares 的磁盘份额权重，配 BFQ 调度器）、`blkio.weight_device`、
以及四个限速项 `blkio.throttle.read_bps_device` / `write_bps_device` / `read_iops_device` / `write_iops_device`。

**网络 I/O**：`net_prio` 给出口流量设优先级（等同 `SO_PRIORITY` socket 选项，控制网络栈里的包处理优先级）；
`net_cls` 给包打 class id 供 qdisc 管理（Kubernetes 的 Pod 也能用 per-Pod 的 net_cls）；
**qdisc**（50 多种，各有策略与可调项）可按 class id 或直接绑到容器虚拟网卡上做优先级与限速——
Kubernetes 的 `kubernetes.io/ingress-bandwidth` / `egress-bandwidth` 就是用**令牌桶过滤器（tbf）qdisc** 实现的。
此外 **BPF 程序可以挂到 cgroup 上**，做自定义的资源控制与防火墙——
Cilium 就是组合了 XDP、cgroup、tc(qdisc) 多层 BPF 来实现安全、负载均衡与防火墙。

### §11.3.4 可观测性：容器里看到的是谁的数字

| 位置 | 能看到 | 看不到 |
|---|---|---|
| 宿主（最高权限 namespace） | **一切**：硬件资源、文件系统、guest 进程、guest TCP 会话；不用登录 guest 就能分析 guest 进程；能直接浏览 guest 文件系统 | — |
| guest（容器） | 自己的进程、文件系统、网卡、TCP 会话 | **内核内部**（⇒ 13–15 章的内核 tracing 工具通常不能用） |

**但有一个重大例外**：

> From the guests : The container can typically only see its own processes, file systems, network interfaces, and TCP sessions. A major exception is system-wide statistics, such as for CPUs and disks: these often show the host rather than just the container.

即：**容器里跑 `iostat`/`mpstat`/`vmstat`/`free`/`uptime`，看到的往往是宿主的数字**，
而这个行为**通常没有文档**（原文说自己为它写了一份文档，见下文 Table 11.6）。

**传统工具在容器里的实际表现**（原文 Table 11.6 的要点）：

| 工具 | 从宿主 | 从容器 |
|---|---|---|
| top / ps / pidstat | 显示全部进程 | 只显示容器进程 |
| uptime / mpstat / vmstat / free / iostat | 宿主数据 | **宿主数据**（不是容器数据） |
| dmesg | 内核日志 | **直接失败** |
| perf | 能剖析一切 | **要么跑不起来，要么能剖析别的租户** |
| tcpdump | 能嗅所有网卡 | 只能嗅容器自己的网卡 |

**第二个盲区是软件限额**：

> Many monitoring tools written for stand-alone systems have yet to develop support for OS virtualization (containers), and are blind to cgroup and other software limits.

后果是双向误导：**会谎报有余量（其实已撞到软件限额）**，**也会把别的租户的用量显示成你的**。

**第三个障碍是「容器 ID 不存在」**：容器 ID 由**用户态软件**创建与管理，内核并不认识它；
传统工具要支持容器就得支持 Kubernetes、Docker 以及所有其它平台——不现实。
Solaris 的 zones 有**内核级 zone ID**，`ps(1)` 就能显示，这是 Linux 缺的那一环。
Linux 上的实际办法有五条：**容器平台自带工具**（如 docker stats）、**监控软件的插件**、
**cgroup 统计**（需自己搞清哪个 cgroup 对应哪个容器）、**namespace 映射**（`nsenter`）、
以及 **BPF tracing**（能直接读 cgroup 与 namespace 信息）。

#### §11.3.4.1 §11.3.4.2 宿主侧：容器工具、cgroup 统计、namespace 映射、BPF

**容器平台工具**：`kubectl top nodes`（节点的累计 CPU 毫秒、当前 CPU%、内存）与
`kubectl top pods`（Pod 的累计 CPU 时间与当前内存），**都需要 metrics server 在跑**；
另有 cAdvisor、Sysdig、Google Cloud Monitoring 等 GUI 方案。
Docker 侧用 `docker stats`（CPU%、内存用量/限额、内存%、网络 I/O、块 I/O、PID 数）。

**cgroup 统计**（`/sys/fs/cgroup` 下）：
`cpuacct.usage` 是该 cgroup 的**总 CPU 纳秒**；
`cpu.stat` 里 `nr_periods`/`nr_throttled`/`throttled_time` 直接告诉你**被限流了多少次、总共被限流多久**；
`cpuacct.usage_percpu` 按 CPU 分别给出（16 CPU 机器就有 16 个字段）。
命令行工具 `htop(1)` 与 `systemd-cgtop(1)` 会读这些统计。

**namespace 映射**：容器里的 PID 与宿主 PID 通常不同。
`/proc/PID/status` 的 `NSpid` 给出对应关系（宿主 4915 = 容器 753）；
需要反向映射时可以扫所有 status 文件，但**同一个容器 PID 可能出现在多个 PID namespace 里**，
此时要用 `/proc/PID/ns/` 下的符号链接比对 **namespace ID** 来确认是哪一个。
挂载 namespace 也会影响工具：perf 会去 `/tmp/perf-PID.map` 找符号文件，而容器里的 `/tmp` 与宿主不是同一个，
PID 也不同（perf 后来获得了 namespace 支持，内核也提供 `/proc/PID/root` 直接访问容器根目录）。
`nsenter` 则可以直接在指定 namespace 里执行命令：

```
# nsenter -t 4915 -m -p top
top - 21:14:24 up 32 days, 23:23,  0 users,  load average: 0.32, 0.09, 0.02
Tasks:   3 total,   2 running,   1 sleeping,   0 stopped,   0 zombie
%Cpu(s):  0.2 us,  0.1 sy,  0.0 ni, 99.4 id,  0.0 wa,  0.0 hi,  0.0 si,  0.2 st
KiB Mem :  1996844 total,    98400 free,   858060 used,  1040384 buff/cache
KiB Swap:        0 total,        0 free,        0 used.   961564 avail Mem

  PID USER      PR  NI    VIRT    RES    SHR S  %CPU %MEM     TIME+ COMMAND
  753 root      20   0  818504  93428  11996 R 100.0  0.2   0:27.88 java
    1 root      20   0  18504   3212   2796 S   0.0  0.2   0:17.57 bash
  766 root      20   0  38364   3420   2968 R   0.0  0.3   0:00.00 top
```

**BPF tracing** 可以读内核里的 cgroup 与 namespace 信息，从而**按容器**聚合。
原文用一个统计新进程数的 bpftrace 工具做演示，扩展点是从
`task_struct` 走到 **uts namespace 的 nodename**，把它当作 map 的 key：

```cpp
#!/usr/local/bin/bpftrace

#include <linux/sched.h>
#include <linux/nsproxy.h>
#include <linux/utsname.h>

tracepoint:syscalls:sys_enter_clone,
tracepoint:syscalls:sys_enter_fork,
tracepoint:syscalls:sys_enter_vfork
{
        $task = (struct task_struct *)curtask;
        $nodename = $task->nsproxy->uts_ns->name.nodename;
        @new_processes[$nodename] = count();
}
```

⚠️ **这里有一条硬限制**（原文明确警告）：这套做法**只在系统调用这类「进程上下文内」的事件上成立**——
`curtask` 能拿到责任进程。但如果 trace 的是**进程异步事件**（例如磁盘 I/O 完成中断），
责任进程可能已经不在 CPU 上，`curtask` **拿不到正确的 nodename**。

#### §11.3.4.3 容器内看到的东西（guest 侧）

在**一个空闲的容器**里跑 `iostat -sxz 1`，会看到 CPU 和磁盘都「很忙」——
那其实是**宿主**的数字，原文用一个具体例子说明这种困惑：
「为什么我的容器是忙的？」答案是工具显示的是包含其他租户活动的宿主统计。

容器里的 cgroup 块 I/O 统计（`blkio.throttle.io_serviced`）按操作类型计数；
原文的验证手法是**间隔打印两次**（中间 `sleep 10`），计数没涨 ⇒ 容器没有在做磁盘 I/O。
字节数在 `blkio.throttle.io_service_bytes`。但这些计数器**还不足以让 iostat 变成 container-aware**，
需要 cgroup 暴露更多计数器。

**「container-aware」有两种不同的含义**（原文把它们称为场景 A / 场景 B）：

| 场景 | 目标 | 工具应该怎么做 |
|---|---|---|
| A：容器是**隔离的服务器**（云厂商的典型做法） | 工具只显示当前容器的活动（iostat 只显示容器发起的磁盘 I/O） | 隔离所有统计来源（/proc、/sys、netlink）。**代价**：诊断邻居争抢变得更难，只能从无法解释的设备延迟上升去推断 |
| B：容器是**打包方案**（公司自营容器云） | 让用户看到宿主统计有助于理解硬件状态与 noisy neighbors | 提供**拆分视图**（当前容器 vs 宿主 vs 其他容器），而不是隐藏 |

两种场景都应该**把资源控制显示出来**（例如 iostat 可以加一个按 blkio 吞吐与 IOPS 限额算出的 `%cap` 列）。
原文强调：**允许看到物理资源，guest 才能排除「noisy neighbors」这类问题**，
这能减轻运维方的支持负担（人容易责怪自己看不见的东西）；
而且这与硬件虚拟化形成对比——**硬件虚拟化对 guest 隐藏物理资源，也没有办法把这些统计共享出去**（除非用外部手段）。

**Tracing 工具**（perf、Ftrace、BPF）在容器里因权限（`perf_event_open(2)`、`bpf(2)`）
以及 `/proc`、`/sys` 的访问限制而**不能直接工作**。两种解法：
① **内核过滤**——事件与参数可由内核过滤，例如 trace `block:block_rq_issue` 时只显示当前容器的磁盘 I/O；
② **宿主 API**——宿主暴露接口，容器请求执行常用 BCC 工具（如 `execsnoop(8)`、`biolatency(8)`），
宿主校验请求、执行**过滤版**的工具并返回输出。
在「不需要隔离」的场景，则是把 `/proc`、`/sys`、系统调用权限放开，让工具自己变 container-aware。

原文的判断：**这会长期伤害高级用户**（登录到系统上敲命令做性能分析的人），
而**用监控产品（agent 往往已经部分 container-aware）的用户受影响较小**。

#### §11.3.4.4 分析策略

**先查软件限制，再查物理资源**。原文的判断很直接：资源控制这些限额**远早于物理限制就会遇到**，
所以更可能是当前生效的原因，**应该先查**。
而传统工具（`top(1)`、`iostat(1)`）是在容器与资源控制出现之前写的，**默认不含资源控制信息**，用户很容易忘查。

逐资源的检查清单：

| 资源 | 查什么 |
|---|---|
| CPU | 按 cpusets / bandwidth / shares 三种机制分别判定（见下面的五统计量流程） |
| 内存 | 当前用量对比 memory cgroup 的限额 |
| 文件系统容量 | 同普通文件系统一样查（`df(1)`） |
| 磁盘 I/O | 查 `/sys/fs/cgroup/blkio` 的限流配置；`blkio.throttle.io_serviced` 与 `io_service_bytes` **若以与限流相同的速率增长 ⇒ 证据表明磁盘 I/O 正被限流**；有 BPF 时可用 `blkthrot(8)` 确认 |
| 网络 I/O | 当前吞吐对比已知带宽上限（**可能只有宿主能看到**）；到达上限后网络 I/O 延迟会上升，因为租户被限流 |

**CPU 被限流的判定流程**（作者自己的经验流程，用五个统计量）：
**① throttled time**（cpu cgroup 的 `throttled_time`）、
**② 非自愿上下文切换**（`/proc/PID/status` 的 `nonvoluntary_ctxt_switches` 增长）、
**③ 宿主是否有空闲 CPU**（`mpstat` 的 `%idle`、`/proc/stat` 等）、
**④ cpuset 的 CPU 是否 100% 忙**（用了 cpusets 时从 `mpstat`、`/proc/stat` 读）、
**⑤ 其他租户是否都空闲**（用 `docker stats` 之类的容器工具，或从 `top` 里只看一个容器在吃 CPU 判断）。
作者指出：**shares 限制最难识别，因为没有对应的统计量**。

最后一条提醒：这一大节描述的是 **cgroup v1 与 Linux 容器的现状**，内核能力变化极快，
厂商文档与工具的 container-aware 进度都在追赶；想跟得上就得读新内核的 Documentation 目录，
以及 cgroup v2 的主要开发者 Tejun Heo 的文档。

## §11.4 轻量虚拟化：两头的优点都要

**目标**：硬件虚拟化的**安全性** + 容器的**效率与快启动**。

**做法**：用基于处理器虚拟化的轻量 hypervisor + **最少量的模拟设备**。
这与全机器 hypervisor 不同——后者源自桌面虚拟机，要支持视频、音频、BIOS、PCI 总线等，
而且要照顾不同年代的处理器支持水平。**只做服务器计算的 hypervisor 不需要支持这些**，
而且今天写的东西可以假定现代处理器虚拟化特性都具备。

**体量对比**（原文给的数字最直观）：

> To describe the difference this makes: quick emulator (QEMU) is a full-machine hardware hypervisor used for KVM, and has over 1.4 million lines of code (QEMU version 4.2). Amazon Firecracker is a lightweight hypervisor and has only 50 thousand lines of code [Agache 20] .

轻量 VM 的行为类似 §11.2 里的 **Config B**；相对硬件 VM 有**更快的启动、更低的内存开销、更好的安全性**，
还可以**把 namespaces 当成额外一层安全**。术语上作者偏好 **MicroVM**，因为 containers 通常被用来指 OS 虚拟化。

**实现一览**：**Intel Clear Containers**（2015，用 Intel VT，启动时间低于 45 ms；2017 并入 Kata Containers）、
**Kata Containers**（2017，OpenStack 基金会治理，口号是「容器的速度，VM 的安全性」）、
**Google gVisor**（2018 开源，用 Go 写的**专用用户态内核**提升容器安全性——这条路更像容器）、
**Amazon Firecracker**（2019 开源，用 KVM 但换掉 QEMU 用新的轻量 VMM，系统启动约 100 ms）。

**开销**：与 KVM 虚拟化类似，但**内存占用低得多**（VMM 很小）——
Intel Clear Containers 2.0 报告每容器 **48–50 MB** 内存开销，Amazon Firecracker 报告 **小于 5 MB**。

**资源控制**：VMM 进程跑在宿主上，因此可以用 **OS 级资源控制**（cgroups、qdisc 等）管理，与 KVM 虚拟化一致。

**可观测性**：与 KVM 一致——宿主侧所有物理资源可用标准 OS 工具观察，
**guest VM 表现为进程**但**看不到 guest 内部**（要运维者分析内部，得先被授权，如 SSH）；
guest 侧因为有独立内核，**内核 tracing 工具（含 BPF）全部可用**。

原文给的一组对照很直观：宿主 `top` 里 Firecracker VM 就是**一个叫 `firecracker` 的进程**
（示例中占 200% CPU，**从宿主看根本不知道是 guest 里的哪个进程在吃**）；
而在 guest 里跑 `top`，能看到 CPU 是**两个 bash 程序**在吃。
另外注意两边的**表头汇总数字也不同**（宿主 1 分钟 load 4.48，guest 1.89）——
因为 guest 有自己的内核、只统计 guest 内的量。**这与容器正好相反**：容器里可能看到宿主统计。

## §11.5 其他类型：连 OS 都没有的云原语

- **FaaS**：开发者提交一个函数，云按需运行。开发体验简化（没有服务器要管，serverless），
  但**有性能后果**：函数的**启动时间可能相当可观**；而且没有服务器，
  **终端用户跑不了传统命令行观测工具**，性能分析通常只能依赖**应用自己提供的时间戳**。
- **SaaS**：高层软件，终端用户不用配置服务器或应用。
  **性能分析限于运营者**——用户拿不到服务器，能做的只有客户端计时。
- **Unikernels**：把应用与最小内核部分编译成**单一二进制**，由硬件 hypervisor 直接执行，不需要操作系统。
  收益：减少指令文本 ⇒ 减少 CPU cache 污染；剥掉未用代码 ⇒ 安全收益。
  代价：**没有 OS 可以登录做观测**（`/proc` 之类的内核统计可能根本不存在）。
  好消息是实现通常允许 unikernel **作为一个普通进程运行**，从而提供一条分析路径
  （只是环境与 hypervisor 不同）；hypervisor 也可以用**栈剖析**的方式检查它们
  （作者做过一个为运行中的 MirageOS unikernel 生成火焰图的原型）。

一句话总结这一节：**这些形态没有 OS 让你登录（也没有容器让你进入）去做传统性能分析**——
FaaS 与 SaaS 必须由运营者分析；unikernel 需要自定义工具与统计，理想情况是 hypervisor 提供剖析支持。

## §11.6 三种技术的比较

原文 Table 11.7 的要点（本篇整理）：

| 维度 | 硬件虚拟化（KVM） | OS 虚拟化（容器） | 轻量虚拟化（Firecracker） |
|---|---|---|---|
| CPU 性能 | 高（有 CPU 支持） | 高 | 高（有 CPU 支持） |
| CPU 分配 | 固定为 vCPU | **灵活（shares + bandwidth）** | 固定为 vCPU |
| I/O 吞吐 / 延迟 | 高 / 低（需 SR-IOV，且无 QEMU） | 高 / 低（无固有开销） | 高 / 低（需 SR-IOV） |
| 内存访问开销 | 有（EPT/NPT 或影子页表） | **无** | 有（EPT/NPT 或影子页表） |
| 内存损耗 | 有（额外内核、页表） | **无** | 有（额外内核、页表） |
| 内存分配 | 固定（可能双重缓存） | **灵活（闲置内存可用于文件系统缓存）** | 固定（可能双重缓存） |
| 资源控制 | 多（内核 + hypervisor） | 多（取决于内核） | 多（内核 + hypervisor） |
| 宿主侧可观测性 | 中（资源用量、hypervisor 统计，KVM 式可做 OS 检查，但看不到 guest 内部） | **高（一切都能看）** | 中（同硬件虚拟化） |
| guest 侧可观测性 | 高（完整内核与虚拟设备检查） | 中（仅用户态、内核计数器、内核可见性受限），且**额外混有宿主级指标**（如 iostat） | 高（完整内核与虚拟设备检查） |
| 可观测性更利于谁 | 终端用户 | 宿主运维者 | 终端用户 |
| hypervisor 复杂度 | 最高 | 中（OS） | 高（轻量 hypervisor） |
| 能否跑不同 OS guest | 能 | 通常不能（有时可用 syscall 翻译，但有性能代价） | 能 |

原文自己给这张表加了限定：**它会随着技术演进而过时**，但仍可用来说明「该看哪些维度」。

**本章最重要的一句论断在 §11.6 的末尾**：

> Virtualization technologies are often compared using micro-benchmarking to see which performs the best. Unfortunately, this overlooks the importance of being able to observe the system, which can lead to the largest performance gains of all.

**用微基准比较虚拟化技术，会忽略「能不能观测系统」这件事，而后者带来的性能收益往往是最大的**——
因为可观测性能让你识别并消除**无用功**，这种收益远大于 hypervisor 之间的细微差别。

由此推出的立场（原文自己的判断）：
**对云运营者**，容器的可观测性最好（宿主侧能看到所有进程及其交互）；
**对终端用户**，虚拟机最好（给用户内核访问权，可以跑所有内核级性能工具）。
还有第三条路：**有内核访问权的容器**，双方都能看全，但这只在**客户自己运营容器宿主**时才可行
（因为容器之间缺少安全隔离）。作者预期**轻量硬件 hypervisor 的使用会继续增长**，理由之一正是它对终端用户的可观测性。

## 章末选型判据

| 场景 | 选什么 | 别用什么 |
|---|---|---|
| 宿主侧要定位「是谁在抢资源」 | 容器（宿主可见全部进程与交互） | 硬件虚拟化（看不到 guest 内部） |
| 终端用户要自己下钻到内核 | 硬件虚拟化或轻量虚拟化（有独立内核，BPF/perf 全可用） | 容器（内核 tracing 工具基本不可用） |
| 需要租户之间按权重弹性共享 CPU | 容器的 CFS shares（但**必须同时用带宽上限削平 burst**） | 固定 vCPU 数量（粗且僵硬） |
| 交付给用户的性能承诺不能被邻居改变 | 硬件虚拟化 / 轻量虚拟化（vCPU 固定） | 纯 shares 的容器（burst 会让承诺失效） |
| 追求单机密度与秒级启动 | 容器（毫秒级启动、共享 page cache） | 全机器 hypervisor（QEMU 百万行代码级复杂度） |
| 追求接近裸机的 I/O 性能 | 有 SR-IOV 且**没有 I/O proxy** 的 hypervisor（如 Nitro） | 带 QEMU proxy 的 Xen/KVM 常规配置 |
| 怀疑 guest 的 CPU 开销来自虚拟化 | 看 `perf kvm stat live` 的 exit 分布与时长；exit 多且 HLT 占比高说明是空闲而非开销 | 只看 guest 内部的 CPU 利用率（分母是虚拟 CPU，看不出来） |
| 怀疑磁盘 I/O 慢 | 先看 `biosnoop` 的**离群点成因**（大小？访问模式？都不是 ⇒ 物理争抢或设备问题） | 只看 `%util`（虚拟设备上难以解释） |
| 怀疑容器被邻居抢了 | 在**宿主**上看邻居进程；在容器里看 cgroup 的 `throttled_time` 与 `nr_throttled` 是否在涨 | 只在容器里看 `iostat`（那是**宿主**的数字，会误导） |
| 判断容器 CPU 是否被限流 | 按五统计量判定（throttled_time / nonvoluntary_ctxt_switches / 宿主 %idle / cpuset 是否满 / 其他租户是否空闲） | 只看容器内 `%CPU`（burst 时它看着很健康） |
| 用传统工具分析容器 | 把「物理资源视图」与「cgroup 软件限额视图」**分开看** | 相信 iostat/mpstat/vmstat/free 在容器里的输出（那是宿主数据） |
| 选 FaaS / SaaS / unikernel | 只在**能接受无服务器级观测**的前提下选（性能分析只能靠应用时间戳或运营者） | 有明确的「自己下钻定位」需求时 |
| 评估两种虚拟化技术的优劣 | 先比较**可观测性**（能看多少、能定位多细），再比微基准 | 只跑微基准比吞吐/延迟（会漏掉最大的那类收益） |
