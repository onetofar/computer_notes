# 互斥锁家族：mutex / recursive_mutex / timed_mutex

> 来源讲义：`C++ Concurrency in Action/Chapter 3. Sharing data between threads（讲义）.md` §3.2–3.3
> `C++ Concurrency in Action/Chapter 4. Synchronizing concurrent operations（讲义）.md` §4.3.4

同一个问题：**一把锁, 什么时候会因为「用法不对」而变成灾难？**
三个文件分别踩三种不同的坑。

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-mutex-lock-guard.cpp` | 手工 `lock()/`unlock()` 在异常路径上会漏解锁; `lock_guard` 靠析构兜住 | `手工 unlock 兜住异常 (boom)` / `lock_guard 自动解锁, 异常照常抛出 (boom2)`, 两条路径后 `try_lock=true` |
| `02-recursive-mutex.cpp` | `recursive_mutex` 允许**同一线程**重复加锁, 但必须加几次解几次 | `同线程第 3 次 try_lock=true` / `全部解锁后 try_lock=true` |
| `03-timed-mutex.cpp` | `timed_mutex` 带超时: `try_lock_for` 拿不到锁就按超时返回 `false`, 不无限阻塞 | `空锁 try_lock=true` / `try_lock_for(30ms)=false, 实际等了 31 ms` |

## 讨论点

1. **`01` 的重点是异常路径**：手工写法必须自己记得解锁, 漏一次就是死锁;
   `lock_guard` 把「必须做的那件事」挪进析构, 于是**漏不掉**。
2. **`02` 不是解决方案, 是症状**：`recursive_mutex` 之所以存在, 通常是因为
   「public 成员函数都加锁, 但其中一个要调另一个」—— 递归锁让代码**跑得起来**,
   但它面对的是**不变量已经破损**的状态, 作者的作者身份立刻反对这种解法。
3. **`03` 的超时不是「精确到 30ms」**：作者说 duration 等待用 steady_clock 计时,
   但**系统调度和时钟精度**会让实际返回时间比请求的长得多, 而且**可能永远拿不到锁**。

## 教材原话

> Most of the time, if you think you want a recursive mutex, you probably need to change your design instead.

> But such usage is not recommended because it can lead to sloppy thinking and bad design.

> the class invariants
> are typically broken while the lock is held, which means that the second member function needs to work even when called with
> the invariants broken.

> It’s usually better to extract a new private member function that’s called from both member functions,
> which does not lock the mutex (it expects it to already be locked).

> the vagaries of system scheduling and the varying precisions of OS clocks means that the time between the thread issuing the call and returning from it may be much longer than 35 ms.

> 讲义附录 D 的锁族速查：`| \`std::recursive_mutex\` | 同一线程可重复 lock |`、`| \`std::timed_mutex\` | \`try_lock_for\` / \`try_lock_until\` |`

## 跑法

```bash
cd <vault>/markdown/lab
bash tools/run_all.sh
# 单个文件:
bash tools/check_one.sh books/cpp-concurrency-in-action/mutex-family/03-timed-mutex.cpp
```
