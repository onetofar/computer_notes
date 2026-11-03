---
title: "C++ Concurrency in Action"
category: 教材
type: 教材节点
book: C++ Concurrency in Action
author: "Anthony Williams"
role: 正在读
tags:
  - 教材精读
  - 并发编程
  - C++
created: 2026-11-01
skills:
  - obsidian-markdown
---

# 《C++ Concurrency in Action 第二版》（Williams） · 书名节点

> **这本讲什么**：并发编程的标准教材。第二版覆盖 C++11 线程库全套：内存模型、原子类型与 `memory_order`、
> 锁式与**无锁**并发数据结构、并发代码设计、线程池、并行算法、多线程测试调试。
> **英文原版**（Manning, 2017），库里没有中文版。
>
> **在库里的角色**：`目标.md` 第 3 条「无锁环形队列」与 roadmap 第 1 步「拆 IO 线程 + 逻辑线程」的**主教材**。
> 库里此前的并发材料（JCiP / Java 并发编程的艺术 / muduo 章节）全是 Java 或应用层视角，
> **C++ 侧的内存模型与无锁数据结构是真空**。
>
> 上级：[[教材-MOC|教材索引]]

## 三个层次（别混）

| 层 | 位置 | 篇数 | 定位 |
|---|---|---|---|
| 原文章节稿 | `markdown/书籍/教材原文/C++ Concurrency in Action/` | 28 | **epub 直抽**，逐字保留，未经净化（英文原文） |
| 讲义 | `markdown/书籍/教材讲义/C++ Concurrency in Action/` | 15 篇 | **现象开场 + 白话解释 + 实战锚点**，牺牲密度换理解 |
| 精简 | `markdown/书籍/教材精简/C++ Concurrency in Action/` | 15 篇 | **已开写**，逐条可回查原文行号|

## 目录表


| 章 | 原文稿 | 讲义 | 精简 | 状态 |
|---|---|---|---|---|
| Copyright | [[00a-Copyright]] | — | — | 不立篇 |
| Brief Table of Contents | [[00b-Brief Table of Contents]] | — | — | 不立篇 |
| Table of Contents | [[00c-Table of Contents]] | — | — | 不立篇 |
| Praise for the first edition | [[00d-Praise for the first edition]] | — | — | 不立篇 |
| Preface | [[00e-Preface]] | — | — | 不立篇 |
| Acknowledgments | [[00f-Acknowledgments]] | — | — | 不立篇 |
| About this book | [[00g-About this book]] | — | — | 不立篇 |
| About the author | [[00h-About the author]] | — | — | 不立篇 |
| About the cover illustration | [[00i-About the cover illustration]] | — | — | 不立篇 |
| 第 1 章 Hello, world of concurrency in C++! | [[01-Chapter 1. Hello, world of concurrency in C++!]] | [[Chapter 1. Hello, world of concurrency in C++!（讲义）]] | [[Chapter 1. Hello, world of concurrency in C++!]] | **已精读** |
| 第 2 章 Managing threads | [[02-Chapter 2. Managing threads]] | [[Chapter 2. Managing threads（讲义）]] | [[Chapter 2. Managing threads]] | **已精读** |
| 第 3 章 Sharing data between threads | [[03-Chapter 3. Sharing data between threads]] | [[Chapter 3. Sharing data between threads（讲义）]] | [[Chapter 3. Sharing data between threads]] | **已精读** |
| 第 4 章 Synchronizing concurrent operations | [[04-Chapter 4. Synchronizing concurrent operations]] | [[Chapter 4. Synchronizing concurrent operations（讲义）]] | [[Chapter 4. Synchronizing concurrent operations]] | **已精读** |
| 第 5 章 The C++ memory model and operations on atomic types | [[05-Chapter 5. The C++ memory model and operations on atomic types]] | [[Chapter 5. The C++ memory model and operations on atomic types（讲义）]] | [[Chapter 5. The C++ memory model and operations on atomic types]] | **已精读** |
| 第 6 章 Designing lock-based concurrent data structures | [[06-Chapter 6. Designing lock-based concurrent data structures]] | [[Chapter 6. Designing lock-based concurrent data structures（讲义）]] | [[Chapter 6. Designing lock-based concurrent data structures]] | **已精读** |
| 第 7 章 Designing lock-free concurrent data structures | [[07-Chapter 7. Designing lock-free concurrent data structures]] | [[Chapter 7. Designing lock-free concurrent data structures（讲义）]] | [[Chapter 7. Designing lock-free concurrent data structures]] | **已精读** |
| 第 8 章 Designing concurrent code | [[08-Chapter 8. Designing concurrent code]] | [[Chapter 8. Designing concurrent code（讲义）]] | [[Chapter 8. Designing concurrent code]] | **已精读** |
| 第 9 章 Advanced thread management | [[09-Chapter 9. Advanced thread management]] | [[Chapter 9. Advanced thread management（讲义）]] | [[Chapter 9. Advanced thread management]] | **已精读** |
| 第 10 章 Parallel algorithms | [[10-Chapter 10. Parallel algorithms]] | [[Chapter 10. Parallel algorithms（讲义）]] | [[Chapter 10. Parallel algorithms]] | **已精读** |
| 第 11 章 Testing and debugging multithreaded applications | [[11-Chapter 11. Testing and debugging multithreaded applications]] | [[Chapter 11. Testing and debugging multithreaded applications（讲义）]] | [[Chapter 11. Testing and debugging multithreaded applications]] | **已精读** |
| Index | [[90-Index]] | — | — | 不立篇 |
| List of Figures | [[91-List of Figures]] | — | — | 不立篇 |
| List of Tables | [[92-List of Tables]] | — | — | 不立篇 |
| List of Listings | [[93-List of Listings]] | — | — | 不立篇 |
| 附录A-Appendix A. Brief reference for some C++11 language features | [[附录A-Appendix A. Brief reference for some C++11 language features]] | [[附录 A. Brief reference for some C++11 language features（讲义）]] | [[附录A-Appendix A. Brief reference for some C++11 language features]] | **已精读** |
| 附录B-Appendix B. Brief comparison of concurrency libraries | [[附录B-Appendix B. Brief comparison of concurrency libraries]] | [[附录 B. Brief comparison of concurrency libraries（讲义）]] | [[附录B-Appendix B. Brief comparison of concurrency libraries]] | **已精读** |
| 附录C-Appendix C. A message-passing framework and complete ATM example | [[附录C-Appendix C. A message-passing framework and complete ATM example]] | [[附录 C. A message-passing framework and complete ATM example（讲义）]] | [[附录C-Appendix C. A message-passing framework and complete ATM example]] | **已精读** |
| 附录D-Appendix D. C++ Thread Library reference | [[附录D-Appendix D. C++ Thread Library reference]] | [[附录 D. C++ Thread Library reference（讲义）]] | [[附录D-Appendix D. C++ Thread Library reference]] | **已精读** |

## 与目标的关系

`目标.md` 第 3 条要「**裸写基于 epoll 的事件循环或无锁环形队列**」，
roadmap 第 1 步要「把 reactor 拆成 IO 线程 + 逻辑线程」。

- **第 5 章（内存模型）** 是「无锁」的地基：`memory_order` 的六个取值不是术语，是正确性保证。
- **第 7 章（无锁数据结构）** 直接对着无锁环形队列：CAS 循环、**内存回收**（最难的一半）。
- **第 9 章（线程管理）** 对着「IO 线程 + 逻辑线程」：线程池、`std::future`、任务队列。

> ⚠️ **本书出版于 2017 年，早于 C++20**：`std::jthread` / `std::stop_token` / `std::atomic_ref`
> **都没有**（全文 0 命中）。C++20 那 4 个有用的特性另找，不靠这本。

## 关联

- 上级索引：[[教材-MOC|教材索引]]
- 三层结构说明：[[markdown/书籍/README|书籍领域说明]]

