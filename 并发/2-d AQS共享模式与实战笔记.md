---
title: 2-d AQS共享模式与实战笔记
category: 并发编程
tags:
  - conc/lock
  - concurrency
difficulty: 深入
source: Java并发编程的艺术
link: ["[[2-4 AQS核心理论笔记|AQS核心理论]]","[[2-c AQS独占模式源码分析笔记|AQS独占模式源码分析]]","[[2-1 锁体系核心理论笔记|锁体系核心理论]]","[[2-2 并发工具与活跃性问题笔记|并发工具与活跃性问题]]"]
---
## 5. 共享式同步状态·获取与释放
### 5.1 共享获取 `acquireShared(int arg)`（书本核心）
#### 源码+逐行注释
```java
public final void acquireShared(int arg) {
    // 书本核心：tryAcquireShared返回值规则
    // >0：获取成功，且有剩余资源；=0：获取成功，无剩余；<0：获取失败
    if (tryAcquireShared(arg) < 0)
        doAcquireShared(arg); // 失败→入队自旋阻塞 ⛰️🌏
}

// 子类重写钩子：共享模式尝试获取资源
protected int tryAcquireShared(int arg) {
    throw new UnsupportedOperationException();
}

// 共享模式自旋阻塞（书本补充核心）
private void doAcquireShared(int arg) {
    final Node node = addWaiter(Node.SHARED); // 创建共享节点入队 🌏
    boolean failed = true;
    try {
        boolean interrupted = false; // ⛰️
        for (;;) {
            final Node p = node.predecessor(); // ⛰️
            if (p == head) {
                int r = tryAcquireShared(arg); // 🌏
                if (r >= 0) {
                    // 书本重点：设置头节点+传播唤醒（共享模式核心）⛰️🌏
                    setHeadAndPropagate(node, r);
                    p.next = null; // 旧头节点GC ⛰️
                    if (interrupted)
                        selfInterrupt(); // ⛰️
                    failed = false;
                    return;
                }
            }
            if (shouldParkAfterFailedAcquire(p, node) && parkAndCheckInterrupt())
                interrupted = true; // ⛰️
        }
    } finally {
        if (failed)
            cancelAcquire(node); // ⛰️
    }
}

// 共享模式：设置头节点+传播唤醒（书本核心）
private void setHeadAndPropagate(Node node, int propagate) {
    Node h = head; // ⛰️
    setHead(node); // 设置新头节点 ⛰️
    // 书本规则：有剩余资源 或 头节点状态异常 → 继续唤醒后继（传播）🌏
    if (propagate > 0 || h == null || h.waitStatus < 0 || (h = head) == null || h.waitStatus < 0) {
        Node s = node.next; // ⛰️
        if (s == null || s.isShared())
            doReleaseShared(); // 传播唤醒后继节点 ⛰️🌏
    }
}
```
#### 书本核心解读
- 共享模式的核心是"传播唤醒" 🌏：获取资源成功后，若还有剩余资源，继续唤醒后继节点（如Semaphore允许多个线程同时获取许可）；
- `tryAcquireShared`返回值是共享模式的关键 🌏：通过返回值判断是否有剩余资源，决定是否传播唤醒。

### 5.2 共享释放 `releaseShared(int arg)`（书本核心）
#### 源码+逐行注释
```java
public final boolean releaseShared(int arg) {
    // 子类自定义释放共享资源 🌏
    if (tryReleaseShared(arg)) {
        doReleaseShared(); // 共享模式：传播唤醒（核心）⛰️🌏
        return true;
    }
    return false;
}

// 子类重写钩子：释放共享资源
protected boolean tryReleaseShared(int arg) {
    throw new UnsupportedOperationException();
}

// 共享模式传播唤醒（书本补充核心）
private void doReleaseShared() {
    for (;;) { // 自旋保证唤醒完成 🌏
        Node h = head; // ⛰️
        if (h != null && h != tail) {
            int ws = h.waitStatus; // ⛰️
            if (ws == Node.SIGNAL) {
                // CAS重置头节点状态，避免重复唤醒 🌏
                if (!compareAndSetWaitStatus(h, Node.SIGNAL, 0))
                    continue;
                unparkSuccessor(h); // 唤醒后继 ⛰️🌏
            }
            // 书本优化：头节点状态为0时，设置为PROPAGATE（保证传播）⛰️
            else if (ws == 0 && !compareAndSetWaitStatus(h, 0, Node.PROPAGATE))
                continue;
        }
        if (h == head) // 头节点未变化→退出自旋 ⛰️
            break;
    }
}
```
#### 书本核心解读
- 共享释放必须保证原子性 🌏：如Semaphore释放许可时，需CAS递增许可数；
- 传播唤醒 🌏：释放资源后，需批量唤醒等待的线程（而非仅唤醒一个），保证共享资源的充分利用。

### 5.3 独占 VS 共享 核心对比（补充书本细节）
| 维度         | 独占模式                                      | 共享模式                                                  | 书本核心说明                                   |
| ------------ | --------------------------------------------- | --------------------------------------------------------- | ---------------------------------------------- |
| state含义    | 0=空闲，N=重入次数 ⛰️                          | 剩余可用资源数量 ⛰️                                        | 独占是"锁的持有状态"，共享是"资源的剩余数量"   |
| 并发线程     | 同一时刻仅1个 ⛰️                               | 同一时刻N个（N=state）⛰️                                   | 独占是"互斥"，共享是"允许多线程并发"           |
| 唤醒机制     | 释放时唤醒1个后继节点 ⛰️                       | 释放时批量唤醒（传播机制）🌏                               | 共享的"传播唤醒"是核心区别，保证资源被充分利用 |
| 钩子返回值   | boolean（成功/失败）⛰️                         | int（>0=有剩余/=0=无剩余/<0=失败）🌏                       | 返回值决定是否需要传播唤醒                     |
| 实现类       | ReentrantLock、ReentrantReadWriteLock（写锁） | Semaphore、CountDownLatch、ReentrantReadWriteLock（读锁） | 读锁是共享、写锁是独占，体现AQS的复用性        |
| 队列节点标记 | nextWaiter=EXCLUSIVE（null）⛰️                 | nextWaiter=SHARED 🌏                                       | 标记节点类型，避免独占/共享唤醒混淆            |

---

## 6. AQS全局核心底层工具方法（书本全量补充）
### 6.1 `shouldParkAfterFailedAcquire`（判断是否阻塞）
```java
private static boolean shouldParkAfterFailedAcquire(Node pred, Node node) {
    int ws = pred.waitStatus; // ⛰️
    if (ws == Node.SIGNAL)
        // 前驱状态为SIGNAL：当前节点可安全阻塞（前驱释放后会唤醒）⛰️
        return true;
    if (ws > 0) {
        // 前驱被取消：向前遍历，跳过所有取消节点（书本：清理无效节点）⛰️
        do {
            node.prev = pred = pred.prev; // ⛰️
        } while (pred.waitStatus > 0);
        pred.next = node; // ⛰️
    } else {
        // 前驱状态为0/PROPAGATE：CAS设置为SIGNAL（保证后续能被唤醒）🌏
        compareAndSetWaitStatus(pred, ws, Node.SIGNAL);
    }
    return false; // 暂不阻塞，继续自旋
}
```
**书本解读**：核心是"保证前驱为SIGNAL状态" ⛰️，避免线程阻塞后无法被唤醒。

### 6.2 `unparkSuccessor`（唤醒后继节点）
```java
private void unparkSuccessor(Node node) {
    int ws = node.waitStatus; // ⛰️
    if (ws < 0)
        // CAS重置节点状态为0（清理SIGNAL/PROPAGATE）🌏
        compareAndSetWaitStatus(node, ws, 0);

    Node s = node.next; // ⛰️
    if (s == null || s.waitStatus > 0) {
        s = null;
        // 书本重点：从尾节点反向遍历，找到第一个未被取消的后继（避免节点丢失）⛰️
        for (Node t = tail; t != null && t != node; t = t.prev) // ⛰️
            if (t.waitStatus <= 0)
                s = t;
    }
    if (s != null)
        LockSupport.unpark(s.thread); // 唤醒线程（内核态→用户态）⛰️🌏
}
```
**书本解读**：反向遍历 ⛰️是为了避免"后继节点被取消"导致的唤醒丢失，保证唤醒的是有效节点。

### 6.3 `cancelAcquire`（取消节点）
```java
private void cancelAcquire(Node node) {
    if (node == null)
        return;
    node.thread = null; // 清空绑定的线程 ⛰️

    // 跳过前驱的取消节点 ⛰️
    Node pred = node.prev;
    while (pred.waitStatus > 0)
        node.prev = pred = pred.prev; // ⛰️

    Node predNext = pred.next; // ⛰️
    node.waitStatus = Node.CANCELLED; // 标记为取消 ⛰️

    // 若当前是尾节点→CAS更新尾节点，清理当前节点 🌏
    if (node == tail && compareAndSetTail(node, pred)) {
        compareAndSetNext(pred, predNext, null); // 🌏
    } else {
        int ws;
        if (pred != head &&
            ((ws = pred.waitStatus) == Node.SIGNAL ||
             (ws <= 0 && compareAndSetWaitStatus(pred, ws, Node.SIGNAL))) &&
            pred.thread != null) {
            Node next = node.next; // ⛰️
            if (next != null && next.waitStatus <= 0)
                compareAndSetNext(pred, predNext, next); // 链接前驱和后继 🌏
        } else {
            unparkSuccessor(node); // 唤醒后继 ⛰️🌏
        }
        node.next = node; // 自引用，便于GC ⛰️
    }
}
```
**书本解读**：取消节点的核心是"清理引用+标记状态+重新链接队列" ⛰️，避免无效节点影响队列结构。

### 6.4 `isOnSyncQueue`（判断节点是否在同步队列）
```java
final boolean isOnSyncQueue(Node node) {
    // 条件：1. waitStatus=CONDITION（条件队列）→ 不在；2. prev=null（未入队）→ 不在 ⛰️
    if (node.waitStatus == Node.CONDITION || node.prev == null)
        return false;
    if (node.next != null) // next≠null → 已入队 ⛰️
        return true;
    // 兜底：从尾节点反向遍历，检查是否存在当前节点 ⛰️
    return findNodeFromTail(node);
}
```

---

## 7. JUC经典同步器·AQS实现拆解（书本核心案例）
### 7.1 ReentrantLock（独占锁，书本重点）
#### 核心设计（书本）
- 同步状态`state`：0=空闲，N=重入次数 ⛰️；
- 公平/非公平锁：通过`tryAcquire`实现不同的抢锁规则 🌏。

#### 非公平锁`tryAcquire`（书本源码）
```java
static final class NonfairSync extends Sync {
    protected final boolean tryAcquire(int acquires) {
        final Thread current = Thread.currentThread(); // ⛰️
        int c = getState(); // ⛰️
        if (c == 0) {
            // 非公平：直接CAS抢锁（不排队）🌏
            if (compareAndSetState(0, acquires)) {
                setExclusiveOwnerThread(current); // 设置独占线程 ⛰️
                return true;
            }
        }
        // 重入：当前线程已持有锁，递增重入次数 ⛰️
        else if (current == getExclusiveOwnerThread()) {
            int nextc = c + acquires;
            if (nextc < 0) // 溢出
                throw new Error("Maximum lock count exceeded");
            setState(nextc); // ⛰️
            return true;
        }
        return false;
    }
}
```

#### 公平锁`tryAcquire`（书本源码）
```java
static final class FairSync extends Sync {
    protected final boolean tryAcquire(int acquires) {
        final Thread current = Thread.currentThread(); // ⛰️
        int c = getState(); // ⛰️
        if (c == 0) {
            // 公平：先检查队列是否有等待节点，无则抢锁 ⛰️
            if (!hasQueuedPredecessors() && compareAndSetState(0, acquires)) { // 🌏
                setExclusiveOwnerThread(current); // ⛰️
                return true;
            }
        }
        // 重入逻辑与非公平一致 ⛰️
        else if (current == getExclusiveOwnerThread()) {
            int nextc = c + acquires;
            if (nextc < 0)
                throw new Error("Maximum lock count exceeded");
            setState(nextc); // ⛰️
            return true;
        }
        return false;
    }
}

// 检查是否有前驱等待节点（公平锁核心）⛰️
public final boolean hasQueuedPredecessors() {
    Node t = tail;
    Node h = head;
    Node s;
    // 逻辑：头节点≠尾节点 且 头节点后继≠当前线程 → 有前驱等待 ⛰️
    return h != t &&
        ((s = h.next) == null || s.thread != Thread.currentThread());
}
```
#### 书本核心解读
- 非公平锁性能更高 🌏：避免了队列检查的开销，允许"插队"抢锁；
- 公平锁更公平 ⛰️：保证线程按FIFO顺序获取锁，避免饥饿。

### 7.2 Semaphore（共享锁，书本重点）
#### 核心设计
- 同步状态`state`：可用许可数 ⛰️；
- `tryAcquireShared`：CAS递减许可数 🌏，<0则获取失败；
- `tryReleaseShared`：CAS递增许可数 🌏，实现许可释放。

#### 核心源码（书本简化版）
```java
static final class NonfairSync extends Sync {
    protected int tryAcquireShared(int acquires) {
        // 非公平：直接CAS抢许可 🌏
        return nonfairTryAcquireShared(acquires);
    }
}

static final class FairSync extends Sync {
    protected int tryAcquireShared(int acquires) {
        for (;;) {
            // 公平：先检查队列 ⛰️
            if (hasQueuedPredecessors())
                return -1;
            int available = getState(); // ⛰️
            int remaining = available - acquires;
            if (remaining < 0 || compareAndSetState(available, remaining)) // 🌏
                return remaining;
        }
    }
}

// 非公平抢许可（核心）🌏
final int nonfairTryAcquireShared(int acquires) {
    for (;;) {
        int available = getState(); // ⛰️
        int remaining = available - acquires;
        if (remaining < 0 || compareAndSetState(available, remaining)) // 🌏
            return remaining;
    }
}

// 释放许可（核心）🌏
protected final boolean tryReleaseShared(int releases) {
    for (;;) {
        int current = getState(); // ⛰️
        int next = current + releases;
        if (next < current) // 溢出
            throw new Error("Maximum permit count exceeded");
        if (compareAndSetState(current, next)) // 🌏
            return true;
    }
}
```

### 7.3 CountDownLatch（共享锁，书本重点）
#### 核心设计
- 同步状态`state`：计数器值 ⛰️；
- `await()`：调用`acquireShared(1)`，`tryAcquireShared`返回state是否为0 🌏；
- `countDown()`：调用`releaseShared(1)`，`tryReleaseShared`递减state至0 🌏。

#### 核心源码（书本简化版）
```java
protected int tryAcquireShared(int acquires) {
    // 计数器为0→获取成功，否则失败（自旋等待）⛰️
    return (getState() == 0) ? 1 : -1;
}

protected boolean tryReleaseShared(int releases) {
    for (;;) {
        int c = getState(); // ⛰️
        if (c == 0)
            return false;
        int nextc = c - 1;
        if (compareAndSetState(c, nextc)) // 🌏
            return nextc == 0; // 仅当计数器为0时返回true，触发唤醒
    }
}
```

---

## 8. 压轴精讲：自定义同步器TwinsLock（书本扩展）
### 8.1 设计目标（对齐书本）
实现**同一时刻最多允许2个线程并发执行**的共享锁，基于AQS共享模式开发，验证AQS的复用性。

### 8.2 同步状态设计（书本）
- `state=0`：无线程占用 ⛰️；
- `state=1`：1个线程执行 ⛰️；
- `state=2`：线程已满，后续入队等待 ⛰️；
- 核心规则：`tryAcquireShared`递增state 🌏，`tryReleaseShared`递减state 🌏。

### 8.3 完整源码+逐行注释（补充书本细节）
```java
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.AbstractQueuedSynchronizer;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;

/**
 * 自定义共享锁：TwinsLock（书本扩展案例）
 * 同一时刻最多允许2个线程并发
 */
public class TwinsLock implements Lock {
    // 自定义AQS同步器（静态内部类，书本推荐写法）
    private static final class Sync extends AbstractQueuedSynchronizer {
        private static final long serialVersionUID = 1L;
        private static final int MAX_PERMITS = 2; // 最大并发数2 ⛰️

        // 共享获取：重写AQS钩子方法
        @Override
        protected int tryAcquireShared(int acquires) {
            for (;;) { // 自旋CAS保证原子性（书本核心）🌏
                int currentState = getState(); // 获取当前同步状态 ⛰️
                int nextState = currentState + acquires; // 计算新状态 ⛰️
                // 超过最大并发数→获取失败，返回-1 ⛰️
                if (nextState > MAX_PERMITS) {
                    return -1;
                }
                // CAS修改状态成功→返回新状态（共享模式核心）🌏
                if (compareAndSetState(currentState, nextState)) {
                    return nextState;
                }
            }
        }

        // 共享释放：重写AQS钩子方法
        @Override
        protected boolean tryReleaseShared(int releases) {
            for (;;) { // 自旋CAS保证原子性 🌏
                int currentState = getState(); // ⛰️
                int nextState = currentState - releases; // 归还许可 ⛰️
                // CAS修改状态成功→返回true（触发传播唤醒）🌏
                if (compareAndSetState(currentState, nextState)) {
                    return true;
                }
            }
        }

        // 书本补充：创建Condition（TwinsLock暂不支持）⛰️
        Condition newCondition() {
            return new ConditionObject();
        }
    }

    // 持有同步器实例（书本推荐：聚合而非继承）⛰️
    private final Sync sync = new Sync();

    // ========== Lock接口实现（对齐书本） ==========
    @Override
    public void lock() {
        sync.acquireShared(1); // 共享模式获取锁 ⛰️🌏
    }

    @Override
    public void unlock() {
        sync.releaseShared(1); // 共享模式释放锁 ⛰️🌏
    }

    @Override
    public void lockInterruptibly() throws InterruptedException {
        sync.acquireInterruptibly(1); // 可中断获取 ⛰️
    }

    @Override
    public boolean tryLock() {
        return sync.tryAcquireShared(1) >= 0; // 尝试获取（非阻塞）🌏
    }

    @Override
    public boolean tryLock(long time, TimeUnit unit) throws InterruptedException {
        return sync.tryAcquireNanos(1, unit.toNanos(time)); // 超时获取 ⛰️🌏
    }

    @Override
    public Condition newCondition() {
        return sync.newCondition(); // 返回Condition（暂不支持，抛异常也可）⛰️
    }
}
```

### 8.4 测试代码+验证（补充书本测试细节）
```java
import java.util.concurrent.TimeUnit;

/**
 * 测试TwinsLock（书本扩展）
 * 验证：同一时刻仅2个线程执行
 */
public class TwinsLockTest {
    public static void main(String[] args) {
        Lock lock = new TwinsLock();
        // 定义任务：加锁→打印→休眠1秒→解锁
        Runnable task = () -> {
            while (true) {
                lock.lock();
                try {
                    System.out.printf("[%s] 执行中 | 当前时间：%d%n",
                            Thread.currentThread().getName(),
                            System.currentTimeMillis() / 1000);
                    TimeUnit.SECONDS.sleep(1); // 模拟业务执行 ⛰️
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt(); // ⛰️
                    break;
                } finally {
                    lock.unlock(); // ⛰️🌏
                }
            }
        };

        // 启动10个线程（书本：验证并发限制）⛰️
        for (int i = 0; i < 10; i++) {
            Thread thread = new Thread(task, "TwinsLock-Thread-" + i);
            thread.setDaemon(true); // 守护线程，便于退出 ⛰️
            thread.start();
        }

        // 主线程休眠5秒，观察输出 ⛰️
        try {
            TimeUnit.SECONDS.sleep(5);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }
}
```
#### 执行结果（书本预期）
```
[TwinsLock-Thread-0] 执行中 | 当前时间：1718000000
[TwinsLock-Thread-1] 执行中 | 当前时间：1718000000
// 1秒后
[TwinsLock-Thread-2] 执行中 | 当前时间：1718000001
[TwinsLock-Thread-0] 执行中 | 当前时间：1718000001
// 1秒后
[TwinsLock-Thread-1] 执行中 | 当前时间：1718000002
[TwinsLock-Thread-3] 执行中 | 当前时间：1718000002
```
✅ 验证结果：同一时刻仅输出2个线程，符合"最多2个并发"的设计目标 ⛰️。

### 8.5 核心原理总结（书本扩展）
1. **复用性**：仅重写`tryAcquireShared`和`tryReleaseShared`两个钩子方法 🌏，AQS自动处理队列、阻塞、唤醒、传播等核心逻辑；
2. **性能**：CAS自旋无锁修改`state` 🌏，无synchronized的重量级锁开销；
3. **扩展性**：修改`MAX_PERMITS`可适配任意并发数（如3个、5个），体现AQS的灵活性；
4. **适用场景**（书本）：接口限流（如每秒仅允许2个请求）、资源池并发控制（如连接池最多2个连接）、轻量级共享锁。

---
