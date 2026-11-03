---
title: "Linux Kernel Programming"
category: 教材
type: 教材节点
book: Linux Kernel Programming
author: "Kaiwan N. Billimoria"
role: 正在读
tags:
  - 教材精读
  - Linux
  - 内核
  - 可观测性
created: 2026-11-02
skills:
  - obsidian-markdown
---

# 《Linux Kernel Programming: A Practical Guide to Kernel Internals, Writing Kernel Modules, and Synchronization》第二版》（Kaiwan N. Billimoria） · 书名节点

> **这本讲什么**：内核**开发实践** —— 从零搭一个内核开发环境（QEMU 虚拟机 + 交叉工具链），
> 到编译 5.x 内核、写第一个内核模块（LKM）、内核内存分配、CPU 调度器、内核同步原语。
> 第二版，Packt，2022。
>
> **英文原版**（Packt Publishing, 2022 / 2023 Early Access）。
>
> ⚠️ **⚠️ 本地 epub 是「试读本」，不是全书。**
> Early Access 目录列到**第 13 章**，但这个 epub 文件里**只有第 1 章**。
> 目录见 [[00b-Packt Early Access 说明与全书目录]]。
> ⇒ 所以本节点目前**只有第 1 章是已沉淀的**，不是「还没写」。
>
> **在库里的角色**：它是《Systems Performance》**第 13 / 14 / 15 章（perf / Ftrace / BPF）
> 那批工具的「上手环境」配套** —— SP 告诉你「怎么量」，这本书第 1 章告诉你
> 「先把这堆工具装上、跑通」。
>
> 上级：[[教材-MOC|教材索引]]

## 三个层次（别混）

| 层 | 位置 | 篇数 | 定位 |
|---|---|---|---|
| 原文章节稿 | `markdown/书籍/教材原文/Linux Kernel Programming/` | 3 | **epub 直抽**，逐字保留，未经净化（英文原文） |
| 讲义 | `markdown/书籍/教材讲义/Linux Kernel Programming/` | 1 篇 | **现象开场 + 白话解释 + 实战锚点**，牺牲密度换理解 |
| 精简 | `markdown/书籍/教材精简/Linux Kernel Programming/` | 1 篇 | **已开写**，逐条可回查原文行号|

## 目录表

| 章 | 原文稿 | 讲义 | 精简 | 状态 |
|---|---|---|---|---|
| 书名页、版权与目录 | [[00a-书名页、版权与目录]] | — | — | 不立篇 |
| Packt Early Access 说明与全书目录 | [[00b-Packt Early Access 说明与全书目录]] | — | — | 不立篇 |
| 第 1 章 Kernel Workspace Setup | [[01-Chapter 1 Kernel Workspace Setup]] | [[Chapter 1. Kernel Workspace Setup（讲义）]] | [[Chapter 1. Kernel Workspace Setup]] | **已精读** |

## 全书目录（Early Access 版列到第 13 章，**本地只有第 1 章**）

| 章 | 标题 | 本地有？ |
|---|---|---|
| 1 | Kernel Workspace Setup | ✅ |
| 2 | Building the 5.x Linux Kernel from Source - Part 1 | ❌ |
| 3 | Building the 5.x Linux Kernel from Source - Part 2 | ❌ |
| 4 | Writing Your First Kernel Module - LKMs Part 1 | ❌ |
| 5 | Writing Your First Kernel Module - LKMs Part 2 | ❌ |
| 6 | Kernel Internals Essentials - Processes and Threads | ❌ |
| 7 | Memory Management Internals - Essentials | ❌ |
| 8 | Kernel Memory Allocation for Module Authors - Part 1 | ❌ |
| 9 | Kernel Memory Allocation for Module Authors - Part 2 | ❌ |
| 10 | The CPU Scheduler - Part 1 | ❌ |
| 11 | The CPU Scheduler - Part 2 | ❌ |
| 12 | Kernel Synchronization - Part 1 | ❌ |
| 13 | Kernel Synchronization - Part 2 | ❌ |

## 关联

- 上级索引：[[教材-MOC|教材索引]]
- 工具层面配套：[[Systems Performance]]（第 4 / 13 / 14 / 15 章：perf / Ftrace / BPF）
- 三层结构说明：[[markdown/书籍/README|书籍领域说明]]
