---
title: Boss部位节点系统—boss.gd
category: 游戏开发
tags:
  - 已废弃
  - 游戏开发
  - Godot
  - GDScript
difficulty: 入门
source: ~~scripts/boss.gd（项目：F:\game_project\otf-game）~~ **该文件已不存在**
created: 2026-08-06
modified: 2026-10-06
commit: 960985c（HEAD；部位节点系统为工作区未提交改动）
maturity: 草稿
skills:
  - obsidian-markdown
---

> [!danger] 本文正文已删除 —— 它描述的代码不存在了
> 写于 2026-08，锚定 `scripts/boss.gd`（单文件原型）。那个文件**已被删除**，
> 部位系统重构为 **sim 裁决层 / view 表现层分离**，部位也从
> 「左前爪 / 右前爪 / 背部」改成 B1 五部位（Head / WingL / WingR / Tail / Leg）。
>
> 保留这个文件（而不是删掉）是因为 MOC 与其它笔记有链接指向它，
> 直接删会产生红链。下面是它应该去的地方。

## 内容去哪了

| 原内容 | 现在在哪 |
|---|---|
| 部位血量 / 倍率 / 破坏 / 核心血结算 | **[[Boss部位破坏与受击区-boss_anatomy]]** |
| 部位受击区节点、转向、命中反馈 | 同上（`view/boss_entity.gd` 一节） |
| 攻击盒查询 / 命中优先级 / 飘字定位 | 同上（源码领读一节） |
| `intersect_shape` 默认不查 Area2D | 同上（踩坑区） |
| `intersect_shape` 结果没有 `position` 键 | 同上（踩坑区） |
| GDScript 缩进必须全 tab | 同上（踩坑区） |
| lambda 按值捕获 / `_hitstop` 需 `ignore_time_scale` / 位置赋值后 `await physics_frame` | **[[Godot反馈与测试的三个反直觉点]]**（这些与项目无关，单独成篇） |

## 原文里已经过时的结论（别再看）

- ❌ **「头部是眩晕条，阈值 ×1.2 梯度上升」** —— 口径已变。
  现在是**五部位全部走"破坏"语义**，头破 = **永久破坏 + 2.0s 时限硬直**。
- ❌ **「总血 20000 远大于各部位血之和，部位全破不代表 Boss 死亡」** ——
  现在的配平口径是 **5 破 ×550 + 沿途芯片 ≈ 3000 = 核心血 → 破完正好击杀**。
- ❌ **「朝向下所有部位任何角度都能打」**（原文的待办项）——
  **已实现**。现在打不打得到由**逐朝向的受击区生成表**决定，见新笔记。
- ❌ **部位为「左前爪 / 右前爪 / 背部」** —— B1 拍板改为
  **Head / WingL / WingR / Tail / Leg**。
- ⚠️ 原文 `damage: float`、`hp: float` —— 现在的实现是 **int 血量**，
  且经 `DamageData` 统一入口结算（禁 `x.hp -= 10`）。

## 关联

- [[游戏开发-MOC|游戏开发 领域MOC]]
- [[Boss部位破坏与受击区-boss_anatomy|Boss部位破坏与受击区（现行实现）]] ← **要看这个**
- [[Godot反馈与测试的三个反直觉点|Godot 反馈与测试的三个反直觉点]]（通用引擎陷阱）
