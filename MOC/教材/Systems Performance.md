---
title: "Systems Performance"
category: 教材
type: 教材节点
book: Systems Performance
author: "Brendan Gregg"
role: 正在读
tags:
  - 教材精读
  - 性能工程
  - Linux
created: 2026-11-01
skills:
  - obsidian-markdown
---

# 《Systems Performance: Enterprise and the Cloud 第二版》（Brendan Gregg） · 书名节点

> **这本讲什么**：性能工程的**方法论** + 工具书：Methodologies（USE 方法）、操作系统、CPU、内存、文件系统、
> 磁盘、网络、云、基准测试，加上 perf / ftrace / BPF 三把尺子。
> **英文原版**（Addison-Wesley, 2020）；第一版的中译本叫《性能之刃》。
>
> **在库里的角色**：`目标.md` 第 2 条「证明深刻理解 CPU Cache」的方法论底座。
> 库里此前的性能材料散在格雷泽第 9 章、彭放第 5 章（VS 性能分析器 / gprof / valgrind），
> **都不成体系** —— 这本把「怎么量」讲成了体系。
>
> 上级：[[教材-MOC|教材索引]]

## 三个层次（别混）

| 层 | 位置 | 篇数 | 定位 |
|---|---|---|---|
| 原文章节稿 | `markdown/书籍/教材原文/Systems Performance/` | 35 | **epub 直抽**，逐字保留，未经净化（英文原文） |
| 讲义 | `markdown/书籍/教材讲义/Systems Performance/` | 21 篇 | **现象开场 + 白话解释 + 实战锚点**，牺牲密度换理解 |
| 精简 | `markdown/书籍/教材精简/Systems Performance/` | 21 篇 | **已开写**，逐条可回查原文行号|

## 目录表


| 章 | 原文稿 | 讲义 | 精简 | 状态 |
|---|---|---|---|---|
| Cover Page | [[00a-Cover Page]] | — | — | 不立篇 |
| About This eBook | [[00b-About This eBook]] | — | — | 不立篇 |
| Half Title Page | [[00c-Half Title Page]] | — | — | 不立篇 |
| Title Page | [[00d-Title Page]] | — | — | 不立篇 |
| Copyright Page | [[00e-Copyright Page]] | — | — | 不立篇 |
| Dedication Page | [[00f-Dedication Page]] | — | — | 不立篇 |
| Contents at a Glance | [[00g-Contents at a Glance]] | — | — | 不立篇 |
| Contents | [[00h-Contents]] | — | — | 不立篇 |
| Preface | [[00i-Preface]] | — | — | 不立篇 |
| Acknowledgments | [[00j-Acknowledgments]] | — | — | 不立篇 |
| About the Author | [[00k-About the Author]] | — | — | 不立篇 |
| 第 1 章 Introduction | [[01-Chapter 1. Introduction]] | [[Chapter 1. Introduction（讲义）]] | [[Chapter 1. Introduction]] | **已精读** |
| 第 2 章 Methodologies | [[02-Chapter 2. Methodologies]] | [[Chapter 2. Methodologies（讲义）]] | [[Chapter 2. Methodologies]] | **已精读** |
| 第 3 章 Operating Systems | [[03-Chapter 3. Operating Systems]] | [[Chapter 3. Operating Systems（讲义）]] | [[Chapter 3. Operating Systems]] | **已精读** |
| 第 4 章 Observability Tools | [[04-Chapter 4. Observability Tools]] | [[Chapter 4. Observability Tools（讲义）]] | [[Chapter 4. Observability Tools]] | **已精读** |
| 第 5 章 Applications | [[05-Chapter 5. Applications]] | [[Chapter 5. Applications（讲义）]] | [[Chapter 5. Applications]] | **已精读** |
| 第 6 章 CPUs | [[06-Chapter 6. CPUs]] | [[Chapter 6. CPUs（讲义）]] | [[Chapter 6. CPUs]] | **已精读** |
| 第 7 章 Memory | [[07-Chapter 7. Memory]] | [[Chapter 7. Memory（讲义）]] | [[Chapter 7. Memory]] | **已精读** |
| 第 8 章 File Systems | [[08-Chapter 8. File Systems]] | [[Chapter 8. File Systems（讲义）]] | [[Chapter 8. File Systems]] | **已精读** |
| 第 9 章 Disks | [[09-Chapter 9. Disks]] | [[Chapter 9. Disks（讲义）]] | [[Chapter 9. Disks]] | **已精读** |
| 第 10 章 Network | [[10-Chapter 10. Network]] | [[Chapter 10. Network（讲义）]] | [[Chapter 10. Network]] | **已精读** |
| 第 11 章 Cloud Computing | [[11-Chapter 11. Cloud Computing]] | [[Chapter 11. Cloud Computing（讲义）]] | [[Chapter 11. Cloud Computing]] | **已精读** |
| 第 12 章 Benchmarking | [[12-Chapter 12. Benchmarking]] | [[Chapter 12. Benchmarking（讲义）]] | [[Chapter 12. Benchmarking]] | **已精读** |
| 第 13 章 perf | [[13-Chapter 13. perf]] | [[Chapter 13. perf（讲义）]] | [[Chapter 13. perf]] | **已精读** |
| 第 14 章 Ftrace | [[14-Chapter 14. Ftrace]] | [[Chapter 14. Ftrace（讲义）]] | [[Chapter 14. Ftrace]] | **已精读** |
| 第 15 章 BPF | [[15-Chapter 15. BPF]] | [[Chapter 15. BPF（讲义）]] | [[Chapter 15. BPF]] | **已精读** |
| 第 16 章 Case Study | [[16-Chapter 16. Case Study]] | [[Chapter 16. Case Study（讲义）]] | [[Chapter 16. Case Study]] | **已精读** |
| Glossary | [[90-Glossary]] | — | — | 不立篇 |
| Index | [[91-Index]] | — | — | 不立篇 |
| Code Snippets | [[92-Code Snippets]] | — | — | 不立篇 |
| 附录A-Appendix A： USE Method： Linux | [[附录A-Appendix A： USE Method： Linux]] | [[附录 A. USE Method： Linux（讲义）]] | [[附录A-Appendix A： USE Method： Linux]] | **已精读** |
| 附录B-Appendix B： sar Summary | [[附录B-Appendix B： sar Summary]] | [[附录 B. sar Summary（讲义）]] | [[附录B-Appendix B： sar Summary]] | **已精读** |
| 附录C-Appendix C： bpftrace One-Liners | [[附录C-Appendix C： bpftrace One-Liners]] | [[附录 C. bpftrace One-Liners（讲义）]] | [[附录C-Appendix C： bpftrace One-Liners]] | **已精读** |
| 附录D-Appendix D： Solutions to Selected Exercises | [[附录D-Appendix D： Solutions to Selected Exercises]] | [[附录 D. Solutions to Selected Exercises（讲义）]] | [[附录D-Appendix D： Solutions to Selected Exercises]] | **已精读** |
| 附录E-Appendix E： Systems Performance Who’s Who | [[附录E-Appendix E： Systems Performance Who’s Who]] | [[附录 E. Systems Performance Who’s Who（讲义）]] | [[附录E-Appendix E： Systems Performance Who’s Who]] | **已精读** |

## 与目标的关系

`目标.md` 第 2 条要「**使用环形缓冲区、对象池、连续内存容器取代离散链表结构，
证明深刻理解 CPU Cache**」——这本书是那半句话的**方法论**：

- **第 6 章（CPUs）**：cache line、false sharing、指令级并行 —— 「连续内存容器」为什么快
- **第 7 章（Memory）**：内存层次、分配器行为、swap
- **第 2 章（Methodologies）**：USE 方法（Utilization / Saturation / Errors）—— **先量后改**
- **第 12–15 章（perf / ftrace / BPF）**：三把尺子

---

## ⚠️ 抽取时踩到的坑（已修，2026-11-01）

本书 toc.ncx 里，**附录 D 的子节点**叫 "Chapter 2—Methodology" / "Chapter 6—CPUs" ……
命中抽取器 `CHAP` 正则的 `chapter\s+\d+`，于是被当成章 ——
**附录 D 被切空，正文落进了「11-Chapter 11—Cloud Computing.md」**。

根因已修（`markdown/_tools/epub_to_chapters.py` 加了「父节点自己也是已选中的章
且与子节点同文件 ⇒ 子节点不成篇」的守卫），并重抽全书验证：
**16 章正文逐字节不变、附录 D 恢复、7 个空壳消失**。

## 关联

- 上级索引：[[教材-MOC|教材索引]]
- 三层结构说明：[[markdown/书籍/README|书籍领域说明]]

