---
title: 游戏同步—game_sync.hpp
category: 计算机网络
tags:
  - 源码分析
  - 网站制作
  - 帧同步
difficulty: 进阶
source: game-server-engine/src/game_server/game/game_sync.hpp
created: 2026-07-30
modified: 2026-07-30
maturity: 完善
---

> [!info] 一句话
> `GameRoomManager` 是游戏服务器中的"游戏房间管理者"——持有所有 `GameRoom`，每个 GameRoom 包含一个 `ArenaDemo` 实例。负责玩家加入/离开游戏房间和驱动 tick 循环。

## 文件概览

| 项目 | 内容 |
|------|------|
| 路径 | `src/game_server/game/game_sync.hpp` |
| 核心类 | `GameRoomManager` + `GameRoom` + `FrameSync` |
| 行数 | ~800+ 行（含 Vector2, InputCommand, EntityState 等辅助类型） |
| 锁粒度 | `mutex_` 保护 GameRoom 映射 |

## 数据结构

### GameRoom

```cpp
struct GameRoom {
    State state{State::WAITING};              // WAITING / PLAYING / FINISHED
    vector<PlayerId> players_;                 // 玩家列表
    size_t max_players_{2};                    // BUG-028: 原默认 4，改为 2
    unique_ptr<ArenaDemo> arena_demo_;         // 竞技场模拟引擎
    // 帧同步相关
    FrameSync frame_sync_;
    deque<InputCommand> input_queue_;
};
```

### GameRoomManager

```cpp
class GameRoomManager {
    unordered_map<RoomId, GameRoom> rooms_;    // RoomId → GameRoom
    mutex mutex_;
public:
    void create_room_with_id(RoomId room_id, size_t max_players = 2);
    bool join_player(RoomId room_id, PlayerId player_id);
    void leave_player(RoomId room_id, PlayerId player_id);
    void leave_by_player(PlayerId player_id);

    void apply_arena_input(RoomId, PlayerId, float dir_x, float dir_y);
    void tick_all_rooms_game(function<void(RoomId, ArenaSnapshot)> cb);
    // ...
};
```

## 核心方法

### create_room_with_id

```cpp
void create_room_with_id(RoomId room_id, size_t max_players = 2) {
    GameRoom room;
    room.max_players_ = max_players > 0 ? max_players : 2;
    room.state = State::WAITING;
    rooms_[room_id] = move(room);
}
```

### join_player

`GameRoomManager::join_player(room_id, player_id)` 只是查找房间后委托给 `GameRoom::join_player(player_id)`（真实逻辑）：

```cpp
// GameRoom::join_player — 实际逻辑
bool join_player(PlayerId player_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 满员检查只对 WAITING 生效：max_players 是"启动阈值"，
    // 游戏开始（PLAYING）后允许继续加入（中途加入/重连）。
    if (state_ == State::WAITING && players_.size() >= max_players_) return false;
    if (state_ != State::WAITING && state_ != State::PLAYING) return false;

    players_.push_back(player_id);

    // 如果是游戏已开始的重连，需要把玩家加入 ArenaDemo
    if (state_ == State::PLAYING && arena_demo_) arena_demo_->add_player(player_id);

    // 人数到齐 → 开始游戏
    if (state_ == State::WAITING && players_.size() >= max_players_) {
        start_game();
    }
    return true;
}
```

> [!warning] max_players 语义：容量上限 → 启动阈值（2026-07-31 修复）
> 修复前的代码是 `if (players_.size() >= max_players_) return false;`——满员检查对 PLAYING 状态也生效。当 `max_players=1` 时：A 加入 → 满员 → 游戏开始（PLAYING）→ B 加入时被 `1 >= 1` 拒绝 → `add_player(B)` 永不执行 → **第二个玩家没有实体**。
>
> 修复：满员检查限定在 WAITING 状态。`max_players` 现在只决定"何时开始游戏"，游戏一旦开始（PLAYING）允许任意玩家加入（中途加入/重连都走 `add_player`）。
>
> 代价：`max_players` 不再限制房间容量——2 人房开始后第 3 人也能进。对教学 demo（支持中途加入）是可接受的取舍。

> [!warning] 重连时 ArenaDemo 未加入（BUG-026）
> `start_game()` 创建 ArenaDemo 时调用了 `add_player()`。但 PLAYING 状态下的 `join_player()` 只把玩家加入 `players_` 列表，未调用 `add_player()`。重连玩家永远没有实体。

### start_game (GameRoom 内部)

```cpp
void start_game() {
    state = State::PLAYING;
    arena_demo_ = make_unique<ArenaDemo>(ArenaConfig{});
    for (auto pid : players_) {
        arena_demo_->add_player(pid);   // 所有玩家加入竞技场
    }
    frame_sync_.start(players_);          // 启动帧同步
}
```

### tick_all_rooms_game（关键 — 防 BUG-001）

```cpp
void tick_all_rooms_game(function<void(RoomId, ArenaSnapshot)> cb) {
    vector<pair<RoomId, ArenaSnapshot>> snapshots;
    {
        lock_guard<mutex> lock(mutex_);
        for (auto& [rid, room] : rooms_) {
            if (room.state == State::PLAYING && room.arena_demo_) {
                room.arena_demo_->tick();              // 推进一帧
                snapshots.push_back({rid, room.arena_demo_->snapshot()});
            }
        }
    }  // ← mutex_ 在这里释放！
    for (auto& [rid, snap] : snapshots) {
        cb(rid, snap);  // 回调可能加 RoomManager 的锁（kRoomState → broadcast_to_room → room_players）
    }
}
```

> [!warning] 为什么必须先释放锁？（BUG-001）
> 之前代码在锁内调用回调 `cb(rid, snap)` → `broadcast_to_room()` → `rooms_.room_players()` 加 `RoomManager::mutex_`。但 `enqueue_match()` 先加 `RoomManager::mutex_` 再加 `GameRoomManager::mutex_`。两个锁按相反顺序获取 → 死锁。修复：先收集快照再释放锁，最后调用回调。

### apply_arena_input

```cpp
void apply_arena_input(RoomId room_id, PlayerId player_id, float dir_x, float dir_y) {
    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) return;
    if (it->second.arena_demo_) {
        it->second.arena_demo_->apply_input(player_id, dir_x, dir_y);
    }
}
```

由 `GameServer::handle_room_broadcast` 在接收到 `kPlayerMove` 时调用。

## 帧同步 (FrameSync)

```cpp
class FrameSync {
    int tick_rate_{30};                  // 30fps
    vector<PlayerId> players_;
    int expected_players_{0};
    bool running_{false};
    mutex mutex_;

    void start(const vector<PlayerId>& players);
    bool add_input(PlayerId, const InputCommand& cmd);

    // 所有玩家输入到齐后，推进一帧
    bool get_frame_inputs(vector<InputCommand>& out);
};
```

`get_frame_inputs` 检查是否所有玩家都提交了当前帧的输入。如果是，收集后广播。

## 数据流

```
GameServer::game_tick (33ms)
  → GameRoomManager::tick_all_rooms_game(cb)
    → 对每个 PLAYING 的 GameRoom:
        1. arena_demo_->tick()        // 推进模拟
        2. arena_demo_->snapshot()    // 导出快照
    → 锁释放
    → cb(room_id, snapshot)          // 回调 = GameServer 广播

GameServer::handle_room_broadcast (玩家输入)
  → GameRoomManager::apply_arena_input(room_id, pid, dx, dy)
    → arena_demo_->apply_input(pid, dx, dy)  // 设置 target 方向
```

## 上下游关系

| 调用方 | 方法 |
|--------|------|
| [[游戏主循环-game_server.hpp]]::handle_create_room | `create_room_with_id()` + `join_player()` |
| [[游戏主循环-game_server.hpp]]::handle_join_room | `join_player()` |
| [[游戏主循环-game_server.hpp]]::game_tick | `tick_all_rooms_game()` |
| [[游戏主循环-game_server.hpp]]::handle_room_broadcast | `apply_arena_input()` |
| [[游戏主循环-game_server.hpp]]::close_session_* | `leave_by_player()` |
| [[竞技场模拟-arena_demo.hpp]] | tick/apply_input/add_player/remove_player |
| [[房间管理-room.hpp]] | `rooms_` 对应此处的 `game_rooms_` |

## 设计要点

1. **游戏引擎隔离**：GameRoom 封装 ArenaDemo，GameServer 不直接操作竞技场
2. **锁释放策略**：`tick_all_rooms_game` 先收集快照再释放锁再回调——消除锁反转死锁
3. **WAITING/PLAYING 双状态支持重连**：PLAYING 状态的 `join_player` 调用 `add_player()` 将重新加入的玩家添加到 ArenaDemo
4. **默认 2 人**：`max_players` 默认 2，适合本地演示（BUG-028 修复）

## ⚠️ 坑

- **BUG-001**：`tick_all_rooms_game` 锁内调用回调导致与 `enqueue_match` 锁顺序反转死锁
- **BUG-026**：PLAYING 状态下 `join_player` 未调用 `arena_demo_->add_player()`，重连玩家无实体
- **BUG-028**：`create_room_with_id` 默认 max_players=4，2 人永远无法触发 `start_game()`
- **新玩家加入无实体（2026-07-31）**：满员检查 `players_.size() >= max_players_` 对 PLAYING 也生效。`max_players=1` 建房后 B 加入被拒，`add_player(B)` 不执行。修复：满员检查限定 WAITING，`max_players` 语义从"容量上限"改为"启动阈值"。

> [!tip] 三个 bug 的连环关系
> 这个项目里围绕 `max_players` / `join_player` 已经出了三个连环 bug：
> 1. **BUG-028**：默认 max_players=4 → 2 人永不开始游戏 → 前端改默认 1
> 2. **满员检查过严**：max_players=1 → 第二人进不来 → 加人无实体
> 3. **BUG-026**：重连加 PLAYING 状态不 add_player → 重连玩家无实体
>
> 共同教训：**"房间人数" 有多个语义（启动阈值 / 容量上限 / 重连资格），必须分清**。