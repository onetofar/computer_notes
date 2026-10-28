---
title: "并发编程 领域知识地图"
category: MOC
tags:
  - 知识索引
  - 并发编程
type: 领域MOC
skills:
  - obsidian-markdown
---
并发编程领域的知识地图。**领域导语见 [[markdown/并发/README|并发笔记导语]]**；下面是完整目录。

# 并发编程 · 领域MOC

## 前置依赖领域
- [[操作系统-MOC|操作系统（CSAPP）]]
- [[JVM-MOC|JVM虚拟机]]

## 一、并发基础层
- [[1-1 并发基础理论笔记|并发本质、JMM、happens-before、线程安全]]
- [[1-2 DCL专题与内存模型笔记|DCL与多CPU内存模型]]
- [[1-a 全链路打通与易错点|并发基础三层联动与易错点]]

## 二、锁与同步核心
- [[2-1 锁体系核心理论笔记|synchronized底层、Lock/AQS体系全链路]]
- [[2-2 并发工具与活跃性问题笔记|阻塞队列、死锁/活锁/饥饿、同步工具]]
- [[2-4 AQS核心理论笔记|AQS核心组件、数据结构、内存语义]]
- [[2-c AQS独占模式源码分析笔记|AQS独占模式acquire/release全流程]]
- [[2-d AQS共享模式与实战笔记|AQS共享模式、ReentrantLock/Semaphore拆解]]
- [[2-a 全链路打通与易错点|锁体系三层联动与易错点]]

## 三、并发容器
- [[2-3 ConcurrentHashMap理论+架构笔记|ConcurrentHashMap架构与设计原理]]
- [[2-b ConcurrentHashMap源码分析笔记|CHM get/put/size/rehash 完整源码]]
- [[2-5_JDK1.7_HashMap_死循环根源分析|JDK1.7 HashMap死循环根源]]

## 四、线程池与任务执行
- [[3-1 线程池核心理论与执行流程|ThreadPoolExecutor核心理论与执行流程]]
- [[3-2 Worker与FutureTask笔记|Worker生命周期与FutureTask状态机]]
- [[3-3 线程池关闭与调优|优雅关闭、参数配置、拒绝策略]]
- [[3-4 Executor框架与四大线程池笔记|四大标准线程池详解]]
- [[3-a CompletionService与定时线程池|CompletionService与ScheduledThreadPoolExecutor]]
- [[3-b 监控排查与最佳实践|异常处理、JVM/CSAPP联动、易错点]]
- [[3-c FutureTask源码分析笔记|FutureTask完整执行流程分析]]

## 五、原子操作与无锁编程
- [[4-4 原子变量与非阻塞算法|CAS指令、ABA问题、无锁数据结构]]
- [[4-6 AtomicReferenceFieldUpdater详解|原子字段更新器与ConcurrentLinkedQueue]]
- [[4-7 AtomicStampedReference与AtomicMarkableReference|ABA问题解决方案：版本号与标记位]]

## 六、性能优化
- [[4-1 活跃性问题与死锁治理|死锁四大条件与避免策略]]
- [[4-2 性能与可伸缩性优化|Amdahl定律与锁竞争优化]]
- [[4-3 AQS同步器与状态依赖管理|模板方法模式与条件队列]]
- [[4-5 JMM与并发安全发布|重排序、Happens-Before与安全发布]]
- [[4-8 并发性能优化全景图|三大瓶颈与三大优化方向逻辑闭环]]
- [[4-9 并发程序测试方法|JCiP并发测试基准与实践]]

## 七、参考
- [[Java并发学习系列文档 格式规范&完整记忆手册|并发学习系列格式规范]]
- [[教材-MOC|教材索引]] — 这些笔记来自哪本书的哪一节