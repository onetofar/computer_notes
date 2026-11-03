# 编译器隐式生成的函数 vs 显式拒绝

> 来源讲义：`Effective C++/Item 5 了解 C++ 为你偷偷地加上和调用了什么函数（讲义）.md`（Item 5）、
> `Effective C++/Item 6 如果你不想使用 compiler-generated functions（编译器生成函数），就明确拒绝（讲义）.md`（Item 6）

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-implicit-generated.cpp` | 一个用户声明的成员函数都没有 | 四个隐式函数全部存在且为 public：`is_default_constructible / is_copy_constructible / is_copy_assignable / is_destructible` 全为 1，`sizeof == 1` |
| `02-declaring-one-stops-rest.cpp` | 用户声明了拷贝构造 vs 声明了移动构造 | 声明拷贝构造 → 移动构造/移动赋值不再隐式生成（移动一个对象实际发生 **1 次拷贝、0 次移动**）；声明移动构造 → 拷贝构造/拷贝赋值直接**不存在**（trait 为 0） |
| `03-uncopyable-private-base.cpp` | Item 6 的 `Uncopyable` 基类（private 继承，拷贝操作 private 且不实现） | 拷贝构造/拷贝赋值/移动构造/移动赋值**全部**为 0 —— 不可拷贝，也不可移动 |
| `04-explicit-delete.cpp` | C++11 的 `= delete`：只禁拷贝，移动用 `= default` 保留 | 拷贝相关为 0，**移动相关为 1**，且真的能移动一个 `HomeForSale` |

对照点最锋利的地方是 `03` 与 `04`：**同样是「拒绝拷贝」，Uncopyable 基类把移动一起掐死了，
`= delete` 可以只禁拷贝而保留移动。**

## 教材原话

> 一个 empty class（空类）什么时候将不再是 empty class（空类）？答案是当 C++ 搞定了它。

> 所有这些函数都被声明为 public 和 inline（参见 Item 30）。

> 这些函数只有在它们被需要的时候才会生成，但是并不需要做太多的事情，就会用到它们。

> 注意，生成的 destructor（析构函数）是 non-virtual（非虚拟）的（参见 Item 7）

> 面对这个难题，C++ 拒绝编译代码。

> 这个策略对于 copy constructor（拷贝构造函数）和 copy assignment operator（拷贝赋值运算符）不起作用，因为，就象 Item 5 中指出的，如果你不声明它们，而有人又想调用它们，编译器就会替你声明它们。

> 解决这个问题的关键是所有的编译器生成的函数都是 public（公有）的。为了防止生成这些函数，你必须自己声明它们，但是你没有理由把它们声明为 public（公有）的。相反，应该将 copy constructor（拷贝构造函数）和 copy assignment operator（拷贝赋值运算符）声明为 private（私有）的。

> 除非你十分聪明地不 define（定义）它们。那么，当有人不小心地调用了它们，在 link-time（连接时）会出现错误。

> 将 link-time error（连接时错误）提前到编译时间也是可行的（早发现错误毕竟比晚发现好）

> 这些函数的 compiler-generated versions（编译器生成版）会试图调用 base class（基类）的相应函数，而这些调用将被拒绝，因为在 base class（基类）中，拷贝操作是 private（私有）的。

> 为了拒绝编译器自动提供的机能，将相应的 member functions（成员函数）声明为 private，而且不要给出 implementations（实现）。使用一个类似 Uncopyable 的 base class（基类）是方法之一。

## 跑法

在 `lab/` 目录下：

```bash
bash tools/run_all.sh          # 跳过 ub 文件
bash tools/run_all.sh --ub     # 连 ub 一起跑, 期望检测器报错
```
