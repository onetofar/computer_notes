# 成员初始化：赋值 vs 初始化列表，顺序由声明顺序决定

> 来源讲义：`Effective C++/Item 4 确保 objects（对象）在使用前被初始化（讲义）.md`（Item 4）

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-assignment-in-body.cpp` | 构造函数体内**赋值**：`Name` 先被默认构造出来，再被赋值覆盖 | 多出一次 `[Name] default ctor`，之后才是 `[Name] copy assign` |
| `02-member-init-list.cpp` | 成员**初始化列表**：`Name` 直接从实参拷贝构造 | 只有 `[Name] copy ctor`，没有 default ctor；顺序是 基类 → 成员(声明序) → 构造函数体 |
| `03-init-list-order-mismatch.cpp` | 初始化列表写成与声明顺序**相反**（`b_` 在前、`a_` 在后） | 实际仍按**声明顺序**执行：`[Slot 1]` 先于 `[Slot 2]` |

三份文件都在文件头标注了 `@source`，可逐条回查讲义。

## 教材原话

> C++ 的规则规定一个 object（对象）的 data members（数据成员）在进入 constructor（构造函数）的函数体之前被初始化。在 ABEntry 的 constructor（构造函数）内，theName，theAddress 和 thePhones 不是 being initialized（被初始化），而是 being assigned（被赋值）。

> 这个 constructor（构造函数）的最终结果和前面那个相同，但是通常它有更高的效率。assignment-based（基于赋值）的版本会首先调用 default constructors（缺省构造函数）初始化 theName，theAddress 和 thePhones，然而很快又在 default-constructed（缺省构造）的值之上赋予新值。那些 default constructions（缺省构造函数）所做的工作被浪费了。

> C++ 并非变幻莫测的方面是一个 object（对象）的数据被初始化的顺序。这个顺序总是相同的：base classes（基类）在 derived classes（派生类）之前被初始化（参见 Item 12），在一个 class（类）内部，data members（数据成员）按照它们被声明的顺序被初始化。

> **即使它们在 member initialization list（初始化列表）中以一种不同的顺序排列（这不幸合法），这依然是成立的。**

> 为了避免读者混淆，以及一些模糊不清的行为引起错误的可能性，initialization list（初始化列表）中的 members（成员）的排列顺序应该总是与它们在 class（类）中被声明的顺序保持一致。

> 为了避免记忆什么时候 data members（数据成员）必须在 member initialization list（成员初始化列表）中初始化，而什么时候又是可选的，最简单的方法就是**总是使用 initialization list（初始化列表）**。它有时是必须的，而且它通常都比 assignments（赋值）更有效率。

## 跑法

在 `lab/` 目录下：

```bash
bash tools/run_all.sh          # 跳过 ub 文件
bash tools/run_all.sh --ub     # 连 ub 一起跑, 期望检测器报错
```

单独跑某个文件：

```bash
bash tools/check_one.sh books/effective-cpp/member-init-order/03-init-list-order-mismatch.cpp
```

`03-init-list-order-mismatch.cpp` 用局部 `#pragma GCC diagnostic ignored "-Wreorder"` 局部关掉了编译器自己的告警，
以保证整个 lab 零警告构建。想看 GCC 的原始诊断，去掉那两行 pragma 再编译即可。
