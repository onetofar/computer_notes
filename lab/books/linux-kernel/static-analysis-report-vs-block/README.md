# 静态分析器：报告式 / 阻断式 / 驯化式

> 对照问题：讲义说静态分析器「会误报」，要学会「驯」它们。
> **同一套规则、同一个样本，三种接法分别长什么样？**

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| [01-report-mode.py](./01-report-mode.py) | **报告式**：扫出命中就打印，**永不阻断** | `疑似问题: 3 条` / `判定: 通过 (报告式永远不阻断流水线)` |
| [02-block-mode.py](./02-block-mode.py) | **阻断式**：任一命中即让流水线失败 | `疑似问题: 3 条` / `判定: 失败 (任一命中即阻断)` |
| [03-tamed-mode.py](./03-tamed-mode.py) | **驯化式**：人工确认过的误报进白名单，只对真阳性阻断 | `疑似问题: 3 条 -> 抑制 2 条, 剩余 1 条` / `判定: 失败 (剩余的 1 条是真阳性, 该阻断)` |
| [04-true-positive-double-free.c](./04-true-positive-double-free.c) | **真阳性的实物**：R1 命中那段代码真跑起来会崩（标了 `@ub: yes`，默认跳过） | 默认跑：`free(): double free detected in tcache 2`；`--ub` 跑：valgrind 报 `Invalid free() / delete / delete[] / realloc()` |

三个分析器用的是**同一套规则**（R1 双重释放 / R2 memcpy 未核长 / R3 结构体未清零），
扫的是**同一段内嵌 C 样本**，所以命中行号必须完全一致（第 14 / 21 / 23 行）。
**样本里 3 条命中：R1 是真阳性，R2、R3 是误报** —— 这就是讲义说的「会误报」。

> `02` / `03` 默认只打印判定、退出码仍是 0（好让三个实现能放在同一批里横着比）；
> 加 `--strict` 才是真的阻断（退出码 1）：
>
> ```bash
> python3 02-block-mode.py --strict; echo "退出码=$?"   # 退出码=1
> ```

## 教材原话

> **Static analysers are tools that, by examining the source code, attempt to identify potential errors within it. They can be tremendously useful to you as the developer, though you must learn how to "tame" them – in the sense that they can result in false positives.**
>
> —— 《Linux Kernel Programming》第 1 章讲义 §3.4「静态分析：三个内核专用 + 两个通用 + 三个商业」

> **Static analysis tools can save the day. Time spent learning to use them effectively is time well spent!**
>
> —— 《Linux Kernel Programming》第 1 章讲义 §3.4

> **只报不改**（脚本输出命中行，由人判断），这与 `make C=1 CHECK=...` 的默认行为一致：**报告，不阻断。**
>
> —— 《Linux Kernel Programming》第 1 章讲义 §4.1（讲义讲本仓静态检查脚本时写的）

> **但要注意差别：`make C=1 CHECK=...` 是每次编译都跑（嵌进流程）；本批的纯度脚本是回归时跑（独立一步）。**
>
> —— 同上；讲义紧接着补了一句：**嵌进流程的检查才是真检查** —— 因为它跑不掉。

## 跑法

```bash
cd markdown/lab
bash tools/check_one.sh books/linux-kernel/static-analysis-report-vs-block/01-report-mode.py
bash tools/check_one.sh books/linux-kernel/static-analysis-report-vs-block/02-block-mode.py
bash tools/check_one.sh books/linux-kernel/static-analysis-report-vs-block/03-tamed-mode.py

# 真阳性实物: 默认跳过(标了 @ub: yes), --ub 才跑, 期望 valgrind 报出 Invalid free
bash tools/check_one.sh books/linux-kernel/static-analysis-report-vs-block/04-true-positive-double-free.c --ub
```

## 讨论点

1. 三个实现跑同一段样本、同一套规则，**唯一变量是「命中之后怎么办」** —— 这就是「对照」的干净做法。
2. 误报不是 bug，是**能力边界**：R2 要跨函数知道长度上限，R3 要做字段级初始化分析。
   规则越简单，误报越多；规则越复杂，越容易漏真阳性。
3. `04` 是 R1 命中的那段代码的**可编译实物**：文本规则说「二次释放」，真跑起来确实崩。
   静态分析的价值就在这 —— 但**只有当它是真阳性时**才有价值。
4. 讲义 §3.4 把「会误报」写在工具介绍的**正前面**，不是附录。本组实验把这句话变成了可验证的：
   误报率高的规则集配上阻断式门禁，结果就是**门禁被稀释成没人看**（开发者学会了忽略它）。
5. 讲义 §4.1 还给了个更狠的判据：**嵌进流程的检查才是真检查，因为它跑不掉** ——
   报告式的检查（本目录 `01`）就属于「跑得掉、也没人看」的那一类。
