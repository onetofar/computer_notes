# 多态基类的虚析构 vs 非成员非友元便利函数

> 来源讲义：`Effective C++/Item 7 在 polymorphic base classes（多态基类）中将 destructors（析构函数）声明为 virtual（虚拟）（讲义）.md`（Item 7）、
> `Effective C++/Item 23 用非成员非友元函数取代成员函数（讲义）.md`（Item 23）

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-nonvirtual-dtor-partial-destroy.cpp`（`@ub: yes`） | 多态基类（有虚函数）却没有虚析构 | 经基类指针 `delete` → **派生析构根本没跑**，派生持有的堆内存泄漏；程序自检并以 `runtime error:` 退出 |
| `02-virtual-dtor-full-destroy.cpp` | 给基类加 `virtual` 析构 | 析构链完整：`[AtomicClock] dtor` → `[Resource] dtor` → `[TimeKeeper] dtor`，无泄漏 |
| `03-virtual-dtor-size-cost.cpp` | 无关地给非多态类加虚析构 | `sizeof(Point)=8` → `sizeof(PointWithVirtualDtor)=16`：每个对象多一个 vptr |
| `04-convenience-function-placement.cpp` | 便利函数放在成员 / 非成员非友元 / 友元 三处 | 成员与非成员非友元**行为完全一致**（`publicCalls=3 cache=1`）；友元多一条特权路径（`publicCalls=0 cache=0`） |

## 教材原话

> C++ 规定：当一个 derived class object（派生类对象）通过使用一个 pointer to a base class with a non-virtual destructor（指向带有非虚拟析构函数的基类的指针）被删除，则结果是未定义的。

> 运行时比较典型的后果是 derived part of the object（这个对象的派生部分）不会被析构。

> 消除这个问题很简单：给 base class（基类）一个 virtual destructor（虚拟析构函数）。

> 最终结果就是无故地将所有 destructors（析构函数）声明为 virtual（虚拟），和从不把它们声明为 virtual（虚拟）一样是错误的。实际上，很多人总结过这条规则：declare a virtual destructor in a class if and only if that class contains at least one virtual function（当且仅当一个类中包含至少一个虚拟函数时，则在类中声明一个虚拟析构函数）。

> 在一个 64-bit 架构中，它们可能从 64 bits 长到 128 bits，因为在这样的架构中指针的大小是 64 bits 的。

> polymorphic base classes（多态基类）应该声明 virtual destructor（虚拟析构函数）。如果一个 class（类）有任何 virtual functions（虚拟函数），它就应该有一个 virtual destructor（虚拟析构函数）。

> 假设在一个成员函数（能访问的不只是一个类的 private 数据，还有 private 函数，枚举，typedefs，等等）和一个提供同样功能的非成员非友元函数（不能访问上述那些东西）之间有一个选择，能获得更强封装性的选择是非成员非友元函数，因为它不会增加能访问类的 private 部分的函数的数量。

> 从封装的观点看，选择不是在成员和非成员函数之间，而是在成员函数和非成员非友元函数之间。

> 用非成员非友元函数取代成员函数。这样做可以提高封装性，包装弹性，和机能扩充性。

## 跑法

在 `lab/` 目录下：

```bash
bash tools/run_all.sh          # 跳过 ub 文件
bash tools/run_all.sh --ub     # 连 ub 一起跑, 期望检测器报错
```

`01` 标了 `@ub: yes`，默认构建时会被跳过，避免污染正常构建。
它自己会检测「派生析构有没有跑」，检测到就打印 `runtime error:` 并以退出码 1 结束，
所以在没装 valgrind 的机器上 `--ub` 也能给出确定结论。
`01` 另外用局部 `#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"` 关掉了 GCC 自己的告警，
以保证整个 lab 零警告构建 —— 那个告警恰恰就是本文件要演示的东西。
