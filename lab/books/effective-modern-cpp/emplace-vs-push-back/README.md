# emplace 还是 insert：插入函数收「对象」，emplacement 函数收「构造参数」

> 来源讲义：`Effective Modern C++/8. Tweaks（讲义）.md` §2.1–2.4 Item 42

## 对照关系

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-insertion-takes-objects.cpp` | `push_back("xyzzy")` vs `emplace_back("xyzzy")` | `push_back`：`构造=1 移动=1 析构=1`（先造临时再搬进去）；`emplace_back`：`构造=1 移动=0 析构=0`（直接原地构造） |
| `02-direct-vs-copy-init.cpp` | emplacement 用直接初始化、insertion 用拷贝初始化 | `is_constructible_v<Widget,int>=true` 而 `is_convertible_v<Widget,int>=false` → `emplace_back(1)` 编译得过，`push_back(1)` 编译不过 |
| `03-emplace-assign-route.cpp` | `emplace_back`（构造路线）vs `emplace(begin)`（赋值路线） | `emplace_back`：`构造=1 移动赋值=0`；`emplace(begin)`：`构造=1 移动赋值=1` → emplacement 省掉临时的那点优势没了 |

## 结论

教材原话：

> **A temporary std::string object is created from the string literal "xyzzy".**

`push_back` 要的是 `std::string`，你给的是 `const char[6]`，编译器只好先生成一个临时 `std::string` 再交出去：构造临时 → 移动进容器 → 析构临时。真正需要的是中间那一步。

> **emplace_back does exactly what we desire: it uses whatever arguments are passed to it to construct a std::string directly inside the std::vector. No temporaries are involved:**

> **Insertion functions take objects to be inserted, while emplacement functions take constructor arguments for objects to be inserted.**

原理就这一句。`01` 文件把三次动作记成计数器：`push_back` 多出一次移动和一次析构。

> **For this code, few implementations will construct the added std::string into the memory occupied by vs[0] . Instead, they'll move-assign the value into place. But move assignment requires an object to move from, and that means that a temporary object will need to be created to be the source of the move.**

插到已有对象的位置上时走的是**赋值**路线——移动赋值需要一个「源对象」，于是又得造临时，emplacement 的优势消失。`03` 文件用「移动赋值」计数器把这条坐实。

> **Emplacement functions use direct initialization, which means they may use explicit constructors. Insertion functions employ copy initialization, so they can't.**

这就是 `02` 文件的全部：`emplace_back` 用直接初始化（允许 `explicit` 构造函数），`push_back` 用拷贝初始化（不允许）。两个 `static_assert` 把这条规则变成了编译期判据：`is_constructible_v` 走直接初始化所以为真，`is_convertible_v` 走拷贝初始化所以为假——`push_back(1)` 编译不过这件事，被写成了两个 trait。

## 跑法

```bash
bash tools/run_all.sh          # 跳过 ub 文件
bash tools/run_all.sh --ub     # 连 ub 一起跑, 期望检测器报错
```

本目录三个文件都不含故意未定义行为，普通构建即可全部运行。
