---
title: Vue3 生命周期与组合式 API
category: VUE3
tags:
  - VUE3
  - 生命周期
  - 组合式API
difficulty: 入门
created: 2026-07-31
modified: 2026-07-31
maturity: 完善
---

> [!info] 一句话
> 组件的一生有几个时刻：创建 → 挂载到页面 → 更新 → 卸载。Vue 提供钩子让你在这些时刻插入代码（`onMounted`/`onUnmounted`）。这跟 C++ 的构造/析构函数一个思路。

## 1. 组件的一生

```
组件被创建（setup 执行）
   ↓
onMounted：插入页面（DOM 就绪）
   ↓
（数据变化时自动更新，onUpdated 可监听）
   ↓
onUnmounted：从页面移除（组件销毁前）
```

> [!tip] C++ 类比
>
> | Vue | C++ |
> |-----|-----|
> | `setup`（script setup 代码体） | 构造函数体 |
> | `onMounted` | 构造完成、可以开始用资源时 |
> | `onUnmounted` | 析构函数（释放资源） |
>
> 我们项目里 `GameRoom.vue` 的 `onMounted` 就是"初始化场景"，`onUnmounted` 就是"释放 WebGL + 断 WS"——**完全对应 C++ 的 RAII 思想**。

## 2. 为什么需要生命周期钩子

浏览器里有个经典问题：**操作 DOM 必须在元素挂载之后**，否则找不到节点。

```vue
<script setup>
import { onMounted } from 'vue'
import * as THREE from 'three'

// ❌ 这里直接访问 canvas 会失败：组件还没挂载，canvas 还不存在
// const renderer = new THREE.WebGLRenderer({ canvas: document.querySelector('canvas') })

// ✅ 挂载后再初始化
onMounted(() => {
  initScene()   // 此时 canvas 已在页面里，可以安全访问
})
</script>
```

## 3. 组合式 API：代码按"功能"组织

`<script setup>` 叫组合式 API，核心思想：**按功能把代码聚在一起，而不是按选项分**。

```vue
<script setup>
// 老写法（选项式 API，Vue2）：
//   data() / methods: / mounted:  —— 同一功能的代码被拆散到三个区块
//
// 新写法（组合式 API，Vue3）：所有相关代码放一起
const score = ref(0)
function addScore() { score.value++ }
onMounted(() => { /* 初始化 */ })
</script>
```

这个区别**不需要深究**——只要知道：**项目里所有 `.vue` 都是组合式 API（`<script setup>`）**，顶层变量自动暴露给模板，函数直接调用，按功能自然分组。

## 4. 真实源码对照

[[前端3D场景-GameRoom.vue]] 的生命周期用得非常标准：

```javascript
onMounted(async () => {
    const token = userStore.token
    if (!token) { router.push('/login'); return }   // 未登录拦截
    await nextTick()
    initScene()                                          // Three.js 场景初始化
    gameStore.connect(token, roomIdParam, userStore.username)  // 连 WebSocket
})

onUnmounted(() => {
    if (animationId) cancelAnimationFrame(animationId)   // 停渲染循环
    if (renderer) renderer.dispose()                     // 释放 WebGL 资源
    gameStore.disconnect()                               // 断开 WebSocket
})
```

> [!tip] 成对出现：onMounted / onUnmounted
> 这个模式贯穿整个前端：
> - `onMounted`：**申请**资源（建场景、连 WS）
> - `onUnmounted`：**释放**资源（停循环、dispose、断开）
>
> 这就是前端版的 RAII——你写 C++ 时会做的 `new`/`delete` 配对，前端用生命周期钩子做。

## 5. watch：监听变化

`watch` 监听某个响应式值，变了就执行回调：

```javascript
// GameRoom.vue：房间号变了就跳转 URL
watch(() => gameStore.roomId, (newVal) => {
    if (newVal && newVal > 0 && roomIdParam === 'new') {
        router.replace(`/game/room/${newVal}`)  // /game/room/new → /game/room/5
    }
})
```

> [!tip] C++ 类比
> `watch(数据, 回调)` ≈ 一个订阅：数据变化 → 调用回调。和信号槽/观察者一样。

## 6. 自测

1. 为什么 Three.js 的 `initScene()` 要放在 `onMounted` 里而不是 `<script setup>` 顶层？
2. `onMounted`/`onUnmounted` 对应 C++ 的什么？

> 答案：1. 组件挂载前 canvas 不存在，初始化会失败；2. 构造后初始化 / 析构释放资源。

## 下一篇

多个页面要共享同一个状态（比如 token、玩家列表）——**Pinia 状态管理**：[[6-Vue3状态管理-Pinia]]
