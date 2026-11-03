# 讲义层写作规约（所有写讲义的 agent 都照这份做）

## 0. 权威来源

- **房格与三条纪律**：`/mnt/kb/markdown/书籍/README.md` 第 27–56 行（「讲义 = 写给人看的层」+「§-定位器的四种写法」）。
- **房格样本（必读，照它的质感写）**：
  - `/mnt/kb/markdown/书籍/教材讲义/Effective C++/Item 12 拷贝一个对象的所有组成部分（讲义）.md`（长文档版，600 行）
  - `/mnt/kb/markdown/书籍/教材讲义/操作系统导论/第26章 并发：介绍（讲义）.md`（紧凑版，180 行）——**你写的更接近这一篇**
- **原文稿**：`/mnt/kb/markdown/书籍/教材原文/<书名>/`，**逐字读**，不要凭印象写。

## 1. 产出物

- 路径：`/mnt/kb/markdown/书籍/教材讲义/<书名>/<原文稿basename去数字前缀>（讲义）.md`
  - 例：原文稿 `04-1. Deducing Types.md` → 讲义 `1. Deducing Types（讲义）.md`
  - 例：原文稿 `05-Chapter 5. The C++ memory model and operations on atomic types.md` → 讲义 `Chapter 5. The C++ memory model and operations on atomic types（讲义）.md`
- 一篇原文稿 = 一篇讲义。**只写派给你的那些，不要多写。**

## 2. frontmatter（照抄这个形状）

```yaml
---
title: <原文稿的章名>（讲义）
category: <C++ / 并发编程 / 性能工程>
type: 讲义
audience: 人
tags:
  - <3–6 个关键词>
source: "《<书名>》（第N版，英文原版）第X章"
companion: "[[<原文稿文件名去掉 .md>]]"
---
```

- `companion` 的 wikilink **必须能在库里解析**（kb_health.py 断链必须为 0）。
- `category` 按书固定：EMC++ → `C++`；CiA → `并发编程`；Systems Performance → `性能工程`。

## 3. 正文骨架

```markdown
# <第X章 / Item N> 讲义：<钩子标题>

> **这是什么**：讲义层，写给人看。现象开场、白话解释、图解——牺牲密度换理解。
> **配套**：[[<原文稿>|原文稿]] 是未经净化的全文章节稿（epub 直抽，保留原文），读懂讲义后可回原文核对细节。
> **实战锚点**：<一到两句，点名具体文件与函数>
> **诚实声明**：标注（推演）的是讲解性类比，不是教材原文；其余内容忠实于原章。

---

## 0. 开局：<一个具体的现象场景>

## N. §<定位器> <英文小节名>

### ...（小标题，可选）

## N. 实战锚点：<repo> 怎么面对这件事

## N. 收尾：本章金句清单
```

- **钩子标题**已定（见下面各章清单），**不要自己另起**。
- **§-定位器**取自原文稿的标题编号：
  - EMC++ → `§Item 1`、`§Case 1`（原文稿里有 `## Item 1: …` 与 `### Case 1: …`）
  - CiA → `§5.1`、`§5.1.2`（原文稿里有 `#### 5.1. …` 与 `##### 5.1.2. …`）
  - SP → `§6.2.1`、`§6.3.4`（原文稿里有 `#### 6.1 …`、`##### 6.2.1 …`）
  - **一节覆盖连续几节** → `§6.2–6.3`；**拆成多节** → 两节共用同一个 §号。**不要混用**。
  - 附录 → `§附录 A` 之类，或直接 `§Appendix D`。
- **开局**要的是一个**具体场景**（一段代码 / 一次线上现象），不是定义背诵。
- **实战锚点**必须引用 `/home/xier/dev/game-project/otf-server/` 里的**真实文件与真实函数**。
  - 动手前先 `grep`/`sed` 看一眼，**不许凭印象编**。
  - 仓库实况（2026-11-01，9692 行，零三方依赖）：
    `src/net/tcp_transport.cpp`（epoll 事件循环，528 行）、`src/net/transport.h`、`src/net/frame_codec.cpp`（帧解码，227 行）、
    `src/net/byte_codec.h`（字节序，173 行）、`src/net/metrics.cpp` / `metrics_server.cpp`、`src/session/session_mgr.cpp`、
    `src/room/room.cpp` / `room_mgr.cpp`、`src/world/world.cpp` + `world_config.h`、`src/sim/`（纯逻辑：boss_brain/boss_aggro/boss_anatomy/boss_rewind/player_sim/player_hp/loot_roll/sim_types）、
    `src/app/reap_guard.h`（RAII 守卫，`= delete` 禁拷贝）、`src/app/tick_scheduler.cpp`、`src/config/game_config.h`（参数唯一产地）、`src/proto/`（protocol.h / msg_codec.cpp / snapshot.h）
  - **已知缺口**（可以拿来当「本章没做」的对照）：**无环形缓冲区**（`c.out.resize(old + n)`）、**无对象池**、
    **无零拷贝解析**、**无无锁队列**、**无 KCP/UDP**、**单线程事件循环**（IO 与逻辑未拆）。
  - 锚点也可以是「**这个仓库现在没做，缺什么，会怎么错**」——**诚实写缺口比硬凑对应更有价值**。
- **收尾金句清单**：每条 = **原书原句（加粗，逐字照抄英文原文）** + 一句中文点评。
  8–18 条。金句必须**真的在原文稿里**，去原文稿搜得到。
  ⚠️ **「逐字」是硬要求，且有实测反例** —— 见下面第 5 节的专门条款。

## 4. 三条纪律（README 已定，此处再强调）

1. **自己加的类比必须标 `（推演）`**。书里没说的话不能混进原文转述。
2. **不编造图号**。原书没编号的图就让它没编号；原文稿里的图号（`Figure 6.1.` 等）照抄。
3. **§号必须指向它真正覆盖的那一节**。

## 5. 英文引用的处理

- 书是英文原版 ⇒ **引文逐字用英文**，放在 blockquote 里（`> ` 开头），不要翻译后再引。
- 英文引文后面跟中文讲解时，**另起一行**，不要把翻译混进引文块。
- 代码块照抄原文的代码（```cpp），**不要顺手「修正」原书的代码**。

### ⚠️ 「逐字」是硬要求（2026-11-01 实测出来的）

已写完的 14 篇里，复核查出 **2 处真实微改写**（引文被当成「差不多的话」顺手改了）：

| 原文 | 讲义里错写成 |
|---|---|
| `There's a direct mapping between template type deduction and auto type deduction.` | `There is a direct mapping …` |
| `The downside to this is that atomic loads can be slow operations` | `the downsides to this is that atomic loads can be slow operations` |

**差别只是「There's→There is」「downside→downsides」一个 s** —— 但引文一旦这么标，
读者去查原文就会发现对不上，**「逐字」这个承诺就废了**。

⇒ **收尾金句清单里的每一条，写完必须用 `grep -F` 在原文稿里核过**：

```bash
# 例：把要引的那句原样交给 grep -F（-F = 固定字符串，不当正则）
grep -F "There's a direct mapping between template type deduction" \
  "/mnt/kb/markdown/书籍/教材原文/Effective Modern C++/04-1. Deducing Types.md"
```

- `grep -F` **必须有输出**才算数；没输出就是引错了，**改到有输出为止**。
- 引文里有 `'` `"` `’` `“` 时，用**双引号**包住 shell 参数；引文里有 `"` 时用**单引号**包。
- 引文**跨原文行断点**时，`grep -F` 不会命中（原文中间有换行）⇒ **改取单行可命中的片段**，
  或写两段并在中间标 `...`（分段省略允许，但**每段都要单独 grep 到**）。
- 引号字符可以统一（`“` vs `"` 属排版差异），**但词不能改**（不能加 s、不能换同义词）。
- ⚠️ **末尾标点也必须跟原文一致** —— 实测最常见的一类偏差是：
  原文 `…initializing expression changes, and that means…`（**逗号**，因为原文那半句是片段），
  讲义写成 `…initializing expression changes.`（**句号**）。
  「把片段收成整句」是很自然的下笔冲动，但**它就是改写**。
  ⇒ 引到句子中间时，**末尾留逗号**，别替原文把它句号化。

## 6. 长度

- 短章（EMC++ 的一个 Item）：**200–350 行**。
- 中章（CiA 一章）：**250–450 行**。
- 长章（SP 一章）：**300–500 行**。
- **宁短勿注水** —— 但「开局场景」和「实战锚点」两节不许省。

## 7. 写完自查（每篇都要做）

> ⚠️⚠️ **落盘优先，校验后置**（2026-11-02 实测踩坑）：
> 有一批 agent 死在「反复校验 → trim → 改写」的路上，**额度耗尽时文件还在内存里，全丢了**。
> ⇒ **正确顺序：先 Write 落盘 → 再校验 → 再改**。
> **绝不要**在写文件之前反复自我校验；**也不要**写完先 trim 再落盘。

1. 用 Read 工具**重新读一遍刚写的文件**（CIFS 缓存会骗你），确认：
   - 没有乱码、没有半截句子；
   - frontmatter 完整闭合；
   - 每个 `（推演）` 都该标的都标了；
   - 金句清单里的英文原句**确实能在原文稿里搜到**（抽样 grep 验证 3 条以上）。
2. 全库断链检查：
   ```bash
   cd /mnt/kb && python3 -I markdown/_tools/kb_health.py 2>&1 | head -5
   ```
   必须仍是 `=== 断链 0 条 / 0 篇 ===`。**别人也在并行写**，若出现断链，**先确认是不是自己那篇的**，
   是就修自己那篇；不是就报告、别乱改。

## 8. 禁止

- ❌ **不要在 `/mnt/kb` 里跑 git**（慢 + 锁问题）。提交由 Windows 侧 `pull_markdown.sh` 负责。
- ❌ 不要写 `教材精简/` 层、不要动 `MOC/`、不要动 `原文稿/`、不要改 `_tools/` 里的脚本。
- ❌ 不要 `rm`。
- ❌ 不要用 `sed -i` / `>` 重定向改别人正在写的文件。
- ❌ 不要为了凑长度重复原文整段抄（原文稿就在库里，讲义的价值是**解释**）。
