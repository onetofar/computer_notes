# Linux多线程服务端编程

> 讲义来源：`书籍/教材讲义/Linux多线程服务端编程/`（第1~12章 + 附录A~D，共 16 篇）
> 原书：陈硕,《Linux多线程服务端编程：使用 muduo C++ 网络库》(电子工业出版社, 2013)

## 主题目录

每个主题目录是一个**对照讨论**：同一个问题的多种写法并列，各自独立编译运行，输出可直接横向对比。

| 目录 | 讨论的问题 | 文件数 |
| --- | --- | --- |
| `deadlock-lock-order/` | 两把锁反着加为什么会死锁，怎么验尸 | 4 |
| `lock-primitives/` | lock_guard / unique_lock / scoped_lock / std::lock 的分工 | 4 |
| `condition-variable-predicate/` | wait 为什么要用 while，signal 与 broadcast 怎么分工 | 3 |
| `copy-on-write-shared-ptr/` | 读要不要加锁：普通 mutex 与 shared_ptr copy-on-write | 4 |
| `object-lifetime-shared-ptr/` | 裸 this / 显式 shared_ptr / enable_shared_from_this | 3 |
| `reactor-models/` | 单线程串行、多线程加锁、one loop per thread | 3 |
| `epoll-echo-server/` | 手写阻塞 echo server 与手写 epoll reactor | 2 |

共 7 个主题目录、23 个实现文件、8 份 README。

## 本书约定

- 文件头 4 行是元信息：`@book` `@topic` `@source` `@expect`，第 5 行可能标 `@ub: yes`
- `@ub: yes` 表示**故意演示未定义行为**：默认构建跳过，`run_all.sh --ub` 才跑，期望检测器报错
- 死锁类演示一律用 `std::async` + `future::wait_for` **超时判定**，不会真的卡死进程
- 教材原话逐字引文只放主题 README，代码里只留机制
- 涉及 `epoll`/POSIX socket 的两个文件只在 Linux 编译执行；其它平台编译为提示信息，不报错

## 跑法

```bash
bash tools/lab_check.py     # 守卫: 目录结构 / 元信息 / 颗粒度
bash tools/run_all.sh       # 编译并运行全部 (跳过 ub 文件)
bash tools/run_all.sh --ub  # 连 ub 一起跑, 期望检测器报错
```

单文件手工跑：

```bash
g++ -std=c++20 -O2 -Wall -Wextra -pthread books/linux-multithread-server/deadlock-lock-order/02-mutual-deadlock.cpp -o /tmp/dd && /tmp/dd
```

## 本批未覆盖

讲义里第5章（高效日志）、第10章（编译链接模型）、第11章（虚函数开销）本轮未做对照实验。
