---
title: 二进制协议前端—binary_frontend.hpp / .cpp
category: 网络
tags:
  - 源码分析
  - 网站制作
  - 三层分离
  - v3
difficulty: 进阶
source: game-server-engine/src/game_server/protocol/frontend/binary_frontend.hpp + .cpp
created: 2026-07-30
modified: 2026-07-30
maturity: 完善
---

> [!info] 一句话
> `BinaryFrontend` 是 v3 的**二进制协议适配层**——在现有 `DefaultCodec`（v1/v2）上包装 `ProtocolFrontend` 接口，让现有 `:9002` 路径也能享受三层分离的架构好处。

## 角色定位

BinaryFrontend 是**适配器**——把旧的 `DefaultCodec` 接口适配到新的 `ProtocolFrontend` 接口。**不改 `:9002` 路径的任何代码**。

```
旧的调用链：
  on_message → codec_.decode → Message → handle_business → ...

新的调用链：
  on_message → BinaryFrontend::decode → IncomingMessage → Dispatcher
```

当前 `:9002` 路径仍然直接用旧的 `dispatch_message` → `handle_business` 链，没有走 BinaryFrontend。BinaryFrontend 是为未来将 `:9002` 也接入 Dispatcher 准备的。

## 核心接口

```cpp
class BinaryFrontend : public ProtocolFrontend {
public:
    using OutputFunc = function<void(const string& conn_key,
                                      const char* data, size_t len)>;

    bool decode(Buffer& buf, IncomingMessage& out) override;
    void send(SessionId, const OutgoingMessage&) override;
    void broadcast(const OutgoingMessage&) override;

    // session ↔ conn_name 映射
    void register_session(SessionId, const string& conn_key) override;
    void unregister_session(SessionId) override;
    SessionId lookup_by_conn(const string& conn_name) const;
};
```

## 解码：二进制 → IncomingMessage

```cpp
bool BinaryFrontend::decode(buf, out) {
    Message raw;
    CodecStatus status = codec_.decode(buf, raw);  // 复用现有 codec
    if (status != kOk) return false;

    out.seq = raw.seq;
    out.type = message_type_to_string(raw.type);  // 枚举 → "login"
    out.data = convert_payload(raw);               // key=value → json

    return true;
}
```

`convert_payload` 按消息类型将 key=value 字符串转为 JSON：

```cpp
switch (raw.type) {
    case kLogin:     // payload = "Alice"
        out.data["name"] = raw.payload;
        break;
    case kPlayerMove:  // payload = "dir=0,-1;action=MOVE"
        // → {dir:{x:0,y:-1}, action:"MOVE"}
        break;
    default:  // 通用 key=value → json
        out.data = parse_key_value(raw.payload);
        break;
}
```

## 编码：OutgoingMessage → 二进制

```cpp
string BinaryFrontend::encode_outgoing(msg) {
    MessageType type = string_to_message_type(msg.type);

    string payload;
    if (type == kLoginOk) {
        // {"session_id":1,"name":"Alice"} → "session=1;name=Alice"
        payload = "session=" + to_string(msg.data["session_id"]) +
                  ";name=" + msg.data["name"].get<string>();
    }
    else if (type == kRoomState) {
        // 按 action 构造 key=value
        payload = json_to_key_value(msg.data);
    }
    else {
        payload = json_to_key_value(msg.data);
    }

    return codec_.encode(type, 0, payload);  // 复用现有 codec
}
```

## 消息类型映射表

| MessageType 枚举 | 字符串名（Incoming/Outgoing） |
|-----------------|------------------------------|
| `kLogin` | `"login"` |
| `kLoginOk` | `"login_ok"` |
| `kCreateRoom` | `"create_room"` |
| `kJoinRoom` | `"join_room"` |
| `kPlayerMove` | `"player_move"` |
| `kRoomState` | `"room_state"` |
| `kError` | `"error"` |

这个映射表是二进制前端和 JSON 前端共享的"规范类型名"——不管底层用哪种线缆格式，`Dispatcher` 看到的都是 `"login"` / `"login_ok"` 等字符串。

## 设计要点

1. **适配器模式**：不修改 DefaultCodec，不修改现有 `:9002` 路径。BinaryFrontend 是新代码，在旧 codec 上包装了一层新接口。
2. **类型名统一**：`MessageType` 枚举 ↔ `"login"` 字符串的双向映射，让二进制和 JSON 前端在 Dispatcher 层面用同一套命名。
3. **key=value 互转**：BinaryFrontend 知道二进制协议的 payload 格式（key=value、game_t= 等），JSON Frontend 不知道这些——这是二进制协议前端存在的意义。

## 关联文档

- [[JSON协议前端-json_frontend.hpp]] — 对等的 JSON 协议前端
- [[路由层-dispatcher.hpp]] — 目标：BinaryFrontend → Dispatcher → GameService
- [[游戏协议-message.hpp]] — 底层 `DefaultCodec` 定义
