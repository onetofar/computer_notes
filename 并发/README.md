这是一系列Java并发笔记。在CSAPP里，我已经理解了为什么需要并发——CPU的流水线、存储层次、上下文切换，这些底层机制决定了“并行”不是一种选择，而是一种物理必然。但是Java在JVM之上是怎么做的。这就是这份笔记的起点。Java并发，这是JVM之上、业务逻辑之下的关键中间层。如果说JVM的伟大在于“一次编译，到处运行”，那么Java并发的伟大就在于“让正确地协调多线程变得可能”。它把硬件层面的内存屏障、原子指令、缓存一致性协议，封装成了JMM的happens-before规则、volatile的可见性语义等——让我们这些Java程序员可以不用直接写内存栅栏，也能写出线程安全的代码。要自底向上学习，JVM是Java程序员必要的过程，而Java并发是这个过程中最关键的一环。

学习Java并发，能知道Java的业务方面的很多硬核问题：比如ConcurrentHashMap为什么不用全局锁也能保证线程安全，比如线程池的拒绝策略在什么场景下会导致生产事故，比如volatile为什么能保证可见性但不能保证原子性。但更多的是能知道它的性能边界在哪——synchronized锁升级的代价、CAS自旋的CPU开销、AQS队列的调度逻辑——以及企业级项目中如何基于这些边界做出正确的trade-off，最终明白它为什么永远达不到C++手写无锁结构的极限性能。

系列笔记大部分取自两本著作：《Java并发编程实战》（JCiP）和《Java并发编程的艺术》。这两本书，一本讲设计哲学，一本讲底层实现。它们和《深入理解Java虚拟机》中的”高效并发”章节一起，构成了从硬件到JVM再到Java API的完整并发知识链条。

---

## 📂 笔记目录

> 按学习阶段递进：基础 → 锁与同步 → 线程池 → 性能优化。

### 一、并发基础（1 阶段）

**理论**
- [并发基础理论](./1-1%20并发基础理论笔记.md) — 并发本质、JMM、happens-before、线程安全、volatile/final、对象共享与组合
- [DCL专题与内存模型](./1-2%20DCL专题与内存模型笔记.md) — DCL五种方案对比、多CPU内存模型、MESI缓存一致性协议

**总结与贯通**
- [全链路打通与易错点](./1-a%20全链路打通与易错点.md) — 三层联动映射表、10大终极易错点、知识闭环

### 二、锁与同步核心（2 阶段）

**理论**
- [锁体系核心理论](./2-1%20锁体系核心理论笔记.md) — synchronized底层、Lock/AQS/ReentrantLock/读写锁
- [并发工具与活跃性问题](./2-2%20并发工具与活跃性问题笔记.md) — 阻塞队列、死锁/活锁/饥饿、同步工具类

**总结与贯通**
- [全链路打通与易错点](./2-a%20全链路打通与易错点.md) — 三层联动映射表、10大易错点、知识闭环

**专题深入**
- [AQS 核心理论](./2-4%20AQS核心理论笔记.md) — 核心组件、数据结构、内存语义、面试题
- [AQS 独占模式源码分析](./2-c%20AQS独占模式源码分析笔记.md) — acquire/release/可中断/超时源码全流程
- [AQS 共享模式与实战](./2-d%20AQS共享模式与实战笔记.md) — 共享模式源码、工具方法、ReentrantLock/Semaphore/CountDownLatch拆解、TwinsLock实战
- [ConcurrentHashMap 理论+架构](./2-3%20ConcurrentHashMap理论+架构笔记.md) — 为什么需要CHM、JDK1.7底层架构、初始化、哈希定位
- [CHM 源码分析笔记](./2-b%20ConcurrentHashMap源码分析笔记.md) — get/put/size/rehash 完整源码+流程图
- [JDK1.7 HashMap 死循环根源](./2-5_JDK1.7_HashMap_死循环根源分析.md)

### 三、任务执行与线程池（3 阶段）

**理论**
- [线程池核心理论与执行流程](./3-1%20线程池核心理论与执行流程.md) — Executor框架、ThreadPoolExecutor核心(ctl/execute/addWorker)
- [Worker与FutureTask笔记](./3-2%20Worker与FutureTask笔记.md) — Worker生命周期、线程复用、FutureTask状态机
- [线程池关闭与调优](./3-3%20线程池关闭与调优.md) — 优雅关闭、核心参数配置、拒绝策略、线程饥饿
- [Executor框架与四大线程池笔记](./3-4%20Executor框架与四大线程池笔记.md) — Fixed/Single/Cached/Scheduled四大标准线程池

**实验与源码分析**
- [CompletionService与定时线程池](./3-a%20CompletionService与定时线程池.md) — CompletionService、ScheduledThreadPoolExecutor核心设计
- [监控排查与最佳实践](./3-b%20监控排查与最佳实践.md) — 扩展钩子、异常处理、JVM/CSAPP底层联动、易错点
- [FutureTask源码分析笔记](./3-c%20FutureTask源码分析笔记.md) — FutureTask整体执行流程分析

### 四、性能优化与全体系闭环（4 阶段）

**核心知识体系（JCiP 第10-16章拆分）**
- [活跃性问题与死锁治理](./4-1%20活跃性问题与死锁治理.md) — 死锁四大条件、避免策略、饥饿与活锁
- [性能与可伸缩性优化](./4-2%20性能与可伸缩性优化.md) — Amdahl定律、锁竞争优化、JMH基准测试
- [AQS同步器与状态依赖管理](./4-3%20AQS同步器与状态依赖管理.md) — 模板方法模式、条件队列、状态依赖
- [原子变量与非阻塞算法](./4-4%20原子变量与非阻塞算法.md) — CAS指令、ABA问题、无锁数据结构
- [JMM与并发安全发布](./4-5%20JMM与并发安全发布.md) — 重排序、Happens-Before、安全发布

**专题深入**
- [AtomicReferenceFieldUpdater 详解](./4-6%20AtomicReferenceFieldUpdater详解.md) — 原子字段更新器深度解析
- [AtomicStampedReference 与 AtomicMarkableReference](./4-7%20AtomicStampedReference与AtomicMarkableReference.md) — ABA问题解决与版本号机制
- [并发性能优化全景图](./4-8%20并发性能优化全景图.md) — 三大瓶颈与三大优化方向逻辑闭环
- [并发程序测试方法](./4-9%20并发程序测试方法.md) — JCiP并发测试基准与实践
