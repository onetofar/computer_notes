# 操作系统导论 · 教材对照实验

> 讲义源：`书籍/教材讲义/操作系统导论/`（40 篇：第 2~49 章 + 附录 B）
> 配套原文稿：`书籍/教材原文/操作系统导论/`
> 每个主题目录 = **一个对照讨论**：同一个问题的 2~4 个独立实现，输出可直接横向对比。

## 主题索引

| 主题目录 | 对照问题 | 讲义出处 | 实现文件 |
| --- | --- | --- | --- |
| [thread-api-create](./thread-api-create/) | `pthread_create` vs `std::thread`：参数怎么递、结果怎么拿 | 第 27 章 §27.1–27.2、第 26 章 §26.1 | 2 |
| [lock-granularity](./lock-granularity/) | 一把大锁 vs 分片 + 懒惰转账：并发结构换了，结果不变 | 第 29 章 §29.1 | 2 |
| [condvar-while-vs-if](./condvar-while-vs-if/) | 条件变量等待用 `while` 还是 `if`：一次假唤醒就分出对错 | 第 30 章 §30.1–30.2 | 2 |
| [semaphore-binary-vs-counting](./semaphore-binary-vs-counting/) | 同一个信号量，初值 1 / 0 / 3 三种语义 | 第 31 章 §31.1–31.3 | 3 |
| [concurrency-anomalies](./concurrency-anomalies/) | 死锁 vs 活锁 vs 饥饿：现象差别 + 死锁的修法 | 第 32 章 §32.3、第 31 章 §31.5 | 4 |
| [raid-redundancy](./raid-redundancy/) | 条带化 vs 镜像 vs 奇偶校验：容量、写放大、故障恢复 | 第 38 章 §38.4–38.6 | 3 |
| [cache-locality](./cache-locality/) | 顺序访问 vs 跨步访问：结果相同，缓存命中天差地别 | 第 41 章 §41.1–41.5 | 2 |

合计 **7 个主题目录 / 18 个实现文件**。每个主题目录的 README 里有：对照表、**逐字教材引文**、跑法。

## 约定

1. 每个实现文件头 4 行元信息：`// @book` `// @topic` `// @source` `// @expect`
2. 单文件 ≤ 120 行，**只验证一个机制**
3. 故意演示的故障（竞态 / 死锁 / 饥饿）额外标 `// @ub: yes`，默认跑法会跳过
4. **教材引文只放 README，不进代码**
5. C 用 `gcc -std=c11 -O2 -Wall -Wextra`，C++ 用 `g++ -std=c++20 -O2 -Wall -Wextra -pthread`，零警告

## 跑法

```bash
cd lab

# 守卫: 目录结构 / 元信息 / 颗粒度
python3 -I tools/lab_check.py

# 语法检查(不产出可执行文件)
bash tools/build_all.sh

# 编译并运行全部(跳过 @ub: yes 的文件)
bash tools/run_all.sh

# 连故意演示 UB 的那批一起跑
bash tools/run_all.sh --ub
```

单个文件：

```bash
bash tools/check_one.sh books/os-concepts/condvar-while-vs-if/01-while-loop.c
bash tools/check_one.sh books/os-concepts/condvar-while-vs-if/02-if-loop.c --ub
```

## 运行环境

- 本批次的**验证环境**：Windows 11 + MSYS2 mingw-w64 GCC 16.1.0（ucrt64），零警告通过
- 依赖的可移植子集：POSIX 线程（`pthread_*`）、POSIX 信号量（`sem_*`）、`clock_gettime`、C11 `<stdatomic.h>`
  → 在 Linux + GCC/Clang 下同样可编译可运行；`build_all.sh` 需要 PATH 里有 `gcc`/`g++`
- **不依赖任何特权**：不写磁盘、不改系统状态、不留挂起进程；死锁实验用限时等待收尾，活锁/饥饿用定轮数或定时长收尾
- 性能类实验（锁粒度、局部性）的**绝对值依机器而定**，可对照的是两个文件的比值与结果一致性

## 本批次未覆盖（及原因）

按"宁缺毋滥"与"先读讲义再写"的要求，下面这些讲义章节**这批没有做**，原因如实记录：

| 讲义章节 | 为什么没做 |
| --- | --- |
| 第 4/5 章 进程 API、 第 26/28 章 进程 vs 线程（`fork` 开销 vs `pthread_create` 开销、内存共享与否） | 需要 POSIX `fork`/`waitpid`；本批验证环境的 mingw-w64 目标**没有** `fork`，写了就会让 `build_all.sh` 报 SYNTAX-FAIL。留待 Linux 侧补 |
| 第 6 章 受限直接执行（用户态/内核态切换成本） | 同上，依赖真实系统调用与 `strace` 类观测，且在 Windows 上语义不成立 |
| 第 13/15/19 章 地址空间与分页（`mmap` vs `brk`） | 本批验证环境**没有** `sys/mman.h`（mingw-w64 不提供） |
| 第 9 章 比例份额调度、 第 17 章 空闲空间管理、 第 47 章 分布式系统（RPC 重试 vs 幂等） | 无平台障碍，属纯逻辑模拟，但本批 7 个主题已覆盖并发核心 + 存储两条主线，这三个留待下一批 |

第 38 章的"校验和（checksum）vs 奇偶校验"对照同样**未按原样做**：讲义里没有 checksum 冗余方案，改用讲义真实存在的 **RAID-1 镜像 vs RAID-4 奇偶校验**（详见该主题 README 的口径说明）。

