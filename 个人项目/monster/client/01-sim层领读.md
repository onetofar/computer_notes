---
title: "01-sim层领读（客户端）"
category: 个人项目
tags:
  - 个人项目
  - monster
  - 客户端
  - 纯逻辑
  - sim
  - 领读
difficulty: 深入
source: 'otf-game/scripts/sim/ 17 个 .gd 共 152 KB · 对照 otf-server/src/sim/｜深度领读，行号 @ 待提交'
created: 2026-10-12
skills:
  - obsidian-markdown
type: 项目领读
---

# 01 · sim 层领读（`otf-game/scripts/sim/`）

> **怎么读**：这是客户端唯一一层**不许碰引擎**的代码。它的存在理由不是整洁，是**将来要搬走**。
> 服务端有一份 C++ 对偶实现，见本文 §3 与 [[05-Boss决策与仇恨领读|server 05]]。
> 📖 教材结合方向与前几篇一致：项目 → 解读 → 教材，单向。

## 0 · 覆盖边界

| | |
|---|---|
| **本文覆盖** | `scripts/sim/` 全部 17 个 .gd（152 KB）**逐个读完** · `test/check_sim_purity.sh` 的守什么 |
| **本文不覆盖** | `view/` `ui/` `net/` `config/` `data/`（各有专篇）· 服务端 `boss_brain` / `boss_rewind` / `player_sim` 三个无对偶文件 · `resource/` 下的 json 数据内容 |
| **核对方式** | 静态通读 + 全仓消费者检索。**没有跑 Godot** |

---

## 1 · 承重设计一：sim 全目录**不读** GameConfig

这是本层最重要的一条，而且它**看起来违反约束 1**。

```
约束 1：参数一律走 GameConfig
sim 层：不读 GameConfig ← 为什么
```

`boss_anatomy.gd:13` 一句话说尽：

> ## 确定性要求：同样的输入序列 → 同样的状态。无随机、无时钟、无场景树。
> ## 参数由 setup() 注入（**不引 GameConfig autoload —— 参数也是输入**）。

四个注入点：`boss_anatomy.gd:37 setup(config)` / `boss_aggro.gd:28 setup(config)` /
`inventory.gd:57 new_inventory()` / `ttk_metrics.gd:79 predict()`。

**为什么 autoload 在这里是毒**：autoload 是**引擎侧的全局状态**，sim 一引用它，
① 这层就再也搬不到服务器（C++ 里没有 Godot autoload）；② 单测无法逐条喂构造数据。

**但注入不能太严，否则老调用方会炸**（`boss_anatomy.gd:38-42`）：

```gdscript
func setup(config: Dictionary) -> void:
	## ★ 先铺默认值再 merge：核心血那一组参数是后加的，老调用方（单测、别的场景）
	##   只会传部位那一组 —— 缺键直接 `_config["core_chip_ratio"]` 会在 apply_hit 里
	##   抛 "Invalid access to property or key"，而且抛在 sim 内部最难查。
	_config = {"head_mult": 1.0, "core_chip_ratio": 0.0, "break_core_damage": 0}
	_config.merge(config, true)
```

**「先铺默认再 merge」是本层反复出现的模式**——注意默认值全是「关闭语义」（1.0 / 0.0），
不是玩法真值。这与 C++ 侧 `AnatomyConfig` 的全 0 默认值**是同一个决定**
（见 [[05-Boss决策与仇恨领读|server 05]] §12）。

**改动地图**：改成直接读 `GameConfig.xxx` → 全目录产生引擎依赖，
`check_sim_purity.sh` 的 `$[A-Z]` 规则当场变红，且 HUD 每 0.1s 算 TTK 时会带出一个 autoload 查找。

---

## 2 · 承重设计二：四份独立实现，同一条纪律 —— 先在副本上模拟，全成才提交

`inventory.gd:160-192` · `crafting.gd:113-135` · `equipment.gd:237-252` · `storage.gd:152-158`

四处**互不相干**的实现，执行同一条纪律。理由写在 `inventory.gd:159`：

> 边加边发现装不下时状态已改，回滚 bug 面比模拟大得多

企划 §1.1 的原话把它说成产品问题：**「手滑丢掉一次，玩家对整个循环的信任就没了」**。

**它挡住的失败是 partial success**——玩家看到「背包满了」，但一半东西悄悄没了。
这类症状最恶的地方是**它看起来像 bug 但像得对**。

**改动地图**：改成边做边判 → 失败路径要回滚，四处各要一套回滚代码；
且 `crafting.gd:132` 那条「材料未扣」的保证同时失效，变成白干一次。

---

## 3 · sim 模块之间**不互相引用**，依赖一律从参数进

`ground_loot.gd:113 reclaim(..., add_all: Callable)` · `crafting.gd:73-75` ·
`equipment.gd:218-220` · `consumable.gd:103-104`

`ground_loot.gd:38-40` 给了理由：

> 检查器把 `preload`/`load` 列为表现层依赖，跨脚本引用即违规

**所以 sim 要用另一个 sim 的能力时，传 `Callable`**，不传类：

```gdscript
# ground_loot.gd:113 的形状
reclaim(..., add_all: Callable)
```

**这一条同时解决了两个问题**：跨文件依赖检查器抓不到、以及 `Callable` 版本可替换实现
（测试里塞假实现就能验调用参数）。与 [[06-协议与验收客户端领读|server 06]] §9
「验收工具与被验收对象的重复是必要的」是同一种判断的两面。

**改动地图**：改成直接 `InventoryLib.add_all(...)` → 纯度检查器红线。

---

## 4 · 承重设计三：手写 xorshift32，因为 GDScript int 是 64 位有符号

`loot_roll.gd:57-72`（xorshift32）、`:113-129`（normalize_seed）

`loot_roll.gd:35-39` 点出了那个不显眼的坑：

> GDScript int 是 64 位有符号，不 `& 0xFFFFFFFF` 拉回就会让 `>>` 变**算术右移**，
> 与 C++ `uint32_t` **静默**分叉

**「静默」是关键词**——两端各自算各自的掉率，差了 0.1%，谁都发现不了。
这与 `架构评审-熵减-2026-10` 里的 B2 是同一条：**掉率差一点点，谁都发现不了。**

**为什么不用引擎的 `RandomNumberGenerator`**（`loot_roll.gd:15-18`）：
它的实现与版本、平台、标准库绑定，**没有任何跨语言一致性保证**。
拿它当「双实现一致」的基准等于在流沙上盖楼。

**改动地图**：改用 `RandomNumberGenerator` → 与服务端 `loot_roll.cpp` 的一致性归零，
而两边唯一的对账手段就是「共用固定种子 + 期望输出向量」。

---

## 5 · 承重设计四：遍历顺序与「不消耗随机数」是**契约**

`loot_roll.gd:88-89`（两处）、`:99-107`

```
部位名 ASCII 升序 · core 表恒最后 · 表内按原序
未命中不摇第二个随机数 · 整张表被跳过时一次都不摇
```

**为什么这要写成契约**：两端的数据结构不同（C++ `std::map` vs GDScript `Dictionary`），
键序天然不同。顺序不一致的后果是**同一 seed 出不同掉落**——而测试报的是
**假阳性红**（代码其实没错，只是顺序不同）。假阳性比真 bug 更消耗信心：
修着修着就麻木了。

所以作者选择**先把契约写死，再让人写实现**。同样的道理写在 `loot_roll.gd:74`：

> ★ 遍历顺序契约（缺了它两端排序不一致 = **假阳性红**）

`target_plan.gd:52,137,148-149` 里有同款约束（目标选择的遍历序）。

**改动地图**：改成按插入序遍历 → 两端立刻分叉，且分叉点是「掉率」，静默。

---

## 6 · 承重设计五：存档向后兼容 —— 缺字段是**事实**，不是损坏

`inventory.gd:277` · `ground_loot.gd:168` · `equipment.gd:308` · `storage.gd:179`

四处一致：`data == null` → `ok = true` + 空状态。

`ground_loot.gd:163-166` 把理由说死：

> 把版本号往上顶会让每个存量存档当场变坏档 = **为了字段整齐把玩家进度判死**

**它挡住的失败是「新增一个系统就判死全部存档」**——而这是那种每次加功能都要重新想一遍的事。

**对偶的一条**：校验不过时返回 `ok=false`，**绝不静默降级**。
`target_plan.gd:139-145` 记了一个真实事故：`core` 是字符串时 typed `Array` 形参在
**调用点**抛错，异常冒出去把整行打成 `{}`，面板崩在离病因很远的地方。
修法是形参改 `Variant` + 显式判型（`:221`），畸形数据记进 `schema_errors` 而不是抛出。

**改动地图**：缺字段即 `ok=false` → 上一轮交付的所有存档不可读；
形参改回 typed → 一个 Boss 的表畸形 → 整行数据丢失 + UI 崩溃。

---

## 7 · 承重设计六：边界判定带 EPS，因为 `0.1+0.2 != 0.3`

`status_machine.gd:46` · `skill_cooldowns.gd:27,35` · `skill_rules.gd:31,78`

三处各自独立定义 `EPS := 0.000001`，理由一致：浮点在「恰好到点」时把首次结算推到下一帧。

`status_machine.gd:181-184` 还记了上一版的写法：

> `next_tick <= budget + EPS` —— 严格小于会让这一跳晚一帧

**它挡住的症状**：「偶尔少跳一次 DoT」「看着 CD 好了却还要再等」。
**单测用整值 dt 测不出来**——必须用能触发浮点误差的 dt 才现形。

**改动地图**：去掉 EPS → 低帧率/累积时间下判定漂移一帧，极难复现。

---

## 8 · 承重设计七：`next_tick` 是**倒计时**，不是已过计数器

`status_machine.gd:161-169`（三行规则）、`:185-193`

`:161-166` 直接记着踩坑史：

> 曾按「已过」写过一版，`next_tick` 每跳一次变大，**循环永不退出**，`tick(dt=1.0)` **挂死进程**

`:170-173` 记着第二处：

> 零头余量不扣回倒计时 —— 单帧大 dt 的测试全过但实机 DoT 永不结算

**第二条特别值得注意**：测试用的 `dt=1.0` 恰好**掩盖**了它。
这就是「测试全绿但功能坏了」的典型形态——**测试的 dt 和实机的 dt 不在同一数量级时，
边界条件就成了摆设**。

**改动地图**：`next_tick` 改成已过计数器 → 死循环；
漏 `next_tick -= budget` → 30fps 下 12 秒一次不跳。

---

## 9 · 承重设计八：状态查询与状态落地分离，「表现不能撒谎」

`consumable.gd:103 plan()` · `:70-77 removables()`

`plan()` **只算不落**，不扣背包（`:100-102`：扣库存与落盘是调用方的事）。

解除类在没东西可清时**拒绝且不扣物品**，并用 `_removals()`（`:183`）作为
**界面灰显与实际判定的同一个函数**。

**它挡住的失败**：扣掉一瓶药、提示「已使用」、身上什么都没变 = 骗玩家；
静默什么都不做更糟。

**「灰显与判定同一个函数」是本模块反复出现的纪律**——
本项目「同屏两处不同源」的问题已经出过多次（见 `game_config.gd` 的 `effective_*` 入口）。

**改动地图**：两处各写一遍判定 → 按钮亮着但按下去被拒。

---

## 10 · 承重设计九：DoT 判据取自 status.json 定义，**不持名单**

`consumable.gd:202-214 _is_dot()` · `status_machine.gd:238-247 total_hp_over()`

`_is_dot` 的判据是 `tick_effect.hp < 0`；`total_hp_over` 是 `floor(duration/interval)`
× 每次 hp 的纯算术，给出可复核的总伤害。

**它挡住的失败**：手抄一份 DoT 名单就是第二个家，第三种 DoT 加进来时名单不自动更新
——净化卷轴清不掉新 DoT，**零报错**。

**改动地图**：改成名单白名单 → 症状静默，且随状态种类增长而腐化。

---

## 11 · 承重设计十：唯一状态变更入口 + 快照/恢复

`boss_anatomy.gd:83 apply_hit()`、`:122-132` · `boss_aggro.gd:102-108` · `skill_cooldowns.gd:67-72`

`apply_hit` 带 `timestamp` 口子（延迟补偿回溯键）、返回事件 dict（广播体）；
已击杀后任何命中不结算（`:87-89`，第二道闸）。

`boss_anatomy.gd:5-10` 的文件头把「为什么这么设计」写成了项目的核心论点：

> ★ 这是"服务器口子"（用户核心论点：**状态一致性 > 并发规模**）：
> 　· 阶段二本对象整体搬进 C++ 服务器，客户端只发"我命中了部位 X"事件、读广播状态
> 　· apply_hit 带 timestamp 口子：阶段二做延迟补偿时，服务器用它回溯命中时刻的状态

**`timestamp` 这个字段现在没有消费者，但它是阶段二的地基**——
见 [[03-世界与命中领读|server 03]] §2：服务器只存 8 tick 的 Boss 位姿，
而「命中时刻」这个键正是客户端上报的。

**改动地图**：拆成 `set_hp()` / `break_part()` 多个入口 → 事件顺序与广播体不再唯一。

---

## 12 · 承重设计十一：感知圈用「进/出两个半径」制造滞后

`boss_aggro.gd:47-49` · `escape_mult`

进用 `aggro_radius`，出用 `aggro_radius × 1.15`；还有 `give_up_dist` 与
`threat_decay_per_sec` 双重遗忘。

`boss_aggro.gd:74-76` 有一条容易漏的规则：**翻滚断仇恨「只削不新建」**——
身上没有仇恨的人不该滚一下就凭空获得记录。

**它挡住的失败**：目标在边界抖动导致 AI 每帧横跳；以及「滚一下凭空被仇恨」。

**改动地图**：单半径 → 边界抖动；改成「滚一下就建仇恨」→ 翻滚从脱战技变成嘲讽技。

---

## 13 · 承重设计十二：TTK 逐步模拟，且与真机**逐挥击**对齐

`ttk_metrics.gd:79-118 predict()` · `:46-49 swing_damage()` · `:34 MAX_HITS`

`predict` 不用经验公式，**逐次挥击模拟**；`swing_damage` 的取整
`int(round(damage*mult))` 与 `boss_anatomy.gd:91` **逐字一致**。

`ttk_metrics.gd:20-25` 承认模型与真机可能分叉，靠测试逐挥击跑真的 `BossAnatomy` 钉住。

`ratio()` 在 X=0 时返回 `-1.0` 而非 0/1（`:122-123`）——那两个数都会被读成「比值完美达标」。

**它挡住的失败**：「面板说 12.5 秒、实战要 30 秒」这种没人发现的事。

---

## 14 · sim ↔ 服务端 C++ 的对应关系

服务端 `otf-server/src/sim/` 与客户端的对偶情况：

| 客户端 | 服务端 | 类型 |
|---|---|---|
| `loot_roll.gd` 6.2 KB | `loot_roll.{cpp,h}` 6.8 KB | **原样移植**，规格级独立重写 |
| `boss_anatomy.gd` 5.3 KB | `boss_anatomy.{cpp,h}` 11.5 KB | **原样移植** |
| `boss_aggro.gd` 4.4 KB | `boss_aggro.{cpp,h}` 11.8 KB | **原样移植**（+ 房间内 slot 结构） |
| 其余 14 个 | 无对偶 | **客户端独有** |

**服务端有 3 个客户端没有的对偶文件**：`boss_brain`（行为层，已从 `view/boss_ai.gd` 抽取）、
`boss_rewind`（回溯）、`player_sim`。这三个是**服务端领先**，不是客户端遗漏——
`boss_brain` 的抽取判据见 [[05-Boss决策与仇恨领读|server 05]] §1。

**唯一用「测试向量」对账的一对是 `loot_roll`**：两边都声明「不共享代码，只共享规格」，
共用 `data/test_vectors/loot_roll.json` + 固定种子。

**新发现的一处不对称**：`loot_roll.gd:57 xorshift32` / `:68 normalize_seed`
在 `scripts/` 与 `test/` 全仓**零外部调用**（只在 `_roll_entry` 与 `roll` 内部用）。
它们是给未来的 M6 服务端加载器预留的接口——但**当前是死代码**。

---

## 15 · ★ 实测发现（已修 1 处，其余待决）

### 15.1 `boss_anatomy.gd` 配平注释停在翻倍前（**已修 2026-10-14**）

```
原：5 破 × 550 + 沿途芯片 ≈ 3000 = 核心满血
现：5 破 × 1100 + 沿途芯片 ≈ 6000 = 核心满血
```

真值在 `game_config.gd:442-448`，且 `:444` 明写「2026-10-08 翻倍（原 3000 / 破坏削核 550）」。

**与 `game_config.gd:215` 的 `boss_scale 1.5` 是同一天漏的两处**（2026-10-08：
路线 C 把 boss_scale 1.5→3.0，狩猎时长扩展把核心血 3000→6000）。
两处都是「值改了、旁边的算式没跟着改」。

> **判据**：一次改一批数值后，要按「**这一天改了哪些数**」去 grep 注释里的这些数，
> 而不是凭印象检查自己改的那几行。

### 15.2 `inventory.gd:105 slot_capped()` 零引用

全仓（`scripts/**` + `test/**`）除定义处外**无任何引用**。相邻的 `is_hard_full()`（`:92`）
已覆盖同类判据。**属死代码，但删不删要看是否打算作为公共 API 保留。**

### 15.3 七处重复的「检查器不剥注释」说明

`inventory.gd:27-28` · `ground_loot.gd:17-18` · `storage.gd:24-25` · `equipment.gd:26` ·
`consumable.gd:23-24` · `gather_logic.gd:19-20` · `loot_roll.gd:25-27`

同一段解释被复制了七份，措辞略有差异。而 `check_sim_purity.sh:7-10` 的文件头
已经完整解释过同一件事（「这是绊线不是证明」）。**注释重复，不是逻辑重复**——
但七份里任何一份漂移都会误导读者以为另外六份不存在。

### 15.4 `float_throttle.gd:38-40` 的字面量初值

`_merge_ms := 400.0` / `_min_amount := 1` 是**字段初值**，随后由 `configure()`（`:49`）
用 `GameConfig.hud_float_merge_ms` / `hud_float_min_amount` 覆盖。

**当前默认值与真值巧合相同**，所以未发现分叉。但注释 `:17-18` 声明
「阈值是入参，不是本文件里的常量」，而代码里存在字面量初值——**若忘记调 `configure()`
就会静默用这组字面量**，而值恰好一样所以测不出来。

### 15.5 `class_name` 覆盖不一致

5 个文件有：`skill_cooldowns` `skill_rules` `float_throttle` `boss_aggro` `boss_anatomy`。
其余 12 个没有，靠 `game.gd:149` 之类的别名（`InventoryLib.new_inventory(...)`）。

**是不一致，不是错误**——但 `class_name` 在无编辑器环境下缓存不可靠，
本项目已因此踩过坑（见 CLAUDE.md 关于 preload 的记载），所以「哪些用 `class_name`」
需要一条明文约定而不是随缘。

### 15.6 未发现「硬编码玩法数值」

除 15.4 那处字段初值，17 个文件逐个核过：伤害系数、概率、容量、冷却、仇恨参数、
堆叠上限**全部**走 `config` 形参或 `items.json` / `status.json` / `equipment.json` /
`recipes.json` / `drops.json` / `gathering.json`。`skill_rules.gd:37-62` 的换算系数
（`1.0 + attack_mult` 等）是**公式**不是数值。

---

## 16 · 验证命令

```bash
# 约束 2（必须 WSL；本篇所有结论的前提就是它通过）
wsl bash /mnt/f/game_project/otf-game/test/check_sim_purity.sh

# 客户端回归（sim 的测试在里头）
powershell -File otf-game/test/run_all.ps1

# 查某字段到底谁在用（本文 §15 的死值结论都是这样验的）
#   在 otf-game/scripts 与 otf-game/test 下检索，除定义行外应零命中

# 服务端对偶的编译期不变量
cd /mnt/f/game_project/otf-server && ./dev.sh build && ./dev.sh test
```

## 阅读顺序建议

`boss_anatomy.gd` 全文（131 行，**最小但最完整**地展示了本层全部纪律：注入、merge、
唯一入口、timestamp 口子）→ `loot_roll.gd` 全文（88 行，展示「跨语言一致」的算法侧）
→ `inventory.gd` 的 `add_all` 与存档读入 → `status_machine.gd:161-196`（倒计时语义 + EPS）
→ `boss_aggro.gd` 全文（88 行）→ 剩下按需。
