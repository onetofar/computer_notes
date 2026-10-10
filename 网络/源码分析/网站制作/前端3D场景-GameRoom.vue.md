---
title: 前端 3D 场景—GameRoom.vue
category: 网络
tags:
  - 源码分析
  - 网站制作
  - Three.js
difficulty: 入门
source: onetofar_web/frontend/src/views/GameRoom.vue
created: 2026-07-30
modified: 2026-07-30
maturity: 完善
---

> [!info] 一句话
> `GameRoom.vue` 是游戏主界面，用 Three.js 渲染 3D 竞技场，通过 Pinia store 响应式读取实体位置并驱动渲染循环。

## 文件概览

| 项目 | 内容 |
|------|------|
| 路径 | `frontend/src/views/GameRoom.vue` |
| 行数 | ~160 行 |
| 依赖 | Three.js + OrbitControls, Pinia gameStore |
| 核心 | `entityMeshes` Map + `animate()` 循环 |

## 源码领读

### 模板结构

```html
<template>
  <!-- 左侧栏：玩家列表 + 离开按钮 -->
  <n-layout-sider>
    <n-list>
      <n-list-item v-for="p in gameStore.players">
        {{ p.username || p.id }}
      </n-list-item>
    </n-list>
    <n-button @click="handleLeave">离开房间</n-button>
  </n-layout-sider>

  <!-- 3D Canvas -->
  <div class="game-canvas-container">
    <canvas ref="canvasRef"></canvas>
    <!-- HUD：实体数量 + 连接状态 -->
    <div class="game-hud">
      <n-tag>物体: {{ entityCount }}</n-tag>
      <n-tag :type="gameStore.connected ? 'success' : 'error'">
        {{ gameStore.connected ? '已连接' : '未连接' }}
      </n-tag>
    </div>
  </div>
</template>
```

### 挂载与初始化

```javascript
onMounted(async () => {
    const token = userStore.token
    if (!token) { router.push('/login'); return }
    await nextTick()
    initScene()                                  // Three.js 场景
    gameStore.connect(token, roomIdParam, userStore.username)  // 连 WS
})

onUnmounted(() => {
    if (animationId) cancelAnimationFrame(animationId)
    if (renderer) renderer.dispose()  // 释放 WebGL 资源
    gameStore.disconnect()            // 断 WS
})
```

### initScene — Three.js 场景初始化

```javascript
function initScene() {
    scene = new THREE.Scene()
    scene.background = new THREE.Color(0x0a0a1a)  // 深空蓝背景

    camera = new THREE.PerspectiveCamera(60, aspect, 0.1, 1000)
    camera.position.set(15, 10, 15)                // 等距视角

    renderer = new THREE.WebGLRenderer({ canvas: canvasRef.value, antialias: true })
    renderer.shadowMap.enabled = true

    // 灯光
    scene.add(new THREE.AmbientLight(0x404060))
    const dirLight = new THREE.DirectionalLight(0xffffff, 1)
    dirLight.position.set(10, 20, 10)

    // 地面网格 (20x20 格子)
    scene.add(new THREE.GridHelper(40, 20, 0xe94560, 0x333366))

    // 半透明地面
    const ground = new THREE.Mesh(
        new THREE.PlaneGeometry(40, 40),
        new THREE.MeshStandardMaterial({ color: 0x16213e, transparent: true, opacity: 0.5 })
    )
    ground.rotation.x = -Math.PI / 2
    scene.add(ground)

    controls = new OrbitControls(camera, renderer.domElement)
    controls.target.set(0, 2, 0)
}
```

### animate() — 渲染循环（核心）

```javascript
const entityMeshes = new Map()  // 游戏实体ID → THREE.Mesh

function animate() {
    animationId = requestAnimationFrame(animate)
    controls.update()

    // 1. 清理已不存在的实体 —— 玩家离开/阵亡
    for (const [eid, mesh] of entityMeshes) {
        if (!gameStore.entities.has(eid)) {
            scene.remove(mesh)
            entityMeshes.delete(eid)
        }
    }

    // 2. 更新/创建 Mesh
    for (const [id, ent] of gameStore.entities) {
        let mesh = entityMeshes.get(id)
        if (mesh) {
            // 已有 Mesh → 更新位置
            mesh.position.set(ent.pos.x, ent.pos.y, ent.pos.z)
        } else if (ent.pos) {
            // 新实体 → 创建 Mesh
            const geo = ent.type === 'player'
                ? new THREE.BoxGeometry(0.8, 1.6, 0.8)  // 玩家：长方体
                : new THREE.BoxGeometry(1, 1, 1)         // 物体：正方体
            const mat = new THREE.MeshStandardMaterial({
                color: ent.color || 0xe94560
            })
            mesh = new THREE.Mesh(geo, mat)
            mesh.position.set(ent.pos.x, ent.pos.y || 0.8, ent.pos.z || 0)
            scene.add(mesh)
            entityMeshes.set(id, mesh)   // 存 Map，非 getObjectById
        }
    }
    renderer.render(scene, camera)
}
```

> [!warning] 为什么用 Map 而非 `scene.getObjectById(id)`？
> Three.js `Object3D.id` 是内部自增 ID：
> - `getObjectById(1)` → Scene 对象（id=1）
> - `getObjectById(2)` → AmbientLight（id=2）
> 游戏实体 ID 1、2 与 Three.js 内部 ID 冲突。用 **`entityMeshes` Map** 按游戏 ID 存取，彻底隔离。

### Watch — 房间创建后自动跳转

```javascript
watch(() => gameStore.roomId, (n) => {
    if (n && n > 0 && roomIdParam === 'new') {
        router.replace(`/game/room/${n}`)  // /game/room/new → /game/room/5
    }
})
```

### handleLeave — 离开房间

```javascript
function handleLeave() {
    gameStore.disconnect()
    router.push('/game')  // 回到大厅
}
```

### WASD 键盘控制（2026-07-31 新增）

```javascript
// 按下的键集合 + 限流
const pressedKeys = new Set()
let lastSentDir = { x: 0, y: 0 }
const INPUT_THROTTLE_MS = 50   // 20fps 输入频率
let lastInputTime = 0

function handleKeyDown(e) {
  pressedKeys.add(e.key)
  if (['w','W','ArrowUp','s','S','ArrowDown','a','A','ArrowLeft','d','D','ArrowRight'].includes(e.key)) {
    e.preventDefault()  // 阻止页面滚动
  }
}
function handleKeyUp(e) { pressedKeys.delete(e.key) }

function sendInput() {
  const now = Date.now()
  if (now - lastInputTime < INPUT_THROTTLE_MS) return  // 限流

  // 从按下的键计算方向向量
  let dx = 0, dy = 0
  if (pressedKeys.has('w')||pressedKeys.has('W')||pressedKeys.has('ArrowUp')) dy -= 1
  if (pressedKeys.has('s')||pressedKeys.has('S')||pressedKeys.has('ArrowDown')) dy += 1
  if (pressedKeys.has('a')||pressedKeys.has('A')||pressedKeys.has('ArrowLeft')) dx -= 1
  if (pressedKeys.has('d')||pressedKeys.has('D')||pressedKeys.has('ArrowRight')) dx += 1

  if (dx === lastSentDir.x && dy === lastSentDir.y) return  // 去重：方向不变不发
  lastSentDir = { x: dx, y: dy }
  lastInputTime = now
  gameSocket.send('input', { dir: { x: dx, y: dy }, action: 'MOVE' })
}
```

在 `animate()` 循环中每帧调用 `sendInput()`（60fps 检查，20fps 发送）。

**关键设计**：
- **限流 50ms（20fps）**：不比服务器 tick（33ms/30fps）快太多，避免刷爆
- **去重**：方向不变时不发——只有按下/松开方向键才产生网络消息
- **斜向归一化**：W+D 同时按时 `dx=1, dy=-1`，服务端 ArenaDemo 内部 `normalized()` 处理，斜向不快于直线
- **停止信号**：离开房间时发 `dir: {0,0}`，让服务器知道玩家停止

## 上下游关系

```
上游                                  GameRoom.vue                       下游
────                                  ────────────                      ──────
用户键盘事件                        → gameSocket.send('input', ...)    → WsServer
[[前端游戏状态-game.js]] entities    → animate() 读取并更新位置
gameStore.players                    → 模板渲染玩家列表
gameStore.connected                  → HUD 状态显示
URL /game/room/new                   → watch → router.replace 跳转
```

## 上下游文档

- [[前端游戏状态-game.js]] — 数据源：读取 `entities`, `players`, `connected`
- [[前端WS客户端-ws.js]] — 发送键盘输入的 `gameSocket.send()`
- [[协议桥接器-ws_game_bridge.hpp]] — 坐标转换：Bridge 把 2D 竞技场坐标映射到 3D 场景

## 设计要点

1. **entityMeshes Map**：消除 Three.js 内部 ID 与游戏 ID 的冲突
2. **整体替换**：每帧遍历 `gameStore.entities` 创建/更新/删除 Mesh，简单可靠
3. **固定玩家高度 y=0.8**：ArenaDemo 是 2D 模拟，Bridge 映射 2D(x,y)→3D(x,0.8,z)
4. **请求动画帧**：`requestAnimationFrame` 在浏览器重绘前更新，60fps 流畅运行

## Vue3 概念补课

> [!tip] 这是"生命周期 + 响应式 + 模板"的综合示例
> - `onMounted`/`onUnmounted`（申请/释放资源，前端版 RAII）→ [[5-Vue3生命周期与组合式API]]
> - 模板指令（`v-for`/`@click`/`{{ }}`/`:type`）→ [[4-Vue3组件与模板语法]]
> - `watch` 监听 roomId 跳转 URL → [[5-Vue3生命周期与组合式API]]
> - 从 store 读响应式数据驱动渲染 → [[3-Vue3响应式系统-ref与computed]]
