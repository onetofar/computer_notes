---
title: "01-彭放-epoll-Reactor工程规范 · 项目落点"
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
> 教材依据（原文摘录、出处行号、与原文的出入）在 [[01-彭放-epoll-Reactor工程规范]]。
> 本篇只回答一个问题：**这些判据在 otf-server / otf-game 里落到哪一行。**
> 代码在动，教材不变 —— 两者分开就是这个原因。

## 本项目落点

> ⚠️ **下面这张表已被下方「2026-10-07 K4 落地」一节取代。**
> 它记录的是**设计时**的判断，保留是为了看出"当初为什么这么定"。
> 真实落点看那一节。**不要再从这张表取行号。**

| 教材结论 | 落到哪 | 怎么落 |
|---|---|---|
| LT 优先于 ET | `技术参考.md` §6.4.1 | 选 LT。**理由是权衡不是不会 ET**——带宽余量 20×，ET 收益为零而"漏读到 EAGAIN"的坑不低 |
| 读干净到 EAGAIN | M2 `frame_codec` | 即使选 LT 也保留这条纪律，让"改成 ET"不需要重新审计 |
| IO/逻辑分离 | `net/` vs `sim/` | epoll 回调只投递收件箱；tick 开始时统一取走 |
| fd 做下标 | M2 `epoll_reactor` | `std::array<Conn, kMaxPlayers>`，玩家数是已知上界 |
| 增删延迟应用 | M2 房间回收 | `pending_close` 标记 + 主循环尾部统一处理 |
| `SO_REUSEADDR` | M2 建连处 | 与 [[07-TCP的真实代价]] 的 `TCP_NODELAY` 放同一处 |

### 2026-10-07 K4 落地：真实行号回填

> 上表原为**设计时的计划位置**（`M2 epoll_reactor` 这类占位符）。
> 下表是**已核实的真实落点**。契约见 `docs/superpowers/specs/2026-10-07-backend-k4-k8-design.md`。
>
> **commit 对照**（行号会漂，commit 不会）：
> - `fc1276f` K4-4 `TcpTransport` LT Reactor —— 本表大部分纪律的首次落地（`TCP_NODELAY` / `EPOLLOUT` 按需 / 延迟销毁）
> - `8bc68e8` 修复波 A-P0a —— `SO_REUSEADDR`/`TCP_NODELAY` 补返回值检查、`MOD` 失败回滚 flag、判死后停止投递
> - `37ba1be` 修复波 A-P0b —— `kMaxFd` 编译期 constness、帧队列深度单一来源
> - `0164013` 修复波 B —— 接上主循环（服务器进程**第一次真正跑起来**）
> - `3426c8a` K4 收尾（回归修复）
>
> ⚠️ **本文早前把 K4 收尾写成 `a82bd40`，是错的** —— 那是 K4-4 的第二轮修复
> （修 `EPOLLOUT` 断言跨环境失败），不是收尾。已在 2026-10-07 订正。
> 这正是管线声明 §0 那条纪律的现实版本：**"写完了"要能指到一个 commit，指错就是没写完。**

| 纪律 | 真实落点 | 依据 |
|---|---|---|
| LT 优先于 ET | `src/net/tcp_transport.cpp` 全文无 `EPOLLET`；`Poll` 走 `epoll_wait` | 选 LT 是**权衡**：§9.2 带宽余量 20×，ET 收益为零而"漏读到 EAGAIN"的坑不低。答辩口径就是这个 |
| **读干净到 EAGAIN** | `tcp_transport.cpp:85` / `frame_codec.cpp:85` 的 `for(;;)` + `EAGAIN` 早退 | 即使选 LT 也保留这条，让"改成 ET"不需重新审计所有收包点 |
| IO/逻辑分离 | `transport.h:75` `OnBytes` 只投递字节；`frame_codec` 由上层持解码器 | ★ **实测违规并修正过**：`transport.h` 原注释指示"直接调 `FeedAll`"——那是 `net/` 越界知道 `FrameDecoder`，违反 §6.4.2 |
| fd 做下标 | `tcp_transport.h` `std::vector<Conn> conns_` + `At(fd)` | 全文无 `map<fd,Conn*>` |
| 增删延迟应用 | `Close` 挂 `pending_close`（`tcp_transport.cpp:167`）→ `ReapClosed` 才 `Destroy` | ★ 评审实测：把 `Close` 改成真关 → 4 条断言红。**这是"服务器必定宕机"那条** |
| `SO_REUSEADDR` | `tcp_transport.cpp:63-71`，**且检查返回值** | 失败会关 fd、置 -1、恢复 errno；不做则重启被 `TIME_WAIT` 挡住 |
| `TCP_NODELAY` | `AcceptNew` 内（建连处） | 反向验证：删掉 `setsockopt` → 断言红。**Nagle 最多 0.5s，不设手感崩**（§6.4.3） |
| `EPOLLOUT` 按需 | `tcp_transport.h:49-51` 三个计数器（`add_with_out` / `mod_with_out` / `mod_without_out`） | ★ 见下方"踩过的坑"——这一条**光看代码看不出来**，是本项目最反直觉的一条 |

### 踩过的坑（比纪律本身更值钱）

**① "EPOLLOUT 是不是常开"从代码里看不出来。**
Conn 里那个 `want_write` flag 是**记账**，不是事实——内核到底注册了什么，
`epoll_ctl` 没有查询 API。最初用"空队列时 `Poll(60)` 必须阻塞 ≥40ms"做行为断言，
**评审实测：在"永远挂 EPOLLOUT"这个变异下它照样绿**（排空之后 socket 真的不可写，
LT 就不报 EPOLLOUT）。**只有计数器能抓它。**
⇒ 正解是 `Ctl()` 包装所有 `epoll_ctl` 并记账，断言**发给内核的数**而非 flag。
**副作用**：这个计数器进了生产代码，评审做过 A/B——删掉它变异就存活，属**承重不可替代**。
它有真实运维价值："当前有多少注册带着 EPOLLOUT"能在线上发现虚假唤醒烧 CPU。

**② 测试的前提不成立时，断言会"莫名其妙地红"。**
EPOLLOUT 摘除那组断言依赖"发送缓冲会填满"，而 loopback 的 socket 缓冲
**可自动调优到 2.6MB**（实测，宿主/容器差 20×），于是容器里 `Send` 从不返回 EAGAIN，
摘除路径**从未执行** → 宿主绿、容器 3/3 稳定红。
修法不是加容差，是**把断言拆成两条**：
一条与缓冲无关的确定性往返（走 `Transport` 公开接口，空队列时 socket 必可写），
一条保留 Send 驱动但**显式区分前提**，不成立时打印含实测 `sndbuf` 的 `SKIP` 并计入汇总——
**绝不静默跳过**（一个永远绿的断言比没有断言更危险）。

**③ 归因写错比代码写错更危险。**
实现者曾在报告里写"时序验证挂在真实摘掉之后就能抓住 M2c"——**这句是错的**，
留着会诱导下一个人删掉那个承重的计数器。
⇒ 凡是写"X 是因为 Y"的地方，都必须能被独立复核。
