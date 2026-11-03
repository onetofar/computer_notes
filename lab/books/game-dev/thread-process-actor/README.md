# 锁住数据，还是把消息传过去？线程 / 进程 / Actor

> 来源讲义：`书籍/教材讲义/网络多人在线游戏开发实战教程/第3章 线程、进程以及Actor模型（讲义）.md` §3.5 / §3.6

四份文件做**同一件事**：8 个工作单元各算 5 万次，合计 40 万。
唯一变化的是"这 8 份工作怎么共享/传递状态"，所以四份输出可以直接横向比。

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-thread-shared-locked.cpp` | 线程共享内存，用 `std::mutex` 保护自增 | `counter=400000 expected=400000`（**恒等于**）、`lock_ops=400000`、耗时约 150 ms |
| `02-thread-shared-racy.cpp` | 同样共享内存，但不保护（读-改-写三步被交错） | `counter=50167 expected=400000 lost=349833`（**每次运行都不一样**，丢失量随调度变化） |
| `03-process-message.cpp` | 拆成 8 个进程，各自算各自的，结果经管道回传 | `counter=400000 expected=400000`、`pipe_bytes=64`、耗时约 2 ms |
| `04-actor-mailbox.cpp` | 8 个 Actor 线程，每个持有**私有**计数器 + 自己的邮箱 | `counter=400000 expected=400000`、`enqueue=400000 dequeue=400000 fifo_preserved=1`、耗时约 390 ms |

四份输出放在一起看：**结果是 40 万的只有三种写法（加锁、进程、Actor），
不加锁的那种会丢数据；代价分别是 40 万次锁、64 字节管道消息、40 万次邮箱出入队。**

## 教材原话

讲义 §3.5 先把进程和线程的差别说死：

> - **进程**：有 PID 的独立内存空间，一个 exe 就是一个进程。进程之间不共享内存（所以进程间通信要靠 Socket）；
> - **线程**：进程内的轻量执行流，有自己的调用堆栈，**依附于进程、共享进程的内存**。

—— §3.5

讲义 §3.5 用一个具体例子说明共享内存为什么危险：

> 两个线程同时写同一个变量，结果不可预估（书里的例子：两个线程各做一次 `global_value++`，最后打印出两个 2——自增在底层是"读-改-写"三步，被交错执行了）。常规解法两条路：
>
> | | 加锁 | 消息机制 |
> |---|---|---|
> | 思路 | 让线程排队访问数据 | 根本不直接碰数据，把"要做的事"排进队列 |
> | 问题 | 锁内退化为单线程；**一个锁会繁殖成无数个锁**，漏一处就宕机 | 入队/出队时要锁，但**只有这一处**要锁 |

—— §3.5

讲义 §3.5 给出的选型结论：

> 书里的判断很干脆：功能越多锁越多，多线程+加锁后期几乎是"毁灭性的灾难"，所以本书选择**消息机制**——这正是 Actor 模型登场的全部理由。

—— §3.5

讲义 §3.6 把 Actor 的定义压成三句话：

> 1. 每个 Actor 有**消息队列（MailBox）+ 行为 + 自己的数据**三部分；
> 2. **任何数据只有它自己能改**，对外封闭；
> 3. Actor 之间**有且只有一种通信方式——发消息**。两个 Actor 不能互调函数。

—— §3.6

讲义 §3.6 说明它换来的第一样东西：

> - **不用加锁**：数据只有自己动，唯一要锁的地方是信箱的出入队；

—— §3.6

## 跑法

```bash
bash tools/run_all.sh

cd books/game-dev/thread-process-actor
g++ -std=c++20 -O2 -Wall -Wextra -pthread 01-thread-shared-locked.cpp -o /tmp/01 && /tmp/01
g++ -std=c++20 -O2 -Wall -Wextra -pthread 02-thread-shared-racy.cpp   -o /tmp/02 && /tmp/02
g++ -std=c++20 -O2 -Wall -Wextra -pthread 03-process-message.cpp     -o /tmp/03 && /tmp/03
g++ -std=c++20 -O2 -Wall -Wextra -pthread 04-actor-mailbox.cpp       -o /tmp/04 && /tmp/04
```

## 关于 `02` 不标 `@ub: yes`

`02-thread-shared-racy.cpp` 是**故意的错误**（数据竞争 = 未定义行为），
但它**没有**标 `@ub: yes`：

- `@ub: yes` 在本仓库的含义是"用 valgrind / ASan 能检出"
  （`tools/check_one.sh --ub` 会去找 `Invalid read`、`AddressSanitizer` 这类输出）；
- 数据竞争**不是**内存错误，valgrind 和 ASan 都查不出来，要查得用 **ThreadSanitizer**；
- 标上它会让 `bash tools/run_all.sh --ub` 判成"没触发检测"，反而变成一个假失败。

`02` 的未定义行为在文件头用 `// @note:` 标了出来，读代码的人不会错过。

**平台说明**：`03-process-message.cpp` 依赖 POSIX `fork`/`pipe`，非 Linux 平台只打印提示就退出。
