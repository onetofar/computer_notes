---
title: WebSocket 服务端—ws_server.hpp
category: 网络
tags:
  - 源码分析
  - 网站制作
  - WebSocket
  - RFC 6455
difficulty: 进阶
source: onetofar_web/backend/src/websocket/ws_server.hpp
created: 2026-07-30
modified: 2026-07-30
maturity: 深入
---

> [!info] 一句话
> `WsServer` 是基于 `TcpServer` 的自实现 WebSocket 服务端，处理 HTTP Upgrade 握手（RFC 6455）和后续的帧解析，是 backend 的入口。

## 文件概览

| 项目 | 内容 |
|------|------|
| 路径 | `backend/src/websocket/ws_server.hpp` |
| 核心类 | `WsServer` |
| 行数 | ~153 行 |
| 依赖 | `TcpServer`, `WsConnection`, OpenSSL SHA-1 |
| 回调 | `set_connect_callback`, `set_message_callback`, `set_close_callback` |

## 源码领读

### WsState 与 WsContext

```cpp
enum WsState { kExpectingUpgrade, kEstablished };

// 连接上下文——解决类型转换 UB 的关键设计
struct WsContext {
    WsState state{kExpectingUpgrade};               // 握手前/后
    std::shared_ptr<WsConnection> ws_conn;           // 握手后创建
};
```

> [!warning] 为什么需要 WsContext？（历史 Bug BUG-018）
> 之前代码握手前存 `shared_ptr<WsState>`（枚举），握手后改为 `shared_ptr<WsConnection>`。下次 `on_message` 用 `static_pointer_cast<WsState>()` 读取，实际指向 `WsConnection` 的内存——枚举的首字节恰好是 0 时触发再次握手逻辑并失败。`WsContext` 同时持有 state 和 ws_conn，消除类型转换 UB。

### on_connection — 新连接

```cpp
void on_connection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        auto ctx = std::make_shared<WsContext>();
        conn->set_context(ctx);  // state = kExpectingUpgrade
    }
}
```

### on_message — 按状态分发

```cpp
void on_message(const TcpConnectionPtr& conn, Buffer* buf) {
    auto ctx = static_pointer_cast<WsContext>(conn->context());
    if (!ctx) return;

    if (ctx->state == kExpectingUpgrade) {
        handle_handshake(conn, buf, ctx);     // HTTP Upgrade
    } else if (ctx->ws_conn) {
        ctx->ws_conn->on_data(buf);            // WebSocket 帧
    }
}
```

### handle_handshake — RFC 6455 握手

```cpp
void handle_handshake(conn, buf, ctx) {
    string request(buf->peek(), buf->readable_bytes());

    // 1. 提取 Sec-WebSocket-Key
    auto key_pos = request.find("Sec-WebSocket-Key: ");
    // key_pos += 19 拿到 value
    string ws_key = request.substr(key_pos, key_end - key_pos);

    // 2. 计算 Accept = Base64(SHA1(key + GUID))
    string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";  // RFC 6455 定义
    string accept_key = sha1_base64(ws_key + magic);

    // 3. 返回 101 Switching Protocols
    string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept_key + "\r\n\r\n";
    conn->send(response);
    buf->retrieve_all();

    // 4. 升级为 WebSocket 连接
    ctx->state = kEstablished;
    auto ws_conn = make_shared<WsConnection>(conn);
    ws_conn->set_message_callback([this](auto ws, auto msg, WsOpCode) {
        if (message_cb_) message_cb_(ws, msg);    // → bridge.on_ws_message()
    });
    ws_conn->set_close_callback([this](auto ws) {
        if (close_cb_) close_cb_(ws);             // → bridge.on_ws_close()
    });
    ctx->ws_conn = ws_conn;

    if (connect_cb_) connect_cb_(ws_conn);         // → bridge.on_ws_connect()
}
```

> [!warning] GUID 魔数错误（BUG-017）
> 之前用 `5AB9B969E2C1BA` 而非 `C5AB0DC85B11`。全部 WebSocket 客户端握手失败。RFC 6455 定义的魔数字符串是 `258EAFA5-E914-47DA-95CA-C5AB0DC85B11`。

### SHA-1 + Base64

```cpp
static string sha1_base64(const string& data) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    ::SHA1((const unsigned char*)data.data(), data.size(), hash);
    return base64_encode(hash, SHA_DIGEST_LENGTH);
}
```

### broadcast — 广播给所有 WS 连接

```cpp
void broadcast(const string& data) {
    auto conns = tcp_server_.connections();
    for (auto& [_, conn] : conns) {
        auto ctx = static_pointer_cast<WsContext>(conn->context());
        if (ctx && ctx->ws_conn) {
            ctx->ws_conn->send_text(data);  // 自动添加 WS 帧头
        }
    }
}
```

## 上下游关系

```
TcpServer accept → WsServer::on_connection
    → on_message → handle_handshake → connect_cb_  → [[协议桥接器-ws_game_bridge.hpp]]::on_ws_connect
                                      message_cb_  → [[协议桥接器-ws_game_bridge.hpp]]::on_ws_message
                                      close_cb_    → [[协议桥接器-ws_game_bridge.hpp]]::on_ws_close
```

- [[协议桥接器-ws_game_bridge.hpp]] — WsServer 注册的三个回调的目标
- [[前端WS客户端-ws.js]] — 浏览器端对应实现

## 设计要点

1. **自实现 WebSocket**：不依赖 libwebsocket 等第三方库，教学目的
2. **WsContext 双重用途**：握手前只读 state，握手后同时持有 state 和 ws_conn
3. **回调注册**：WsServer 不耦合具体业务逻辑，通过回调注入——WsGameBridge 注册后接管消息处理
