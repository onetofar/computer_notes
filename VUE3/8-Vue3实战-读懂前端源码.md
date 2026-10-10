---
title: Vue3 实战—读懂前端源码
category: VUE3
tags:
  - VUE3
  - 实战
  - 源码分析
difficulty: 进阶
created: 2026-07-31
modified: 2026-07-31
maturity: 深入
---

> [!info] 一句话
> 把前面 7 篇的概念串起来，真实走读一遍项目前端的 4 个核心文件。读完后你能自己读任意 `.vue` 文件。

## 0. 四个文件，一条链路

```
main.js    应用入口（装配）
router     路由（URL → 页面）
user.js    Pinia store（认证状态）
Login.vue  页面（登录表单）
GameLobby.vue / GameRoom.vue  页面（游戏）
```

项目结构（只看游戏相关）：

```
frontend/src/
├── main.js                  ← 入口
├── router/index.js          ← 路由
├── stores/
│   ├── user.js              ← 认证状态
│   └── game.js              ← 游戏状态（WS 联动）
├── services/
│   ├── api.js               ← HTTP 封装
│   └── ws.js                ← WebSocket 封装
└── views/
    ├── Login.vue / Register.vue
    ├── GameLobby.vue        ← 大厅
    └── GameRoom.vue         ← 3D 游戏场景
```

## 1. main.js — 应用装配

```javascript
import { createApp } from 'vue'
import { createPinia } from 'pinia'
import naive from 'naive-ui'
import App from './App.vue'
import router from './router'

const app = createApp(App)
app.use(createPinia())   // 注册 Pinia（enable useGameStore/useUserStore）
app.use(router)          // 注册路由
app.use(naive)           // 注册 Naive UI 组件库
app.mount('#app')        // 挂载到 index.html 的 #app
```

> 看懂的关键：`app.use(X)` 就是"装上 X 插件"。装完 Pinia 才有 `useGameStore`，装完 router 才有 `<router-link>`。

## 2. router — 路由表与守卫

```javascript
const routes = [
  { path: '/game', component: () => import('@/views/GameLobby.vue'),
    meta: { requiresAuth: true } },          // 懒加载 + 要登录
  { path: '/game/room/:id', component: () => import('@/views/GameRoom.vue'),
    meta: { requiresAuth: true } },
]

router.beforeEach((to, from, next) => {
  if (to.meta.requiresAuth) {
    const token = localStorage.getItem('token')
    if (!token) {
      next({ name: 'login', query: { redirect: to.fullPath } })
      return
    }
  }
  next()
})
```

> 结合 [[7-Vue3路由与导航守卫]]：没登录访问 `/game` → 被弹去 `/login?redirect=/game`。**认证状态在 localStorage，刷新不丢。**

## 3. stores/user.js — 认证状态（Pinia）

```javascript
export const useUserStore = defineStore('user', () => {
    // 初始化时从 localStorage 恢复登录态
    const token = ref(localStorage.getItem('token') || '')
    const user = ref(JSON.parse(localStorage.getItem('user') || 'null'))

    const isLoggedIn = computed(() => !!token.value)   // 派生状态

    async function login(credentials) {
        const res = await userApi.login(credentials)   // 调 HTTP
        token.value = res.data.token
        user.value = res.data.user
        localStorage.setItem('token', res.data.token)  // 持久化
        localStorage.setItem('user', JSON.stringify(res.data.user))
        return res.data
    }

    return { token, user, isLoggedIn, login }
})
```

> 结合 [[6-Vue3状态管理-Pinia]]：单例 + 双份存储（Pinia 内存 / localStorage 磁盘）。

## 4. Login.vue — 登录页（组件 + 模板 + 生命周期）

```vue
<script setup>
const userStore = useUserStore()
const form = reactive({ username: '', password: '' })
const loading = ref(false)
const error = ref('')

async function handleLogin() {
  loading.value = true
  try {
    await userStore.login(form)          // → store → HTTP → 后端
    const redirect = route.query.redirect || '/'   // 守卫记录的地址
    router.push(redirect)                // 跳回去
  } catch (e) {
    error.value = e.response?.data?.error || '登录失败'
  } finally {
    loading.value = false
  }
}
</script>

<template>
  <n-input v-model:value="form.username" />      <!-- 双向绑定 -->
  <n-input v-model:value="form.password" type="password" />
  <n-alert v-if="error" :title="error" />        <!-- 条件渲染错误 -->
  <n-button :loading="loading" @click="handleLogin">登录</n-button>
</template>
```

> 一页看全所有概念：`ref`/`reactive` 状态、`v-model` 双向绑定、`@click` 事件、`v-if` 条件、`:loading` 动态属性、async/await 异步。

## 5. GameLobby.vue — 大厅（模板循环）

```vue
<script setup>
onMounted(async () => {
  try {
    await gameStore.fetchRooms()      // 生命周期中拉数据
    rooms.value = gameStore.rooms
  } catch (e) {
    error.value = '加载房间列表失败'
  }
})
function handleCreateRoom() { router.push('/game/room/new') }
function joinRoom(id) { router.push('/game/room/' + id) }
</script>

<template>
  <n-empty v-if="rooms.length === 0" description="暂无房间，创建一个吧！" />
  <n-list v-else>
    <n-list-item v-for="room in rooms" :key="room.id">
      {{ room.players || 0 }}/{{ room.max_players || 4 }} 人
      <n-tag :type="room.state === 'PLAYING' ? 'warning' : 'success'">
        {{ room.state === 'PLAYING' ? '游戏中' : '等待中' }}
      </n-tag>
      <n-button @click="joinRoom(room.id)">加入</n-button>
    </n-list-item>
  </n-list>
</template>
```

> 结合 [[4-Vue3组件与模板语法]]：`v-if`/`v-else` 双态、`v-for` 循环 + `:key`、三元表达式动态属性。

## 6. GameRoom.vue — 3D 场景（生命周期 + 响应式 + 渲染循环）

```vue
<script setup>
const gameStore = useGameStore()
const entityMeshes = new Map()

onMounted(async () => {
  initScene()                                  // Three.js 初始化
  gameStore.connect(token, roomIdParam, username)  // 连 WS
})

onUnmounted(() => {
  cancelAnimationFrame(animationId)            // 停渲染循环
  renderer.dispose()                           // 释放 WebGL
  gameStore.disconnect()                       // 断 WS
})

function animate() {
  animationId = requestAnimationFrame(animate)   // 每帧调用
  for (const [id, ent] of gameStore.entities.value) {   // 读响应式数据
    // 更新 Three.js mesh 位置 → 画面变化
  }
  renderer.render(scene, camera)
}
</script>
```

> 结合 [[5-Vue3生命周期与组合式API]] 和 [[3-Vue3响应式系统-ref与computed]]：**关键循环**——WS 推数据 → store 更新 `entities` → animate() 下一帧读到新数据 → 画面更新。前端逻辑走通。

## 7. 你现在能读懂的完整链路

```
浏览器打开 /game
  → 路由守卫检查 token（localStorage）
  → 无 token → 弹去 /login?redirect=/game
  → Login.vue 提交表单 → user.js login → api.js HTTP → 后端 :8080 返回 JWT
  → token 存 localStorage + Pinia → router.push('/game')
  → GameLobby.vue onMounted → fetchRooms → 显示房间列表
  → 点创建 → push /game/room/new → GameRoom.vue
  → onMounted → initScene(Three.js) + connect(WS)
  → 键盘输入 → WS → 后端 → state_sync → store 更新 → animate() 画面更新
```

## 8. 自测

1. 为什么 `GameRoom.vue` 的 `initScene` 要放 `onMounted`？
2. `gameStore.entities.value` 和 `gameStore.entities` 有什么区别？
3. 用户点了"创建房间"，URL 是怎么从 `/game/room/new` 变成 `/game/room/5` 的？

> 答案：
> 1. 组件挂载后 canvas 才存在。
> 2. 前者是实际 Map（响应式数据），后者是 ref 包装对象。
> 3. 服务器返回 room_id → store 更新 roomId → GameRoom.vue 的 watch 检测到 → `router.replace('/game/room/5')`（见 [[5-Vue3生命周期与组合式API]]）。

## 关联源码分析文档

- [[前端游戏状态-game.js]] — 这篇讲的 Pinia store 的完整版
- [[前端3D场景-GameRoom.vue]] — 生命周期 + 渲染循环的完整版
- [[前端WS客户端-ws.js]] — WS 封装的完整版
- [[网站前端入口链路-浏览器登录到进房]] — 完整 HTTP/WS 双通道链路
- [[1-MOC 全链路总览-索引]] — 全链路索引
