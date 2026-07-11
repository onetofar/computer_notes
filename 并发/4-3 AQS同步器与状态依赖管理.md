---
title: "4-3 AQS同步器与状态依赖管理"
category: 并发编程
tags:
  - conc/perf
  - conc
difficulty: 深入
source: "JCiP / CSAPP"
link: ["[[2-4 AQS核心理论笔记|AQS核心理论]]","[[2-c AQS独占模式源码分析笔记|AQS独占模式源码]]","[[2-d AQS共享模式与实战笔记|AQS共享模式与实战]]"]
---
# 4-3 AQS同步器与状态依赖管理

## 🎯 三大核心维度定义

### 核心定位
AQS（AbstractQueuedSynchronizer）是 JUC 同步器（ReentrantLock、CountDownLatch、Semaphore）的底层骨架，解决「状态依赖性」问题（线程需等待某个状态满足才能执行），是 Java 并发的核心基础设施。

### 核心行为语义
1. AQS 核心是「状态+队列」：通过 `volatile int state` 维护同步状态，通过 CLH 双向队列（FIFO）管理等待线程。
2. 状态依赖管理的核心是条件队列：每个 ConditionObject 对应一个条件队列，线程通过 `await()` 进入条件队列，`signal()` 唤醒到同步队列。
3. AQS 支持独占模式（ReentrantLock）和共享模式（Semaphore/CountDownLatch）。

### 核心设计思想
1. 模板方法模式：AQS 定义同步器的骨架（获取/释放锁的流程），子类实现 `tryAcquire`/`tryRelease` 等方法定制同步逻辑。
2. 非阻塞设计：AQS 的队列操作基于 CAS，避免内核态锁，提升并发性能。
3. 状态可见性：`state` 的 volatile 修饰保证多线程可见性，底层依赖 JVM 内存屏障⛰️。

---

## 1. 基础层（书本定义）

📘 JCiP 14.1 原文：**状态依赖性操作**必须在前提条件满足时才能执行，若条件不满足，线程必须等待直至条件成立；内置锁的 `wait/notify` 是最原始的状态依赖实现，但只能关联一个条件队列，灵活性差。

📘 JCiP 14.2 原文：**条件队列**使线程能够等待某个特定条件为真，每个显式锁（`ReentrantLock`）可以关联**多个条件队列**，这是 `Condition` 优于 `wait/notify` 的核心优势，可彻底避免"惊群效应"。

📘 JCiP 14.4 原文：AQS 采用**模板方法模式**，将同步器的公共逻辑（队列管理、线程阻塞/唤醒）封装在抽象类中，子类仅需实现**尝试获取/释放同步状态**的核心逻辑。

📘 JCiP 14.5 原文：AQS 支持**独占式**与**共享式**两种同步模式：
- 独占模式：同一时刻仅一个线程持有同步状态（`ReentrantLock`）
- 共享模式：多个线程可同时持有同步状态（`CountDownLatch`、`Semaphore`）

📘 JCiP 14.6 原文：AQS 提供**可中断、可超时、非中断**三种获取同步状态的方式，全面覆盖不同场景的状态依赖需求。

---

## 2. 实现层（JVM 底层⛰️）

⛰️ AQS 核心数据结构：**volatile int state**（同步状态）+ **CLH 变体双向同步队列**（等待线程管理），所有队列操作基于 `Unsafe` 的 CAS 指令原子完成。

⛰️ `state` 变量由 `volatile` 修饰，JVM 在其写操作后插入 **StoreLoad 内存屏障**，读操作前插入 **LoadLoad/LoadStore 屏障**，保证多线程间的可见性与有序性。

⛰️ 线程阻塞/唤醒基于 `LockSupport.park()/unpark()` 实现，直接映射操作系统内核态线程调度，无需依赖锁上下文，比 `Object.wait/notify` 更轻量。

⛰️ `ConditionObject`（条件队列）是 AQS 的内部类，每个条件队列独立维护等待线程，`await()` 时将线程移入条件队列并释放锁，`signal()` 时将节点移回同步队列等待唤醒。

⛰️ 可重入特性由 AQS 的 `exclusiveOwnerThread` 实现，记录持有锁的线程，同一线程重复获取时直接累加 `state` 值，无需再次竞争。

---

## 3. 机制层（CSAPP 硬件原理🌏）

🌏 AQS 的 CAS 操作依赖 CPU **LOCK CMPXCHG 原子指令**，硬件层面锁定缓存行（MESI 协议），保证指令执行的原子性，避免多核并发冲突。

🌏 CLH 同步队列采用 FIFO 规则，线程按等待顺序排队，减少 CPU 缓存行频繁失效，提升硬件缓存命中率。

🌏 `LockSupport.park()` 会将线程置于 **TIMED_WAITING 状态**，释放 CPU 核心资源，降低硬件功耗与上下文切换开销。

🌏 条件队列的线程切换仅涉及**内存指针修改**，无内核态互斥锁竞争，硬件级并发效率远高于内置锁的等待队列。

---

## 4. 核心代码示例

```java
// 📘 JCiP Listing 14.10 基于AQS实现独占锁（原文标准示例）
public class SimpleMutex implements Lock {
    // 自定义AQS同步器
    private static class Sync extends AbstractQueuedSynchronizer {
        @Override
        protected boolean tryAcquire(int acquires) {
            assert acquires == 1;
            if (compareAndSetState(0, 1)) {
                setExclusiveOwnerThread(Thread.currentThread());
                return true;
            }
            return false;
        }

        @Override
        protected boolean tryRelease(int releases) {
            assert releases == 1;
            if (getState() == 0) {
                throw new IllegalMonitorStateException();
            }
            setExclusiveOwnerThread(null);
            setState(0);
            return true;
        }

        Condition newCondition() {
            return new ConditionObject();
        }
    }

    private final Sync sync = new Sync();

    @Override
    public void lock() { sync.acquire(1); }

    @Override
    public void lockInterruptibly() throws InterruptedException {
        sync.acquireInterruptibly(1);
    }

    @Override
    public boolean tryLock() { return sync.tryAcquire(1); }

    @Override
    public boolean tryLock(long timeout, TimeUnit unit) throws InterruptedException {
        return sync.tryAcquireNanos(1, unit.toNanos(timeout));
    }

    @Override
    public void unlock() { sync.release(1); }

    @Override
    public Condition newCondition() { return sync.newCondition(); }
}

// 📘 JCiP Listing 14.5 状态依赖：有界缓冲区（条件队列标准用法）
public class BoundedBuffer<E> {
    private final Lock lock = new ReentrantLock();
    private final Condition notFull = lock.newCondition();
    private final Condition notEmpty = lock.newCondition();

    private final E[] items;
    private int putPtr, takePtr, count;

    public BoundedBuffer(int capacity) {
        items = (E[]) new Object[capacity];
    }

    public void put(E e) throws InterruptedException {
        lock.lock();
        try {
            while (count == items.length) {
                notFull.await();
            }
            items[putPtr] = e;
            if (++putPtr == items.length) putPtr = 0;
            count++;
            notEmpty.signal();
        } finally {
            lock.unlock();
        }
    }

    public E take() throws InterruptedException {
        lock.lock();
        try {
            while (count == 0) {
                notEmpty.await();
            }
            E e = items[takePtr];
            if (++takePtr == items.length) takePtr = 0;
            count--;
            notFull.signal();
            return e;
        } finally {
            lock.unlock();
        }
    }
}
```

---

## 5. 问题排查与最佳实践

### 常见故障
1. **死锁**：多线程按不同顺序获取多个 AQS 锁，形成循环等待 → 统一锁获取顺序
2. **虚假唤醒**：`await()` 未在循环中检查条件 → 强制使用 `while(条件不满足)` 包裹等待
3. **线程泄露**：`signal()` 误用为 `signalAll()`，导致无效唤醒 → 单线程用 `signal()`，多线程用 `signalAll()`
4. **中断丢失**：忽略 `InterruptedException`，导致线程无法正常退出 → 捕获中断并恢复中断状态

### 最佳实践
1. 状态依赖检查**必须用 while 循环**，禁止用 if 判断，抵御虚假唤醒
2. 单一等待条件用 `signal()`，多条件等待用 `signalAll()`，减少无效唤醒
3. 优先使用 `lockInterruptibly()` 支持中断，提升程序响应性
4. 显式锁（AQS）用完必须 `unlock()`，放入 finally 块防止异常泄露锁
5. 高并发场景优先使用 AQS 实现的同步器，而非内置 `synchronized`

---
