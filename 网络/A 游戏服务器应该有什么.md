---
title: "游戏服务器架构设计"
category: 计算机网络
tags:
  - net/server
  - networking
difficulty: 进阶
source: "自整理"
---
## 1. Reactor 架构深化：目前还缺多 Reactor / 多 I/O 线程
现在看起来你的游戏服务器更像是：
> 单 epoll I/O 线程 + worker 线程池处理业务
这是一个非常好的起点，但高性能网关/游戏服通常还会继续演进到：
```text
MainReactor
  └── 只负责 accept 新连接
SubReactor[N]
  ├── 每个线程一个 epoll
  ├── 每个连接绑定到固定 I/O 线程
  ├── 负责 read/write/close
  └── 通过 queue/eventfd 接收跨线程任务
```
建议新增模块：
```text
game_server/
├── event_loop.hpp
├── event_loop_thread.hpp
├── event_loop_thread_pool.hpp
├── channel.hpp
├── poller_epoll.hpp
```
这会让你的服务器从「demo 级 epoll」进化到更接近 muduo / libevent / nginx worker 模型的网络库结构。
**优先级：最高。**
## 2. Buffer 模块：现在读写缓冲应该独立成高性能组件
游戏服务器网络层里，Buffer 非常关键。你现在已经有 session 里的输入/输出缓冲，但后面建议抽象成独立模块：
```text
game_server/
├── buffer.hpp
```
需要支持：
- prependable / readable / writable 三段式缓冲区
- 自动扩容
- `readFd()` 使用 `readv` 一次读到栈外缓冲，减少系统调用
- `append()`
- `retrieve()`
- `peek()`
- `readableBytes()`
- `writableBytes()`
典型结构：
```text
| prependable | readable data | writable space |
```
这块很适合你深耕网络，因为它能直接影响：
- 半包/粘包处理
- 内存拷贝次数
- 系统调用次数
- 写缓冲堆积处理
- 背压控制
**优先级：最高。**
---

------

## 3. 写队列与背压：目前需要防止慢连接拖垮服务器

游戏服务器很容易遇到这种情况：

> 某个客户端网络很差，服务器不断给它发消息，结果它的 socket 写不出去，发送缓冲无限增长。

你需要补一个明确的**写侧背压机制**。

建议新增：



```text
session.hpp
```

里补充：

- per-session output buffer 高水位线
- 超过高水位线后：
  - 暂停广播给该连接
  - 或主动断开慢连接
  - 或降低消息频率
- EPOLLOUT 只在真的有待发送数据时注册
- 写完后取消 EPOLLOUT，避免 busy loop

建议设计参数：



```cpp
static constexpr size_t kHighWaterMark = 4 * 1024 * 1024;
static constexpr size_t kMaxPacketSize = 64 * 1024;
```

这对于游戏服务器非常重要，因为真实环境里慢客户端、移动网络、丢包、弱网都很常见。

**优先级：最高。**

------

## 4. 定时器模块：目前心跳有了，但需要通用 TimerWheel / MinHeap Timer

你现在已经有心跳超时检查，但高性能游戏服务器需要一个通用定时器系统。

建议新增：



```text
game_server/
├── timer.hpp
├── timer_queue.hpp
```

用途：

- 心跳超时
- 匹配超时
- 房间超时
- 技能 CD
- 战斗 tick
- 延迟任务
- 重传超时
- 限流窗口清理
- session idle timeout

可以先实现两种之一：

### 方案 A：最小堆定时器

适合学习，简单清晰：



```text
priority_queue<Timer>
```

### 方案 B：时间轮

更接近游戏服务器：



```text
TimingWheel
  ├── slot[0]
  ├── slot[1]
  ├── ...
  └── slot[N]
```

如果你要做大量玩家、房间、技能 CD，时间轮更有价值。

**优先级：高。**

------

## 5. 网络协议层：目前有基础消息协议，但缺协议演进能力

你现在已经有 [game_server/message.hpp](vscode-webview://1tnovoi3r5o06uo4c4mddep4m506in0p6jkpb25vg9lqab3j36fa/game_server/message.hpp)，这很好。接下来建议继续扩展为更完整的协议层：



```text
game_server/
├── protocol.hpp
├── codec.hpp
├── message_dispatcher.hpp
```

建议补这些能力：

### 必备

- message id 路由
- version 字段兼容
- request id / sequence id
- 错误码
- 最大包长限制
- 非法包断连
- 协议统计

### 进阶

- protobuf / flatbuffers / msgpack 支持
- 压缩标记
- 加密标记
- 协议版本灰度
- 客户端/服务端协议生成工具

游戏服务器后面一定会遇到：



```text
客户端 A 是旧版本
客户端 B 是新版本
部分消息字段新增
部分消息要灰度
部分房间内消息要广播
部分消息只允许登录后发送
```

所以协议层不能只停留在「能 encode/decode」。

**优先级：高。**

------

## 6. 消息分发器：现在业务逻辑还可以再解耦

建议增加：



```text
game_server/
├── message_handler.hpp
├── dispatcher.hpp
```

目标是把这种逻辑：



```cpp
if message.type == LOGIN
if message.type == MATCH
if message.type == MOVE
```

变成：



```cpp
dispatcher.registerHandler(MessageType::Login, handleLogin);
dispatcher.registerHandler(MessageType::Match, handleMatch);
dispatcher.registerHandler(MessageType::Move, handleMove);
```

进一步可以支持：

- 登录前允许的消息白名单
- 房间内才允许的消息
- 鉴权中间件
- 限流中间件
- 统计中间件
- trace id

这会让你后面加游戏业务不至于把 [game_server/game_server.hpp](vscode-webview://1tnovoi3r5o06uo4c4mddep4m506in0p6jkpb25vg9lqab3j36fa/game_server/game_server.hpp) 写成巨型类。

**优先级：高。**

------

## 7. UDP / KCP / QUIC：如果主攻游戏网络，这块非常关键

目前你主要是 TCP 服务器。游戏服务器如果想深入网络，建议后面一定补：



```text
game_server/
├── udp_server.hpp
├── kcp_connection.hpp
├── reliable_udp.hpp
```

不同游戏类型选择不同传输层：

| 游戏类型              | 常见选择                   |
| --------------------- | -------------------------- |
| 回合制 / 卡牌 / 棋牌  | TCP 足够                   |
| MMO                   | TCP 或 TCP + UDP           |
| FPS / MOBA / 实时动作 | UDP / KCP / 自研可靠 UDP   |
| Web 游戏              | WebSocket                  |
| 全球化弱网            | QUIC / KCP / RakNet 类方案 |

你可以分阶段学：

1. UDP echo server
2. UDP session 伪连接
3. 心跳 + 超时
4. sequence id
5. ack
6. resend
7. jitter buffer
8. RTT 估算
9. 拥塞控制
10. KCP 接入或自研简版可靠 UDP

你已经有 [src/include/rtt_estimator.hpp](vscode-webview://1tnovoi3r5o06uo4c4mddep4m506in0p6jkpb25vg9lqab3j36fa/src/include/rtt_estimator.hpp)，这个可以直接作为可靠 UDP 的基础。

**优先级：很高，尤其符合你的目标。**

------

## 8. 帧同步 / 状态同步：游戏服务器网络的核心业务层

如果目标是游戏高性能服务器，不能只停留在 socket 层。你还需要做游戏同步模型。

建议新增：



```text
game_server/
├── sync/
│   ├── frame_sync.hpp
│   ├── state_sync.hpp
│   ├── snapshot.hpp
│   ├── input_command.hpp
│   └── interpolation.hpp
```

### 帧同步要覆盖

- 客户端上传输入
- 服务端收集输入
- 固定 tick 广播
- lockstep
- 输入延迟
- 掉线处理
- 重连补帧

### 状态同步要覆盖

- entity snapshot
- delta compression
- AOI 可见性裁剪
- 插值 / 外推
- 状态校验

游戏网络真正难的地方很多都在这里：



```text
延迟、抖动、丢包、乱序、重传、预测、回滚、插值、同步一致性
```

**优先级：高。**

------

## 9. AOI / Interest Management：多人游戏必备

如果你以后做 MMO、房间战斗、地图同步，一定需要 AOI。

建议新增：



```text
game_server/
├── aoi/
│   ├── grid_aoi.hpp
│   ├── entity.hpp
│   └── interest_manager.hpp
```

需要解决：

- 玩家只收到附近实体消息
- 进入视野
- 离开视野
- 移动更新
- 广播裁剪
- 房间内大量对象时减少消息量

常见实现：

| AOI 算法     | 适合           |
| ------------ | -------------- |
| 九宫格 Grid  | 入门、MMO 地图 |
| 十字链表     | 2D 大地图      |
| 四叉树       | 空间分布不均   |
| BVH / KDTree | 更复杂场景     |

**优先级：中高。**

------

## 10. 内存池 / 对象池：高性能服务器需要控制分配开销

目前线程池、session、message 这些大概率还是依赖普通堆分配。后面可以补：



```text
game_server/
├── memory_pool.hpp
├── object_pool.hpp
├── slab_allocator.hpp
```

优先用在：

- Message 对象
- Session 对象
- Timer 对象
- Room 对象
- Packet buffer
- 任务队列节点

目标：

- 减少 `new/delete`
- 降低内存碎片
- 提高缓存命中率
- 降低高并发下 allocator 锁竞争

不过这块建议在 Reactor / Buffer / Timer 后再做，不要太早陷入过度优化。

**优先级：中。**

------

## 11. 无锁队列 / MPSC 队列：跨线程投递必备

如果后面你做多 Reactor，会遇到：



```text
业务线程想让某个 I/O 线程给连接发消息
```

这时就需要跨线程任务投递：



```text
worker thread
  -> push task to io thread queue
  -> eventfd wakeup io thread
```

建议新增：



```text
game_server/
├── concurrent_queue.hpp
├── eventfd_notifier.hpp
```

常见结构：

- MPSC queue：多个业务线程投递到一个 I/O 线程
- eventfd：唤醒 epoll
- pending functors：I/O 线程内执行任务

这块是从「单 epoll demo」走向「真正网络库」的关键。

**优先级：高。**

------

## 12. 限流 / 防刷 / 连接治理：线上服务器必须有

建议新增：



```text
game_server/
├── rate_limiter.hpp
├── connection_limiter.hpp
├── token_bucket.hpp
```

需要覆盖：

- 单 IP 最大连接数
- 单 IP 每秒新建连接数
- 单 session 每秒消息数
- 单消息类型频率限制
- 登录接口限流
- 匹配接口限流
- 黑名单 / 白名单
- 异常包断连
- 空闲连接清理

游戏服务器很容易被：



```text
狂点匹配
狂发登录
狂发移动包
恶意大包
慢连接
半连接洪泛
```

虽然完整 DDoS 防护不是应用层能完全解决，但应用层治理必须有。

**优先级：高。**

------

## 13. 认证与安全协议：目前缺登录安全模型

现在如果只是 demo 登录可以，但真实游戏服至少需要：



```text
game_server/
├── auth.hpp
├── token.hpp
```

能力：

- token 校验
- session 绑定 user id
- 重复登录踢下线
- 登录态过期
- 协议签名
- replay 防护
- 简单消息加密
- 敏感消息校验

注意：游戏服通常不会直接做完整账号系统，而是：



```text
客户端 -> 登录服认证 -> 拿 token -> 连接网关/游戏服 -> 游戏服校验 token
```

所以你后面可以拆：



```text
login_server/
gateway_server/
game_server/
```

**优先级：中高。**

------

## 14. 网关 Gateway：高性能游戏服务器通常不是单体

你现在的 [game_server/](vscode-webview://1tnovoi3r5o06uo4c4mddep4m506in0p6jkpb25vg9lqab3j36fa/game_server/) 更像「单体游戏服」。后面建议演化成：



```text
client
  -> gateway_server
      -> login_server
      -> match_server
      -> room_server
      -> battle_server
```

建议新增目录：



```text
gateway_server/
match_server/
room_server/
battle_server/
common/
```

或者先在一个进程内模拟模块边界：



```text
game_server/
├── gateway/
├── match/
├── room/
├── battle/
├── common/
```

网关负责：

- 客户端长连接
- 协议解码
- 鉴权
- 限流
- 路由
- 转发
- 踢人
- 断线重连

战斗服负责：

- 房间 tick
- 状态同步
- 战斗逻辑
- 广播

匹配服负责：

- 匹配池
- 段位 / 地区 / 模式匹配
- 超时扩大匹配范围

**优先级：中。**

------

## 15. 服务间通信：缺内部 RPC / 消息总线

如果未来拆多进程，你需要服务间通信：



```text
game_server/
├── rpc/
│   ├── rpc_channel.hpp
│   ├── rpc_codec.hpp
│   └── service_registry.hpp
```

或者简单一点：



```text
internal_message.hpp
internal_client.hpp
internal_server.hpp
```

可以支持：

- gateway -> room
- room -> match
- room -> db
- battle -> gateway
- server -> server broadcast

技术选型可以从简单 TCP 私有协议开始，不急着上 gRPC。

**优先级：中。**

------

## 16. 数据持久化：缺 Redis / DB 模块

游戏服务器通常需要：



```text
game_server/
├── storage/
│   ├── redis_client.hpp
│   ├── mysql_client.hpp
│   └── player_repository.hpp
```

最开始可以先做 Redis：

- token 校验
- 在线状态
- 玩家基础信息
- 匹配队列
- 房间状态
- 排行榜
- 分布式锁

再做 MySQL：

- 账号数据
- 角色数据
- 背包
- 战绩
- 充值订单

但这个方向偏后端工程，不如 Reactor / UDP / 同步模型那么贴近「深耕网络」。

**优先级：中低。**

------

## 17. 可观测性：现在日志有了，但缺 metrics / tracing / profiling

你现在有 [game_server/logger.hpp](vscode-webview://1tnovoi3r5o06uo4c4mddep4m506in0p6jkpb25vg9lqab3j36fa/game_server/logger.hpp)，但线上需要更完整：



```text
game_server/
├── metrics.hpp
├── profiler.hpp
├── trace.hpp
```

建议统计：

- 当前连接数
- 每秒新连接数
- 每秒断开数
- 每秒入包数
- 每秒出包数
- 平均包大小
- epoll wait 次数
- 业务队列长度
- worker 队列延迟
- 消息处理耗时
- 房间数
- 匹配人数
- 广播耗时
- 慢消息 handler

可以暴露：



```text
/metrics
```

给 Prometheus。

**优先级：中高。**

------

## 18. 压测工具：你现在还缺专门的 game client benchmark

高性能服务器一定要有自己的压测工具。

建议新增：



```text
tools/
├── game_client_bench.cpp
├── connection_storm.cpp
├── packet_replay.cpp
└── latency_probe.cpp
```

需要支持：

- 建立 N 个连接
- 模拟登录
- 模拟匹配
- 模拟进房间
- 定时发心跳
- 定时发移动包
- 统计 RTT
- 统计 QPS
- 统计断线率
- 统计 p50 / p95 / p99 延迟
- 模拟慢客户端
- 模拟半包发送
- 模拟大包攻击
- 模拟断线重连

没有压测，你很难判断优化是否有效。

**优先级：最高。**

------

## 19. 异常测试 / 协议 fuzz：网络服务器很需要

建议新增：



```text
tests/
├── protocol_fuzz_test.cpp
├── session_test.cpp
├── timer_test.cpp
├── buffer_test.cpp
```

重点测：

- 半包
- 粘包
- 空包
- 超大包
- 错误 magic
- 错误 version
- 错误 length
- 随机 bytes
- 多个消息连续 decode
- decode 后 buffer 状态
- 慢写
- 对端半关闭
- 对端 reset

这类测试对网络服务器非常有价值。

**优先级：高。**

------

## 20. 配置系统：缺运行时参数管理

建议新增：



```text
game_server/
├── config.hpp
```

支持：

- 监听 IP
- 端口
- worker 数量
- max connections
- max packet size
- heartbeat interval
- heartbeat timeout
- room size
- tick rate
- log level

现在这些如果写死，后面压测和调参会很麻烦。

**优先级：中。**

------

# 我建议你的后续路线

如果目标是「网络深耕 + 游戏服务器」，不要一上来做 DB、账号、复杂业务。建议按这个顺序：

## 第一阶段：把 TCP 网络库打扎实

1. 独立 Buffer 模块
2. 写队列 + 高水位背压
3. TimerQueue
4. EventLoop / Channel / EpollPoller 抽象
5. 多 Reactor
6. eventfd 跨线程唤醒
7. 压测工具

这一阶段完成后，你就有自己的小型 muduo 风格网络库。

------

## 第二阶段：把游戏协议层打扎实

1. Codec 独立
2. MessageDispatcher
3. message id 路由
4. session 状态权限校验
5. request id / sequence id
6. 协议 fuzz test
7. 版本兼容

这一阶段完成后，你的服务器才真正适合承载游戏业务。

------

## 第三阶段：深入游戏网络

1. UDP server
2. UDP session
3. RTT/RTO
4. reliable UDP
5. ACK / resend
6. jitter buffer
7. KCP 接入
8. 帧同步 demo
9. 状态同步 demo
10. AOI

这一阶段最符合你说的「深耕网络」。

------

## 第四阶段：工程化与线上能力

1. metrics
2. profiling
3. rate limiter
4. auth token
5. gateway
6. Redis
7. 服务拆分
8. 灰度 / 热更新配置

------

# 最推荐你现在马上补的 10 个模块

按优先级排序：



```text
1. Buffer 高性能缓冲区
2. 写队列 + 背压控制
3. TimerQueue / TimingWheel
4. EventLoop / Channel / EpollPoller 抽象
5. 多 Reactor / 多 I/O 线程
6. eventfd 跨线程唤醒
7. MessageDispatcher 消息分发器
8. game_client_bench 压测客户端
9. UDP session / reliable UDP 原型
10. 帧同步 demo
```

如果你只想选一个最应该做的，我建议：

> **先重构出 EventLoop + Channel + Buffer + TimerQueue。**

这四个是高性能 C++ 网络服务器的底座。后面无论你做 TCP 游戏服、UDP/KCP、网关、战斗服、压测、限流，都会复用它们。