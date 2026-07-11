这是一系列网络协议与Linux网络编程笔记。

当我回顾Java底层，发现Java的黑科技本质是保护程序员的工具——屏蔽了内存、屏蔽了指针、屏蔽了数据包的真实结构。但要真正拆解网络，Java做不到。抓包、解构、重现——这些需要直接操作字节、解析协议头、理解内核等待队列的能力，只有C++能给你。这也是我学习游戏服务器编程的必经之路。而网络模块，作为操作系统和应用程序的交界层，是这条路上最好的起点。

转码之前，我以为网络就是Socket，高性能的Netty。转码之后，从ARP一路追到epoll源码，才知道“数据包怎么从网卡跑到你的buffer里”是一条完整的确知链——以太网帧、IP分片、TCP状态机、等待队列回调——每一层都是确定的，每一层都能被拆穿。这就是我想要的：一个从头到尾没有模糊地带的知识体系。

系列笔记覆盖《TCP/IP详解 卷一》、《Linux高性能服务器编程》《C++ Primer》的核心章节，部分参考《Netty权威指南》的设计思想对比。从链路层ARP抓包开始，到epoll源码的rdlist删除机制结束——这是我自底向上学习计划中最硬的一块。后面学C++并发、Reactor框架、无锁队列时，我会反复回到这些笔记里找线索。因为我已经知道，系统不是黑盒，它是一个可以被拆到底层、被自己亲手实现的确定世界。

---

## 📂 笔记目录

> 按自底向上的学习路径排列：链路层 → IP 层 → 传输层 → Socket 编程 → IO 复用。编号 `N-M` 中 `N` 为阶段、`M` 为序号，`N-a/b/c` 为对应阶段的实验/踩坑笔记。

### 一、链路层与网络基础

**理论**
- [网络体系结构与分层模型](./1-0%20%E7%BD%91%E7%BB%9C%E5%89%8D%E7%BD%AE%E7%9F%A5%E8%AF%86.md)
- [以太网帧结构与链路层](./1-1%20%E9%93%BE%E8%B7%AF%E5%B1%82%E4%B8%8E%E7%BD%91%E7%BB%9C%E5%9F%BA%E7%A1%80.md)
- [ARP 地址解析协议](./1-2%20arp%E7%AC%94%E8%AE%B0.md)
- [IPv4/IPv6 头部结构](./1-3%20ipv4%E5%92%8Cipv6%E5%A4%B4%E9%83%A8%E7%BB%93%E6%9E%84.md)

**实验与踩坑**
- [Docker 配置踩坑](./1-a%20docker%E9%85%8D%E7%BD%AE%E8%B8%A9%E5%9D%91.md)
- [WSL2 + Docker C++ 开发环境](./1-b%20WSL2%20%2B%20Docker%20C%2B%2B%20%E5%BC%80%E5%8F%91%E7%8E%AF%E5%A2%83%E6%90%AD%E5%BB%BA%E8%AE%B0%E5%BD%95.md)
- [全链路 ARP 发包抓包](./1-c%20WSL2%2BDocker%2BVSCode%20%E5%85%A8%E9%93%BE%E8%B7%AF%20ARP%20%E5%8F%91%E5%8C%85%E6%8A%93%E5%8C%85.md)
- [Ping 网关连通性测试](./1-d%20ping%20%E7%BD%91%E5%85%B3%20192.168.1.1.md)
- [RAII Socket 封装实践](./1-e%20%E5%88%9B%E5%BB%BARAII%20socket%E7%B1%BB%20%E5%8F%91%E8%AF%B7%E6%B1%82.md)
- [Windows/MSYS2 服务器环境](./1-f%20Windows%E5%81%9A%E6%9C%8D%E5%8A%A1%E5%99%A8%20MSYS2%20C%2B%2B%20%E5%BC%80%E5%8F%91%E7%8E%AF%E5%A2%83%20%20Docker%20%E5%81%9A%E5%AE%A2%E6%88%B7%E7%AB%AF.md)

### 二、IP 层与 UDP

**理论**
- [ICMPv4 协议与 IP 选路](./2-0%20%E5%89%8D%E7%BD%AE%E7%9F%A5%E8%AF%86.md)
- [UDP 数据报格式与校验和](./2-1%20UDP%E5%A4%B4%E9%83%A8.md)
- [IP 分片与重组机制](./2-2%20IP%E5%88%86%E7%89%87.md)
- [Linux Socket 函数基础](./2-3%20linux%E4%B8%8B%E7%9A%84socket%E5%87%BD%E6%95%B0.md)

**实验与踩坑**
- [Wireshark 抓包：IP/UDP 头部解析](./2-a%20wireshark%E6%8A%93ip%E5%A4%B4%E5%92%8Cudp%E5%A4%B4%E8%A7%A3%E6%9E%90%E5%92%8Cudp%E6%A0%A1%E9%AA%8C%E5%92%8C%E5%88%86%E6%AD%A5%E8%AE%A1%E7%AE%97.md)
- [Wireshark 抓取 IP 分片实验](./2-b%20%E5%B0%9D%E8%AF%95%E6%8A%93%E5%8F%96%E5%88%86%E7%89%87.md)
- [IP 分片踩坑记录](./2-c%20%E5%88%86%E7%89%87%E8%B8%A9%E5%9D%91.md)

### 三、TCP 协议

**理论**
- [TCP 头部字段全解](./3-1%20TCP%E5%A4%B4%E9%83%A8.md)
- [三次握手与四次挥手](./3-2%20TCP%E7%9A%84%E8%BF%9E%E6%8E%A5%E5%BB%BA%E7%AB%8B%E4%B8%8E%E7%BB%88%E6%AD%A2.md)
- [RTO 重传超时与 RTT 计算](./3-3%20TCP%E7%9A%84%E9%87%8D%E4%BC%A0%E4%B8%8E%E8%B6%85%E6%97%B6.md)
- [epoll 核心流程详解](./3-4%20epoll%E7%AC%94%E8%AE%B0.md)
- [Netty EventLoop 模型 C++ 衍生](./3-4%20netty%E7%9A%84eventloop%E6%A8%A1%E5%9E%8B%E8%A1%8D%E7%94%9Fcpp.md)

**实验与踩坑**
- [Wireshark 监控 TCP 头部](./3-a%20%E7%9B%91%E6%8E%A7%E6%9C%AC%E5%9C%B0%E6%9C%8D%E5%8A%A1%E5%99%A8tcp%E5%A4%B4%E9%83%A8.md)
- [Seq/Ack 序列号模拟分析](./3-b%20seq_ack_simulator%E6%96%87%E4%BB%B6%E6%A8%A1%E6%8B%9F%E5%AE%A2%E6%88%B7%E7%AB%AF%E5%8F%91%E9%80%81%E5%88%86%E6%9E%90.md)
- [阻塞式 Echo 服务器](./3-d%20blocking_echo_server.md)
- [回调式 Echo 服务器](./3-e%20callback_echo_server.md)

### 四、流量控制与拥塞控制

**理论**
- [滑动窗口与流量控制](./4-1%20%E6%B5%81%E9%87%8F%E6%8E%A7%E5%88%B6%E4%BB%A5%E5%8F%8A%E6%BB%91%E5%8A%A8%E7%AA%97%E5%8F%A3.md)
- [拥塞控制：慢启动 / 拥塞避免 / 快速恢复](./4-2%20%E6%8B%A5%E5%A1%9E%E6%8E%A7%E5%88%B6.md)

**实验与踩坑**
- [拥塞控制 Shell 脚本观测](./4-a%20%E6%8B%A5%E5%A1%9E%E6%8E%A7%E5%88%B6%20sh%E8%84%9A%E6%9C%AC%E8%A7%82%E6%B5%8B%E8%A7%A3%E6%9E%90.md)

### 五、Socket 选项与 IO 复用

**理论**
- [Socket 选项全集](./5-1%20socket%E9%80%89%E9%A1%B9.md)
- [select / poll / epoll IO 复用技术](./5-2%20IO%E5%A4%8D%E7%94%A8%E7%9B%B8%E5%85%B3%E6%8A%80%E6%9C%AF.md)
- [select / poll / epoll 源码分析](./5-2-1%20select%20poll%20epoll%E6%BA%90%E7%A0%81%E5%88%86%E6%9E%90.md)
- [epoll LT 与 ET 触发模式](./5-2-2%20epoll%E4%B8%8BLT%E5%92%8CET%E7%9A%84%E5%85%B7%E4%BD%93%E5%B7%AE%E5%88%AB.md)
- [epoll EPOLLONESHOT 实现](./5-2-3%20epoll%E4%B8%8BEPOLLONESHOT%20%E5%85%B7%E4%BD%93%E5%AE%9E%E7%8E%B0.md)
- [IO 复用应用：非阻塞 connect](./5-2-4%20IO%E5%A4%8D%E7%94%A8%E7%9A%84%E5%BA%94%E7%94%A8%E4%B9%8B%E4%B8%80-%E9%9D%9E%E9%98%BB%E5%A1%9E%E7%9A%84connect.md)

**实验与踩坑**
- [epoll 非阻塞 connect 性能差异](./5-2-a%20epoll%E4%B8%8B%E9%9D%9E%E9%98%BB%E5%A1%9Econnect%E6%80%A7%E8%83%BD%E5%B7%AE%E5%BC%82.md)

**定时器**
- [SIGALRM 信号高性能定时器：时间轮](./5-3%20SIGALRM%E4%BF%A1%E5%8F%B7%E9%AB%98%E6%80%A7%E8%83%BD%E5%AE%9A%E6%97%B6%E5%99%A8%E2%80%94%E2%80%94%E6%97%B6%E9%97%B4%E8%BD%AE.md)
- [升序链表的定时器服务器源码分析](./5-3-a%20%E5%8D%87%E5%BA%8F%E9%93%BE%E8%A1%A8%E7%9A%84%E5%AE%9A%E6%97%B6%E5%99%A8%E6%9C%8D%E5%8A%A1%E5%99%A8%E6%BA%90%E7%A0%81%E5%88%86%E6%9E%90.md)

### 六、附录：参考与工具

- [TCP/IP 参考书籍目录](./A%20%E4%B9%A6%E7%B1%8D%E7%9B%AE%E5%BD%95.md)
- [TCP/IP 详解卷一 英文版翻译](./A%20TCPIP%E8%AF%A6%E8%A7%A3%E5%8D%B7%E4%B8%80%E8%8B%B1%E6%96%87%E7%89%88%E7%BF%BB%E8%AF%91.md)
- [各网络协议头部汇总](./A%20%E5%90%84%E4%B8%AA%E7%BD%91%E7%BB%9C%E5%8D%8F%E8%AE%AE%E7%9A%84%E5%A4%B4%E9%83%A8.md)
- [Linux 常用检测工具](./A%20linux%E5%B8%B8%E7%94%A8%E6%A3%80%E6%B5%8B%E5%B7%A5%E5%85%B7.md)
- [服务器常用命令](./A%20%E6%9C%8D%E5%8A%A1%E5%99%A8%E5%B8%B8%E7%94%A8%E5%91%BD%E4%BB%A4.md)
- [游戏服务器架构设计](./A%20%E6%B8%B8%E6%88%8F%E6%9C%8D%E5%8A%A1%E5%99%A8%E5%BA%94%E8%AF%A5%E6%9C%89%E4%BB%80%E4%B9%88.md)
- [网络与游戏服务器引擎学习计划](./A%20%E7%BD%91%E7%BB%9C%20%2B%20%E6%B8%B8%E6%88%8F%E6%9C%8D%E5%8A%A1%E5%99%A8%E5%BC%95%E6%93%8E%E6%B7%B1%E5%BA%A6%E5%AD%A6%E4%B9%A0%E8%AE%A1%E5%88%92.md)
- [Netty IO 模型演进](./A%20Netty%E7%9A%84%E6%BC%94%E8%BF%9B.md)
- [Skynet 游戏服务器框架](./A%20skynet%E6%BA%90%E7%A0%81%E6%8C%87%E5%8D%97.md)
- [Claude Code 使用笔记](./A%20Claude%20Code.md)
- [C++ 基础笔记](./A%20cpp%E7%AC%AC%E4%B8%80%E9%83%A8%E5%88%86_cpp%E5%9F%BA%E7%A1%80%E7%AC%94%E8%AE%B0.md)
- [C++ 标准库笔记](./A%20cpp%E7%AC%AC%E4%BA%8C%E9%83%A8%E5%88%86_cpp%E6%A0%87%E5%87%86%E5%BA%93.md)
- [C++ 可调用对象统一框架](./C%2B%2B%20%E5%8F%AF%E8%B0%83%E7%94%A8%E5%AF%B9%E8%B1%A1%E7%9A%84%E7%BB%9F%E4%B8%80%E7%90%86%E8%AE%BA%E6%A1%86%E6%9E%B6.md)

### 七、附录：构建与配置

- [CMake 与 clang-format 配置](./B%20CMakeList.txt%20%E4%B8%8E%20.clang-Format%E6%96%87%E4%BB%B6%E8%AF%A6%E8%A7%A3.md)
- [socket.hpp 源码解析](./B%20socket.hpp%E6%96%87%E4%BB%B6%E8%A7%A3%E6%9E%90.md)