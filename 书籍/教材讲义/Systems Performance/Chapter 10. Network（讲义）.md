---
title: Chapter 10. Network（讲义）
category: 性能工程
type: 讲义
audience: 人
tags:
  - 网络延迟
  - TCP
  - 拥塞控制
  - 可观测性
  - 性能工程
source: "《Systems Performance》（Brendan Gregg，2018，英文原版）第10章 Network"
companion: "[[10-Chapter 10. Network]]"
---

# 第 10 章 讲义：网络总被冤枉，因为「延迟」不是一个数

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[10-Chapter 10. Network|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：本仓库**是一个纯 TCP 的单线程 epoll reactor**——`src/net/tcp_transport.cpp` 的 `TcpTransport::Poll()` 是唯一的 IO 等待点，`src/net/transport.h` 的文件头用 40 行写清了「为什么是 TCP、迁 UDP 要付什么代价」。本章的落点有三个：**`TCP_NODELAY` 必设**（Nagle 会把小包攒 0.5 秒）、**发送缓冲不是环形缓冲**（`c.out.resize(old + n)` + `memcpy`）、**观测面只有应用内埋点**（`src/net/metrics.h` 没有 `rxdrop/s`、没有 `retrans/s`）。
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原章。

---

## 0. 开局：0.05 毫秒到 183 毫秒，中间隔着 3600 倍

原书 §10.3.5 给了 Table 10.1。前面几列是真实的 ping 延迟，最后一列 "Scaled" 是**假设本地 ping 延迟是 1 秒**时等比例放大后的结果：

| From | To | Via | Latency | Scaled |
|---|---|---|---|---|
| Localhost | Localhost | Kernel | 0.05 ms | 1 s |
| Host | Host (same subnet) | 10 GbE | 0.2 ms | 4 s |
| Host | Host (same subnet) | 1 GbE | 0.6 ms | 12 s |
| Host | Host (same subnet) | Wi-Fi | 3 ms | 1 minute |
| San Francisco | New York | Internet | 40 ms | 13 minutes |
| San Francisco | United Kingdom | Internet | 81 ms | 27 minutes |
| San Francisco | Australia | Internet | 183 ms | 1 hour |

同一句「网络很慢」，在这张表里可以差 **3600 倍**。
所以「网络很慢」这三个字**本身没有信息量**。它不告诉你慢在哪一段——是网卡排队？是交换机过载？是 RTT 太高导致 TCP 窗口撑不满？是重传？还是服务端自己的处理时间？

这一章真正在做的事情，就是**把「延迟」这个词拆开**：

> Latency is an important metric for network performance and can be measured in different ways, including name resolution latency, ping latency, connection latency, first-byte latency, round-trip time, and connection life span.

**六种延迟，六种完全不同的排查路径。** 名字解析慢 → DNS/缓存问题；ping 慢 → 路径问题；连接慢 → backlog / SYN 重传；首字节慢 → 服务端处理慢（**这一种跟网络基本无关**）；RTT 高 → 带宽延迟积；连接寿命短 → 端口耗尽 / TIME_WAIT。

（推演）这张表最实用的一条推论是：**先定位「哪一段」，再动手。** 同样是「客户端觉得卡」，如果 ping 正常而首字节慢，那把网络设备全换一遍也是白花钱——问题在服务器。
本仓库也有一个形状相同的困境：它**没有** ping、没有抓包、没有 iperf，`src/net/metrics.h` 里也没有任何「网络延迟」指标。那么当玩家说「客户端卡」时，服务器侧能看见什么？答案是：**只能看见 tick 漂移**（`tick_drift_max_ms`），看不见任何网络层事实。本章最后一节会把这条缺口写清楚。

---

## 1. §10.1 Terminology：先把词钉死

网络术语最容易被误用的地方，是**同一个词在不同层指的不是同一段时间 / 同一个东西**：

| 词 | 原文的定义要点 |
|---|---|
| **Interface port** | 物理网络连接器。**Interface / link** 指 OS 看到和配置的逻辑实例。⚠️ 并非所有 OS interface 都有硬件支撑——**有些是虚拟的** |
| **Packet** | 包交换网络里的一条消息，比如 IP 包 |
| **Frame** | 物理网络层的一条消息，比如以太网帧 |
| **Socket** | 源自 BSD 的网络端点 API |
| **Bandwidth** | 该网络类型的**最大**数据传输率。⚠️ **每个方向可能有各自的带宽上限**：100 GbE 可以 100 Gbit/s 发 + 100 Gbit/s 收并行（合计 200 Gbit/s） |
| **Throughput** | 当前实际的数据传输率 |
| **Latency** | 可能指**往返**时间，也可能指**建立连接**的时间（比如 TCP 握手），**不含其后的数据传输时间** |

**Bandwidth vs Throughput 是全章第一组必须分清的词**：一个是上限，一个是现状。原文的整章逻辑都建立在「利用率 = Throughput / Bandwidth」这个比值上（§10.3.10）。

---

## 2. §10.2 Models：接口、控制器、协议栈

- **§10.2.1 Network Interface**：网络接口是 **OS 侧的网络端点**，一个由系统管理员配置和管理的抽象。它映射到物理端口；端口通常有**独立的发送与接收通道**。
- **§10.2.2 Controller**：网卡（NIC）给系统提供一到多个网络端口，板上住着一枚**网络控制器**——在端口与系统 IO 传输通道之间搬运包的微处理器。通常是独立扩展卡，或集成在主板上。
- **§10.2.3 Protocol Stack**：网络由**协议栈**实现，每层各司其职。下层画得更宽表示**协议封装**：发出的消息从应用往下走，收到的消息往上走。⚠️ 两个常被忽略的变化：**安全层可以插在 IP 之上**（IPsec、WireGuard）；**隧道会让一个协议栈套进另一个**（如 VXLAN）。

同一个「消息」，在三层有三个名字：

> at the transport layer a message is a segment or datagram ; at the network layer a message is a packet ; and at the data link layer a message is a frame .

**跨层沟通时说不清是哪一层的「包」，就会吵起来。**

---

## 3. §10.3.1–10.3.4 网络、协议、封装、包大小

**§10.3.1 Networks and Routing**：把全球做成**一个大网络**是不 desirable 的，理由是**可扩展性**——拆成子网后，**广播消息被限制在本地**（否则规模一大就是广播风暴），**常规消息只走源到目的之间的那些网**。**路由**管理包在这些网络之间的投递，所需的地址信息装在 **IP 头**里。⚠️ 主机 A 连主机 D 要过三个路由器，而**路由器是共享的**——别的流量造成的争用会伤到你的性能。主机之间是 **unicast（单播）**；**multicast（组播）**允许一次发给多个目的端（可能跨网），但**必须由路由器配置支持**，公有云里常被封掉。典型网络里还会有**防火墙**。

**§10.3.2 Protocols**：IP / TCP / UDP 这类协议标准是系统之间能通信的**必要前提**，通信靠传输可路由的**包**（通常是载荷的封装）。

> Network protocols have different performance characteristics, arising from the original protocol design, extensions, or special handling by software or hardware.

**性能差异有三个来源**：原始设计、后加的扩展、软件/硬件的特殊处理。原文举的第一个例子很日常：**IPv4 和 IPv6 会走不同的内核代码路径**。其它协议则是**设计上就不同**、按负载挑的：SCTP、MPTCP、QUIC。另外提醒：协议往往还有**系统级可调参数**（缓冲大小、算法、各种定时器）会改变它的性能表现。

**§10.3.3 Encapsulation**：封装在载荷的**头部（header）**、**尾部（footer）**或两边加上元数据。它**不改变载荷数据**，但会**略微增大消息总长**，这份开销要算进传输成本。

**§10.3.4 Packet Size**：

> The size of the packets and their payload affect performance, with larger sizes improving throughput and reducing packet overheads.

**包越大，吞吐越高、每包开销越低**——但包大小通常被 **MTU** 卡住，以太网常见配置是 **1500 字节**。⚠️ 1500 有历史原因：早期以太网主机**共享介质**（同轴或集线器），包越大，别人等它传完的延迟越久。

**Jumbo frames（巨帧，约 9000 字节）**能提升吞吐与延迟（包更少），但**两个东西联手拖住了它的普及**：① **老硬件**——不支持巨帧的设备要么用 IP 协议**分片**（重组有代价），要么回 ICMP "can't fragment"；② **配置错的防火墙**——历史上有人用 ICMP 攻击（包括 "ping of death"），于是有管理员**一刀切封掉所有 ICMP**，⇒ 那条有用的消息到不了发送端，**包超过 1500 字节就被静默丢弃**。**很多系统因此老老实实留在 1500 的默认值上。** 1500 的劣势被网卡特性**部分追平**：**TCP offload / large segment offload**——把更大的缓冲区交给网卡，由专用硬件切成小帧发出。

---

## 4. §10.3.5 Latency：全章的核心概念

这一节是整章的**心脏**：**把「延迟」拆成六种能分别测的量**。

| 延迟 | 定义 | ⚠️ 陷阱 |
|---|---|---|
| **名字解析延迟** | 建立连接时把主机名解析成 IP 地址（DNS）的时间 | **最坏情况是名字解析超时，可以到几十秒**；OS 通常提供**带缓存**的名字解析服务，后续查询能很快命中 |
| **Ping 延迟** | ICMP echo request 到 echo response 的时间 | ⚠️ **未必等于应用请求的往返时间**——**路由器可能给 ICMP 不同的优先级**。这是 ping 数据的第一个系统性偏差 |
| **连接延迟** | 建立连接、**尚未传输任何数据**之前的时间。TCP 的连接延迟 = **TCP 握手时间**（客户端看是「发出 SYN 到收到 SYN-ACK」） | ⚠️ 它和 ping 相似，但**要走更多内核代码**，而且**包含丢包重传的时间**：SYN 因**服务端 backlog 满**被丢 ⇒ 客户端**定时重传 SYN**，**每次加上一秒量级** |
| **首字节延迟 TTFB** | 连接建立到客户端收到第一个数据字节的时间。它包含：远端接受连接、**调度服务它的线程**、该线程执行并发出首字节 | ⚠️ **这是六个延迟里最容易被误判的一个**：TTFB 高**很可能是服务器自己忙**，跟网络无关。原文还提到它包含 TCP backlog 和 CPU 调度延迟 |
| **往返时间 RTT** | 请求在端点间走一个来回的时间，包含**信号传播**和**每一跳的处理** | 理想情况下 RTT 应该**由包在网络上的时间主导**，而不是远端处理请求的时间 |
| **连接寿命** | 连接建立到关闭的时间 | **keep-alive** 延长连接，让后续操作复用现有连接、**避开反复建立连接（含 TLS）的开销和延迟** |

⚠️ 原文特别强调**命名**：连接延迟更准确的叫法是「**连接建立延迟**」，以和「**连接寿命**」清楚区分——两个词只差一个词，含义差一个数量级（毫秒 vs 分钟）。

原文 Table 10.3 把它铺得更开，还加了几个应用侧关心的：**系统调用 send/receive 延迟**、**系统调用 connect 延迟**、**中断延迟**（网卡中断到内核处理）、**栈内延迟**（穿过内核 TCP/IP 栈）。呈现方式三种：**每区间平均**（最好按 client/server 对分开算，以隔离中间网络的影响）、**完整分布**（直方图或热力图）、**每次操作**（逐条列出，含源/目的 IP）。

> A common source of issues is the presence of latency outliers caused by TCP retransmits.

**离群点（outliers）是这一节的靶心**——平均值会把它们藏起来。

> Note that some sources of extra latency are transient and only occur during system load.

⚠️ **空闲时测的延迟没有意义**——很多额外延迟只在负载下才出现，原文特别强调要**同时在空闲与负载下测量**。Linux 上测这些延迟的手段：`SO_TIMESTAMP`（入包时间，纳秒版 `SO_TIMESTAMPNS`）与 `SO_TIMESTAMPING`（逐事件时间戳，能识别传输延迟、RTT、栈内延迟）。

---

## 5. §10.3.6–10.3.11 缓冲、backlog、协商、拥塞、利用率、本地连接

**§10.3.6 Buffering**：缓冲能让你**在面对高 RTT 时仍然维持高吞吐**——在阻塞等待确认之前先把数据发出去。TCP 用缓冲 + **滑动发送窗口**；socket 自己有缓冲；应用也可能自己再缓冲一层。⚠️ **但缓冲也会变成问题**：
> Buffering can also be performed by external network components, such as switches and routers, in an effort to improve their own throughput.

交换机和路由器也缓冲。而**中间设备的大缓冲会引发 bufferbloat**：包被长时间排队 → 主机侧 TCP 拥塞避免被触发 → **吞吐反而被自己掐住**。Linux 3.x 为此加了一批特性（byte queue limits、CoDel 队列调度、TCP small queues）。原文给出的原则很值得记：
> The function of buffering (or large buffering) may be best served by the endpoints—the hosts—and not the intermediate network nodes, following a principle called end-to-end arguments [Saltzer 84] .

**§10.3.7 Connection Backlog**：TCP 有 **backlog**：SYN 请求可以在内核里排队，等用户态进程来 accept。请求太多、进程 accept 不及时 ⇒ **backlog 到达上限 ⇒ SYN 包被丢弃**，客户端之后重传 ⇒ **客户端连接时间变长**。⚠️ 这个上限**是可调的**：`listen(2)` 的 backlog 参数，内核还可能提供系统级上限。
> Backlog drops and SYN retransmits are indicators of host overload.

**这两件事是主机过载的指标**，不是网络过载的指标。

**§10.3.8 Interface Negotiation**：接口会在收发器之间**自动协商**工作模式：**带宽**（10 / 100 / 1000 / 10000 / 40000 / 100000 Mbits/s）和**双工**（半/全）。接口通常按**最高带宽和协议**描述（如 1 GbE），**但可能协商到更低的速率**——对端跑不动，或者**介质本身有问题（坏线）**。**全双工**收发各走各的路径、各跑满带宽；**半双工**同一时刻只能一个方向。

**§10.3.9 Congestion Avoidance**：网络是**共享资源**，负载高时会拥塞 ⇒ 路由器/交换机丢包 ⇒ **引发延迟的 TCP 重传**；主机也可能被高包率淹没而自己丢包。各协议的拥塞避免机制：**Ethernet** 有 pause frame（IEEE 802.3x）；**IP** 有 **ECN** 字段；**TCP** 有**拥塞窗口**与各种拥塞控制算法。

**§10.3.10 Utilization**：
> Network interface utilization can be calculated as the current throughput over the maximum bandwidth.

⚠️ 但**没听起来那么简单**：带宽和双工都随自动协商变化，**全双工下利用率要按方向分别算**。通常只有**一个方向**最要紧——主机通常是**不对称的**：服务器偏发送，客户端偏接收。
> Once a network interface direction reaches 100% utilization, it becomes a bottleneck, limiting performance.

⚠️ 还有一个坑：有些工具**只报包数不报字节数**。由于包大小差异极大，**包数无法换算成字节数**，因而**没法用它算吞吐或利用率**。

**§10.3.11 Local Connections**：同机两个应用之间的连接走 **loopback**（虚拟接口）。分布式应用拆成 web server / db server / cache server / proxy server / app server，**跑在同一台机器上时它们的连接就是 localhost 连接**。两条 IPC 路线：**IP socket**（走 TCP/IP 栈）与 **Unix domain sockets (UDS)**（在文件系统上建一个文件）——**UDS 可能更快**，绕开内核 TCP/IP 栈，跳过内核代码与协议封装的开销。TCP/IP socket 还有个内核优化：握手后内核可能识别出这是本地连接，**短路 TCP/IP 栈**传数据（这个特性叫 TCP friends，没被合并进内核；现在可以用 **BPF** 做到，Cilium 就是这么用的）。

---

## 6. §10.4.1 Protocols：IP / TCP / UDP / QUIC

**IP**：IPv4 的 **Type of Service** 字段、IPv6 的 **Traffic Class** 字段，现在被重新定义为 **DSCP**（区分服务代码点）+ **ECN**（显式拥塞通知）。**DSCP** 支持不同的**服务等级**，每级有不同的**丢包概率**特性。**ECN** 让路径上的服务器/路由器/交换机**在 IP 头里打一个位来显式表示拥塞**，而不是丢包；接收方把信号回传给发送方，发送方据此**降速**。⇒ **拿到拥塞避免的好处，不付丢包的代价**。

**TCP** 靠**缓冲 + 滑动窗口**在高延迟网络上也能跑出高吞吐；再叠加**拥塞控制**（发送方设一个拥胜窗口），在各种变化的网络里维持「高但可靠」的传输速率。性能特性清单（原文列的）：**Sliding window**（窗口内的多个包可以在收到确认前就发出去，窗口大小由**接收方通告**）、**Congestion avoidance**（防止发太多导致饱和 → 丢包 → 更糟）、**Slow-start**（从小拥胜窗口开始，收到 ACK 就涨，收不到就缩）、**SACK**（允许确认**不连续**的包，减少重传次数）、**Fast retransmit**（不等定时器，**靠重复 ACK** 触发重传——这是 RTT 的函数，比慢得多的定时器快得多）、**Fast recovery**（检测到重复 ACK 后**重置连接去做 slow-start**）、**TCP fast open**（客户端在 **SYN 里就带数据**，可用加密 cookie 认证）、**TCP timestamps**（时间戳由 ACK 回传 ⇒ **可以测 RTT**）、**TCP SYN cookies**（SYN flood 期间让合法客户端仍能连上，而服务端**不必为这些连接尝试存数据**）。

⚠️ **三次握手与状态**：握手完成后进入 **ESTABLISHED**。⚠️ **状态机里有个性能陷阱**——完全关闭的会话进入 **TIME_WAIT**（RFC 793 里写作 TIME-WAIT），以免迟到的包被误认成同端口上的新连接。⚠️ **TIME_WAIT 通常是两分钟**（有些内核可调），这会带来**端口耗尽**问题（→ §10.5.7）。ESTABLISHED 上还可能有个 **keep-alive 定时器**（通常很长，比如两小时）。

⚠️ **重传有两种机制**：**定时器重传**——超时了还没收到确认，这是 **TCP 重传超时（RTO）**，**根据连接的 RTT 动态计算**；Linux 上第一次重传至少 **200 ms**（`TCP_RTO_MIN`），**之后每次按指数退避加倍**（原文脚注指出这**似乎违反了 RFC 6298**，该 RFC 规定 RTO 最小一秒）。**快速重传**——重复 ACK 到达时，TCP 假定有包丢了，**立刻重传**。⚠️ 但有个洞：**最后一个发出的包丢了**，后面没有包来触发重复 ACK 检测；**Tail Loss Probe (TLP)** 解这个——在最后一次发送后一小段时间**再发一个探测包**。

⚠️ **拥塞控制算法**可能是网络性能差异最大的一项：

> The congestion control algorithm can make a large difference to network performance.

五个算法：**Reno**（三个重复 ACK ⇒ 拥塞窗口减半 + 快速重传恢复）、**Tahoe**（同样三个重复 ACK，但**拥塞窗口直接重置为一个 MSS**、回到 slow-start）、**CUBIC**（三次函数缩放窗口 + hybrid start 退出 slow-start，**比 Reno 更激进**，**Linux 默认**）、**BBR**（**不基于窗口**，而是用探测阶段**显式建模路径特性**（RTT 与带宽），某些路径上**吞吐剧增**、某些路径上**反而更差**）、**DCTCP**（靠交换机在**极浅队列**时发 ECN 标记，快速爬到可用带宽，**不适合跨互联网部署**，但在配置良好的受控环境里能显著提升）。Netflix 云服务用 BBR，发现在**重度丢包**期间吞吐能提升到三倍。Linux 5.6 起还可以**用 BPF 开发新的拥塞控制算法**，按需加载。

⚠️ **Nagle**：
> This algorithm (RFC 896) [Nagle 84] reduces the number of small packets on the network by delaying their transmission to allow more data to arrive and be coalesced.

**Nagle 会攒小包**：如果流水线里还有未确认数据，而且已经在经历延迟，就**推迟发送**以便合并。⚠️ 系统通常提供一个可调参数或 **socket 选项来关掉 Nagle**——当它与**延迟 ACK**冲突时必须关。

⚠️ **延迟 ACK**：
> This algorithm (RFC 1122) [Braden 89] delays the sending of ACKs up to 500 ms, so that multiple ACKs may be combined.

**延迟 ACK 最长 500 ms**——把多个 ACK 合并成一个包。⚠️ **Nagle + 延迟 ACK 是经典的互锁组合**：发送方等「攒够再发」，接收方等「攒够再 ACK」，于是**小包被拖住几百毫秒**。

**SACK / FACK / RACK**：**SACK** 让接收方告诉发送方「我收到了一块不连续的数据」——没有它，一次丢包会导致**整个发送窗口被重传**（为维持顺序确认），**严重伤害 TCP 性能**。**FACK** 是 SACK 的扩展，**Linux 默认支持**，追踪额外状态、更好地调节网络里未确认数据的总量。**RACK** 用 **ACK 的时间信息**（而不只是序号）做丢包检测与恢复。

**初始窗口（IW）**：

> The initial window (IW) is the number of packets a TCP sender will transmit at the beginning of a connection before waiting for acknowledgment from the sender.

**短连接**（典型 HTTP）如果 IW 足够大到能覆盖整个待发数据，**完成时间会大幅缩短**；但 IW 大了有**拥塞与丢包风险**，**多流同时启动时更甚**。

> The Linux default (10 packets, aka IW10) can be too high on slow links or when many connections start up

其它 OS 默认是 2 或 4 个包（IW2 / IW4）。

**UDP** 的三个优点：**简单**（小协议头）、**无状态**、**无重传**（重传会给 TCP 连接加上可观的延迟）。

> While simple and often high-performing, UDP is not intended to be reliable, and data can be missing or received out of order.

⚠️ UDP **也没有拥塞避免**——它可能反过来**加剧**网络拥塞。UDP 的一大用途是 **DNS**（简单、无拥塞控制、通常不被防火墙拦），于是有了**建在 UDP 之上、自己实现拥塞控制**的新协议——**QUIC**。

**QUIC 与 HTTP/3**：QUIC 由 Google 的 Jim Roskind 设计，是 **TCP 的高性能低延迟替代品**，**构建在 UDP 之上**，提供：① 在同一个「连接」上**多路复用**多个应用定义的流；② TCP 风格的**可靠有序流传输**，可按子流**单独关闭**；③ **客户端换网络地址时的连接恢复**（基于连接 ID 的加密认证）；④ **载荷全加密，包括 QUIC 头**；⑤ **0-RTT 握手**（对曾经通信过的对端，含加密）。Chrome 浏览器**大量使用** QUIC。

---

## 7. §10.4.2–10.4.3 Hardware 与 Software

**§10.4.2 Hardware**——网络硬件：接口、控制器、交换机、路由器、防火墙。

- **接口**：收发 **frame**。接口类型按二层标准定，各有最大带宽；**带宽越高数据传输延迟越低，成本也越高**。以太网选项：1/10/40/100/200/400 GbE（铜缆或光纤）。⚠️ **你的 OS 可能没有某些卡的驱动**。
- **控制器**：由微处理器驱动，通过 IO 传输通道（如 PCI）挂载。⚠️ **这两者都可能成为网络吞吐或 IOPS 的限制者**。原文算了个细例子：一块双 10 GbE 网卡插在 **PCIe Gen 2 四通道**槽上——卡的上限是 2 × 10 = 20 Gbit/s（双向 40），而槽的上限是 4 × 4 = 16 Gbit/s。⇒ **两个口都不可能同时跑满线速**。
- **交换机与路由器**：交换机给任意两台主机一条专用路径，取代了早期共享总线的**冲突**（CSMA/CD，指数退避重传，负载下就是性能灾难）。路由器用路由表在网络之间投递包。⚠️ **路径是不确定的**——路由和路径**动态更新**以应对故障和均衡负载，所以**任一时刻没人能确定包实际走的哪条路**；多条路径还可能**乱序投递**，这会引发 TCP 性能问题。⚠️ 路由器、交换机常是**限速转换点**（10 Gbps 链路降到 1 Gbps），需要缓冲避免大量丢包——但很多设备**缓冲过量**（bufferbloat）；**在源头做 pacing** 也是一种缓解。
- **防火墙**：有状态规则要为**每个见过的连接**存元数据，连接一多就可能**内存压力过大**——**DoS 攻击**用海量连接灌满它，或者大量出站连接也要做连接跟踪。⚠️ **防火墙在性能测试时也是麻烦**：做带宽实验可能要改防火墙规则，还得跟安全团队协调。
- **其它设备**（hub、bridge、repeater、modem）也都可能是瓶颈与丢包源。

**§10.4.3 Software**——

- **网络栈**：现代内核上网络栈是**多线程的**，入站包可由多个 CPU 处理。Linux 上包以 **`struct sk_buff`**（socket buffer）在各组件间传递。
- **TCP 连接队列**：**两个** backlog——**SYN backlog**（半开连接，等握手完成）与 **listen backlog**（已建立会话，等应用 accept）。早期内核只有**一个**队列，**对 SYN flood 很脆弱**（大量假 IP 的 SYN 灌满队列，真实客户端连不上）。两个队列之后，**第一个可以当可疑连接的暂存区**，只有连接建立后才提升到第二个；第一个队列可以开得很长以吸收 SYN 洪水，且只存最少量的元数据。**SYN cookies 会绕过第一个队列**。**两个队列的长度可以独立调**。
- **TCP 缓冲**：socket 的**发送/接收缓冲**都可调。**更大的缓冲提升吞吐**，代价是**每连接占更多主存**；可以不对称设置（服务器发多收少就调大发送缓冲）。Linux 内核还会**根据连接活动动态调整**大小，并允许调**最小/默认/最大**三个值。
- **GSO / TSO / GRO**：为避免发送大量小包的网络栈开销，Linux 用 **generic segmentation offload (GSO)** 发出最大 **64 KB** 的「超级包」，临近交给网卡时才切成 MSS 大小的段；网卡与驱动支持 **TCP segmentation offload (TSO)** 时由硬件切分。**GRO** 是接收侧对应物。⚠️ **GSO/GRO 是内核软件实现的，TSO 是网卡硬件实现的。**
- **队列调度（qdisc）**：一层**可选**的流量分类、调度、操作、过滤与整形。Linux 提供大量 qdisc 算法，用 `tc(8)` 配置，每个都有 man page（`man -k tc-`）。⚠️ **内核默认是 `pfifo_fast`，而 systemd 没那么保守，默认 `fq_codel`**。BPF 可以增强这一层（`BPF_PROG_TYPE_SCHED_CLS` / `SCHED_ACT`），挂在内核 ingress/egress 上做过滤、改包、转发。
- **驱动与中断合并**：网卡驱动通常还有一个**环形缓冲**在内核内存与网卡之间搬运包。**中断合并**——不为每个到达的包都中断内核，而是**等定时器（轮询）或攒够一定数量的包**才中断 ⇒ **内核与网卡的交互次数减少** → 吞吐更高，**代价是延迟变差**。Linux 的 **NAPI** 框架：低包率用中断（softirq 调度），高包率**关中断改轮询**以实现合并。NAPI 还有包限速、接口调度（保证繁忙网卡之间的公平）、**`SO_BUSY_POLL`**（用户态可要求忙等以降低接收延迟）。
- **CPU 扩展**：**RSS**（多队列网卡按 IP + 端口哈希分流到不同队列、各绑不同 CPU ⇒ **同一连接的包落到同一 CPU**）、**RPS**（RSS 的软件实现，给不支持多队列的网卡用）、**RFS**（**倾向把包送到上次处理该 socket 的那个 CPU**，提高缓存命中率与内存局部性）、**ARFS**（在**硬件里**实现 RFS）、**XPS**（多发送队列时支持多 CPU 发送）。⚠️ **没有负载均衡策略时，网卡可能只中断一个 CPU，那个 CPU 会跑到 100%**——表现为**单核 softirq 时间很高**。**负载均衡器与代理服务器**（如 nginx）尤其容易撞上。
- **内核旁路（Kernel Bypass）**：**DPDK**——应用在**用户态实现自己的网络协议**，直接访问网卡内存。**XDP**——**可编程快路径**，用扩展 BPF，**集成进现有内核栈而不是绕开它**。⚠️ 旁路的代价原文说得很直接：

> With kernel network stack bypass, instrumentation using traditional tools and metrics is not available because the counters and tracing events they use are also bypassed.

**你把观测工具一起旁路掉了。** 除了完全旁路，还有**避免拷贝**的手段：`send(2)` 的 **`MSG_ZEROCOPY`** 标志，以及 `mmap(2)` 零拷贝接收。
- **其它优化**：**Pacing**（控制**何时**发包，把发送摊开，避免突发与 incast）、**TCP Small Queues (TSQ)**（限制网络栈排队量，避免 bufferbloat）、**Byte Queue Limits (BQL)**（自动把驱动队列调得**足够大以免饿死、足够小以限制排队延迟**，并避免耗尽网卡 TX 描述符）、**Earliest Departure Time (EDT)**（用**时间轮**而不是队列来排序发给网卡的包）。

> A TCP sent packet can be processed by any of the congestion controls, TSO, TSQ, pacing, and queueing disciplines, before it ever arrives at the NIC.

**一句话：一个包在到达网卡之前，可能被七八个机制各动一次。** 这就是为什么「调一个参数试试」是低效的。

---

## 8. §10.5 Methodology：十种方法与它们的顺序

原书 Table 10.2 把网络分析的方法列成一张表，并给了**明确的起始顺序**：

> These may be followed individually or used in combination. My suggestion is to use the following strategies to start with, in this order: performance monitoring, the USE method, static performance tuning, and workload characterization.

**性能监控 → USE 方法 → 静态性能调优 → 工作负载刻画。** ⚠️ 注意这和第 9 章磁盘那节的顺序**不一样**（磁盘是 USE → 性能监控 → 工作负载刻画 → 延迟分析 → 微基准 → 静态分析 → 事件追踪）。**别把两章的顺序混起来。**

**§10.5.1 Tools Method**——清单：`nstat` / `netstat -s` 看**重传率与乱序包**（⚠️「高」到什么程度**取决于客户端**：面向互联网的系统应当比同机房内部系统有更高的重传率）；`ip -s link` / `netstat -i` 看接口**错误计数器**（errors / dropped / overruns）；`ss -tiepm` 看**limiter 标志**（重要 socket 的瓶颈在哪）；`nicstat` / `ip -s link` 看收发字节速率（⚠️ 高吞吐可能被**协商出的链路速率**或外部限速卡住，也会造成**同机网络用户之间的争用**）；`tcplife` 记录 TCP 会话（进程、时长、吞吐）；`tcptop` 实时看 top TCP 会话；`tcpdump`（⚠️ **CPU 与存储开销大**，但短时间用能发现异常流量）；`perf(1)` / BCC / bpftrace 检查应用与线路之间的选定包。

**§10.5.2 USE Method**——对**每个网络接口**、**每个方向**（TX / RX）查三项：

**① Utilization**：接口收发帧时**忙碌的时间**。⚠️ **通常没有工具直接给**（`nicstat` 是例外），要自己算：**当前吞吐 / 当前协商速率**。⚠️ **吞吐必须按网络上的字节算——包含所有协议头**。⚠️ **如果环境有网络带宽限制**（某些云环境如此），利用率还要**相对「被施加的限制」**再算一份。

**② Saturation**：因接口满载而产生的**额外排队、缓冲或阻塞**的程度。
> Saturation of the network interface is difficult to measure.

**接口饱和度很难测**：正常缓冲是应该的（应用发数据比接口传得快）。可能的测法是**测应用线程阻塞在网络发送上的时间**；另外查内核里跟饱和度更相关的统计，比如 Linux 的 **overruns**。⚠️ Linux 用 **BQL** 调节网卡队列大小，这**有助于避免网卡饱和**。

**③ Errors**：接收侧——坏校验和、太短（小于数据链路头）或太长、冲突（交换式网络里不太可能）；发送侧——迟到冲突（坏线）。
> Errors may be checked first, since they are typically quick to check and the easiest to interpret.

⚠️ **TCP 层的重传通常 readily available**，可以当网络饱和的指示——但它**测的是服务器到客户端之间整条路径**，任何一跳的问题都可能造成它。USE 方法也能套到**控制器**上，⚠️ 但这些组件的观测工具很少，**通常要靠接口统计与拓扑来推断**（控制器 A 上有端口 A0、A1，则控制器吞吐 = A0 + A1）。

**§10.5.3 Workload Characterization**——最基本的三项：**网络接口吞吐**（RX/TX，bytes/s）、**网络接口 IOPS**（RX/TX，frames/s）、**TCP 连接速率**（主动与被动，connections/s）。原文给了一段范例写法（值得照抄这个句式）：
> The network throughput varies based on users and performs more writes (TX) than reads (RX). The peak write rate is 200 Mbytes/s and 210,000 packets/s, and the peak read rate is 10 Mbytes/s with 70,000 packets/s. The inbound (passive) TCP connection rate reaches 3,000 connections/s.

⚠️ **这三条之外还有一串进阶清单**：平均包大小（RX/TX 各多少）、各层协议分布（TCP / UDP，UDP 里可能含 QUIC）、哪些 TCP/UDP 端口在活动、广播与组播包率、**哪些进程在用网络**。

**§10.5.4–10.5.5 Latency Analysis 与 Performance Monitoring**——测**尽可能多种**延迟来缩小范围，呈现方式（承接 §10.3.5）：每区间平均、完整分布、每次操作；**离群点靠完整分布或逐条追踪来发现**（包括**按最小延迟阈值过滤**）。监控侧的关键指标：**吞吐**（接口 bytes/s，RX/TX，理想情况按接口分开）、**连接**（TCP connections/s）、**错误**（含丢包计数器）、**TCP 重传**（用于与网络问题关联）、**TCP 乱序包**。

**§10.5.6 Packet Sniffing**——
> For observational analysis this may be the last resort, as it can be expensive to perform in terms of CPU and storage overhead.

**网络内核代码路径是按周期优化的**——要处理每秒百万级包，对任何额外开销都敏感。降开销的办法：**内核用环形缓冲把包数据通过共享内存映射传给用户态工具**（`perf(1)` 的输出环缓冲、AF_XDP）；另一种是**旁路抓包机**——接在交换机 "tap" 或 "mirror" 口上的独立服务器（Amazon 和 Google 都提供这项云服务）。抓包通常落到文件再离线分析，每个包可记：**时间戳、完整包（含各层协议头与部分/完整载荷）、元数据（包数、丢包数）、接口名**。
> Because packet capture can be a CPU-expensive activity, most implementations include the ability to drop events instead of capturing them when overloaded.

⚠️ **过载时宁可丢事件也不硬抓**——丢掉的包数会记进日志。另一个降开销手段：**过滤表达式在内核里执行**（典型用 BPF 编译成字节码、再 JIT 成机器码）。

**§10.5.7 TCP Analysis**——除了 §10.5.4 的延迟，还要看：**Socket 收发缓冲的使用情况**；**TCP backlog 队列的使用情况**；**backlog 满导致的内核丢包**；**拥塞窗口大小**（含 zero-size advertisement）；**TCP TIME_WAIT 期间收到的 SYN**。⚠️ 最后一条会变成**可扩展性问题**：服务器频繁连同一个目的端口、源目的 IP 又相同时，区分每条连接的只有**客户端源端口（ephemeral port）**——TCP 里是 **16 位**，还可能被 OS 的端口范围进一步限制。**加上 TIME_WAIT 间隔（可能 60 秒）**，⇒ **60 秒内超过 65,536 条连接就会撞上端口冲突**。避免办法：Linux 尝试快速复用连接、服务器用**多个 IP 地址**、或者用 **`SO_LINGER`** 设一个很短的 linger 时间。

**§10.5.8 Static Performance Tuning**——静态配置检查表（原文列的，压缩成清单）：接口数量与实际使用数；接口的**最大速率**与**当前协商速率**；**半双工还是全双工**；**MTU**；是否 **trunk**；驱动 / IP 层 / TCP 层的可调参数及**有没有被改过默认值**；**路由**配置与默认网关；数据路径上**各组件（含交换机、路由器背板）的最大吞吐**；数据路径的**最大 MTU**与**会不会分片**；有没有**无线连接**及干扰；是否**开着转发**（本机在当路由器？）；**DNS 怎么配的**、服务器有多远；网卡固件 / 驱动 / 内核 TCP/IP 栈的**已知性能 bug**；**防火墙**；**软件施加的网络吞吐限制**。
> The answers to these questions may reveal configuration choices that have been overlooked.

⚠️ **最后一条对云环境尤其关键**：那里可能有强加的吞吐上限。

**§10.5.9–10.5.10 Resource Controls 与 Micro-Benchmarking**——操作系统可以限制网络资源：**网络带宽限制**（内核施加的最大吞吐）、**IP QoS**（IP 头的 ToS 位 → 现在的 DSCP）、**包延迟**（如 `tc-netem(8)`，**可以模拟别的网络来测试性能**）。网络微基准的用途是**确认网络至少能达到期望吞吐**；⚠️ 微基准通常**比应用简单得多、debug 快得多**——先确认真的是网络的问题，再回应用；**网络调到位之后，才把注意力还给应用**。可测的因子：**方向**、**协议**（TCP/UDP 及端口）、**线程数**、**缓冲大小**、**接口 MTU**。⚠️ **100 Gbit/s 级别的接口可能需要多个客户端线程才能跑满带宽。**

---

## 9. §10.6 Observability Tools：Linux 上够用的那一套

原文 Table 10.4 把工具和对应小节列成表。这里只记每个工具**是什么、什么时候用**：

| 工具 | 用途 |
|---|---|
| **`ss`** | socket 统计。默认输出是快照（Netid / State / Recv-Q / Send-Q / 本地与对端地址端口），可用来**刻画当前负载**。加选项能看更多：`-t` 只看 TCP、`-i` TCP 内部信息、`-e` 扩展、`-p` 进程信息、`-m` 内存占用。⚠️ **`-e` 能看到 limiter 标志**（这个 socket 的瓶颈是什么） |
| **`ip`** | 接口与路由统计（`ip -s link` 看错误计数、`ip route` 看路由） |
| **`ifconfig`** | 接口统计（老工具） |
| **`nstat`** | 打印内核维护的**网络指标**（带 SNMP 名），`-s` 不重置计数器 |
| **`netstat`** | 比 `ss` 老，信息少但兼容性好 |
| **`sar`** | 归档式性能统计 |
| **`nicstat`** | 接口统计，**含吞吐与利用率**，风格像 `iostat` / `mpstat`（作者本人写的 Solaris 版，Linux 版由 Tim Cook 贡献） |
| **`ethtool`** | 静态配置（`-i` 驱动信息、`-k` 特性开关）与驱动统计（`-S`） |
| **`tcplife` / `tcptop`** | 记录 / 实时看 TCP 会话 |
| **`tcpretrans`** | 重传统计 |
| **`bpftrace`** | 自定义追踪（内核态与用户态、TCP 状态机、socket 事件） |
| **`tcpdump` / `Wireshark`** | 抓包与离线分析 |

⚠️ 这一节最大的价值不是「有哪些命令」，而是 §10.4.3 那条警告的呼应：**这些工具全部依赖内核计数器与追踪事件**，而内核旁路会把它们一起旁路掉。

---

## 10. §10.7 Experimentation：主动把网络压一压

网络性能常用**做实验**的工具测，而不只是观测状态。

- **§10.7.1 `ping`**：发 ICMP echo request 测连通性与 RTT。⚠️ 旧版 `ping` 在**用户态**测 RTT，会被内核执行与调度延迟**略微放大**；新内核与新版本 `ping` 用内核时间戳（`SIOCGSTAMP` / `SO_TIMESTAMP`）提高精度。⚠️ **ICMP 包可能被路由器降优先级**，延迟方差看起来比平时大。
- **§10.7.2 `traceroute`**：逐包**递增 TTL**，逼沿途网关回 ICMP time exceeded，路径因此「自曝」。⚠️ 某跳显示 `*` 可能是**那跳不回 ICMP**，也可能是**防火墙挡了 ICMP**——可以改用 TCP（`-T`）。⚠️ **路径在一次运行中也会变**（原文的例子里第 7 跳地址就换了）。
- **§10.7.3 `pathchar`**：类似 traceroute，但**给出跳之间的带宽**。⚠️ 很不幸没能流行，源码似乎没公开，Linux 上的二进制还停在 1997 年的 2.0.30 内核；而且**跑一次要几十分钟**。
- **§10.7.4 `iperf`**：测 TCP/UDP 最大吞吐。⚠️ **必须服务端与客户端同时跑**。选项：`-c` 目标主机、`-l` socket 缓冲、`-P` 并行线程数、`-i` 间隔输出、`-t` 持续时长。⚠️ **并行模式常常是必需的**——单线程客户端喂不饱网络。原文例子 `-P 2` 跑出 244 Mbit/s。
- **§10.7.5 `netperf`**：请求/响应性能的高级微基准，原文用它测 **TCP 往返延迟**（`TCP_RR` 测试，例子读出 98.7 ms）。
- **§10.7.6 `tc`**：可以选 qdisc；**做实验时用 `netem` qdisc 制造丢包/延迟**（原文示例：`tc qdisc add dev eth0 root netem loss 1%`，再用 `tc -s qdisc show` 看丢包计数）。
- **§10.7.7 其它**：`pktgen`（内核自带的包生成器）、**Flent**（一次跑多个微基准并画图）、`mtr`（带 ping 统计的 traceroute）、`tcpreplay`（回放抓到的流量，**能重现只在特定包序列或位模式下出现的问题**）。

---

## 11. §10.8 Tuning：能调什么，以及为什么应该先别调

> Network tunable parameters are usually already tuned to provide high performance.

**网络可调参数通常已经调好了**——网络栈本身也设计成能对不同负载**动态响应**。
> Before trying tunable parameters, it can be worthwhile to first understand network usage.

⚠️ **先搞清楚网络怎么用**，这可能直接发现**可以消除的多余工作**，收益远大于调参。

**§10.8.1 System-Wide**：用 `sysctl` 查看与设置，写进 `/etc/sysctl.conf`，也可以从 `/proc/sys/net` 读写。⚠️ Linux 5.3 上光含 "tcp" 的参数就有 **70 个**。原文给了 **Netflix 的实例**（14 条，在启动脚本里应用）：

```
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

⚠️ 原文强调：**这是一个时点快照，不是配方**。各类参数的用途（压缩成清单）：**socket / TCP 缓冲**——`rmem_max` / `wmem_max`（所有协议类型的上限，**要跑满 10 GbE 可能需要 16 MB 或更大**）、`tcp_moderate_rcvbuf = 1`（开启自动调优）、`tcp_rmem` / `tcp_wmem`（各三个值：最小、默认、最大）；⚠️ **调最大值**能提吞吐，**调最小值和默认值会多占每连接内存**，通常没必要。**TCP backlog**——`tcp_max_syn_backlog`（第一个队列）、`somaxconn`（第二个队列）；⚠️ **两者可能都需要从默认值往上加**以扛住突发。**设备 backlog**——`netdev_max_backlog`（每 CPU），10 GbE 网卡可能要 10,000。**拥塞控制**——`tcp_available_congestion_control`（能 `modprobe` 加载）、`tcp_congestion_control`。**TCP 选项**——`tcp_sack`、`tcp_fack`（⚠️ 高延迟网络上能提吞吐，**代价是一些 CPU 负载**）、`tcp_tw_reuse`、`tcp_tw_recycle`（⚠️ **不如 tw_reuse 安全**）。**ECN**——`tcp_ecn`（0 关闭 / 1 允许入站并在出站请求 / 2 允许入站且不在出站请求；默认 2）。**BQL**——在 `/sys/.../byte_queue_limits/` 下用 `limit_min` / `limit_max` 钳住范围。**资源控制**——cgroups 的 `net_prio`（出站优先级）与 `net_cls`（打 class ID，配合 qdisc 限速，也供 BPF 使用）。**qdisc**——`net.core.default_qdisc`（很多发行版已默认 `fq_codel`）。**Tuned 项目**——自动调优，profile 如 `network-latency`（以功耗为代价换确定性低延迟）、`network-throughput`。

**§10.8.2 Socket Options**：应用可以**逐 socket** 用 `setsockopt(2)` 调。⚠️ **这要求你能改代码并重新编译。** 原文 Table 10.8 的样本里最关键的几项：`SO_SNDBUF` / `SO_RCVBUF`（收发缓冲大小，可调到前述系统上限；还有 `SO_SNDBUFFORCE` 可以越过发送上限）；`SO_REUSEPORT`（多个进程/线程绑同一端口，内核分发负载，Linux 3.9 起）；`SO_MAX_PACING_RATE`（最大 pacing 速率，见 `tc-fq(8)`）；`SO_LINGER`（**缩短 TIME_WAIT 延迟**）；`SO_TXTIME`（基于时间的包发送，Linux 4.19 起）；**`TCP_NODELAY`**（**禁用 Nagle，尽快发送分段。⚠️ 可能以更高的网络利用率（更多包）为代价换来更低的延迟**）；`TCP_CORK`（**暂停发送直到能发满包**，提升吞吐；系统级的还有 `tcp_autocorking`）；`TCP_QUICKACK`（立即发 ACK，可能提升发送带宽）；`TCP_CONGESTION`（该 socket 的拥塞控制算法）。

⚠️ 还有系统调用层面的标志：**`MSG_ZEROCOPY`**（Linux 4.14 起）让用户态缓冲区直接参与传输，省掉拷进内核的开销。⚠️ 但**用起来没这么简单**：`send(2)` 可能在数据真正发出之前就返回，**发送方必须等内核通知**才能安全地释放或复用那块内存。

**§10.8.3 Configuration**——**以太网巨帧**（MTU 1500 → ~9000，**前提是整个网络基础设施都支持**）；**链路聚合**（多张网卡组合成一张，需要交换机支持与正确配置）；**防火墙配置**（例如在 egress hook 上用 iptables 或 BPF 按规则设置 IP 头的 ToS / DSCP 等级，实现按端口分优先级）。

---

## 12. 实战锚点：otf-server 面对「网络」这件事

> **诚实前提**：**本仓库没有 UDP、没有 KCP、没有网卡调优、没有抓包。**
> 全仓 `grep -in "udp\|kcp" src/` 只命中 `src/net/transport.h` 一个文件——而那个文件里的 "UDP" 全部是**迁移分析的注释**，不是实现。全仓没有 `tcpdump` / `iperf` / `ss` 集成，没有 perf / eBPF / bpftrace，**没有内核层网络指标**。
> ⇒ **本章的绝大多数内容在本仓库是空白的。** 下面写的是**能对应上的那一部分**，以及**对应不上但值得记下的缺口**。

### 12.1 §10.4.1 Nagle —— `TCP_NODELAY` 是必设项，而且失败必须看得见

原书 Table 10.8 说 `TCP_NODELAY`「禁用 Nagle，尽快发送分段」。本仓库把它当**必设项**，而且**专门检查返回值**（`src/net/tcp_transport.cpp:308` 的 `AcceptNew` 路径、`:411` 的 `Accept()` 路径，客户端 `src/cli/cli_client.cpp:605` 还有第三份）：

```cpp
if (::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) < 0) {
  log::warn(std::string("TCP_NODELAY 失败（Nagle 将保持开启，小包可能延迟 0.5s）: ") + std::strerror(errno));
}
```

⚠️ **为什么必须设**：本仓库的输入包是 **8 字节**（`kPlayerInputPayload = 8`）、心跳包 **0 字节**（`kHeartbeatPayload = 0`）。⚠️ **Nagle 的判定逻辑对「小包 + 已有未确认数据」就会推迟发送，最长 0.5 秒**——而 0.5 秒正好是本仓库 `kTickRateHz = 20` 的 **10 个逻辑帧**。⇒ **不设 TCP_NODELAY 的症状不是「有点卡」，是「玩家按了攻击键，服务器 10 帧之后才知道」。**

⚠️ 而**检查返回值**这一步更重要。失败时的处置是 **warn 并继续**，不是关连接，仓库把理由写下来了：**失败只发生在 ENOPROTOOPT（内核不认这个选项）/ EBADF 这类情形**，对一个已经 accept 出来的 AF_INET/SOCK_STREAM 实际不可达；真发生了，代价是**延迟劣化**，连接本身仍完全可用；**关连接会把一个可降级的性能问题升级成硬故障**。
⇒ 这与 §10.8.2 的原书建议方向一致（`TCP_NODELAY` 用延迟换更高的包数量），但仓库多走了一步：**它假定这个选项可能设置失败，所以让它失败时可见。**

### 12.2 §10.4.3 发送缓冲 —— 没有环形缓冲，每次 resize 都要搬旧数据

原书 §10.4.3 讲的是**网卡驱动侧**的 ring buffer。本仓库的**应用侧**发送缓冲**不是环形缓冲**（`src/net/tcp_transport.cpp:467-469`）：

```cpp
const std::size_t old = c.out.size();
c.out.resize(old + n);
std::memcpy(c.out.data() + old, buf, n);
```

⚠️ `std::string out` + `size_t out_sent` 是**线性缓冲**：`out_sent` 标记已发送位置，写完整个缓冲才 `c.out.clear()`（`FlushWrite`，`tcp_transport.cpp:376`）。⇒ **每次追加都要把「还没发出去的尾巴」整体搬一次**（`resize` 在尾部追加时可能触发重分配 + 全量拷贝）。仓库注释的辩护是「resize **保留已有 capacity**，所以稳态下**零分配**」——⚠️ **但零分配 ≠ 零搬运**：`resize` 仍要把已有内容**往后挪**。在「发送缓冲长期接近 `kSendBufCap = 128 KB`」的稳态下，这意味着**每追加一小段就要搬 128 KB**。

（推演）这正好是原书 §10.4.3 那句的镜像：
> A TCP sent packet can be processed by any of the congestion controls, TSO, TSQ, pacing, and queueing disciplines, before it ever arrives at the NIC.

**一个包在到达网卡前被处理了几次**——本仓库的处理次数是：**帧编码一次拷贝 → 进发送缓冲再一次拷贝 → `write()` 进内核再一次拷贝**。**三次。** 而真正的环形缓冲能省掉中间那次。对照之下，`src/net/frame_codec.h:53` 的 `BeginFrame` 已经做到**每帧 0 次分配、0 次额外搬运**（注释原话：「走 BeginFrame 之后是**每帧 0 次分配、0 次额外搬运**」）——**编码侧优化到位了，缓冲侧没有。**

⚠️ 但要诚实：**这个瓶颈的量级取决于「发送缓冲到底积多长」。** 本仓库的快照是 **10 Hz 下行**、单帧 payload 上限 **1400 字节**（`kMaxPayloadSize`），⇒ 正常情况下发送缓冲很短、几乎不会触发扩容。**在「对端消费不过来」（背压）这条路径上才会积起来**——而那条路径的处置是**直接断连**（`tcp_transport.cpp:454`：`if (queued + n > kSendBufCap) { ... Close(id); }`）。
⇒ **本仓库用「断开」代替了「缓冲排队」。** 这与原书 §10.3.6 的 bufferbloat 讨论方向**正好相反**：原书说「缓冲大能扛高 RTT」，本仓库说「缓冲超 128 KB 就断你」。⚠️ 这个选择对**实时游戏**是合理的（延迟比吞吐重要），但它把 §10.3.7 那种「排队延迟」变成了「连接直接没了」。

### 12.3 §10.4.1 IP / UDP —— 本仓库明确选了 TCP，代价写在代码里

`src/net/transport.h` 的**文件头**有 40 多行专门做 UDP 迁移分析。它先给了一个诱人的结论（成立的部分只有**帧解码层**：Recv/Send 是**字节流**语义，UDP 实现把「拆帧」换成「整个 datagram = 一帧」，拆帧之上的收件箱 / 心跳 / 超时 / 广播几层可以照搬），然后立刻自我否定，逐条列出**「UDP 实现零改动」不成立**的 **5 处冲突**：

| # | 冲突 | 为什么 |
|---|---|---|
| ① | **`ConnId = fd` 这条不变量直接失效** | UDP 没有 per-peer fd，必须自造 handle（对端地址 → 表项映射），`ConnId` 不再等于任何内核句柄 |
| ② | **`Send` 的「未写完的尾巴由内部队列持有」无意义** | datagram 是原子投递，`sendto()` 失败即丢，不存在「写了一半、回头续传」；`bool Send` 的二态语义与 EPOLLOUT 回冲路径一起失效 |
| ③ | **`EnableWrite` / `DisableWrite` 没有对应物** | UDP 无流控可等；要防丢包得自己做拥塞/速率控制，那是另一个系统 |
| ④ | **`Accept` 没有对应物** | 没有握手、没有 accept 队列 |
| ⑤ | **「Recv 的 0 分支永不出现」的理由写错了** | UDP `recvfrom()` 返回 0 是「收到一个长度为 0 的 datagram」，**完全合法，不是 EOF**；真正的事实是 **UDP 根本没有「对端关闭」这个概念**，所以**掉线只能由上层心跳超时判定** |

结论（照抄）：

> 结论：UDP 要落地上层，代价集中在**会话/连接模型**（①②③④），帧解码层零改动（那是唯一真的省下来的部分）。

⚠️ ⑤ 这条与本仓库的实现**直接咬合**：本仓库的掉线判定是 `kConnectionTimeoutMs = 5000`（`src/proto/protocol.h:169`）的**心跳超时**，正好是原文说的那条路。⇒ **如果真迁到 UDP，「5 秒无包判死」这套机制要原封不动搬过去**——因为 UDP 侧没有任何「对端关闭」信号可用。
**这就是本仓库对 §10.4.1 的答案：本章不需要「要不要 UDP」的讨论，因为代价已经被算清楚了并写在代码注释里。**

### 12.4 §10.3.5 延迟 —— 本仓库测的是「逻辑延迟」，不是「网络延迟」

原书 §10.3.5 的核心是：**把延迟拆成六种，分别测**。本仓库能测的**只有一种**，而且它是**应用内部**的——主循环里 `transport.Poll(sched.PollTimeoutMs(now))` 是唯一的 IO 等待点，其后取 `post_poll = SteadyNowMs()` 再算 `drift = sched.DriftMs(post_poll)` 交给 `metrics.on_tick_drift(drift)`。仓库注释把 `post_poll` 的理由写得很清楚：**Poll 阻塞过，Poll 之前取的 now 已经过期；拿旧 now 算补帧会少追帧 —— 症状是「逻辑帧率随负载下降」。**

⇒ **这是「等 IO 的时间」在本仓库的落点**，也是 `src/net/metrics.h` 里 `tick_drift_max_ms` 的口径。

⚠️ 但**它不是网络延迟**：它测的是「**主循环这一拍落后墙钟多少**」，里面混着网络等待、逻辑耗时、补帧等等。而 §10.3.5 的六种延迟**一种都没有对应物**：**名字解析延迟**（客户端直连 IP，无 DNS）、**ping 延迟**（没有 ping 工具）、**连接延迟**（三次握手在 accept 时完成，**没有任何测量**）、**首字节延迟**（`on_accept_` 到第一帧发出去之间没有计时）、**RTT**（没有 TCP timestamps，也没有应用层 ping/pong）、**连接寿命**（只有「当前连接数」，没有「连接持续了多久」）——**全部为空**。

（推演）**最小可行的一步**：客户端已有心跳（`kHeartbeatIntervalMs = 1000`），服务器在 `SessionManager::Tick` 里判死。⇒ **只要在收到心跳包时记一个时间戳、发出心跳包时也记一个，就能得到一个粗略的应用层 RTT**（虽然把心跳间隔也算了进去）。这比引入全套网络观测便宜得多，且**正好落在 §10.5.4 说的「逐次操作延迟」那一栏**。

### 12.5 §10.3.7 Backlog —— `listen(128)`，但看不见 backlog

本仓库的监听 socket 是 `::listen(listen_fd_, 128)`（`src/net/tcp_transport.cpp:109`）；指标端点另开一个 socket，`kListenBacklog = 64`（`src/net/metrics_server.cpp:29`）。

⚠️ **两个缺口**：① **没有 backlog 指标**。§10.3.7 明确说「backlog 满 ⇒ SYN 被丢 ⇒ 客户端重传 ⇒ 连接时间变长」，§10.5.2 说 **backlog 丢弃与 SYN 重传是主机过载的指标**。本仓库**两个都看不见**：`src/net/metrics.h` 里既没有 backlog 深度，也没有重传计数。② **backlog 满了不会断连，只会静默等待**。⚠️ 而本仓库的处置哲学在别处是**宁可断连不可静默**（发送缓冲超限就断、超长帧就拒）。⇒ **backlog 是这条哲学的一个例外，而且是一个没人提的例外。**
（推演）最便宜的补法：配合 `/proc/net/netstat` 里的 `ListenOverflows` / `ListenDrops` 读一次，就能在告警里说清「是不是 backlog 满了」。⚠️ 但这需要**外部观测**（本仓库的观测面在应用内），属于 §10.6 那一整层缺失的连带后果。

### 12.6 §10.5–10.6 观测 —— 本仓库的观测面只有应用内埋点

这是本章最诚实、也最值钱的一条对照。
**本仓库有的**（`src/net/metrics.h` 的 `MetricsSnapshot`）：`connections`、`rooms`、`frames_sent` / `frames_recv`、`heartbeat_timeouts`、`oversize_rejects`、`tick_drift_max_ms`、`snapshot_send_ms_max`、`epoll_ctl_add` / `_del` / `_mod`、`epoll_ctl_add_with_out`。

**本仓库没有的**（§10.5.5 点名的网络监控关键指标）：**接口吞吐**（RX/TX bytes/s——没有它就没有 §10.3.10 的利用率）、**TCP 重传率**（§10.5.2 的网络饱和指示）、**乱序包**、**丢包计数**（`ip -s link` 要看的 "errors / dropped / overruns"）、**backlog 深度**、**SO_RCVBUF / SO_SNDBUF 使用量**（§10.5.7 的第一项）、**TIME_WAIT 数量**（端口耗尽的前兆）。
⚠️ 本仓库**甚至没有一个「内核层」的视角**——`src/net/metrics.h` 里那 11 个计数器，**全部是应用自己埋的**。§10.5.1 那张工具清单里的 `nstat`、`ip -s link`、`ss`、`nicstat`、`ethtool`、**一个都没用**。
⇒ 按 §10.5.1 的「工具法」打分：**本仓库的网络工具法是空的。** 它只有**应用内埋点**这一层，而这层**看不到任何一个网络内核事实**。
⚠️ 值得强调的是 §10.4.3 那条警告在本仓库**不适用**：本仓库**没有**内核旁路（它老老实实用 epoll + 普通 socket），所以**内核计数器是存在的**，只是**没人去读**。⇒ **这是「看得见但没看」，不是「看不见」。** 这两者的处置完全不同：前者只需要加一个采集步骤，后者需要换架构。

### 12.7 §10.8 调优 —— 本仓库调了什么、刻意没调什么

本仓库在 `setsockopt` 上做了**明确取舍**，而且都写了理由（`src/net/tcp_transport.cpp:78`）：

```cpp
// ⚠️ SO_RCVBUF / SO_SNDBUF 刻意不调（§6.4.3）：带宽余量 20 倍以上，
//   调它只会带来"为什么我调了没用"的困惑。
```

⚠️ 这条与原书 §10.8.1 的方向**是相反的**：原书说 `rmem_max` / `wmem_max` 可能需要设到 16 MB 才能跑满 10 GbE。⇒ **但原书的前提是「你要跑满线速」**；本仓库的负载是 10 Hz 小快照，**带宽余量 20 倍以上**，所以调它确实「没用」。
⚠️ **不过原书还有一条本仓库没采纳的建议值得单独提**：`SO_REUSEPORT`（多进程/线程绑同一端口，内核分发负载）。⚠️ 本仓库是**单线程 reactor**，`SO_REUSEPORT` 对它**没有意义**——它解决的是「多核扩展」问题，而本仓库的并发模型是「一个事件循环」。
⇒ **§10.8 的结论对本仓库是「大部分不适用」，而不是「没做」。** 原书那句「Network tunable parameters are usually already tuned to provide high performance」在这里要反过来说：**本仓库的瓶颈从来不在内核可调参数上。**

### 12.8 本章的一句话行动清单

1. **加一个应用层 RTT 指标**——客户端已有 1 秒心跳，服务器记收发时间戳即可。这是最便宜、且直接落在 §10.5.4「逐次操作延迟」那一栏的动作。
2. **加一个发送缓冲水位指标**（`send_buf_bytes` + high-water mark）——把 §12.2 那条「每次 resize 都搬旧数据」从**理论成本**变成**可测数字**；顺带补上 §10.5.7 缺失的「Socket 发送缓冲使用量」。
3. **给 backlog 加一条观察**（`/proc/net/netstat` 的 `ListenOverflows` / `ListenDrops`，或内部自计）——让 §10.3.7 那条「backlog 丢弃是主机过载指标」在本仓库可观测。
4. **统一「静默」与「断连」的处置口径**——§12.5 指出 backlog 满是当前的唯一例外；把它和发送缓冲超限对齐（宁可断连不可静默），或者至少在注释里写清为什么它是例外。
5. **评估发送缓冲是否值得改成环形缓冲**——先做第 2 条拿到水位分布，再决定。⚠️ **在带宽余量 20 倍的负载下，大概率不值得**，但**背压路径（对端消费不过来）上的行为值得单独测一次**。
6. **不要在这上面花力气去调 sysctl**——§12.7 的结论：本仓库的瓶颈不在内核可调参数上。

---

## 13. 收尾：本章金句清单

1. **The network is often blamed for poor performance given the potential for congestion and its inherent complexity (blame the unknown).** —— 全章的起点，也是全章存在的理由：**怀疑网络之前，先证明它**。本章最后一句话说的就是「这可能会**为网络开脱**，好让分析继续往下走」。
2. **Latency is an important metric for network performance and can be measured in different ways, including name resolution latency, ping latency, connection latency, first-byte latency, round-trip time, and connection life span.** —— **「延迟」不是一个数，是六个数**。这六个数对应六条完全不同的排查路径——**先确定是哪一种，再动手**。
3. **It may not exactly reflect the round-trip time of application requests, as ICMP may be handled with a different priority by routers.** —— ping 数据的**第一个系统性偏差**：路由器可能给 ICMP 降优先级。**ping 正常不等于应用正常。**
4. **Connection latency might be better termed connection establishment latency to clearly differentiate it from connection life span.** —— **两个词只差一个词，含义差一个数量级**（毫秒 vs 分钟）。命名不精确，性能讨论就会滑掉。
5. **While ping and connection latency measures the latency incurred by the network, first-byte latency includes the think time of the target server.** —— **首字节延迟高，很可能跟网络无关**——是服务器自己忙。**这一条能省下换硬件的钱。**
6. **Backlog drops and SYN retransmits are indicators of host overload.** —— **主机过载，不是网络过载**。看到重传，先怀疑自己的机器。
7. **Buffering can also be performed by external network components, such as switches and routers, in an effort to improve their own throughput.** —— 缓冲**在中间设备上**做，是为了它们自己的吞吐；**代价却由端点承担**。
8. **The function of buffering (or large buffering) may be best served by the endpoints—the hosts—and not the intermediate network nodes, following a principle called end-to-end arguments [Saltzer 84] .** —— **端到端原则**：缓冲这件事最好由端点做，不是由中间节点做。中间节点的大缓冲是 bufferbloat 的源头。
9. **Once a network interface direction reaches 100% utilization, it becomes a bottleneck, limiting performance.** —— **方向要分开算**：全双工下 TX 与 RX 互不相关，一个方向打满就是瓶颈。
10. **This algorithm (RFC 896) [Nagle 84] reduces the number of small packets on the network by delaying their transmission to allow more data to arrive and be coalesced.** —— **Nagle 是「攒小包」**。对控制类小包，它就是**延迟的来源**——`TCP_NODELAY` 存在的全部理由。
11. **This algorithm (RFC 1122) [Braden 89] delays the sending of ACKs up to 500 ms, so that multiple ACKs may be combined.** —— **延迟 ACK 最长 500 ms**。它和 Nagle 是一对**互锁组合**：一个攒发送、一个攒确认，**小消息的延迟天花板由此而来**。
12. **The initial window (IW) is the number of packets a TCP sender will transmit at the beginning of a connection before waiting for acknowledgment from the sender.** —— **IW 是短连接性能的命脉**：IW 覆盖不住全部待发数据，就要多等一个 RTT。
13. **The Linux default (10 packets, aka IW10) can be too high on slow links or when many connections start up** —— **默认值在慢链路上反而是错的**。「默认值合理」这句话要**带条件**才成立。
14. **A common source of issues is the presence of latency outliers caused by TCP retransmits.** —— **离群点是网络性能的头号敌人**，而**平均值会把它们藏起来**。所以要看分布、看逐条，不要只看平均。
15. **Note that some sources of extra latency are transient and only occur during system load.** —— **空闲时测的延迟不算数**。很多额外延迟只在负载下才出现。
16. **With kernel network stack bypass, instrumentation using traditional tools and metrics is not available because the counters and tracing events they use are also bypassed.** —— **你把观测工具一起旁路掉了**。性能与可观测性是一对，**不能只要其中一个**。
17. **Network tunable parameters are usually already tuned to provide high performance.** —— **调参之前先想清楚**：内核默认值通常已经不错，**而且网络栈本身会动态响应负载**。
18. **Disables Nagle, sending segments as soon as possible. This may improve latency at the cost of higher network utilization (more packets).** —— **`TCP_NODELAY` 的本质是一个交换**：拿更多的包换更低的延迟。**它在实时系统里几乎是必选项**——代价是网络利用率。
