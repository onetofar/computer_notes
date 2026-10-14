---
title: "03-view层领读（客户端）"
category: 个人项目
tags:
  - 个人项目
  - monster
  - 客户端
  - view
  - BossAI
  - 领读
difficulty: 深入
source: 'otf-game/scripts/view/ 30 个 .gd 全量精读｜深度领读，行号 @ 90e1359'
created: 2026-10-12
skills:
  - obsidian-markdown
type: 项目领读
---

# 03 · view 层领读（`otf-game/scripts/view/`）

> **怎么读**：view 层是**表现与决策的交界带** —— 它是全工程唯一一处「该迁服务器的还没迁、该留下的必须留下」的地方。
> 读完这一篇你会明白：**哪些代码正在为阶段二做镜像、哪些是永久的表现层、哪些混在一起分不开。**

## 0 · 覆盖边界

| | |
|---|---|
| **本文覆盖** | `scripts/view/` 全部 **30 个 .gd 逐个精读**（`player32.gd` 951 · `level_01.gd` 894 · `boss_entity.gd` 720 · `boss_ai.gd` 494 · `town.gd` 494 · `boss_attack_fx.gd` 423 …）· 一次越界核验：`scenes/boss.tscn` 未覆盖 `skip_turn` |
| **本文不覆盖** | `ui/`（[[04-UI层领读]]）· `net/`（[[05-联机与编排领读]]）· `sim/`（[[01-sim层领读]]）· GameConfig 的值来源（[[02-配置与数据层领读]]）· **`.tscn` 场景结构**（除 boss.tscn 一次 grep） |
| **诚实声明** | §5 全部为**静态阅读所得**，未运行 Godot、未跑回归。涉及"实测症状"的条目是**转述代码注释里的历史记录**，我没有独立复现。`boss_brain.cpp` 未读，§3 的决策/表现分类依据 `boss_ai.gd:15-19` 自述的镜像契约，**未与 C++ 侧逐行对照**。 |

---

## 1 · 一句话说清这一层在干什么

**view 层同时干两件事，而这正是它最难拆的原因**：

- **决策**（迁服务器）：Boss 状态机、仇恨、命中裁决、伤害结算
- **表现**（永久留下）：动画、音效、粒子、震屏、飘字

**边界划在「数据源」，不在「表现层」。** 联机开关 `GameConfig.online_mode` 在 view 里有 **8 处**读取（`boss_ai.gd:56,138` · `boss_entity.gd:65` · `level_01.gd:147,229,266` · `player32.gd:81,826`），每一处切的都是「这个数从哪来」，不是「画不画」。

---

## 2 · 四条承重设计

### 2.1 Boss AI 在客户端**照抄**服务端口径

`boss_ai.gd:15-27`（枚举与轮转常量）、`boss_ai.gd:186-272`（状态机全流程）。

```
IDLE → CHASE → CHARGE → ATTACK → RECOVERY → BREATH → CHARGE_UP
```

**必须与 C++ 逐条同步的五处**：枚举顺序、轮转推进点、到站即停、目标丢失中止、几何口径。

**为什么现在必须这样**：只迁一半 → 「功能做了、玩起来没有」。单机玩第一关永远看不到 BREATH / CHARGE_UP，症状像"没做完"而不是"没镜像"。

> 客户端这一份的对照物是 [[05-Boss决策与仇恨领读|server 05]]。

### 2.2 联机模式有**两道闸**关掉本地 Boss AI

| 闸 | 位置 | 做什么 |
|---|---|---|
| 闸一 | `boss_ai.gd:52-69` | `_ready` 直接 `set_physics_process(false)` + **根本不实例化 `_aggro`** |
| 闸二 | `boss_ai.gd:133-140` | `_physics_process` 第一拍再兜底关一次 |

**为什么要两道**：只在一处关太脆 —— `combat_demo.gd:56-57` 的 0 键调试会把 `physics_process` 重新打开。结果是"客户端自己追自己"，屏幕上一切正常而服务器上 Boss 根本没动过。

**判据**：任何新增的「开关/热重载/调试键」路径都会被兜底闸抓住，不必逐处排查。

### 2.3 攻击距离是**方向性**的，不是固定半径

`boss_ai.gd:43-47`（理由）→ `boss_ai.gd:346-350`（`_reach_towards`）：

```
reach = max(轮廓点在方向单位向量上的投影) × boss_scale
```

**为什么不能用固定半径**：

| 错法 | 症状 |
|---|---|
| 半径太小（背后 380+，停步圈才 146） | **永远在走、永远推着玩家、rig 永远 walk、永不攻击** |
| 半径调到 400+ | 正面远远站着**隔空咬** |

`_strike` / `_breath_cone` / `_ram_sweep` **三处共用同一口径**，输入统一来自 `boss.body_poly()`（`boss_ai.gd:78`）—— 数据驱动怪换轮廓即自动生效。

### 2.4 判定滞后、标记不滞后 —— 两者**刻意解耦**

| | 位置 | 用哪个朝向 |
|---|---|---|
| **判定**（Hitbox） | `boss_entity.gd:422-432` `zones_settled()` | 滞后的 |
| **标记**（红块/闪白） | `part_flash.gd:13-17` + `:138-140` | **当前**朝向，不等判定 |

**为什么**：让标记跟着判定一起滞后 → 玩家绕到左侧、龙转向时，红块"自己先跑到左边去了"。

> **判据：判定可以滞后，表现不能撒谎。**

---

## 3 · `boss_ai.gd` 的决策/表现分界线（最难拆的部分）

### A. 决策（已迁 C++，客户端只剩镜像）

- `State` + `AttackKind` 枚举 + `ROTATION_SIZE`（`:21-27`）
- `_physics_process` 整个 match（`:156-275`）
- 轮转推进 `_finish_attack`（`:279-282`）与唯一读取点（`:189`）
- 命中裁决 `_strike`(355) / `_breath_cone`(394) / `_ram_sweep`(421)
- 换向判定 `_dir_of` + `_face_towards` 的冷却节流（`:453-485`）

### B. 表现（永远不迁）

`_set_rig_moving`(306-310) · `_sync_fx` + `_proto_state`(287-302) · telegraph 渐亮(201,230) · `boss_strike()` 的一次性震屏(379)。

> `_breath_cone` **刻意不**调震屏（`:410-413`：每拍都判，震动会连抖 1 秒）。

### C. 两者都有 —— 四类混合，单独点出

1. **`_reach_towards`(`:346-350`)** —— 算的是**裁决半径**，输入是表现层的 `boss.body_poly()`。几何来源换怪，攻击判定就变 → **决策与表现的数据边界没有对齐到 sim 层**。
2. **`_rig_steady_on`(`:336-341`)** —— 这是**决策闸门**，判据来自动画树的 `current_state()`。**决策依赖表现的内部状态**：服务端无法复现（服务端没有动画树，只有自己的 tick 计数）。
3. **`_boss.is_turning()` 作闸门(`:173,319`)** —— 同上，且因 `skip_turn=true` 恒为 false，**实际已失效**（§5.2-1）。
4. **`_windup()`(`:195,201`) 做 telegraph 分母** —— 这条是**同源的好设计**（决策时长一改表现自动跟随）；但 `_strike` 的 `GameConfig.boss_attack_gap` / `boss_hit_dot_threshold`(`:366-368`) 是**裁决与表现共用的裸 GameConfig**，不走取数口（§5.4-1）。

---

## 4 · 联机模式下「客户端说了算」的 13 个临时裁决点

阶段一让客户端说了算、本该在服务器的裁决，**逐条**列在 §4 表里是本文最有用的部分之一。这里给压缩版：

| # | 裁决点 | 现在 | 联机时怎么变 |
|---|---|---|---|
| 1 | Boss 状态机本体 | 客户端跑 | 只播，快照里的 `BossState` 经 `apply_attack_state` 下发（`boss_entity.gd:709`） |
| 2 | 仇恨表 `_aggro` | 实例化 sim 层 | **连仇恨表都不建**（`boss_ai.gd:56-58`） |
| 3 | 攻击命中判定 + 伤害 | 客户端裁决 | `boss_brain.cpp` 裁决，客户端这两块**整体删除** |
| 4 | 部位血量 | `_sim.apply_hit` | 走 `net_hit_reported` 上报，`_sim` 不实例化（`boss_entity.gd:43-44`） |
| 5 | 部位破坏硬直 + 腿破减速 | 本地计时 | 随 `notify_net_part_broken` 下发（`:606-612`） |
| 6 | 玩家血量 / i-frames / DoT | 客户端 | 服务端裁决客户端只播 |
| 7 | 技能冷却时钟 | 组件自己的 `_clock` | 走 `cooldowns_snapshot` / `cooldowns_restore`（`skill_component.gd:316-322`） |
| 8 | **掉落掷骰 + 背包 + 落盘** | `level_01.gd:335-358` | ⚠️ **缺口**：判定逻辑已共用（171 条向量），但调用点还在客户端 |
| 9 | **采集判定 + `depleted_until`** | 客户端字典（`:462-488`） | ⚠️ **缺口**：注释已自认「T10 再判断」 |
| 10 | 玩家打 Boss「打到哪一块部位」 | `hitbox.gd:62-73` 取最近块 | **保持不变** —— 协议已明文规定客户端判定 |
| 11 | 顿帧强度 | `Engine.time_scale = 0.08` 硬编码 | 服务端给 `duration_ms`，客户端 `request_from_server`(`impact_fx.gd:129-138`) 只映射 |
| 12 | 三个 AI 队友占 player_id 2..4 | `companion_ai.gd:39-60` | 换成服务器广播的实体 |
| 13 | 玩家移动 | `move_and_slide` | `net_driven=true` 时位置由联机层写；**服务器没有物理碰撞**，本地再走一遍就是"假移动" |

**结论**：**真正的缺口只有第 8、9 两条**（掉落与采集仍纯客户端），外加联机协议里没有 run-state 字段（`level_01.gd:264-266` 自认的已知缺口）。

---

## 5 · 实测发现的异常（只报事实）

### 5.1 该走 GameConfig 却硬编码（30 余处，挑代表性的）

| 位置 | 值 | 性质 |
|---|---|---|
| `level_01.gd:583` | `Engine.time_scale = 0.08` | **顿帧强度硬编码**，而 ImpactFx 同一件事走 `GameConfig.impact_*_scale` |
| `boss_attack_fx.gd:41` | `BREATH_SPREAD := 0.40` | ⚠️ 注释自认「**与**服务端 dot > 0.35 的锥**接近**」——**表现锥角与权威锥角不是同一个数** |
| `aim_reticle.gd:13` | `HITBOX_RADIUS := 18.0` | 注释写"命中判定的**宽容圈**" —— 参与命中判定的数值却没走 GameConfig |
| `player_controller.gd:12,26,57,78` | `SPEED 220 / ROLL_SPEED 460 / …` | **遗留 demo 控制器**，全部硬编码 |
| `boss_attack_fx.gd:40-52` | `BREATH_LEN 250 / CORES 3 / …` | 文件头声明「颜色/密度是纯视觉常量」，属**已声明的口径例外** ✅ |

> 判据：`boss_attack_fx.gd:41` 那条**不是**例外 —— 它参与攻击表现的范围，与权威锥角是同一个物理量的两份。

### 5.2 过期注释

1. **★ `boss_ai.gd:468` —— 最实的一条。** 注释：「转向动画链长达 1.9~2.2s，斜角处不加节流会导致 Boss 反复转身卡住」。

   **实测推翻**：`boss_walk_tree.gd:48` 的 `skip_turn` 默认 `true`，且 **`scenes/boss.tscn` 没有覆盖它**（该文件只设了 `respond_to_input=false` / `start_state="idle_back"`）→ 现役龙的转向是**硬切**，不存在 1.9~2.2s 的转向链。`:474` 的 `_turn_cooldown = GameConfig.boss_turn_rate` 依据已失效。

2. **同因第二处**：`:335`「front 无 tb，最多等一轮 1.27s」—— `skip_turn=true` 时 `boss_walk_tree.gd:174-176` 走硬切，不再经 tb 退场。

3. **`:333-341` 整条 `_rig_steady_on` 契约**依赖 `current_state().begins_with("idle_")`；`monster_rig.gd:88-89` 保留 `idle_` 前缀正是为它 —— 但这条闸门在服务端无法复现（见 §3-C-2）。

4. **`boss_attack_fx.gd:291`** 用 `GameConfig.boss_breath_time` 归一化喷流进度 —— 联机下这是**本地猜测值**（服务端吐息时长由自己 tick 决定），与 `:25` 声称的"与服务端 BrainConfig 同名同值"是一句**未验证的假设**。

5. **`player32.gd:763-766`**：「与 **0.2 的**实测手感逐位相同」「TTK **16.5s → 12.5s**」—— 引用旧构建的实测数据，同句自己承认"与现役构建不同源"。

6. **`boss_ai.gd:43-46`**：「背面体表离原点 **380+**」「接触距离 **400+**」 vs 同文件 `:234-236`「0.4s × **520px/s** 走 **208px** 而停步圈才 **146**」—— 三组具体数字全部来自 GameConfig，但**注释里没有任何一处标注对应哪个键**，无法自证。

7. **`town_npc.gd:41-45`**：`COL_NEAR`/`COL_IDLE` 声明"值与 `UiKit.COL_ACCENT` **逐位相同**（同源，不是第二份定义）" —— 但代码在本文件**另存了一份字面量**。注释与实现矛盾。

### 5.3 同段逻辑在两个 view 文件里重复

| 逻辑 | 两处 | 危害 |
|---|---|---|
| **整套瓦片装配**（`_make_tileset`/`_foot_poly`/`_atlas_of`/`_hash_idx`/`_parse_args`/`_setup_camera`/`TILE`/`SCALE`/`PROP_SORT_ORIGIN`=27） | `level_01.gd:22-69,642-786,789-808` ↔ `town.gd:43-52,130-140,229-321,326-369` | 关卡与城镇改一处得改两处 |
| **顿帧写 `Engine.time_scale`** | `level_01.gd:579-586` ↔ `impact_fx.gd:47-98` | ⚠️ **互不协调**：两者各有独立状态（`_hit_stop_busy` vs `_scale_left`）。若 ImpactFx 正持有 0.35 而 `_hit_stop` 进来，它退出时会**无条件写回 `1.0`**（`:585`），中途状态被覆盖。注释只说了"只震不改 time_scale"，**未处理反向覆盖** |
| **四向 → 动画向映射** | **三份**：`boss_entity.gd:38` / `boss_entity.gd:623-627` / `boss_ai.gd:489-493` | 改一处两处不一致 |
| 四向 → 单位向量 | `boss_ai.gd:383-388` ↔ `boss_attack_fx.gd:198-205` | |
| 四相位整数像素方波 bob | `player32.gd:684` ↔ `monster_rig.gd:99-101` | |
| `zone_table()` 的 GameConfig 兜底 | `part_flash.gd:115-118` ↔ `zones_debug.gd:48-51` | 同款三行 |

### 5.4 同一字段被多个文件各自算一次

1. **★ 攻击节律一半走取数口、一半走 GameConfig。** `boss_ai.gd:103-126` 为 windup/attack_time/recovery/cd/damage 建了 **5 个「问 Boss 要」的 getter**，但同文件的 `_strike` 路径**绕过它们**直读 `GameConfig.boss_ram_windup`(192)、`boss_breath_time`(205)、`boss_ram_dash`(222)、`boss_ram_speed`(240)、`boss_ram_back_speed`(228)、`boss_breath_range`(404)、`boss_breath_damage`(407)、`boss_ram_width`(439)。

   而 `monster_boss.gd:116-137` 只 override 了那 5 个口 → **数据驱动怪的冲撞与吐息仍拿铁鳞飞龙的数值，且零报错。**

   > 这是「取数口建了但不守」—— **建口本身不产生隔离，得所有读路径都走口**。

2. **两套玩家 hitbox 参数命名**：`player_controller.gd:83` 用 `GameConfig.player_hitbox_forward`，`player32.gd:442` 用 `GameConfig.player32_hitbox_radius`。
3. **两套步频常量**：`player32.gd:681` 用 `paperdoll_walk_fps`，`monster_rig.gd:95` 用 `boss_walk_fps`。
4. **`last_hit_part` 四个消费方各自遍历**：`skill_component.gd:326-330` / `companion_ai.gd:170-195` / `focus_marker.gd:84` / `level_01.gd:285` —— 都走 Boss 公开口（✅ 正确），但 `level_01._broken_parts`(`:537-545`) 另走 `BossCatalogLib.entry()` 枚举部位。

### 5.5 私有字段被外部直读

1. **`boss_attack_fx.gd:200`**：`String(boss.get("_facing"))` —— 直读 `boss_entity` 的私有字段，而同体系**已有公开口** `facing()`（`boss_entity.gd:330-331`，`part_flash.gd:207` 就在用）。
2. **`combat_demo.gd:56-57`**：`ai.set_physics_process(...)` 直接开关 Boss AI 的物理帧 —— **正是 `boss_ai.gd:133-140` 兜底闸要防的那条路径**。
3. **`companion_ai.gd`（30 处 `.set()/.get()/.call()`）** —— 这些字段在 `player32.gd` 里都是**公开 var**，走 `set()` 是为了兼容旧 demo 控制器。

---

## 6 · 三条可复用的判据

### 6.1 「取数口」要守住全部读路径

`boss_entity` 用「一组取数口」而不是「一个 monster_id + if」（`:119-132`）—— 这个设计**是对的**，它挡住了「改 JSON 零效果、零报错」。但 §5.4-1 证明：**口建了，8 条读路径里 3 条绕过它**。

> **建口不产生隔离。判断隔离是否真的存在，要数读路径的条数。**

### 6.2 整数像素纪律

`player32.gd:655-673`（走路）、`:674-690`（停下回零）、`:18-24`（伪骨骼自限）。

- **sub-pixel** → 最近邻采样在两像素间反复跳 = "糊**且**抖"，比不动更糟
- **相位浮点累加停在 1 或 3** → 人**站成一个迈步的姿势**而画面无任何解释

`monster_rig.gd:92-101` 是同款四相位方波。**伪骨骼不能屈膝、两腿不能分别动**（上 Skeleton2D 会丢掉已验证的整数像素性质）。

### 6.3 纸娃娃的降级契约

`player32.gd:284-300`（多退少补 + 降级）、`:341-345`（先 `exists()` 再 `load()`）。

> **这一层画洋红，其余层照常，人还能动。**

**为什么用 `push_warning` 不用 `push_error`**：`run_all.ps1` 的日志扫描器把 ERROR 判成失败 —— 降级是**预期路径**，不该让回归红。

**实测依据**：本项目指向不存在文件**静默过四次**（`--check` 全绿、`file_exists` 为真、背包里是空图）。

> ⚠️ 层序：实测 `cloak ∩ body = 100%` → **cloak 必须在 body 之前**。写反的症状是"穿上护甲看不见护甲"而日志全绿。

---

## 7 · 待拍板

1. **`boss_ai.gd:468` 那条注释** —— `skip_turn=true` 是有意还是遗留？若是遗留，该改的是 `boss.tscn` 打开 `skip_turn`，不是改注释。
2. **§5.3 的顿帧双写** —— ImpactFx 与 `_hit_stop` 互相覆盖是**真 bug**（不是设计），应合并到一个口。
3. **§5.4-1 的取数口漏读** —— 8 条读路径补齐是一条机械工作，收益是新怪参数真正生效。
4. **`level_01.gd` / `town.gd` 的瓦片装配重复** —— 约 150 行，抽到 `sim/` 之外的一个 `view/tile_kit.gd` 是自然解（但**不能进 sim/**，它 import 渲染）。

---

## 8 · 相关

- [[00-客户端项目领读|00 客户端项目领读]] —— 目录地图
- [[01-sim层领读|01 sim 层领读]] —— view 消费的那一层，也是唯一能进 `sim/` 的纯逻辑
- [[02-配置与数据层领读|02 配置与数据层领读]] —— GameConfig 与「新增怪读自己 JSON」的判据
- [[05-Boss决策与仇恨领读|server 05 Boss 决策与仇恨]] —— `boss_ai.gd` 镜像的 C++ 原件
- [[04-UI层领读|04 UI 层领读]] · [[05-联机与编排领读|05 联机与编排]]
