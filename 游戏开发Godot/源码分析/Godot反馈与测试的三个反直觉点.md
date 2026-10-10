---
title: "Godot反馈与测试的三个反直觉点"
category: 游戏开发
tags:
  - 源码分析
  - 游戏开发
  - Godot
  - GDScript
  - 踩坑
  - 测试
difficulty: 进阶
source: 'otf-game 原型期（commit 960985c 前后）与农耕教程的实战沉淀｜项目：F:\game_project\otf-game'
created: 2026-10-06
skills:
  - obsidian-markdown
---

> [!info] 核心概念
> 三条**与项目无关**的 Godot 反直觉点，来自 otf-game 与农耕教程两个项目的实战：
> headless 测试里的 lambda 捕获是**按值**的；命中停顿必须用
> `ignore_time_scale=true` 的定时器才能恢复；物理体位置赋值后要
> `await physics_frame` 否则会被碰撞恢复顶开。
> 它们都不是"我写错了"，是**引擎行为与直觉相反**——所以换个项目还会再踩一次。

> [!note] 拆分说明
> 本文从旧笔记 `Boss部位节点系统-boss.gd` 拆出。那篇描述的
> `scripts/boss.gd` **已不存在**（架构已重构为 sim/view 分离），
> 但其中**与具体项目无关**的引擎陷阱仍然成立，因此保留在这里。
> otf-game 当前的部位系统见 [[Boss部位破坏与受击区-boss_anatomy]]。

## 一、lambda 捕获局部变量是**按值**的

**症状**：headless 测试里发射一个信号，回调里给局部变量赋值，**回调永远看不到新值**。

**原因**：GDScript 的 lambda 捕获局部变量时持有的是**副本**（闭包持有副本），
不是引用。回调执行时改的是副本，函数返回后副本销毁。

**解法**：用一个**数组或字典当"可写引用"**——它们是引用语义：

```gdscript
var landed := [false]
boss.part_hit.connect(func(_part, _dmg): landed[0] = true)
# …触发攻击…
await get_tree().process_frame
assert(landed[0], "攻击未命中")
```

同理，闭包里想累加计数要用 `var count := [0]` 然后 `count[0] += 1`，
写 `var count := 0` 再 `count += 1` 是**改副本**，断言会永远失败。

> **为什么值得单独记**：这个 bug 的表现是"断言莫名其妙不过"，
> 而直觉会指向"事件没连上"或"攻击没触发"。**排查方向完全错**。

## 二、命中停顿必须用 `ignore_time_scale=true` 的定时器恢复

**症状**：`Engine.time_scale = 0` 之后游戏永久卡死，取消不了。

**原因**：**普通定时器受 `time_scale` 影响**。停帧期间 `time_scale = 0`，
所有普通 `create_timer()` 的时间流速也是 0，**恢复它的那个定时器自己也不会走**。

**解法**：

```gdscript
func _hitstop(duration: float) -> void:
	Engine.time_scale = 0.0
	await get_tree().create_timer(duration, ignore_time_scale=true).timeout
	Engine.time_scale = 1.0        # 此刻 time_scale 仍为 0，这一行照常执行
```

> **注意顺序**：恢复 `time_scale` 必须在**自己的 timer 回调里**，
> 不能放在设置停帧的那个函数末尾——那个函数已经 `await` 挂起了。

**顺带**：停帧期间**所有**基于 `delta` 的逻辑都会停，
这正是停帧的效果（"画面冻住、逻辑冻住"）。但粒子、UI 动画若要继续动，
需要单独用 `ignore_time_scale` 的 tween 或直接乘一个不受影响的时钟。

## 三、物理体位置赋值后必须 `await physics_frame`

**症状**：测试里把玩家 `global_position = ` 放到 Boss 身边，
下一行就断言命中——**没打中**，或者"玩家莫名其妙被弹开"。

**原因**：直接赋 `global_position` 只改了**变换**，**没有走物理服务器的移动流程**。
下一个物理帧时，碰撞解算仍按上一帧的旧位置算，会把玩家推回"不重叠"的位置。
如果赋的位置与 Boss 重叠，物理引擎会主动把两者分开。

**解法**：

```gdscript
player.global_position = spot
await get_tree().physics_frame        # 让物理服务器消化这次移动
await get_tree().physics_frame        # 状态查询类 API 建议等两个帧
```

**为什么有时要等两个**：`Area2D` 刚 `monitoring = true` 时，
broadphase 更新与 overlap 列表刷新**不在同一帧**。
这与 [[Boss部位破坏与受击区-boss_anatomy]] 里"Hitbox 激活后要等两个物理帧"
是同一个机制的两种表现。

**连带坑**：测试玩家被 Boss 身体顶开时，**站位要放在 Boss 碰撞体之外**，
否则它会在你设置位置后立刻把你挤出去，导致"位置明明设对了却不生效"。

## 易错点

- **lambda 捕获按值**这条不只影响 GDScript。C++ 里 `[&]` 是引用捕获，
  两种语言在这一点上**正好相反**——从 C++ 切过来最容易踩。
- **`ignore_time_scale` 的定时器本身仍会正常流逝**，
  所以它可以用来实现"停帧期间的 UI 动画"，不只能用来恢复停帧。
- **等一个帧还是两个帧，取决于你要的 API**：
  物理状态（`PhysicsServer2D`）一个帧；overlap 列表（`get_overlapping_areas`）两个帧。

## 关联笔记

- [[游戏开发-MOC|游戏开发 领域MOC]]
- [[Boss部位破坏与受击区-boss_anatomy|Boss 部位破坏与受击区]] — 项目侧现状与判定层踩坑
- [[TileMap目标格坐标换算-game.gd|TileMap 目标格坐标换算]] — 另一项目（农耕教程）的 GDScript 源码分析
