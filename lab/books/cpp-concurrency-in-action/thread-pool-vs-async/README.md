# 任务怎么跑：async 每次起线程 / 固定线程池 / 传参生命周期陷阱

> 来源讲义：`C++ Concurrency in Action/Chapter 4. Synchronizing concurrent operations（讲义）.md` §4.2.1
> `C++ Concurrency in Action/Chapter 9. Advanced thread management（讲义）.md` §9.1.1
> `C++ Concurrency in Action/Chapter 2. Managing threads（讲义）.md` §2.2

同一个问题：**64 个任务, 到底该起多少个线程？**

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-async-per-task.cpp` | 每个 `std::async(std::launch::async)` 任务**各起一个线程** | `async-per-task: 任务=64 同时在线线程峰值=64 和=85344 耗时=… us` |
| `02-async-dangling-ref.cpp` | ⚠️ **故意演示 UB**：线程捕获了裸指针, 指向的对象先被 `delete` | `runtime error: 悬垂读 —— 读的是已 delete 的对象 (期望 7, 实际 …)`, 退出码 **1** |
| `03-fixed-thread-pool.cpp` | 固定 8 个 worker + 任务队列: 线程数**不随任务数增长** | `thread-pool: 任务=64 固定线程=8 同时在线线程峰值=8 耗时=… us` |

## 讨论点

1. **`01` 的峰值线程数 = 任务数**：`std::async` 每个任务起一个线程是**合规行为**,
   不是某个实现的偷懒。连接数一涨, 线程数就跟着涨, OS 资源和上下文切换都会被吃掉——
   这正是讲义里「小型服务器用少量线程 处理多个连接」的动机。
2. **`03` 把线程数钉死**：任务进队列, worker 取走就跑。**线程数与任务数解耦**,
   这是线程池的全部价值。讲义还提醒了两处工程细节：
   **成员声明顺序决定析构顺序**（队列必须最后死）, 以及**构造中途失败要能收场**。
3. **`02` 是本组唯一的 UB 文件**：`std::thread` 的构造函数**按值拷贝实参**,
   但**拷贝的是指针, 不是指针指向的对象**。新线程真正解引用它时, 对象可能已经没了。
   讲义原话是「指针会在新线程里才被解引用」。

### 关于 `02` 的检测方式

- 有 ASan/UBSan 的环境里, 读已释放的堆对象会被**抢先报出** `heap-use-after-free`;
- 没有 sanitizer 时, 程序自己发现「读的是已 delete 的对象」, 打印 `runtime error:` 开头的诊断并**以非零码退出**。

两种情况下 `bash tools/run_all.sh --ub` 都会判定 `PASS(ub-detected)`。
普通 `bash tools/run_all.sh` 会**跳过**这一类带 `@ub: yes` 的文件。

## 教材原话

> it’s perfectly conforming for an implementation of std::async to start a new thread for each task

> A thread pool allows you to accomplish this; tasks that can be executed concurrently are submitted to the pool, which puts ... them on a queue of pending work.

> of the members is important: both the done flag and the worker_queue must be declared before the threads vector, which must in turn be declared before the joiner .

> Starting a thread can fail by throwing an exception, so you need to ensure that any threads you’ve already started are stopped

> the std::thread constructor copies the supplied values as is, without converting to the expected argument type

> you need to wrap the arguments that need to be references in std::ref

## 跑法

```bash
cd <vault>/markdown/lab
bash tools/run_all.sh            # 02 会被跳过（@ub: yes）
bash tools/run_all.sh --ub       # 02 会跑, 期望 PASS(ub-detected)
# 单个文件:
bash tools/check_one.sh books/cpp-concurrency-in-action/thread-pool-vs-async/03-fixed-thread-pool.cpp
```
