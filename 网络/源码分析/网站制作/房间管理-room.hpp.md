---
title: 房间管理—room.hpp
category: 网络
tags:
  - 源码分析
  - 网站制作
  - 房间匹配
difficulty: 入门
source: game-server-engine/src/game_server/game/room.hpp
created: 2026-07-30
modified: 2026-07-30
maturity: 完善
---

> [!info] 一句话
> `RoomManager` 是线程安全的房间与匹配管理器——创建/加入/离开房间、排队匹配、成员迁移。持有最轻量的房间数据（只存成员关系）。

## 文件概览

| 项目 | 内容 |
|------|------|
| 路径 | `src/game_server/game/room.hpp` |
| 核心类 | `RoomManager` + `Room` |
| 行数 | ~203 行 |
| 锁粒度 | `mutex_` 保护所有容器 |

## Room 结构

```cpp
struct Room {
    RoomId id{0};
    unordered_set<SessionId> players;   // 房间成员
};
```

只存成员 ID 集合，**不存网络连接**、**不存缓冲**。真正的网络状态由 `Session` 管理。

## RoomManager 核心方法

### create_room — 创建房间

```cpp
RoomId create_room(SessionId owner) {
    lock_guard<mutex> lock(mutex_);
    remove_from_queue_locked(owner);        // 从匹配队列移除
    leave_locked(owner);                     // 从旧房间移除

    Room room;
    room.id = next_room_id_++;               // 自增 ID
    room.players.insert(owner);
    session_room_[owner] = room.id;
    rooms_[room.id] = move(room);
    return room.id;                           // 返回新房间号
}
```

### join_room — 加入房间

```cpp
bool join_room(SessionId session_id, RoomId room_id) {
    lock_guard<mutex> lock(mutex_);
    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) return false;     // 房间不存在

    remove_from_queue_locked(session_id);     // 出匹配队列
    leave_locked(session_id);                  // 离旧房间
    it->second.players.insert(session_id);     // 加入
    session_room_[session_id] = room_id;
    return true;
}
```

> [!warning] 房间不存在（BUG-024）
> 所有玩家断线后 `leave_locked` 发现房间空 → `rooms_.erase(it)` → 房间销毁。下一个人 join 时 `it == rooms_.end()`。GameServer::handle_join_room 对此做了修复：自动用原 ID 重建房间。

### create_room_with_id — 指定 ID 重建

```cpp
void create_room_with_id(SessionId owner, RoomId room_id) {
    // 与 create_room 相同，但使用指定的 room_id 而非自增
    Room room;
    room.id = room_id;
    room.players.insert(owner);
    session_room_[owner] = room_id;
    rooms_[room_id] = move(room);
}
```

### leave — 离开房间

```cpp
vector<SessionId> leave(SessionId session_id) {
    lock_guard<mutex> lock(mutex_);
    remove_from_queue_locked(session_id);
    return leave_locked(session_id);
}

// leave_locked 内部：
// 1. 从 session_room_ 擦除
// 2. 从 rooms_[room_id].players 擦除
// 3. 如果房间空了 → rooms_.erase(it) 销毁
// 4. 返回剩余玩家列表
```

### enqueue_match — 匹配

```cpp
MatchResult enqueue_match(SessionId session_id) {
    // 加入匹配队列
    match_queue_.push_back(session_id);
    in_queue_.insert(session_id);
    if (match_queue_.size() < players_per_room_) {
        return {false, 0, {}};   // 人不够，继续等
    }

    // 凑够人数 → 创建房间
    Room room;
    room.id = next_room_id_++;
    for (size_t i = 0; i < players_per_room_; ++i) {
        SessionId id = match_queue_.front();
        match_queue_.pop_front();
        in_queue_.erase(id);
        room.players.insert(id);
        session_room_[id] = room.id;
        result.players.push_back(id);
    }
    rooms_[room.id] = move(room);
    return {true, room.id, result.players};
}
```

### migrate_member — 断线重连迁移

```cpp
void migrate_member(SessionId old_id, SessionId new_id, RoomId room_id) {
    // 断线重连时 session_id 可能变化，把房间成员身份从旧的迁移到新的
    room->players.erase(old_id);
    room->players.insert(new_id);
    session_room_.erase(old_id);
    session_room_[new_id] = room_id;
}
```

## 数据流示意

```
RoonManager 内部容器关系：

session_room_                rooms_
┌──────────┐               ┌──────────┐
│session 1 → room 5       │room 5 → {session 1, session 2}
│session 2 → room 5       │room 7 → {session 3}
│session 3 → room 7       └──────────┘
└──────────┘

match_queue_              in_queue_
┌──────────┐              ┌──────────┐
│ (空)     │              │ (空)     │
└──────────┘              └──────────┘
```

## 上下游关系

| 调用方 | 方法 | 说明 |
|--------|------|------|
| `GameServer::handle_create_room` | `create_room()` | 创建并加入 |
| `GameServer::handle_join_room` | `join_room()` | 加入指定房间 |
| `GameServer::handle_leave_room` | `leave()` | 离开 |
| `GameServer::handle_match` | `enqueue_match()` | 匹配 |
| `GameServer::close_session_by_*` | `leave()` | 断线时移出 |
| `GameServer::handle_reconnect` | `migrate_member()` | 重连迁移 |
| `GameServer::broadcast_to_room` | `room_players()` | 获取房间成员列表 |
| `GameServer::handle_room_broadcast` | `room_of()` | 查玩家所在房间 |

## 关联文档

- [[会话管理-session.hpp]] — 房间成员用 `SessionId` 标识
- [[游戏主循环-game_server.hpp]] — 业务分发中调用 RoomManager

## 设计要点

1. **线程安全**：全部加 `mutex_`——`create_room`、`leave`、`enqueue_match` 可能被多个 worker 线程并发调用
2. **自动销毁空房间**：`leave_locked` 中 `if (it->second.players.empty()) rooms_.erase(it);`——不需要 GC
3. **create_room_with_id**：应对"所有玩家断线→房间销毁→重连时房间号还在"的场景
4. **匹配队列**：先到先匹配，凑够人数一次性出队