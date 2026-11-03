# 网络多人游戏架构与编程

> 讲义来源：`书籍/教材讲义/网络多人游戏架构与编程/`（13 篇讲义：第1~13章）
> 原书：Joshua Glazer, *网络多人游戏架构与编程*（Networked Game Programming）

## 主题目录

| 目录 | 讨论的问题 | 文件数 |
| --- | --- | --- |
| `wire-format-encoding/` | 线格式怎么定：手写 Write/Read 镜像对 vs 成员表驱动的通用 Serialize；定长 vs 变长；主机序 vs 网络序 | 4 |
| `object-replication-strategies/` | 状态怎么搬过去：全量快照 vs 增量 vs 相关性裁剪（发给谁） | 4 |
| `latency-smoothing/` | 网络慢怎么让玩家感觉不到：冗余重发最新值 vs 插值 vs 航位推测 | 3 |
| `transport-reliability-model/` | 传输层怎么选：TCP 可靠有序 vs UDP 不可靠无序 vs 队头阻塞 | 3 |
| `socket-io-multiplexing/` | 一个进程怎么同时伺候一堆连接：阻塞 vs select vs poll vs epoll | 4 |
| `authority-topology/` | 谁说了算：客户端权威 vs 服务器权威 vs 监听服务器 vs 同步频率 | 4 |
| `simulation-timestep/` | 模拟步长定死还是跟着帧率走：固定步长 vs 可变步长 | 2 |

共 7 个主题目录 / 24 个实现文件 / 7 份主题 README。

## 本组约定

- 每个主题目录 = 一个**对照讨论**：同一问题的几种写法并列，各自独立编译、独立运行、**输出可横向对比**
- 单文件 ≤ 120 行，只验证一个机制
- 文件头四行元信息：`@book @topic @source @expect`；`@ub: yes` 表示故意演示未定义行为
- **教材引文只放主题 README，不进代码**；代码只保留机制本身
- 全部实现是**纯 ISO C++20**（只用标准库），不依赖 POSIX/Winsock：
  - 因此**不占端口、不挂起进程、不留残留**，在任何机器上跑结果都一样
  - 涉及 socket 语义的对照（第3章 select/poll/epoll）以**用户态模型**实现就绪集合与兴趣表的语义
  - 涉及 TCP/UDP 的对照（第2/7章）用**离线链路模型**（严格照讲义 §7.7 的四步）跑确定性的丢包 / 抖动 / 乱序
- 两个故意演示未定义行为的文件（默认跳过，`run_all.sh --ub` 才跑）：
  - `wire-format-encoding/04-unaligned-read-ub.cpp`——字节流未对齐按 uint32 读取
  - `object-replication-strategies/04-lost-create-overrun-ub.cpp`——创建包丢失后读头越过包长

## 跑法

```bash
cd lab
python3 -I tools/lab_check.py                 # 守卫: 目录结构 / 元信息 / 颗粒度
bash tools/build_all.sh                       # 全部文件的语法检查
for f in books/game-arch/*/0*.cpp; do bash tools/check_one.sh "$f"; done
bash tools/run_all.sh --ub                    # 连 UB 演示一起跑
```

单独编译某个文件：

```bash
g++ -std=c++20 -O2 -Wall -Wextra -pthread \
    books/game-arch/simulation-timestep/01-fixed-step-lockstep.cpp -o /tmp/kb-lab-build/ts01
```

## 本轮未覆盖的章节

- **第9章 可扩展性**：服务器分区、实例化、优先级与频率（本组只在 `simulation-timestep` 里引用了"时间膨胀"）
- **第10章 安全性**：明文 / 混淆 / 对称加密的往返开销对照
- **第11章 真实世界的引擎**：Unreal / Unity 的复制与 RPC 形态对照
- **第12章 玩家服务**：会话状态集中 vs 分片存储
- **第13章 云托管专用服务器**
