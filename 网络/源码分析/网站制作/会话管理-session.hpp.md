---
title: 会话管理—session.hpp
category: 计算机网络
tags:
  - 源码分析
  - 网站制作
  - Session
difficulty: 入门
source: game-server-engine/src/game_server/game/session.hpp
created: 2026-07-30
modified: 2026-07-30
maturity: 完善
---

> [!info] 一句话
> `Session` 是 game_server 中最核心的数据结构——代表一个玩家连接的完整运行时信息（身份、状态、网络连接）。

## 文件概览

| 项目 | 内容 |
|------|------|
| 路径 | `src/game_server/game/session.hpp` |
| 核心类 | `Session` + `SessionState` 枚举 |
| 行数 | ~111 行 |
| 别名 | `SessionId = uint64_t`, `RoomId = uint64_t` |

## SessionState 状态机

```cpp
enum class SessionState {
    kConnected,    // TCP 刚连上，尚未登录
    kLoggedIn,     // 已登录，在 lobby
    kMatching,     // 正在匹配
    kInRoom,       // 在房间中（包含游戏中）
    kClosing,      // 正在关闭（不再接收新消息）
};
```

转换关系：

```
TCP accept → kConnected
  → handle_login → kLoggedIn
    → enqueue_match → kMatching
    → create_room / join_room → kInRoom
    → leave_room → kLoggedIn
  断线/超时 → kClosing → 销毁
```

## Session 结构

```cpp
struct Session {
    SessionId id{0};                                     // 唯一标识（自增）
    string conn_name;                                     // TcpConnection 名称（用于反向查找）
    weak_ptr<TcpConnection> conn;                          // 弱引用——防止循环引用
    string peer;                                          // "ip:port"

    mutable mutex mutex_;                                  // 以下字段由业务线程修改
    string player_name;
    string token;                                          // 重连凭证
    bool is_reconnected{false};
    SessionState state{kConnected};
    RoomId room_id{0};
    TimePoint last_heartbeat;                              // 心跳时间
    TimePoint last_active;                                 // 最后活跃时间
    unique_ptr<SlidingWindow> msg_window;                  // 单会话 QPS 限流
};
```

### 关键方法

```cpp
// 判断是否已登录（可用于路由权限检查）
bool logged_in() const {
    return state == kLoggedIn || state == kMatching || state == kInRoom;
}

// 线程安全发送
void send(const string& data) const {
    shared_ptr<TcpConnection> ptr;
    {
        lock_guard<mutex> lock(mutex_);
        ptr = conn.lock();           // weak_ptr → shared_ptr
    }
    if (ptr) ptr->send(data);
}
```

> [!tip] 为什么 conn 用 weak_ptr？
> `Session` 和 `TcpConnection` 互相引用。用 `shared_ptr` 会导致循环引用无法释放。Session 持有 `weak_ptr`，使用时 `lock()` 提升为 shared_ptr，如果连接已关闭则返回 nullptr。

```cpp
// 提取 IP 部分（用于连接限流统计）
string peer_ip() const {
    auto pos = peer.rfind(':');
    return pos == string::npos ? peer : peer.substr(0, pos);
}
```

## 上下游关系

| 谁操作 Session | 做什么 |
|---------------|--------|
| `GameServer::on_connection()` | 创建 Session，分配 session_id |
| `GameServer::handle_login()` | 设置 name/token/state |
| `GameServer::handle_business()` | 业务处理中读取 state |
| `GameServer::queue_message()` | 通过 `session->send()` 写回 |
| `GameServer::close_session_by_id/conn()` | 清理并进入重连缓冲 |
| `GameServer::heartbeat_wheel_` | 超时后调用 `close_session_by_id()` |

## 上下游文档

- [[游戏主循环-game_server.hpp]] — Session 的创建者和管理者
- [[房间管理-room.hpp]] — 房间里的成员用 SessionId 表示

## 设计要点

1. **weak_ptr 防循环引用**：TcpConnection ↔ Session 双向引用，必须有一方 weak
2. **mutex 保护业务字段**：EventLoop 线程和 ThreadPool 线程都可能读写 Session
3. **token 断线重连**：登录时生成 token，断线后新连接可用 token 恢复 Session
4. **SlidingWindow 限流**：每个 Session 独立 QPS 限制，防止恶意刷包