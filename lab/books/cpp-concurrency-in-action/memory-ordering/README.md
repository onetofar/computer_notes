# 同一个逻辑, 三种内存序：relaxed / acquire-release / seq_cst

> 来源讲义：`C++ Concurrency in Action/Chapter 5. The C++ memory model and operations on atomic types（讲义）.md` §5.3.3
> `C++ Concurrency in Action/Chapter 7. Designing lock-free concurrent data structures（讲义）.md` §7.3.1

三个文件是**同一段程序**, 只改内存序常量, 输出可以直接横向比。

## 程序在做什么

经典 **store buffering** 检验（对应讲义 Listing 5.4 / 5.5 / 5.7）：

```
线程 A:  x = r;  再读 y
线程 B:  y = r;  再读 x
```

如果**两个线程都读到对方还没写的那个值**（都读到上一轮的旧值）, 就是「两个线程对事件顺序没有共识」。
作者证明：**在 `seq_cst` 下这不可能发生**（写 x 与写 y 总有一个在前, 于是至少有一方能看到）。
而在 `relaxed` 与 `acquire/release` 下, 这是**允许**的。

每个文件跑 5 万轮, 输出两个计数：

- `双方都读到陈旧值`：**两边都陈旧**的轮数 —— 这才是被禁止/被允许的那个性质
- `单边陈旧`：只有一边陈旧的轮数 —— **三种内存序下都可能发生**, 不是关键指标

## 对照表

| 文件 | 差异点 | 预期输出（x86-64 实测） |
| --- | --- | --- |
| `01-relaxed.cpp` | store/load 全用 `memory_order_relaxed`, 不产生 synchronizes-with | `双方都读到陈旧值=2456 轮, 单边陈旧=47544 轮 / 共 50000 轮, 耗时=… us` |
| `02-acquire-release.cpp` | store 用 `release`、load 用 `acquire`: 点对点同步, **没有全局全序** | `双方都读到陈旧值=1156 轮, 单边陈旧=48841 轮 / 共 50000 轮, 耗时=… us` |
| `03-seq-cst.cpp` | 全用 `memory_order_seq_cst`: 有全局全序 | `双方都读到陈旧值=0 轮, 单边陈旧=47304 轮 / 共 50000 轮, 耗时=… us` |

三次的**具体计数每次跑都会变**, 但有两点是稳定的: `03` 的「双方都陈旧」**恒为 0**（这是保证, 不是运气）;
`03` 的**耗时最长**（`seq_cst` 的 store 在 x86 上要退化成一条全屏障指令）。

## 讨论点

1. **relaxed 的代价不是「慢一点」, 是「语义变了」**：它不参与 synchronizes-with,
   所以线程**真的可以看见不同的世界**, 不只是「编译器重排」。
2. **acquire/release 必须成对**：一个 release 写 + 一个 acquire 读（读到那个值）才产生同步;
   它是**点对点**的, 不像 `seq_cst` 那样有全局序, 所以上面的 store buffering 依然可能出现。
3. **`seq_cst` 是全局最贵的**：原文说它「requires global synchronization between all threads」。
   这就是本组三个文件里 `03` 明显更慢的原因。
4. **不要一上来就放松内存序**：作者把它列为**原型期指南**——放松内存序是**优化, 不是起点**。
   而「跑通了」不等于「保证对」：在 x86 上 `relaxed` 版的异常**未必每次都出现**,
   但标准允许它出现, 所以你不能拿「我这儿没复现」当证据。

## 教材原话

> they represent three models: sequentially consistent ordering ( memory_order_seq_cst ), acquire-release ordering ( memory_order_consume , memory_order_acquire , memory_order_release , and memory_order_acq_rel ), and relaxed ordering ( memory_order_relaxed ).

> Operations on atomic types performed with relaxed ordering don’t participate in synchronizes-with relationships.

> It’s so important I’ll say it again: threads don’t have to agree on the order of events .

> relaxed atomic operations unless they’re absolutely necessary, and even then using them only with extreme caution.

> In order to provide any synchronization, acquire and release operations must be paired up.

> Sequential consistency is the most straightforward and intuitive ordering, but it’s also the most expensive memory ordering ... because it requires global synchronization between all threads.

> using other memory orderings is an optimization , and as such you need to avoid doing it prematurely.

> This is complicated by the fact that the code may work when tested but isn’t guaranteed. Unless you have an algorithm checker that can systematically test all possible combinations of thread visibilities that are consistent with the specified ordering guarantees (and these things do exist), running the code isn’t enough.

## 跑法

```bash
cd <vault>/markdown/lab
bash tools/run_all.sh
# 单个文件:
bash tools/check_one.sh books/cpp-concurrency-in-action/memory-ordering/03-seq-cst.cpp
```
