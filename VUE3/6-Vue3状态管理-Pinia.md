---
title: Vue3 状态管理—Pinia
category: VUE3
tags:
  - VUE3
  - Pinia
  - 状态管理
difficulty: 进阶
created: 2026-07-31
modified: 2026-07-31
maturity: 完善
---

> [!info] 一句话
> Pinia 是 Vue 官方的全局状态管理库。当多个页面要共享同一个数据（登录 token、玩家列表、实体位置），把它放 Pinia store 里，任何组件都能读写、且都响应式。

## 1. 没有 Pinia 时的问题

```javascript
// 场景：多个页面都要用"玩家列表"
// ❌ 用全局变量：
window.players = []
// 问题：
// 1. 改了没有通知——页面不会自动刷新
// 2. 命名冲突、可维护性差
// 3. 谁改的都不知道
```

游戏场景尤其明显：`GameRoom.vue`（3D 场景）、`GameLobby.vue`（大厅）都要读 `entities`、`players`、`connected`——**必须共享**，而且一变全部要更新。

## 2. Pinia store 长什么样

```javascript
// stores/game.js
import { defineStore } from 'pinia'

export const useGameStore = defineStore('game', () => {
    // state：状态（用 ref，跟组件里一样）
    const connected = ref(false)
    const players = ref([])
    const entities = ref(new Map())

    // actions：改状态的方法
    function setPlayers(list) {
        players.value = list
    }

    // 返回：对外暴露的接口
    return { connected, players, entities, setPlayers }
})
```

> [!tip] C++ 类比
> `useGameStore()` ≈ 一个**全局单例对象**：
> ```cpp
> // 全局单例（伪代码）
> class GameStore {
>   std::atomic<bool> connected;
>   std::mutex m; std::vector<Player> players;
>   void setPlayers(...) { lock_guard lk(m); players = ...; }
> };
> GameStore& gameStore = GameStore::instance();   // useGameStore()
> ```
> 状态集中管理 + 方法封装（actions），跟单例模式一模一样。

## 3. 怎么用 store

```javascript
// 组件里：
const gameStore = useGameStore()   // 获取单例

gameStore.players.value          // 读取（注意：store 里的 ref 要 .value）
gameStore.players.push(...)      // 直接改（响应式）
gameStore.setPlayers(list)       // 或调 action
```

> [!warning] 常见坑：store 里的 ref 在组件里也要 .value
> ```javascript
> const gameStore = useGameStore()
> console.log(gameStore.players.value)  // ✅
> console.log(gameStore.players)        // ❌ 这是 ref 对象，不是数组
> ```

## 4. 真实源码对照

[[前端游戏状态-game.js]] 是项目里最标准的 Pinia store：

```javascript
export const useGameStore = defineStore('game', () => {
    const connected = ref(false)
    const players = ref([])
    const entities = ref(new Map())
    const sessionId = ref(0)
    const roomId = ref(0)
    const lastError = ref('')

    function connect(token, targetRoomId, username) { ... }
    function disconnect() { ... }

    return { connected, players, entities, sessionId, roomId, lastError, connect, disconnect }
})
```

组件消费：

```javascript
// GameRoom.vue
const gameStore = useGameStore()
// 读取：游戏场景渲染
for (const [id, ent] of gameStore.entities.value) { ... }
// 读取：显示玩家列表
<p>{{ p.username }}</p>  // 模板中 v-for 遍历 gameStore.players.value
```

> [!note] store 怎么和 WebSocket 联动
> `game.js` 里 `connect()` 会注册 `gameSocket.on('state_sync', ...)` 回调，回调里 `entities.value = 新数据` → **WS 推送数据 → store 更新 → 所有组件响应式刷新**。这是 Pinia + 响应式配合 WS 的典型用法。

## 5. 自测

1. 为什么把 `players` 放 store 而不是每个组件自己 `ref`？
2. store 里 `const count = ref(0)`，组件里读它写 `gameStore.count` 还是 `gameStore.count.value`？

> 答案：1. 多个组件共享，且要响应式同步；2. `gameStore.count.value`。

## 下一篇

页面之间的跳转和权限控制靠路由——**VueRouter 与导航守卫**：[[7-Vue3路由与导航守卫]]
