# 对象生命期：裸 this / 显式 shared_ptr / enable_shared_from_this

> 来源讲义：`书籍/教材讲义/Linux多线程服务端编程/第1章 线程安全的对象生命期管理（讲义）.md` §1.5、§1.11、§1.11.1

## 对照表

| 文件 | 回调里存的东西 | 对象何时析构 | 预期输出 |
| --- | --- | --- | --- |
| `01-raw-this-callback.cpp` | 裸 `this` 指针 | 外层作用域结束，**回调随即悬垂** | 回调再 `delete` 一次 → 双重释放（UB，检测器报错） |
| `02-shared-ptr-callback.cpp` | `shared_ptr` 副本 | 最后一个 `shared_ptr` 消失时 | 对象存活到回调销毁，**不悬垂** |
| `03-enable-shared-from-this.cpp` | `shared_from_this()` | 同上，且**控制块是同一个** | 打印 `use_count`，析构被推迟到回调销毁 |

## 教材原话

讲义 §1.5 讲「原始指针有何不妥」：

> 指向对象的原始指针（raw pointer）是坏的，尤其当暴露给别的线程时。Observable应当保存的不是原始的Observer*，而是别的什么东西，能分辨Observer对象是否存活。

讲义 §1.5 讲「空悬指针」：

> 有两个指针p1和p2，指向堆上的同一个对象Object，p1和p2位于不同的线程中（图1-1的左图）。假设线程A通过p1指针将对象销毁了（尽管把p1置为了NULL），那p2就成了空悬指针（图1-1的右图）。这是一种典型的C/C++内存错误。

讲义 §1.5 讲「一个更好的解决办法」：

> 慢着！这不正是引用计数型智能指针吗？

讲义 §1.11.1 讲「enable_shared_from_this」：

> 有办法，用enable_shared_from_this。这是一个以其派生类为模板类型实参的基类模板，继承它，this指针就能变身为shared_ptr。

讲义 §1.11.1 强调的前提条件：

> 万事俱备，可以让this摇身一变，化为shared_ptr<StockFactory>了。

> 这样一来，boost::function里保存了一份shared_ptr<StockFactory>，可以保证调用StockFactory::deleteStock的时候那个StockFactory对象还活着。

讲义 §1.11.1 给出的必须记住的限制：

> 注意一点，shared_from_this()不能在构造函数里调用，因为在构造StockFactory的时候，它还没有被交给shared_ptr接管。

讲义 §1.9 讲「再论 shared_ptr 的线程安全」：

> 虽然我们借shared_ptr来实现线程安全的对象释放，但是shared_ptr本身不是100％线程安全的。
>
> **它的引用计数本身是安全且无锁的，但对象的读写则不是**，因为shared_ptr有两个数据成员，读写操作不能原子化。

讲义 §1.9 的结论性提醒：

> **请注意，以上是shared_ptr对象本身的线程安全级别，不是它管理的对象的线程安全级别。**

## 讨论点

1. **三个文件解决的是同一个问题**：回调要在对象可能已经不在的时候被调用。
   差别只在「回调持有的是所有权还是借用」。
2. **`01` 与 `02` 的差别只有捕获方式**：`[p = stock.get()]` 是借用，
   `[s = stock]` 是所有权。借用在对象先死时必然悬垂——`01` 里回调甚至还会对同一个对象
   再 `delete` 一次，于是从「读到垃圾」升级成「双重释放」。
3. **`03` 演示的是「怎么在成员函数里拿到指向自己的 `shared_ptr`」。**
   直接写 `shared_ptr<Stock>(this)` 会**新开一个控制块**，与创建对象那个控制块互不相识，
   最终 double free。`enable_shared_from_this` 的做法是让对象内部记住
   「当初创建我的那个 `shared_ptr` 的控制块」，`shared_from_this()` 返回的是同一控制块的新 handle。
4. **`03` 也体现了讲义那句限制**：构造函数里不能调 `shared_from_this()`，
   因为那时控制块还没建立。

## 跑法

```bash
bash tools/run_all.sh          # 跳过 ub 文件, 02 和 03 会跑
bash tools/run_all.sh --ub     # 连 01 一起跑, 期望检测器报错
```
