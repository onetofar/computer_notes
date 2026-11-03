# 谁来 join、异常怎么回来：thread / async / future / promise

> 来源讲义：`C++ Concurrency in Action/Chapter 2. Managing threads（讲义）.md` §2.1
> `C++ Concurrency in Action/Chapter 4. Synchronizing concurrent operations（讲义）.md` §4.2

同一个问题：**后台线程算出来的结果，怎么回到主线程？出了异常又怎么办？**
四个文件各给一种答案，差别全在「谁负责收尾」。

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-thread-manual-join.cpp` | `std::thread` 没有返回值通道, 异常得自己用 `exception_ptr` 搬回来; 主线程必须 `join()` 才拿得到结果 | `result=42` / `异常自己搬回主线程 -> worker failed` / `join 后 joinable=false` |
| `02-async-future-exception.cpp` | `std::async` + `std::future`: 异常由 `get()` 重抛; `launch::deferred` 的任务在 `get()` 时才在**当前线程**执行 | `get() 重抛异常 -> async task failed` / `status=deferred` / `get() 返回 7` |
| `03-promise-explicit.cpp` | `std::promise`: 值和异常都由持有者亲手兑现; 一个值都不兑现就析构 = broken_promise | `set_value 后 get() = 99` / `set_exception -> explicit failure` / `未兑现就析构 -> std::future_error: Broken promise` |
| `04-shared-future-consumers.cpp` | `std::shared_future`: 每个消费者拿一份**拷贝**, 都能取到同一个结果 | `consumer0/1/2: 取到 2026` |

## 讨论点

1. **`join()` 是粗但有效的**：它把「线程结束」和「结果到手」绑成一件事。`std::thread` 没有第二个通道,
   所以 `01` 里线程只能把结果写进共享变量、把异常包成 `exception_ptr` —— **搬运工作是你自己写的**。
2. **`future` 把异常也变成结果**：`02` 里子线程抛出的异常不会 `terminate`, 而是躺在 future 里,
   等 `get()` 时原样重抛。**谁调用 `get()`, 谁负责接异常。**
3. **`promise` 是显式版**：`03` 里值从哪来、什么时候兑现, 全看持有者。好处是**契约不会沉默**——
   promise 被销毁而没兑现, 等待方拿到的是 `broken_promise` 异常, 而不是永远等下去。
4. **`future` 是一次性的, `shared_future` 才是可多次读的**：`04` 里三个消费者各拿一份拷贝,
   库内部负责同步同一个共享状态。注意是「**每个线程一份拷贝**」, 不是「多线程访问同一个 future 对象」。

## 教材原话

> 以下引文逐字取自讲义所引的原文稿（讲义注明为单行可命中的片段）。

> join() is a simple and brute-force technique—either you wait for a thread to finish or you don’t.

> you need to ensure that you’ve called either join() or detach() before a std::thread object is destroyed

> the call to join() is liable to be skipped if an exception is thrown

> if the function call invoked as part of std::async throws an exception, that exception is stored in the future in place of a stored value, the future becomes ready , and a call to get() rethrows that stored exception.

> If you destroy the std::promise without setting a value, an exception is stored instead.

> This is by design: std::future models unique ownership of the asynchronous result

> The preferred way to use it would be to pass a copy of the shared_future object to each thread

> the future objects themselves don’t provide synchronized accesses.

## 跑法

```bash
cd <vault>/markdown/lab
bash tools/run_all.sh
# 单个文件:
bash tools/check_one.sh books/cpp-concurrency-in-action/thread-vs-async-future/02-async-future-exception.cpp
```
