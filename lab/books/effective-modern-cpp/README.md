# Effective Modern C++

> 讲义来源：`书籍/教材讲义/Effective Modern C++/`（8 篇讲义）
> 原书：Scott Meyers, *Effective Modern C++* (O'Reilly, 2014)

## 主题目录

| 目录 | 讨论的问题 | 文件数 |
| --- | --- | --- |
| `shared-ptr-control-block/` | 同一裸指针造两个 `shared_ptr` 为何是 UB | 3 |

## 本书约定

- 每个主题目录 = 一个「对照讨论」，至少 2 个实现文件各自可独立编译运行
- 文件头 `@ub: yes` 表示故意演示未定义行为，默认构建跳过，`run_all.sh --ub` 才跑
- 讲义原文引文放在主题 README 里，代码只保留机制本身
