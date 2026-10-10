---
title: Vue3 路由与导航守卫
category: VUE3
tags:
  - VUE3
  - VueRouter
  - 路由守卫
difficulty: 入门
created: 2026-07-31
modified: 2026-07-31
maturity: 完善
---

> [!info] 一句话
> 前端路由 = 一张 `URL → 页面组件` 的表。浏览器地址变化时，Vue Router 找到对应组件渲染。"导航守卫"= 跳转前先做检查（比如"没登录就滚去登录页"）。

## 1. 路由表：URL → 组件

```javascript
// router/index.js
const routes = [
    { path: '/',                component: Home.vue },
    { path: '/login',           component: Login.vue },
    { path: '/game',            component: GameLobby.vue },
    { path: '/game/room/:id',   component: GameRoom.vue },   // :id 是路径参数
]
```

> [!tip] C++ 类比
> 路由表 ≈ 一个 `map<string, Handler>` 或 switch-case：
> ```cpp
> if (url == "/")        render(HomePage);
> else if (url == "/game") render(GameLobby);
> else if (url starts_with "/game/room/") {  // :id 匹配任意值
>   int id = extract(url); render(GameRoom, id);
> }
> ```
> `:id` 就是路径参数——`/game/room/5` 时 `id=5`。

## 2. 懒加载

```javascript
// 懒加载：点到了才加载该页面的 JS
component: () => import('@/views/GameRoom.vue')
```

> [!tip] 为什么懒加载
> 不懒加载 = 网站一打开就把所有页面的 JS 下载下来（很重）。
> 懒加载 = 打开 /game 才下载 GameLobby 的代码。相当于 C++ 的**按需加载**（动态加载插件/库）。

## 3. 路由守卫：跳转前的安全检查

```javascript
router.beforeEach((to, from, next) => {
    if (to.meta.requiresAuth) {          // 页面要求登录
        const token = localStorage.getItem('token')
        if (!token) {
            next({ name: 'login', query: { redirect: to.fullPath } })  // 未登录 → 登录页
            return
        }
    }
    next()   // 放行
})
```

**工作机制**：每次跳转前执行这个回调。返回 `next()` 放行，`next({...})` 重定向。

> [!tip] C++ 类比
> `beforeEach` ≈ 一个**拦截器/中间件**，跟游戏服务器里每个请求先查 token 一样：
> ```cpp
> void before_each(Request& to) {
>   if (to.requires_auth && !has_token()) {
>     redirect("/login?redirect=" + to.url);   // next({...})
>     return;
>   }
>   continue_jump();   // next()
> }
> ```

**`query: { redirect }` 的妙用**：记录"你本来想去哪"，登录成功后跳回去。在 [[网站前端入口链路-浏览器登录到进房]] 里登录页用它 `route.query.redirect || '/'`。

## 4. 真实源码对照

项目 `router/index.js`：

```javascript
const routes = [
  { path: '/game', name: 'game-lobby',
    component: () => import('@/views/GameLobby.vue'),
    meta: { requiresAuth: true } },            // 这个页面要登录
  { path: '/game/room/:id', name: 'game-room',
    component: () => import('@/views/GameRoom.vue'),
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
  if (to.path === '/admin') { /* 管理员额外检查 */ }
  next()
})
```

**配套**：`localStorage` 存 token（刷新不丢），守卫读 localStorage。这正是 [[6-Vue3状态管理-Pinia|Pinia 那篇]] 说的"localStorage 是持久层，Pinia 是内存缓存"。

## 5. 自测

1. 路由守卫里，用户没登录访问 `/game` 会发生什么？
2. `next({ name: 'login', query: { redirect: '/game' } })` 之后，登录页怎么知道要回 `/game`？

> 答案：1. 被重定向到 `/login?redirect=/game`；2. 读 `route.query.redirect`，再 `router.push(redirect)`。

## 下一篇

概念都齐了，现在实战——**用这些概念读懂真实的前端源码**：[[8-Vue3实战-读懂前端源码]]
