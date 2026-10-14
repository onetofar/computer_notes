---
title: 游戏主循环—game_server.hpp
category: 计算机网络
tags:
  - 源码分析
  - 网站制作
  - 游戏服务器
  - epoll
  - v3
difficulty: 深入
source: game-server-engine/src/game_server/game/game_server.hpp
created: 2026-07-30
modified: 2026-07-30
maturity: 深入
---

> [!info] 一句话
> `GameServer` 是事件循环中枢——既保留了 v2 完整的 `:9002` 二进制路径，又在 `:9003` 上集成了 v3 三层分离组件（JsonFrontend + Dispatcher）。

## 重大变化（v3）

| 新增 | 描述 |
|------|------|
| `--bridge-port 9003` | 启动参数，开启 JSON 协议前端端口 |
| `bridge_server_` | 独立的 TcpServer 监听 `:9003` |
| `json_frontend_` | JSON 协议前端（解码/编码） |
| `game_service_` | GameService 实例（业务逻辑） |
| `dispatcher_` | Dispatcher 路由层 |
| `route_to_conn_` | route_id → TcpConnection 映射 |
| `queue_message` JSON 分支 | 根据 `session->transport` 选择 JSON 或二进制编码 |

## v3 架构总览

```
EventLoop (main thread)
  ├── server_ (:9002) → 二进制路径（v2 完整保留）
  ├── bridge_server_ (:9003) → JSON 路径（v3 新增）
  │     └── on_bridge_message(conn, buf)
  │           ├── json_frontend_->decode(*buf, msg, route_id)
  │           ├── 查找/创建 session（route_id ↔ session_id）
  │           ├── convert_incoming_to_message(msg)  →  Message
  │           └── dispatch_message(session_id, msg) → 复用 v2 业务
  ├── heartbeat_wheel_ → 心跳超时清理
  └── TimerQueue → game_tick（33ms）

ThreadPool (N workers)
  └── handle_business → 对 :9002 和 :9003 路径同样有效
```

## :9003 桥接消息处理

```
Bridge TCP 消息到达
  → on_bridge_message(conn, buf)
    while 循环拆帧:
      1. 检查关闭帧（json_len=0）
         → close_session_by_id() + unmap route
      2. json_frontend_->decode(buf, msg, route_id)
         → 提取 route_id, type, data
      3. 记录 route_to_conn_[route_id] = conn（弱引用）
      4. 查找 session：
         - 已存在 → json_frontend_->lookup_session(route_id)
         - 不存在 → 创建新 Session（transport=kJson）
      5. msg.session_id = session_id
      6. convert_incoming_to_message(msg) → Message
      7. dispatch_message(session_id, msg)
```

## queue_message JSON 分支

`queue_message` 是二进制和 JSON 路径的出口汇合点。根据 session 的 transport 自动选择编码：

```cpp
void queue_message(session_id, type, seq, payload) {
    if (session->transport == kJson && json_frontend_) {
        // JSON 路径
        auto data = convert_payload_to_json(type, payload);
        string type_str = (game_t= 时) ? "state_sync" : lower(type_name);
        OutgoingMessage msg{session_id, type_str, data};
        json_frontend_->send(session_id, msg);
    } else {
        // 二进制路径（v2 完整保留）
        session->send(codec_.encode(type, seq, payload));
    }
}
```

`convert_payload_to_json` 将 key=value 格式转为 JSON：
- `"session=1;name=Alice;token=xxx"` → `{session_id:1, name:"Alice", token:"xxx"}`
- `"game_t=42|p=1,x=400,..."` → `{entities:[{id:1, pos:{x:20,z:15}, ...}]}`
- `"room=5;created=1"` → `{room_id:5, action:"created"}`

## 数据流对比

### :9002 二进制路径（不变）

```
TCP → on_message → codec_.decode → Message{enum, string}
  → dispatch_message → ThreadPool → handle_business
  → queue_message → codec_.encode → session->send(binary)
```

### :9003 JSON 路径（新增）

```
TCP → on_bridge_message → JsonFrontend::decode
  → IncomingMessage{type, data}
  → convert_incoming_to_message → Message{enum, string}
  → dispatch_message → ThreadPool → handle_business
  → queue_message → convert_payload_to_json
  → OutgoingMessage{type, data} → JsonFrontend::send
```

## 玩家移动链路（WASD）

```
浏览器 W 键 → GameRoom.vue keydown
  → gameSocket.send('input', {dir:{x:0,y:-1}, action:'MOVE'})
  → Bridge → JsonFrontend → IncomingMessage{type:"input", dir:{x:0,y:-1}}
  → convert_incoming_to_message → payload = "dir=0,-1;action=MOVE"
  → handle_room_broadcast → parse_move_direction → dir_x=0, dir_y=-1
  → game_rooms_.apply_arena_input(room_id, pid, 0, -1)
  → ArenaDemo::apply_input → 下一 tick 移动
```

> [!warning] parse_move_direction 格式不匹配（2026-07-31 修复）
> `parse_move_direction` 原本只认 `"dx=x;dy=y"` 格式，但前端发的 payload 是 `"dir=0,-1;action=MOVE"`（`dir` 字段名 + 逗号分隔）。**字段名和分隔符都不同** → 方向解析恒为 0 → 玩家永远不动。
>
> 这个 bug 从 v2 时期的 Bridge 就存在（旧 Bridge 一直发 `"dir=dx,dy"` 格式），直到加了前端 WASD 控制才暴露——因为之前根本没有客户端会发移动消息。
>
> 修复：`parse_move_direction` 同时支持两种格式：
> - `"dx=1.0;dy=-1.0"`（二进制客户端）
> - `"dir=0,-1;action=MOVE"`（JSON 前端）
>
> 📌 教训：**协议 payload 的格式约定（字段名、分隔符）是隐形契约**，一端改了另一端不报错只是不工作。需要跨层测试覆盖。

## 设计要点

1. **零回归保证**：`:9002` 路径一字不改。所有 v3 组件是新增增量，不与原有路径交互。
2. **JSON → Message 适配**：`convert_incoming_to_message` 将 JSON 消息转为现有的 `Message{enum, string}` 格式，100% 重用已有 `handle_xxx` 逻辑。
3. **queue_message 双重编码**：JSON 分支将响应从 key=value 转回 JSON——相当于把 v2 Bridge 的 `handle_game_to_ws` 移到了 GameServer 内部。
4. **关闭帧检测**：`json_len=0` 特殊帧触发 session 清理，解决 "刷新后重复玩家" 的竞态问题。

## 关联文档

- [[JSON协议前端-json_frontend.hpp]] — :9003 端口的解码/编码组件
- [[路由层-dispatcher.hpp]] — 消息 → handler 的路由
- [[业务层-game_service.hpp]] — 纯业务逻辑接口
