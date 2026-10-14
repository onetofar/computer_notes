---
title: JSON 协议前端—json_frontend.hpp / .cpp
category: 计算机网络
tags:
  - 源码分析
  - 网站制作
  - 三层分离
  - v3
difficulty: 进阶
source: game-server-engine/src/game_server/protocol/frontend/json_frontend.hpp + .cpp
created: 2026-07-30
modified: 2026-07-30
maturity: 完善
---

> [!info] 一句话
> `JsonFrontend` 是 v3 新增的 JSON 协议前端——监听 `:9003` 端口，接收 Bridge 的 `[route_id][len][json]` 帧，解码后交给 Dispatcher。

## 协议格式

```
Bridge → GameServer (:9003):

[route_id(4) BE][json_len(4) BE][json_bytes(N)]

  route_id : Bridge 分配的 uint32，WS 连接唯一标识
  json_len : JSON 字节数（0 = 关闭帧）
  json     : 浏览器 WS 协议的原始 JSON

JSON 格式：
  {"type":"login",       "data":{"name":"Alice"}}
  {"type":"create_room",  "data":{"max_players":2}}
  {"type":"input",       "data":{"dir":{"x":0,"y":-1},"action":"MOVE"}}
  {"type":"join_room",   "data":{"id":"5"}}
```

## 核心接口

```cpp
class JsonFrontend : public ProtocolFrontend {
public:
    // 发送回调：由 GameServer 注入，负责将帧写入对应 TCP 连接
    using SendCallback = function<void(uint32_t route_id,
                                       const char* data, size_t len)>;

    // 解码（3 参数版本，返回 route_id）
    bool decode(Buffer& buf, IncomingMessage& out, uint32_t& route_id);

    // ProtocolFrontend 接口
    bool decode(Buffer& buf, IncomingMessage& out) override;
    void send(SessionId, const OutgoingMessage&) override;
    void broadcast(const OutgoingMessage&) override;

    // route ↔ session 映射
    void map_session(SessionId, uint32_t route_id);
    uint32_t lookup_route(SessionId) const;
    SessionId lookup_session(uint32_t route_id) const;
    void unmap(SessionId);

private:
    mutable mutex map_mutex_;
    unordered_map<uint32_t, SessionId> route_to_session_;
    unordered_map<SessionId, uint32_t> session_to_route_;
    SendCallback send_cb_;  // 由 GameServer 提供
};
```

## 解码路径

```cpp
bool JsonFrontend::decode(buf, msg, route_id) {
    if (buf->readable_bytes() < 8) return false;       // 帧头不够

    route_id = read_u32_be(buf->peek());                // 4 bytes
    json_len = read_u32_be(buf->peek() + 4);            // 4 bytes
    if (buf->readable_bytes() < 8 + json_len) return false;  // 半包

    string json(buf->peek() + 8, json_len);
    buf->retrieve(8 + json_len);

    auto j = json::parse(json_str);
    msg.type = j["type"];
    msg.data = j["data"];
    msg.session_id = lookup_session(route_id);  // 由调用方覆写
    return true;
}
```

> [!tip] 半包处理
> 帧头固定 8 字节，先等满 8 字节，再根据 json_len 等完整帧。粘包情况在 while 循环中逐帧处理——和二进制 `DefaultCodec` 同模式。

## 编码路径

```cpp
string JsonFrontend::encode_frame(msg, route_id) {
    json j{{"type", msg.type}, {"data", msg.data}};
    string json_str = j.dump();

    string frame;
    write_u32_be(frame, route_id);          // 4 bytes
    write_u32_be(frame, json_str.size());    // 4 bytes
    frame.append(json_str);                  // N bytes
    return frame;
}

void JsonFrontend::send(session_id, msg) {
    uint32_t rid = lookup_route(session_id);
    if (rid == 0) return;  // session 未注册

    string frame = encode_frame(msg, rid);
    if (send_cb_) send_cb_(rid, frame.data(), frame.size());
}
```

`send_cb_` 由 GameServer 在创建 JsonFrontend 时注入：

```cpp
auto send_cb = [this](uint32_t route_id, const char* data, size_t len) {
    TcpConnectionPtr conn;
    {
        lock_guard<mutex> lock(bridge_conn_mutex_);
        auto it = route_to_conn_.find(route_id);
        if (it != route_to_conn_.end()) conn = it->second.lock();
    }
    if (conn) conn->send(data, len);
};
```

## 设计要点

1. **纯字节视图**：JsonFrontend 不解耦 JSON 内容——只关心 route_id 和帧边界。`type` 和 `data` 对它是透明字节。
2. **route ↔ session 映射**：Bridge 连接是无状态 TCP 池，多个 session 共享一条 TCP。`route_id` 是区分不同 session 的唯一标识。
3. **关闭帧特殊处理**：`json_len=0` 的帧不是合法 JSON，在 `on_bridge_message` 中提前拦截并触发 session 清理。

## 关联文档

- [[协议桥接器-ws_game_bridge.hpp]] — 发送端的对应组件
- [[路由层-dispatcher.hpp]] — 解码后的消息交给 Dispatcher 路由
- [[游戏主循环-game_server.hpp]] — 持有 JsonFrontend 实例的宿主
