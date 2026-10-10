---
title: Vue3 组件与模板语法
category: VUE3
tags:
  - VUE3
  - 组件
  - 模板
  - 单文件组件
difficulty: 入门
created: 2026-07-31
modified: 2026-07-31
maturity: 完善
---

> [!info] 一句话
> `.vue` 文件把"一个页面/部件"装在一起：`<template>`（长什么样）+ `<script setup>`（逻辑）+ `<style>`（样式）。模板里的 `{{ }}`、`v-if`、`v-for`、`@click` 是 Vue 给的"魔法 HTML 属性"。

## 1. .vue 文件的三段式

```vue
<template>
  <!-- 长什么样：HTML + Vue 指令 -->
  <p v-if="isLogin">欢迎，{{ username }}</p>
  <button @click="logout">退出</button>
</template>

<script setup>
// 逻辑：就是 JS，顶层变量自动暴露给模板
import { ref } from 'vue'
const isLogin = ref(true)
const username = ref('Alice')
function logout() { isLogin.value = false }
</script>

<style scoped>
/* 样式：只作用于本组件 */
button { color: red; }
</style>
```

> [!tip] C++ 类比
> `.vue` ≈ 一个"类"：`<template>` 是它的声明/接口视图，`<script setup>` 是它的实现/成员变量，`<style scoped>` 是它的私有静态成员。`import { ref }` 像 `#include`。

**`<script setup>` 的特殊之处**：你在里面声明的**所有顶层变量和函数，模板里可以直接用名字访问**——不用 `this.`，不用 export。这是 Vue3 的组合式 API 写法（项目里的 `.vue` 全是这种写法）。

## 2. 模板语法速查（必须会的 5 个）

### ① `{{ }}` 插值 — 显示数据

```vue
<p>当前分数：{{ score }}</p>       <!-- 读取 ref 会自动解包，不用 .value -->
```

> [!tip] 为什么模板里不用写 `.value`
> 因为模板编译器帮你解包了：`{{ count }}` 等价于 `{{ count.value }}`。**在 `<script>` 里必须写 `.value`，在模板里不用**——这是新手最容易懵的点。

### ② `v-if` / `v-else` — 条件渲染

```vue
<n-tag :type="gameStore.connected ? 'success' : 'error'">
  {{ gameStore.connected ? '已连接' : '未连接' }}
</n-tag>
```

`v-if` 是"有就渲染，没有就删掉"。三元表达式 `a ? b : c` 在模板里非常常见。

### ③ `v-for` — 循环渲染

```vue
<n-list-item v-for="p in gameStore.players" :key="p.id">
  {{ p.username || p.id }}
</n-list-item>
```

`v-for="p in 数组"` 遍历数组；`:key` 告诉 Vue 每个元素的唯一标识（用于 diff 优化）。**`v-for` 一定要配 `:key`**。

### ④ `@click` — 事件绑定

```vue
<button @click="handleCreateRoom">创建房间</button>
<button @click="gameStore.disconnect()">离开</button>
```

`@click="函数名"` 等价于 `onclick`，但传的是函数引用，可传参。

### ⑤ `:prop` / `v-model` — 传值 & 双向绑定

```vue
<!-- :type 是动态属性，值来自 JS 表达式 -->
<n-tag :type="room.state === 'PLAYING' ? 'warning' : 'success'">

<!-- v-model 是双向绑定：输入框的值 ↔ 变量同步 -->
<n-input v-model:value="form.username" />
```

- `:xxx="表达式"`（`v-bind` 缩写）：把 JS 值动态传给子组件的 prop
- `v-model`：省掉"用户输入 → 更新变量"的手动代码（表单最常见）

## 3. 模板里能写什么 JS

模板不是完整 JS，只能写**表达式**（有返回值的一句话），不能写语句：

```vue
<!-- ✅ 可以 -->
<p>{{ a + b }}</p>
<p>{{ list.length > 0 ? '有' : '无' }}</p>

<!-- ❌ 不行 -->
<p>{{ const x = 1 }}</p>
```

复杂逻辑放 `<script setup>` 里的函数，模板只调用。

## 4. 真实源码对照

[[前端3D场景-GameRoom.vue]] 的模板部分：

```vue
<n-list-item v-for="p in gameStore.players">      <!-- 循环玩家列表 -->
  {{ p.username || p.id }}                         <!-- 显示名字，没有就显示 id -->
</n-list-item>

<n-button @click="handleLeave">离开房间</n-button> <!-- 点击事件 -->

<n-tag>{{ entityCount }}</n-tag>                   <!-- 插值显示物体数量 -->
<n-tag :type="gameStore.connected ? 'success' : 'error'">  <!-- 动态属性 -->
  {{ gameStore.connected ? '已连接' : '未连接' }}
</n-tag>
```

[[网站前端入口链路-浏览器登录到进房#6. GameLobby.vue — 游戏大厅|GameLobby.vue]] 里的条件渲染 + 循环：

```vue
<n-empty v-if="rooms.length === 0" description="暂无房间，创建一个吧！" />
<n-list v-else>
  <n-list-item v-for="room in rooms" :key="room.id">
    {{ room.players || 0 }}/{{ room.max_players || 4 }} 人
    <n-button @click="joinRoom(room.id)">加入</n-button>
  </n-list-item>
</n-list>
```

> [!note] `n-` 前缀是什么
> 项目用了 **Naive UI** 组件库，`<n-button>`、`<n-list>`、`<n-tag>`、`<n-input>` 都是它的组件。用法和普通标签一样，只是功能更丰富（自带样式/状态）。

## 5. 自测

1. `<script setup>` 里的 `const count = ref(0)`，在模板里写 `{{ count }}` 还是 `{{ count.value }}`？
2. `v-for="p in players"` 里的 `p` 是什么？循环多少次？
3. `@click="handleLeave"` 和 `@click="handleLeave()"` 有区别吗？

> 答案：1. `{{ count }}`（模板自动解包）；2. p 是数组元素，循环 players.length 次；3. 有——前者传函数引用（点击时执行），后者立即执行一次。**项目里用前者**。

## 下一篇

模板和数据都会读了，还差"代码什么时候执行"——**生命周期与组合式 API**：[[5-Vue3生命周期与组合式API]]
