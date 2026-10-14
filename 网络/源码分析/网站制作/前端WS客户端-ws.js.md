---
title: 前端 WS 客户端—ws.js
category: 计算机网络
tags:
  - 源码分析
  - 网站制作
  - WebSocket
difficulty: 入门
source: onetofar_web/frontend/src/services/ws.js
created: 2026-07-30
modified: 2026-07-30
maturity: 完善
---

> [!info] 一句话
> `GameSocket` 是前端 WebSocket 客户端单例，封装了连接、JSON 协议分发、自动重连和事件监听管理。

## 文件概览

| 项目 | 内容 |
|------|------|
| 路径 | `frontend/src/services/ws.js` |
| 核心导出 | `GameSocket` 类 + `gameSocket` 单例 |
| 行数 | ~90 行 |
| 依赖 | 浏览器原生 `WebSocket` API |
| 协议 | JSON `{"type":"xxx","data":{}}` |

## 源码领读

### 类结构

```javascript
class GameSocket {
    ws = null              // 原生 WebSocket 实例
    connected = false      // 连接状态
    handlers = new Map()   // 事件处理器 {type → handler}
    reconnectAttempts = 0
    maxReconnect = 5
}
```

### connect(token, roomId) — 建立连接

```javascript
connect(token, roomId) {
    if (this.ws) this.disconnect()

    // 直连后端 WS 桥 (:9090)，不走 Vite proxy（有 Node v20.20.2 bug）
    const url = `ws://localhost:9090/ws/game?token=${token}&room_id=${roomId || ''}`
    this.ws = new WebSocket(url)

    this.ws.onopen = () => {
        this.connected = true
        this.reconnectAttempts = 0
        this.emit('_connected')         // → game.js 发 join
    }

    this.ws.onmessage = (event) => {
        const msg = JSON.parse(event.data)  // {"type":"state_sync","data":{...}}
        const handler = this.handlers.get(msg.type)
        if (handler) handler(msg.data)       // 直接分发给具体 handler
        this.emit('_message', msg)
    }

    this.ws.onclose = () => {
        this.connected = false
        this.emit('_disconnected')
        this.autoReconnect(token, roomId)   // 自动重连
    }
}
```

**关键设计**：`onmessage` 根据 `msg.type` 查找对应的 handler。这是发布-订阅模式的最简实现——上游不用 switch-case，只需 `.on('state_sync', handler)`。

### send(type, data) — 发送消息

```javascript
send(type, data = {}) {
    if (this.ws && this.connected) {
        this.ws.send(JSON.stringify({ type, data }))
    }
}
```

序列化为 `{"type":"join","data":{"name":"Alice"}}` 发送。

### on/emit/removeAllListeners — 事件系统

```javascript
on(type, handler) {
    this.handlers.set(type, handler)
    return () => this.handlers.delete(type)  // 返回取消函数
}

removeAllListeners() {
    this.handlers = new Map()   // 全清，防止监听器泄漏
}
```

> [!warning] 监听器泄漏（历史 Bug）
> 之前 `connectToRoom()` 每次调用都 `.on()` 注册新 handler，但从未清理。旧 handler 不断累积，导致 `room_state` 被多个 handler 重复处理。`removeAllListeners()` 在 `disconnect()` 中调用，确保连接生命周期 = 监听器生命周期。

### autoReconnect — 指数退避重连

```javascript
autoReconnect(token, roomId) {
    if (this.reconnectAttempts >= this.maxReconnect) return  // 最多 5 次
    this.reconnectAttempts++
    setTimeout(() => {
        this.connect(token, roomId)
    }, Math.min(1000 * Math.pow(2, this.reconnectAttempts), 10000))  // 2^n 秒，最大 10s
}
```

## 上下游关系

```
上游调用者                    GameSocket                    下游
──────────                  ──────────                   ──────
game.js:connect()    →    connect()        →    WsServer (TCP 连 :9090)
game.js:send()       →    send(type, data) →    WebSocket 帧 → backend
gameStore handlers ←      onmessage        ←    JSON 响应
game.js:disconnect() →    disconnect()     →   close()
```

- [[前端游戏状态-game.js]] — 调用方：`gameSocket.connect/send/on`
- [[前端3D场景-GameRoom.vue]] — 间接调用方：键盘事件 → `gameSocket.send('input', ...)`
- [[WebSocket服务端-ws_server.hpp]] — 服务端对应实现

## 设计要点

1. **单例模式**：`export const gameSocket = new GameSocket()` — 全应用共用一个 WS 连接
2. **JSON 协议**：所有消息 `{type, data}`，type 做路由，data 是具体负载
3. **直连 :9090**：Vite 5.4.21 on Node v20.20.2 的 WS proxy 有 bug，前端绕过
4. **on() 返回值可取消**：`const off = gameSocket.on('xxx', fn); off()` 可单独移除某个监听器

## Vue3 概念补课

> [!tip] 这篇几乎纯 JS，Vue3 含量最低
> `ws.js` 不依赖 Vue——它是独立的事件分发类。读懂它只需要老式 JS 的事件/回调思维。若被 `game.js`/`GameRoom.vue` 里"`.on()` 注册回调 → store 更新 → 页面刷新"的联动卡住，回看 [[3-Vue3响应式系统-ref与computed]]。
