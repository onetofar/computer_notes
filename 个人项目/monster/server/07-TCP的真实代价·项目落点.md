---
title: "07-TCP的真实代价 · 项目落点"
category: 多人网络
tags:
  - 多人网络
  - otf-game
  - 项目落点
difficulty: 进阶
source: '项目落点：F:\game_project\otf-server + otf-game｜教材依据见同名「教材精读」篇'
created: 2026-10-06
skills:
  - obsidian-markdown
---

> [!info] 本篇是**项目落点**
> 教材依据（原文摘录、出处行号、与原文的出入）在 [[07-TCP的真实代价]]。
> 本篇只回答一个问题：**这些判据在 otf-server / otf-game 里落到哪一行。**
> 代码在动，教材不变 —— 两者分开就是这个原因。

## 本项目落点

| 项 | 决定 |
|---|---|
| 传输层 | **raw TCP，自定长帧**（决策 D1） |
| `TCP_NODELAY` | **建连处必设**，写进 M2 验收项（决策 D9） |
| `SO_REUSEADDR` | 建连处必设（见 [[01-彭放-epoll-Reactor工程规范]]） |
| `SO_RCVBUF` / `SO_SNDBUF` | **不调**。带宽余量 20×，调它只带来"为什么我调了没用"的困惑 |
| 备选方案 | ENet（epoll 被内部封装，**展示价值归零**）/ WebSocket（面向网页端）/ 裸 UDP（可靠性要自己做，教科书级工作量） |

**为什么敢选 TCP**：本项目是 PvE 共斗，属格雷泽容忍度表里 **RTS 那一档（可到 500ms）**。
延迟预算本来就宽裕，TCP 的那点代价在预算内可消化——
**前提是 `TCP_NODELAY` 设了**。

### 2026-10-07 K4 落地：TCP_NODELAY 的真实落点

- **代码**：`otf-server/src/net/tcp_transport.cpp`，`AcceptNew()` 内 `setsockopt(IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one))`
- **commit**：`fc1276f`（K4-4 首次落地）→ `8bc68e8`（修复波 A-P0a 补返回值检查）
  <br>⚠️ 早前此处误记为 `a82bd40`（K4 收尾），2026-10-07 订正。
- **反向验证**：把 `setsockopt` 删掉 → 测试立刻红。**这条断言查的是服务端 accepted fd**，
  不是客户端（查客户端是恒真的假断言——brief 最初就写错了，被实现者发现并改正）

**为什么它是硬要求**（格雷泽 §7.4 的原话是"使用 TCP 的游戏通常禁用 Nagle"，
不是"应该用 UDP"）：`PlayerInput` 30Hz、`WorldSnapshot` 10Hz 都是小包，
Nagle 会把它们攒着等下一个包，**手感直接崩**。

**一条被顺带证伪的替代方案**：换 UDP 解决不了这个问题。
本项目下行 ≈1.0 KB/s、上行 ≈0.4 KB/s（§9.2），带宽余量 20 倍——
UDP 相对 TCP 的核心优势（省带宽、不受队头阻塞）**收益为零**，
而它要付出的是：帧格式的 `len` 前缀失去意义、`TCP` 的有序交付免疫要自己用
`server_tick` 补、拥塞控制/PMTU/重传全要自己做。
**真正要 UDP 的那天，条件是玩家数上到 30+ 或单包逼近 1400B 上限需要 AOI**（§7.3 给了换轨条件）。
现在一个都不成立。

**但架构上留了口子**：`src/net/transport.h` 定义的是**字节流语义**的 `Transport` 抽象。
UDP 实现只需把"拆帧"换成"整个 datagram = 一帧"，上层（收件箱/心跳/超时/广播）零改动。
**这个口子是设计出来的，不是注释里的空话**——它已经是 `TcpTransport` 的父类。
