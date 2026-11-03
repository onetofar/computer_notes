# C++ Concurrency in Action

> 讲义来源：`书籍/教材讲义/C++ Concurrency in Action/`（11 章讲义 + 4 附录）
> 原书：Anthony Williams, *C++ Concurrency in Action* (2nd ed., 2019)

## 主题目录

| 目录 | 讨论的问题 | 文件数 |
| --- | --- | --- |
| `lock-granularity/` | 锁粒度如何决定并发吞吐 | 3 |

## 本书约定

- 线程类实验统一用 `steady_clock` 计时，输出可直接横向对比
- 涉及竞态的对照用 ASan/UBSan 跑，必须零报错才算通过
