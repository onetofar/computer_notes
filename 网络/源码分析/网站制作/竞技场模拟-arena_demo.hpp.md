---
title: 竞技场模拟—arena_demo.hpp
category: 计算机网络
tags:
  - 源码分析
  - 网站制作
  - 游戏逻辑
difficulty: 进阶
source: game-server-engine/src/game_server/game/arena_demo.hpp
created: 2026-07-30
modified: 2026-07-30
maturity: 深入
---

> [!info] 一句话
> `ArenaDemo` 是**服务器权威的游戏逻辑引擎**——在服务端计算玩家移动、碰撞、伤害和重生，客户端只负责渲染。这是"相信服务器，而不是客户端"的核心体现。

## 文件概览

| 项目 | 内容 |
|------|------|
| 路径 | `src/game_server/game/arena_demo.hpp` |
| 核心类 | `ArenaDemo` + `ArenaPlayer` + `ArenaConfig` |
| 行数 | ~310 行 |
| 数据结构 | `vector<ArenaPlayer>` |
| 线程安全 | **不自加锁**——依赖调用方（GameRoom）在外部串行化 |

## 关键数据结构

### ArenaPlayer

```cpp
struct ArenaPlayer {
    uint64_t id{0};
    float x{0}, y{0};                  // 当前位置（2D 竞技场坐标）
    float target_x{0}, target_y{0};    // 目标方向（-1~1，由 apply_input 设置）
    float speed{3.0f};                 // 每 tick 移动像素
    int hp{3};                         // 生命值
    int score{0};                      // 分数（BUG-013：未实现加分）
    bool alive{true};
    int respawn_timer{0};              // 重生倒计时（tick 数）
};
```

### ArenaConfig

```cpp
struct ArenaConfig {
    float width{800.0f};               // 竞技场宽度
    float height{600.0f};              // 竞技场高度
    int max_score{5};                  // 先得 N 分获胜（未实现）
    int respawn_ticks{60};             // 重生等待 ≈ 2 秒（60 ticks × 33ms）
    float player_radius{15.0f};        // 玩家碰撞半径
    float touch_damage_distance{30.0f}; // 碰触判伤距离
};
```

### ArenaSnapshot

```cpp
struct ArenaSnapshot {
    uint64_t tick{0};                  // 当前帧号
    vector<ArenaPlayer> players;       // 所有玩家状态
    string winner;                     // 胜利者（空=无人胜出）
};
```

## 核心流程

### add_player — 加入竞技场

```cpp
void add_player(uint64_t player_id) {
    // 检查是否已在
    for (auto& p : players_) if (p.id == player_id) return;

    ArenaPlayer p;
    p.id = player_id;
    spawn_at_random(p);    // 随机出生点
    players_.push_back(p);
}
```

### apply_input — 接收玩家输入

```cpp
void apply_input(uint64_t player_id, float dir_x, float dir_y) {
    for (auto& p : players_) {
        if (p.id == player_id && p.alive) {
            p.target_x = dir_x;   // 设置移动方向
            p.target_y = dir_y;
            return;
        }
    }
}
```

### tick() — 核心模拟循环

每帧按顺序执行三个步骤（`arena_demo.hpp:130-241`）：

```
tick()
  │
  ├─ 步骤 1: 玩家移动
  │    for each alive player:
  │      len = sqrt(target_x² + target_y²)
  │      if len > 0.001:          ← 有方向输入时才移动
  │        dx = target_x / len     ← 归一化
  │        dy = target_y / len
  │      new_x = x + dx * speed
  │      new_y = y + dy * speed
  │      clamp_to_bounds()        ← 夹紧在边界内
  │
  ├─ 步骤 2: 碰撞检测 + 伤害
  │    for each pair (i, j):
  │      dist = distance(p[i], p[j])
  │
  │      if dist < player_radius * 2:    ← 碰撞重叠
  │        overlap = min_dist - dist
  │        push_x = (dx / dist) * overlap * 0.5   ← 各推一半
  │        push_y = (dy / dist) * overlap * 0.5
  │        p[i] -= push; p[j] += push
  │        clamp_to_bounds(p[i])           ← 推后夹紧边界
  │        clamp_to_bounds(p[j])
  │
  │      if dist < touch_damage_distance:  ← 碰触扣血
  │        p[i].hp -= 1                     ← 双方各扣 1 HP
  │        p[j].hp -= 1
  │        p[i] -= dx * 0.5; p[j] += dx * 0.5  ← 弹开避免连续伤害
  │        clamp_to_bounds(...)
  │
  └─ 步骤 3: 阵亡/重生
       for each player:
         if not alive:
           respawn_timer > 0 → timer--
           timer == 0 → hp=3, alive=true, spawn_at_random()
         if hp <= 0:
           alive = false
           respawn_timer = 60
```

### snapshot() — 导出快照

```cpp
ArenaSnapshot snapshot() const {
    ArenaSnapshot snap;
    snap.tick = tick_;
    snap.players = players_;
    for (auto& p : players_) {
        if (p.score >= config_.max_score)
            snap.winner = "Player " + to_string(p.id);
    }
    return snap;
}
```

序列化为 `"game_t=42|p=1,x=400,y=300,hp=3,alive=1|..."` 用于广播。

## 完整的生命周期

```
玩家 A 加入 room 5
  → game_rooms_.join_player(5, 1)
    → room.players_ = [1]
    → size(1) < max_players(2) → 等待

玩家 B 加入 room 5
  → rooms_.join_player(5, 2)
    → room.players_ = [1, 2]
    → size(2) >= max_players(2) → start_game()

start_game():
  → arena_demo_ = new ArenaDemo({800, 600})
  → arena_demo_.add_player(1)   → 随机出生 (421, 153)
  → arena_demo_.add_player(2)   → 随机出生 (237, 489)

game_tick() (每 33ms):
  → arena_demo_.tick()
    → 移动 → 碰撞 → 伤害 → 重生
  → snapshot() → broadcast → bridge → 前端渲染

玩家 B 按 W 键:
  → apply_input(2, 0, -1) → p[2].target_y = -1
  → next tick: p[2].y -= 3.0
  → next broadcast: p[2] 新位置
```

## 上下游关系

| 调用方 | 方法 |
|--------|------|
| [[游戏同步-game_sync.hpp]]::join_player | `add_player()` |
| [[游戏同步-game_sync.hpp]]::leave_player | `remove_player()` |
| [[游戏同步-game_sync.hpp]]::apply_arena_input | `apply_input()` |
| [[游戏同步-game_sync.hpp]]::tick_all_rooms_game | `tick()` + `snapshot()` |
| [[游戏主循环-game_server.hpp]]::game_tick | 触发 tick 循环 |
| [[协议桥接器-ws_game_bridge.hpp]] | 解析 `game_t=` payload 为 JSON |

## 设计要点

1. **服务器权威**：所有物理计算在服务端，客户端只渲染。防止玩家作弊修改客户端代码
2. **2D 简化**：ArenaDemo 使用 2D 坐标，Bridge 负责映射到 Three.js 3D 场景
3. **无自锁**：ArenaDemo 不自加锁——调用方 `GameRoomManager` 的锁已经保护了串行访问
4. **推离 + 夹紧**：碰撞推离后立即 `clamp_to_bounds`，防止玩家被推到竞技场外（BUG-014）
5. **重生机制**：60 ticks ≈ 2 秒，给战斗节奏喘息的窗口

## ⚠️ 坑

- **BUG-008 连续伤害**：碰撞后未推离足够远，下一帧同对玩家再次判伤。修复：推离距离翻倍
- **BUG-013 分数未实现**：碰触伤害双方扣血但不区分伤害来源，无法给攻击者加分
- **BUG-014 出界**：推离后未夹紧边界，玩家被推到竞技场外。修复：每步夹紧
- **无运动插值**：客户端直接跳到新位置，没有线性插值，高速移动时看起来一跳一跳的