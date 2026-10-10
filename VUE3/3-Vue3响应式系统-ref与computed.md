---
title: Vue3 响应式系统—ref 与 computed
category: VUE3
tags:
  - VUE3
  - 响应式
  - 观察者模式
difficulty: 进阶
created: 2026-07-31
modified: 2026-07-31
maturity: 完善
---

> [!info] 一句话
> `ref` 把一个普通变量变成"响应式"——谁在读取它，谁就自动成为它的观察者；它一变，所有观察者自动更新。这是 Vue 的灵魂。

## 1. 为什么要 .value

```javascript
const count = ref(0)     // ref 返回一个"包装对象"
count.value = 1          // 读写都必须通过 .value
console.log(count.value) // 读取
```

`ref(0)` 返回的不是数字，而是一个 `{ value: 0 }` 对象。**为什么包一层？** 因为这样才能在对象属性赋值时"做手脚"——拦截 `value` 的读写，从而知道"谁读了它、谁改它"。

> [!tip] C++ 类比
> `ref` ≈ `std::shared_ptr` 的封装：不直接持有数据，而是通过一个"指针层"访问，指针层可以在读写时加逻辑。
>
> ```cpp
> // 伪代码：ref 的内部大概长这样
> template<class T> class Ref {
>   T value_;
>   std::vector<Observer*> observers_;   // 谁在读我
>  public:
>   T& get() { register_observer(current); return value_; }  // 读时注册观察者
>   void set(const T& v) { value_ = v; notify_all(); }       // 写时通知
> };
> ```
> 是不是很像观察者模式？**Vue 的响应式就是语言层面的观察者模式**。

## 2. 依赖追踪是怎么发生的

```vue
<script setup>
const count = ref(0)
const double = computed(() => count.value * 2)   // ① 读取 count → 注册依赖
function add() { count.value++ }                  // ② 修改 count
</script>

<template>
  <p>{{ count }} 的两倍是 {{ double }}</p>   <!-- ③ 模板也读取了 count -->
</template>
```

流程：
1. `computed` 的回调执行时读取了 `count.value` → 系统记录"`double` 依赖 `count`"
2. 模板渲染时读取了 `count` 和 `double` → 系统记录"这段 HTML 依赖它们"
3. 你 `count.value++` → 系统发现 `count` 变了 → 通知 `double`（重新计算）和 HTML（重新渲染）

**这就是"响应式"的全部秘密**：读写被拦截 → 记录依赖 → 变化时只通知依赖方。

## 3. computed 为什么存在

```javascript
// 不用 computed：每次都要算
const total = ref(cart.value.price * cart.value.quantity)

// 用 computed：声明"派生值"，依赖变化自动重算
const total = computed(() => cart.value.price * cart.value.quantity)
```

> [!tip] C++ 类比
> `computed` ≈ 一个**带缓存的 getter**：
> ```cpp
> double total() const {
>   if (dirty_) { total_ = price_ * quantity_; dirty_ = false; }
>   return total_;
> }
> ```
> 只在依赖变时才重算，否则用缓存值。`computed` 在 Vue 里就是"懒计算 + 缓存"。

**什么时候用 computed**：一个值是从其他响应式值推导出来的、且模板里用到它。直接声明成 `computed`，而不是在模板里写复杂表达式。

## 4. ref vs reactive（够用就好）

```javascript
// ref：包一层，任何类型都能包（数字/字符串/对象/数组）
const count = ref(0)                    // 访问 count.value
const user = ref({ name: 'Alice' })     // 访问 user.value.name

// reactive：只对对象，直接访问属性
const user2 = reactive({ name: 'Bob' }) // 直接 user2.name
```

> [!warning] 本项目只用 ref
> 我们的前端源码（[[前端游戏状态-game.js]]）**只用 `ref`，没用 `reactive`**。你读懂 `ref` 就够用。`reactive` 知道有这回事即可。

## 5. 真实源码对照

[[前端游戏状态-game.js]] 中，整个游戏状态都是 `ref`：

```javascript
const connected = ref(false)        // 是否已连接
const players = ref([])             // 玩家列表
const entities = ref(new Map())     // 实体 Map<id, {pos, ...}>
const sessionId = ref(0)            // 我的 session ID
const roomId = ref(0)               // 当前房间号
```

当 GameServer 推来 `state_sync`，store 更新 `entities`：

```javascript
gameSocket.on('state_sync', (data) => {
    entities.value = new Map(...)   // 整体替换！
})
```

- `entities.value = 新Map` → 触发所有"读取了 entities"的地方重新渲染
- [[前端3D场景-GameRoom.vue]] 在渲染循环里读 `gameStore.entities` → 下一帧自动用新位置更新 mesh

## 6. 自测

1. 为什么 `ref(0)` 返回的对象不能直接 `count + 1`，必须 `count.value + 1`？
2. `computed(() => a.value + b.value)` 在 a 不变时，会被重复执行吗？

> 答案：1. 包了一层对象，需要 .value 解包；2. 不会，有缓存，只在依赖变化时重算。

## 下一篇

数据是响应式的了，现在看代码长什么样——**组件和模板语法**：[[4-Vue3组件与模板语法]]
