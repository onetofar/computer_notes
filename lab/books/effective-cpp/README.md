# Effective C++

> 讲义来源：`书籍/教材讲义/Effective C++/`（56 篇讲义：Item 1~55 + 附录 A）
> 原书：Scott Meyers, *Effective C++*（第三版中文版, 机械工业出版社）

## 主题目录

| 目录 | 讨论的问题 | 文件数 |
| --- | --- | --- |
| `member-init-order/` | 构造函数体内赋值 vs 成员初始化列表；初始化顺序由**声明顺序**决定 | 3 |
| `compiler-generated-functions/` | 编译器隐式生成的函数 vs 显式拒绝（`Uncopyable` 基类 vs `= delete`） | 4 |
| `polymorphic-base-dtor/` | 多态基类的虚析构 vs 非成员非友元便利函数 | 4 |
| `self-assignment/` | 自赋值安全：identity test vs copy-and-swap；`operator=` 的返回类型 | 4 |
| `resource-management-raii/` | RAII 与 `shared_ptr`；`new` 与智能指针之间的泄漏窗口 | 4 |
| `return-object-vs-reference/` | 返回对象 vs 返回引用；返回内部构件的「句柄」 | 4 |
| `exception-safety-levels/` | 异常安全四档：无保证 / 基本保证 / 强保证 / 不抛保证 | 4 |

## 本书约定

- 每个主题目录 = 一个「对照讨论」，至少 2 个实现文件各自可独立编译运行
- 文件头 `@ub: yes` 表示故意演示未定义行为，默认构建跳过，`run_all.sh --ub` 才跑
- 讲义原文引文放在主题 README 里，代码只保留机制本身
