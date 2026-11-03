# 深入理解计算机系统 (CSAPP)

> 讲义来源：`书籍/教材讲义/深入理解计算机系统/`（第 1~12 章 + 附录A，共 13 篇讲义）
> 原书：Randal E. Bryant, David R. O'Hallaron, *Computer Systems: A Programmer's Perspective* (3rd ed., 2016)

本目录是**对照讨论式**实验集：每个主题目录只问**一个问题**，
给出 2~4 个**各自独立可运行**的实现文件，跑一遍就能横向比出差异。
所有结论都出自上面那 13 篇讲义，讲义原话放在各主题的 `README.md` 里，代码里不放引文。

## 主题目录

| 目录 | 讨论的问题 | 来源章节 | 文件数 |
| --- | --- | --- | --- |
| `integer-overflow/` | 有符号溢出与无符号回绕的差别；隐式转无符号如何让比较结果反转 | 第2章 §2.2.5 §2.2.8 §2.3 | 4 |
| `float-precision/` | float 与 double 的精度差；为什么浮点不能用 `==` 比较 | 第2章 §2.4.4 §2.4.5 §2.4.6 | 3 |
| `pass-by-value-vs-pointer/` | 传值与传指针的拷贝代价、栈帧"按需付费" | 第3章 §3.7 §3.7.3 §3.7.4 | 3 |
| `branch-vs-cmov/` | 同一个条件表达式，编译器可以翻成跳转也可以翻成条件传送 | 第3章 §3.6.5 §3.6.6 | 3 |
| `cache-locality/` | 访问顺序与数据布局如何决定不命中率 | 第6章 §6.5 §6.6.1 §6.6.3 | 4 |
| `virtual-memory-alloc/` | malloc 与 mmap 给出的地址有什么不同 | 第9章 §9.8.4 §9.9.1 §9.11.9 | 3 |
| `thread-vs-process/` | 进程与线程的创建开销；共享变量要不要加锁 | 第12章 §12.1 §12.5 §12.6 | 4 |

合计 24 个实现文件 + 8 个 README。

## 跑法

```bash
cd markdown/lab
bash tools/run_all.sh          # 编译并运行全部, 跳过标了 @ub: yes 的两个文件
bash tools/run_all.sh --ub     # 连 @ub 一起跑, 期望检测器报出未定义行为
python3 -I tools/lab_check.py  # 目录结构 / 元信息 / 颗粒度 三项硬约束
bash tools/build_all.sh        # 只做语法检查
```

单个文件自己跑：

```bash
cd books/csapp/integer-overflow
gcc -std=c11 -O2 -Wall -Wextra 01-signed-overflow.c -o /tmp/01 && /tmp/01
```

## 约定

每个实现文件开头四行是元信息，守卫 `tools/lab_check.py` 会检查：

```
// @book: 深入理解计算机系统
// @topic: <主题目录名>
// @source: <讲义文件名> <小节号>
// @expect: <预期现象>
```

- 故意演示未定义行为的文件再加一行 `// @ub: yes`（本目录：`integer-overflow/04`、`virtual-memory-alloc/03`）。
- 单文件 ≤ 120 行，只验证一个机制。
- C 用 `gcc -std=c11 -O2 -Wall -Wextra`；C++ 用 `g++ -std=c++20 -O2 -Wall -Wextra -pthread`；目标是零警告。
- 讲义原话只放在 `README.md`，代码里不放引文。

## 平台说明

示例以 **Linux** 为主要目标（讲义和原书都是这个语境），但尽量做成了两边都能跑的：

| 文件 | Linux | Windows |
| --- | --- | --- |
| `virtual-memory-alloc/02-mmap-address.c` | `mmap` | `VirtualAlloc`（同样是页对齐的新区域） |
| `thread-vs-process/01-fork-vs-thread.c` | `fork` + `waitpid` | 打印一行提示就退出（Windows 没有 fork，不假装给出数字） |

另外 `thread-vs-process/04-racy-counter.cpp` 是**故意**的数据竞争，
但它**没有**标 `@ub: yes`：竞态要 ThreadSanitizer 才查得出，
valgrind/ASan 查不出，标上会让 `run_all.sh --ub` 判成假失败。详见该目录 README。

## 本批未覆盖

任务里列出的对照点还有几个没做，如实记在这里，不脑补：

- **第5章 循环优化 / 向量化**：只在 `cache-locality/04` 里带了"编译器敢不敢向量化"这一角
  （volatile 累加器作为优化阻塞点），没有单独做 `-O0` vs `-O2` 两版对照。
- **第7章 链接**：静态库 vs 动态库、`static` vs 外部链接的符号可见性，整章未做。
- **第10章 系统级 IO**：`read` vs `mmap` 读文件、缓冲 vs 无缓冲，整章未做。
- **附录A 错误处理**：`goto` error vs RAII/异常，整篇未做。
- **第2章 位运算 vs 除法/取模**、**第6章 重排循环提升空间局部性**（§6.6.2 的矩阵乘六版本）也未做。
