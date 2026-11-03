# Linux Kernel Programming · 教材对照实验

讲义源：`书籍/教材讲义/Linux Kernel Programming/Chapter 1. Kernel Workspace Setup（讲义）.md`
（本书目前只整理出 **1 篇**讲义 —— 第 1 章「Kernel Workspace Setup」）
原书：Kaiwan N. Billimoria, *Linux Kernel Programming* (Packt, 2022)

本目录是**对照讨论式**实验集：每个主题目录只问**一个问题**，
给出 2~4 个**各自独立可运行**的实现文件，跑一遍就能横向比出差异。
教材原话放在各主题的 `README.md` 里，代码里不放引文。

## 这本书的讲义讲的不是内核机制，是「怎么把地板铺好」

第 1 章全部内容是**环境搭建**：虚拟机、工具链、静态分析器、trace 工具、eBPF 前置条件。
所以本目录**不写内核模块**（那要 root、要加载模块、要拿机器冒险），
而是把讲义里的每一条**前置条件 / 工具语义**搬到用户态，做成能跑、能横向比的对照实验。

| 主题目录 | 讨论的问题 | 讲义出处 | 文件数 |
| --- | --- | --- | --- |
| [`syscall-vs-library-api`](./syscall-vs-library-api/) | 「man 2 = 系统调用、man 3 = 库 API」这条界线，在用户态到底差多少 | §3.1 | 3 |
| [`static-analysis-report-vs-block`](./static-analysis-report-vs-block/) | 静态分析器会误报：报告式 / 阻断式 / 驯化式三种接法差在哪 | §3.4 | 4 |
| [`kernel-env-doctor`](./kernel-env-doctor/) | 「我这台机器能不能编译内核」怎么用一条命令问出来 | §1.2 §2.1 §2.2 §3.7 | 3 |

合计 **3 个主题目录 / 10 个实现文件** + 3 个主题 README。

## 约定

1. 每个实现文件头 4 行元信息：`// @book` `// @topic` `// @source` `// @expect`（`.py` / `.sh` 用 `#` 注释）
2. 单文件 ≤ 120 行，**只验证一个机制**
3. 故意演示未定义行为的文件加 `// @ub: yes`（本目录：`static-analysis-report-vs-block/04`），
   默认跑法跳过；`--ub` 才跑，期望检测器（valgrind）报出 UB
4. **教材引文只放 README，不进代码**
5. C 用 `gcc -std=c11 -O2 -Wall -Wextra`，C++ 用 `g++ -std=c++20 -O2 -Wall -Wextra -pthread`，零警告

## 跑法

```bash
cd markdown/lab

# 守卫: 目录结构 / 元信息 / 颗粒度
python3 -I tools/lab_check.py

# 语法检查(不产出可执行文件)
bash tools/build_all.sh

# 编译并运行全部(跳过 @ub: yes 的文件)
bash tools/run_all.sh

# 连故意演示 UB 的那个一起跑, 期望 valgrind 报出 Invalid free
bash tools/run_all.sh --ub

# 单个文件
bash tools/check_one.sh books/linux-kernel/syscall-vs-library-api/01-libc-clock-gettime.c
```

## 运行环境

- **验证环境**：服务器 `fedora` (192.168.1.13)，内核 `7.2.12-200.fc44.x86_64`，
  gcc/g++ 16.2.1，valgrind 可用；本批的编译 / 运行验证都在这台机器上做
- 本地 Windows 机器**没有** gcc/g++（`build_all.sh` 需要 PATH 里有编译器）
- 所有程序**只读**：不写 `/proc` `/sys`、不改系统状态、不需要 root、跑完自动退出
- 性能类数字（每次系统调用开销）**依机器而定**，能横向比的是**比值**

## 本批未覆盖（及原因）

| 讲义内容 | 为什么没做 |
| --- | --- |
| §2.1 装虚拟机（VirtualBox / BIOS 虚拟化） | 装虚拟机要改宿主 BIOS，实验场里做不到也不该做；只保留「虚拟化扩展在不在」这条**可观测**的判定（见 `kernel-env-doctor`） |
| §3.5 LTTng + Trace Compass | 要装 LTTng 内核模块并 trace，会改系统状态，与本批「只读」约束冲突 |
| §3.6 procmap（VAS 可视化） | 依赖 eBPF / 作者自研工具，而本机体检结论是「bpftrace、bcc 都没装」（见 `kernel-env-doctor/02`），不做假数据 |
| §3.8 LDV（规则 + 真实违规案例） | 需要拉内核源码树再跑 LDV，本批没有源码树 |
| §4.2 ctags / cscope 索引大代码库 | 需要一份内核源码树才有意义 |
| §3.3 内核文档（Sphinx / `make htmldocs`） | 需要内核源码树 |

讲义只整理出 1 篇，**没讲到的内容一律不补**：本目录所有结论都能回指到那 1 篇讲义的原文。
