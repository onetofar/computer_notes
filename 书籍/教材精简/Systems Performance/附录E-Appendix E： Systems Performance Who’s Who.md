---
title: "附录E-Appendix E： Systems Performance Who’s Who（精读）"
category: 性能工程
tags:
  - 性能工程
  - 术语索引
  - 追踪技术
  - BPF
  - 内核机制
difficulty: 入门
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）附录E Systems Performance Who’s Who"
source_section: "精读 §附录 E"
skills:
  - obsidian-markdown
---

# 附录E-Appendix E： Systems Performance Who’s Who

> **小节地图**：附录 E 不是一章正文，是一份**术语人物志**——全书用到的技术，谁创造的，一行一人（L27–L175），
> 开头只有一句定位（L25），末尾是一张书目（`E.1 References`）。
> 因此本篇的立篇单位是 **§附录 E**，不设子节编号；下面五节是**本篇**按技术族群对这份名录做的重排：
> §附录 E 这份清单是什么 / §附录 E 性能方法论 / §附录 E 追踪与可观测 / §附录 E 内核机制与历史 / §附录 E 工具谱系。
> **不立篇**：`E.1 References`（书目表，不承载机制陈述）。
> 底层依赖：方法论见 [[Chapter 2. Methodologies]]；追踪机制见 [[Chapter 14. Ftrace]] 与 [[Chapter 15. BPF]]；
> 工具见 [[Chapter 13. perf]]；内核机制见 [[Chapter 3. Operating Systems]]。
> ⚠️ 本篇**不写实战锚点**——那是讲义层的事。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §附录 E 这份清单是什么 | 附录E-Appendix E： Systems Performance Who’s Who.md L23 | Systems Performance Who’s Who |
| §附录 E 性能方法论 | 附录E-Appendix E： Systems Performance Who’s Who.md L27 | John Allspaw : Capacity planning [Allspaw 08] . |
| §附录 E 性能方法论 | 附录E-Appendix E： Systems Performance Who’s Who.md L77 | Dr. Neil Gunther : Universal Scalability Law, ternary plots for CPU utilization, performance books [Gunther 97]. |
| §附录 E 性能方法论 | 附录E-Appendix E： Systems Performance Who’s Who.md L99 | Mike Loukides : First book on Unix systems performance [Loukides 90] , which either began or encouraged the tradition of resource-based analysis: CPU, memory, disk, network. |
| §附录 E 性能方法论 | 附录E-Appendix E： Systems Performance Who’s Who.md L117 | Cary Millsap : Method R. |
| §附录 E 性能方法论 | 附录E-Appendix E： Systems Performance Who’s Who.md L157 | Martin Thompson : Mechanical sympathy. |
| §附录 E 追踪与可观测 | 附录E-Appendix E： Systems Performance Who’s Who.md L37 | Daniel Borkmann : Co-creator and maintainer of extended BPF. |
| §附录 E 追踪与可观测 | 附录E-Appendix E： Systems Performance Who’s Who.md L43 | Bryan Cantrill : Co-created DTrace; Oracle ZFS Storage Appliance Analytics. |
| §附录 E 追踪与可观测 | 附录E-Appendix E： Systems Performance Who’s Who.md L57 | Mathieu Desnoyers : Linux Trace Toolkit (LTTng), kernel tracepoints, main author of userspace RCU. |
| §附录 E 追踪与可观测 | 附录E-Appendix E： Systems Performance Who’s Who.md L73 | Sasha Goldshtein : BPF tools (argdist(8), trace(8), etc.), BCC contributions. |
| §附录 E 追踪与可观测 | 附录E-Appendix E： Systems Performance Who’s Who.md L97 | John Levon : OProfile. |
| §附录 E 追踪与可观测 | 附录E-Appendix E： Systems Performance Who’s Who.md L141 | Steven Rostedt : Ftrace, KernelShark, real-time Linux, adaptive spinning mutexes, Linux tracing support. |
| §附录 E 追踪与可观测 | 附录E-Appendix E： Systems Performance Who’s Who.md L153 | Alexei Starovoitov : Co-creator and maintainer of extended BPF. |
| §附录 E 内核机制与历史 | 附录E-Appendix E： Systems Performance Who’s Who.md L35 | Jeff Bonwick : Invented kernel slab allocation, co-invented user-level slab allocation, co-invented ZFS, kstat, first developed mpstat. |
| §附录 E 内核机制与历史 | 附录E-Appendix E： Systems Performance Who’s Who.md L87 | Bill Joy : vmstat(1), BSD virtual memory work, TCP/IP performance, FFS. |
| §附录 E 内核机制与历史 | 附录E-Appendix E： Systems Performance Who’s Who.md L119 | Ingo Molnar : O(1) scheduler, completely fair scheduler, voluntary kernel preemption, ftrace, perf, and work on real-time preemption, mutexes, futexes, scheduler profiling, work queues. |
| §附录 E 内核机制与历史 | 附录E-Appendix E： Systems Performance Who’s Who.md L155 | Ken Thompson : Unix, and its original performance features: process priorities, swapping, buffer cache, etc. |
| §附录 E 工具谱系 | 附录E-Appendix E： Systems Performance Who’s Who.md L31 | Jens Axboe : CFQ I/O Scheduler, fio, blktrace, io_uring. |
| §附录 E 工具谱系 | 附录E-Appendix E： Systems Performance Who’s Who.md L71 | Sebastian Godard : sysstat package for Linux, which contains numerous performance tools including iostat(1), mpstat(1), pidstat(1), nfsiostat(1), cifsiostat(1), and an enhanced version of sar(1), sadc(8), sadf(1) (see the metrics in Appendix B ). |
| §附录 E 工具谱系 | 附录E-Appendix E： Systems Performance Who’s Who.md L111 | Arnaldo Carvalho de Melo : Linux perf(1) maintainer. |

> **书里没说的**（本篇的推论，原文没有，别当原文观点）：
> - **「按技术族群分四节」是本篇的切法**：原文附录 E 是一份**平铺的人名录**（L27–L175，每行一人、纯字母序），
>   没有任何分节。本篇把同样的行重排成四张导航表，顺序与原文不同。
> - **「该读哪一章」这一列是本篇加的**：原文只回答「谁做的」，不指路；这一列是本篇逐条回查全书各章原文稿得到的落点。
> - **「术语（提出者）」这个提法是本篇的**：原文格式是「人名 : 他做了什么」，本篇反过来先给术语、再给人名。
> - **「机械同理心在正文没有展开」是本篇的检索结论**，不是原文的说法：原文只在 L157 挂了个名。

## §附录 E 这份清单是什么

**机制**：附录 E 是一份**名录**——按「人 → 他创造了什么」列出全书涉及的技术的作者。原文自己给的定位是：

> It can be useful to know who created the technologies that we use. This is a list of who’s who in the field of systems performance, based on the technologies in this book.

两个要点：

1. 它**基于这本书里的技术**（based on the technologies in this book）——所以它不是性能工程界的完整人名录，只是**这本书能追溯到的那些**。
2. 它是**名单**不是**索引**：L27–L175 按人名字母序排（Allspaw → Zijlstra），不按主题、不按重要性。**想按概念找作者，必须扫一遍**——这正是本篇把它重排成导航表的理由。

**追人的三条官方入口**（原文 L25）：

> If you wish to dig further into the people and history, see the chapter references and the names listed in the Linux source, both the Linux repository history and the MAINTAINERS file in the Linux source code.

即：① 各章末尾的 references；② Linux 源码里的 `MAINTAINERS` 文件（谁负责哪个子系统）；③ Linux 仓库的提交历史。
作者还补了第四个入口：他的 BPF 书 [Gregg 19] 的致谢章节，extended BPF / BCC / bpftrace / kprobes / uprobes 背后的人在那里列得最全。

**怎么用（推演）**：这份清单的实用价值不在「学习」，而在**排障时定位权威**——某个工具的行为反直觉时，去找作者本人的实现和论文，而不是二手博客。下面四张表把它变成「**术语 → 该读哪一章**」的导航表。

## §附录 E 性能方法论

这一族对应**第 2 章 Methodologies** 里那套方法论词汇表的人名出处。

| 术语（提出者） | 原文逐字：这个人做了什么 | 行 | 该读哪一章 |
|---|---|---|---|
| Amdahl 定律（Gene M. Amdahl） | Early work on computer scalability [Amdahl 67] . | L29 | 第 2 章 · [[Chapter 2. Methodologies]] |
| 资源分析法（Mike Loukides） | First book on Unix systems performance [Loukides 90] , which either began or encouraged the tradition of resource-based analysis: CPU, memory, disk, network. | L99 | 第 2 章 · [[Chapter 2. Methodologies]]；第 12 章 · [[Chapter 12. Benchmarking]] |
| 性能工程理论（Raj Jain） | Systems performance theory [Jain 91]. | L83 | 第 2 章 · [[Chapter 2. Methodologies]] |
| Method R（Cary Millsap） | Method R. | L117 | 第 2 章 §2.5.14 · [[Chapter 2. Methodologies]] |
| 通用可扩展定律（Dr. Neil Gunther） | Universal Scalability Law, ternary plots for CPU utilization, performance books [Gunther 97]. | L77 | 第 2 章 §2.6.4 · [[Chapter 2. Methodologies]] |
| 容量规划（John Allspaw） | Capacity planning [Allspaw 08] . | L27 | 第 2 章 §2.7 · [[Chapter 2. Methodologies]] |
| 性能工程手册（Adrian Cockcroft） | Performance books [Cockcroft 95] [Cockcroft 98] , Virtual Adrian (SE Toolkit). | L51 | 第 2 章 · [[Chapter 2. Methodologies]] |
| USE 方法 / 火焰图 / 热力图（Brendan Gregg） | USE method, heat maps (latency, utilization, subsecond-offset), flame graphs, flame scope | L75 | 附录 A · [[附录A-Appendix A： USE Method： Linux]]；第 2 章 · [[Chapter 2. Methodologies]]；第 13 章 · [[Chapter 13. perf]] |
| 机械同理心（Martin Thompson） | Mechanical sympathy. | L157 | 本书正文未展开（见下） |

⚠️ **「该读哪一章」这一列是本篇加的**：附录 E 本身只回答「谁做的」，不指路。

⚠️ **「机械同理心」是全表唯一没有章节落点的一项**：在本书正文（含第 1、2、5 章）检索 "mechanical sympathy" **0 命中**，第 2 章的方法论清单里也没有它。⇒ 这个概念只在附录里挂了个名，**要学得出去找 Loukides 1990 那条线**。

**方法论在本书里是有编号小节的**：Method R 收在 §2.5.14、通用可扩展定律收在 §2.6.4、容量规划单列为 §2.7；而 USE 方法是**唯一被单独开了一整个附录**的（附录 A）——本库已有对应笔记 [[附录A-Appendix A： USE Method： Linux]]。

## §附录 E 追踪与可观测

追踪这条线在本书里占两章：[[Chapter 14. Ftrace]]（内核侧的追踪机制）与 [[Chapter 15. BPF]]（可编程观测），[[Chapter 13. perf]] 是事件采样那条线；而各追踪技术的**原理底座**（kprobes、tracepoints、LTTng）集中在 [[Chapter 4. Observability Tools]]。

| 术语（提出者） | 原文逐字：这个人做了什么 | 行 | 该读哪一章 |
|---|---|---|---|
| DTrace（Bryan Cantrill） | Co-created DTrace; Oracle ZFS Storage Appliance Analytics. | L43 | 第 1 章 · [[Chapter 1. Introduction]]（DTrace 起源叙述）；第 3 章 · [[Chapter 3. Operating Systems]]（DTrace 词条定义） |
| DTrace（Michael Shapiro） | Co-created DTrace. | L145 | 第 3 章 · [[Chapter 3. Operating Systems]]（DTrace 词条定义）；第 1 章 · [[Chapter 1. Introduction]]（DTrace 起源叙述） |
| DProbes / kprobes（Richard J. Moore） | DProbes, kprobes. | L121 | DProbes 本书正文 0 命中（未讨论）；kprobes 见第 4 章 · [[Chapter 4. Observability Tools]]（36 处）、第 14 章 · [[Chapter 14. Ftrace]]（30 处） |
| LTTng / 内核 tracepoint（Mathieu Desnoyers） | Linux Trace Toolkit (LTTng), kernel tracepoints, main author of userspace RCU. | L57 | LTTng：第 4 章 · [[Chapter 4. Observability Tools]]（全书唯一定义处）；内核 tracepoint：第 14 章 · [[Chapter 14. Ftrace]]、第 15 章 · [[Chapter 15. BPF]] |
| Ftrace（Steven Rostedt） | Ftrace, KernelShark, real-time Linux, adaptive spinning mutexes, Linux tracing support. | L141 | 第 14 章 · [[Chapter 14. Ftrace]] |
| Ftrace 直方图触发器（Tom Zanussi） | Ftrace hist triggers. | L173 | 第 14 章 · [[Chapter 14. Ftrace]] |
| eBPF（Alexei Starovoitov） | Co-creator and maintainer of extended BPF. | L153 | 第 15 章 · [[Chapter 15. BPF]] |
| eBPF（Daniel Borkmann） | Co-creator and maintainer of extended BPF. | L37 | 第 15 章 · [[Chapter 15. BPF]] |
| BTF（Yonghong Song） | BTF, and extended BPF and BCC work. | L151 | 第 15 章 · [[Chapter 15. BPF]] |
| BCC（Brenden Blanco） | Brenden Blanco : BCC. | L33 | 第 15 章 · [[Chapter 15. BPF]] |
| bpftrace（Alastair Robertson） | Created bpftrace. | L139 | 第 15 章 · [[Chapter 15. BPF]] |
| USDT / libstapsdt（Mary Marchini） | libstapsdt: dynamic USDT for various languages. | L103 | 第 15 章 · [[Chapter 15. BPF]]；第 13 章 · [[Chapter 13. perf]] |
| SystemTap（Frank Ch. Eigler） | Lead developer for SystemTap. | L59 | 第 15 章 · [[Chapter 15. BPF]] |
| LTT（Karim Yaghmour） | LTT, push for tracing in Linux. | L169 | 本书未讨论 LTT（16 章 + 4 附录 + 索引检索 0 命中；索引里的 `LTTng tool` 条目指 LTTng 而非 LTT）；同类 LTTng 见第 4 章 · [[Chapter 4. Observability Tools]] |

⚠️ **L153 与 L37 只有描述部分逐字相同，人名不同**——L37 是 `Daniel Borkmann : Co-creator and maintainer of extended BPF.`，L153 是 `Alexei Starovoitov : Co-creator and maintainer of extended BPF.`：**后半句一模一样，前面的人名不一样**。eBPF 是**共同创建 + 共同维护**，原文给两人各写了一行**描述相同**的句子；DTrace 同理，L43 与 L145 的描述都含 `Co-created DTrace`，但措辞与人名都不同（Cantrill / Shapiro）。⇒ 这份名录**刻意把「共同署名」拆成多行**，不是重复录入。

**三代追踪技术的分工（推演）**：Ftrace 是**内核里静态插好的插桩点**，eBPF 是**可动态加载进内核的程序**，bpftrace 建在 eBPF 之上。本书把三代分在第 14、15 两章讲，章节顺序就是演进顺序。

## §附录 E 内核机制与历史

| 术语（提出者） | 原文逐字：这个人做了什么 | 行 | 该读哪一章 |
|---|---|---|---|
| Unix 的原始性能特性（Ken Thompson） | Unix, and its original performance features: process priorities, swapping, buffer cache, etc. | L155 | 第 3 章 · [[Chapter 3. Operating Systems]]；第 8 章 · [[Chapter 8. File Systems]] |
| 同上（Dennis Ritchie） | Unix, and its original performance features: process priorities, swapping, buffer cache, etc. | L137 | 第 3 章 · [[Chapter 3. Operating Systems]] |
| vmstat(1) / FFS / TCP-IP（Bill Joy） | vmstat(1), BSD virtual memory work, TCP/IP performance, FFS. | L87 | 第 3 章 · [[Chapter 3. Operating Systems]]；第 8 章 · [[Chapter 8. File Systems]] |
| FFS（Marshall Kirk McKusick） | FFS, work on BSD. | L109 | 第 8 章 · [[Chapter 8. File Systems]] |
| ext2 / ext3（Rémy Card） | Rémy Card : Primary developer for the ext2 and ext3 file systems. | L45 | 第 8 章 · [[Chapter 8. File Systems]] |
| slab 分配器 / ZFS / kstat / mpstat（Jeff Bonwick） | Invented kernel slab allocation, co-invented user-level slab allocation, co-invented ZFS, kstat, first developed mpstat. | L35 | 第 7 章 · [[Chapter 7. Memory]]；第 8 章 · [[Chapter 8. File Systems]] |
| SLUB 分配器（Christoph Lameter） | Christoph Lameter : SLUB allocator. | L91 | 第 7 章 · [[Chapter 7. Memory]] |
| 调度器 / 抢占 / ftrace / perf（Ingo Molnar） | O(1) scheduler, completely fair scheduler, voluntary kernel preemption, ftrace, perf, and work on real-time preemption, mutexes, futexes, scheduler profiling, work queues. | L119 | 第 3 章 · [[Chapter 3. Operating Systems]]；第 6 章 · [[Chapter 6. CPUs]] |
| futex（Rusty Russell） | Rusty Russell : Original futexes, various Linux kernel work. | L143 | 第 3 章 · [[Chapter 3. Operating Systems]] |
| 内存 cgroup / 延迟统计（Balbir Singh） | Linux memory resource controller, delay accounting, taskstats, cgroupstats, CPU accounting. | L149 | 第 3 章 · [[Chapter 3. Operating Systems]] |
| hugetlbfs（Nadia Yvette Chambers） | Linux hugetlbfs. | L47 | 第 7 章 · [[Chapter 7. Memory]] |
| fadvise / 预读（Andrew Morton） | Andrew Morton : fadvise, read-ahead. | L123 | 第 8 章 · [[Chapter 8. File Systems]]；第 9 章 · [[Chapter 9. Disks]] |
| Linux 内核（Linus Torvalds） | The Linux kernel and numerous core components necessary for systems performance, Linux I/O scheduler, Git. | L159 | 第 3 章 · [[Chapter 3. Operating Systems]] |
| Linux 网络协议栈（Alan Cox） | Linux network stack performance. | L55 | 第 10 章 · [[Chapter 10. Network]] |
| 自旋锁 / hardirq 回调框架（Peter Zijlstra） | Adaptive spinning mutex implementation, hardirq callbacks framework, other Linux performance work. | L175 | 第 14 章 · [[Chapter 14. Ftrace]] |

⚠️ **L155 与 L137 只有描述部分逐字相同，人名不同**——L155 是 `Ken Thompson : Unix, and its original performance features: process priorities, swapping, buffer cache, etc.`，L137 是 `Dennis Ritchie : Unix, and its original performance features: process priorities, swapping, buffer cache, etc.`：**后半句一模一样，前面的人名不一样**。Unix 的「进程优先级、换出、buffer cache」这套原始性能机制，附录同时挂在 Thompson 和 Ritchie 名下，因为那本来就是两人共同的设计。⇒ **「同一段描述出现两行」在本附录里是署名规则，不是录入错误**。

⚠️ **L35 是全表最「重」的一行**：Jeff Bonwick 一行里横跨**内存（slab）、文件系统（ZFS）、统计（kstat、mpstat）**三个领域。⇒ 这份名录是**按人排**的，一个人的条目天然跨章；按主题找「ZFS 在哪」时不能只翻文件系统那一章。

## §附录 E 工具谱系

| 术语（提出者） | 原文逐字：这个人做了什么 | 行 | 该读哪一章 |
|---|---|---|---|
| sysstat 工具集（Sebastian Godard） | sysstat package for Linux, which contains numerous performance tools including iostat(1), mpstat(1), pidstat(1), nfsiostat(1), cifsiostat(1), and an enhanced version of sar(1), sadc(8), sadf(1) (see the metrics in Appendix B ). | L71 | 附录 B · [[附录B-Appendix B： sar Summary]]；第 4 章 · [[Chapter 4. Observability Tools]]（sar(1)/sysstat 专节）；第 9 章 · [[Chapter 9. Disks]]（iostat(1) 同属该包） |
| top(1)（William LeFebvre） | Wrote the first version of top(1), inspiring many other tools. | L93 | 第 3 章 · [[Chapter 3. Operating Systems]] |
| perf(1)（Arnaldo Carvalho de Melo） | Linux perf(1) maintainer. | L111 | 第 13 章 · [[Chapter 13. perf]] |
| fio / io_uring / blktrace / CFQ（Jens Axboe） | CFQ I/O Scheduler, fio, blktrace, io_uring. | L31 | 第 9 章 · [[Chapter 9. Disks]]；第 8 章 · [[Chapter 8. File Systems]] |
| iotop(1)（Guillaume Chazarain） | iotop(1) for Linux. | L49 | 第 5 章 · [[Chapter 5. Applications]] |
| dstat（Dag Wieers） | Dag Wieers : dstat. | L167 | 本书未讨论 dstat（全书检索 0 真实命中，pidstat 是子串误配）；实际出处＝附录 E L167 |
| nicstat(1)（Tim Cook） | nicstat(1) for Linux, and enhancements. | L53 | 第 10 章 · [[Chapter 10. Network]] |
| latencytop / PowerTOP / irqbalance（Arjan van de Ven） | latencytop, PowerTOP, irqbalance, work on Linux scheduler profiling. | L161 | 第 6 章 · [[Chapter 6. CPUs]] |
| ping(8)（Mike Muuss） | ping(8). | L127 | 第 10 章 · [[Chapter 10. Network]] |
| Bonnie 磁盘微基准（Tim Bray） | Authored the Bonnie disk I/O micro-benchmark, known for XML. | L41 | 第 12 章 · [[Chapter 12. Benchmarking]]；第 8 章 · [[Chapter 8. File Systems]] |
| vfsstat(1M) / ZFS I-O 节流（Bill Pijewski） | Solaris vfsstat(1M), ZFS I/O throttling. | L135 | 第 8 章 · [[Chapter 8. File Systems]] |
| hrtimers（Thomas Gleixner） | Various Linux kernel performance work including hrtimers. | L69 | 本书未讨论 hrtimers 子系统（第 14 章 36 处命中全是 hrtimer_nanosleep / hrtimer_sleeper 等函数与字段名）；实际出处＝附录 E L69 |
| /proc / truss（Roger Faulkner） | Wrote /proc for UNIX System V, thread implementation for Solaris, and the truss(1) system call tracer. | L67 | 第 3 章 · [[Chapter 3. Operating Systems]] |

⚠️ **L71 里 Godard 明写「see the metrics in Appendix B」**——附录 B 就是 sar 指标总表，本库已有对应笔记 [[附录B-Appendix B： sar Summary]]。这是附录 E 内部指向附录 B 的**硬引用**，也是全篇唯一一处直接指向别的附录的引用。

⚠️ **BCC 的作者在原文里写作 "Brenden Blanco"（L33）**，而同一份附录里作者本人是 "Brendan Gregg"（L75）——**两个不同的人，名字却只差一个字母**。翻这份名录时别把两人混成一人。（本篇引文照抄原文，不改拼写。）

## 章末选型判据

| 场景 | 怎么做 | 别怎么做 |
|---|---|---|
| 只知道一个工具名，想知道它出自谁、原理在哪 | 先在本篇导航表里按术语定位行号 → 回原文对应行读定义 → 跳到「该读哪一章」那篇笔记 | 直接搜博客；博客给你结论，不给你出处 |
| 想系统学一套性能方法论 | 走第 2 章：Amdahl → 资源分析法 → Method R（§2.5.14）→ 通用可扩展定律（§2.6.4）→ 容量规划（§2.7）→ USE 方法（附录 A） | 从附录 E 入手——它是名录，不是教程 |
| 选追踪工具 | 先判「要静态插桩点（Ftrace，第 14 章）、可编程观测（eBPF / bpftrace，第 15 章）还是事件采样（perf，第 13 章）」 | 三者混用后再归因——三者的数据口径不同 |
| 判断一个内核机制该找谁问 | 查「内核机制与历史」表定位术语行，再去查 Linux 源码的 `MAINTAINERS` 与提交历史 | 只看博客作者是谁 |
| 看到附录 E 里两个人写着同一段描述（人名不同） | 那是**共同署名**（eBPF 的 Starovoitov / Borkmann、Unix 的 Thompson / Ritchie、DTrace 的 Cantrill / Shapiro），不是重复录入 | 当成录入错误去「修」 |
| 找不到某个概念在本书哪章 | 认了：附录 E 只给「谁做的」；「机械同理心」就是正文不收的一例 | 花时间在第 2 章里找它——检索 0 命中是结论，不是没找 |
