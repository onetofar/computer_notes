---
title: "JCiP-Java并发编程实战"
category: 教材
type: 教材节点
book: JCiP
author: "Brian Goetz 等"
role: 已沉淀
tags:
  - 教材精读
  - 并发编程
created: 2026-10-07
skills:
  - obsidian-markdown
---

# 《Java 并发编程实战》（JCiP）· 书名节点

> **这本讲什么**：并发**设计**而非实现 —— 线程安全的三种定义、
> 死锁治理、可伸缩性、原子类、并发测试。英文原著，库里笔记用 `JCiP` 缩写指代它。
>
> **在库里的角色**：它是**主源**的只有 4 篇（原子类与测试那几章）；
> 另有约 12 篇是 `JCiP / …` 的**多来源之一**，那些挂在对应的主源书节点下。
>
> 上级：[[教材-MOC|教材索引]]

## 对应位置（以本书为主源 · 4 篇）

| 章 | 笔记 |
|---|---|
| 原子类 · `AtomicReferenceFieldUpdater` | [[4-6 AtomicReferenceFieldUpdater详解]] |
| 原子类 · `AtomicStampedReference` / `AtomicMarkableReference` | [[4-7 AtomicStampedReference与AtomicMarkableReference]] |
| 并发性能优化 | [[4-8 并发性能优化全景图]] |
| 并发程序测试 | [[4-9 并发程序测试方法]] |

## 作为参考出现的 12 篇

这些笔记的主源是别的书（多为 [[Java并发编程的艺术|《Java 并发编程的艺术》]]
或 [[CSAPP-深入理解计算机系统|CSAPP]]），frontmatter 写成 `JCiP / X`：

`markdown/并发/` 下 1-1 并发基础理论 / 1-2 DCL 专题与内存模型 / 1-a 全链路打通与易错点 /
2-1 锁体系核心理论 / 2-2 并发工具与活跃性 / 2-a 全链路打通与易错点 /
3-1 线程池核心 / 3-2 Worker与FutureTask / 3-3 线程池关闭与调优 /
3-a CompletionService与定时线程池 / 3-b 监控排查与最佳实践 / 4-1 活跃性问题与死锁治理 /
4-2 性能与可伸缩性优化 / 4-3 AQS 同步器与状态依赖管理 / 4-4 原子变量与非阻塞算法 /
4-5 JMM 与并发安全发布

## 原文

- **PDF 原件**：**`book/` 下没有** —— 若要净化需先补原件。

## 关联

- [[教材-MOC|教材索引]]
- [[Java并发编程的艺术|《Java 并发编程的艺术》]] —— 主要交叉对象
- [[并发编程-MOC|并发编程 MOC]]