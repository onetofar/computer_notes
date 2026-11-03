# 条件变量：wait 端为什么要 while，signal 与 broadcast 怎么分工

> 来源讲义：`书籍/教材讲义/Linux多线程服务端编程/第2章 线程同步精要（讲义）.md` §2.2

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-while-predicate.cpp` | 消费者用 `while` + 谓词条件等待 | 3 条全部收到，条数与顺序正确 |
| `02-if-no-predicate.cpp` | 消费者误用 `if`，且真的来了一次虚假唤醒 | 虚假唤醒后读到失效槽位 → 双重释放（UB，检测器报错） |
| `03-signal-vs-broadcast.cpp` | `notify_one` 对 `notify_all` | 第一轮只有 1 个被唤醒，广播后全部被唤醒 |

## 教材原话

讲义 §2.2 讲「互斥器不是等待原语」：

> 互斥器（mutex）是加锁原语，用来排他性地访问共享数据，它不是等待原语。在使用mutex的时候，我们一般都会期望加锁不要阻塞，总是能立刻拿到锁。然后尽快访问数据，用完之后尽快解锁，这样才能不影响并发性和性能。

> 如果需要等待某个条件成立，我们应该使用条件变量（condition variable）。条件变量顾名思义是一个或多个线程等待某个布尔表达式为真，即等待别的线程“唤醒”它。条件变量的学名叫管程（monitor）。Java Object内置的wait()、notify()、notifyAll()是条件变量。

讲义 §2.2 给出的 wait 端三条规则：

> 1．必须与mutex一起使用，该布尔表达式的读写需受此mutex保护。

> 2．在mutex已上锁的时候才能调用wait()。

> 3．把判断布尔条件和wait()放到while循环中。

讲义 §2.2 讲「为什么必须是 while 而不是 if」：

> 上面的代码中必须用while循环来等待条件变量，而不能用if语句，原因是spurious wakeup。这也是面试多线程编程的常见考点。

讲义 §2.2 讲 signal 与 broadcast 的分工（引 [RWC] 原话）：

> broadcast should generally be used to indicate state change rather than resource availability.

讲义 §2.2 对应的中文对照表：

| | 语义 | 用法 |
|---|---|---|
| **signal** | 「**一个**资源可用」 | 队列里有 1 个元素 → 叫醒 1 个人 |
| **broadcast** | 「**状态**变了，自己判断」 | 计数器归零 / 谓词整体失效 → 叫醒所有人 |

## 讨论点

1. **`02` 把「虚假唤醒」做成了确定性事件。** 真实的虚假唤醒在 POSIX 下是允许但不可预测的，
   复现率极低。这里让生产者在条件**还没成立**时就 `notify_all()` 一次，把「合法但反直觉」的规则
   变成必然事件——`if` 写法当场露馅，`while` 写法安然无恙。
2. **`01` 与 `02` 的差别只有一行。** 一个 `while` 和一个 `if` 的差别，
   就是讲义说的「这类 bug 的现场是『同一个程序，同一份代码，同一个输入，有时对有时不对』」。
3. **`03` 对应讲义那句 signal/broadcast 的分工。** 「一个资源可用」用 signal，
   「状态变了」用 broadcast。反过来用，signal 会漏唤醒（正确性陷阱），
   broadcast 会制造 N² 次无效唤醒（性能陷阱）。
4. **`03` 里消费者用的是谓词版 `wait`**（`g_cv.wait(lk, pred)`），这是标准库把
   「while + 谓词」封装好的形式——教材写的是手写 while，两者语义相同。

## 跑法

```bash
bash tools/run_all.sh          # 跳过 ub 文件, 01 和 03 会跑
bash tools/run_all.sh --ub     # 连 02 一起跑, 期望检测器报错
```
