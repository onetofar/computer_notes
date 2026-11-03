# 保护同一份共享状态：mutex / atomic / call_once / 手工 once flag

> 来源讲义：`C++ Concurrency in Action/Chapter 3. Sharing data between threads（讲义）.md` §3.2.8, §3.3.1
> `C++ Concurrency in Action/Chapter 5. The C++ memory model and operations on atomic types（讲义）.md` §5.2

同一个问题：**保护一份共享数据, 到底该用哪种手段, 代价差在哪？**

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-mutex-counter.cpp` | `std::mutex` + `lock_guard`: 每次加法都过一次锁 | `mutex-counter: counter=200000 (期望 200000) 耗时=… us` |
| `02-atomic-counter.cpp` | 同一份计数器换成 `std::atomic<long long>` 的 `fetch_add(relaxed)` | `atomic-counter: counter=200000 (期望 200000) 耗时=… us` |
| `03-call-once-init.cpp` | `std::call_once`: 初始化**恰好一次**, 之后每次调用只查一次标志 | `Resource(7) 构造` 只出现一次 / `call_once: id=7 耗时=… us` |
| `04-manual-once-flag.cpp` | 手工 once flag（`mutex` + `bool`）: 语义相同, 但**每次调用都要过一次锁** | `Resource(7) 构造` 只出现一次 / `manual-once: id=7 耗时=… us` |

## 讨论点

1. **`01` vs `02`：换成 atomic 并不等于更快。** 4 个线程全在**同一根缓存行**上做 RMW,
   cache line 在核之间来回弹, 原子操作的代价被放大。作者在无锁那章把账算得很直白:
   **原子操作可能让整体性能不升反降**。要真正并行, 得先分片（见 `lock-granularity/`）。
2. **`03` vs `04`：语义完全一样, 代价不一样。** 两者都保证「初始化恰好一次」,
   但 `04` 的 40 万次调用每次都要走一遍 `mutex`; `03` 的 `call_once` 在初始化完成后
   只查一次原子标志。实测 `call_once` 明显更快——这就是原文说「开销低于显式用 mutex」的意思。
3. **两者的差别在**「没兑现会怎样」**：`call_once` 内部封装了「已经在做了 / 做完了 / 出异常了」
   三种状态; 手工 flag 要自己保证「检查与置位」在同一把锁里, 写错就是**双检锁**那类经典陷阱。
4. **函数内 `static` 是第三条路**：原文强调 **`Initialization guaranteed to be thread-safe`**——
   C++11 之后局部静态的初始化保证恰好发生一次, 连 `once_flag` 都不用。

## 教材原话

> it’s a bad idea to hold a lock on a mutex for longer than necessary.

> the C++ Standard Library provides std::once_flag and std::call_once to handle this situation.

> Use of std::call_once will typically have a lower overhead than using a mutex explicitly, especially when the initialization has already been done

> like std::mutex , std::once_flag instances can’t be copied or moved

> Initialization guaranteed to be thread-safe

> Atomic operations are inherently slow—often 100 times slower than an equivalent non-atomic operation on desktop CPUs—so this makes pop() an expensive operation.

> although it can increase the potential for concurrency of operations on a data structure and reduce the time an individual thread spends waiting, it may well decrease overall performance.

> In order to use std::atomic<UDT> for some user-defined type UDT ,, this type must have a trivial copy-assignment operator.

## 跑法

```bash
cd <vault>/markdown/lab
bash tools/run_all.sh
# 单个文件:
bash tools/check_one.sh books/cpp-concurrency-in-action/protecting-shared-state/03-call-once-init.cpp
```
