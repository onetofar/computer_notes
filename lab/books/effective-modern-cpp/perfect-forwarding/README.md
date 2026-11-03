# 完美转发：std::forward vs std::move，以及万能引用的代价

> 来源讲义：`Effective Modern C++/5. Rvalue References, Move Semantics, and Perfect Forwarding（讲义）.md` §1 Item 23、§3 Item 25、§4 Item 26–27、§5 Item 28；`Effective Modern C++/6. Lambda Expressions（讲义）.md` §3 Item 33

## 对照关系

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-forward-is-conditional.cpp` | 同一个 `T&&` 形参上 `std::move` 与 `std::forward` 的区别 | 传左值：`move` 后是右值（0）、`forward` 后是左值（1）；传右值：两者都是右值。再加一条：无脑 `std::move` 会把调用方的对象掏空（`tag=""`） |
| `02-universal-reference-hijack.cpp` | 完美转发构造 vs 拷贝构造的匹配优先级 | 拷贝一个非 const `Person` → 走模板构造（`模板=1 拷贝=0`），名字被改写成 `Person(Alice)`；拷贝一个 const `Person` → 才走拷贝构造（`模板=0 拷贝=1`） |
| `03-reference-collapsing.cpp` | 引用折叠的四种组合 + `std::forward` 的返回类型 | `int& &`/`int& &&`/`int&& &` 全折成 `int&`，只有 `int&& &&` 折成 `int&&`；实参是左值时 `T` 推成引用，是右值时 `T` 推成非引用 |
| `04-generic-lambda-forwarding.cpp` | 通用 lambda 按值收参 vs `auto&&` + `forward<decltype(x)>` | `byValue(右值)` → `c-lvalue`（右值退化成了左值）；`forwarded(右值)` → `d-rvalue`（右值原样传下去） |

## 结论

教材原话：

> **Apply std::move to rvalue references and std::forward to universal references the last time each is used.**

> **std::forward casts its argument to an rvalue only if that argument is bound to an rvalue.**

一句话：`std::move` = 「无条件当右值」，`std::forward` = 「原来是右值才当右值」。而且都要**最后一次使用时**才转——`01` 文件里那个 `steal` 就是反面教材：调用方传左值时，它把调用方的对象掏空了。

> **Functions taking universal references are the greediest functions in C++.**

> **Overloading on universal references almost always leads to the universal reference overload being called more frequently than expected.**

万能引用几乎能给任何实参推出精确匹配，所以重载时它总是赢。`02` 文件把讲义里那个「完美转发构造劫持拷贝构造」的坑做成了可运行版本：对非 const 左值，模板构造匹配得更好，拷贝构造根本没被调用——讲义原版这里的表现是**编译不过**，这里改成「编译过了但名字被改写」，好让两种路径都能被观察到。

> **When an lvalue is passed as an argument, T is deduced to be an lvalue reference. When an rvalue is passed, T is deduced to be a non-reference.**

注意这里的**不对称**：左值推成 `T&`，右值推成 `T`（不是引用）。`03` 文件用类型 trait 把这条差异打了出来。

> **If either reference is an lvalue reference, the result is an lvalue reference. Otherwise (i.e., if both are rvalue references) the result is an rvalue reference.**

折叠只发生在模板实例化、`auto` 类型生成、别名声明、`decltype` 这四个语境——`std::forward` 正是靠它工作的：`forward<int&>` 折成左值引用（转换无效），`forward<int>` 折成右值引用。

> **Use decltype on auto&& parameters to std::forward them.**

通用 lambda 里没有 `T` 可写，答案就是 `decltype(x)`：实参是左值时它给出左值引用类型，实参是右值时给出右值引用类型，而右值引用引右值引用折叠成单个右值引用——所以两边都合法。`04` 文件直接看两个 `normalize` 重载各自被命中。

## 跑法

```bash
bash tools/run_all.sh          # 跳过 ub 文件
bash tools/run_all.sh --ub     # 连 ub 一起跑, 期望检测器报错
```

本目录四个文件都不含故意未定义行为，普通构建即可全部运行。
