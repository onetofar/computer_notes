---
title: Godot场景与节点入门—第一次接触
category: 游戏开发
tags:
  - 游戏开发
  - Godot
  - GDScript
  - 入门
difficulty: 入门
source: otf-game 项目（F:\game_project\otf-game）
created: 2026-08-06
modified: 2026-08-06
maturity: 初稿
skills:
  - obsidian-markdown
---

> [!info] 核心概念
> Godot 里一切都建立在三个概念上：**节点（Node）、场景（Scene）、脚本（Script）**。场景是一棵"节点树"，脚本给节点加行为。物理世界有三类节点——角色体（自己动）、静态体（不动挡人）、区域（感应不挡人），本项目的 Player / Boss / 部位正好各用一类。理解这些，再看 `main.tscn` / `player.tscn` / `boss.tscn` 三个文本文件就能读懂整个 Demo。

## 三个核心概念（用乐高比喻）

| 概念 | 类比 | 是什么 |
| --- | --- | --- |
| **节点 Node** | 一块乐高积木 | 有某种功能的最小单元：Sprite2D 显示图片、CollisionShape2D 管碰撞、Camera2D 管镜头、Label 显示文字 |
| **场景 Scene** | 用积木搭好的一组成品 | 节点 + 子节点组成的树，可整体保存（`.tscn` 文件）、可整体复用。一个场景就是一个角色/一个关卡/一个 UI |
| **脚本 Script** | 给积木装"脑子" | 挂在某个节点上的 GDScript 代码，决定这个节点怎么动、怎么响应。本项目脚本都在 `scripts/` 目录 |

> [!tip] 关键认知
> Godot 的 `.tscn` 场景文件是**纯文本**——所以人类可以直接手写它（本项目所有场景都是手写/程序生成的）。编辑器只是图形化编辑这个文本。

## 节点树：场景的真实形态

节点是**父子树状**组织的。父节点动，子节点跟着动。看本项目的 `main.tscn`：

```text
Main (Node2D)                        ← 场景根节点，挂 main.gd
├── Player (CharacterBody2D)         ← 实例化了 player.tscn 整个场景
│   ├── Visual (Node2D)
│   │   └── Body (Sprite2D)          ← 主角贴图，scale 0.054
│   ├── CollisionShape2D             ← 32x32 的移动碰撞盒
│   ├── AttackVisual (Sprite2D)      ← 攻击特效，默认隐藏
│   └── Camera2D                     ← 跟随玩家的镜头
├── Boss (StaticBody2D)              ← 实例化了 boss.tscn 整个场景
│   ├── Visual/Body (Sprite2D)       ← Boss 贴图，scale 0.15
│   ├── CollisionShape2D             ← 240x240 身体碰撞
│   ├── PartHead (Area2D)            ← 头部部位节点
│   ├── PartLeftPaw / PartRightPaw / PartTail / PartBack (Area2D)
└── HUD (CanvasLayer)                ← 独立 UI 层，不随世界缩放
    ├── BossHPBar (ProgressBar)      ← Boss 总血条
    ├── BossHPLabel / Message / Hint (Label)
```

## 物理体三兄弟：理解三类"会动的对象"

这是理解本项目（甚至所有 2D 游戏）最关键的一张表：

| 类型 | 通俗理解 | 会用到的 API | 本项目里 |
| --- | --- | --- | --- |
| **CharacterBody2D** | "自己会动的人" | `move_and_slide()` 移动；撞墙被挡 | Player |
| **StaticBody2D** | "不动的柱子/墙" | 不需要，天生不动 | Boss 身体 |
| **Area2D** | "感应圈，不挡人" | 重叠检测、空间查询命中 | 5 个部位节点 |

> [!tip] 为什么部位用 Area2D？
> 部位是"Boss 身上一块能被精确打中的判定区域"——它不需要挡人，只需要**能被攻击检测到**。Area2D 不参与物理碰撞（玩家能穿过它），但空间查询可以命中它。所以 Boss 身体用 StaticBody2D 挡人，部位用 Area2D 做判定，两者互补。

## 从零创建：完整流程（对应本项目每一步）

1. **新建项目**：Godot 编辑器 → New Project → 生成 `project.godot`（工程配置：窗口大小、输入映射、主场景）。
2. **新建场景**：编辑器 → 新场景（New Scene）→ 选**根节点类型**。本项目根节点分别是 `Main`(Node2D)、`Player`(CharacterBody2D)、`Boss`(StaticBody2D)。
3. **添加子节点**：右键加子节点——Sprite2D（显示图）、CollisionShape2D（碰撞形状，需要先建一个 Shape 资源）、Camera2D（镜头）。
4. **挂脚本**：选中节点 → Attach Script → 生成 `xxx.gd`，在 `_ready()`（进入场景时）/`_physics_process(delta)`（每物理帧）里写行为。
5. **场景嵌套（实例化）**：在 `main.tscn` 里点拖 `player.tscn` / `boss.tscn` 进去，变成 Main 的子节点——这就是"把整个角色场景装进主场景"。
6. **设主场景**：`project.godot` 里 `main_scene` 指向 `main.tscn`，按 F5 运行就从这个场景开始。

对应到文件系统：
```text
otf-game/
├── project.godot      # 第1步：工程配置（含输入映射 WASD + 攻击键）
├── scenes/
│   ├── main.tscn      # 第2步+第5步：主场景，实例化了 player/boss
│   ├── player.tscn    # 第2步：玩家场景
│   └── boss.tscn      # 第2步：Boss 场景
├── scripts/           # 第4步：所有脚本（main/player/boss/part.gd）
└── resource/          # 美术素材（PNG）
```

## 碰撞层（Collision Layers）：怎么让"打得到"但"走不过"

每个物理节点有 `collision_layer`（自己属于哪层）和 `collision_mask`（会和谁碰撞）。

| 物体 | layer | mask | 效果 |
| --- | --- | --- | --- |
| Boss 身体 | 2 | — | 属于第 2 层 |
| 部位节点 | 4 | 0 | 属于第 4 层，自己不探测任何东西 |
| Player 移动 | 1 | 3 (=1+2) | 移动时被层 1、层 2 挡住 → 被 Boss 身体挡住 |
| Player 攻击查询 | — | 2\|4 | 攻击判定主动去查层 2 + 层 4 → 既打身体又打部位 |

> [!tip] 关键差异
> Player 的**移动**用 mask=3，所以走得过去的地方、过不去的墙都正常；Player 的**攻击**不用 mask，而是手动构造一个空间查询去查层 2|4，还额外开了 `collide_with_areas` 才能命中 Area2D 部位。移动和攻击是两套独立的检测体系。

## 关联笔记

> 编写时查阅 [[游戏开发-MOC|游戏开发 领域MOC]]

- [[游戏开发-MOC|游戏开发 领域MOC]]
- [[Boss部位节点系统-boss.gd|Boss 部位节点系统源码分析]]

## ⚠️ 踩坑/待办

### 已确认的坑

- **`.tscn` 引用了脚本就必须在节点上写 `script = ExtResource(...)`**，漏写则脚本不附加，节点没行为。
- **Area2D 默认 `collision_layer=1`**，必须显式改（本项目部位用 4），否则会被别人误命中。
- **`_ready()` 只执行一次**（进场景时），每帧逻辑要写 `_physics_process(delta)`（物理帧）或 `_process(delta)`（渲染帧）。

### 【待确认】的问题

> [!question] 【待确认】每个节点的 scale 是怎么定的？
> Player 贴图 scale 0.054、Boss 贴图 scale 0.15——原始 AI 素材图很大（上千像素），缩放让它们在地图上显示成合理大小（主角 ~40px、Boss ~280px，比例约 1:7）。如果换素材，这些 scale 需要重调。
