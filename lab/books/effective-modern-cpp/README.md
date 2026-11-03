# Effective Modern C++

> 讲义来源：`书籍/教材讲义/Effective Modern C++/`（8 篇讲义）
> 原书：Scott Meyers, *Effective Modern C++* (O'Reilly, 2014)

## 主题目录

| 目录 | 讨论的问题 | 文件数 |
| --- | --- | --- |
| `auto-deduction-boundary/` | `auto` / `decltype` 的推导边界：数组退化、花括号例外、括号陷阱 | 4 |
| `shared-ptr-control-block/` | 同一裸指针造两个 `shared_ptr` 为何是 UB | 3 |
| `move-vs-copy-semantics/` | `std::move` 是转换不是搬家；SSO 与 RVO 的反直觉代价 | 4 |
| `perfect-forwarding/` | `std::forward` vs `std::move`；万能引用重载劫持拷贝构造 | 4 |
| `lambda-capture-lifetime/` | 默认捕获模式的悬垂：按引用、按值抓 `this`、init capture | 4 |
| `condvar-vs-future/` | 一次性事件通知：条件变量 vs 原子 flag 轮询 vs `void future` | 4 |
| `emplace-vs-push-back/` | 插入函数收「对象」，emplacement 函数收「构造参数」 | 3 |

## 本书约定

- 每个主题目录 = 一个「对照讨论」，至少 2 个实现文件各自可独立编译运行
- 文件头 `@ub: yes` 表示故意演示未定义行为，默认构建跳过，`run_all.sh --ub` 才跑
- 讲义原文引文放在主题 README 里，代码只保留机制本身
