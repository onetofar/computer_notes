# 知识库体检脚本

两个只读脚本，放在这里是为了**跟着 vault 一起走**（写到 `.scratch/` 会被系统清理，
写进笔记又会污染 frontmatter）。它们按自身位置反推 vault 根，不含绝对路径。

## 用法

```powershell
# 全库体检：断链 + frontmatter 有效性
python markdown/_tools/kb_health.py

# 教材锚点体检：每条 L### 引文回查原文所在行
python markdown/_tools/kb_anchor_check.py

# 只查某几篇
python markdown/_tools/kb_anchor_check.py "markdown/教材精简/网络多人游戏架构与编程/第5章 对象复制.md"
```

## 两个脚本各自的口径

### `kb_health.py`

- 扫描范围 = **整个 vault**（`MOC/`、`Excalidraw/`、`md_pict/` 都在内），
  不是只有 `markdown/`。只扫 `markdown/` 会把所有指向 MOC 的链接误报成断链。
- 跳过的目录：`.git` `.obsidian` `.trash` `Base` `node_modules`。
  `md_pict/` **必须扫** —— 图片附件在 vault 根下，排除它等于把 50 多条图片链接全报成断链。
- wikilink 解析前先抹掉**代码围栏内**与**行内反引号内**的内容，
  否则 C++ 的 `std::array<T>` 与 GDScript 的 `arr[[0]]` 会被当成断链。
- 目标解析同时登记 `name` 与 `stem` 两种 key，所以 `[[笔记名]]` 和 `[[笔记名.md]]` 都认。
- `.excalidraw.md` 额外登记一个 `名字.excalidraw` 的 key —— Obsidian 的链接解析是
  「去掉 `.md` 的全名」，所以 `[[名字.excalidraw]]` 有效、`[[名字]]` 是断的。

### `kb_anchor_check.py`

- 从笔记里抓 `<某个.md> L123` 或 `L123-456`，后面跟的**引文逐字**去对应行比对（去空白后子串匹配）。
- 报三类错：`行号越界` / `找不到原文稿` / `锚点行不含引文`。
- 只信「行号 + 引文」两列都有。三列模板见 [[第5章 对象复制]] 的「0 · 原文锚点」一节。

## 这个脚本自己被反向验证过

一个永远通过的校验器毫无价值。`kb_anchor_check.py` 交付前造了四种错做反向测试：

| 造的错 | 是否捕获 |
|---|---|
| 行号偏一行（L65 → L64） | 捕获 |
| 篡改引文一个字（赠品 → 赠点） | 捕获 |
| 指向不存在的原文稿 | 捕获 |
| 行号越过文件末尾 | 捕获 |

对未改动的原文件跑，误报 0 条。

> 造这四种错时暴露了一个真实缺陷：比对最初只取引文**前 8 字**，
> 「赠品 → 赠点」正好落在第 9 字上，于是漏网。改成整句比对后才抓住。
> 任何「只取前缀」的校验都要先想一遍：**差异会不会正好落在前缀之外**。

## 已知边界

- `.excalidraw.md` 会被当成普通笔记计入 `files`，其 frontmatter（`excalidraw-plugin: parsed`）
  不是键值映射 —— 报「frontmatter 异常」属误报。
- `kb_anchor_check.py` 的引文比对按「笔记里写的必须是原文行的子串」。
  如果原文层之后继续插入内容（当前正在做「逐段解读」补注），行号会整体位移，
  **锚点会集体失效但脚本不会报错** —— 因为原文里那句话还在，只是搬了家。
  补完一批「逐段解读」后必须重跑并整体更新行号。