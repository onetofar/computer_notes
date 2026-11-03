---
title: "附录B-Appendix B： sar Summary（精读）"
category: 性能工程
tags:
  - sar
  - 性能观测
  - 系统指标
  - CPU利用率
  - 内存与分页
  - 磁盘与网络
difficulty: 进阶
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）附录B sar Summary"
source_section: "精读 §附录 B"
skills:
  - obsidian-markdown
---

# 附录B-Appendix B： sar Summary

> **小节地图**：本篇是**一张速查表**，不是理论章。原文只有一级标题（`# Appendix B: sar Summary`），
> 正文是一张三列表——**Option / Metrics / Description**，共 20 行，把 `sar(1)` 的选项与它打印的指标一一对上。
> **源章形态（决定锚点怎么挑）**：本章原文是**一张扁平对照表**，整章**没有可挑的「机制句」**——
> 正文就是 Option / Metrics / Description 三列的机械转写。所以锚点表不按「机制句」挑，
> 而是**按资源域各取一条代表行**（共 12 条）：取原文 Description 列里
> 「这条选项回答什么问题」的那一格（如 `Memory utilization`、`Disk statistics`、`TCP errors`）。
> **字段级明细（Metrics 列整行）一律不进锚点表**，它们完整保留在
> §附录 B 三列读法与选项索引 的「全部 20 行索引」表与各节的指标表里；
> 本章仅有的两句散文（本章是什么、有什么前提）留在 §附录 B 定位与边界 的正文引用里，同样逐字。
> 本篇按**资源域**重组为十一节：§附录 B 定位与边界、§附录 B 三列读法与选项索引、
> §附录 B CPU 利用率与频率、§附录 B CPU 调度与上下文切换、§附录 B 内存占用、
> §附录 B 分页与大页、§附录 B 交换区、§附录 B 磁盘 I/O、§附录 B 网络接口、
> §附录 B 网络协议栈、§附录 B 内核表。
> **不立篇**：无（原文没有子节，也没有独立的 References 段）。
> 底层依赖：USE 方法的框架见 [[附录A-Appendix A： USE Method： Linux]]，方法论见
> [[Chapter 2. Methodologies]]；各资源的机制分别在 [[Chapter 6. CPUs]]、[[Chapter 7. Memory]]、
> [[Chapter 8. File Systems]]、[[Chapter 9. Disks]]、[[Chapter 10. Network]]；
> 观测工具链见 [[Chapter 13. perf]] 与 [[Chapter 14. Ftrace]]。
> ⚠️ **第 4 章 Observability Tools 与第 15 章在本库尚无同层笔记**，故只作文字提及，不造链接。
> ⚠️ **本篇的性质**：原文**只给「哪个指标挂在哪个选项下」，不给任何阈值**。
> 所以下面每张表里的「异常的样子」「往哪查」两列**全部是本篇的推演**，不是原文观点。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。
**源章是扁平对照表，没有机制句可挑**：原文整章就是 Option / Metrics / Description 三列的转写，
所以锚点表**只收各资源域各一条的代表行**——取 Description 列里「这条选项回答什么问题」的那一格。
本章仅有的两句散文（本章是什么、有什么前提）留在 §附录 B 定位与边界 的正文引用里，同样逐字。
**字段级明细（Metrics 列整行）不留在锚点表**：
它们完整保留在下方「全部 20 行索引」表与各节的「指标 → 怎么读 → 什么值算异常 → 往哪查」表里。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §附录 B CPU 利用率与频率 | 附录B-Appendix B： sar Summary.md L45 | CPU utilization |
| §附录 B CPU 调度与上下文切换 | 附录B-Appendix B： sar Summary.md L63 | CPU run-queue size |
| §附录 B CPU 调度与上下文切换 | 附录B-Appendix B： sar Summary.md L69 | CPU scheduler events |
| §附录 B 内存占用 | 附录B-Appendix B： sar Summary.md L87 | Memory utilization |
| §附录 B 分页与大页 | 附录B-Appendix B： sar Summary.md L75 | Paging statistics |
| §附录 B 分页与大页 | 附录B-Appendix B： sar Summary.md L81 | Huge pages |
| §附录 B 交换区 | 附录B-Appendix B： sar Summary.md L99 | Swapping statistics |
| §附录 B 磁盘 I/O | 附录B-Appendix B： sar Summary.md L111 | Disk statistics |
| §附录 B 网络接口 | 附录B-Appendix B： sar Summary.md L117 | Network interface statistics |
| §附录 B 网络接口 | 附录B-Appendix B： sar Summary.md L123 | Network interface errors |
| §附录 B 网络协议栈 | 附录B-Appendix B： sar Summary.md L147 | TCP errors |
| §附录 B 内核表 | 附录B-Appendix B： sar Summary.md L105 | Kernel tables |

> **书里没说的**（本篇的推演，原文没有，别当原文观点）：
> - **原文是一张「选项 → 指标」对照表，不含任何阈值、判据或处置建议**。本篇各表里
>   「异常的样子」「往哪查」两列是本篇按 USE 方法的框架补的（框架见
>   [[附录A-Appendix A： USE Method： Linux]]），**不是原文陈述**。
> - **「指标清单按资源域重排」是本篇的重组**：原文的 20 行是按 sar 选项排的，
>   本篇按 CPU / 内存 / 磁盘 / 网络归堆，指标清单本身逐字照抄原文。
> - **「单位后缀（`/s`、`kb`、`kB`、`%`）决定读数口径」是本篇的补充**：
>   原文只给字段名，没有解释速率与存量的区别。
> - **「作者加粗了哪些关键指标」在本文本里无法核实**：原文说了加粗（见 §附录 B 定位与边界 引的那句），
>   但 epub 抽取后加粗标记丢失，本篇**不猜**哪些指标被加粗。
> - **原文逐核 CPU 那一行的选项写作 `-u-P ALL`、频率那一行写作 `-m CPU-P ALL`**，
>   两个参数之间没有空格。实际命令行需要拆成 `sar -u -P ALL` 与 `sar -m CPU -P ALL`（推演）。
> - **本篇未收录的采集侧内容**（如 sysstat 的安装、`sa1`/`sadc` 定时落盘、`sar -f` 回放历史）
>   原文一概没有，本篇也不补——那是部署问题，不是本附录的职责。

## §附录 B 定位与边界

**机制**：这份附录是 **`sar(1)` 的选项—指标对照表**，用途是「帮你回忆哪个指标挂在哪个选项下」。

> This is a summary of options and metrics from the system activity reporter, sar(1). You can use this to jog your memory about which metrics are under which options. See the man page for the full list.

注意最后半句：**完整列表看 man page**。本表是**记忆辅助**，不是权威定义。

原文也交代了它在全书里的位置：

> sar(1) is introduced in Chapter 4 , Observability Tools , Section 4.4 , and selected options are summarized in later chapters ( 6 , 7 , 8 , 9 , 10 ).

即：`sar(1)` 本身在第 4 章（Observability Tools）介绍，第 6/7/8/9/10 章（CPU、内存、文件系统、
磁盘、网络）各自挑了相关选项展开。本附录是那五章的**横向索引**。

**两条使用前提**（原文明说）：

> I have highlighted in bold the key metrics that I look for.

作者把「自己会看的关键指标」加了粗。**本篇提示**：加粗标记在抽取时已丢失，无法确认是哪些指标（见上面「书里没说的」）。

> Some sar(1) options may require kernel features enabled (e.g., huge pages), and some metrics were added in later versions of sar(1) (version 12.0.6 is shown here).

**两个版本前提**：① 有些选项**要内核特性支持**（大页是典型）；② 有些指标是后来版本才加的，本表对应 **sar 12.0.6**。

**容易错在哪**：拿本表当「sar 的字段字典」。它是**选项 → 指标**的索引，不是字段语义表；
「这个字段到底怎么算的」只能查 man page 或相应章节。

**判据**：查「某个指标该用哪个 sar 选项打印」→ 用本表；查「这个指标怎么算的」→ 查 man page。

## §附录 B 三列读法与选项索引

原文的表头就是三列：**Option / Metrics / Description**。

- **Option**：`sar` 的选项串（有的带参数，如 `-n DEV`、`-u ALL`）。
- **Metrics**：这个选项会打印哪些指标——**逐字字段名**，带单位后缀。
- **Description**：这个选项组的用途，一句话。

**单位后缀是读数的前提**（推演）：`pswpout/s` 是速率、`kbswpused` 是存量，
混着看会得出相反的结论（换出速率高但 swap 占用低 = 刚换出就换回，和「swap 用满」完全不是一回事）。

**两类修饰符**（推演）：`ALL` 表示「扩展字段集或全部设备」，`P`（per-CPU）表示「逐 CPU 拆开」。

### 全部 20 行索引（按资源域重排，指标清单逐字照抄原文）

| 选项 | 指标（原文列） | 这组指标回答什么问题 | 本篇哪一节 |
|---|---|---|---|
| `sar -u` | %user %nice %system %iowait %steal %idle | CPU 被谁占住了 | §附录 B CPU 利用率与频率 |
| `sar -u ALL` | ... %irq %soft %guest %gnice | 多出来的 irq/soft/guest 是谁 | §附录 B CPU 利用率与频率 |
| `sar -u -P ALL` | （同 `-u` 的六项，逐 CPU 列出） | 是不是有单核热点 | §附录 B CPU 利用率与频率 |
| `sar -m CPU -P ALL` | MHz | 每核实际跑在多少 MHz | §附录 B CPU 利用率与频率 |
| `sar -q` | runq-sz plist-sz ldavg-1 ldavg-5 ldavg-15 blocked | 有多少任务在排队等 CPU | §附录 B CPU 调度与上下文切换 |
| `sar -w` | proc/s cswch/s | 进程创建与上下文切换有多频繁 | §附录 B CPU 调度与上下文切换 |
| `sar -r` | kbmemfree kbavail kbmemused %memused kbbuffers kbcached kbcommit %commit kbactive kbinact kbdirty | 内存用在哪、还剩多少可用 | §附录 B 内存占用 |
| `sar -B` | pgpgin/s pgpgout/s fault/s majflt/s pgfree/s pgscank/s pgscand/s pgsteal/s %vmeff | 缺页与内存回收压力 | §附录 B 分页与大页 |
| `sar -H` | kbhugfree kbhugused %hugused | 大页占了多少 | §附录 B 分页与大页 |
| `sar -S` | kbswpfree kbswpused %swpused kbswpcad %swpcad | 交换区占用了多少 | §附录 B 交换区 |
| `sar -W` | pswpin/s pswpout/s | 是否正在换入 / 换出 | §附录 B 交换区 |
| `sar -d` | tps rkB/s wkB/s areq-sz aqu-sz await svctm %util | 磁盘忙不忙、请求有没有排队 | §附录 B 磁盘 I/O |
| `sar -n DEV` | rxpck/s txpck/s rxkB/s txkB/s rxcmp/s txcmp/s rxmcst/s %ifutil | 网卡吞吐与利用率 | §附录 B 网络接口 |
| `sar -n EDEV` | rxerr/s txerr/s coll/s rxdrop/s txdrop/s txcarr/s rxfram/s rxfifo/s txfifo/s | 网卡丢包与错误在哪一层 | §附录 B 网络接口 |
| `sar -n IP` | irec/s fwddgm/s idel/s orq/s asmrq/s asmok/s fragok/s fragcrt/s | IP 层收了多少、碎了多少 | §附录 B 网络协议栈 |
| `sar -n EIP` | ihdrerr/s iadrerr/s iukwnpr/s idisc/s odisc/s onort/s asmf/s fragf/s | IP 层的错在哪 | §附录 B 网络协议栈 |
| `sar -n TCP` | active/s passive/s iseg/s oseg/s | TCP 连接与数据段的形态 | §附录 B 网络协议栈 |
| `sar -n ETCP` | atmptf/s estres/s retrans/s isegerr/s orsts/s | TCP 层的失败与重传 | §附录 B 网络协议栈 |
| `sar -n SOCK` | totsck tcpsck udpsck rawsck ip-frag tcp-tw | socket 存量与 TIME_WAIT | §附录 B 网络协议栈 |
| `sar -v` | dentunusd file-nr inode-nr pty-nr | 内核表项用到什么程度 | §附录 B 内核表 |

## §附录 B CPU 利用率与频率

**机制**：`-u` 给 CPU 利用率分项，`-u ALL` 追加 irq / soft / guest / gnice，
`-u -P ALL` 把同样的分项拆到每个 CPU，`-m CPU -P ALL` 给每核的实际频率。

> %user %nice %system %iowait %steal %idle

> ... %irq %soft %guest %gnice

`-u` 给的是这一组六项：

```bash
sar -u
sar -u ALL
sar -u -P ALL
sar -m CPU -P ALL
```

**「异常的样子」「往哪查」两列为本篇推演**（原文不含阈值）：

| 指标 | 它测什么 | 异常的样子（推演） | 往哪查 |
|---|---|---|---|
| `%user` | 用户态占用的 CPU 比例 | 持续贴近 100% ⇒ 真 CPU 饱和 | `sar -q` 的 `runq-sz`、进程级定位 |
| `%nice` | 被降过优先级（nice）的用户态时间 | 明显高于 `%user` ⇒ 有一批任务被有意让路 | 任务优先级策略，不是容量问题 |
| `%system` | 内核态时间 | 高 ⇒ 系统调用 / 中断 / 上下文切换多 | `sar -w` 的 `cswch/s`、`sar -q` |
| `%iowait` | CPU 空闲但有 I/O 在飞的那部分时间 | 持续 > 0 ⇒ **不是 CPU 瓶颈，是 I/O 瓶颈** | `sar -d`、`sar -B` |
| `%steal` | 被 hypervisor 抢走的时间 | > 0 ⇒ 虚拟化环境被宿主超卖 | 宿主负载（云上见 [[Chapter 11. Cloud Computing]]） |
| `%idle` | 真正空闲 | 持续贴近 0 ⇒ CPU 用满 | `sar -q` 的 `runq-sz` |
| `%irq` / `%soft` | 硬件中断 / 软中断占用 | `%irq` 高 ⇒ 设备驱动或硬件中断风暴 | `sar -d`、`sar -B` |
| `%guest` / `%gnice` | 客户机 OS 占用（这台是宿主机） | > 0 ⇒ 本机承载虚拟机 | 宿主机视角的另一次统计 |
| `MHz`（`-m CPU -P ALL`） | 每核实际工作频率 | 满载时频率反而下降 ⇒ 频率调节 / 散热 / 降频 | `sar -m` 逐核对比与温度、功耗策略 |

**两条容易错的读法**（推演）：

1. **`%iowait` 不是「CPU 花在等 I/O 上的时间」**，它是「CPU 空闲且有 I/O 请求在飞」的时间。
   一台磁盘很忙但 CPU 跑满的机器，`%iowait` 可以是 0。所以它**只能用来否定 CPU 瓶颈**，
   不能用来量化 I/O 等待。
2. **逐核（`-u -P ALL`）与系统级（`-u`）的数字不能混着比**。系统级 60% 而某一个核 100%，
   说明有单核热点（机制见 [[Chapter 6. CPUs]]）。

**判据**：先看 `%idle` 与 `%iowait` 的组合——`%idle` 低 + `%iowait` 低 = CPU 饱和；
`%idle` 低 + `%iowait` 高 = I/O 饱和；`%idle` 高但应用仍慢 ⇒ 不是 CPU 问题，往内存与磁盘查。

## §附录 B CPU 调度与上下文切换

**机制**：`-q` 给**排队情况**（有多少任务在等 CPU、负载均值、有多少卡在 I/O），
`-w` 给**切换频率**（每秒新建多少进程、每秒切换多少次上下文）。

> runq-sz plist-sz ldavg-1 ldavg-5 ldavg-15 blocked

> proc/s cswch/s

```bash
sar -q
sar -w
```

**「异常的样子」「往哪查」两列为本篇推演**：

| 指标 | 它测什么 | 异常的样子（推演） | 往哪查 |
|---|---|---|---|
| `runq-sz` | 运行队列长度（正在等 CPU 的任务数） | 持续大于 CPU 数 ⇒ CPU 饱和 | 逐核 `sar -u -P ALL` 找热点，进程级定位 |
| `plist-sz` | 进程表里的任务总数 | 很大但 `runq-sz` 很小 ⇒ 多数任务不是在等 CPU | 看 `blocked` 与 `sar -d` |
| `ldavg-1/5/15` | 1 / 5 / 15 分钟负载均值 | 高但 `runq-sz` 不高 ⇒ 负载里混了不可中断 I/O 的任务 | 见下方「load average 的坑」 |
| `blocked` | 阻塞在 I/O 上的任务数 | > 0 ⇒ 有任务卡在 I/O，不是卡在 CPU | `sar -d`、`sar -B` |
| `proc/s` | 每秒创建的进程数 | 很高 ⇒ 进程 churn（服务反复重启、fork 风暴） | 服务日志、cgroup 限制 |
| `cswch/s` | 每秒上下文切换次数 | 很高 ⇒ 调度抖动，切换本身在吃 CPU | `sar -q`、`sar -u` 的 `%system`、perf sched |

**load average 的坑（推演，但有原文出处）**：Linux 的 load average **把处于不可中断 I/O 状态（D 状态）
的任务也算进去**，所以 load 高不等于 CPU 忙。这一点在
[[附录A-Appendix A： USE Method： Linux]] 里有原文级的明确说明（附录 A 特意说明它**不**把 load average
列进 CPU 指标）。本附录只列字段名、不断言这层语义，两者合起来才是完整的读法。

**判据**：`-q` 与 `-w` 是一对：`runq-sz` 高 + `cswch/s` 高 = 真的在排队抢 CPU；
`runq-sz` 高 + `blocked` 高 + `cswch/s` 低 = 大家其实都卡在 I/O 上，别去调 CPU 配额。

## §附录 B 内存占用

**机制**：`-r` 给内存的**存量视图**（多少空闲、多少缓存、多少已承诺、多少脏页）。

> kbmemfree kbavail kbmemused %memused kbbuffers kbcached kbcommit %commit kbactive kbinact kbdirty

```bash
sar -r
```

**「异常的样子」「往哪查」两列为本篇推演**：

| 指标 | 它测什么 | 异常的样子（推演） | 往哪查 |
|---|---|---|---|
| `kbmemfree` | 完全没被占用的内存 | 低本身**不是问题** | 看 `kbavail` |
| `kbavail` | **不swap就能拿到的内存**（含可回收缓存） | 低 ⇒ 真的要分配内存时会紧张 | 这是判断「还有没有余量」的字段 |
| `%memused` / `kbmemused` | 主存占用率 | 高但 `kbavail` 高 ⇒ 多半是 page cache，不是问题 | 以 `kbavail` 为准 |
| `kbbuffers` / `kbcached` | 传统缓冲区 / page cache | `kbcached` 大 ⇒ 缓存吃内存，可回收 | 与 `kbavail` 一起读 |
| `kbcommit` / `%commit` | 已承诺的虚拟内存量 / 占上限比例 | `%commit` 贴近 100 ⇒ 提交量超限，有 OOM 风险 | 进程虚拟内存、overcommit 策略 |
| `kbactive` / `kbinact` | 活跃 / 非活跃页 | 活跃页持续涨 ⇒ 进程在吃内存 | 进程级 RSS 排序 |
| `kbdirty` | 脏页（已改但还没写回） | 高 ⇒ 回写压力大，磁盘会被写爆 | `sar -d` 的 `wkB/s` 与 `await` |

**判据**：判断「内存够不够」只看两个字段：**`kbavail`**（还能不能分配）和
**`%commit`**（会不会被 OOM）。`%memused` 高而 `kbavail` 高，是缓存，不是故障
（机制见 [[Chapter 7. Memory]]）。

## §附录 B 分页与大页

**机制**：`-B` 给**缺页与回收**的活动速率，`-H` 给大页的占用。

> pgpgin/s pgpgout/s fault/s majflt/s pgfree/s pgscank/s pgscand/s pgsteal/s %vmeff

> kbhugfree kbhugused %hugused

```bash
sar -B
sar -H
```

**「异常的样子」「往哪查」两列为本篇推演**：

| 指标 | 它测什么 | 异常的样子（推演） | 往哪查 |
|---|---|---|---|
| `fault/s` | 每秒缺页次数（**含**次要缺页） | 高 ⇒ 内存分配频繁 churn | 进程级 minor fault 排序 |
| `majflt/s` | 每秒**主要缺页**（真的去读了磁盘） | > 0 ⇒ 已经产生磁盘 I/O，页缓存没命中 | `sar -d`、工作集超过内存 |
| `pgpgin/s` / `pgpgout/s` | 页面读入 / 写出速率 | 持续非零 ⇒ 页在来回搬 | `sar -d`、工作集大小 |
| `pgfree/s` | 回收的空闲页数 | 与 `pgscank` 同看 | — |
| `pgscank/s` / `pgscand/s` | 页扫描器扫描的页数 | 持续 > 0 ⇒ **内存回收压力已经出现** | `sar -r` 的 `kbavail`、进程 RSS |
| `pgsteal/s` | 被换出（steal）走的页 | > 0 ⇒ 已经在把页往外赶 | `sar -S` / `sar -W` |
| `%vmeff` | 回收效率（回收多少 vs 扫描多少） | 长期偏低 ⇒ 内存正在抖动 | `sar -S`、工作集 |
| `%hugused`（`-H`） | 大页已用比例 | 大页没起来 ⇒ TLB 压力没缓解 | 内核是否开启大页（见下） |

**分水岭是 `majflt/s`**（推演）：`fault/s` 高而 `majflt/s` 接近 0 = 只是分配 churn，不碰磁盘；
`majflt/s` 非零 = **已经在读磁盘**，问题从「内存分配」变成「工作集装不下」。

**大页的前提**：原文明确提醒有些选项要内核特性支持（举的就是大页），
所以 `sar -H` 的输出可能为空——那是**特性没开**，不是「没有大页使用」（见 §附录 B 定位与边界 引的那句）。

## §附录 B 交换区

**机制**：`-S` 给交换区的**存量**，`-W` 给**换入换出的速率**。

> kbswpfree kbswpused %swpused kbswpcad %swpcad

> pswpin/s pswpout/s

```bash
sar -S
sar -W
```

**「异常的样子」「往哪查」两列为本篇推演**：

| 指标 | 它测什么 | 异常的样子（推演） | 往哪查 |
|---|---|---|---|
| `%swpused` | 交换区占用率 | 高 = 历史上换出去过很多 | 看 `%swpcad` 判断是不是「刚换出又换回」 |
| `kbswpcad` / `%swpcad` | swap cache（已换出但仍在内存里的页） | 占比高 ⇒ 大量页在 swap 与内存之间来回 | `pswpout/s` 是否同时很高 |
| `pswpin/s` / `pswpout/s` | 每秒换入 / 换出的页数 | **持续非零才是真的在换** | `sar -B` 的 `pgsteal/s`、`sar -d` |

**判据**：**只看 `sar -W` 的速率**。`%swpused` 高但 `pswpout/s` 为 0 = 历史上换过、现在很安静；
`pswpout/s` 持续为正 = 正在抖。

**一个术语坑（推演）**：现代 Linux 的「swap」是**分页**，不是把整个进程换出；
说「这个进程被换出了」在 Linux 上是不成立的说法（机制见 [[Chapter 3. Operating Systems]] 与 [[Chapter 7. Memory]]）。

## §附录 B 磁盘 I/O

**机制**：`-d` 给每个设备的**吞吐（tps、kB/s）、请求形态（areq-sz）、
排队（aqu-sz、await）、忙碌程度（%util）**。

> tps rkB/s wkB/s areq-sz aqu-sz await svctm %util

```bash
sar -d
```

**「异常的样子」「往哪查」两列为本篇推演**：

| 指标 | 它测什么 | 异常的样子（推演） | 往哪查 |
|---|---|---|---|
| `tps` | 每秒 I/O 次数 | 高 = 随机小 I/O 负载 | 与 `rkB/s` 一起看请求大小 |
| `rkB/s` / `wkB/s` | 每秒读 / 写千字节 | 写速率持续高 ⇒ 回写或刷盘 | `sar -r` 的 `kbdirty` |
| `areq-sz` | 平均请求大小（kB ÷ tps） | 很小 = 随机小 IO；很大 = 顺序大 IO | 决定 IOPS 与吞吐哪个是瓶颈 |
| `aqu-sz` | **平均队列深度** | 持续 > 1 ⇒ 请求在排队 | 这是本组里最关键的饱和度字段 |
| `await` | 平均请求延迟（含排队） | 高 ⇒ 有等待 | 与 `aqu-sz` 组合判别 |
| `svctm` | 设备真正在处理请求的时间 | —— | 拿不准时直接用 `aqu-sz` |
| `%util` | 设备处于忙状态的时间占比 | 100% **不等于**满吞吐 | 与 `aqu-sz`、`await` 一起看 |

**三条组合判据**（推演，也是本篇最该记住的一段）：

| 组合 | 结论 |
|---|---|
| `%util` 高 + `aqu-sz` 高 + `await` 高 | 设备被打满 ⇒ 加设备或减负载 |
| `%util` 低 + `await` 高 | **不是排队** ⇒ 介质 / 控制器 / 线缆问题 |
| `%util` 高 + `areq-sz` 小 | 请求太小吃掉了 IOPS ⇒ 合并 / 批量化 I/O |

（`%util` 的这个陷阱在 [[附录A-Appendix A： USE Method： Linux]] 里有原文级的说明。）

## §附录 B 网络接口

**机制**：`-n DEV` 给每个网卡的**吞吐与利用率**，`-n EDEV` 给**错误与丢包**。

> rxpck/s txpck/s rxkB/s txkB/s rxcmp/s txcmp/s rxmcst/s %ifutil

> rxerr/s txerr/s coll/s rxdrop/s txdrop/s txcarr/s rxfram/s rxfifo/s txfifo/s

```bash
sar -n DEV
sar -n EDEV
```

**「异常的样子」「往哪查」两列为本篇推演**：

| 指标 | 它测什么 | 异常的样子（推演） | 往哪查 |
|---|---|---|---|
| `rxkB/s` / `txkB/s` | 每秒收 / 发千字节 | 接近链路上限 ⇒ 带宽饱和 | 与网卡标称速率比（见附录 A） |
| `%ifutil` | 网卡利用率（**自带分母**） | 持续接近 100% ⇒ 链路易满 | `rxkB/s` ÷ 标称速率交叉验证 |
| `rxpck/s` / `txpck/s` | 每秒包数 | 高而 kB/s 低 = 小包多，协议栈开销大 | 抓包看包长分布 |
| `rxmcst/s` | 组播包速率 | 异常高 ⇒ 组播风暴 | 上游组播源 |
| `rxerr/s` / `txerr/s` | 收发错误（硬件 / 驱动层） | > 0 ⇒ 网卡、线缆或驱动 | `dmesg`、驱动统计 |
| `rxdrop/s` / `txdrop/s` | 内核丢弃（队列满） | > 0 ⇒ **本机处理不过来** | 队列深度、softnet backlog |
| `rxfifo/s` / `txfifo/s` | 硬件 FIFO 溢出 | > 0 ⇒ 硬件层丢包 | 网卡中断亲和性、队列数 |
| `rxfram/s` / `txcarr/s` | 帧错误 / 载波丢失 | > 0 ⇒ 物理链路问题 | 换线、换口 |
| `coll/s` | 冲突 | > 0 ⇒ 半双工冲突 | 改全双工（遗留场景） |

**一条重要的分工**（推演）：**drop 是「本机队列满」，error 是「链路或硬件不对」**。
两者都不是「路径上丢了包」——路径丢包要到 `sar -n ETCP` 的 `retrans/s` 才看得到（见下节）。
附录 A 明确说过：丢包同时算饱和与错误两种指标。

## §附录 B 网络协议栈

**机制**：`-n IP` / `-n EIP` 看 IP 层，`-n TCP` / `-n ETCP` 看 TCP 层，
`-n SOCK` 看 socket 存量。**这三个层次必须分开读**（推演）：同一件事在三层各有各的指纹。

> ihdrerr/s iadrerr/s iukwnpr/s idisc/s odisc/s onort/s asmf/s fragf/s

> atmptf/s estres/s retrans/s isegerr/s orsts/s

> totsck tcpsck udpsck rawsck ip-frag tcp-tw

```bash
sar -n IP
sar -n EIP
sar -n TCP
sar -n ETCP
sar -n SOCK
```

**「异常的样子」「往哪查」两列为本篇推演**：

| 指标 | 它测什么 | 异常的样子（推演） | 往哪查 |
|---|---|---|---|
| `active/s` / `passive/s` | 每秒新建的**主动 / 被动**连接 | active 远高于 passive ⇒ 本机以客户端为主 | 应用角色与连接模型 |
| `iseg/s` / `oseg/s` | 收到 / 发出的 TCP 段数 | 段数高而带宽不高 = 小包 | 业务形态 |
| `atmptf/s` | 连接尝试失败 | > 0 ⇒ 连不上（对端拒、端口关、资源不足） | 应用日志、`tcp-tw` |
| `estres/s` | 已建立连接被重置 | 突然升高 ⇒ 服务端异常或中间设备清连接 | 服务日志、LB 超时配置 |
| `retrans/s` | **每秒重传的段数** | 持续 > 0 ⇒ **路径上丢包 / 拥塞** | MTU、链路质量、中间设备（机制见 [[Chapter 10. Network]]） |
| `isegerr/s` | 收到但不合法的段 | > 0 ⇒ 对端实现或版本不兼容 | 抓包 |
| `orsts/s` | 发出重置 | > 0 ⇒ 本机主动断开 | 应用侧 |
| `ihdrerr/s` / `iadrerr/s` | IP 头 / 地址错误 | > 0 ⇒ 收到坏包 | 抓包、上游设备 |
| `idisc/s` / `odisc/s` | 收到 / 发出被丢弃的包 | > 0 ⇒ 协议栈按策略丢了 | 防火墙 / 限速 / 队列 |
| `fragf/s` / `fragcrt/s` | 分片失败 / 被创建 | fragcrt 持续非零 ⇒ **报文太大被碎** | MTU / PMTU 黑洞 |
| `totsck` / `tcpsck` / `udpsck` | socket 总数 / TCP / UDP 数 | 逼近上限 ⇒ fd 或端口耗尽 | `sar -v`、ulimit |
| `tcp-tw` | **TIME_WAIT 数量** | 高 ⇒ 连接churn，端口可能被占满 | 短连接的写法、连接池 |

**判据**：**丢包的第一现场是 `retrans/s`**，不是 drop。链路层 drop 说明是本机的问题，
重传说明是路径的问题——两者归因完全不同。

## §附录 B 内核表

**机制**：`-v` 给内核维护的**全局表项**的使用量。

> dentunusd file-nr inode-nr pty-nr

```bash
sar -v
```

**「异常的样子」「往哪查」两列为本篇推演**：

| 指标 | 它测什么 | 异常的样子（推演） | 往哪查 |
|---|---|---|---|
| `file-nr` | 已分配的文件句柄 / 文件结构数 | 逼近 `/proc/sys/fs/file-max` ⇒ **会开始出现 EMFILE** | 谁的 fd 泄漏（附录 A 给了进程级路径） |
| `inode-nr` | 已用 inode 数 | 逼近 `nr_inodes` ⇒ 无法再创建文件 | 文件数量、单目录文件数 |
| `dentunusd` | 未使用的 dentry 缓存条目 | 持续增长 ⇒ 文件系统元数据 churn | 工作目录变动频繁的服务 |
| `pty-nr` | 伪终端数量 | 异常高 ⇒ 有大量 shell / 作业 | 容器与 CI 环境 |

**判据**：`file-nr` 逼近上限是**硬失败**（fd 满直接 EMFILE，不排队），
所以它属于「错误」这一问而不是「饱和」这一问——理由见
[[附录A-Appendix A： USE Method： Linux]] 里「存储容量 / 文件描述符」那两格。

## 章末选型判据

| 场景 | 用哪个选项 | 判据 | 别用什么 |
|---|---|---|---|
| 还没定位到是哪个资源出问题 | 按资源域依次取：`-u` → `-q` → `-r` → `-d` → `-n DEV` | 每一域先看「利用率」，再看「饱和」，再看「错误」 | 别拿一条 `sar` 输出下结论 |
| 「机器很慢」，不确定是不是 CPU | `sar -u` 的 `%idle` 与 `%iowait` | `%idle` 低 + `%iowait` 低 ⇒ CPU 饱和 | 别只看 `%user` |
| CPU 利用率正常但应用还是慢 | `sar -q` 的 `runq-sz`、`blocked` | `blocked` > 0 ⇒ 卡在 I/O | 别拿 load average 当 CPU 利用率 |
| 怀疑有单核热点 | `sar -u -P ALL` | 系统级不高但某核接近 100% ⇒ 单核热点 | 别拿系统级数字下结论 |
| 怀疑进程churn | `sar -w` 的 `proc/s`、`cswch/s` | 两者同时高 ⇒ 创建/切换风暴 | 别只看 CPU 利用率 |
| 「内存不够用」 | `sar -r` 的 `kbavail` 与 `%commit` | `%memused` 高但 `kbavail` 高 ⇒ 是缓存不是故障 | 别用 `%memused` 判内存不足 |
| 怀疑内存在抖动 | `sar -B` 的 `majflt/s`、`pgscank/s`、`%vmeff` | `majflt/s` 非零 = 已经在读盘 | 别用 `%memused` 判抖动 |
| 怀疑在换页 | `sar -W` 的 `pswpin/s`、`pswpout/s` | 速率持续非零才算 | 别用 `%swpused` 判断「正在换」 |
| 磁盘「慢」 | `sar -d` 的 `aqu-sz` + `await` + `%util` | 队列深 ⇒ 排队；`%util` 低而 `await` 高 ⇒ 不是排队 | 别单看 `%util` |
| 网络「带宽不够」 | `sar -n DEV` 的 `%ifutil`、`rxkB/s`/`txkB/s` | 必须对链路标称速率算分母 | 别报没有分母的百分比 |
| 网络「丢包」 | `sar -n EDEV` 的 drop vs `sar -n ETCP` 的 `retrans/s` | drop ⇒ 本机队列满；retrans ⇒ 路径丢包 | 别把两者混为一谈 |
| 连接数打满 / 端口耗尽 | `sar -n SOCK` 的 `tcp-tw`、`totsck` | `tcp-tw` 持续很高 ⇒ 连接churn | 别用 `netstat` 的一次快照判断 |
| 「打不开文件」 | `sar -v` 的 `file-nr` vs `file-max` | 逼近上限 ⇒ 即将 EMFILE | 别等报错才查 |
| 大页没起作用 | `sar -H` 的 `%hugused` | 输出为空 = 内核没开这个特性 | 别当成「没有大页使用」 |

**三条「什么时候别用这份表」**（推演）：

1. **你要的是字段定义，不是选项索引** —— 本附录只回答「哪个指标挂在哪个选项下」，
   字段语义查 man page，机制查对应章节。
2. **你要的是阈值** —— 本附录**一个阈值都没有**；阈值要么来自 USE 清单
   （[[附录A-Appendix A： USE Method： Linux]]），要么来自各章的机制推导。
3. **你要的是采集与回放** —— 原文没讲怎么把数据存下来（sysstat 的定时任务、历史文件回放），
   本篇也不补。
