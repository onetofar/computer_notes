---
title: 全链路总览-索引—v3 无状态桥接层 + 三层分离
category: 计算机网络
tags:
  - 源码分析
  - 网站制作
  - 索引
  - v3
difficulty: 进阶
source: 全链路索引
created: 2026-07-30
modified: 2026-07-30
maturity: 完善
---

> [!info] 本索引的作用
> 将"浏览器按键 → Vue → WebSocket → 桥接 → 游戏服务器 → 竞技场模拟"的完整链路拆为多篇源码分析。**每篇对应一个源文件**。
>
> 当前架构为 **v3**——Bridge 从有状态协议翻译器变为无状态字节代理，GameServer 从单协议单体变为三层分离。

## v3 架构图

```
浏览器 (Vue 3 + Three.js)
├── [[前端WS客户端-ws.js]]          — WebSocket 连接/重连
├── [[前端游戏状态-game.js]]        — Pinia store (entities/players)
└── [[前端3D场景-GameRoom.vue]]     — Three.js 渲染循环
        │ JSON 协议 {type:"login", data:{...}}
        │ ws://localhost:9090/ws/game
        ▼
Gateway 层 — [[协议桥接器-ws_game_bridge.hpp]]
  **无状态字节代理**
  ├── WS 连接 → route_id 分配
  ├── WS 消息 → [route_id(4)][len(4)][json] → TCP 连接池
  ├── TCP 响应 → 提取 route_id → 路由到 WS
  └── TCP 连接池 *M* 条，自动重连 + 指数退避
        │ [route_id(4)][json_len(4)][json_bytes(N)]
        │ TCP :9003
        ▼

GameServer 三层分离

协议前端层（ProtocolFrontend）
├── [[JSON协议前端-json_frontend.hpp]]    — :9003（Bridge 连入）
└── [[二进制协议前端-binary_frontend.hpp]] — :9002（直连客户端）

路由层
└── [[路由层-dispatcher.hpp]]
    type → handler 映射，结果 → OutgoingMessage

业务逻辑层
└── [[业务层-game_service.hpp]]
    GameService 接口（纯 struct，零协议依赖）
    实现：Session/Room/Reconnect/Arena

共享基础设施
├── [[游戏协议-message.hpp]]          — v1/v2 编解码
├── [[会话管理-session.hpp]]          — Session 状态机
├── [[房间管理-room.hpp]]             — RoomManager
├── [[游戏主循环-game_server.hpp]]     — 业务分发 + :9002
├── [[游戏同步-game_sync.hpp]]        — GameRoom + 帧同步
└── [[竞技场模拟-arena_demo.hpp]]      — 权威模拟 tick
```

## 一次按键的完整旅程

```
W 键按下
  → [[前端3D场景-GameRoom.vue]] keydown
  → [[前端游戏状态-game.js]] gameSocket.send('input', {dir:{x:0,y:-1}})
  → [[前端WS客户端-ws.js]] WebSocket.send(JSON)
  → [[WebSocket服务端-ws_server.hpp]] 帧解析 → 回调
  → [[协议桥接器-ws_game_bridge.hpp]] on_ws_message
      → prepend [route_id(4)][len(4)] → TCP 连接池
  → [[JSON协议前端-json_frontend.hpp]] decode
      → route_id=1, type="input", data={dir:{x:0,y:-1}}
  → [[路由层-dispatcher.hpp]] dispatch → handle_player_input
  → [[业务层-game_service.hpp]] handle_player_move
      → game_rooms_.apply_arena_input(room_id, pid, dx, dy)
      → [[竞技场模拟-arena_demo.hpp]] apply_input(pid, dir)
  → 下一 tick (33ms): ArenaDemo::tick()
  → game_tick() → broadcast
      → [[JSON协议前端-json_frontend.hpp]] encode → [rid]{"type":"state_sync",...}
  → [[协议桥接器-ws_game_bridge.hpp]] on_tcp_message
      → strip route_id → route_table_[rid] → ws->send_text(json)
  → [[前端WS客户端-ws.js]] onmessage
  → [[前端游戏状态-game.js]] state_sync → entities = new Map(entities)
  → [[前端3D场景-GameRoom.vue]] animate() → mesh.position.set()
  → 画面更新 ✅
```

## v2 → v3 对比

| 维度 | v2（之前） | v3（现在） |
|------|-----------|-----------|
| **Bridge** | 有状态（session/room）、1:1 TCP、硬编码 type 枚举 | 无状态字节代理、M:N 连接池、零业务逻辑 |
| **Bridge ↔ GS** | `WsGameBridge` + 完整 `GameMessageType` 枚举 | `GameBridge` + `[route_id(4)][len(4)][json]` |
| **GS 协议** | 单端口 `:9002` 二进制 | 双端口 `:9002` 二进制 + `:9003` JSON |
| **业务逻辑** | `handle_business` 300 行 switch | `GameService` 接口 + `Dispatcher` 路由 |
| **数据格式** | key=value 字符串 | JSON 结构化（:9003 路径） |
| **坐标变换** | Bridge 做 2D→3D | GameServer 直接发 3D 坐标 |

## 阅读指南

### 入口流程
| 你想深入 | 入口文档 |
|---------|---------|
| 从登录到进房的完整 HTTP/WS 流程 | [[网站前端入口链路-浏览器登录到进房]] |

### 前端层
| 你想深入 | 入口文档 |
|---------|---------|
| 浏览器怎么建立 WebSocket 连接 | [[前端WS客户端-ws.js]] |
| 前端状态管理（玩家/物体/连接） | [[前端游戏状态-game.js]] |
| Three.js 渲染 + WASD 移动 | [[前端3D场景-GameRoom.vue]] |

> [!tip] 0 前端基础？
> 先补 Vue3 最小概念再读前端文档：[[1-VUE3认知地图-索引]]（8 篇速查，C++ 类比，从登录到进房闭环）

### 桥接层（后端 Gateway）
| 你想深入 | 入口文档 |
|---------|---------|
| WebSocket 握手怎么做的（RFC 6455） | [[WebSocket服务端-ws_server.hpp]] |
| Bridge 为什么改成无状态 | [[协议桥接器-ws_game_bridge.hpp]] |

### 服务端三层分离
| 你想深入 | 入口文档 |
|---------|---------|
| JSON 协议前端怎么工作（:9003） | [[JSON协议前端-json_frontend.hpp]] |
| 二进制协议适配层（:9002） | [[二进制协议前端-binary_frontend.hpp]] |
| Dispatcher 怎么路由 | [[路由层-dispatcher.hpp]] |
| 业务逻辑怎么解耦 | [[业务层-game_service.hpp]] |
| 双端口集成（事件循环中枢） | [[游戏主循环-game_server.hpp]] |

### 服务端基础设施
| 你想深入 | 入口文档 |
|---------|---------|
| 协议编解码（v1/v2 格式） | [[游戏协议-message.hpp]] |
| Session 状态机 | [[会话管理-session.hpp]] |
| 房间匹配/成员管理 | [[房间管理-room.hpp]] |
| GameRoom 帧同步 | [[游戏同步-game_sync.hpp]] |
| 竞技场权威模拟（碰撞/tick） | [[竞技场模拟-arena_demo.hpp]] |
