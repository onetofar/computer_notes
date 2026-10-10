---
title: 前端游戏状态—game.js
category: 网络
tags:
  - 源码分析
  - 网站制作
  - Pinia
difficulty: 入门
source: onetofar_web/frontend/src/stores/game.js
created: 2026-07-30
modified: 2026-07-30
maturity: 完善
---

> [!info] 一句话
> `useGameStore` 是 Pinia store，管理连接生命周期和游戏状态（玩家列表、实体位置），是前端的数据中枢。

## 文件概览

| 项目 | 内容 |
|------|------|
| 路径 | `frontend/src/stores/game.js` |
| 核心导出 | `useGameStore` (Pinia defineStore) |
| 行数 | ~96 行 |
| 依赖 | `gameSocket` (ws.js) |
| 响应式状态 | `connected`, `players`, `entities`, `sessionId`, `roomId`, `lastError` |

## 源码领读

### 状态定义

```javascript
const connected = ref(false)        // WS 是否连接
const players = ref([])             // [{id, username}, ...]
const entities = ref(new Map())     // Map<entityId, {id, type, pos, hp, alive, color}>
const sessionId = ref(0)            // 我的 session ID
const roomId = ref(0)              // 当前房间号
const lastError = ref('')          // 最后一条错误消息
```

`entities` 用 `Map` 而非对象/数组——O(1) 查找，且 Three.js 渲染循环需要按 id 快速查询。

### connect(token, targetRoomId, username) — 连接主流程

```javascript
function connect(token, targetRoomId, username) {
    disconnect()  // 先清干净——防止监听器泄漏

    gameSocket.connect(token, targetRoomId)

    gameSocket.on('_connected', () => {
        connected.value = true
        gameSocket.send('join', { name: username })  // 立即登录
    })

    gameSocket.on('login_ok', (data) => {
        sessionId.value = data.session_id
        if (targetRoomId && targetRoomId !== 'new') {
            gameSocket.send('join_room', { id: String(roomId.value || targetRoomId) })
        } else {
            gameSocket.send('create_room', { max_players: 2 })
        }
    })
    // ... 更多 on 注册
}
```

**关键流程**：`_connected` → `join` → `login_ok` → `create_room` 或 `join_room`。

### room_state 处理器

```javascript
gameSocket.on('room_state', (data) => {
    if (data.action === 'created') {
        roomId.value = data.room_id
        players.value = [{ id: sessionId.value, username }]  // 自己是第一个
    }
    if (data.action === 'player_joined') {
        if (data.player_ids) {
            // 完整列表同步——替换而非增量
            const list = []
            for (let i = 0; i < data.player_ids.length; i++) {
                list.push({
                    id: data.player_ids[i],
                    username: data.player_names[i] || ('player_' + data.player_ids[i])
                })
            }
            players.value = list
        } else {
            // 增量添加（兼容旧协议）
            const pid = data.player
            if (!players.value.find(p => p.id === pid)) {
                players.value.push({ id: pid, username: data.name || 'player_' + pid })
            }
        }
    }
    if (data.action === 'leave') {
        const leaverId = parseInt(data.status)
        if (!isNaN(leaverId)) players.value = players.value.filter(p => p.id !== leaverId)
    }
})
```

> [!tip] 完整列表 vs 增量添加
> 新加入者需要知道房间里**已有谁**，增量添加做不到。`player_ids + player_names` 是 BUG-029 修复后增加的字段。

### state_sync 处理器

```javascript
gameSocket.on('state_sync', (data) => {
    const map = new Map()
    for (const e of (data.entities || [])) {
        if (!e.destroyed) map.set(e.id, e)  // 跳过错毁实体
    }
    entities.value = map  // 整体替换，不用更新单个实体
})
```

每次 `state_sync` 创建一个**全新的 Map** 替换旧的。这是最简单的做法——Vue 的 reactive 能检测到 `Map` 引用变化触发重渲染。

### disconnect() — 清理

```javascript
function disconnect() {
    gameSocket.disconnect()
    gameSocket.removeAllListeners()   // 清空所有 on 注册
    connected.value = false
    sessionId.value = 0
    roomId.value = 0
    players.value = []
    entities.value = new Map()
    lastError.value = ''
}
```

**一次性重置所有状态**，避免下次 connect 时遗留旧数据。

### fetchRooms() — 大厅房间列表（2026-07-31 新增）

```javascript
// 加载房间列表（HTTP 通道，供大厅页展示）
async function fetchRooms() {
  const res = await gameApi.listRooms()        // GET /api/game/rooms
  rooms.value = res.data.rooms || []
  return rooms.value
}
```

> [!warning] 踩坑：GameLobby 调用了不存在的 fetchRooms
> `GameLobby.vue` 的 `onMounted` 调 `gameStore.fetchRooms()`，但 store 重构时删掉了这个方法 → 打开 `/game` 大厅页报 `fetchRooms is not a function`。
>
> 修复：补上 `fetchRooms` 和 `rooms` 状态。后端 `/api/game/rooms` 目前是占位实现（返回空数组），大厅显示"暂无房间"。

## 上下游关系

```
用户操作                  gameStore                 gameSocket
─────────                ─────────                ──────────
GameRoom.vue             connect()         →      ws.js connect + send
                         disconnect()      →      ws.js disconnect
gameSocket._connected     ← handler ←      ←      ws.js emit
gameSocket.login_ok       ← handler
gameSocket.room_state     ← handler
gameSocket.state_sync     ← handler

Vue 组件
├── GameRoom.vue       读取 entities, players, connected
├── GameLobby.vue       读取 roomId, connected
└── (其他)              读取 lastError
```

## 上下游文档

- [[前端WS客户端-ws.js]] — `gameSocket` 是此 store 的通信基础
- [[前端3D场景-GameRoom.vue]] — 消费者：读取 `entities` 渲染 3D、读取 `players` 显示列表

## 设计要点

1. **连接即生命周期**：`connect()` 内注册所有 handler，`disconnect()` 清除全部——handler 不会泄漏
2. **Map 整体替换**：`entities` 每次 `state_sync` 全新 Map，而非原地修改——简化响应式追踪
3. **完整列表替代增量**：`player_ids` + `player_names` 彻底解决玩家列表不一致的问题

## Vue3 概念补课

> [!tip] 这是 Pinia store 的教科书示例
> - 状态全用 `ref` 声明、整体替换触发响应式 → [[3-Vue3响应式系统-ref与computed]]
> - store 结构与单例类比 → [[6-Vue3状态管理-Pinia]]
> - 消费端怎么读 `.value` → [[8-Vue3实战-读懂前端源码]]
