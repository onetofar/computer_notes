# 一次性事件通知：条件变量、原子 flag、void future

> 来源讲义：`Effective Modern C++/7. The Concurrency API（讲义）.md` §5 Item 39

同一个问题——「任务 A 检测到事件，任务 B 必须等这个事件」——有四种写法，各自的代价完全不同。

## 对照关系

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-condition-variable.cpp` | 条件变量：带谓词 vs 裸 `wait` | 带谓词 `wait_for` 立刻返回 `1`；裸 `wait_for` 因为「通知早于等待」而错过，只能超时返回 `0` |
| `02-atomic-flag-poll.cpp` | 原子 flag 轮询 | 不用锁、不怕通知早到、不怕伪唤醒，但等待期间线程**一直在跑**：50ms 内轮询了约 40 万次 |
| `03-void-future.cpp` | `promise<void>`/`future<void>` 当一次性开关 | 先 `set_value()` 后 `wait()`：立刻返回、耗时 0ms（通知不丢）；等待方先阻塞时，`set_value()` 把它放行 |
| `04-shared-future-suspend.cpp` | 用 void future **挂起**线程 / `shared_future` 一次放行多个 | 线程先建好、此刻挂起，`set_value()` 后才真正开始跑；`shared_future` 一次放行 3 个线程 |

## 结论

教材原话：

> **During the time the task is waiting for the flag to be set, the task is essentially blocked, yet it’s still running.**

这是原子 flag 轮询的全部代价：它占着一个硬件线程，每次时间片切换都有上下文切换的开销，还可能让一个本可以关掉的核一直开着。真正被阻塞的任务（`cv.wait`）这些**全都没有**。`02` 文件把「还在跑」量化成轮询次数。

> **Using std::promise s and futures dodges these issues, but the approach uses heap memory for shared states, and it’s limited to one-shot communication.**

条件变量的三个毛病——多余的锁、通知早于等待会永久挂死、伪唤醒——`void future` 一次全绕开：不用锁、不怕通知早到（共享状态记住了值）、免疫伪唤醒。代价是共享状态通常在堆上，而且 **promise 只能 set 一次**。

讲义里对条件变量的三条批评，逐条对应到文件：

1. 「多了一把没必要的锁」——mutex 是用来保护共享数据的，而「检测方初始化完就交棒、反应方拿到才用」的场景里，两个任务完全可以靠程序逻辑互不打扰。
2. 「通知早于等待 → 反应方永久挂死」——`01` 文件用 `wait_for` 的超时把这个「永久挂死」变成可观测的 `0`（真的去挂死，测试就卡在 20 秒超时上了）。
3. 「伪唤醒（spurious wakeup）」——所以醒来之后必须重新检查条件，`01` 文件里那个谓词 lambda 就是这次检查。

「挂起一个线程」的技巧也在这一组里：既然 `future.wait()` 能确定性地挂起一个线程，那就可以先把线程创建出来（开销先付掉），等真正要跑的时候再放行——`04` 文件的前半段演示的就是它。

## 跑法

```bash
bash tools/run_all.sh          # 跳过 ub 文件
bash tools/run_all.sh --ub     # 连 ub 一起跑, 期望检测器报错
```

本目录四个文件都不含故意未定义行为，普通构建即可全部运行（涉及线程与等待，单次运行约 0.2 秒）。
