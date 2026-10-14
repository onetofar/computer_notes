---
title: "Java并发编程的艺术"
category: 教材
type: 教材节点
book: Java并发编程的艺术
author: "方腾飞 / 冯玉才"
role: 已沉淀
tags:
  - 教材精读
  - 并发编程
created: 2026-10-07
skills:
  - obsidian-markdown
---

# 《Java 并发编程的艺术》· 书名节点

> **这本讲什么**：以"并发编程的原理 + JDK 源码"双线讲并发 ——
> 并发基础、锁、AQS、ConcurrentHashMap、线程池、Executor、原子类。
> 中文原创，翻译腔问题少，但仍有大量"前面结论、后面再讲一遍"的车轱辘话。
>
> **注意**：这 12 篇里有 7 篇 frontmatter 写的是 `JCiP / Java并发编程的艺术`
> （多来源）。本节点只收录**以本书为主源**的。
>
> 上级：[[教材-MOC|教材索引]]

## 对应位置

| 章 | 笔记 |
|---|---|
| ConcurrentHashMap 原理 | [[2-3 ConcurrentHashMap理论+架构笔记]] |
| AQS 原理 | [[2-4 AQS核心理论笔记]] |
| ConcurrentHashMap 源码 | [[2-b ConcurrentHashMap源码分析笔记]] |
| AQS 独占模式 | [[2-c AQS独占模式源码分析笔记]] |
| AQS 共享模式 | [[2-d AQS共享模式与实战笔记]] |
| 线程池核心 | [[3-1 线程池核心理论与执行流程]] · [[3-2 Worker与FutureTask笔记]] · [[3-3 线程池关闭与调优]] |
| Executor 框架 | [[3-4 Executor框架与四大线程池笔记]] |
| CompletionService / 定时线程池 | [[3-a CompletionService与定时线程池]] |
| 监控与排障 | [[3-b 监控排查与最佳实践]] |
| FutureTask 源码 | [[3-c FutureTask源码分析笔记]] |

**12 篇 · 精读 0 篇。**

## 原文

- **PDF 原件**：**`book/` 下没有**（清单里未列出本书原件）——
  若要净化需先补原件或用现成 md 稿。

## 关联

- [[教材-MOC|教材索引]]
- [[JCiP-Java并发编程实战|《Java 并发编程实战》（JCiP）]] —— 多处交叉引用
- [[CSAPP-深入理解计算机系统|CSAPP]] —— 并发章节常与 CSAPP 第 12 章对照
- [[并发编程-MOC|并发编程 MOC]]