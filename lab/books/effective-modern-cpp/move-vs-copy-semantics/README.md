# 移动语义 vs 拷贝语义：std::move 到底做了什么

> 来源讲义：`Effective Modern C++/5. Rvalue References, Move Semantics, and Perfect Forwarding（讲义）.md` §1 Item 23、§3 Item 25、§6 Item 29

## 对照关系

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-move-is-a-cast.cpp` | `std::move` 是「转换」还是「搬家」 | 只调用 `std::move` 后对象一点没变；真正构造时 `拷贝=0 移动=1`，源被掏空（`data=0x0`），目的接管同一块堆指针 |
| `02-const-eats-move.cpp` | 形参是 `const` 与否 | 参数非 const → `拷贝=0 移动=1`；参数是 const → `拷贝=1 移动=0`（`std::move` 被 const 吃掉，静默退回拷贝） |
| `03-sso-move-not-cheaper.cpp` | SSO 短串 vs 堆上的长串 | 短串移动前后 `data` 指针不变（没有堆指针可搬）；长串目的接管了源的堆指针、源被掏空 |
| `04-return-move-breaks-rvo.cpp` | `return w` vs `return std::move(w)` | `return w` → `拷贝=0 移动=0`（走 NRVO）；`return std::move(w)` → `移动=1`（挡掉了 NRVO） |

## 结论

教材原话：

> **std::move performs an unconditional cast to an rvalue. In and of itself, it doesn’t move anything.**

> **Neither std::move nor std::forward do anything at runtime.**

`std::move` 生成的机器码是零条——它只是告诉编译器「把 `a` 当成右值看待」。真正搬东西的是**被选中的那个构造函数**，所以 `01` 文件把计数器挂在构造函数上，而不是挂在 `std::move` 上。

> **The only thing you know for sure about the result of applying std::move to an object is that it’s an rvalue.**

仅此而已：它不保证发生移动，也不保证移动便宜，更不保证源对象变空。

讲义点名的那个「静悄悄的失败」由这条规则直接推出：移动构造不接受 `const` 右值，于是 `std::move` 一个 `const` 对象时**静默退回拷贝**——编译通过、行为正确、性能少一半，而且没有任何警告。`02` 文件把这个退回过程量化成 `拷贝=1 移动=0`。

> **Never apply std::move or std::forward to local objects if they would otherwise be eligible for the return value optimization.**

`return std::move(w)` 把返回类型变成 `Widget&&`，NRVO 的条件就不成立，编译器**必须**真的构造一个临时对象再移动。`04` 文件的计数器把这次多余的移动量了出来。GCC/Clang 本身也会报 `-Wpessimizing-move`（文件里局部静音了，好让整个目录零警告）——编译器早就在提醒你这件事。

> **Assume that move operations are not present, not cheap, and not used.**

> **Moving small strings using an SSO-based implementation is no faster than copying them, because the copy-only-a-pointer trick that generally underlies the performance advantage of moves over copies isn’t applicable.**

`03` 文件把「移动不一定更便宜」变成可观测的指针比较：短串的 `data()` 移动前后不变——SSO 把字符内联在对象里，根本没有堆指针可以搬，于是移动和拷贝一样贵。

## 跑法

```bash
bash tools/run_all.sh          # 跳过 ub 文件
bash tools/run_all.sh --ub     # 连 ub 一起跑, 期望检测器报错
```

本目录四个文件都不含故意未定义行为，普通构建即可全部运行。
