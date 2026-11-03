# 一次要拿几把锁：lock_guard / std::lock / scoped_lock / unique_lock

> 来源讲义：`C++ Concurrency in Action/Chapter 3. Sharing data between threads（讲义）.md` §3.2.1-3.2.6

同一个问题：**要同时保护两个对象时, 怎么加锁才不会死锁？**
`02` 和 `03` 是**同一段 transfer 逻辑**, 只换加锁机制, 方便直接对比。

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-lock-guard-single.cpp` | 只锁一把: `lock_guard` 的作用域就是临界区, 出作用域自动解锁 | `lock_guard: value=1000` |
| `02-std-lock-adopt.cpp` | 锁两把: `std::lock` 一次拿全, 再用 `adopt_lock` 让两个 `lock_guard` 接管所有权 | `std::lock: x=0 y=0 (两者之和应为 0)` |
| `03-scoped-lock-multi.cpp` | 同一段逻辑, 换成 C++17 的 `std::scoped_lock`（变参模板, 析构时全部解锁） | `scoped_lock: x=0 y=0 (两者之和应为 0)` |
| `04-unique-lock-defer.cpp` | `unique_lock`: 构造时 `defer_lock` 不加锁, 之后自己 `lock()/`unlock()`, 临界区外的活儿不占着锁 | `构造后 owns_lock=false` → `lock() 后 owns_lock=true` → `unlock() 后 owns_lock=false` → `counter=2` |

## 讨论点

1. **固定加锁顺序会被参数顺序反转打败**：`02`/`03` 里两个线程分别是 `transfer(x,y)` 和 `transfer(y,x)`——
   这正是原文点名的死锁形状。`std::lock`/`scoped_lock` 用「一次拿全」的算法把它变成不可能。
2. **`std::lock` 是 all-or-nothing**：拿到第一把、第二把失败时, **第一把自动释放**。
   这就是它能当异常安全基座的原因。
3. **`adopt_lock` 的语义是「锁已经在我手上, 你只管接管」**：不接管就会二次加锁,
   对同一把 mutex 二次加锁是 UB。
4. **`unique_lock` 是拿灵活性换体积**：它多存一个「我现在到底拥不拥有这把锁」的标志位,
   所以比 `lock_guard` 更大更慢。作者的忠告很直接：**能用 `scoped_lock` 就别用 `unique_lock`**。

## 教材原话

> if a fixed order is chosen (for example, the mutex for the instance supplied as the first parameter, then the mutex for the instance supplied as the second parameter), this can backfire: all it takes is for two threads to try to exchange data between the same two instances with the parameters swapped, and you have deadlock!

> the C++ Standard Library has a cure for this in the form of std::lock —a function that can lock two or more mutexes at once without risk of deadlock.

> std::lock provides all-or-nothing semantics with regard to locking the supplied mutexes.

> The existence of std::scoped_lock means that most of the cases where you would have used std::lock prior to C++17 can now be written using std::scoped_lock , with less potential for mistakes, which can only be a good thing!

> Although std::lock (and std::scoped_lock<> ) can help you avoid deadlock in those cases where you need to acquire two or more locks together, it doesn’t help if they’re acquired separately.

> std::unique_lock takes more space and is slightly slower to use than std::lock_guard

> Unless you’re going to be transferring lock ownership around or doing something else that requires std::unique_lock , you’re still better off using the C++17 variadic std::scoped_lock if it’s available to you

## 跑法

```bash
cd <vault>/markdown/lab
bash tools/run_all.sh
# 单个文件:
bash tools/check_one.sh books/cpp-concurrency-in-action/multi-lock-acquisition/02-std-lock-adopt.cpp
```
