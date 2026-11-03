---
title: Chapter 10. Network（精读）
category: 性能工程
tags:
  - 网络
  - TCP
  - 拥塞控制
  - 延迟分析
  - 内核网络栈
  - 可观测性
difficulty: 进阶
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）第10章 Network"
source_section: "精读 §10.1 / §10.2.3 / §10.3.4 / §10.3.5 / §10.3.6 / §10.3.7 / §10.4.1 / §10.4.2 / §10.4.3 / §10.5.1 / §10.5.2 / §10.5.3 / §10.5.4 / §10.5.7 / §10.5.10 / §10.6.1 / §10.6.9 / §10.6.11 / §10.6.13 / §10.8.1 / §10.8.2；§10.9 Exercises 与 §10.10 References 不立篇（练习题与参考文献是全书共用的自测/索引材料，不承载机制陈述）；§10.2.1 / §10.2.2 / §10.3.1 / §10.3.2 / §10.3.3 / §10.3.8 / §10.3.9 / §10.3.10 / §10.3.11 / §10.5.5 / §10.5.6 / §10.5.8 / §10.5.9 / §10.6.2–§10.6.8 / §10.6.10 / §10.6.12 / §10.6.14 / §10.6.15 / §10.7 / §10.8.3 不单独立篇（概念背景与命令清单型材料，机制要点已在正文对应小节里带过）"
skills:
  - obsidian-markdown
---

# Chapter 10. Network

> **小节地图**：本章是全书的**网络分析专章**——它不提供新的性能方法论（那是第 2 章），
> 而是把「网络为什么会慢」拆成**可测量的机制**。
> 立篇：§10.1（术语口径）、§10.2.3（协议栈与分层术语）、§10.3.4（包长与 MTU）、
> §10.3.5（延迟口径）、§10.3.6（缓冲与 bufferbloat）、§10.3.7（连接积压）、
> §10.4.1（TCP 窗口/重传/拥塞控制）、§10.4.2（硬件上限）、§10.4.3（内核网络栈）、
> §10.5（方法论顺序）、§10.6（工具取舍）、§10.8（调优）。
> **不立篇**：§10.9 Exercises、§10.10 References（自测与文献）；§10.2.1 / §10.2.2、
> §10.3.1–§10.3.3 / §10.3.8–§10.3.11、§10.5.5 / §10.5.6 / §10.5.8 / §10.5.9、
> §10.6 除 ss/tcplife/tcpretrans/tcpdump 外的各工具节、§10.7、§10.8.3
> （概念背景与命令清单，机制要点在正文对应小节已带过）。
> 底层依赖：中断与 softirq 见 [[Chapter 3. Operating Systems]]、CPU 与 softirq 分布见 [[Chapter 6. CPUs]]、
> 系统调用与 strace 见 [[Chapter 5. Applications]]、文件系统与 UDS 见 [[Chapter 8. File Systems]]、
> 方法论总纲见 [[Chapter 2. Methodologies]]、基准测试见 [[Chapter 12. Benchmarking]]、
> BPF 工具链见 [[Chapter 13. perf]] 与 [[Chapter 14. Ftrace]]。
> 同层对照：[[Chapter 11. Cloud Computing]]（云里的网络限速与虚拟化开销）、
> [[Chapter 9. Disks]]（同一套「排队即延迟」的思路在 I/O 侧的展开）。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §10.1 术语口径 | 10-Chapter 10. Network.md L85 | Latency : Network latency can refer to the time it takes for a message to make a round-trip between endpoints, or the time required to establish a connection (e.g., TCP handshake), excluding the data transfer time that follows. |
| §10.3.4 包长与 MTU | 10-Chapter 10. Network.md L185 | Ethernet now supports larger packets (frames) of up to approximately 9,000 bytes, termed jumbo frames . These can improve network throughput performance, as well as the latency of data transfers, by requiring fewer packets. |
| §10.3.5 延迟口径 | 10-Chapter 10. Network.md L203 | This is the time for an ICMP echo request to echo response, as measured by the ping(1) command. This time is used to measure network latency between hosts, including hops in between, and is measured as the time needed for a network request to make a round-trip. It is in common use because it is simple and often readily available: many operating systems will respond to ping by default. It may not exactly reflect the round-trip time of application requests, as ICMP may be handled with a different priority by routers. |
| §10.3.5 延迟口径 | 10-Chapter 10. Network.md L303 | While ping and connection latency measures the latency incurred by the network, first-byte latency includes the think time of the target server. This may include latency if the server is overloaded and needs time to process the request (e.g., TCP backlog) and to schedule the server (CPU scheduler latency). |
| §10.3.6 缓冲与 bufferbloat | 10-Chapter 10. Network.md L321 | Buffering can also be performed by external network components, such as switches and routers, in an effort to improve their own throughput. Unfortunately, the use of large buffers on these components can lead to bufferbloat , where packets are queued for long intervals. This causes TCP congestion avoidance on the hosts, which throttles performance. Features have been added to the Linux 3.x kernels to address this problem (including byte queue limits, the CoDel queueing discipline [Nichols 12] , and TCP small queues). There is also a website for discussing the issue [Bufferbloat 20] . |
| §10.3.7 连接积压 | 10-Chapter 10. Network.md L329 | Backlog drops and SYN retransmits are indicators of host overload. |
| §10.4.1 TCP 传输机制 | 10-Chapter 10. Network.md L403 | Sliding window : This allows multiple packets up to the size of the window to be sent on the network before acknowledgments are received, providing high throughput even on high-latency networks. The size of the window is advertised by the receiver to indicate how many packets it is willing to receive at that time. |
| §10.4.1 重传与拥塞控制 | 10-Chapter 10. Network.md L473 | Timer-based retransmits : These occur when a time has passed and a packet acknowledgment has not yet been received. This time is the TCP retransmit timeout, calculated dynamically based on the connection round-trip time (RTT). On Linux, this will be at least 200 ms (TCP_RTO_MIN) for the first retransmit |
| §10.4.1 重传与拥塞控制 | 10-Chapter 10. Network.md L493 | BBR : Instead of window-based, BBR builds an explicit model of the network path characteristics (RTT and bandwidth) using probing phases. BBR can provide dramatically better performance on some network paths, while hurting performance on others. BBRv2 is currently in development and promises to fix some of the deficiencies of v1. |
| §10.4.1 重传与拥塞控制 | 10-Chapter 10. Network.md L527 | The Linux default (10 packets, aka IW10) can be too high on slow links or when many connections start up; other operating systems default to 2 or 4 packets (IW2 or IW4). |
| §10.4.2 硬件链路 | 10-Chapter 10. Network.md L587 | For example, a dual 10 GbE network interface card is connected to a four-channel PCI express (PCIe) Gen 2 slot. The card has a maximum send or receive bandwidth of 2 × 10 GbE = 20 Gbits/s, and bidirectional, 40 Gbit/s. The slot has a maximum bandwidth of 4 × 4 Gbits/s = 16 Gbits/s. Therefore, network throughput on both ports will be limited by PCIe Gen 2 bandwidth, and it will not be possible to drive them both at line rate at the same time (I know this from practice!). |
| §10.4.3 连接队列 | 10-Chapter 10. Network.md L645 | Bursts of inbound connections are handled by using backlog queues. There are two such queues, one for incomplete connections while the TCP handshake completes (also known as the SYN backlog ), and one for established sessions waiting to be accepted by the application (also known as the listen backlog ). These are pictured in Figure 10.9 . |
| §10.4.3 分段卸载 | 10-Chapter 10. Network.md L671 | Network devices and networks accept packet sizes up to a maximum segment size (MSS) that may be as small as 1500 bytes. To avoid the network stack overheads of sending many small packets, Linux uses generic segmentation offload (GSO) to send packets up to 64 Kbytes in size (“super packets”), which are split into MSS-sized segments just before delivery to the network device. If the NIC and driver support TCP segmentation offload (TSO), GSO leaves splitting to the device, improving network stack throughput. |
| §10.4.3 设备驱动与中断合并 | 10-Chapter 10. Network.md L716 | The Linux kernel uses a new API (NAPI) framework that uses an interrupt mitigation technique: for low packet rates, interrupts are used (processing is scheduled via a softirq); for high packet rates, interrupts are disabled, and polling is used to allow coalescing [Corbet 03] [Corbet 06b] . This provides low latency or high throughput, depending on the workload. Other features of NAPI include: |
| §10.4.3 CPU 扩展 | 10-Chapter 10. Network.md L750 | Without a CPU load-balancing strategy for network packets, a NIC may interrupt only one CPU, which can reach 100% utilization and become a bottleneck. This may show up as high softirq CPU time on a single CPU (e.g., using Linux mpstat(1): see Chapter 6 , CPUs , Section 6.6.3 , mpstat ). This may especially happen for load balancers or proxy servers (e.g., nginx), as their intended workload is a high rate of inbound packets. |
| §10.4.3 内核旁路 | 10-Chapter 10. Network.md L760 | With kernel network stack bypass, instrumentation using traditional tools and metrics is not available because the counters and tracing events they use are also bypassed. This makes performance analysis more difficult. |
| §10.5.2 USE 方法 | 10-Chapter 10. Network.md L898 | Errors may be checked first, since they are typically quick to check and the easiest to interpret. |
| §10.5.4 延迟分析 | 10-Chapter 10. Network.md L1014 | A common source of issues is the presence of latency outliers caused by TCP retransmits. These can be identified using full distributions or per-operation latency tracing, including by filtering for a minimum latency threshold. |
| §10.5.7 TCP 分析 | 10-Chapter 10. Network.md L1099 | The last behavior can become a scalability problem when a server is connecting frequently to another on the same destination port, using the same source and destination IP addresses. The only distinguishing factor for each connection is the client source port—the ephemeral port —which for TCP is a 16-bit value and may be further constrained by operating system parameters (minimum and maximum). Combined with the TCP TIME_WAIT interval, which may be 60 seconds, a high rate of connections (more than 65,536 during 60 seconds) can encounter a clash for new connections. In this scenario, a SYN is sent while that ephemeral port is still associated with a previous TCP session that is in TIME_WAIT, and the new SYN may be rejected if it is misidentified as part of the old connection (a collision). To avoid this issue, the Linux kernel attempts to reuse or recycle connections quickly (which usually works well). The use of multiple IP addresses by the server is another possible solution, as is the SO_LINGER socket option with a low linger time. |
| §10.8 调优前置 | 10-Chapter 10. Network.md L3267 | Before trying tunable parameters, it can be worthwhile to first understand network usage. This may also identify unnecessary work that can be eliminated, leading to much greater performance wins. Try the workload characterization and static performance tuning methodologies using the tools in the previous section. |

> **书里没说的**（本篇有几处推论，原文没有，别当成原文观点）：
> - **「六种延迟按责任方二分（网络侧 / 内核侧 / 服务端侧）」是本篇的读法**。原文 §10.3.5
>   是顺序介绍六种延迟，§10.5.4 的 Table 10.3 是一张平铺的清单，都没有做归因分组。
> - **「先证明不是网络，再回应用」这个顺序是本篇从 §10.5 的建议起步顺序
>   （performance monitoring → USE → static performance tuning → workload characterization）
>   与 §10.5.7 / §10.5.10 推出来的**，原文没有这样一句总结。
> - **「把延迟拆成 传播 / 排队 / 处理 三段」是本篇的拆法**。原文只区分了「网络造成的延迟」
>   与「服务端 think time」（L303），没有给出三段式模型。
> - **「bufferbloat 的排查顺序」是本篇的推论**：原文只说大缓冲导致 bufferbloat 并让 TCP 降速
>   （L321），没给排查步骤。
> - **§10.6 里「按问题选工具」的三档分层是本篇的整理**：原文 §10.6 开头只是说这些工具
>   「starting with traditional tools and statistics, then tracing tools, and finally packet
>   capture tools」。
> - **「窗口大小 = 延迟 × 带宽」这个直觉模型是本篇的推演**：原文 §10.4.1 只说滑动窗口
>   允许在收到确认前发送窗口大小的多个包、窗口由接收方通告，没有给这个模型。

## §10.1 术语口径

这一节是全章的**口径表**。网络性能里最贵的错误不是「算错了」，而是**把两种不同的延迟
当成一种**，或者把「带宽」当成「吞吐」。

原文 §10.1 给了 8 个词条，这里挑最容易出事的三个：

| 词 | 原文定义要点 | 容易踩的坑 |
|---|---|---|
| **Bandwidth** 带宽 | 该网络类型的最大数据传输速率，通常以 bit/s 计。"100 GbE" 指带宽 100 Gbit/s 的以太网 | **每个方向各有上限**：100 GbE 可以收 100 Gbit/s 同时发 100 Gbit/s，合计 200 Gbit/s 吞吐 |
| **Throughput** 吞吐 | 网络端点之间**当前**的数据传输速率，bit/s 或 byte/s | 拿 bandwidth 填 throughput，结论全是错的 |
| **Latency** 延迟 | 见下 | 延迟这个词在本章至少指六种不同的东西（§10.3.5） |

**Latency 的原文定义**（这一句是全章的口径基准）：

> Latency : Network latency can refer to the time it takes for a message to make a round-trip between endpoints, or the time required to establish a connection (e.g., TCP handshake), excluding the data transfer time that follows.

注意最后半句：**延迟被明确定义为不含数据传输时间**。也就是说「下载慢」不是延迟问题，
而是吞吐问题——两者的排查路径完全不同（延迟 → 看 RTT 与离群值；吞吐 → 看链路利用率与包长）。

另外两个容易混的词（原文 §10.1）：

- **interface port** 指物理网络连接器；**interface / link** 指操作系统看到并配置的那个
  逻辑实例。**不是所有 OS 接口都有硬件支撑**——有些是虚拟的（比如 loopback、veth）。
- **packet** 是包交换网络里的消息（如 IP 包）；**frame** 是物理网络层的消息（如以太网帧）；
  **socket** 是源自 BSD 的网络端点 API。

**判据**：报告「网络慢」之前，先在纸上写清楚这句话指的是**带宽不够**、**吞吐不够**，
还是**延迟太高**——三者的第一个动作完全不同。

## §10.2.3 协议栈：分层与消息的叫法

**机制**：网络通信由**协议栈**完成，每一层各司其职。原文那张图把**下层画得更宽**，
表示**协议封装**：发送时消息自应用向下走，接收时向上走。

**同一份消息在不同层的名字不同**（OSI 口径，原文 §10.2.3）：

| 层 | 消息名 |
|---|---|
| 传输层 | segment（TCP）或 datagram（UDP） |
| 网络层 | packet |
| 数据链路层 | frame |

> **（推演）** 这三个词在日志里混用会造成极具体的误判：网卡计数器里叫 frame、
> tcpdump 里叫 packet、`ss` 里叫 segment。读一份「掉了 3 万个包」的报告时，
> 必须先确认它说的是哪一层的计数。

**两条常被忽略的机制**：

1. **Ethernet 标准同时描述物理层**——铜缆还是光纤也在协议栈里。
2. **协议栈可以嵌套**：IPsec 或 Linux WireGuard 位于 IP 层**之上**，在两个 IP 端点之间
   提供安全；使用隧道（VXLAN）时，**一个协议栈会被封装进另一个**。

**（推演）** 隧道对本章所有延迟口径都有叠加效应：多一层封装就多一段排队与处理，
而 `ss` 看到的 RTT 与 `tcpdump` 抓到的外层包时刻**对不上**——这正是 §10.5.4 推荐
`SO_TIMESTAMPING` 的原因（它能同时标出发送时刻、往返时间与栈内时间）。

## §10.3.4 包长与 MTU：1500 从哪来

**机制**：包的大小直接决定性能——**包越大吞吐越高、包头开销越小**。

**具体数字（原文 §10.3.4）**：

- TCP/IP + 以太网的包可以是 **54 到 9,054 字节**，其中 54 字节是协议头（更多选项或
  不同版本会更大）。
- 包长通常受网卡 **MTU** 限制，多数以太网配 **1,500 字节**。
- **1500 这个数字的来历**：来自早期以太网版本，需要在**网卡缓冲内存成本**与
  **传输延迟**之间取平衡（原文引 [Nosachev 20]）。当年主机**共享介质**（同轴或集线器），
  包越大，等着轮到自己发的时间越长。

**jumbo frames**（超大帧）是这个限制的现代补丁：

> Ethernet now supports larger packets (frames) of up to approximately 9,000 bytes, termed jumbo frames . These can improve network throughput performance, as well as the latency of data transfers, by requiring fewer packets.

**为什么它没普及**——原文点了两个"合谋"因素：

1. **老硬件不支持**：要么用 IP 协议**分片**（重组有性能代价），要么回一个 ICMP
   "can't fragment" 错误让发送方降包长。
2. **配错的防火墙**：历史上出现过基于 ICMP 的攻击（包括 "ping of death"），
   一些管理员因此**屏蔽了全部 ICMP**。这样一来有用的 "can't fragment" 消息到不了发送方，
   于是**包长一超过 1500 就被静默丢弃**。就算 ICMP 到了并且分了片，也存在被不支持分片的
   设备丢掉的风险。

**结果**：很多系统就守着 1500 默认值不动。

**缓解路径**：网卡侧的 **TCP offload 与 large segment offload** 把更大的缓冲区交给网卡，
由**专用优化硬件**再切成小帧——这在很大程度上**缩小了 1500 与 9000 之间的性能差距**
（细节见 §10.4.3 分段卸载）。

**判据**：改 MTU 前必须**端到端**确认每一跳都支持（含所有中间防火墙）；
只要有一跳静默丢包，表现就是「小包正常、大包超时」——而抓包可能什么都看不到，
因为包根本没发出去。

## §10.3.5 延迟口径：六种延迟各测什么

这是本章**最该背下来的一节**。原文把延迟拆成六种，各自有不同的**责任方**：

| 延迟 | 测的是什么 | 谁负责 | 出问题时的典型现场 |
|---|---|---|---|
| **Name resolution** 名字解析 | 主机名 → IP 的时间（通常 DNS） | DNS 服务器 / 解析器 | 最坏可到**几十秒**（解析超时）；OS 常见有缓存 |
| **Ping latency** | ICMP echo 请求到响应 | 网络 + 两端内核栈 | 但**不等于应用 RTT**（见下） |
| **Connection latency** 连接建立 | 建立连接、尚未传数据 | 网络 + 两端内核栈 | TCP 下就是握手时间：客户端发 SYN 到收到 SYN-ACK |
| **First-byte latency** 首字节 | 连接建立 → 收到第一个数据字节 | **服务端** | 含 accept、调度线程、服务端处理 |
| **Round-trip time (RTT)** | 请求往返一次 | 网络 + 各跳处理 | 理想情况下由网络主导 |
| **Connection life span** 连接寿命 | 建立到关闭 | 应用 | keep-alive 策略延长它以摊薄建连成本 |

### Ping latency：一个必须打折的数字

> This is the time for an ICMP echo request to echo response, as measured by the ping(1) command. This time is used to measure network latency between hosts, including hops in between, and is measured as the time needed for a network request to make a round-trip. It is in common use because it is simple and often readily available: many operating systems will respond to ping by default. It may not exactly reflect the round-trip time of application requests, as ICMP may be handled with a different priority by routers.

最后一句是关键：**路由器可能给 ICMP 更低的优先级**，所以 ping 延迟可能**高于**应用请求的
真实 RTT。原文 §10.7.1 补了一句反向的情况：有些网络反而给 ICMP **更高**优先级，
让 ping 基准更好看。**结论：ping 是粗筛，不是判决。**

### Connection latency：握手里藏着重传

握手本身是三次交互，丢一个包就要等定时器超时再重传。原文特别点了 **SYN 包**：

- 如果服务器 **backlog 已满**，服务器会**丢弃 SYN**；
- 客户端于是**基于定时器重传 SYN**；
- 这发生在握手期间，所以 **connection latency 会包含重传延迟，可能增加一秒以上**。

所以「connect 偶尔要 3 秒」不是网络抖动，**是握手丢包 + 200ms 起步的 RTO 退避**（见 §10.4.1）。

### First-byte latency：这一段延迟**不归网络管**

> While ping and connection latency measures the latency incurred by the network, first-byte latency includes the think time of the target server. This may include latency if the server is overloaded and needs time to process the request (e.g., TCP backlog) and to schedule the server (CPU scheduler latency).

这一句是本节的**分水岭**：

- ping / connection latency = **网络 + 内核栈**的延迟；
- first-byte latency = 上面那些 **加上服务端的思考时间**，包括
  **TCP backlog 排队**与 **CPU 调度延迟**。

**（推演）** 所以用 first-byte latency 当「网络健康度」指标是**归因错误**：
它把应用慢、线程调度慢、backlog 满全算进了网络账上。原文也正是这样用的——
first-byte latency 更像「应用性能与当前负载」的度量。

**RTT**：原文的定义是「请求往返一次所需时间，含信号传播与每一跳的处理时间」，
**理想情况下 RTT 应由包在网络上的时间主导**，而不是远端主机处理请求的时间。
研究 RTT 时常用 ICMP echo，是因为远端处理时间极小。

**Connection life span**：有些协议用 **keep-alive** 策略延长连接寿命，让后续操作复用既有连接，
从而**省掉建连（以及 TLS 握手）的开销与延迟**。

**判据**：拿到一个延迟数字，先问三句——**① 测的是哪一种延迟？② 它含不含服务端 think time？
③ 是平均值还是分布？** 只报平均值的延迟报告基本没有诊断价值。

## §10.3.6 缓冲与 bufferbloat

**机制**：网络吞吐靠**发送端与接收端的缓冲**撑住。更大的缓冲可以**缓解高往返时间的伤害**——
在阻塞等待确认之前继续发数据。TCP 用缓冲 + **滑动发送窗口**提高吞吐；
socket 也有缓冲，应用自己还可能再缓冲一层。

**反直觉的核心**：缓冲**不总是越多越好**，尤其是**中间设备**的缓冲：

> Buffering can also be performed by external network components, such as switches and routers, in an effort to improve their own throughput. Unfortunately, the use of large buffers on these components can lead to bufferbloat , where packets are queued for long intervals. This causes TCP congestion avoidance on the hosts, which throttles performance. Features have been added to the Linux 3.x kernels to address this problem (including byte queue limits, the CoDel queueing discipline [Nichols 12] , and TCP small queues). There is also a website for discussing the issue [Bufferbloat 20] .

拆开这条链路（**这是 bufferbloat 的完整因果链**）：

```
中间设备缓冲过大 → 包在队列里排很久 → 端点看到「延迟升高」
  → TCP 判定为拥塞 → 拥塞窗口缩小 → 吞吐被压下去
```

**结果是「缓冲越大，吞吐越低、延迟越高」**——设备自己的吞吐确实上去了，
端到端的吞吐反而塌了。Linux 3.x 为此加了三样东西：**byte queue limits**、
**CoDel 队列管理算法**、**TCP small queues**（见 §10.4.3）。

**原文给出的原则**（end-to-end arguments，§10.3.6）：缓冲（尤其是大缓冲）的功能
**最好由端点（主机）来承担，而不是中间网络节点**。这条原则是后面 §10.4.3 里
TSQ / BQL / EDT 这一串机制的共同出发点。

**判据**：**延迟高 + 吞吐上不去 + 重传率正常** ⇒ 高度怀疑 bufferbloat，
而不是「网线不够长」或「服务器忙」。查法是看队列管理算法与队列长度上限
（§10.6 的 `sar` / 接口计数 + §10.8.1 的 qdisc 配置）。

## §10.3.7 连接积压：backlog 是过载信号

**机制**：TCP 实现了 **backlog**——SYN 请求可以先在内核里排队，等用户态进程来 accept。
当连接请求多到来不及被 accept，backlog 达到上限，**SYN 包被丢弃**，随后由**客户端重传**。
重传直接体现为**客户端 connect 时间变长**。

**限额是可调的**：它是 `listen(2)` 系统调用的一个参数，内核还可能提供**系统级上限**。

> Backlog drops and SYN retransmits are indicators of host overload.

这一句把「网络现象」翻译成了「主机现象」：**backlog 丢包与 SYN 重传是主机过载的指标**，
而不是网络链路质量差。丢的是内核队列，不是链路。

**判据**：SYN 重传率上升时，第一反应应当是「服务端 accept 不够快 / backlog 太小」，
而不是「网络抖动」。三处可调点见 §10.8.1（`tcp_max_syn_backlog`、`somaxconn`、
应用自己的 `listen()` backlog 参数）。

## §10.4.1 协议机制：TCP 的窗口、重传与拥塞控制

这一节是全章**技术密度最高**的部分，讲的是「TCP 为什么会慢、慢在哪一步」。

### IP 层：ECN 让路径能说话

IPv4 的 **Type of Service** 字段与 IPv6 的 **Traffic Class** 字段后来被重新定义为
**DSCP**（区分服务代码点，RFC 2474）+ **ECN**（显式拥塞通知，RFC 3168）。

- **DSCP** 用来支持不同的**服务等级**，每级有不同的**丢包概率**——电话、广播视频、
  低延迟数据、高吞吐数据、低优先级数据。
- **ECN** 允许路径上的服务器/路由器/交换机**在 IP 头里置一个位来显式表示拥塞**，
  而不是丢包。接收方把这个信号回显给发送方，发送方据此**降低发送速率**。
  **好处是拿到拥塞避免的好处而不承担丢包的代价**——前提是全网都正确使用 ECN 位。

### TCP 的性能特性清单（原文 §10.4.1 摘要）

| 特性 | 机制要点 |
|---|---|
| **Sliding window** 滑动窗口 | 见下 |
| **Congestion avoidance** 拥塞避免 | 防止发送过多导致饱和、丢包、性能崩溃 |
| **Slow-start** 慢启动 | 拥塞控制的一部分：从小窗口开始，**按时间收到 ACK 就扩大**，收不到就缩小 |
| **SACK** 选择性确认 | 允许确认**不连续**的数据块，减少需要的重传次数 |
| **Fast retransmit** 快重传 | 不等定时器，**根据重复 ACK 到达**立即重传丢包——**由 RTT 决定**，比慢得多的定时器快得多 |
| **Fast recovery** 快恢复 | 检测到重复 ACK 后**重置连接转入慢启动**，恢复性能 |
| **TCP fast open** | 客户端可在 SYN 里带数据，服务端不必等握手完成就能开始处理（RFC 7413），可用加密 cookie 认证客户端 |
| **TCP timestamps** | 已发包带时间戳、在 ACK 里回传，从而**测出 RTT**（RFC 1323） |
| **TCP SYN cookies** | 可能遭受 SYN flood（backlog 全满）时用加密 cookie 让合法客户端仍能连上，且服务端**不必为这些连接请求存额外数据** |

滑动窗口是吞吐的根基：

> Sliding window : This allows multiple packets up to the size of the window to be sent on the network before acknowledgments are received, providing high throughput even on high-latency networks. The size of the window is advertised by the receiver to indicate how many packets it is willing to receive at that time.

**关键**：窗口大小由**接收方通告**——它表达的是「我现在愿意收多少」。

### 三次握手与状态机

**机制**：连接由三次握手建立。一方**被动监听**（RFC 793 的术语，原文注明常见叫法是
`listen`），另一方**主动发起**（叫 `connect`）。在 C/S 模型里，**服务器 listen、客户端 connect**。
握手完成后会话进入 **ESTABLISHED** 状态。原文特别指出：**图里画的是最好情况**——
任何一个包被丢掉都会因超时重传而增加延迟。

**状态机**：LISTEN、SYN-SENT、SYN-RECEIVED、ESTABLISHED、FIN-WAIT-1、FIN-WAIT-2、
CLOSE-WAIT、CLOSING、LAST-ACK、**TIME-WAIT**、CLOSED。
性能分析通常只关心 **ESTABLISHED**（活跃连接，可能在传数据，也可能空闲等下一个事件）。

**两个定时器**：

- **TIME_WAIT 通常是 2 分钟**（有些内核如 Windows 允许调整）。它存在的目的是
  **不让迟到的包被误认为同端口上的新连接**——代价是**端口耗尽**（见 §10.5.7）。
- ESTABLISHED 上还有 **keep-alive 定时器**，通常设得很长（例如 2 小时），
  用来发探测包确认远端还活着。

### 重传：两种机制，代价差一个数量级

**机制一：定时器重传（timer-based）**。

> Timer-based retransmits : These occur when a time has passed and a packet acknowledgment has not yet been received. This time is the TCP retransmit timeout, calculated dynamically based on the connection round-trip time (RTT). On Linux, this will be at least 200 ms (TCP_RTO_MIN) for the first retransmit

（原文此行的脚注指出：Linux 这个 **200 ms** 的下限**似乎违反 RFC 6298**——后者规定
RTO 最小值应为**一秒**。）后续重传**慢得多**，走**指数退避**，超时时间翻倍。

**机制二：快重传（fast retransmit）**——重复 ACK 到达时立即重传，不必等定时器。

**TLP（Tail Loss Probe）**：解决「**最后一个包丢了**」的盲区——此时没有后续包来触发
重复 ACK。TLP 在最后一次发送后**短超时**再补发一个探测包，帮助检测丢包（[Dukkipati 13]）。

**机制代价对照**（**这是本节最该记住的数**）：

| 丢包类型 | 检测方式 | 延迟代价 |
|---|---|---|
| 中间包丢 | 3 个重复 ACK → 快重传 | 约 1 个 RTT |
| 最后一个包丢 | TLP 探测 | 约 1 个 RTT + 探测超时 |
| SYN 丢 / ACK 丢 | 只能靠定时器 | **≥ 200 ms，且每次翻倍** |

所以**「连接偶尔卡一下」和「稳定的高延迟」是两种不同的问题**：前者是定时器重传，
后者多半是排队。

### 拥塞控制算法

原文列举了这些，Linux 允许把算法作为**系统调优的一部分**来选择：

| 算法 | 机制 |
|---|---|
| **Reno** | 3 个重复 ACK 触发：拥塞窗口减半、慢启动阈值减半、快重传、快恢复 |
| **Tahoe** | 3 个重复 ACK 触发：快重传、慢启动阈值减半、拥塞窗口置为 1 个 MSS、进入慢启动（与 Reno 同为 4.3BSD 首发） |
| **CUBIC** | 用**三次函数**放大窗口，用 "hybrid start" 退出慢启动。比 Reno **更激进**，是 **Linux 默认** |
| **BBR** | **不用窗口**，而是用探测阶段**显式建模路径特征（RTT 与带宽）** |
| **DCTCP** | 数据中心 TCP：依赖交换机在**队列很浅时**就发出 ECN 标记，从而**迅速爬到可用带宽**（RFC 8257）。**不适合跨互联网部署**，但在配置良好的受控环境里能显著提升性能 |

BBR 的关键在于它的收益**不是无条件的**：

> BBR : Instead of window-based, BBR builds an explicit model of the network path characteristics (RTT and bandwidth) using probing phases. BBR can provide dramatically better performance on some network paths, while hurting performance on others.

**「换 BBR 就一定更快」是错的**。原文给的实证是：Netflix 云服务用 BBR，
**发现在重丢包期间吞吐能提升到三倍**（[Ather 17]）——注意前提是「重丢包期间」。

**可扩展性**：Linux 5.6（2020）支持**用 BPF 开发新的拥塞控制算法**（[Corbet 20]），
可以由最终用户定义并按需加载。

### Nagle 与 Delayed ACK：两个算法互相打架

- **Nagle**（RFC 896）：减少网络上的**小包**——**推迟发送**以让更多数据到达后合并。
  原文限定得很准：**只有在流水线里已有数据、且已经在经历延迟时才推迟**。
- **Delayed ACK**（RFC 1122）：**把 ACK 推迟最多 500 ms**，以便多个 ACK 合并，
  也顺带合并其他 TCP 控制消息。

**冲突**：Nagle 等数据、Delayed ACK 等 ACK → **互相等待**，经典的小包延迟问题。
两个算法**都可以被关掉**（系统参数或 socket 选项）：

- `TCP_NODELAY` → 关 Nagle，**尽快发段**（代价：网络利用率更高、包更多）；
- `TCP_QUICKACK` → **立即发 ACK**（原文注：可以提升发送带宽）。

### SACK / FACK / RACK

- **SACK** 让接收方告知「我收到了一段**不连续**的数据」。没有它，一次丢包会导致
  **整个发送窗口被重传**，以维持顺序确认——这严重伤害 TCP 性能。现代 OS 基本都支持。
- **FACK**（前向确认）是 SACK 的扩展，**Linux 默认支持**：它追踪额外状态，
  更好地调节网络中的**在途数据量**。
- **RACK**（Recent ACKnowledgment）更进一步：**用 ACK 携带的时间信息**做丢包检测与恢复，
  而不只看 ACK 序列。FreeBSD 上 Netflix 基于 RACK/TLP 重构了一套新的 TCP 栈（[Stewart 18]）。

### Initial Window：Linux 的默认值在慢链路上是错的

> The Linux default (10 packets, aka IW10) can be too high on slow links or when many connections start up; other operating systems default to 2 or 4 packets (IW2 or IW4).

机制：IW 是连接刚开始时、发送方在等待确认前会发出的**包数**。对**短连接**（典型 HTTP）
来说，**只要 IW 足够覆盖整个要发的数据，就能大幅缩短完成时间**。但更大的 IW
**有拥塞与丢包风险**，**多条流同时启动时更会被放大**。

**（推演）** 这条与「让第一包尽量带更多数据」是同一类问题：
**在长连接上是浪费，在短连接上是收益**——判断依据是连接寿命与单次请求的数据量，
不是包大小本身。

### UDP 与 QUIC

**UDP 提供三样东西**：**简单**（头小，计算与体积开销低）、**无状态**（连接与发送开销低）、
**无重传**（TCP 的重传会增加显著延迟）。代价是：**不可靠**——数据可能丢失或乱序，
因此不适合很多连接类型；而且 **UDP 没有拥塞避免**，**会加剧网络拥塞**。

**DNS 是 UDP 的大用户**。正因为 UDP 简单、无拥塞控制、且在互联网上通常不被防火墙拦，
现在有**新协议建在 UDP 之上**并自行实现拥塞控制等特性——**QUIC** 就是代表。

**QUIC**（Jim Roskind 设计，Google）提供：在同一「连接」上**多路复用多条应用流**、
可**按子流关闭**的 TCP 式可靠有序传输、客户端换地址时的**连接恢复**（基于连接 ID 的
加密认证）、**负载数据全加密**（含 QUIC 头）、以及 **0-RTT 握手**（对曾经通信过的对端）。
Chrome 浏览器大量使用 QUIC；IETF 正在标准化 QUIC 传输本身与 HTTP/3。

## §10.4.2 硬件：瓶颈可能不在网口

**接口**：物理网络接口收发 **frame**，负责电/光/无线信号处理与传输错误处理。
接口类型基于二层标准，各有最大带宽；**更高带宽的接口提供更低的数据传输延迟，代价更高**。
以太网选项包括铜缆与光纤，最大速率 1 / 10 / 40 / 100 / 200 / 400 GbE。

**控制器**：网卡为系统提供一个或多个网络端口，内含**网络控制器**——一颗在端口与
系统 I/O 传输层之间搬包**的微处理器**。**控制器与 I/O 传输层任一都可能成为吞吐或 IOPS 的限制**。

原文给的算例是本节最实用的部分：

> For example, a dual 10 GbE network interface card is connected to a four-channel PCI express (PCIe) Gen 2 slot. The card has a maximum send or receive bandwidth of 2 × 10 GbE = 20 Gbits/s, and bidirectional, 40 Gbit/s. The slot has a maximum bandwidth of 4 × 4 Gbits/s = 16 Gbits/s. Therefore, network throughput on both ports will be limited by PCIe Gen 2 bandwidth, and it will not be possible to drive them both at line rate at the same time (I know this from practice!).

**算术**：网卡侧单方向 20 Gbit/s、双向 40 Gbit/s；而 **PCIe Gen 2 四通道槽只有 16 Gbit/s**。
所以**两个端口不可能同时跑满线速**——瓶颈在**总线**，不在网口。原文作者还补了一句
「这是我从实践里知道的」。

**交换机与路由器**：

- 交换机为任意两台主机提供**专用通信路径**，允许多对主机同时通信而不互相干扰。
  它取代了**集线器**（以及更早的共享物理总线）——共享导致同时发送产生**冲突**，
  由 **CSMA/CD**（载波侦听多路访问/冲突检测）算法**指数退避并重传**直到成功，
  **负载下必然出性能问题**。交换机已经解决了这件事，但**有些可观测工具仍保留碰撞计数器**
  ——如今碰撞通常只由错误（协商失败或接线不良）引起。
- 路由器用协议与**路由表**决定投递路径。路由与路由表通常是**动态更新**的，
  以便自动应对网络与路由器故障、并做负载均衡。**结果是：在某个时刻没人能确定一个包实际走了哪条路**；
  存在多条路径时，**包可能乱序到达**，这会引起 TCP 性能问题。
- **路由器与交换机都有缓冲与微处理器**，**在负载下它们自己会成为瓶颈**。原文举例：
  某台早期 10 GbE 交换机因为 CPU 能力有限，**全部端口加起来只能跑 11 Gbit/s**。

**速率转换点**是另一个常被忽略的机制：

> Note that switches and routers are also often where rate transitions occur (switching from one bandwidth to another, e.g., a 10 Gbps link transitions to a 1 Gbps link). When this happens, some buffering is necessary to avoid excessive drops, but many switches and routers over-buffer (see the bufferbloat issue in Section 10.3.6 , Buffering ), leading to high latencies. Better queue management algorithms can help eliminate this problem, but not all network device vendors support them. Pacing at the source can also be a way to alleviate issues with rate transitions by making the traffic less bursty.

**关键**：10 Gbps → 1 Gbps 这种**降速点**必然要靠缓冲吸收，否则会大量丢包；
但**很多设备缓冲过度** ⇒ 高延迟（就是 §10.3.6 的 bufferbloat）。
更好的队列管理算法能解决，但**不是所有厂商都支持**——**源端 pacing**（把流量摊平）
是另一条缓解路径（对应 §10.4.3 的 Pacing）。

**防火墙**：按规则集放行通信以提升安全。**有状态**规则要为**每个见过的连接存元数据**，
连接很多时**内存压力很大**——可能是 **DoS 攻击**用海量连接淹没目标，也可能是
**大量出站连接**需要同样的连接跟踪。防火墙在性能测试时也是麻烦：要测带宽可能得
**改防火墙规则**（还要跟安全团队协调）。

**（原文的工业趋势）** 用**扩展 BPF 在通用硬件上实现防火墙**正在普及，
理由是性能、可编程性、易用性与最终成本——采用者包括 Facebook、Cloudflare、Cilium。

**判据**：怀疑网络硬件瓶颈时，**沿数据路径逐段比带宽**：网卡端口速率 → PCIe 通道带宽
→ 控制器 CPU → 交换机背板 → 路由器背板 → 防火墙会话表。**哪一段最小，哪一段就是天花板。**

## §10.4.3 软件：内核网络栈的六个抓手

**通用模型**：现代内核上网络栈是**多线程**的，**入站包可以由多个 CPU 处理**。
Linux 上网络栈是核心内核组件，**设备驱动是附加模块**；包在这些内核组件之间以
**`struct sk_buff`（socket buffer）** 数据类型传递。另外 **IP 层可能还有排队**
（用于包重组，原文说图里没画）。

### §10.4.3 连接队列：两个队列，不是一个

> Bursts of inbound connections are handled by using backlog queues. There are two such queues, one for incomplete connections while the TCP handshake completes (also known as the SYN backlog ), and one for established sessions waiting to be accepted by the application (also known as the listen backlog ). These are pictured in Figure 10.9 .

**两个队列的分工**：

- **SYN backlog**：握手未完成的半开连接；
- **listen backlog**：已建立、等应用来 accept 的会话。

**为什么要有两个**：早期内核**只有一个队列**，因此**易受 SYN flood 攻击**——
从伪造 IP 发出海量 SYN，把队列填满，让真实客户端连不上。
有了两个队列，**第一个可以当作潜在伪造连接的暂存区**，**只有连接建立后才提升到第二个**；
第一个可以**做得很长**以吸收 SYN flood，并且**只存最小必要的元数据**。

**SYN cookies 直接绕过第一个队列**——因为 cookie 本身已经证明了客户端是被授权的。

**两个队列的长度可以独立调**（见 §10.8.1），第二个还可以由应用通过 `listen(2)` 的
backlog 参数设置。

### §10.4.3 缓冲：socket 发送与接收缓冲

吞吐靠 socket 关联的**发送缓冲与接收缓冲**提升。**两者大小都可调**：
更大的缓冲提升吞吐，代价是**每连接占用更多主存**。发送与接收缓冲可以**设成不一样大**
（取决于服务器更偏发送还是接收）。**Linux 内核还会按连接活动动态调整这些缓冲的大小**，
并允许分别调**最小值、默认值、最大值**。

### §10.4.3 分段卸载：GSO / TSO / GRO

> Network devices and networks accept packet sizes up to a maximum segment size (MSS) that may be as small as 1500 bytes. To avoid the network stack overheads of sending many small packets, Linux uses generic segmentation offload (GSO) to send packets up to 64 Kbytes in size (“super packets”), which are split into MSS-sized segments just before delivery to the network device. If the NIC and driver support TCP segmentation offload (TSO), GSO leaves splitting to the device, improving network stack throughput.

**三者的分工（原文明确）**：

| 机制 | 在哪实现 | 干什么 |
|---|---|---|
| **GSO** 通用分段卸载 | **内核软件** | 发出最大 64 KB 的 "super packet"，临近交给网卡时才切成 MSS 大小的段 |
| **TSO** TCP 分段卸载 | **网卡硬件** | GSO 干脆不切，把切分留给设备（需要网卡与驱动支持） |
| **GRO** 通用接收卸载 | **内核软件** | GSO 的接收侧互补 |

**为什么重要**：MSS 可能只有 1500 字节，若在网络栈里逐包处理，每秒百万级的小包
意味着巨量栈开销。**GSO 把这份开销从「每包」摊薄到「每 super packet」**。
（原文脚注：UDP 的 GSO/GRO 支持是 Linux 2018 年才加的，QUIC 是关键用例。）

### §10.4.3 设备驱动与中断合并：延迟与吞吐的交换

网络设备驱动通常还有一层**环形缓冲**，在**内核内存与网卡之间**收发包。

**中断合并（interrupt coalescing）**是高速网络下越来越常见的能力：

> A performance feature that has become more common with high-speed networking is the use of interrupt coalescing mode . Instead of interrupting the kernel for every arrived packet, an interrupt is sent only when either a timer (polling) or a certain number of packets is reached. This reduces the rate at which the kernel communicates with the NIC, allowing larger transfers to be buffered, resulting in greater throughput, though at some cost in latency.

**交换关系写得很直白**：**吞吐更高，延迟更高**。这不是 bug，是设计取舍。

**NAPI** 是 Linux 的中断缓解框架：

> The Linux kernel uses a new API (NAPI) framework that uses an interrupt mitigation technique: for low packet rates, interrupts are used (processing is scheduled via a softirq); for high packet rates, interrupts are disabled, and polling is used to allow coalescing [Corbet 03] [Corbet 06b] . This provides low latency or high throughput, depending on the workload. Other features of NAPI include:

**读法**：**低包率用中断（延迟低）、高包率关中断改轮询（吞吐高）**——**同一个机制
按负载自动切换**。NAPI 的另外三项能力：

- **包限速（packet throttling）**：在网卡里**提前丢包**，防止系统被包风暴压垮；
- **接口调度（interface scheduling）**：用**配额**限制每个轮询周期处理的缓冲量，
  保证繁忙网卡之间的**公平**；
- **`SO_BUSY_POLL`** socket 选项支持：用户态应用可以**忙等**（自旋到事件发生）来降低接收延迟。

原文补充：**中断合并对虚拟机的网络尤其重要**，AWS EC2 的 **ena** 驱动就用了它。

### §10.4.3 NIC 收发：描述符不够就得停

- **发送**：NIC 被通知后通常用 **DMA** 从内核内存读取包（frame）。NIC 提供
  **发送描述符（transmit descriptor）**管理 DMA 包；**若 NIC 没有空闲描述符，
  网络栈会暂停发送**，等 NIC 跟上来。（原文脚注：**BQL 通常能防止描述符耗尽**，见下。）
- **接收**：NIC 用 DMA 把包放进**内核环形缓冲**，再用中断通知内核（**中断可以被忽略以允许合并**）。
  中断触发 **softirq**，把包交给网络栈处理。

### §10.4.3 CPU 扩展：把中断摊到多核

Linux 支持多种多 CPU 包处理方式（见内核文档 `Documentation/networking/scaling.txt`）：

| 机制 | 在哪实现 | 干什么 |
|---|---|---|
| **RSS** Receive Side Scaling | 网卡硬件 | 现代网卡支持多队列，把包**哈希**到不同队列，由不同 CPU 处理、直接中断它们。哈希可基于 **IP 地址与 TCP 端口**，使**同一连接的包落到同一 CPU** |
| **RPS** Receive Packet Steering | **软件** | RSS 的软件版，给不支持多队列的网卡用：一个短的 ISR 把入站包**映射到某个 CPU** |
| **RFS** Receive Flow Steering | 软件 | 类似 RPS，但**亲和到该 socket 上次被处理的 CPU**，以提高 **CPU 缓存命中率与内存局部性** |
| **ARFS** Accelerated RFS | **网卡硬件** | RFS 的硬件版：把流信息更新给网卡，**由网卡决定中断哪个 CPU** |
| **XPS** Transmit Packet Steering | 网卡硬件 | 支持多发送队列的网卡可由**多个 CPU** 向不同队列发送 |

不做负载均衡的后果：

> Without a CPU load-balancing strategy for network packets, a NIC may interrupt only one CPU, which can reach 100% utilization and become a bottleneck. This may show up as high softirq CPU time on a single CPU (e.g., using Linux mpstat(1): see Chapter 6 , CPUs , Section 6.6.3 , mpstat ). This may especially happen for load balancers or proxy servers (e.g., nginx), as their intended workload is a high rate of inbound packets.

**现场特征**：**单个 CPU 的 softirq 时间很高**（用 `mpstat` 看，见 [[Chapter 6. CPUs]]）。
**负载均衡器与代理服务器是重灾区**——它们的本职就是处理海量入站包。

**另一条路**：按**缓存一致性**等因素把中断映射到 CPU（即 RFS 的做法）能明显提升性能；
**`irqbalance` 进程**也能做 IRQ 线的分配。

### §10.4.3 内核旁路：性能换可观测性

**DPDK**：应用**绕过内核网络栈**，在**用户态实现自己的网络协议**，通过 DPDK 库向网络驱动
写入（配合 **UIO** 或 **VFIO** 驱动）。**拷贝包数据的开销可以避免**——直接访问网卡上的内存。

**XDP** 是另一条路径：它**不是旁路**，而是一条**可编程快路径**，用**扩展 BPF**
**整合进现有内核栈**。原文注明 **DPDK 现在也支持用 XDP 接收包**，把部分功能移回内核。

**旁路的代价，原文讲得很直接**：

> With kernel network stack bypass, instrumentation using traditional tools and metrics is not available because the counters and tracing events they use are also bypassed. This makes performance analysis more difficult.

**计数器与追踪事件跟着一块被旁路了**——所以旁路之后，**`ss` / `nstat` / `sar` / bpftrace 全瞎**。

**（推演）** 这是本章里**最该写进架构决策的一条权衡**：旁路买到的是数量级的包吞吐，
赔上的是**出事时没有证据**。做旁路方案前，先确认自己有一套**替代的观测手段**
（如网卡厂商计数器、DPDK 自己的统计、旁路嗅探器）。

**部分零拷贝**：即使不旁路全栈，也可以避免拷贝成本——
**`MSG_ZEROCOPY`** 的 `send(2)` 标志（Linux 4.14 起）与 **`mmap(2)` 零拷贝接收**。

### §10.4.3 其他优化：TSQ / BQL / EDT / Pacing

原文在 TCP 发送路径图里列了一串算法，其中四个此前没提过的：

| 算法 | 机制 | 版本 |
|---|---|---|
| **Pacing** | 控制**何时**发包，把发送摊开，**避免突发**伤害性能（可避免 TCP micro-burst 造成的排队延迟，甚至导致交换机丢包；也可缓解 **incast**——许多端点同时向一个目标发送） | — |
| **TCP Small Queues (TSQ)** | 限制（减少）网络栈排队的量，避免 **bufferbloat** 等问题 | — |
| **Byte Queue Limits (BQL)** | **自动**把驱动队列定得**足够大以避免饿死，又足够小以降低排队包的最大延迟**，并**避免耗尽网卡 TX 描述符**。做法是**必要时暂停往驱动队列加包** | Linux 3.3 |
| **Earliest Departure Time (EDT)** | 用**时间轮（timing wheel）而不是队列**来排序发给网卡的包，每个包按策略与速率配置打上时间戳 | Linux 4.20 |

**原文的总结句**：这些算法**常常组合使用**——一个 TCP 包在被送到网卡之前，
可能已经过了拥塞控制、TSO、TSQ、pacing 与队列管理这一整套。

**（推演）** 这张表就是「为什么不能逐个参数调优」的答案：它们**互相影响**，
调一个会改变另一个的前提。原文 §10.8 开篇就说了这件事（见本篇 §10.8）。

## §10.5 方法论：网络分析的顺序

原文 Table 10.2 把十种方法按「观测 / 调优 / 实验」分类：

| § | 方法 | 类型 |
|---|---|---|
| §10.5.1 | Tools method 工具法 | 观测分析 |
| §10.5.2 | USE method | 观测分析 |
| §10.5.3 | Workload characterization 工作负载特征 | 观测分析、容量规划 |
| §10.5.4 | Latency analysis 延迟分析 | 观测分析 |
| §10.5.5 | Performance monitoring 性能监控 | 观测分析、容量规划 |
| §10.5.6 | Packet sniffing 抓包 | 观测分析 |
| §10.5.7 | TCP analysis | 观测分析 |
| §10.5.8 | Static performance tuning 静态调优 | 观测分析、容量规划 |
| §10.5.9 | Resource controls 资源控制 | 调优 |
| §10.5.10 | Micro-benchmarking 微基准 | **实验分析** |

**作者给的起步顺序**（原文 §10.5 末尾）：**performance monitoring → USE method →
static performance tuning → workload characterization**。可单独用，也可组合用。

### §10.5.1 Tools Method：工具法及其盲区

**机制**：**迭代**可用的工具，检查它们提供的关键指标。

**它的问题（原文自己说的）**：**对工具提供不了可见性或可见性差的问题会视而不见**，
而且**执行起来耗时**。

**网络上的检查清单（原文 §10.5.1）**：

| 工具 | 看什么 |
|---|---|
| `nstat` / `netstat -s` | **高重传率与乱序包**。「多高算高」取决于客户端——**面向不可靠公网客户端的系统，重传率应当比同机房内部系统更高** |
| `ip -s link` / `netstat -i` | 接口错误计数：**errors、dropped、overruns** |
| `ss -tiepm` | 重要 socket 的 **limiter 标志**（看瓶颈是谁）与其他 socket 健康指标 |
| `nicstat` / `ip -s link` | 收发字节率。**高吞吐可能受协商速率限制，或被外部网络限速**，也会造成系统内网络用户之间的争用 |
| `tcplife` | 记录 TCP 会话的**进程信息、寿命、吞吐** |
| `tcptop` | 实时看排名靠前的 TCP 会话 |
| `tcpdump` | **CPU 与存储开销都很大**，但**短时间使用**有助于发现异常流量或协议头问题 |
| `perf(1)` / BCC / bpftrace | 检查应用与线路之间的包，**包括查看内核状态** |

**工具法的正确用法**：发现问题后，**把可用工具的所有字段都看一遍**以获得上下文——
**其他方法能发现不同类型的问题**。

### §10.5.2 USE Method：每个接口、每个方向、三个维度

USE 方法用于**快速定位瓶颈与错误**。对**每个网络接口**、**每个方向（TX 与 RX）**检查：

| 维度 | 网络上的具体口径 |
|---|---|
| **Utilization** 利用率 | 接口**忙于发送或接收 frame 的时间占比**。可算为**当前吞吐 ÷ 当前协商速率**（分方向）。**吞吐必须按字节/秒、且包含所有协议头** |
| **Saturation** 饱和度 | 因接口满载而产生的**额外排队、缓冲或阻塞**的程度 |
| **Errors** 错误 | 接收侧：坏校验和、帧过短（小于数据链路头）或过长、碰撞（交换网络里不太可能）；发送侧：晚碰撞（接线不良） |

**顺序有讲究**：

> Errors may be checked first, since they are typically quick to check and the easiest to interpret.

**三个关键补充**：

1. **利用率通常要自己算**——操作系统与监控工具一般不直接提供（**`nicstat` 是例外**）。
   对于实现了**网络带宽限制（资源控制）**的环境（某些云环境如此），
   **还要按施加的限制值再算一栏**，而不只是物理上限。
2. **饱和度最难测**。有些网络缓冲是**正常的**（应用发送速度可以远高于接口发送速度）。
   可行的度量是**应用线程阻塞在网络发送上的时间**（随饱和度上升而增加），
   以及更贴近接口饱和的内核统计，例如 Linux 的 **"overruns"**。
   原文还提到：**Linux 用 BQL 调节网卡队列大小，有助于避免 NIC 饱和**。
3. **TCP 层的重传**通常 readily available，可作饱和指标；但它**测的是服务器到客户端之间的
   整条路径**，**任何一跳的问题都可能造成它**。

**控制器也能用 USE**：因为控制器的可观测工具稀少，**可以用接口统计与拓扑反推**——
例如控制器 A 装着端口 A0 与 A1，**控制器吞吐 = A0 + A1 的吞吐之和**，
再配合已知最大吞吐即可算出控制器利用率。

### §10.5.3 Workload Characterization：先说清楚负载是什么

原文列出的**最基本**的三个特征：

| 特征 | 单位 |
|---|---|
| **网络接口吞吐** | RX 与 TX，字节/秒 |
| **网络接口 IOPS** | RX 与 TX，帧/秒 |
| **TCP 连接速率** | active（客户端 connect）与 passive（服务端 accept），连接/秒 |

**原文给的示例写法**（值得照抄这个格式）：

> The network throughput varies based on users and performs more writes (TX) than reads (RX). The peak write rate is 200 Mbytes/s and 210,000 packets/s, and the peak read rate is 10 Mbytes/s with 70,000 packets/s. The inbound (passive) TCP connection rate reaches 3,000 connections/s.

**注意它同时给了字节率与包率**——因为**包长会变**（§10.3.10 明说：只报包数的工具
**无法换算成字节数**，因而无法算吞吐或基于吞吐的利用率）。

**进阶清单（原文以问题形式给出）**：

- 平均包长是多少？RX 与 TX 各是？
- **每一层**的协议分布？传输层：TCP、UDP（**可能包含 QUIC**）。
- 哪些 TCP/UDP 端口在活动？字节/秒、连接/秒？
- **广播与组播**的包率？
- **哪些进程在用网络**？

**按接口分别表达**这些特征很有用：这样能确定**接口瓶颈**——
只要观察到吞吐已经**到达线速**。若存在**网络带宽限制（资源控制）**，
可能在**到达线速之前**就被限流掐住。

### §10.5.4 Latency Analysis：延迟要测分布，不是测平均

原文 Table 10.3 的完整延迟清单（本篇 §10.3.5 已讲过其中几种，这里补齐）：

| 延迟 | 说明 |
|---|---|
| Name resolution latency | 主机名解析为 IP 的时间，通常是 DNS——**性能问题的常见来源** |
| Ping latency | ICMP echo 请求到响应；测量**网络与两端内核栈**对包的处理 |
| TCP connection initialization latency | SYN 发出到 SYN-ACK 收到；**无应用参与**，测的是网络与内核栈，外加 TCP 会话的额外内核处理。**TCP Fast Open (TFO)** 可降低它 |
| TCP first-byte latency (TTFB) | 连接建立到客户端收到首个数据字节；**含 CPU 调度与应用 think time**，因此更像应用性能与当前负载的度量 |
| TCP retransmits | 一旦存在，**可能给网络 I/O 增加数千毫秒的延迟** |
| TCP TIME_WAIT latency | 本地关闭的 TCP 会话等待迟到包的这段时间 |
| Connection/session lifespan | 连接从初始化到关闭的时长；HTTP 等协议可用 keep-alive 保持空闲连接，避免重复建连的开销与延迟 |
| System call send/receive latency | socket 读写系统调用（`read(2)`、`write(2)`、`recv(2)`、`send(2)` 及其变体）的时间 |
| System call connect latency | 建连用的 connect；注意有些应用把它做成**非阻塞系统调用** |
| Network round-trip time | 请求往返一次的时间；**内核会用它做拥塞控制** |
| Interrupt latency | 从网络控制器为收到的包发出中断，到内核服务它的时间 |
| Inter-stack latency | 一个包穿过内核 TCP/IP 栈的时间 |

**呈现方式（原文给了三种）**：

- **按区间的平均值**——**最好按 client/server 对分组**，以隔离中间网络造成的差异；
- **完整分布**——直方图或热力图；
- **逐次操作的延迟**——列出每次事件的明细，**含源与目的 IP 地址**。

**常见问题源是重传造成的延迟离群值**：

> A common source of issues is the presence of latency outliers caused by TCP retransmits. These can be identified using full distributions or per-operation latency tracing, including by filtering for a minimum latency threshold.

**测量手段**：追踪工具，以及某些延迟可用的 socket 选项——Linux 上的
**`SO_TIMESTAMP`**（入包时间）与 **`SO_TIMESTAMPNS`**（纳秒分辨率）、
**`SO_TIMESTAMPING`**（每事件时间戳）。**`SO_TIMESTAMPING` 能识别发送延迟、
网络往返时间与栈内延迟**——分析**涉及隧道的复杂包延迟**时尤其有用。

**原文的一条纪律**：某些额外延迟来源是**瞬态的，只在系统负载下出现**——
所以**要同时测空闲系统与负载下的系统**。

### §10.5.7 TCP Analysis：五个具体行为 + 端口耗尽

除 §10.5.4 之外，原文列出还能查的具体 TCP 行为：

- socket 发送/接收**缓冲的使用情况**；
- **backlog 队列的使用情况**；
- **backlog 队列满导致的内核丢包**；
- **拥塞窗口大小**，**包括零窗口通告（zero-size advertisements）**；
- **TCP TIME_WAIT 期间收到的 SYN**。

最后一项是**可扩展性**问题，原文给完整推导：

> The last behavior can become a scalability problem when a server is connecting frequently to another on the same destination port, using the same source and destination IP addresses. The only distinguishing factor for each connection is the client source port—the ephemeral port —which for TCP is a 16-bit value and may be further constrained by operating system parameters (minimum and maximum). Combined with the TCP TIME_WAIT interval, which may be 60 seconds, a high rate of connections (more than 65,536 during 60 seconds) can encounter a clash for new connections. In this scenario, a SYN is sent while that ephemeral port is still associated with a previous TCP session that is in TIME_WAIT, and the new SYN may be rejected if it is misidentified as part of the old connection (a collision). To avoid this issue, the Linux kernel attempts to reuse or recycle connections quickly (which usually works well). The use of multiple IP addresses by the server is another possible solution, as is the SO_LINGER socket option with a low linger time.

**算术**：临时端口 **16 位 = 65,536 个**，TIME_WAIT **60 秒** ⇒
**60 秒内连接数超过 65,536 就会撞端口**，新 SYN 可能被误判成旧连接的一部分而**被拒**。

**三种解法**：**内核快速复用/回收连接**（Linux 默认、通常有效）、
**服务端使用多个 IP 地址**、**`SO_LINGER` 设一个很短的 linger 时间**。

### §10.5.10 Micro-Benchmarking：先用网络微基准确认网络本身

**机制**：网络微基准工具**比应用简单得多、调试也快得多**。用途是
**确认网络至少能达到期望的吞吐**——如果做不到，就先查网络，把网络调到目标速度后，
**再把注意力放回应用**。

**要测的因子**：**方向**（发/收）、**协议与端口**（TCP/UDP）、**线程数**、
**缓冲大小**、**接口 MTU**。

**具体数字**：**100 Gbit/s 这类快速网卡，可能需要多个客户端线程才能被驱动到最大带宽**。

## §10.6 可观测工具：按问题选工具

原文 §10.6 把工具分成三档：

1. **传统统计工具**：`ss`、`ip`、`nstat`、`netstat`、`sar`、`nicstat`、`ethtool`；
2. **追踪工具**（BPF 系，BCC 与 bpftrace 前端）：`socketio`、`tcplife`、`tcptop`、`tcpretrans`；
3. **抓包工具**：`tcpdump`、`Wireshark`。

**工具的「出身」会影响它的鲜度**（原文 §10.6 末）：

> The first statistical tools covered are ss(8), ip(8), and nstat(8), as these are from the iproute2 package that is maintained by the network kernel engineers. Tools from this package are most likely to support the latest Linux kernel features. Similar tools from the net-tools package, namely ifconfig(8) and netstat(8), are also covered as they are in widespread use, although Linux kernel network engineers consider these deprecated.

**结论**：**优先 `ss` / `ip` / `nstat`（iproute2）**；`ifconfig` / `netstat` 虽广泛使用，
但**内核网络工程师视其为过时**。

### §10.6.1 ss：socket 状态快照与内部统计

默认输出是**当前状态快照**（协议、状态、Recv-Q、Send-Q、本地与对端地址），
可以用来**刻画当前工作负载**：有多少客户端连接、对某个依赖服务有多少并发连接。

加选项能看更多：**`-t`** 只看 TCP、**`-i`** TCP 内部信息、**`-e`** 扩展 socket 信息、
**`-p`** 进程信息、**`-m`** 内存用量。原文给的例子（照抄）：

```bash
# ss -tiepm
State     Recv-Q  Send-Q    Local Address:Port      Peer Address:Port

ESTAB     0       0         100.85.142.69:65264     100.82.166.11:6001
 users:(("java",pid=4195,fd=10865)) uid:33 ino:2009918 sk:78 <->
         skmem:(r0,rb12582912,t0,tb12582912,f266240,w0,o0,bl0,d0) ts sack bbr ws
cale:9,9 rto:204 rtt:0.159/0.009 ato:40 mss:1448 pmtu:1500 rcvmss:1448 advmss:14
48 cwnd:152 bytes_acked:347681 bytes_received:1798733 segs_out:582 segs_in:1397
data_segs_out:294 data_segs_in:1318 bbr:(bw:328.6Mbps,mrtt:0.149,pacing_gain:2.8
8672,cwnd_gain:2.88672) send 11074.0Mbps lastsnd:1696 lastrcv:1660 lastack:1660
pacing_rate 2422.4Mbps delivery_rate 328.6Mbps app_limited busy:16ms rcv_rtt:39.
822 rcv_space:84867 rcv_ssthresh:3609062 minrtt:0.139
[...]
```

**必须会读的几个字段**（原文逐项解释）：

| 字段 | 含义 | 怎么用 |
|---|---|---|
| `rto:204` | TCP 重传超时 **204 毫秒** | 与 §10.4.1 的 200 ms 下限对上 |
| `rtt:0.159/0.009` | 平均往返时间 0.159 ms，平均偏差 0.009 ms | 偏差小 = 网络稳定 |
| `mss:1448` | 最大段大小 1448 字节 | 与 MTU 1500 差 52 = TCP + IP 头 |
| `cwnd:152` | 拥塞窗口 **152 × MSS** | 拥塞控制的状态 |
| `bytes_acked` / `bytes_received` | 成功发出 340 KB / 收到 1.72 MB | 配合连接寿命算吞吐 |
| `pacing_rate 2422.4Mbps` | pacing 速率 | 远高于实际 delivery_rate，说明是应用受限 |
| `minrtt:0.139` | 最小往返时间（毫秒） | 与平均值和平均偏差对比，看网络波动与拥塞 |
| `app_limited` | **拥塞窗口没被用满 ⇒ 连接是应用受限的** | 关键归因字段 |

**`ss` 能打印的三个「受限」标志**：

- `app_limited`：**应用受限**；
- `rwnd_limited:Xms`：**受接收窗口限制**（含受限毫秒数）；
- `sndbuf_limited:Xms`：**受发送缓冲限制**（含受限毫秒数）。

**原文指出一个缺的信息**：输出里**没有连接的年龄**，而**算平均吞吐需要它**。
**作者给的变通办法**：对 `/proc/<pid>/fd/<fd>` 这个文件描述符用 `stat(1)`，
用它的**变更时间戳**推算连接年龄。

**netlink vs /proc/net（数据来源之争）**：

- `ss` 通过 **netlink(7)** 接口读取扩展细节——它经由 **AF_NETLINK** 族的 socket 从内核取信息；
- `netstat` 则读 **`/proc/net` 下的文件**。

原文的判断很直接：`/proc/net` 文件是**文本**，**做临时统计用 awk 就够了**，
但**正式的监控工具应该用 netlink(7)**——因为它**传二进制格式，避免了文本解析的开销**。

### §10.6.9 tcplife：用状态变更事件替代抓包

**机制**：追 TCP **socket 状态变更事件**，在状态变为 **TCP_CLOSE** 时打印汇总
（时长、地址、吞吐，尽可能带责任进程的 PID 与名字）。

**为什么它便宜**（原文明确）：

> This tool works by tracing TCP socket state-change events, and prints the summary details when the state changes to TCP_CLOSE. These state-change events are much less frequent than packets, making this approach much less costly in overhead than per-packet sniffers. This has made tcplife(8) acceptable to run continuously as a TCP flow logger on Netflix production servers.

**状态变更事件比包稀疏得多 ⇒ 开销远低于逐包嗅探器 ⇒ 可以在生产服务器上常驻**。
（原文脚注说它与「所有打开会话的快照」耦合。）

### §10.6.11 tcpretrans：内核里的重传，比抓包看得更清

**机制**：追内核里的 TCP 重传事件，打印 IP 地址与端口、**以及 TCP 状态**。

**开销可忽略**——重传本来就不该频繁。

**与「抓包后离线找重传」相比，原文给了两条理由**：

> Packet-capture can only see details that are on the wire, whereas tcpretrans(8) prints the TCP state directly from the kernel, and can be enhanced to print more kernel state if needed.

**抓包只能看到线路上的东西，而 tcpretrans 直接从内核打印 TCP 状态**，
而且可以增强打印更多内核状态。

**读法（原文）**：**ESTABLISHED 状态下的高重传率 ⇒ 指向外部网络问题**；
**SYN_SENT 状态下的高重传率 ⇒ 指向服务端应用没能足够快地消费 SYN backlog（即服务端过载）**。

### §10.6.13 tcpdump：抓包很贵，所以要省着用

**开销来源**：CPU 与存储。原文解释了**为什么**开销敏感：

> Network kernel code paths are typically cycle-optimized, since they need to handle up to millions of packets per second and are sensitive to any extra overhead.

**内核网络路径是按周期优化的**——它要处理每秒百万级包，对任何额外开销都敏感。

**两条降开销的路**：

1. **环形缓冲**：内核用 ring buffer 通过共享内存映射把包数据传给用户态追踪工具——
   例如用 BPF 配合 `perf(1)` 的 **output ring buffer**，以及 **AF_XDP**。
2. **带外嗅探器**：**另一台服务器**接到交换机的 **"tap" 或 "mirror" 端口**上。
   公有云厂商（Amazon、Google）把它作为**服务**提供。

**抓包的常规形态**：先抓到文件，再做各种分析。日志可以含每包的**时间戳**、
**完整包**（所有协议头 + 部分或全部负载）、**元数据（包数、丢包数）**、**接口名**。

**内核侧过滤**：抓包实现通常允许用户提供**过滤表达式**并在**内核里执行**——
避免把不需要的包传到用户态。过滤表达式通常用 **BPF** 优化，
**编译成 BPF 字节码并由内核 JIT 成机器码**。

**丢包计数**：因为抓包是 CPU 密集活动，多数实现在过载时**选择丢弃事件而不是捕获它们**，
**丢弃的包数可能出现在日志里**——看到这个数字就说明**抓包数据不完整**。

## §10.8 调优：参数是最后一步

原文 §10.8 开篇给了一条**顺序上的纪律**：

> Before trying tunable parameters, it can be worthwhile to first understand network usage. This may also identify unnecessary work that can be eliminated, leading to much greater performance wins. Try the workload characterization and static performance tuning methodologies using the tools in the previous section.

**先理解网络的使用方式**，这**可能直接暴露出可以消除的不必要工作**，收益远大于调参。
而且原文明确说了两件事：**网络 tunable 参数通常已经被调过以提供高性能**，
**网络栈本身也通常被设计成能自适应不同负载**。

**所以调参不是起点，是最后一步。**

### §10.8.1 System-Wide：sysctl 与 Netflix 的 14 行

**查看与设置**：用 `sysctl(8)`，写进 `/etc/sysctl.conf`；
也可以通过 `/proc/sys/net` 读写。原文给的例子：

```bash
# sysctl -a | grep tcp
net.ipv4.tcp_abort_on_overflow = 0
net.ipv4.tcp_adv_win_scale = 1
net.ipv4.tcp_allowed_congestion_control = reno cubic
net.ipv4.tcp_app_win = 31
net.ipv4.tcp_autocorking = 1
net.ipv4.tcp_available_congestion_control = reno cubic
net.ipv4.tcp_available_ulp =
net.ipv4.tcp_base_mss = 1024
net.ipv4.tcp_challenge_ack_limit = 1000
net.ipv4.tcp_comp_sack_delay_ns = 1000000
net.ipv4.tcp_comp_sack_nr = 44
net.ipv4.tcp_congestion_control = cubic
net.ipv4.tcp_dsack = 1
[...]
```

原文注明：这个内核（5.3）上**含 "tcp" 的参数有 70 个**，此外 `net.` 下还有更多
（IP、以太网、路由、网络接口）。

**生产实例（Netflix 的云实例调优，照抄）**：

```bash
net.core.default_qdisc = fq
net.core.netdev_max_backlog = 5000
net.core.rmem_max = 16777216
net.core.somaxconn = 1024
net.core.wmem_max = 16777216
net.ipv4.ip_local_port_range = 10240 65535
net.ipv4.tcp_abort_on_overflow = 1
net.ipv4.tcp_congestion_control = bbr
net.ipv4.tcp_max_syn_backlog = 8192
net.ipv4.tcp_rmem = 4096 12582912 16777216
net.ipv4.tcp_slow_start_after_idle = 0
net.ipv4.tcp_syn_retries = 2
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_wmem = 4096 12582912 16777216
```

**原文自己加的限定**：这只设了 **14 个**参数，**是某一时点的示例，不是配方**；
Netflix 在 2020 年正考虑改其中两个（`netdev_max_backlog` 改 1000、`somaxconn` 改 4096），
**待非回归测试**。（原文脚注说这两个新值 Google 已在用。）

**逐项要点**：

| 项 | 原文要点 |
|---|---|
| **socket 缓冲上限** | `rmem_max` / `wmem_max`，单位字节。**可能要设到 16 MB 或更高才能支撑满速 10 GbE 连接** |
| **TCP 接收缓冲自动调节** | `net.ipv4.tcp_moderate_rcvbuf = 1` |
| **TCP 读写缓冲三档** | `tcp_rmem` / `tcp_wmem` = **最小、默认、最大**三个值。**实际大小从默认值自动调节**。想提吞吐**先调最大值**；**调最小值与默认值会每连接多占内存，通常没必要** |
| **TCP backlog** | `tcp_max_syn_backlog = 4096`（第一个队列，半开连接）、`somaxconn = 1024`（第二个队列，交给 `accept(2)`）。**两者都常需要从默认值调高**以应对突发负载 |
| **设备 backlog** | `net.core.netdev_max_backlog = 10000`，**每 CPU** 的网络设备队列长度。**10 GbE NIC 可能需要调到 10,000** |
| **拥塞控制** | `modprobe tcp_htcp` 加载模块（可能可用但未加载），`tcp_congestion_control` 选当前算法。**可按 socket 用 `TCP_CONGESTION` 覆盖** |
| **TCP 选项** | `tcp_sack`、`tcp_fack`（**高延迟网络下提升吞吐，代价是一些 CPU 负载**）、`tcp_tw_reuse`、`tcp_tw_recycle`（后者**不如前者安全**） |
| **ECN** | `tcp_ecn`：**0 禁用 / 1 允许入站并对出站请求 ECN / 2 允许入站且不对出站请求**，**默认是 2**。另有 `tcp_ecn_fallback`（默认 1）——**内核检测到对端行为不当时对该连接禁用 ECN** |
| **Byte Queue Limits** | 通过 `/sys` 调：`.../net/ens5/queues/tx-0/byte_queue_limits/limit{,_min,_max}`。**limit 值由自动调节产生**；要控制它就设 `limit_min` 与 `limit_max` 来**钳住允许区间** |
| **资源控制** | cgroups 的 **net_prio**（给进程/进程组的出站流量打优先级）、**net_cls**（给包打 class ID，供 qdisc 做包/带宽限制、供 BPF 程序使用）。BPF 还可用 **cgroup v2 ID** 做容器感知，并把**分类、测量、重标记移到 tc egress hook**，**缓解 root qdisc 锁的压力**（[Fomichev 20]） |
| **队列管理（qdisc）** | 内核默认 **`pfifo_fast`**，而 **systemd 更保守地设为 `fq_codel`** 以降低 bufferbloat 风险，**代价是 qdisc 层复杂度略高**。很多 Linux 发行版已默认 `fq_codel` |
| **Tuned 项目** | 自动调优，**28 个 profile**，覆盖 RHEL / Fedora / Ubuntu / CentOS |

**Tuned 的两个网络 profile（照抄）**：

```bash
# tuned-adm list
Available profiles:
[...]
- balanced                    - General non-specialized tuned profile
[...]
- network-latency             - Optimize for deterministic performance at the cost of
increased power consumption, focused on low latency network performance
- network-throughput          - Optimize for streaming network throughput, generally
only necessary on older CPUs or 40G+ networks
[...]
```

**看某个 profile 到底设了什么**（读它的配置文件）：

```bash
$ more tuned/profiles/network-latency/tuned.conf
[...]
[main]
summary=Optimize for deterministic performance at the cost of increased power
consumption, focused on low latency network performance
include=latency-performance

[vm]
transparent_hugepages=never

[sysctl]
net.core.busy_read=50
net.core.busy_poll=50
net.ipv4.tcp_fastopen=3
kernel.numa_balancing=0

[bootloader]
cmdline_network_latency=skew_tick=1
```

注意它有 **include 指令**，把 **latency-performance** profile 的 tunable 一并带入。
**（推演）** 这份配置本身就是本章的一条交叉验证：`busy_read` / `busy_poll`
对应 §10.4.3 的 NAPI 与 `SO_BUSY_POLL`，`tcp_fastopen=3` 对应 §10.4.1 的 TCP fast open，
`skew_tick=1` 对应第 3 章的时钟与调度抖动。

### §10.8.2 Socket Options：按 socket 调

应用可以通过 `setsockopt(2)` **逐个 socket 调优**——但前提是**你能改代码或重编译**。

**原文 Table 10.8（照抄）**：

| 选项 | 说明 |
|---|---|
| `SO_SNDBUF`, `SO_RCVBUF` | 发送与接收缓冲大小（可调到前面说的系统上限；另有 **`SO_SNDBUFFORCE`** 可越过发送上限） |
| `SO_REUSEPORT` | 允许多个进程或线程绑定同一端口，**由内核把负载分给它们**（Linux 3.9 起） |
| `SO_MAX_PACING_RATE` | 设定最大 pacing 速率（字节/秒，见 `tc-fq(8)`） |
| `SO_LINGER` | 可用于**缩短 TIME_WAIT 延迟** |
| `SO_TXTIME` | 请求基于时间的包发送，可提供截止时间（Linux 4.19 起），用于 UDP pacing |
| `TCP_NODELAY` | **关闭 Nagle**，尽快发送报文段。**可能改善延迟，代价是更高的网络利用率（更多包）** |
| `TCP_CORK` | **暂停发送**直到能发满包，**改善吞吐**（另有系统级 `net.ipv4.tcp_autocorking` 让内核自动尝试 corking） |
| `TCP_QUICKACK` | **立即发送 ACK**（可以提升发送带宽） |
| `TCP_CONGESTION` | 该 socket 的拥塞控制算法 |

**I/O 系统调用标志也能影响性能**：**`MSG_ZEROCOPY`**（Linux 4.14 加进 `send(2)`）
**允许用户态缓冲区在传输期间直接被使用**，避免把它拷进内核空间。
**原文脚注给了关键警告**：用 `MSG_ZEROCOPY` **不是设个标志那么简单**——
**`send(2)` 可能在数据真正发出之前就返回**，
所以**发送方应用必须等内核通知**，才知道什么时候可以释放或复用那块缓冲区内存。

**（推演）** 这是本篇唯一一处「**用错就是 use-after-free**」的 API：
把 `MSG_ZEROCOPY` 当成「免费的零拷贝」用，会直接踩内存生命周期 bug。

### §10.8.3 配置层（原文只列了三条）

- **以太网 jumbo frames**：把默认 MTU 从 1,500 提到 ~9,000，**前提是网络基础设施支持**；
- **链路聚合（link aggregation）**：多个网口组合成一个、用合并带宽，**需要交换机支持与正确配置**；
- **防火墙配置**：例如 `iptables` 或 egress hook 上的 BPF 程序，按防火墙规则设置 IP 头的
  **ToS（DSCP）**，可**按端口给流量定优先级**。

## 章末选型判据

| 场景 | 做什么 | 别做什么 |
|---|---|---|
| 有人说「网络慢」，你还没定位 | 先分清是**带宽不够**、**吞吐不够**、**延迟太高**；延迟再分**哪一种延迟**、含不含服务端 think time | 拿 ping 值直接当应用 RTT；拿带宽数字当吞吐 |
| 要证明「不是网络的问题」 | 跑 §10.5 的起步顺序：performance monitoring → USE → static tuning → workload characterization；用接口统计 + `ss` + 重传率 | 直接去改内核参数 |
| 接口看起来没满，但吞吐上不去 | 沿数据路径逐段比带宽：网卡端口 → **PCIe 通道** → 控制器 CPU → 交换机背板 → 防火墙会话表 | 只盯着网口协商速率 |
| 延迟高 + 吞吐低 + 重传率正常 | 怀疑 **bufferbloat**：查中间设备的队列管理与缓冲长度、源端 pacing、`fq_codel` / CoDel / BQL | 加更多缓冲（越加越糟） |
| SYN 重传率上升 | 判为**主机过载**：查 accept 速度与 backlog 大小（`somaxconn`、`tcp_max_syn_backlog`、应用 `listen()` 参数） | 去查链路质量 |
| connect 偶尔卡 3 秒 | 判为**握手丢包 + 定时器重传**：RTO 起步 200 ms 且指数退避 | 当成网络抖动 |
| 短连接多、每次数据量小 | 关注 **Initial Window**（Linux 默认 IW10 在慢链路 / 多连接同时起跑时偏高），并考虑 TCP fast open | 只调 `tcp_rmem` |
| 高延迟链路上 TCP 跑不满 | 关注**滑动窗口**（窗口由接收方通告）、SACK / FACK、缓冲上限（10 GbE 需 16 MB 级） | 只加带宽 |
| Nagle 与 delayed ACK 互相拖慢 | `TCP_NODELAY`（发）与 `TCP_QUICKACK`（收）成对考虑 | 单开一个 |
| 换拥塞控制算法 | BBR 在**重丢包**路径上收益大，但**在某些路径上会更差**；CUBIC 是 Linux 默认。用 `TCP_CONGESTION` **按 socket 试**，别全局一刀切 | 凭「BBR 更快」的印象全局替换 |
| 服务端主动外连打不出去 | 查 **TIME_WAIT 与 16 位临时端口**：60 秒内超过 65,536 次连接会撞端口 | 只加大 backlog |
| 单核 softirq 打满 | 上 **RSS / RPS / RFS / XPS** 做 CPU 负载均衡；检查 `irqbalance` | 以为要加网卡 |
| 想用内核旁路（DPDK）换吞吐 | 先确认**可观测性**的替代方案：旁路会让 `ss` / `nstat` / `sar` / bpftrace 的计数器与事件**一起失效** | 旁路后再想办法取证 |
| 抓包很贵 | 短时间使用；内核侧 BPF 过滤（JIT 字节码）；环形缓冲 / AF_XDP；带外嗅探器；**检查日志里的丢包计数** | 长时间全量抓包 |
| 要写自己的监控工具 | 用 **netlink(7)**（二进制、免文本解析），不要长期读 `/proc/net`（文本） | 用 `netstat` / `ifconfig`（内核工程师视为过时） |
| 想在生产机常驻看 TCP 会话 | 用 **tcplife**（追状态变更事件，比逐包便宜得多）；用 **tcpretrans** 看重传并直接读内核 TCP 状态 | 靠抓包离线找重传 |
| 准备调参 | **先**做工作负载特征与静态调优——常常能发现**可消除的不必要工作**，收益大于调参 | 直接抄 sysctl 参数 |
| 抄 Netflix 的 sysctl | 只当**某一时点的示例**（原文原话）；先按自己的工作负载与链路条件重算 | 当配方直接照搬 |
| 想改 MTU 到 9000 | 端到端确认每一跳（含防火墙）都支持；改完确认不是「小包正常、大包超时」 | 只改一端就上 |






