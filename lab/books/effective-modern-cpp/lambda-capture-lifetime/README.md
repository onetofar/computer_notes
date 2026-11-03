# lambda 捕获的不是「变量」，是「还能活多久」

> 来源讲义：`Effective Modern C++/6. Lambda Expressions（讲义）.md` §0 开局、§1 Item 31、§2 Item 32

## 对照关系

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-capture-by-reference-dangling.cpp` | `[&]` 捕获的局部变量随作用域一起死，闭包却活着 | 闭包逃出作用域后被调用：读到已释放的堆内存，并把已被释放的资源再释放一次 → double free（`@ub: yes`） |
| `02-default-by-value-captures-this.cpp` | 成员函数里的 `[=]` 到底抓了什么 vs `init capture` | `[=]` 闭包 `sizeof=8`（抓的是 `this` 指针），改成员后闭包看到新值（`f1(10)=1`）；`[d=divisor]` 闭包 `sizeof=4`（抓的是值拷贝，`f2(10)=0`） |
| `03-init-capture-moves.cpp` | init capture 转移所有权 vs 按引用捕获不转移 | init capture：闭包是最后一个持有者，闭包活着对象就活着，闭包销毁对象跟着死；按引用：作用域一结束对象先死，闭包还在 → 悬垂 |
| `04-this-capture-dangling.cpp` | `[=]` 抓到的 `this` 是一份拷贝，对象死了就是悬垂指针 | 对象被 `unique_ptr` 销毁后调用闭包：读到已释放的堆内存，并把已被释放的资源再释放一次 → double free（`@ub: yes`） |

## 结论

教材原话：

> **Default by-reference capture can lead to dangling references.**

> **Long-term, it’s simply better software engineering to explicitly list the local variables and parameters that a lambda depends on.**

`[&divisor]` 和 `[&]` 的代码一样错，但可读性完全不同：前者里那个 `divisor` 是一根记忆锚，每次读到它你都会想起「这东西得比闭包活得久」。

> **Wrong. Completely wrong. Horribly wrong. Fatally wrong.**

这是讲义对「以为 `[=]` 抓了 `divisor`」的评价——`divisor` 是数据成员，**压根不能被捕获**；`[=]` 真正抓的是 `this` 指针的一份拷贝。`02` 文件用两条硬证据说明这件事：闭包 `sizeof` 是指针大小（不是 int 大小），而且改动成员之后闭包立刻看到新值。

> **Default by-value capture is susceptible to dangling pointers (especially this ), and it misleadingly suggests that lambdas are self-contained.**

按值抓一个指针，只是把指针的**值**拷进闭包，指向的东西的生命周期一点没变。

> **The scope on the left is that of the closure class.**

> **Use C++14’s init capture to move objects into closures.**

init capture 的 `=` 左右两边作用域不同：左边是闭包类的数据成员名，右边是定义处那个局部变量。`03` 文件用它把 `shared_ptr` 的所有权真正搬进闭包（用 `weak_ptr` 观察：闭包活着对象就活着）。

## 跑法

```bash
bash tools/run_all.sh          # 跳过 ub 文件
bash tools/run_all.sh --ub     # 连 ub 一起跑, 期望检测器报错
```

`01` 和 `04` 标了 `@ub: yes`：它们都是「先 use-after-free，再 double free」的形状——逃出去的闭包既读已被释放的堆内存，又把那块内存再释放一次。valgrind 会报 `Invalid read` + `Invalid free`，ASan 会报 `heap-use-after-free` + `attempting double-free`。

`02` 里的 `[=]` 在 C++20 下写成 `[=, this]`：C++20 起「`[=]` 隐式抓 `this`」被弃用，显式写出语义完全相同，且整个目录保持零警告。
