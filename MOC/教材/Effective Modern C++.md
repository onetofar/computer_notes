---
title: "Effective Modern C++"
category: 教材
type: 教材节点
book: Effective Modern C++
author: "Scott Meyers"
role: 正在读
tags:
  - 教材精读
  - C++
  - Effective Modern C++
created: 2026-11-01
skills:
  - obsidian-markdown
---

# 《Effective Modern C++》（Meyers） · 书名节点

> **这本讲什么**：C++11/14 时代的 42 条具体做法 —— 类型推导、auto、智能指针的新范式、右值引用与完美转发、
> lambda、并发 API。**它是《Effective C++》的直系续作**：开头三章回顾老书条款，第 4 章起全是新东西。
> **英文原版**（O'Reilly, 2014），库里没有中文版。
>
> **在库里的角色**：补上《C++ Primer》精简层缺的两块 —— **类型推导**（那边几乎空白）与**并发速览**。
> `目标.md` 第 1 条「现代 C++20 与 RAII 的严谨掌控」靠它落地。
>
> 上级：[[教材-MOC|教材索引]]

## 三个层次（别混）

| 层 | 位置 | 篇数 | 定位 |
|---|---|---|---|
| 原文章节稿 | `markdown/书籍/教材原文/Effective Modern C++/` | 12 | **epub 直抽**，逐字保留，未经净化（英文原文） |
| 讲义 | `markdown/书籍/教材讲义/Effective Modern C++/` | 8 篇 | **现象开场 + 白话解释 + 实战锚点**，牺牲密度换理解 |
| 精简 | `markdown/书籍/教材精简/Effective Modern C++/` | 8 篇 | **已开写**，逐条可回查原文行号|

## 目录表


| 章 | 原文稿 | 讲义 | 精简 | 状态 |
|---|---|---|---|---|
| From the Publisher | [[01-From the Publisher]] | — | — | 不立篇 |
| Acknowledgments | [[02-Acknowledgments]] | — | — | 不立篇 |
| Introduction | [[03-Introduction]] | — | — | 不立篇 |
| 1. Deducing Types | [[04-1. Deducing Types]] | [[1. Deducing Types（讲义）]] | [[1. Deducing Types]] | **已精读** |
| 2. auto | [[05-2. auto]] | [[2. auto（讲义）]] | [[2. auto]] | **已精读** |
| 3. Moving to Modern C++ | [[06-3. Moving to Modern C++]] | [[3. Moving to Modern C++（讲义）]] | [[3. Moving to Modern C++]] | **已精读** |
| 4. Smart Pointers | [[07-4. Smart Pointers]] | [[4. Smart Pointers（讲义）]] | [[4. Smart Pointers]] | **已精读** |
| 5. Rvalue References, Move Semantics, and Perfect Forwarding | [[08-5. Rvalue References, Move Semantics, and Perfect Forwarding]] | [[5. Rvalue References, Move Semantics, and Perfect Forwarding（讲义）]] | [[5. Rvalue References, Move Semantics, and Perfect Forwarding]] | **已精读** |
| 6. Lambda Expressions | [[09-6. Lambda Expressions]] | [[6. Lambda Expressions（讲义）]] | [[6. Lambda Expressions]] | **已精读** |
| 7. The Concurrency API | [[10-7. The Concurrency API]] | [[7. The Concurrency API（讲义）]] | [[7. The Concurrency API]] | **已精读** |
| 8. Tweaks | [[11-8. Tweaks]] | [[8. Tweaks（讲义）]] | [[8. Tweaks]] | **已精读** |
| Index | [[12-Index]] | — | — | 不立篇 |

## 与同族书的关系

| 书 | 库里状态 | 关系 |
|---|---|---|
| [[Effective C++]] | 60 篇原文稿 + 56 篇讲义 | **上一本**。库里那份 epub 文件名写着「中文第三版」，但正文其实是**第二版**（55 条，止于「条款55 让自己熟悉Boost」）；第三版的并发章（Items 78–82）**不在文件里** |
| 本书 | 12 篇原文稿 | **下一本**。EMC++ 开头三章回顾老书条款，第 4 章起全是 C++11/14 新东西 |

> ⚠️ **两本书的条款不重叠**：EMC++ 的 42 条里没有一条是老书的 55 条。
> ⚠️ **老书第三版的并发章（78–82）不必另找**：本书第 7 章（The Concurrency API）
> 加上 [[C++ Concurrency in Action]] 的第 5、7 章，覆盖面更宽、含 C++11 以后的写法。

## 关联

- 上级索引：[[教材-MOC|教材索引]]
- 三层结构说明：[[markdown/书籍/README|书籍领域说明]]

