# auto 的推导边界：四种写法，四种类型

> 来源讲义：`Effective Modern C++/1. Deducing Types（讲义）.md` §1 Item 1、§2 Item 2、§3 Item 3

同一个对象，写法不同，推出来的类型就不同。这一组把讲义里「滤镜有哪几种」逐条变成可运行、可横向对照的证据。

## 对照关系

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-auto-vs-explicit-type.cpp` | 同一个左值上 `auto` / `const auto&` / `decltype(auto)` / 显式类型 | `auto`→`Widget`（拷贝，const 被吃掉）；`const auto&`→`const Widget&`；`decltype(auto)`→`Widget&`（保留引用，改它就是改原对象）；显式→`Widget` |
| `02-array-decay.cpp` | 数组按值接（退化成指针）vs 按引用接（保留数组类型） | `auto`→`sizeof=8`（`const char*`）；`auto&`→`sizeof=13`（`const char[13]`）；`arraySize(keyVals)=7` |
| `03-braced-initializer-list.cpp` | `auto` + 花括号 → `initializer_list`；显式类型不受影响 | `auto x={27}`→`initializer_list<int>`（1 个元素）；`int x={27}`→`int`；`auto x={1,2,3}`→3 个元素 |
| `04-decltype-paren-dangling.cpp` | `decltype(x)` vs `decltype((x))`：多一对括号就多一个引用 | `decltype(x)=int`、`decltype((x))=int&`；`byParen()` 返回的引用指向已释放的堆内存，再释放一次 → double free（`@ub: yes`） |

## 结论

教材原话：

> **The type deduced for T is dependent not just on the type of expr, but also on the form of ParamType.**

模板推导有两个输入：实参的类型，和 `ParamType` 的写法。`auto` 只是把两者换成了「初始化器」和「变量的类型说明符」，规则原样搬过来。

> **During template type deduction, arguments that are array or function names decay to pointers, unless they're used to initialize references.**

数组按值接就退化成指针，按引用接才保住数组类型——`arraySize` 这个工具正是靠「按引用接」把长度变成类型的一部分。

> **auto type deduction is usually the same as template type deduction, but auto type deduction assumes that a braced initializer represents a std::initializer_list, and template type deduction doesn't.**

`auto` 多出一条自己的规则：初始化器被花括号包住时，推出来的是 `std::initializer_list`。模板推导没有这条，所以 `f({1,2,3})` 根本推不出 `T`。

> **For lvalue expressions of type T other than names, decltype always reports a type of T&.**

> **Putting parentheses around a name can change the type that decltype reports for it!**

`decltype(x)` 是 `int`，`decltype((x))` 是 `int&`。配合 `decltype(auto)`，这就是讲义说的那趟「开往未定义行为的特快列车」——`04` 文件里的 `byParen()` 就是它：把局部变量的引用带出了函数。

> **auto in a function return type or a lambda parameter implies template type deduction, not auto type deduction.**

`auto` 当返回类型时规则又换了一套（`03` 文件末尾注释里那两行就是这条）。

## 跑法

```bash
bash tools/run_all.sh          # 跳过 ub 文件
bash tools/run_all.sh --ub     # 连 ub 一起跑, 期望检测器报错
```

`04-decltype-paren-dangling.cpp` 标了 `@ub: yes`：它先读一块已被 `unique_ptr` 释放的堆内存（use-after-free），再透过那个悬垂引用删一次（double free）。valgrind 会报 `Invalid read` + `Invalid free`，ASan 会报 `heap-use-after-free` + `attempting double-free`。
