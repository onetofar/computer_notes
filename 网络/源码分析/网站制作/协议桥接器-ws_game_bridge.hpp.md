---
title: v3 协议桥接器—ws_game_bridge.hpp
category: 计算机网络
tags:
  - 源码分析
  - 网站制作
  - 无状态网关
  - v3
difficulty: 进阶
source: onetofar_web/backend/src/websocket/ws_game_bridge.hpp + .cpp
created: 2026-07-30
modified: 2026-07-30
maturity: 深入
---

> [!info] 一句话
> v3 的 `GameBridge` 是一个**无状态字节代理**——不再理解游戏协议，只做三件事：路由表管理、TCP 连接池、字节帧转发。

## v2 → v3 变化

| 维度 | v2 `WsGameBridge`（之前） | v3 `GameBridge`（现在） |
|------|--------------------------|------------------------|
| 行数 | ~650 行 | ~200 行 |
| 状态 | `BridgeSession` 存 session/room/logged_in | 只有 route_table（route_id → WS） |
| 业务知识 | 完整 `GameMessageType` 枚举 + payload 解析 | 零业务逻辑 |
| 连接模型 | 每个 WS → 一个独立 TCP | M:N 连接池（默认 2 条 TCP） |
| 重连 | 无 | 指数退避自动重连（1s→2s→...→30s） |
| WS 断开 | 清路由表 | 清路由表 + 发关闭帧通知 GameServer |

## 核心数据结构

```cpp
class GameBridge {
    // 路由表：route_id → WS 连接（唯一有状态处）
    shared_mutex route_mutex_;
    unordered_map<uint32_t, weak_ptr<WsConnection>> route_table_;
    unordered_map<WsConnection*, uint32_t> ws_to_route_;
    atomic<uint32_t> next_route_id_{1};

    // TCP 连接池
    struct TcpConn {
        shared_ptr<TcpClient> client;
        shared_ptr<TcpConnection> conn;
        bool connected{false};
        int reconnect_delay_ms{1000};  // 指数退避
        size_t index{0};
    };
    vector<unique_ptr<TcpConn>> tcp_pool_;
};
```

## 数据流

```
WS 连接 → on_ws_connect
  → route_id = next_route_id_++
  → route_table_[route_id] = ws_conn
  → ws_to_route_[ws_conn] = route_id

WS 消息 → on_ws_message
  → route_id = ws_to_route_[ws_conn]
  → pool_idx = route_id % pool_size
  → frame = make_frame(route_id, json)
       = [route_id(4)][json_len(4)][json_bytes]
  → tcp_pool_[pool_idx].conn->send(frame)

TCP 响应 → on_tcp_message
  → 循环解析 buf 中的帧：
    → route_id = read_u32(buf)
    → json_len = read_u32(buf + 4)
    → frame_len = 8 + json_len
    → ws = route_table_[route_id].lock()
    → if ws: ws->send_text(json_bytes)
    → buf->retrieve(frame_len)

WS 断开 → on_ws_close
  → 清理 route_table_ + ws_to_route_
  → 发送关闭帧：[route_id(4)][len=0]
  → GameServer 收到 len=0 → close_session_by_id()
```

## TCP 连接池

```cpp
void GameBridge::init_tcp_pool() {
    for (size_t i = 0; i < config_.tcp_pool_size; ++i) {
        auto tc = make_unique<TcpConn>();
        tc->index = i;
        tc->client = make_shared<TcpClient>(loop_, host, port);
        // 注册回调...
        tc->client->connect();
        tcp_pool_.push_back(move(tc));
    }
}
```

连接池在 Bridge 构造时一次性创建。重连逻辑：

```cpp
void GameBridge::schedule_reconnect(TcpConn* tc) {
    int delay = tc->reconnect_delay_ms;
    tc->reconnect_delay_ms = min(delay * 2, 30000);  // 指数退避

    thread([this, tc, delay]() {
        this_thread::sleep_for(chrono::milliseconds(delay));
        loop_->run_in_loop([this, tc]() {
            if (!tc->connected) tc->client->connect();
        });
    }).detach();
}
```

## Wire 协议

```
Bridge ↔ GameServer (:9003):

[route_id(4) BE][json_len(4) BE][json_bytes(N)]

  route_id : Bridge 分配的 uint32，WS 连接唯一标识
  json_len : 后续 JSON 字节数（0 = 关闭帧，见下文）
  json     : 浏览器 WS 协议的原始 JSON，不加封不解

  关闭帧：json_len = 0 → GameServer 关闭该 route 的 session
```

## 设计要点

1. **无状态**：不存 session_id、room_id、player_name——只有 route_id ↔ WS 的映射
2. **零业务耦合**：Bridge 不知道 JSON 内容，不解析、不翻译、不关心 type 字段
3. **连接池**：M:N 复用，`route_id % pool_size` 保证同一 session 的请求/响应走同一条 TCP
4. **指数退避重连**：TCP 断开后 1s→2s→4s→...→30s 重连
5. **关闭帧通知**：WS 断开时发 `len=0` 帧，GameServer 收到后立即清理 session，避免"刷新后重复玩家"

## 关联文档

- [[JSON协议前端-json_frontend.hpp]] — GameServer 端接收此协议的组件
- [[WebSocket服务端-ws_server.hpp]] — 上游：WS 连接到此桥接
