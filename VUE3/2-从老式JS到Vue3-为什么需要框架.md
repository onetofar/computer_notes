---
title: 从老式 JS 到 Vue3—为什么需要框架
category: VUE3
tags:
  - VUE3
  - 前端
  - 声明式编程
difficulty: 入门
created: 2026-07-31
modified: 2026-07-31
maturity: 完善
---

> [!info] 一句话
> Vue3 解决的是老式 JS 最痛苦的问题：**"数据变了，页面要跟着变"需要你手动做，代码越多越容易漏**。Vue 让你只声明"页面长什么样"，剩下的自动变。

> [!tip] 前置
> 本篇代码用到 `getElementById`、`addEventListener` 等老式 DOM 函数，若已遗忘先回看 [[0-Vue3前置认知基础-老式JS与DOM]]。

## 1. 你认识的"老式 JS"长这样

你写 H5 时，页面和数据的同步是**命令式**的——每一步都手动操作：

```javascript
// 老式 JS：数据变了，手动更新 DOM
let count = 0
document.getElementById('btn').addEventListener('click', () => {
  count++                              // 1. 改数据
  document.getElementById('count').textContent = count  // 2. 手动改页面
})

// 如果有 10 个地方显示 count，就要写 10 次更新
// 如果漏写一处 → 页面和内存数据不一致 → bug
```

**痛点**：`数据` 和 `页面` 是两套东西，你负责让它们保持一致。项目一大，到处是 `textContent = ...`、`innerHTML = ...`，改一个变量要记得改所有显示它的地方。

## 2. Vue3 的做法：声明式

```vue
<script setup>
const count = ref(0)     // 1. 声明数据（ref = 响应式包装）
function add() { count.value++ }   // 2. 只管改数据
</script>

<template>
  <button @click="add">加一</button>
  <p>当前值：{{ count }}</p>   <!-- 3. 声明"这里显示 count" -->
</template>
```

- 你在 `<template>` 里**声明**"这里显示 count"
- 之后 **count 一变，这行字自动更新**——你不需要写任何 DOM 操作代码

> [!tip] C++ 类比
> 老式 JS = 你在 main() 里手动维护一个 `player.position` 和 `SDL_RenderCopy`，每次改坐标都要记得重绘。
> Vue = 你声明"绘制时用 `player.position`"，框架自动监听它变化然后重绘。
>
> 本质上 Vue 帮你实现了一个 **"观察者模式"**：页面注册了对数据的观察，数据一变自动通知。

## 3. 为什么会有"框架"这个层

| 维度 | 老式 JS | Vue3 |
|------|---------|------|
| 数据→页面 | 手动 `textContent=...` | 自动（声明式绑定 `{{ }}`） |
| 页面结构 | 手拼 HTML 字符串 | 单文件组件 `.vue`（HTML 模板） |
| 状态管理 | 全局变量，乱 | Pinia store（有组织） |
| 模块化 | `<script>` 全局污染 | `import/export` 干净隔离 |
| 更新性能 | 全量重绘 | 精确到单个元素（虚拟 DOM diff） |

> [!warning] 别误会：Vue 不是"运行时魔法"
> Vue 是 **编译器 + 运行时**：编译阶段把模板解析成 JS 代码，运行时用依赖追踪知道"谁依赖了谁"，变化时只更新受影响的部分。原理上和你写的观察者模式代码一样，只是它帮你写好了、还优化了。

## 4. 真实源码对照

在我们项目里，最典型的例子是 [[前端3D场景-GameRoom.vue]] 的渲染循环：

```javascript
// GameRoom.vue —— 关键认知：数据在 store 里，页面只是"声明我要显示它"
for (const [id, ent] of gameStore.entities) {   // 读 store 里的数据
    mesh.position.set(ent.pos.x, ent.pos.y, ent.pos.z)  // 更新 Three.js mesh
}
```

这里 `gameStore.entities` 是 Vue 响应式数据（Pinia store），`GameRoom.vue` 通过 `watch`/渲染循环响应它的变化。**数据在 store 一改，页面/场景自动跟着变**——正是本篇文章说的"声明式"。

## 5. 自测

1. 老式 JS 里"数据变了页面没变"的根本原因是什么？
2. Vue 的 `{{ count }}` 和 `document.getElementById(...).textContent = count` 的区别是什么？

> 答案：1. 手动同步容易漏写；2. 前者声明式自动同步，后者命令式手动同步。

## 下一篇

知道了"为什么需要框架"，下一步学它的灵魂——**响应式系统**：[[3-Vue3响应式系统-ref与computed]]
