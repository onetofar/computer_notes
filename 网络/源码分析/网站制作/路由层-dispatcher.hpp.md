---
title: 路由层—dispatcher.hpp
category: 网络
tags:
  - 源码分析
  - 网站制作
  - 三层分离
  - v3
difficulty: 进阶
source: game-server-engine/src/game_server/dispatch/dispatcher.hpp
created: 2026-07-30
modified: 2026-07-30
maturity: 完善
---

> [!info] 一句话
> `Dispatcher` 是三层分离中的**路由编排层**——接收协议无关的 `IncomingMessage`，提取 struct 调用 `GameService`，将结果组装为 `OutgoingMessage` 发回。

## 三层分离中的位置

```
协议前端层 → 路由层（Dispatcher）← 本文件
                   ↓
             业务层（GameService）
```

Dispatcher 的职责：

| 做 | 不做 |
|----|------|
| `"login"` → 提取 `LoginRequest` → 调用 `handle_login` | 解码字节流（那是 Frontend 的事） |
| `LoginResult` → 组装 `OutgoingMessage("login_ok", {name, token})` | 编码字节流（那是 Frontend 的事） |
| `ArenaTickResult` → `OutgoingMessage("state_sync", {entities})` | 操作 Session 状态机（那是 Service 的事） |
| `room_players()` → 循环 `frontend->send()` 做广播 | 管理房间（那是 Service + RoomManager 的事） |

## 数据结构

```cpp
// 入站（Frontend → Dispatcher）
struct IncomingMessage {
    SessionId session_id;
    uint32_t seq;
    std::string type;           // "login" / "create_room" / "input" / ...
    nlohmann::json data;        // 结构化数据
};

// 出站（Dispatcher → Frontend）
struct OutgoingMessage {
    SessionId session_id;
    std::string type;           // "login_ok" / "room_state" / "state_sync" / ...
    nlohmann::json data;
};
```

## dispatch 方法

```cpp
void dispatch(ProtocolFrontend* frontend, SessionId sid,
              const IncomingMessage& msg) {
    if (msg.type == "login") {
        LoginRequest req{msg.data.value("name", "")};
        auto result = service_->handle_login(sid, req);
        frontend->send(sid, {
            sid, "login_ok", {
                {"session_id", result.session_id},
                {"name", result.name},
                {"token", result.token}
            }
        });
    }
    else if (msg.type == "create_room") {
        CreateRoomRequest req{msg.data.value("max_players", 2)};
        auto result = service_->handle_create_room(sid, req);
        push_room_event(frontend, sid, RoomEvent{...});
    }
    else if (msg.type == "input") {
        MoveInput input{
            msg.data["dir"].value("x", 0),
            msg.data["dir"].value("y", 0),
            msg.data.value("action", "MOVE")
        };
        service_->handle_player_move(sid, input);
    }
    else if (msg.type == "join_room") { ... }
    else if (msg.type == "leave_room") { ... }
    else if (msg.type == "reconnect") { ... }
    else {
        frontend->send(sid, {sid, "error", {{"message", "unsupported"}}});
    }
}
```

## push 方法（非请求-响应）

对于 `game_tick` 这种服务器主动推送的数据，Dispatcher 提供专门的 `push_*` 方法：

```cpp
void push_game_tick(ProtocolFrontend* frontend) {
    ArenaTickResult result = service_->on_game_tick();
    for (auto& snap : result.room_snapshots) {
        OutgoingMessage msg{0, "state_sync", {
            {"entities", entities_json(snap.entities)},
            {"frame", snap.tick}
        }};
        broadcast_to_room(frontend, snap.room_id, msg);
    }
}
```

目前 `push_game_tick` 通过 `broadcast_to_room` 发送——对房间内所有玩家调用 `frontend->send()`。

## 设计要点

1. **薄路由层**：Dispatcher 不做业务逻辑，只做"拆参数→调方法→组响应"的编排。业务变化时改 Service，协议变化时改 Frontend，Dispatcher 通常不变。
2. **广播辅助**：`push_room_event` 和 `broadcast_to_room` 是 Dispatcher 的便利方法——从 Service 获得结果后，负责决定"这条消息该发给谁"。
3. **没有 switch 之外的调度**：当前用 if-else 链。生产级可用注册表：`handlers_["login"] = &dispatch_login`。

## 关联文档

- [[业务层-game_service.hpp]] — Dispatcher 调用的下一层
- [[JSON协议前端-json_frontend.hpp]] / [[二进制协议前端-binary_frontend.hpp]] — Dispatcher 发送响应的目标
