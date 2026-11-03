# 每次 new / delete，还是对象池借还

> 来源讲义：`书籍/教材讲义/网络多人在线游戏开发实战教程/第5章 性能优化与对象池（讲义）.md` §5.5

同一份"20 万个会话对象生下来、用完、扔掉"的负载，
三种写法跑同一件事：每次 `new`/`delete`、对象池借还、对象池借还但**忘了清场**。

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-new-each-time.cpp` | 20 万次 `new` + `delete`，每次都去堆上要一块 | `iterations=200000 ctor=200000 dtor=200000 bytes_each=36 peak_in_use=1`，耗时约 21 ms |
| `02-object-pool.cpp` | 启动时预造 1024 个，借出→写→`BackToPool()` 清场→归还 | `pool_size=1024 ctor=1024 dtor=0 borrow=200000 return=200000`——**构造次数从 20 万降到 1024**，耗时约 16 ms |
| `03-pool-without-clean.cpp` | 归还时漏掉 `BackToPool()` | `第二个借用者读到的残留: account="player-A" id=1001`；清场后则是 `account="" id=0` |

三份输出放在一起看：**池子把 20 万次堆分配压成 1024 次构造，
但池子本身不保证数据干净——清场是借用人自己的责任。**

## 教材原话

讲义 §5.5 先说清楚游戏服务器为什么需要池：

> 游戏服务器里大量对象**生得勤、死得也勤**：每个登录会话一个 Account、每条连接一个 ConnectObj、每个协议一个 Packet。每次都 `new` / `delete`，不仅要付堆分配的运行时开销，还欠下两笔账：忘了 delete 就是泄漏（valgrind 抓到的正是这个），跨线程忘了同步就是崩溃。

—— §5.5

讲义 §5.5 给出池的做法：

> 对象池的做法：**程序启动时预造一批对象，要用时从池里借，用完还回池里**。

—— §5.5

讲义 §5.5 特别强调归还方也要出力，这正是 `03` 演示的那件事：

> 还回来之前必须清干净，否则下一个借它的人会看到上一个人的残留。规范做法是池中对象的基类带一个虚函数 `BackToPool()`——由**对象自己**负责清空自己的数据。谁的数据谁清楚，池子只管借和收。

—— §5.5

讲义 §5.5 最后点出池子的额外收益：

> **池不只是省分配**——所有对象过池进出，内存监控从事后尸检变成实时点名。

—— §5.5

## 跑法

```bash
bash tools/run_all.sh

cd books/game-dev/object-pool-vs-new
g++ -std=c++20 -O2 -Wall -Wextra -pthread 01-new-each-time.cpp      -o /tmp/01 && /tmp/01
g++ -std=c++20 -O2 -Wall -Wextra -pthread 02-object-pool.cpp        -o /tmp/02 && /tmp/02
g++ -std=c++20 -O2 -Wall -Wextra -pthread 03-pool-without-clean.cpp -o /tmp/03 && /tmp/03
```

## 内存碎片怎么查

讲义 §5.5 讲的是"省掉分配"，**碎片**要靠工具看，本目录只给出对照代码、不下结论。
服务器上有 valgrind，可以这么看两次分配路径的差别：

```bash
valgrind --tool=massif --massif-out-file=/tmp/01.out ./01
valgrind --tool=massif --massif-out-file=/tmp/02.out ./02
ms_print /tmp/01.out | head -40
ms_print /tmp/02.out | head -40
```
