---
title: SPSC无锁队列实现详解
date: 2026-07-03
tags:
  - cpp
  - cpp/mem
source: 转自:"https://www.cnblogs.com/sinkinben/p/17949761/spsc-queue"
status: complete
---
# 无锁队列 SPSC Queue：从基础实现到高性能优化

在多线程编程中，生产者-消费者问题是一个经典的并发场景。当生产者和消费者各自只有一个时，可以设计一个完全无锁的队列——**SPSC（Single Producer Single Consumer）Queue**。

本文从最基础的实现开始，逐步应用缓存行对齐和本地缓存优化，将吞吐量从约 2900 万元素/秒提升至约 8000 万元素/秒，并解释每一步背后的底层原理。

---

## 0. 应用场景

考虑一个计算密集型且延迟敏感的循环，每次迭代结束需要输出当前的迭代次数和计算结果：

```cpp
void matrix_compute() {
    for (int i = 0; i < n; ++i) {
        // 计算密集型代码...
        // 打印 i 和计算结果
        std::cout << "Iteration " << i << ": result = " << result << std::endl;
    }
}
```

`std::cout` 涉及 I/O 操作，每次输出都会造成显著的延迟。一个直观的解决方案是：将日志封装为字符串，传递给另一个专用线程去打印，实现异步日志输出。SPSC 无锁队列正是这种场景下最契合的解决方案——生产者在计算线程中入队日志，消费者在后台线程中出队并打印，两者无需等待对方。

---

## 1. 基础实现

使用环形缓冲区（RingBuffer）实现队列。核心前提：`head` 只被消费者写入，`tail` 只被生产者写入，因此不需要锁，但需要保证写入的原子性。

### 1.1 数据结构

```cpp
template <class T>
class spsc_queue {
private:
    std::vector<T> m_buffer;          // 环形缓冲区，容量为 capacity + 1
    std::atomic<size_t> m_head{0};    // 消费者指针：下一个要读的位置
    std::atomic<size_t> m_tail{0};    // 生产者指针：下一个要写的位置
public:
    spsc_queue(size_t capacity) : m_buffer(capacity + 1) {}
    inline bool enqueue(const T &item);
    inline bool dequeue(T &item);
};
```

**为什么 `m_buffer` 的实际容量是 `capacity + 1`？** 因为环形缓冲区需要浪费一个槽位来区分“队列满”和“队列空”。当 `head == tail` 时队列为空；当 `(tail + 1) % size == head` 时队列为满。如果不浪费这个槽位，`head == tail` 无法区分是“空”还是“满”。

### 1.2 环形缓冲区的物理布局

`head` 和 `tail` 索引在 `[0, buffer.size())` 范围内循环，通过取模运算映射回数组的物理索引：

```
m_buffer:  [ slot0 ][ slot1 ][ slot2 ][ slot3 ][ slot4 ][ slot5 ][ slot6 ][ slot7 ]
索引:         0        1        2        3        4        5        6        7

head = 2  → 消费者下次从 slot2 读取
tail = 5  → 生产者下次写入 slot5

已消费区域: [ slot0, slot1 ]          ← head 左边的区域，可被覆盖写入
有效数据:   [ slot2, slot3, slot4 ]    ← head 和 tail 之间的区域，等待消费
空闲区域:   [ slot5, slot6, slot7 ]    ← tail 及右边的区域，可写入新数据
```

### 1.3 入队与出队实现

```cpp
inline bool enqueue(const T &item) {
    const size_t tail = m_tail.load(std::memory_order_relaxed);
    const size_t next = (tail + 1) % m_buffer.size();
    if (next == m_head.load(std::memory_order_acquire))  // 判满
        return false;
    m_buffer[tail] = item;
    m_tail.store(next, std::memory_order_release);
    return true;
}

inline bool dequeue(T &item) {
    const size_t head = m_head.load(std::memory_order_relaxed);
    if (head == m_tail.load(std::memory_order_acquire))  // 判空
        return false;
    item = m_buffer[head];
    const size_t next = (head + 1) % m_buffer.size();
    m_head.store(next, std::memory_order_release);
    return true;
}
```

### 1.4 内存序的选择逻辑

| 操作 | 写入者 | 读取者用 `relaxed` | 读取者用 `acquire` |
|:---|:---|:---|:---|
| `m_tail` | 生产者 | 生产者只读自己的变量，无需同步 | 消费者需要看到新写入的数据，建立 happens-before |
| `m_head` | 消费者 | 消费者只读自己的变量，无需同步 | 生产者需要看到消费者腾出的空位，建立 happens-before |

### 1.5 基础版本性能

| 指标 | 值 |
|:---|:---|
| **Mean** | 29,158,897.20 elements/s |
| **Median** | 29,178,822.00 elements/s |
| **Max** | 29,315,199 elements/s |
| **Min** | 28,995,515 elements/s |

> Benchmark 方法：生产者和消费者分别执行 `1e8` 次 `enqueue/dequeue`，计算总时间 `t`，吞吐量 = `1e8 / t`。重复 10 次取统计值。

---

## 2. 消除伪共享

### 2.1 什么是伪共享？

现代 CPU 的缓存以缓存行（Cache Line）为基本单位，通常为 64 字节。当两个线程分别修改位于同一个缓存行的不同数据时，MESI 协议会触发缓存一致性问题，导致频繁的缓存行同步，显著降低性能。

**示例**：

```cpp
int a[1024];
void worker(int idx) {
    for (int j = 0; j < 1e9; j++)
        a[idx] = a[idx] + 1;
}
```

- **P1**：启动 2 线程，执行 `worker(0)` 和 `worker(1)`
- **P2**：启动 2 线程，执行 `worker(0)` 和 `worker(16)`

**P2 的执行速度比 P1 快**。因为 `a[0]` 和 `a[1]` 在同一个缓存行，每次写入都触发缓存同步；而 `a[0]` 和 `a[16]` 间距 64 字节（`16 × sizeof(int)`），落不同缓存行，避免了这个问题。

### 2.2 解决方案：缓存行对齐

```cpp
template <class T>
class spsc_queue {
private:
    std::vector<T> m_buffer;
    alignas(64) std::atomic<size_t> m_head;
    alignas(64) std::atomic<size_t> m_tail;
};
```

`alignas(64)` 确保每个成员占据独立的缓存行。更合理的方式是使用标准的 `hardware_constructive_interference_size` 常量：

```cpp
#ifdef __cpp_lib_hardware_interference_size
using std::hardware_constructive_interference_size;
#else
constexpr std::size_t hardware_constructive_interference_size = 64;
#endif
```

**效果对比**：

```
优化前:  29,158,897 elements/s
优化后:  38,993,940 elements/s  →  +34%
```

**为什么 `+34%` 是合理的？** 伪共享的代价不是固定开销，而是“概率性”地发生。每次 `m_head` 或 `m_tail` 被对方核心读取时，如果恰好与另一个变量的修改在同一缓存行，就触发一次缓存同步。基础版本中，这两个变量可能落在同一缓存行，概率大约是 50%。消除伪共享后，每次访问都命中自己的缓存行，不再有无效同步——提升幅度接近伪共享发生的概率对应的额外开销。

---

## 3. 减少无效内存访问

### 3.1 问题分析

在 `enqueue`/`dequeue` 的判满/判空逻辑中，每次操作都要执行一次跨核心的原子加载：

```cpp
if (next == m_head.load(std::memory_order_acquire))
    return false;
```

实际场景下，判满/判空条件极少触发——大部分时候队列有空位也有数据。但即使队列有空位，CPU 仍然每次都要跨核心获取另一个线程修改的变量值，这是一次昂贵的共享内存访问。

### 3.2 优化方案：本地缓存挡一道

在类中增加两个本地缓存变量，先检查本地缓存，只在缓存显示“可能满/空”时才真正去读共享内存：

```cpp
template <class T>
class spsc_queue {
private:
    std::vector<T> m_buffer;
    alignas(hardware_constructive_interference_size) std::atomic<size_t> m_head;
    alignas(hardware_constructive_interference_size) std::atomic<size_t> m_tail;

    alignas(hardware_constructive_interference_size) size_t cached_head{0};
    alignas(hardware_constructive_interference_size) size_t cached_tail{0};
};

inline bool enqueue(const T &item) {
    const size_t tail = m_tail.load(std::memory_order_relaxed);
    const size_t next = (tail + 1) % m_buffer.size();

    if (next == cached_head) {                          // 本地缓存说“可能满了”
        cached_head = m_head.load(std::memory_order_acquire);  // 才真正读共享内存
        if (next == cached_head)                        // 确认确实满了
            return false;
    }
    m_buffer[tail] = item;
    m_tail.store(next, std::memory_order_release);
    return true;
}
```

**为什么 `cached_head` 能挡住大部分读操作？** `head` 只会被消费者向后移动。如果 `cached_head != next`，说明消费者已经读取了更多数据，队列比上次检查时更“空”了，无需确认。只有当 `cached_head == next` 时，才需要真正读共享内存——而这种情况几乎只在队列快满时才会发生。
#### 共享内存 vs 本地缓存：物理层面的本质区别

|维度|`m_head`（共享内存）|`cached_head`（本地缓存）|
|---|---|---|
|**物理位置**|主存中，所有CPU核心可见|当前CPU核心的L1/L2缓存中|
|**写入者**|消费者线程|生产者线程自己的本地副本|
|**读取时的硬件操作**|需要跨核心缓存同步（MESI协议）|直接从本地缓存行读取，无同步开销|
|**原子性**|需要`std::atomic`和`memory_order`保证|不需要，只有生产者自己访问|

**“共享内存”**在物理上不是一块特殊的内存，而是指**被多个CPU核心同时访问的变量**。`m_head`就是共享内存——消费者在修改它，生产者在读取它。每次`m_head.load(acquire)`都需要通过MESI协议确保当前核心的缓存和消费者核心的缓存一致。

**“本地缓存”**在物理上也不是一块特殊的缓存，而是指**只被当前线程访问的变量**。`cached_head`就是一个普通的`size_t`成员变量，只有生产者线程在读写它。它就在当前CPU核心的L1缓存里，读取它不需要和任何其他核心同步。

### 3.3 优化效果

| 指标 | 基础版本 | 消除伪共享 | 减少无效访问 |
|:---|:---|:---|:---|
| **Mean** | 29.2 M/s | 39.0 M/s | **79.7 M/s** |
| **Median** | 29.2 M/s | 39.0 M/s | **79.8 M/s** |
| **提升** | - | +34% | **+104%（累计 2.7x）** |

**为什么这次提升比消除伪共享更显著？** 消除伪共享减少的是“每次访问共享变量时的缓存同步开销”——但每次判满/判空仍然要访问共享内存。这次优化直接砍掉了绝大多数共享内存访问本身——队列有空位时，生产者根本不需要知道 `head` 的最新值。从“每次都要跨核心访问”变成“偶尔跨核心访问”，减少了实际的共享内存流量。

---

## 4. 三个版本的演进总结

| 版本 | 优化点 | 平均吞吐量 (elements/s) | 相对提升 |
|:---|:---|:---|:---|
| **v1** | 基础实现 | 29.2 M | - |
| **v2** | 消除伪共享（缓存行对齐） | 39.0 M | +34% |
| **v3** | 减少无效内存访问（本地缓存） | 79.7 M | +104%（累计 2.7x） |

---

## 5. 可进一步优化的方向

- **非 POD 类型的性能优化**：如果 `T` 是 `std::string` 等非平凡拷贝的类型，频繁的拷贝构造和赋值重载会带来额外开销。可使用类似 `vector::emplace_back` 的原地构造（通过 Placement new 和 `std::forward` 实现），避免临时对象拷贝。

---

> **Github 仓库**：[https://github.com/sinkinben/spsc-queue](https://github.com/sinkinben/spsc-queue)