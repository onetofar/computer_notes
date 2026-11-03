# 四种锁封装：lock_guard / unique_lock / scoped_lock / std::lock

> 来源讲义：`书籍/教材讲义/Linux多线程服务端编程/第2章 线程同步精要（讲义）.md` §2.1、§2.1.2、§2.4

## 对照表

| 文件 | 锁的形态 | 能力 | 预期输出 |
| --- | --- | --- | --- |
| `01-lock-guard-basic.cpp` | `std::lock_guard` | 最小可用：构造即加锁，析构即解锁 | 临界区随作用域结束，计数正确 |
| `02-unique-lock-deferred.cpp` | `std::unique_lock` | 可 `defer_lock` / `try_lock` / 中途 `unlock` | 打印 `owns_lock()` 在各阶段的变化 |
| `03-scoped-lock-two-mutex.cpp` | `std::scoped_lock` | 一次拿两把锁，内部死锁避免 | 两线程反序请求也 `NO DEADLOCK` |
| `04-std-lock-two-mutex.cpp` | `std::lock` | 一次拿两把锁，**但释放要自己写** | 两线程反序请求也 `NO DEADLOCK` |

## 教材原话

讲义 §2.1 讲 mutex 的四条主要原则：

> **（1）用 RAII 手法封装 mutex 的创建、销毁、加锁、解锁这四个操作。**

> **（2）只用非递归的 mutex（即不可重入的 mutex）。**

> **（3）不手工调用 `lock()` 和 `unlock()` 函数，一切交给栈上的 Guard 对象的构造和析构函数负责。**

> **（4）在每次构造 Guard 对象的时候，思考一路上（调用栈上）已经持有的锁，防止因加锁顺序不同而导致死锁（deadlock）。**

讲义 §2.1 对第 (3) 条的理由：

> Guard对象的生命期正好等于临界区（分析对象在什么时候析构是C++程序员的基本功）。这样我们保证始终在同一个函数同一个scope里对某个mutex加锁和解锁。避免在foo()里加锁，然后跑到bar()里解锁；也避免在不同的语句分支中分别加锁、解锁。这种做法被称为Scoped Locking。

讲义 §2.4 讲「三个 class 都不允许拷贝构造和赋值」：

> 这几个class都不允许拷贝构造和赋值。

讲义 §2.4 讲「Condition class：老老实实一对一」：

> Pthreads condition variable允许在wait()的时候指定mutex，但是我想不出有什么理由一个condition variable会和不同的mutex配合使用。Java的intrinsic condition和Condition class都不支持这么做，因此我觉得可以放弃这一灵活性，老老实实地一对一好了。

讲义 §2.4 批评 `boost::thread` 同步原语庞杂时列出的清单：

> ·Concept有四种Lockable、TimedLockable、SharedLockable、UpgradeLockable。

> ·Lock有六种：lock_guard、unique_lock、shared_lock、upgrade_lock、upgrade_to_unique_lock、scoped_try_lock。

> ·Mutex有七种：mutex、try_mutex、timed_mutex、recursive_mutex、recursive_try_mutex、recursive_timed_mutex、shared_mutex。

讲义 §2.4 的落点：

> 提供灵活性固然是本事，然而在不需要灵活性的地方把代码写死，更需要大智慧。

## 讨论点

1. **四个文件对应讲义批评的那份「庞杂清单」的最小子集。**
   C++11 采纳了 `boost::thread` 的方案（`lock_guard`/`unique_lock`/`scoped_lock`/`std::lock` 都在其中），
   讲义作者本人更愿意自己写几个「简简单单的一看就明白的 class」。
2. **`02` 展示了 `unique_lock` 多出来的那点能力：中途放手。**
   `lock_guard` 做不到——它的临界区长度等于作用域长度。
   讲义 §2.4 那张表里 `unique_lock` 属于「Lock 有六种」之一，能力多但心智负担也大。
3. **`03` 与 `04` 是同一件事的两种写法。** `scoped_lock` 是 RAII 对象（析构自动解锁），
   `std::lock` 是函数（**返回时锁还持有着**，必须自己 `unlock`）。
   两者内部都用死锁避免算法，所以反序加锁都不会死。
4. **这四个文件都不含数据竞争**，ASan/UBSan 干净退出是预期结果。

## 跑法

```bash
bash tools/run_all.sh
```
