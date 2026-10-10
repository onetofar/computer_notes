---
title: 业务层—game_service.hpp / .cpp
category: 网络
tags:
  - 源码分析
  - 网站制作
  - 三层分离
  - v3
difficulty: 进阶
source: game-server-engine/src/game_server/service/game_service.hpp + .cpp
created: 2026-07-30
modified: 2026-07-30
maturity: 深入
---

> [!info] 一句话
> `GameService` 是三层分离中的**纯业务逻辑层**——接收纯 struct 参数，返回纯 struct 结果，不知道二进制/JSON/网络连接。

## 三层分离中的位置

```
协议前端层（ProtocolFrontend）
  → 路由层（Dispatcher）
    → 业务层（GameService）← 本文件
```

GameService 的职责边界：

| 知道 | 不知道 |
|------|--------|
| 登录、房间、匹配、竞技场 | MessageType 枚举 |
| Session 状态管理 | TcpConnection、Buffer |
| 断线重连逻辑 | route_id、序列号 |
| ArenaDemo tick | JSON 或二进制编码 |
| RoomManager、GameRoomManager | WebSocket |

## 领域结构体

所有请求和响应都是纯 struct：

```cpp
// 登录
struct LoginRequest  { std::string name; };
struct LoginResult   { SessionId session_id; std::string name; std::string token; };

// 房间创建
struct CreateRoomRequest  { uint32_t max_players{2}; };
struct CreateRoomResult   { RoomId room_id; };

// 加入房间
struct JoinRoomRequest  { RoomId room_id; };
struct JoinRoomResult   {
    RoomId room_id;
    bool is_new_join;
    std::vector<SessionId> players;
    std::vector<std::string> names;
};

// 移动输入
struct MoveInput { float dir_x, dir_y; std::string action{"MOVE"}; };
```

## GameService 接口

```cpp
class GameService {
public:
    // 连接生命周期
    virtual SessionId on_session_open() = 0;
    virtual void on_session_close(SessionId, bool clean) = 0;

    // 业务
    virtual LoginResult handle_login(SessionId, const LoginRequest&) = 0;
    virtual CreateRoomResult handle_create_room(SessionId, const CreateRoomRequest&) = 0;
    virtual JoinRoomResult handle_join_room(SessionId, const JoinRoomRequest&) = 0;
    virtual LeaveRoomResult handle_leave_room(SessionId) = 0;
    virtual void handle_player_move(SessionId, const MoveInput&) = 0;
    virtual ReconnectResult handle_reconnect(SessionId, const ReconnectRequest&) = 0;

    // 游戏循环
    virtual ArenaTickResult on_game_tick() = 0;
    virtual void cleanup_expired_reconnects() = 0;

    // 查询
    virtual ServerStats get_stats() const = 0;
    virtual std::vector<SessionId> room_players(RoomId) const = 0;
};
```

所有方法都是从 `handle_business` 那个 300 行 switch 语句拆分而来的。

## 实现内部（game_service.cpp）

```cpp
class GameServiceImpl : public GameService {
    // 内部 Session（不含网络层字段）
    struct BusinessSession {
        SessionId id;
        std::string player_name, token;
        SessionState state;
        RoomId room_id;
        TimePoint last_heartbeat;
        unique_ptr<SlidingWindow> msg_window;
        mutable mutex mutex;
    };

    GameServiceConfig cfg_;
    TokenGenerator token_gen_;
    unordered_map<SessionId, shared_ptr<BusinessSession>> sessions_;
    atomic<SessionId> next_session_id_{1};
    RoomManager rooms_;                // 与 :9002 路径共享
    GameRoomManager game_rooms_;       // 与 :9002 路径共享
    unordered_map<string, ReconnectEntry> reconnect_buffer_;
};
```

`BusinessSession` 是 Session 的业务子集——去掉了 `conn`、`conn_name`、`peer` 等网络字段。

## 设计要点

1. **零协议依赖**：不知道 `MessageType` 枚举、不知道 key=value 格式。Dispatcher 负责将入站消息转为领域 struct，将响应从领域 struct 转回出站消息。
2. **RoomManager 共享**：`:9002` 和 `:9003` 路径共享同一套 `rooms_` 和 `game_rooms_`。同一个房间的玩家可以来自不同协议前端——一个从 TCP 直连，一个从 Bridge 进来。
3. **内部 Session**：`BusinessSession` 是 GameService 自己管理的轻量级状态，与 `:9002` 路径的 `Session` 不冲突——两套 session map 独立管理，session_id 相同但状态同步。

## 关联文档

- [[路由层-dispatcher.hpp]] — 调用 GameService 的上一层
- [[JSON协议前端-json_frontend.hpp]] — 协议前端 → Dispatcher → GameService
- [[游戏主循环-game_server.hpp]] — 持有 GameService 实例的宿主
