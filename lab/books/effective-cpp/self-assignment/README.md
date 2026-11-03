# 自赋值安全，以及 `operator=` 该返回什么

> 来源讲义：`Effective C++/Item 11 在 operator= 中处理 assignment to self（自赋值）（讲义）.md`（Item 11）、
> `Effective C++/Item 10 让 assignment operators（赋值运算符）返回一个 reference to this（引向 this 的引用）（讲义）.md`（Item 10）

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-unsafe-self-assign.cpp`（`@ub: yes`） | Item 11 的原始写法：先 `delete` 再拷贝 | 自赋值时拷贝构造读到**已被 delete** 的对象（use-after-free）；程序自检并以 `runtime error:` 退出 |
| `02-identity-test.cpp` | 加 identity test（`if (this == &rhs) return *this;`） | 自赋值安全（bitmap 仍存活）；但**拷贝构造抛异常时成员指针悬垂**（存活=0） |
| `03-copy-and-swap.cpp` | copy-and-swap：`operator=(Widget rhs)` 按值传参 + swap | 不需要 identity test；自赋值安全；拷贝抛异常时**对象原样没动**（存活=1） |
| `04-return-this-by-reference.cpp` | 返回 `Widget&` vs 返回 `Widget` | 返回引用：`a=b=c` 期间 **0 次拷贝**，`(a=b)=c` 改的是 `a`；返回对象：**2 次拷贝**，`(a=b)=c` 改的是临时对象，`a` 不变 |

## 教材原话

> Widget 将以持有一个指向被删除的 Bitmap 的指针而告终。

> 如果两个或更多 objects（对象）相同，确保任何操作多于一个 object（对象）的函数行为正确。

> 这样的指针是有毒的，你不能安全地删除它们。你甚至不能安全地读取它们。

> 幸亏，使 operator= exception-safe（异常安全）一般也同时弥补了它的 self-assignment-safe（自赋值安全）。这就导致了更加通用的处理 self-assignment（自赋值）问题的方法就是忽略它，而将焦点集中于达到 exception safety（异常安全）。

> 甚至不需要 identity test（一致性检测），这里的代码也能处理 assignment to self（自赋值），因为我们做了一个原始 bitmap（位图）的拷贝，删除原始 bitmap（位图），然后指向我们作成的拷贝。

> 另一个可选的手动排列 operator= 中语句顺序以确保实现是 exception- and self-assignment-safe（异常和自赋值安全）的方法是使用被称为 "copy and swap" 的技术。

> 关于 assignments（赋值）的一件有意思的事情是你可以把它们穿成一串。

> 这里的实现方法是让 assignment（赋值）返回一个 reference to its left-hand argument（引向它的左侧参数的引用），而且这就是当你为你的 classes（类）实现 assignment operators（赋值运算符）时应该遵守的惯例：

> return *this; // return the left-hand object

## 跑法

在 `lab/` 目录下：

```bash
bash tools/run_all.sh          # 跳过 ub 文件
bash tools/run_all.sh --ub     # 连 ub 一起跑, 期望检测器报错
```

`01` 标了 `@ub: yes`，默认构建时会被跳过。
它在 Bitmap 的拷贝构造里先查「还有没有活着的 Bitmap」再读成员 ——
检测本身不依赖未定义行为，所以结论是确定的。
