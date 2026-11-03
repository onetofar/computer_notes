# 死锁与加锁顺序：两把锁反着加

> 来源讲义：`书籍/教材讲义/Linux多线程服务端编程/第2章 线程同步精要（讲义）.md` §2.1.1、§2.1.2

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-self-deadlock.cpp` | 单线程二次 lock 同一把非递归 mutex | `DEADLOCK DETECTED`（300ms 超时判定） |
| `02-mutual-deadlock.cpp` | 两线程、两把锁，**加锁顺序相反** | `DEADLOCK DETECTED` + 双方持锁/等待关系 |
| `03-global-lock-order.cpp` | 同样两把锁，**全局顺序一致** | `NO DEADLOCK: ... 耗时=... us` |
| `04-recursive-mutex-hides-bug.cpp` | 递归 mutex：不死锁，但遍历中修改容器 | 迭代器失效 → 双重释放（UB，检测器报错） |

四个文件构成一组**梯度对照**：`01` 是单线程就能触发的最简死锁，`02` 是教材里的经典互斥死锁，
`03` 是同一份代码加上「全局锁序」之后的正确版本，`04` 则演示教材说的另一条分支——
换成递归锁以后死锁消失，但**问题被藏起来了**，换成另一种更难查的故障。

## 教材原话

讲义 §2.1.1 讲「为什么首选非递归 mutex」：

> 首选非递归mutex，绝对不是为了性能，而是为了体现设计意图。……在同一个线程里多次对non-recursive mutex加锁会立刻导致死锁，我认为这是它的优点，能帮助我们思考代码对锁的期求，并且及早（在编码阶段）发现问题。

讲义 §2.1.1 讲「递归锁方便，于是它会藏问题」：

> 将来有一天，Foo::doit()间接调用了post()，那么会很有戏剧性的结果：
>
> 1．mutex是非递归的，于是死锁了。
> 2．mutex是递归的，由于push_back()可能（但不总是）导致vector迭代器失效，程序偶尔会crash。

讲义 §2.1.1 给出的判词（本章最好记的一句工程原则）：

> 这时候就能体现non-recursive的优越性：把程序的逻辑错误暴露出来。死锁比较容易debug，把各个线程的调用栈打出来，只要每个函数不是特别长，很容易看出来是怎么死的……程序反正要死，不如死得有意义一点，留个“全尸”，让验尸（post-mortem）更容易些。

讲义 §2.1.2 讲「死锁是怎么形成的」：

> 注意到main()线程是先调用Inventory::printAll(#6)再调用Request::print(#5)，而threadFunc()线程是先调用Request::～Request(#6)再调用Inventory::remove(#5)。这两个调用序列对两个mutex的加锁顺序正好相反，于是造成了经典的死锁。

讲义 §2.1.2 讲「怎么查」：

> 通过gdb查看两个线程的函数调用栈，我们发现两个线程都等在mutex上（__lll_lock_wait），估计是发生了死锁。因为一个程序中的线程一般只会等在condition variable上，或者等在epoll_wait上。

讲义 §2.1.2 讲「怎么修」：

> 解决死锁的办法很简单，要么把print()移出printAll()的临界区，这可以用§2.8介绍的办法；要么把remove()移出～Request()的临界区。

## 讨论点

1. **`02` 与 `03` 只差一个约定。** 两份代码里加的是同样的两把锁、同样的调用；唯一的差别是
   全局约定「inventory_mu 永远先于 request_mu」。这就是讲义说的
   **「给所有锁定一个全局顺序，全局顺序一致 ⇒ 不可能成环」**。
2. **`02` 的死锁判定没有靠 gdb。** 用 `std::async` + `future::wait_for` 超时判定，
   复现了讲义那句「一个程序中的线程一般只会等在 condition variable 上，或者等在 epoll_wait 上」
   的启发式——线程卡在 `mutex.lock()` 上而不是卡在等事件上，就是死锁的形状。
3. **`04` 演示的是「失败模式换了一个」。** 递归锁让死锁消失了，但 `push_back` 在遍历期间
   让 `vector` 迭代器失效，代价从「确定性的死锁」变成「十万分之一概率的崩溃」。
   讲义说的「留个全尸，让验尸更容易」，讲的就是这个取舍。
4. **`04` 是 UB 文件。** 它用 `std::vector` 的迭代器在扩容后继续自增，ASan/valgrind 会
   报出越界读（`heap-use-after-free` / `Invalid read`）。默认构建跳过。

## 跑法

```bash
bash tools/run_all.sh          # 跳过 ub 文件, 01/02/03 都会跑
bash tools/run_all.sh --ub     # 连 04 一起跑, 期望检测器报错
```

单独跑：

```bash
g++ -std=c++20 -O2 -Wall -Wextra -pthread \
  books/linux-multithread-server/deadlock-lock-order/02-mutual-deadlock.cpp -o /tmp/dd
/tmp/dd
```

`02` 会在 500ms 后打印 `DEADLOCK DETECTED` 并**正常退出**（退出码 0），不会卡死进程。
