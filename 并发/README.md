这是一系列Java并发笔记。在CSAPP里，我已经理解了为什么需要并发——CPU的流水线、存储层次、上下文切换，这些底层机制决定了“并行”不是一种选择，而是一种物理必然。但是Java在JVM之上是怎么做的。这就是这份笔记的起点。Java并发，这是JVM之上、业务逻辑之下的关键中间层。如果说JVM的伟大在于“一次编译，到处运行”，那么Java并发的伟大就在于“让正确地协调多线程变得可能”。它把硬件层面的内存屏障、原子指令、缓存一致性协议，封装成了JMM的happens-before规则、volatile的可见性语义等——让我们这些Java程序员可以不用直接写内存栅栏，也能写出线程安全的代码。要自底向上学习，JVM是Java程序员必要的过程，而Java并发是这个过程中最关键的一环。

学习Java并发，能知道Java的业务方面的很多硬核问题：比如ConcurrentHashMap为什么不用全局锁也能保证线程安全，比如线程池的拒绝策略在什么场景下会导致生产事故，比如volatile为什么能保证可见性但不能保证原子性。但更多的是能知道它的性能边界在哪——synchronized锁升级的代价、CAS自旋的CPU开销、AQS队列的调度逻辑——以及企业级项目中如何基于这些边界做出正确的trade-off，最终明白它为什么永远达不到C++手写无锁结构的极限性能。

系列笔记大部分取自两本著作：《Java并发编程实战》（JCiP）和《Java并发编程的艺术》。这两本书，一本讲设计哲学，一本讲底层实现。它们和《深入理解Java虚拟机》中的”高效并发”章节一起，构成了从硬件到JVM再到Java API的完整并发知识链条。

---

## 📂 笔记目录

> 按学习阶段递进：基础 → 锁与同步 → 线程池 → 性能优化。

### 一、并发基础（1 阶段）

- [并发基础：线程、内存模型与可见性](./1阶段-并发基础.md)

### 二、锁与同步核心（2 阶段）

- [锁与同步核心机制](./2阶段-锁与同步核心.md)
- [AQS 队列同步器（全量源码+流程图）](./2阶段-Java并发编程艺术·AQS队列同步器 全量源码+流程图+实战手册.md)
- [AQS 队列同步器（思想+极简版）](./2阶段-Java并发编程艺术·AQS队列同步器（思想+极简源码流程版）.md)
- [ConcurrentHashMap 深度解析](./2阶段-Java并发容器和框架：ConcurrentHashMap.md)
- [JDK1.7 HashMap 死循环根源](./2阶段-JDK1.7 HashMap 死循环的唯一根源：.md)

### 三、任务执行与线程池（3 阶段）

- [任务执行与线程池源码级整合](./3阶段-任务执行与线程池 源码级深度整合文档.md)
- [JUC Executor 框架与 FutureTask](./3阶段-JUC Executor 框架与 FutureTask 完整详解.md)

### 四、性能优化与全体系闭环（4 阶段）

- [性能优化与全体系闭环](./4阶段- 性能优化与全体系闭环.md)
- [并发性能优化全景图](./4阶段-并发性能优化全景图.md)
- [AtomicReferenceFieldUpdater 详解](./4阶段-AtomicReferenceFieldUpdater 详解.md)
- [AtomicStampedReference & AtomicMarkableReference](./4阶段-AtomicStampedReference & AtomicMarkableReference 文档.md)
- [并发程序测试方法](./4阶段-并发程序测试-简单.md)
