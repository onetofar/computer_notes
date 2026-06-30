---
title: "4-5 JMM与并发安全发布"
category: 并发编程
tags:
  - conc/perf
  - concurrency
difficulty: 深入
source: "JCiP / CSAPP"
link: ["[[1-1 并发基础理论笔记|并发基础理论]]","[[1-2 DCL专题与内存模型笔记|DCL与内存模型]]"]
---
# 4-5 JMM与并发安全发布

## 🎯 三大核心维度定义

### 核心定位
JMM 屏蔽硬件内存差异，定义**可见性、有序性、原子性**规则；安全发布保证对象**完全初始化后**才被线程访问，避免半初始化对象逸出。

### 核心行为语义
1. 重排序：编译器/CPU 优化指令顺序，破坏多线程内存一致性。
2. Happens-Before：JMM 核心偏序规则，保证操作间的可见性。
3. 初始化安全性：`final` 域保证构造函数完成后对所有线程可见。

### 核心设计思想
通过 `volatile/final/锁` 建立 Happens-Before 规则，禁止有害重排序，实现安全发布与可见性。

---

## 1. 基础层（书本定义）

📘 JCiP 16.1 原文：JMM 为所有操作定义**偏序关系（Happens-Before）**，同步操作、volatile 读写满足**全序关系**，全局唯一有序。

📘 JCiP 16.2 原文：重排序分为**编译器重排序、指令重排序、内存重排序**，JMM 通过内存屏障禁止有害重排序。

📘 JCiP 16.3 原文：**安全发布 4 种方式**：静态初始化、volatile 引用、锁、AtomicReference；**初始化安全性**：正确构造对象的 final 域无需同步即可全局可见。

---

## 2. 实现层（JVM 底层⛰️）

⛰️ JMM 通过 4 种内存屏障保证有序性：LoadLoad、LoadStore、StoreStore、StoreLoad。

⛰️ volatile 写后插入 **StoreLoad 屏障**，禁止写→读重排序，保证可见性。

⛰️ final 域：JVM 禁止将 final 域写入重排序到构造函数之外，保证初始化安全。

⛰️ Happens-Before 规则直接映射 JVM 内存屏障，是可见性的语法层抽象。

---

## 3. 机制层（CSAPP 硬件原理🌏）

🌏 重排序根源：CPU **乱序执行**与**缓存延迟写入**，提升指令级并行。

🌏 内存屏障：CPU 指令（mfence/lfence）强制缓存刷新，保证顺序执行。

🌏 全序关系：锁/volatile 操作在硬件层面全局有序，任意两个操作可排先后。

---

## 4. 核心代码示例

```java
// 📘 JCiP Listing 16.4 安全发布（volatile禁止重排序）
public class SafePublisher {
    private volatile Holder holder;

    public void initialize() {
        holder = new Holder(42); // volatile保证对象完全初始化后发布
    }

    public static class Holder {
        private final int value;
        public Holder(int value) {
            this.value = value;
        }
    }
}

// 📘 JCiP 初始化安全性：final域无需同步
public class SafeFinalObject {
    private final Map<String, String> states;
    public SafeFinalObject() {
        states = new HashMap<>();
        states.put("CA", "California");
    }
    public String getState(String key) {
        return states.get(key); // 无需加锁，安全读取
    }
}
```

---

## 5. 问题排查与最佳实践

### 问题排查
1. **半初始化对象**：未安全发布，线程读到未初始化字段 → 用 volatile/锁发布
2. **final 域失效**：构造函数中 this 逸出 → 禁止构造函数中发布对象引用
3. **可见性故障**：普通变量多线程不一致 → 加 volatile 或锁

### 最佳实践
1. 不可变对象用 final 修饰，天然安全发布，无需同步
2. 可变对象安全发布用 volatile、锁或 AtomicReference
3. 禁止在构造函数中启动线程、注册监听器，防止 this 逸出

---

## 🎯 全体系闭环总结

### 底层联动深度解析

| 核心问题 | CSAPP 硬件层🌏           | JVM 实现层⛰️                      | Java 应用层                 |
| -------- | --------------------- | --------------------------------- | -------------------------- |
| 可见性   | MESI 缓存一致性        | volatile 内存屏障、final 初始化屏障 | volatile、锁、原子类       |
| 有序性   | CPU 乱序执行           | JMM 内存屏障、Happens-Before       | 禁止指令重排序、安全发布   |
| 原子性   | CAS 原子指令           | Unsafe、synchronized 管程          | 原子变量、锁、非阻塞算法   |
| 锁竞争   | 伪共享、上下文切换    | 锁升级、锁消除、锁粗化            | 缩小锁范围、分片锁、无锁   |
| 性能瓶颈 | Amdahl 定律、CPU 利用率 | JMH 基准测试、GC 联动             | 异步化、非阻塞 IO、资源隔离 |

### 全链路工具链
1. 死锁：jstack、Arthas、VisualVM
2. 锁竞争：Arthas profiler、JFR、perf
3. 伪共享：perf cachestat、JOL
4. 可见性：JFR、AsyncProfiler
5. GC 联动：GC 日志、MAT、Arthas

### 调优黄金法则
1. **量化先行**：所有调优基于 JMH/压测数据
2. **从底到上**：先硬件→再 JVM→最后应用
3. **最小共享**：无状态 > ThreadLocal > 共享变量
4. **全异步化**：阻塞 IO 全部转为异步非阻塞
5. **资源隔离**：线程池、锁、缓存独立隔离

### 核心口诀
> 死锁靠破环，活锁靠退避；
> 性能看串行，锁优缩粒度；
> 无锁防 ABA，版本是利器；
> JMM 靠屏障，发布用 final。

### 认知闭环
1. **认知闭环**：硬件（Amdahl/伪共享）→ JVM（锁优化/内存屏障）→ 应用（无锁/同步/异步）
2. **技术闭环**：JCiP 理论 + 并发艺术实战 + 底层原理 + 代码落地 + 故障排查
3. **工程闭环**：设计无锁/异步 → 开发 JMH 验证 → 测试压测 → 运维监控告警

---
