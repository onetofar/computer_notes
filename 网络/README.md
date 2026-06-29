这是一系列网络协议与Linux网络编程笔记。

当我回顾Java底层，发现Java的黑科技本质是保护程序员的工具——屏蔽了内存、屏蔽了指针、屏蔽了数据包的真实结构。但要真正拆解网络，Java做不到。抓包、解构、重现——这些需要直接操作字节、解析协议头、理解内核等待队列的能力，只有C++能给你。这也是我学习游戏服务器编程的必经之路。而网络模块，作为操作系统和应用程序的交界层，是这条路上最好的起点。

转码之前，我以为网络就是Socket，高性能的Netty。转码之后，从ARP一路追到epoll源码，才知道“数据包怎么从网卡跑到你的buffer里”是一条完整的确知链——以太网帧、IP分片、TCP状态机、等待队列回调——每一层都是确定的，每一层都能被拆穿。这就是我想要的：一个从头到尾没有模糊地带的知识体系。

系列笔记覆盖《TCP/IP详解 卷一》、《Linux高性能服务器编程》《C++ Primer》的核心章节，部分参考《Netty权威指南》的设计思想对比。从链路层ARP抓包开始，到epoll源码的rdlist删除机制结束——这是我自底向上学习计划中最硬的一块。后面学C++并发、Reactor框架、无锁队列时，我会反复回到这些笔记里找线索。因为我已经知道，系统不是黑盒，它是一个可以被拆到底层、被自己亲手实现的确定世界。

---

## 📂 笔记目录

> 按自底向上的学习路径排列：链路层 → IP 层 → 传输层 → Socket 编程 → IO 复用。编号 `N-M` 中 `N` 为阶段、`M` 为序号，`N-a/b/c` 为对应阶段的实验/踩坑笔记。

### 一、链路层与网络基础

**理论**
- [网络体系结构与分层模型](./1-0 网络前置知识.md)
- [以太网帧结构与链路层](./1-1 链路层与网络基础.md)
- [ARP 地址解析协议](./1-2 arp笔记.md)
- [IPv4/IPv6 头部结构](./1-3 ipv4和ipv6头部结构.md)

**实验与踩坑**
- [Docker 配置踩坑](./1-a docker配置踩坑.md)
- [WSL2 + Docker C++ 开发环境](./1-b WSL2 + Docker C++ 开发环境搭建记录.md)
- [全链路 ARP 发包抓包](./1-c WSL2+Docker+VSCode 全链路 ARP 发包抓包.md)
- [Ping 网关连通性测试](./1-d ping 网关 192.168.1.1.md)
- [RAII Socket 封装实践](./1-e 创建RAII socket类 发请求.md)
- [Windows/MSYS2 服务器环境](./1-f Windows做服务器 MSYS2 C++ 开发环境  Docker 做客户端.md)

### 二、IP 层与 UDP

**理论**
- [ICMPv4 协议与 IP 选路](./2-0 前置知识.md)
- [UDP 数据报格式与校验和](./2-1 UDP头部.md)
- [IP 分片与重组机制](./2-2 IP分片.md)
- [Linux Socket 函数基础](./2-3 linux下的socket函数.md)

**实验与踩坑**
- [Wireshark 抓包：IP/UDP 头部解析](./2-a wireshark抓ip头和udp头解析和udp校验和分步计算.md)
- [Wireshark 抓取 IP 分片实验](./2-b 尝试抓取分片.md)
- [IP 分片踩坑记录](./2-c 分片踩坑.md)

### 三、TCP 协议

**理论**
- [TCP 头部字段全解](./3-1 TCP头部.md)
- [三次握手与四次挥手](./3-2 TCP的连接建立与终止.md)
- [RTO 重传超时与 RTT 计算](./3-3 TCP的重传与超时.md)
- [epoll 核心流程详解](./3-4 epoll笔记.md)
- [Netty EventLoop 模型 C++ 衍生](./3-4 netty的eventloop模型衍生cpp.md)

**实验与踩坑**
- [Wireshark 监控 TCP 头部](./3-a 监控本地服务器tcp头部.md)
- [Seq/Ack 序列号模拟分析](./3-b seq_ack_simulator文件模拟客户端发送分析.md)
- [阻塞式 Echo 服务器](./3-d blocking_echo_server.md)
- [回调式 Echo 服务器](./3-e callback_echo_server.md)

### 四、流量控制与拥塞控制

**理论**
- [滑动窗口与流量控制](./4-1 流量控制以及滑动窗口.md)
- [拥塞控制：慢启动 / 拥塞避免 / 快速恢复](./4-2 拥塞控制.md)

**实验与踩坑**
- [拥塞控制 Shell 脚本观测](./4-a 拥塞控制 sh脚本观测解析.md)

### 五、Socket 选项与 IO 复用

**理论**
- [Socket 选项全集](./5-1 socket选项.md)
- [select / poll / epoll IO 复用技术](./5-2 IO复用相关技术.md)
- [select / poll / epoll 源码分析](./5-2-1 select poll epoll源码分析.md)
- [epoll LT 与 ET 触发模式](./5-2-2 epoll下LT和ET的具体差别.md)
- [epoll EPOLLONESHOT 实现](./5-2-3 epoll下EPOLLONESHOT 具体实现.md)
- [IO 复用应用：非阻塞 connect](./5-2-4 IO复用的应用之一-非阻塞的connect.md)

**实验与踩坑**
- [epoll 非阻塞 connect 性能对比](./5-a epoll下非阻塞connect性能差异.md)

### 六、附录：参考与工具

- [TCP/IP 参考书籍目录](./A 书籍目录.md)
- [TCP/IP 详解卷一 英文版翻译](./A TCPIP详解卷一英文版翻译.md)
- [各网络协议头部汇总](./A 各个网络协议的头部.md)
- [Linux 常用检测工具](./A linux常用检测工具.md)
- [服务器常用命令](./A 服务器常用命令.md)
- [游戏服务器架构设计](./A 游戏服务器应该有什么.md)
- [网络与游戏服务器引擎学习计划](./A 网络 + 游戏服务器引擎深度学习计划.md)
- [Netty IO 模型演进](./A Netty的演进.md)
- [Skynet 游戏服务器框架](./A skynet源码指南.md)
- [Claude Code 使用笔记](./A Claude Code.md)
- [C++ 基础笔记](./A cpp第一部分_cpp基础笔记.md)
- [C++ 标准库笔记](./A cpp第二部分_cpp标准库.md)
- [C++ 可调用对象统一框架](./C++ 可调用对象的统一理论框架.md)

### 七、附录：构建与配置

- [CMake 与 clang-format 配置](./B CMakeList.txt 与 .clang-Format文件详解.md)
- [socket.hpp 源码解析](./B socket.hpp文件解析.md)