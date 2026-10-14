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
这是一系列Java并发笔记。在CSAPP里，我已经理解了为什么需要并发——CPU的流水线、存储层次、上下文切换，这些底层机制决定了“并行”不是一种选择，而是一种物理必然。但是Java在JVM之上是怎么做的。这就是这份笔记的起点。Java并发，这是JVM之上、业务逻辑之下的关键中间层。如果说JVM的伟大在于“一次编译，到处运行”，那么Java并发的伟大就在于“让正确地协调多线程变得可能”。它把硬件层面的内存屏障、原子指令、缓存一致性协议，封装成了JMM的happens-before规则、volatile的可见性语义等——让我们这些Java程序员可以不用直接写内存栅栏，也能写出线程安全的代码。要自底向上学习，JVM是Java程序员必要的过程，而Java并发是这个过程中最关键的一环。

学习Java并发，能知道Java的业务方面的很多硬核问题：比如ConcurrentHashMap为什么不用全局锁也能保证线程安全，比如线程池的拒绝策略在什么场景下会导致生产事故，比如volatile为什么能保证可见性但不能保证原子性。但更多的是能知道它的性能边界在哪——synchronized锁升级的代价、CAS自旋的CPU开销、AQS队列的调度逻辑——以及企业级项目中如何基于这些边界做出正确的权衡，最终明白它为什么永远达不到C++手写无锁结构的极限性能。

系列笔记大部分取自两本著作：《Java并发编程实战》（JCiP）和《Java并发编程的艺术》。这两本书，一本讲设计哲学，一本讲底层实现。它们和《深入理解Java虚拟机》中的“高效并发”章节一起，构成了从硬件到JVM再到Java API的完整并发知识链条。

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