# 用对象管理资源：RAII、shared_ptr，以及那道泄漏窗口

> 来源讲义：`Effective C++/Item 13 使用对象管理资源（讲义）.md`（Item 13）、
> `Effective C++/Item 17 在一个独立的语句中将 new 出来的对象存入智能指针（讲义）.md`（Item 17）

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-raw-pointer-manual-delete.cpp` | 裸指针 + 手动 `delete` | acquire 与 delete 之间一旦抛出异常，delete 被跳过：**created=1 destroyed=0**（泄漏） |
| `02-raii-unique-ptr.cpp` | 同样的异常，但资源由 `unique_ptr` 管 | 栈展开时析构照样跑：**created=1 destroyed=1**（无泄漏） |
| `03-shared-ptr-refcount.cpp` | 共享所有权（RCSP）+ `get()` 取裸资源 | `use_count` 依次为 1 → 2 → 2 → 1；最后一个 `shared_ptr` 离开作用域才 delete |
| `04-leak-window-in-one-statement.cpp` | 获取资源与「可能失败的操作」之间的间隔 | 场景 A（隔了一段代码）：**created=1 destroyed=0**；场景 B（独立语句接管）：**created=1 destroyed=1**；场景 C：教材原句**已不能编译** |

## 教材原话

> 依赖于 f 总能到达它的 delete 语句根本靠不住。

> 为了确保 createInvestment 返回的资源总能被释放，我们需要将那些资源放入一个类中，这个类的析构函数在控制流程离开 f 的时候会自动释放资源。

> 实际上，因为获取一个资源并在同一个语句中初始化资源管理对象是如此常见，所以使用对象管理资源的观念也常常被称为 Resource Acquisition Is Initialization (RAII)。

> 这个简单的例子示范了使用对象管理资源的两个重要的方面：
> >
> > · 获得资源后应该立即移交给资源管理对象。
> >
> > · 资源管理对象使用它们的析构函数确保资源被释放。

> 相对于 auto_ptrs，另一个可选方案是一个引用计数智能指针（reference-counting smart pointer, RCSP）。一个 RCSP 是一个智能指针，它能持续跟踪有多少对象指向一个特定的资源，并能够在不再有任何东西指向那个资源的时候删除它。

> 与垃圾收集不同的是，无论如何，RCSP 不能打破循环引用（例如，两个没有其它使用者的对象互相指向对方）。

> 作为最后的意见，我必须指出 createInvestment 的裸指针（raw pointer）的返回形式就是资源泄漏的请帖，因为调用者忘记在他们取回来的指针上调用 delete 实在是太容易了。

> 有两个常规方法来做这件事。显式转换和隐式转换。

> tr1::shared_ptr 和 auto_ptr 都提供一个 get 成员函数进行显示转换，也就是说，返回一个智能指针对象内部的裸指针（raw pointer）（或它的一个副本）。

> 在编译器能生成一个对 processWidget 的调用之前，它们必须传递实际参数来计算形式参数的值。

> C++ 编译器允许在一个相当大的范围内决定这三件事被完成的顺序。

> 由于一个异常可能插入资源创建的时间和将资源交给一个资源管理对象的时间之间，所以调用 processWidget 可能会发生一次泄漏。

> 在一个独立的语句中将 new 出来的对象存入智能指针。如果疏忽了这一点，当异常发生时，可能引起微妙的资源泄漏。

## 一处现代标准带来的差异（实测，不是记忆）

教材原版场景 C 写的是：

```cpp
processWidget(new Widget, priority());
```

在现代 C++（C++11 起）里，`std::shared_ptr` 的指针构造函数是 **explicit**，不再允许隐式转换，
所以这一行**已经不能编译**。`04-leak-window-in-one-statement.cpp` 用
`std::is_convertible_v<Widget*, std::shared_ptr<Widget>>` 把这件事变成了可观测的事实（实测为 `0`）。

C++17 之后，实参的初始化各自是完整的一段，因此「`new` 与交给智能指针之间插进一个失败的操作」
这个窗口在**一条语句**的写法里已经很难复现；真正还在的窗口是**跨语句**的那一种
——也就是场景 A：先 `new`，中间做一件可能失败的事，最后才交给智能指针。
讲义里那句「这样做是因为编译器在不同的语句之间重新安排操作顺序的活动余地比在一个语句之内要小得多」
说的正是这个区别。

## 跑法

在 `lab/` 目录下：

```bash
bash tools/run_all.sh          # 跳过 ub 文件
bash tools/run_all.sh --ub     # 连 ub 一起跑, 期望检测器报错
```

`01` 与 `04` 的场景 A 会**故意泄漏**一次，所以文件末尾都补了一句手动回收，
免得污染进程；泄漏本身已经由 `created/destroyed` 的差值记录下来了。
