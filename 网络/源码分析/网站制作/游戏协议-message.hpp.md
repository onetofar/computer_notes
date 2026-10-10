---
title: 游戏协议—message.hpp
category: 网络
tags:
  - 源码分析
  - 网站制作
  - 协议
difficulty: 进阶
source: game-server-engine/src/game_server/protocol/message.hpp
created: 2026-07-30
modified: 2026-07-30
maturity: 完善
---

> [!info] 一句话
> 定义 game_server 的应用层协议格式——固定头 + 变长 payload，解决 TCP 粘包/半包问题。Bridge 和 game_server 之间通信的基石。

## 文件概览

| 项目 | 内容 |
|------|------|
| 路径 | `src/game_server/protocol/message.hpp` |
| 行数 | ~220 行 |
| 核心 | `encode_message()` + `try_decode_message()` |
| 说明 | v1 协议 12 字节头；v2 协议同时在 `protocol/protocol.hpp` 中实现 |

## 协议格式

### v1（12 bytes header）

```
 0      1      2      3      4-7        8-11       12+
┌──────┐──────┬──────┬──────┬──────────┬──────────┬──────────┐
│magic  │ ver  │ type │ seq  │ plen     │ payload  │
│0xCAFE │  1   │ 见下  │BE u32│ BE u32   │  N bytes │
└──────┴──────┴──────┴──────┴──────────┴──────────┴──────────┘
```

### v2（18 bytes header — 在 protocol.hpp 中定义）

```
 0      1      2      3      4      5      6-9      10-13     14-17     18+
┌──────┐──────┬──────┬──────┬──────┬──────┬──────────┬──────────┬──────────┬──────────┐
│magic  │ ver  │ flags│ type │rsrvd │ seq  │ plen     │ checksum │ payload  │
│0xCAFE │  2   │  0   │ 见下  │  0   │BE u32│ BE u32   │ CRC32/0  │ N bytes  │
└──────┴──────┴──────┴──────┴──────┴──────┴──────────┴──────────┴──────────┴──────────┘
```

v2 比 v1 多 6 字节：flags(1) + reserved(1) + checksum(4)。通过 `version` 字节自动区分，无需配置。

### 消息类型

```cpp
enum class MessageType : uint8_t {
    kPing = 1,       kPong = 2,
    kLogin = 3,      kLoginOk = 4,
    kMatch = 5,      kMatchOk = 6,
    kCreateRoom = 7, kJoinRoom = 8,
    kLeaveRoom = 9,  kPlayerMove = 10,
    kChat = 11,      kRoomState = 12,
    kServerStats = 13,
    kReconnect = 14, kReconnectOk = 15,
    kError = 255,
};
```

## 核心函数

### encode_message — 编码

```cpp
inline string encode_message(MessageType type, uint32_t seq, const string& payload) {
    string out;
    out.reserve(kHeaderSize + payload.size());
    append_u16(out, kMagic);                    // 0xCAFE
    out.push_back((char)kProtocolVersion);      // version = 1
    out.push_back((char)type);                  // type
    append_u32(out, seq);                        // seq
    append_u32(out, (uint32_t)payload.size());   // payload_len
    out.append(payload);                          // payload
    return out;
}
```

> [!tip] 注意这是 v1 编码
> v1 的 `encode_message` 是简化版。实际 game_server 使用 `DefaultCodec`（`protocol/protocol.hpp`），支持 v1/v2 双版本编码，出站默认 v2。

### try_decode_message (Buffer版本) — 解码

```cpp
inline DecodeStatus try_decode_message(Buffer& buffer, Message& out) {
    if (buffer.readable_bytes() < kHeaderSize) return kNeedMore;

    const char* data = buffer.peek();
    uint16_t magic = read_u16(data);
    uint8_t version = data[2];
    auto type = (MessageType)data[3];
    uint32_t seq = read_u32(data + 4);
    uint32_t payload_len = read_u32(data + 8);

    if (magic != kMagic || version != kProtocolVersion || payload_len > kMaxPayloadSize)
        return kBadPacket;

    size_t total = kHeaderSize + payload_len;
    if (buffer.readable_bytes() < total) return kNeedMore;

    out.type = type;
    out.seq = seq;
    out.payload.assign(data + kHeaderSize, payload_len);
    buffer.retrieve(total);
    return kOk;
}
```

解码返回三种状态：

| 状态 | 含义 | 处理 |
|------|------|------|
| `kOk` | 解码成功，业务处理 | `handle_business()` |
| `kNeedMore` | 数据不足（半包），等更多数据 | `break` 退出 while 循环 |
| `kBadPacket` | 协议错误，关闭连接 | `conn->force_close()` |

### 辅助函数

```cpp
inline void append_u16(string& out, uint16_t value) { /* htons + append */ }
inline uint16_t read_u16(const char* data) { /* memcpy + ntohs */ }
// ... 同理 append_u32 / read_u32
```

所有网络字节序转换通过 `htons`/`htonl`/`ntohs`/`ntohl` 完成。

## payload 格式（key=value 约定）

payload 是纯文本，采用 `key=value;key=value` 格式（类似 HTTP query string）：

| type | payload 示例 | 说明 |
|------|-------------|------|
| `kLogin` | `Alice` | 纯字符串表示玩家名 |
| `kLoginOk` | `session=1;name=Alice;token=abc123` | 多字段 |
| `kRoomState` | `room=5;join=2;names=Alice,Bob;players=1,2` | 房间操作 |
| `kRoomState` | `game_t=42\|p=1,x=400,y=300,hp=3,alive=1` | 竞技场状态 |
| `kPlayerMove` | `dir=0,-1;action=MOVE` | 方向+动作 |
| `kError` | `please LOGIN first` | 错误描述 |

> [!tip] 为什么不用 Protobuf/FlatBuffers？
> 教学项目，key=value 格式可以直接用 `nc` 或 Python socket 测试，无需额外工具。

## 上下游关系

- [[协议桥接器-ws_game_bridge.hpp]] — 使用本协议的 v1/v2 编解码
- [[游戏主循环-game_server.hpp]] — 使用 `DefaultCodec` 解码收到的消息
- [[竞技场模拟-arena_demo.hpp]] — `kRoomState` payload 包含 `game_t=` 格式

## 设计要点

1. **长度前缀**：先读 12/18 字节头拿到 payload_len，再读 payload——天然解决 TCP 粘包
2. **Magic 校验**：`0xCAFE` 做第一道过滤，非本协议的数据包快速识别为 `kBadPacket`
3. **v1/v2 渐进升级**：version 字节放在 data[2]，读头之前就能区分版本。v2 的 checksum=0 表示"跳过校验"