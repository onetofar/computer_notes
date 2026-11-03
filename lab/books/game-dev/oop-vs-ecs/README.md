# 继承 + 虚函数 vs 组合 + 数据驱动：一个空 Update 的代价

> 来源讲义：`书籍/教材讲义/网络多人在线游戏开发实战教程/第6章 搭建ECS框架（讲义）.md` §6.1

同一个"64 个实体，每帧更新一次"的场景，两种组织方式：
一个用继承（父类强制每个子类实现 `Update()`），一个用迷你 ECS（**按需实现接口**）。

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-oop-inheritance.cpp` | 继承模型：8 个 `Account` 真的需要更新，56 个 `AccountStatic` 只能写个空 `Update()` 交差 | `entities=64 frames=100 virtual_calls=6400 effective_updates=800 empty_updates=5600`、`empty_ratio=87.5%` |
| `02-ecs-composition.cpp` | 迷你 ECS：`dynamic_cast` 探测接口，实现了 `IUpdateSystem` 的才进更新队列 | `virtual_calls=800 effective_updates=800 empty_updates=0`、`update_queue_size=8`——**同样 100 帧，虚调用少 8 倍** |
| `03-ecs-init-before-update.cpp` | 依赖顺序：`Shield.Update()` 要读 `Health` 的值 | `先初始化后更新: Shield 读到 Health = 42`；`先更新后初始化: ... = 0`（顺序颠倒就读到脏数据） |

三份输出放在一起看：**继承把"父类有的"强加给所有子类，于是 87.5% 的每帧调用是空转；
ECS 让不干活的对象不进队列，代价是每帧一次 `dynamic_cast` 探测 + 一次依赖顺序检查。**

## 教材原话

讲义第 0 节用一个小尴尬点题：

> 第 4 章的框架里有个很不起眼的小尴尬：login 工程的 `Account` 类继承自 `ThreadObject`，而 `ThreadObject` 规定所有子类必须实现 `Update()`——但 `Account` 根本不需要每帧更新，于是只能写一个空的 `Update()` 交差。
>
> 一个空函数不算事故，但它指着整个框架的结构性欠账：**继承是把「父类有的」强加给「所有子类」**。

—— 第 0 节

讲义 §6.1 给出 ECS 里实体与组件的判据：

> - **一个类下面还要挂组件 → 它是实体**；
> - **一个类不再需要更小的组件 → 它是最基础的组件**。

—— §6.1

讲义 §6.1 说系统从接口开始，这正是"空函数消失"的地方：

> 系统先以接口形式出现：`IInitializeSystem`（要初始化的实现 `Initialize()`）、`IUpdateSystem`（要帧更新的实现 `Update()`）。一个对象**按需实现**：不更新？那就别实现这个接口，也不必再写空函数——第 0 节的尴尬就此消失。

—— §6.1

讲义 §6.1 写下每帧更新的铁律：

> 每个帧的 `Update()` 有一条铁律——**初始化队列清完，才轮到更新队列**

—— §6.1

讲义 §6.1 解释为什么值得较这个真：

> 为什么要较这个真？看依赖场景：A 的 `Update()` 要用 B 的对象。如果 B 还没初始化就被 A 摸到，轻则空指针重则脏数据。变通写法是每次 Update 前都 `if (!HasComponent<B>()) return;`——每帧都查一遍，显然不体面。**依赖检查放到初始化做一次，比在更新里查一万次强。**

—— §6.1

## 跑法

```bash
bash tools/run_all.sh

cd books/game-dev/oop-vs-ecs
g++ -std=c++20 -O2 -Wall -Wextra -pthread 01-oop-inheritance.cpp   -o /tmp/01 && /tmp/01
g++ -std=c++20 -O2 -Wall -Wextra -pthread 02-ecs-composition.cpp    -o /tmp/02 && /tmp/02
g++ -std=c++20 -O2 -Wall -Wextra -pthread 03-ecs-init-before-update.cpp -o /tmp/03 && /tmp/03
```

**注意**：`01` 里那个"空 `Update()`"并非真的什么都不做——它只把调用次数记进计数器，
否则就没法把"空转占比"量出来；业务语义上它仍然等价于一个空函数。
