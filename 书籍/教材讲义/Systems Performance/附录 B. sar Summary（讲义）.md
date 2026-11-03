---
title: 附录 B. sar Summary（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - sar
  - 指标速查
  - 可观测性
  - 延迟
  - 故障排查
source: "《Systems Performance》（Brendan Gregg，2018，英文原版）附录 B：sar Summary"
companion: "[[附录B-Appendix B： sar Summary]]"
---

# 附录 B 讲义：21 组选项，一张表覆盖你 90% 的性能问题

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[附录B-Appendix B： sar Summary|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：`src/net/metrics.h`（本仓**自制**的固定计数器端点 —— 手写的「小号 sar」）、`src/net/tcp_transport.cpp:308`（`TCP_NODELAY` 必设 —— 对应表里 `TCP errors` / `retrans/s` 那一组）、`src/net/metrics.h:43`（`tick_drift_max_ms` —— 本仓唯一的**延迟**指标）。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原附录。

> ⚠️ **这一篇是速查表，不是章节。** 原文稿只有 157 行、一张被 epub 拍平的表。
> 但**表被拍平这件事本身有代价** —— 原文说「我把常看的指标**加粗**了」，
> 而 epub 抽取**把加粗全丢了**。所以你手上这份速查表**缺了作者最想让你看的那几个**。
> 讲义这一篇做三件事：把表**重建**出来、给表**加一根轴**（症状 → 查哪一项）、
> 再诚实地说明**哪些加粗丢了**。

---

## 0. 开局：`sar -q` 显示 runq-sz 是 1，然后呢？

某台服务器卡了。你登上机器，敲：

```bash
$ sar -q 1 5
```

```
14:23:01      CPU     %user     %nice     %system   %iowait       %steal       %idle
14:23:02         0      0.00      0.00      0.51      0.00         0.00      99.48
...
```

不对，`-q` 不是 CPU 利用率，`-q` 是**运行队列**：

```
00:00:01  runq-sz  plist-sz   ldavg-1   ldavg-5  ldavg-15   blocked
00:00:02       1.00      2.00      0.00      0.00      0.00      0.00
```

**runq-sz = 1，blocked = 0。**

这两个数一起说明了一件事：

- **runq-sz = 1** ⇒ 跑着的就是那个唯一可运行的进程，**CPU 没排队**。
- **blocked = 0** ⇒ **没有进程在等 I/O**（D 状态）。

⇒ 「CPU 没排队，也没人等 I/O」。**那卡在哪？**

答案可能是：卡在**网络**（包丢了、重传了、内核缓冲溢出了），
也可能是卡在**锁**（进程在等锁，但那不体现在 `blocked` 里），
也可能是卡在**一次 syscall 里的内核计算**。

**而上面这两条信息，`sar -q` 一个字都没说。**

⇒ 这就是这张表存在的意义：**它不是「性能总表」，它是「症状 → 该翻哪一页」的索引。**
本篇第 2 节就建这张索引。

---

## 1. §附录 B 这张表怎么读

### 1.1 原表有三列

| 列 | 内容 |
|---|---|
| Option | `sar` 的选项（`-u` / `-n TCP` / `-d` …） |
| Metrics | 这个选项下有哪些指标（一个长列表） |
| Description | 一句话说明这组指标量的是什么 |

⚠️ **在原文稿里，这张表是被拍平的**：epub 抽取把表格的每一格拆成了独立的一行，
所以你读到的原文稿长这样：

```
Option

Metrics

Description

-u-P ALL

%user %nice %system %iowait %steal %idle

Per-CPU utilization ( -u optional)
```

**本讲义把表重建了**（下面 1.2），但**要记住重建这件事本身就是信息** ——
它说明这份原文稿在这一节**不可直接阅读**，得回原书看排版。

### 1.2 重建后的表（按「问题域」重排）

**CPU 域**

| Option | Metrics | Description |
|---|---|---|
| `-u` | %user %nice %system %iowait %steal %idle | CPU utilization |
| `-u ALL` | … %irq %soft %guest %gnice | CPU utilization extended |
| `-u -P ALL` | 同上（**按 CPU 逐个**） | Per-CPU utilization ( -u optional) |
| `-m CPU -P ALL` | MHz | Per-CPU frequency |
| `-q` | runq-sz plist-sz ldavg-1 ldavg-5 ldavg-15 blocked | CPU run-queue size |
| `-w` | proc/s cswch/s | CPU scheduler events |

**内存域**

| Option | Metrics | Description |
|---|---|---|
| `-r` | kbmemfree kbavail kbmemused %memused kbbuffers kbcached kbcommit %commit kbactive kbinact kbdirty | Memory utilization |
| `-S` | kbswpfree kbswused %swpused kbswpcad %swpcad | Swap utilization |
| `-W` | pswpin/s pswpout/s | Swapping statistics |
| `-B` | pgpgin/s pgpgout/s fault/s majflt/s pgfree/s pgscank/s pgscand/s pgsteal/s %vmeff | Paging statistics |
| `-H` | kbhugfree kbhugused %hugused | Huge pages |
| `-v` | dentunusd file-nr inode-nr pty-nr | Kernel tables |

**存储域**

| Option | Metrics | Description |
|---|---|---|
| `-d` | tps rkB/s wkB/s areq-sz aqu-sz await svctm %util | Disk statistics |

**网络域**

| Option | Metrics | Description |
|---|---|---|
| `-n DEV` | rxpck/s txpck/s rxkB/s txkB/s rxcmp/s txcmp/s rxmcst/s %ifutil | Network interface statistics |
| `-n EDEV` | rxerr/s txerr/s coll/s rxdrop/s txdrop/s txcarr/s rxfram/s rxfifo/s txfifo/s | Network interface errors |
| `-n IP` | irec/s fwddgm/s idel/s orq/s asmrq/s asmok/s fragok/s fragcrt/s | IP statistics |
| `-n EIP` | ihdrerr/s iadrerr/s iukwnpr/s idisc/s odisc/s onort/s asmf/s fragf/s | IP errors |
| `-n TCP` | active/s passive/s iseg/s oseg/s | TCP statistics |
| `-n ETCP` | atmptf/s estres/s retrans/s isegerr/s orsts/s | TCP errors |
| `-n SOCK` | totsck tcpsck udpsck rawsck ip-frag tcp-tw | Socket statistics |

> 表里 `-u-P ALL` 的写法是 epub 拍平后的产物 —— 原文那格是
> `-u -P ALL`（`-P` 是可选的 per-CPU 参数）。同类地，
> `-m CPU-P ALL` 应读作 `-m CPU -P ALL`。

### 1.3 一条贯穿全表的规律：**`E` 前缀 = 错误**

看网络那七行：`DEV` / **`EDEV`** / `IP` / **`EIP`** / `TCP` / **`ETCP`** / `SOCK`。

**带 `E` 的三行是错误计数器。** 它们的用法和别的指标**完全相反**：

- 普通指标：**看趋势**（涨了说明负载上来了）。
- 错误指标：**平时恒为 0，一旦有值就是事故**。

⇒ **推演（讲解性类比，非原文）**：`sar` 的普通指标像**体温**（关心的是偏高还是偏低），
错误指标像**心电图里的报警音**（不响最好，响了一下就该停下手里的活去看）。

### 1.4 作者说「我加粗了常看的指标」—— **但加粗丢了**

原文最后一句：

> I have highlighted in bold the key metrics that I look for.

**这句话在原文稿里失去了它的后半截。** epub 抽取把表格拍平，
**加粗标记（`<b>` / `<strong>`）随之丢失**，所以你手上这份表是**等权重的** ——
作者标出来的那几个「我最常看的」，现在和其余指标长得一模一样。

⇒ **要拿到加粗版本，只能回原书。** 这是这份原文稿的一个**真实信息损失**，
本篇把它记下来，免得你以为「表里全 equally 重要」。

原文还有一条版本说明：

> Some sar(1) options may require kernel features enabled (e.g., huge pages), and some metrics were added in later versions of sar(1) (version 12.0.6 is shown here).

⇒ **这张表是 `sar` 12.0.6 的快照。** 你机器上的 `sar` 版本不同，
指标可能多也可能少 —— 附录自己也说了「完整清单看 man page」。

---

## 2. §附录 B 真正该先记住的：症状 → 查哪一项

这张表有 21 行。**记不住 21 行，也没必要。** 记「症状 → 选项」这条映射就够了。

### 2.1 CPU 不够

`sar -u` → 看 `%user`（你的程序在算）、`%system`（你在内核里花得多）、
`%iowait`（在等 I/O）、`%idle`。

**关键判据**：`%iowait` 高 ⇒ **不是 CPU 不够，是 I/O 不够**。
这时候加 CPU **一点用都没有**，该去查磁盘/网络。

`sar -q` → `runq-sz` 大于 CPU 核数 ⇒ 真的排队了。
`blocked` 大 ⇒ 有进程卡在 D 状态（不可中断睡眠 = 等 I/O）。

### 2.2 「卡顿」但 CPU 也不满

`sar -w` → `cswch/s`（每秒上下文切换次数）。

**上下文切换多 ⇒ CPU 花在做切换上，不是做正事上。**
常见成因：线程太多、锁竞争、**定时器太密**。

`proc/s` 是每秒创建的进程数 —— 这个数突然变大，说明有东西在**反复 fork**。

### 2.3 内存不够 / 被换出

`sar -r` → `kbavail`（**真正能给程序用的**，比 `kbmemfree` 有意义得多）、
`%memused`、`kbcommit` / `%commit`（**承诺量**，和 `memused` 不是一回事）。

> **推演（讲解性类比，非原文）**：`kbavail` 是「手头现金」，`kbmemfree` 是「柜子里锁着的现金」——
> 锁着的那部分要等回收才能用。**Linux 的内存管理就是「不让内存闲下来」**，
> 所以 `kbmemfree` 常年接近 0 是**正常的**，别拿它报警。

`sar -S` → swap 的使用量。**`%swpused` 涨 = 真的在换页 = 延迟会飙。**
`sar -W` → `pswpin/s` / `pswpout/s`，换页的**速率**。

`sar -B` → `majflt/s`（**主缺页**：缺页不得不去读磁盘）vs `fault/s`（次缺页）。
**`majflt/s` 持续非零 = 在读磁盘 = 慢。**
`%vmeff` 是「虚拟内存效率」，越接近 100 越好。

### 2.4 磁盘慢

`sar -d` → `await`（平均 I/O 等待时间）、`%util`（设备忙碌百分比）、
`aqu-sz`（平均队列深度）、`areq-sz`（平均请求大小）。

**`%util` 是最容易被误读的指标**：`%util` 高**不一定**意味着「要加盘」——
现代设备（NVMe、RAID）能在**高队列深度**下把 `%util` 顶到 100% 而延迟依然很低。
**看延迟（`await`）比看利用率准。**

### 2.5 网络出问题

`sar -n EDEV` → `rxdrop/s` / `txdrop/s`（**内核缓冲丢包**）、
`rxerr/s` / `txerr/s`、`coll/s`（**冲突**，老式共享总线以太网才有）。

**`rxdrop/s` 非零 ⇒ 包在到达应用之前就被内核扔了。**
这时候应用层看到的现象是「连接还在，但消息少了几条」—— **最难查的一类**。

`sar -n ETCP` → `retrans/s`（**重传**）、`atmptf/s`（**连接尝试失败**）、
`estres/s`（**established 状态的重置**）、`isegerr/s`（入站段错误）。

**`retrans/s` 是网络健康度的第一指标。** 它涨 ⇒ 丢包或对端不响应。

`sar -n TCP` → `active/s` / `passive/s`（主动/被动打开数）、`iseg/s` / `oseg/s`（入/出站段速率）。

`sar -n SOCK` → `totsck`（socket 总数）、`tcp-tw`（**TIME_WAIT 数量**）、`ip-frag`。

**`tcp-tw` 暴涨** ⇒ 短连接太多 ⇒ 端口耗尽 / `tcp_tw_reuse` 没开。
这是「连不上但服务器没崩」的经典原因。

---

## 3. 实战锚点：`otf-server` 用不用 `sar`？

### 3.1 短答：**不用**，而且**大部分不需要**

本仓是**单线程 epoll reactor**（`src/main.cpp` 的主循环：
`transport.Poll` 在 `:314`，`sched.Advance` 在 `:340`）。逐条对照：

| `sar` 选项 | 本仓的情况 |
|---|---|
| `-q` runq-sz | **恒为 1** —— 只有一个线程在跑，没有第二个可运行的进程 |
| `-q` blocked | 进程很少阻塞在 D 状态（socket 是非阻塞的，`epoll_wait` 是可中断的） |
| `-w` cswch/s | 上下文切换几乎全是 `epoll_wait` 的唤醒，**与负载无关** |
| `-S` / `-W` swap | 游戏服务器内存占用小，**不构成 swap 压力** |
| `-d` await / %util | 游戏状态**全在内存**，磁盘不在热路径 |
| `-n TCP` active/s | 每个客户端一条长连接，`active/s` 与连接建立速率同阶 |
| `-v` file-nr | 连接数上限（房间数 32、每房 4 人）**远低于 fd 上限** |

⇒ **换句话说：这张表里 21 组选项，本仓真正需要关注的可能只有 `-n EDEV` 和 `-n ETCP` 两组。**

### 3.2 但那两组，本仓**恰恰没有观测手段**

`sar -n EDEV` 的 `rxdrop/s`、`sar -n ETCP` 的 `retrans/s` ——
**内核层面的丢包和重传，应用层看不见**。

本仓的 `MetricsSnapshot`（`src/net/metrics.h:29`）里有 `frames_sent` / `frames_recv`
（**累计**帧数），但：

- 它是**总量**，不是**速率**，也不是**差值**；
- 它**看不到** TCP 重传、内核缓冲丢包 —— 那些包根本没进 `epoll_wait`。

⇒ **推演（讲解性类比，非原文）**：`frames_recv` 像是「前台收到的包裹数」，
而 `rxdrop/s` 是「**快递公司门口扔掉的包裹数**」。
你盯着前台签收数看，永远发现不了后门在丢件。

**这正是 SP 第 4 章（Observability Tools）的主题**：
**观测手段决定你能看见哪一类故障。** 本仓的观测面是**应用内埋点**，
所以它**结构性地看不见内核层的丢包和重传**。

### 3.3 本仓的「自制 sar」：一个手写的固定计数器端点

`src/net/metrics.cpp` + `src/net/metrics_server.cpp` 做的事情，
就是 `sar` 做的**一个子集**：

- **固定计数器**（不是时间序列）⇒ `MetricsSnapshot` 是一个**瞬时快照**。
- **刻意手写、零第三方依赖**（`metrics_server.h` 文件头明写）。
- **端口本身就是访问控制**（不做路由、不做鉴权、不看请求内容）。
- **自己开 socket，不注册进游戏的 epoll** ⇒ 观测端点不会拖垮游戏端点。

但 `metrics_server.h` 的文件头同时写了一条硬约束：

> `Poll()` 永不阻塞、永不抛异常、永不无限循环

⇒ 这是**故障域隔离**的代价：**观测手段自己必须先活着。**

`MetricsSnapshot` 里两个**延迟类**指标值得单独看：

| 字段 | 含义 |
|---|---|
| `tick_drift_max_ms` | 最大 tick 漂移（**峰值，带符号**，`std::optional`） |
| `snapshot_send_ms_max` | 快照发送延迟峰值（**峰值**） |

`tick_drift_max_ms` 那个 `std::optional` 的来由，文件头注释里写得很清楚：
取峰值的比较是 `v > cur`、初值 0 ⇒ **所有负漂移被静默吞掉**；
而「sim 跑得比墙钟快」正是会导致双端不同步的故障，**它恰好只在负半轴出现**。

⇒ **这一条在 `sar` 的世界里没有对应物** ——
`sar` 没有「应用逻辑比墙钟快」这个指标。
**这是「自制观测」比通用工具强的地方，也是它比通用工具弱的地方。**

### 3.4 一个反向的对照：`TCP_NODELAY` 与表里的 TCP 组

`src/net/tcp_transport.cpp:308` 有一处必设：

```cpp
if (::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) < 0) {
```

注释写明：Nagle 会把小包攒着等下一个包，**最多延迟 0.5 秒**，症状是「偶发 0.5s 卡顿」。

⇒ 这件事在 `sar` 的世界里**有一个近似对应物**：`sar -n TCP` 的 `oseg/s`（出站段速率）
**会异常地低**（因为小包被攒着没发），而 `sar -n ETCP` 什么也不显示 ——
**Nagle 造成的延迟不是错误，是「正确地做错事」**。

⇒ 这条是本篇的一条**判据**：
**通用计数器能告诉你「哪里不对」，但不一定能告诉你「为什么不对」。**
本仓能，因为「为什么」写在代码注释里。

---

## 4. 收尾：本章金句清单

1. **This is a summary of options and metrics from the system activity reporter, sar(1). You can use this to jog your memory about which metrics are under which options.** —— 附录的定位：**备忘卡**，不是教材。
2. **See the man page for the full list.** —— 完整清单在 man page 里；这张表是索引，不是全集。
3. **sar(1) is introduced in Chapter 4 , Observability Tools , Section 4.4 , and selected options are summarized in later chapters ( 6 , 7 , 8, 9 , 10 ).** —— 这张表是**散落的各章选段的汇总**。
4. **I have highlighted in bold the key metrics that I look for.** —— ⚠️ **这句在原文稿里丢了后半截**：加粗标记被 epub 拍平掉了，作者标出的「最常看的」现在和其余指标长得一样。
5. **Some sar(1) options may require kernel features enabled (e.g., huge pages), and some metrics were added in later versions of sar(1) (version 12.0.6 is shown here).** —— **这张表是 sar 12.0.6 的快照**，版本不同，指标集不同。
6. **CPU utilization extended** —— `-u ALL` 比 `-u` 多了 `%irq` / `%soft` / `%guest`：**中断**和**虚拟化偷走的时间**在这里才看得见。
7. **CPU run-queue size** —— `runq-sz` 大于核数才叫「CPU 不够」；`blocked` 才是「I/O 不够」。
8. **CPU scheduler events** —— `cswch/s` 是「卡顿但 CPU 不满」时的第一个该看的数。
9. **Memory utilization** —— 看 `kbavail`（真能用的），不是 `kbmemfree`（躺着不动的）。
10. **Paging statistics** —— `majflt/s` 持续非零 = 真的在读磁盘 = 慢。
11. **Disk statistics** —— **`%util` 高不等于该加盘**；看 `await`（延迟）比看利用率准。
12. **Network interface errors** —— `rxdrop/s` 非零 = **包在到达应用之前就被内核扔了**。
13. **IP statistics** / **IP errors** —— IP 层看「进出与分片」，IP 错误看「IP header 错 / 地址错 / 未知协议」。
14. **TCP statistics** / **TCP errors** —— `active/s`/`passive/s` 是连接建立的速率；`retrans/s` 是网络健康度的第一指标。
15. **Socket statistics** —— `tcp-tw` 暴涨 = 短连接太多 = 「连不上但服务器没崩」的经典原因。
