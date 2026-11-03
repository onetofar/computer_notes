---
title: Chapter 11. Cloud Computing（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - 云计算
  - 虚拟化
  - 多租户
  - 容器
  - 性能工程
source: "《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg，Addison-Wesley 2020，英文原版）第 11 章"
companion: "[[11-Chapter 11. Cloud Computing]]"
---

# 第 11 章 讲义：云上慢，通常慢在你自己

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[11-Chapter 11. Cloud Computing|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：`/home/xier/dev/game-project/otf-server/` —— 本仓库跑在**本地/自建**环境，不在云上；本章借它对照「哪些指标一旦上云就会失真」。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原章。

---

## 0. 开局：慢了三倍，是云的锅吗？

一个服务从自建机器搬到云厂商的通用实例，第一天测出来**延迟是原来的三倍**。

团队的第一反应是「云慢」。第二反应是打开监控，看到 CPU 使用率只有 20%——那就更说不通了：不忙，怎么会慢？

第三反应才是对的：**别看使用率，看排队。**

第 2 章的 USE 方法里，`Saturation`（饱和度）说的是「排队等资源的程度」。在自建机器上，排队只有一个来源：物理资源不够。在云上，排队有**两个**来源：

1. **物理的**：vCPU 真的不够、真的被抢了；
2. **软件的**：cgroup 的 CPU 限额、hypervisor 的配额、邻居租户。

软件限额**先于**物理上限撞上。所以你看到「20% 使用率 + 三倍延迟」是完全自洽的——那 20% 是**已经被限额削过一遍**之后的 20%。

这一章讲的就是：云到底在你的机器中间插了几层、每层塞了什么、哪些层你看得见哪些看不见，以及——**为什么绝大多数「云上慢」最后查出来是自己的问题**。

（推演）把云想象成「一个你不能选邻居的合租公寓」：硬件是大家的、水电是大家的、但你的门牌号决定了你能拧开哪些水龙头、看得到哪些电表。

---

## 1. §11.1 Background：一台实例，其实是一份可调参数

### §11.1.1 Instance Types：实例类型变成了旋钮

原文列了 AWS 的家族划分：

```
m5 : General purpose (balanced)
c5 : Compute optimized
i3, d2 : Storage optimized
r4, x1 : Memory optimized
p1, g3, f1 : Accelerated computing (GPUs, FPGUs, etc.)
```

同族里还有尺寸，从 `m5.large`（2 vCPU / 8 GB）到 `m5.24xlarge`（96 vCPU / 384 GB）。原文有一句是全章的题眼：

> With so many options and the ease of redeploying instances, the instance type has become like a tunable parameter that can be modified as needed.

对。**过去你买的是五年不能改的机器，现在你买的是一个旋钮。** 这是云最实在的性能红利：性能调优的反馈回路从「半年一次采购」缩到「五分钟一次重启」。

### §11.1.2 Scalable Architecture：横向扩展成为默认

自建时代是**纵向扩展**（买更大的机器，有电梯门和集装箱的物理上限，还有 CPU cache 一致性、供电散热的墙），云时代是**横向扩展**（多台小机器）。

原文给的分层图（Figure 11.1 Cloud architecture: horizontal scaling）：负载均衡器 → Web → 应用 → 数据库。每一层由若干实例并行组成，负载来了就加实例；也可以整组加（原文提到 Shopify 把这种组合单元叫 "pods"）。

数据库是这套模型的痛点：传统数据库必须有一个 primary，于是有 **shards**（逻辑切分，每片一个主从），再往上是 Cassandra / CockroachDB / Aurora / DynamoDB 这类云原生数据库。

### §11.1.3 Capacity Planning：容量规划从「预测」变成「反应」

原文的理由很实在：自建机器从审批到上架要几个月（审批、等配件、发货、上架、安装、测试），而且采购是**按峰值买、按平均跑**——机器大部分时间在吃灰。

云上反过来：

> Instead of spending time planning what may be needed, companies can increase the number of server instances they use as needed

而且能**自动**——根据监控指标驱动云 API 扩容。原文列的两项活动：

- **Dynamic sizing（自动伸缩）**；
- **Scalability testing**（临时买一大片云环境，用合成负载压，**这本身就是一个基准测试活动**——第 12 章会展开）。

自动伸缩的代价，原文直接点名：

> Auto scaling can solve the need to quickly respond to changes in load, but it also risks overprovisioning

**DoS 攻击在自动伸缩眼里就是「负载上涨」。** 同理，**一次性能回归在自动伸缩眼里也是「负载上涨」**——于是它加机器，加了机器更慢，更慢更慢。这解释了原文那个 Netflix 每天增删「数万台实例」图（Figure 11.3）的另一面。

还有一种更贵的坑：**bursting**（突发）。某些编排平台能让你在没有资源时「先借用」空闲 CPU，不额外收费。原文的判断很克制：这**本来是为了防止过度采购**（先借一点验证负载是不是真的），但它变成了一个**测量陷阱**——

> A problem with shares is that bursting can confuse capacity planning, especially since many monitoring systems do not show bursting statistics (they should).

以及那句最有画面感的：

> To the customer, this can feel like bait and switch.

（先给你 100%，等你依赖上，再变回 10%。）

### §11.1.4 Storage：三种存储，三种延迟脾气

| 类型 | 例子 | 说明 |
|---|---|---|
| File store | NFS | 文件 |
| Block store | iSCSI | 块设备 |
| Object store | HTTP API | 对象 |

实例自带的盘是 **ephemeral**（实例销毁即消失）；持久化要另外接上面三种服务。原文提醒：它们**跑在网上**，而网络基础设施和存储设备**与其他租户共享**，所以

> For these reasons, performance can be much less predictable than with local disks, although performance consistency can be improved by the use of resource controls by the cloud provider.

配套的补偿手段是缓存：

> The increased latency for network storage access is typically mitigated by using in-memory caches for frequently accessed data.

以及买 IOPS（AWS EBS Provisioned IOPS）——**花钱买确定性**，这在自建时代是买不到的。

### §11.1.5 Multitenancy：噪音邻居

> For example, another guest on the same host might perform a full database dump during your peak load, interfering with your disk and network I/O.

> Worse, a neighbor could be evaluating the cloud provider by executing micro-benchmarks that deliberately saturate resources in order to find their limit.

原文给的解法叫 **resource management** / **resource isolation**：给每个租户设限额或优先级（CPU、内存、磁盘 / 文件系统 I/O、网络吞吐）。

### §11.1.6 Orchestration（Kubernetes）

Kubernetes 把容器当成 **Pods**（共址的一组容器，共享网络命名空间、可以用 localhost 通信）部署，每个 Pod 有自己的 IP；**Service** 是给一组 Pod 端点提供的稳定抽象；物理机叫 **Nodes**，连同一个 API server 的 Node 组成一个 **cluster**。

性能相关的两块：调度（放哪台机器上）与网络（容器网络和负载均衡的实现方式）。原文指出 CNI 有两条路线：**Calico** 基于 netfilter / iptables，**Cilium** 基于 BPF；Cilium 还提供 BPF 版 kube-proxy 替代品。

---

## 2. §11.2 Hardware Virtualization：多了一层，谁在替谁干活

### §11.2.1 Implementation：三代虚拟化技术

**Type 1 / Type 2** 的老分类：Type 1 直接跑在处理器上（Xen），Type 2 跑在宿主 OS 里（KVM），客人看起来像宿主上的进程。作者说这个分类**已经不完全适用**（因为 Type 2 用内核模块拿到了直接硬件访问权），并给出了更实用的 **Config A / Config B**（Figure 11.5）：

- **Config A（裸金属 hypervisor）**：hypervisor 直接跑在 CPU 上，为客人建 domain，把虚拟 CPU 调度到真 CPU 上。例：Xen。
- **Config B（宿主 OS 内）**：hypervisor 由宿主内核调度，客人和其他进程一起被调度。例：KVM。

两者都可能跑一个 **I/O proxy**（典型是 QEMU 软件）来代服客人的 I/O——**这一层就是开销的来源**。

然后是**虚拟化指令的三代技术**：

| 技术 | 做法 | 代价 |
|---|---|---|
| **Binary translation** | 识别并翻译客人内核里操作物理资源的指令。VMware 早期方案：VMM 跑在 ring 0，客人内核挪到 ring 1，需要翻译时回调 VMM | 「costing significant CPU overhead」 |
| **Paravirtualization** | 需要虚拟化的指令换成 **hypercall**；guest OS 知情、可优化。Xen 还能把多个 hypercall 合成 **multicall** | 依赖 guest OS 配合（Windows 历史上不提供） |
| **Hardware-assisted** | AMD-V / Intel VT-x（2005–2006 引入）。客人特权指令**陷入**到更高特权的 VMM，由它模拟 | 通常首选 |

原文引 VMware 2007 年的一段自白很有意思——他们说自己被「过高的 hypervisor→guest 转换开销和僵硬的编程模型」拖着，所以二进制翻译**当时**反而比第一代硬件辅助快。

**guest exits**（客人退出）是度量这一切的指标：

> The rate of transitions between the guest and hypervisor, as well as the time spent in the hypervisor, can be studied as a metric of CPU overhead.

原文给了 KVM 的退出处理函数表（`arch/x86/kvm/vmx/vmx.c`，Linux 5.2）——`EXIT_REASON_HLT` → `handle_halt` → `kvm_vcpu_halt()`。整个函数只有几行，作者点明原因：

> As with many guest exit types, the code is kept small to minimize CPU overhead.

### I/O 路径：步数决定性能

原文 Figure 11.7（Xen、KVM、Nitro 的 I/O 路径）里，GK 是 guest kernel、BE 是 back end，虚线是控制路径、实线是数据路径。结论一句话：

> The number of steps in the I/O path, both control and data, is critical for performance: the fewer, the better.

原文举了个 2006 年的数：Xen（特权客人）做一次 I/O 要 10 步，KVM 只要 5 步。**Nitro**（AWS 2017）连 QEMU proxy 都不用，直接硬件支持，所以「near bare-metal」。

---

## 3. §11.2.2 Overhead / §11.2.3 Resource Controls

### 开销清单（按资源过一遍）

- **CPU**：客体应用**直接跑在处理器上**，所以
  > In general, the guest applications execute directly on the processors, and CPU-bound applications may experience virtually the same performance as a bare-metal system.
  开销集中在特权调用、硬件访问、内存映射。
- **内存映射**：多了一次翻译。客人虚拟 → 客人物理（guest kernel 做）→ 宿主物理（hypervisor VMM 做）。现代 CPU 有 **EPT**（Intel）/ **NPT**（AMD）把两次翻译合并；没有的话 hypervisor 维护 **shadow page tables**，并在客人运行时改写客人的 **CR3** 寄存器指向影子页表。
- **内存大小**：客人自带内核、存储架构可能**双重缓存**、KVM 式还要多跑一个 QEMU 进程。
- **I/O**：历史上开销最大（每次设备 I/O 都要翻译）。缓解手段：半虚拟化驱动（合并 I/O、少触发中断）、**PCI pass-through**（设备直给客人，性能最好但牺牲多租户灵活性和 live migration）、**SR-IOV**（一张卡的多个功能直分给不同客人）。
- **多租户争抢**：
  > Depending on the hypervisor configuration and how much CPUs and CPU caches are shared between tenants, there may be CPU stolen time and CPU cache pollution caused by other tenants, reducing performance.

### 配额：Xen 与 KVM 的两条路

- **vCPU** 是最粗的粒度；Xen 的 hypervisor CPU 调度器能做细粒度配额，三种算法：**BVT**（借用虚拟时间）、**SEDF**（实时，保证期限）、**Credit-based**（权重 + 上限 + 负载均衡）。KVM 这类则由宿主 OS 的 cgroup CPU 带宽控制来做。
- 两者都有同样的**盲区**：
  > A guest’s CPU usage is typically opaque to the hypervisor, and guest kernel thread priorities cannot typically be seen or respected.

  也就是说，一个客人里的低优先级日志轮转守护进程，和另一个客人里的关键应用服务器，在 hypervisor 眼里**优先级相同**。Xen 用 **IDD**（isolated driver domains）把 I/O 服务拆出去以便单独计量。
- **CPU Cache**：Intel **CAT** 可以在客人之间切分 LLC。原文提醒：限制 cache 访问**本身也会伤性能**。
- **内存**：guest 配置里写死上限；guest 内核自己在这个限额内做分页 / 交换。VMware 的 **balloon driver** 是经典招数——在客人里「充气球」占内存，hypervisor 把它收走。原文的提醒很实在：
  > When balloon drivers are in use (to check from the guest, search for “balloon” in the output of dmesg(1)), I would be on the lookout for performance issues that they may cause.

---

## 4. §11.2.4 Observability：两层之间的裂缝

这一节是全章最实用的部分：**你从哪儿看，能看到什么**。

| 从哪儿看 | 能看到 | 看不到 |
|---|---|---|
| 特权客人 / 宿主（Xen / KVM） | 所有物理资源（标准 OS 工具全都能用）；per-guest 资源统计 | **客人的内部**（进程、应用调用栈）；走 pass-through / SR-IOV 的 I/O 甚至看不见 |
| 硬件支持的宿主（Nitro） | 同上，但 SR-IOV 让设备 I/O 更难观测 | （Nitro 的内部实现不公开） |
| 客人里 | 虚拟资源及其使用情况；**自己的内核完整可见** → perf / Ftrace / BPF 全都能用 | 物理资源（只能**推断**） |

两个必须记住的细节：

1. **pass-through / SR-IOV 会让 I/O 对 hypervisor 隐身**：
   > I/O can become invisible to the hypervisor, and not appear in iostat(1) or other tools.

   原文给的绕法是用 PMC 反推。
2. **客人的视角是「被隔离的」**。物理资源用量通常**看不到**，只有脱敏后的 SNMP / 云 API。这直接导致原文那句：
   > This may tempt the guest customers to blame mysterious performance issues on resource contention caused by noisy neighbors.

   ——**看不见的东西，人最爱怪。** 而更糟的是：**有时怪对了**。

### 工具：怎么从宿主看客人的 CPU 去了哪

**Xen**：`xentop`（原文给了输出样例，字段有 `CPU(%)` / `MEM(k)` / `MAXMEM(k)` / `VCPUS` / `NETS` / `NETTX(k)` / `NETRX(k)` / `VBDS` / `VBD_OO`（阻塞排队的请求 = 饱和）/ `VBD_RD` / `VBD_WR`）。更深的用 `xentrace` + `xenanalyze`，还有 `xenoprof`。

**KVM**：客人在宿主 OS 里就是一个进程（`qemu-system-x86`）。`top` 能看到它；`pidstat -tp` 能看到它的线程（原文输出里 `CPU 0/KVM` / `CPU 1/KVM` 各自占 73% / 84%）。要区分哪个 QEMU 是哪个客人，看进程参数：`ps -wwfp PID` 读 `-name`。

**看 guest exits**（最有信息量的一张表）：

```
host# perf kvm stat live
...
           VM-EXIT Samples Samples%   Time%  Min Time    Max Time       Avg time

         MSR_WRITE    1668   68.90%   0.28%    0.67us     31.74us    3.25us ( +-  2.20% )
               HLT     466   19.25%  99.63%    2.61us 100512.98us 4160.68us ( +- 14.77% )
  PREEMPTION_TIMER     112    4.63%   0.03%    2.53us     10.42us    4.71us ( +-  2.68% )
[...]
```

原文的解读方法值得抄下来：**退出次数少、且大部分是 `HLT`，说明客人的 CPU 基本闲着**；反之，如果 I/O 操作多、既有中断产生又有中断注入，客人多半在走虚拟网卡 / 虚拟盘。

再深一点，`perf list | grep kvm` 有一堆 `kvm:*` tracepoint，其中 `kvm:kvm_exit` 最有用：

```
host# bpftrace -lv t:kvm:kvm_exit
tracepoint:kvm:kvm_exit
    unsigned int exit_reason;
    unsigned long guest_rip;
    u32 isa;
    u64 info1;
    u64 info2;
```

配合 `kvm:kvm_entry` 就能测出**每次退出花了多久**。

### 客人里能看什么

`mpstat -P ALL 1`（只显示分配给客人的 vCPU）、`vmstat 1`——后者有一列 **`st`（stolen）**，原文称之为「a rare example of a virtualization-aware statistic」：

> Stolen shows CPU time not available to the guest: it may be consumed by other tenants or other hypervisor functions

**`st` 这一列，是云上最有价值的一个数。** 它直接告诉你「有多少 CPU 时间不是你的」。自建机器上没有这一列——这就是「云上慢，通常慢在你自己」这句话的**可证伪形式**：如果 `st` 很高，那不全是你的问题；如果 `st` 接近 0 而延迟还是高，那**大概率是你自己的问题**。

设备延迟可以看 `biosnoop(8)`（BPF 工具，客人里能跑，因为客人有自己的内核）。

---

## 5. §11.3 OS Virtualization：容器把「一台机器」缩成「一份配额」

### §11.3.1 Implementation：内核里其实没有「容器」

容器 = **namespaces**（隔离视野）+ **cgroups**（限制用量）+ 通常再加 **seccomp-bpf**（限制系统调用）。原文给了 namespaces 表（`pid` / `mnt` / `net` / `user` / `uts` / `ipc` / `cgroup` / `time`），以及：

```
# lsns
        NS TYPE   NPROCS   PID USER             COMMAND
4026531835 cgroup    105     1 root             /sbin/init
4026531836 pid       105     1 root             /sbin/init
...
```

**容器里 PID 1 和宿主 PID 1 不是同一个进程**，只是各自命名空间里的 1。

容器相对硬件虚拟化的**优势**（原文列的）：毫秒级启动、内存全给应用（没有额外内核）、统一文件缓存（避免宿主与客人双重缓存）、cgroup 的细粒度共享控制、宿主侧可观测性更高、共享内存页、CPU 是真 CPU（自适应互斥锁的假设仍然成立）。

**劣势**（原文列的）：内核资源争抢加剧（锁、缓存、缓冲、队列）、**客人侧可观测性下降**（内核通常分析不了）、**任何一次内核 panic 波及所有客人**、客人装不了自定义内核模块、跑不了长时间 PGO 内核、不能跑不同内核版本。

原文把前两条放在一起点了一下——那是最要命的一处：

> Consider the first two disadvantages together: A guest moving from a VM to a container will more likely encounter kernel contention issues, while also losing the ability to analyze them.

### §11.3.2 Overhead：容器本身的 overhead 很小，争抢才是问题

> The overhead of container execution should be lightweight: application CPU and memory usage should experience bare-metal performance

用户态运行**零额外 CPU 开销**（所有进程本来就在某个 namespace 和 cgroup 里）；内存映射、load / store 也无开销。开销来自 **I/O 路径上的额外层**（overlayfs、bridge 网络）和**多租户争抢**。

争抢的具体清单（原文 §11.3.2 Multi-Tenant Contention）：CPU cache 命中率下降（切到别的容器线程甚至可能刷掉 L1）、TLB 命中率下降、别人的设备中断打断你、**内核层面的 buffer / cache / queue / lock 争抢**（多租户能把负载抬高一个数量级）、iptables 带来的网络 CPU 开销。

---

## 6. §11.3.3 Resource Controls：一份 CPU 配额的完整算术

### cgroups v1 的清单

`blkio`（块 I/O：字节 + IOPS）、`cpu`（shares）、`cpuacct`（统计）、`cpuset`（绑核）、`devices`、`hugetlb`、`memory`（进程内存 + 内核内存 + swap）、`net_cls`（打 class id 给 qdisc 用）、`net_prio`、`perf_event`（让 perf 能监控某个 cgroup）、`pids`、`rdma`。

### CPU：cpusets vs shares vs bandwidth

- **cpusets**：整颗 CPU 划给容器。好处是不被打断、容量稳定；坏处是**空闲的 CPU 别人用不了**。
- **shares**（CFS）：允许**借用**空闲 CPU。原文给了两行公式，这是全章最实用的两行数学：

```
container CPU = (all CPUs × container shares) / total busy shares on system
```

> 当只有 A、B 想要 CPU 时（A 10 shares、B 30 shares），A 拿到全机 25%：all CPUs × 10/(10 + 30)。
> 当**所有**容器都忙时：A 拿到 10%：CPUs × 10/100。

于是 A 的实际可用范围是 **10% 到 100%**，取决于别的容器忙不忙。原文一句总结：

> The share allocation provides a minimum guarantee of CPU usage.

- **bandwidth**（CFS 带宽控制）：给上限，防止无限 burst。原文举的例子：A 的 shares 给 10%，bandwidth 设在 20%，那 A 的实际范围就是 **10%–20%**（Figure 11.12）。

### 内存、swap、文件系统、磁盘、网络

`memory` cgroup 的四个设置（原文表 11.4）：`memory.limit_in_bytes`（超了触发换页或 OOM killer）、`memory.soft_limit_in_bytes`（尽力回收，把容器推向软限）、`memory.kmem.limit_in_bytes`、`memory.kmem.tcp.limit_in_bytes`；还有 `memory.pressure_level`（低内存通知，走 eventfd）和 `memory.oom_control`。swap 的设置是 `memory.memsw.limit_in_bytes`（内存 + swap）。文件页缓存**自动**记在容器的 memory cgroup 里，不用额外设置。

`blkio` cgroup（表 11.5）：`blkio.weight`（份额，配 BFQ 调度器）、`blkio.weight_device`、`blkio.throttle.read_bps_device` / `write_bps_device` / `read_iops_device` / `write_iops_device`（字节与 IOPS 的限速）。

网络：`net_prio`（等价 `SO_PRIORITY`）+ `net_cls`（打 class id）+ **qdisc**（50 多种，各有策略和可调项；K8s 的 `kubernetes.io/ingress-bandwidth` 就是用 **token bucket filter（tbf）** 实现的）+ **BPF**（BPF 程序挂到 cgroup 上做自定义控制与防火墙，Cilium 就是 XDP + cgroup + tc 三层 BPF 的组合）。

---

## 7. §11.3.4 Observability：容器里那些「说谎的表」

### §11.3.4.1 Traditional Tools：一张必须背下来的表

原文表 11.6 的核心事实（加粗的是「出乎意料」的项）：

| 工具 | 从宿主跑 | 从容器里跑 |
|---|---|---|
| `top` | 汇总行显示**宿主**，进程表显示**所有**进程 | 汇总行**混杂**（有些来自宿主、有些来自容器），进程表只显示容器进程 |
| `ps` | 全部进程 | 容器进程 |
| `uptime` / `mpstat` / `vmstat` / `free` / `iostat` | 宿主 | **宿主**（不是容器） |
| `pidstat` | 全部进程 | 容器进程 |
| `perf` | 能 profile 一切 | **要么跑不起来，要么能跑但会 profile 到别的租户** |
| `tcpdump` | 所有网卡 | 只有容器网卡 |
| `dmesg` | 内核日志 | **跑不起来** |

原文给了个让人血脉贲张的例子——在一个**完全空闲**的容器里跑 `iostat -sxz 1`：

```
avg-cpu:  %user   %nice %system %iowait  %steal   %idle
          57.29    0.00    8.54   33.17    0.00    1.01

Device             tps      kB/s    rqm/s   await aqu-sz  areq-sz  %util
nvme0n1        2578.00  12436.00   331.00    0.33   0.00     4.82 100.00
```

> This output shows both a CPU and disk workload, and yet this container is completely idle.

原文的一句总结，是这一节的题眼：

> Many monitoring tools written for stand-alone systems have yet to develop support for OS virtualization (containers), and are blind to cgroup and other software limits.

它们会**falsely report that systems have headroom**（明明撞了软件限额，却报「有余量」），也会**把别的租户的用量算成你的**。

### §11.3.4.2 Host：从宿主看容器

难点在于**内核里没有容器 ID**：

> On Linux, container observability from both the host and guests is made still more complex and time-consuming by the fact that there is currently no container ID in the kernel

（容器 ID 是用户态软件造出来的。）Solaris 的 zones 有内核级 zone ID，所以 `ps` 能显示——Linux 没有。

四条替代路径：

1. **容器平台自带的工具**：`kubectl top nodes` / `kubectl top pods`、`docker stats`；
2. **cgroup 统计**：`/sys/fs/cgroup/.../cpu.stat` 里的 `nr_periods` / `nr_throttled` / `throttled_time`——原文的解读是「507 个周期里被限流 74 次，共 3.8 秒」。`systemd-cgtop` 读同一批数据；
3. **namespace 映射**（`nsenter`）：容器里的 PID 和宿主的 PID **不一样**。`/proc/PID/status` 里的 `NSpid:` 给出对照（`NSpid: 4915 753`），反过来查要扫一遍：

```
host# awk '$1 == "NSpid:" && $3 == 753 { print $2 }' /proc/*/status
```

   `nsenter -t 4915 -m -p top` 可以直接在客人的 mount + process namespace 里跑宿主上的 `top`。
4. **BPF tracing**：bpftrace 能从内核里读 cgroup / namespace 信息。原文给了 `forks.bt` 的改法（走 `curtask->nsproxy->uts_ns->name.nodename` 拿容器名做 key）。**重要限制**：这依赖 syscall 在**任务上下文**里发生；异步事件（比如磁盘 I/O 完成中断）里 `curtask` 拿不到正确的 nodename。

### §11.3.4.3 Guest（容器里）

容器里能看的：自己的进程、文件系统、网卡、TCP 会话。**例外**是系统级统计（CPU、磁盘）——**它们显示的是宿主**。

作者还给了容器内查 cgroup blkio 计数的办法（`blkio.throttle.io_serviced`，隔 10 秒读两次看有没有涨）。

### §11.3.4.4 Strategy：先查软件限额，再查物理资源

> Since these limits, where present, are encountered long before the physical limits, they are more likely to be in effect and should be checked first.

这就是本章最实用的一条操作原则。原文逐资源给了检查清单：

- **CPU**：用 Figure 11.13 的流程图（`throttled_time` / `nonvoluntary_ctxt_switches` / 宿主 `%idle` / cpuset 是否 100% 忙 / 其他租户是否空闲，五个统计量）；
- **内存**：当前用量 vs memory cgroup 限额；
- **文件系统容量**：`df`；
- **磁盘 I/O**：`blkio` cgroup 的 throttle 配置 + `blkio.throttle.io_serviced` / `io_service_bytes`——**如果它们的增速和 throttle 限速一致，就是被限速了**；
- **网络 I/O**：当前吞吐 vs 已知带宽上限（可能只有宿主看得到）。

### §11.3.4.4 补充：容器感知该做哪样，追踪工具在容器里怎么办

原文承认：perf / Ftrace / BPF **在容器里现在跑不起来**（`perf_event_open(2)`、`bpf(2)` 的权限，以及 `/proc`、`/sys` 的访问权）。两条出路：

- **需要隔离**：内核过滤（事件和参数按容器过滤），或**宿主 API**（容器请求宿主跑一个过滤过的工具，把结果返回）；
- **不需要隔离**：把 syscall、`/proc`、`/sys` 的权限放开，让工具跑，再让工具自己学会容器感知。

原文的收尾很清醒：

> Making tools and statistics container-aware has been a slow process in Linux and other kernels, and will likely take many years before they are all done.

**而且它伤的是「会用工具的人」**——监控产品的 agent 早就半容器感知了。

---

## 8. §11.4 Lightweight Virtualization：两头都要

一句话定位：

> Lightweight hardware virtualization has been designed to be the best of both worlds: The security of hardware virtualization with the efficiency and fast boot time of containers.

关键实现差异在**代码量**上——原文给的对照极其直观：

> quick emulator (QEMU) is a full-machine hardware hypervisor used for KVM, and has over 1.4 million lines of code (QEMU version 4.2). Amazon Firecracker is a lightweight hypervisor and has only 50 thousand lines of code

因为不需要支持视频、音频、BIOS、PCI 总线这些**桌面虚拟机**才需要的东西，并且**可以假定现代处理器的虚拟化特性一定存在**。

原文列了四个项目：**Intel Clear Containers**（2015，启动 < 45 ms）、**Kata Containers**（2017，OpenStack 基金会治理，口号「The speed of containers, the security of VMs」）、**Google gVisor**（2018，Go 写的用户态内核，路线更接近容器）、**Amazon Firecracker**（2019，KVM + 新的轻量 VMM，不用 QEMU，启动约 100 ms）。

内存开销：Clear Containers 2.0 报 48–50 MB / 容器，Firecracker 报**不到 5 MB**。

作者主张用 **MicroVM** 这个词而不是 containers，因为「containers」这个词已经绑定在 OS 虚拟化上了。

**可观测性**：和 KVM 一样——宿主能看所有物理资源，客人在宿主上就是一个进程（原文的 `top` 输出里整台 VM 就是一行 `firecracker`，占 200% CPU，但**看不出是哪些客人进程在吃**）；客人里因为有自己的内核，**所有内核 tracing 工具都能用**。

---

## 9. §11.5 Other Types / §11.6 Comparisons

### §11.5 三种「连操作系统都不给你」的形态

- **FaaS**：「serverless」——没有服务器可管。代价：**冷启动**可能很久，而且**没法跑传统命令行观测工具**，性能分析通常只能靠应用自己埋的时间戳。
- **SaaS**：性能分析完全限于运营方；用户只能做客户端计时。
- **Unikernels**：把应用和最小内核片段编成一个二进制，直接跑在 hypervisor 上。好处是少指令、少 cache 污染、少攻击面；坏处是**没有 OS 可以登录**，`/proc` 之类的内核统计可能根本不存在。原文提了个有意思的变通：多数实现允许 Unikernel 作为一个普通进程运行，这给了分析路径。

### §11.6 表 11.7 的要点

原文的表 11.7（Comparing virtualization technology performance attributes）横轴是三种技术，纵轴是二十来项性能属性。挑几条最有判断价值的：

| 属性 | 硬件虚拟化 | OS 虚拟化（容器） | 轻量虚拟化 |
|---|---|---|---|
| CPU 分配 | 固定到 vCPU | **灵活（shares + bandwidth）** | 固定到 vCPU |
| 内存访问开销 | 有（EPT/NPT 或影子页表） | **无** | 有 |
| 宿主侧可观测性 | 中 | **高（什么都能看）** | 中 |
| 客人侧可观测性 | **高（内核全通）** | 中（只能用户态 + 若干内核计数器，且混着宿主的统计） | **高** |
| 不同 guest OS | 可以 | 通常不行 | 可以 |

表下面那两段是全章的结论：

> Virtualization technologies are often compared using micro-benchmarking to see which performs the best. Unfortunately, this overlooks the importance of being able to observe the system, which can lead to the largest performance gains of all.

> Observability often makes it possible to identify and eliminate unnecessary work, making possible performance wins far greater than minor hypervisor differences.

**微基准测出来的「谁快 5%」远不如「能不能看见自己在干什么」。**

然后是本章最反直觉的结论：

> For cloud operators, the highest observability option is containers, because from the host they can see all processes and their interaction. For end users, it is virtual machines, since those give users kernel access to run all kernel-based performance tools

**容器对运营方最友好，虚拟机对最终用户最友好。** 这不是技术优劣，是**谁站在哪个位置上看**的差别。（作者还提了个组合选项：容器 + 内核访问权限 = 两边都全看见，但那只在客户自己运营容器宿主时才成立，因为容器之间没有安全隔离。）

---

## 10. 实战锚点：otf-server 上云会踩到什么

**先说清楚前提**：`otf-server` 跑在**本地 / 自建**环境——`dev.sh` 的内环（宿主原生构建 / ctest）是主力，外环是容器里从零构建（`./dev.sh verify`）用来证明「换个环境也成立」。仓库里**没有**跑在 AWS / GCP / K8s 上的配置。所以本章的绝大多数内容对本仓库是**「未来风险」而不是「今天的问题」**。

但正因为是未来风险，现在就该知道**哪些地方会先坏**。

### 10.1 本仓库已经「容器感知」的那一半

`src/net/metrics.h` 的 `Metrics` 是**进程内**的计数器（原子量，`on_connect` / `on_frame_sent` / `on_epoll_ctl` 之类），`MetricsServer`（`src/net/metrics_server.cpp`）把它渲染成 Prometheus 文本格式（`otf_connections` / `otf_frames_sent` / `otf_epoll_ctl_add` …）。

这意味着：**仓库的指标天生是「我这个进程」的**，不是「这台机器」的。放到容器里，它**不会**像 `iostat` 那样把宿主的数据算进来。这正好落在原文 §11.3.4.4 说的两种「container awareness」里的**场景 A（容器是隔离的服务器）**。

**这个选择在上云后是纯赚的。** 因为云上最容易犯的错就是「在容器里跑 `iostat`，看到宿主 100% util，然后以为自己很忙」——本仓库的指标结构性地绕开了这个坑。

### 10.2 本仓库「看不见」的那一半

反过来，仓库**看不到宿主的任何东西**：

- **没有 `st`（stolen）**：自建机器上无所谓，上云后这是第一号指标。`otf_tick_drift_max_ms`（逻辑帧落后墙钟的峰值）会因为邻居抢 CPU 而变大，但**你分不清**是自己逻辑跑慢了还是被抢了。原文 §11.2.4.2 的 `vmstat` 里那列 `st`，本仓库**没有对应物**。
- **没有 guest exit 观测**：仓库是**裸进程**，没有 hypervisor，所以 `kvm:kvm_exit` 那条路**用不上**（只在容器化 + KVM 的宿主上，作为**宿主运维**的视角才有）。
- **没有冷启动测量**：`MetricsServer` 起来就能应答，但它**测不出**「进程启动到能服务第一个请求」花了多久。上云后这叫 **cold start**，是 FaaS 之外的另一种「启动慢」（原文 §11.5 讲 FaaS 的启动时间，原文 §11.4 讲 Firecracker 的 ~100 ms）。

### 10.3 上云时**会先咬你**的三件事

按「先撞上」的顺序：

1. **存储变成网络存储。** 原文 §11.1.4 说网络存储「performance can be much less predictable than with local disks」。本仓库现在是**零磁盘 IO** 的纯内存服务器（游戏状态在内存里），所以这一条暂时不痛——但**日志落盘、崩溃转储、将来的持久化房间状态**都会撞上。原文给的解法是内存缓存 + 买 IOPS；对本仓库就是：**别把热路径的持久化设计成网络盘上的同步写**。
2. **配置项变成「不可改」。** 原文 §11.1.1 说实例类型是旋钮，但**仓库的参数不是旋钮**：`src/config/game_config.h` 是**编译期 constexpr**，`kTickRateHz` / `kSnapshotRateHz` / `kMaxFd` / `kSendBufCap` 全是编译进去的。也就是说：**云上想换个实例规格就得重编译重部署**。这是本仓库上云前最该补的一块（把 `game_config.h` 的运行时可变部分抽出来，从配置/环境读）。
3. **自动伸缩遇上性能回归。** 原文 §11.1.3 说 auto scaling 的风险是 overprovisioning——**性能回归在它眼里就是「负载上涨」**。本仓库有 `otf_frames_recv` / `otf_connections` 这些量，真接了 HPA 之后，**一次解码器性能回归会被自动扩容掩盖成「多机扛住了」**。

### 10.4 云上排查时，本仓库的指标该怎么用

把第 2 章的 USE 方法和本章的「先查软件限额，再查物理资源」合起来，本仓库上云后的排查顺序是：

1. **先看 `otf_tick_drift_max_ms`（饱和度）**——这是本仓库版的「排队延迟」。它涨了，说明逻辑帧跟不上墙钟。注意：它的方向是**有符号**的（`metrics.h` 里写明 sim 超前 = 负 / 落后 = 正，两种都导致双端不同步），所以**别只看绝对值**。
2. **看 `otf_epoll_ctl_add_with_out`**——这是一个**不变量**指标（正确实现下**恒为 0**）。它的存在说明仓库早就知道「应用层指标看不见内核实际收到什么」；上云后同理，`otf_epoll_ctl_add` 全绿**不代表**内核真的收到了注册。
3. **用宿主侧的 `vmstat 1` 看 `st` 一列**（原文 §11.2.4.2）——这是**仓库自己的指标给不了、只能从宿主拿**的那一类数字。
4. **别在容器里信 `iostat` / `mpstat` / `vmstat`**（原文表 11.6）。要么从宿主跑，要么用仓库自己的 `otf_*`。

### 10.5 缺口清单（本仓库欠的）

1. **没有 `st` 的等价物**：无法区分「我逻辑慢」和「被邻居抢」。
2. **没有宿主视图**：仓库**故意**只做进程内视角，所以**看不见** noisy neighbor——原文说「people tend to blame what they cannot observe」，这是本仓库**主动选择**的盲区，得知道它存在。
3. **没有 cold-start 计时**：`MetricsServer` 能应答不等于「服务已就绪」。
4. **参数编译期固化**：`game_config.h` 的 constexpr 让「实例类型当旋钮」这件事在仓库这一层不成立。
5. **`MetricsServer` 没有鉴权**：`src/net/metrics_server.cpp` 的文件头写明「不做路由、不做鉴权、不看请求内容 —— 端口本身就是访问控制」。本机自跑没问题；**上云后如果把这个端口暴露成 LoadBalancer，等于把内部指标裸奔到公网**。在 K8s 里正确的做法是 ClusterIP + NetworkPolicy。
6. **零 BPF / ftrace 资产**（第 14、15 章会补这个洞）：上云后如果进容器，`perf` / Ftrace / BPF 在容器里的可用性问题（原文 §11.3.4.4 的 Tracing Tools）会直接卡住排查。

---

## 11. 收尾：本章金句清单

1. **The rise of cloud computing solves some old problems in the field of performance while creating new ones.** —— 云不是性能问题的解药，是换了问题集。
2. **A cloud environment can be created instantly and scaled on demand, without the typical overheads of building and managing an on-premises data center.** —— 云最大的性能红利不是快，是**反馈回路短**。
3. **With so many options and the ease of redeploying instances, the instance type has become like a tunable parameter that can be modified as needed.** —— 实例类型成了旋钮；前提是你的参数**也得是运行时可调的**。
4. **Instead of spending time planning what may be needed, companies can increase the number of server instances they use as needed** —— 容量规划从「预测」变成「反应」——于是「买多少」这个问题被换成了「怎么回缩」。
5. **Auto scaling can solve the need to quickly respond to changes in load, but it also risks overprovisioning** —— 自动伸缩分不清「负载上涨」和「性能回归」，后者会让它加机器、加机器更慢。
6. **For example, another guest on the same host might perform a full database dump during your peak load, interfering with your disk and network I/O.** —— 噪音邻居不是抽象概念，是隔壁有人在跑 mysqldump。
7. **Worse, a neighbor could be evaluating the cloud provider by executing micro-benchmarks that deliberately saturate resources in order to find their limit.** —— 邻居可能正在「压测云厂商」，而你正好在同一条总线上。
8. **The number of steps in the I/O path, both control and data, is critical for performance: the fewer, the better.** —— 虚拟化的 I/O 开销，本质上是**步数**的开销；Nitro 的全部卖点就是少几步。
9. **In general, the guest applications execute directly on the processors, and CPU-bound applications may experience virtually the same performance as a bare-metal system.** —— 硬件虚拟化**不是** CPU 变慢的主因；别一上来就怪它。
10. **The rate of transitions between the guest and hypervisor, as well as the time spent in the hypervisor, can be studied as a metric of CPU overhead.** —— guest exit 是虚拟化 CPU 开销的**计量单位**。
11. **A guest’s CPU usage is typically opaque to the hypervisor, and guest kernel thread priorities cannot typically be seen or respected.** —— 你的进程优先级，hypervisor 看不见——guest 里的「低优先级」是幻觉。
12. **I/O can become invisible to the hypervisor, and not appear in iostat(1) or other tools.** —— pass-through / SR-IOV 让 I/O 对 hypervisor 隐身；看不见的东西，人最爱怪。
13. **Stolen shows CPU time not available to the guest: it may be consumed by other tenants or other hypervisor functions** —— `st` 这一列是云上最有价值的一个数：它一分为二，帮你把责任分给「你」和「邻居」。
14. **A key difference from hardware virtualization technologies is that only one kernel is running.** —— 容器省掉的是内核，赔进去的是**内核级的争抢与不可观测**。
15. **Increased contention for kernel resources (locks, caches, buffers, queues).** —— 容器的头号性能问题不是「容器慢」，是「大家共用一把锁」。
16. **For the guests: reduced performance observability, as the kernel typically cannot be analyzed.** —— 容器最大的隐性成本不在性能，在**你失去了排查性能的能力**。
17. **container CPU = (all CPUs × container shares) / total busy shares on system** —— shares 给的是**下限保证**；上限由 bandwidth 决定。
18. **A problem with shares is that bursting can confuse capacity planning, especially since many monitoring systems do not show bursting statistics (they should).** —— burst 让压测结果好看，让容量规划失真。
19. **To the customer, this can feel like bait and switch.** —— 先给你 100%，等你依赖上，再变回 10%。
20. **Many monitoring tools written for stand-alone systems have yet to develop support for OS virtualization (containers), and are blind to cgroup and other software limits.** —— 一堆工具会**假装**有余量，其实你已经撞了软件限额。
21. **This output shows both a CPU and disk workload, and yet this container is completely idle.** —— 在容器里跑 `iostat` 看到满载？那是**宿主**在忙。
22. **Since these limits, where present, are encountered long before the physical limits, they are more likely to be in effect and should be checked first.** —— **先查软件限额，再查物理资源**——这一条能省掉大半冤枉云的时间。
23. **Making tools and statistics container-aware has been a slow process in Linux and other kernels, and will likely take many years before they are all done.** —— 容器观测能力落后于容器普及速度，这是场**长期欠账**。
24. **Amazon Firecracker is a lightweight hypervisor and has only 50 thousand lines of code** —— 少 28 倍代码，换来启动快、内存省、攻击面小——轻量虚拟化是「两头都要」的工程直觉。
25. **Virtualization technologies are often compared using micro-benchmarking to see which performs the best. Unfortunately, this overlooks the importance of being able to observe the system, which can lead to the largest performance gains of all.** —— 微基准比出来的 5%，远不如「能不能看见自己」。
26. **Observability often makes it possible to identify and eliminate unnecessary work, making possible performance wins far greater than minor hypervisor differences.** —— **看见 → 删掉无用功**，这是性能工程里回报最高的一步。
27. **For cloud operators, the highest observability option is containers, because from the host they can see all processes and their interaction. For end users, it is virtual machines, since those give users kernel access to run all kernel-based performance tools** —— 容器对运营方最友好，虚拟机对最终用户最友好——差别不在技术，在**你站在哪儿看**。
