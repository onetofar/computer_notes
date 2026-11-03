# 阻塞式 vs 非阻塞式 socket：睡死与白问

> 来源讲义：`书籍/教材讲义/网络多人在线游戏开发实战教程/第1章 网络编程基础（讲义）.md` §1.4 / §1.5

同一台机器、同一份 echo 逻辑，只改 Socket 的阻塞属性，
观察「调用返回时发生了什么」以及「为了不睡，付出了什么代价」。

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-blocking-echo.cpp` | 全程阻塞：`recv` 没数据就睡死（用 `SO_RCVTIMEO` 兜底 2 秒） | `[server] recv 4 bytes: ping` × 3；末尾 `[server] recv stopped n=0`（对端 FIN）与 `blocking echo done` |
| `02-nonblocking-poll-loop.cpp` | `O_NONBLOCK` + 主循环挨个问；同时统计「问了几次 / 命中几次」 | `rounds=46 recv_calls=45 data_hits=5 empty_calls=40 empty_ratio=88.9%`（轮数随机器负载浮动，空问占比稳定在 85%~95%） |

两份输出放在一起看：**阻塞版一次 `recv` 就拿到 4 字节，但代价是有数据前它一直睡着；
非阻塞版全程没睡死，代价是 40 次白问只换来 5 次命中。**

## 教材原话

讲义 §1.4 讲完阻塞式七步之后，立刻点出它的死穴：

> **阻塞模式的死穴：服务端在 `::accept`、`::recv` 上随时可能睡死，一个连接没动静就拖住所有人。**
> 解法是给 Socket 设非阻塞属性

—— §1.4 / §1.5

讲义 §1.5 说明设完属性之后调用行为的变化：

> 设完之后，原来会阻塞的函数**立刻返回**：accept 没新连接就返回 `INVALID_SOCKET`，recv 没数据就返回负数。

—— §1.5

讲义 §1.5 给出这份代价的量化方式：

> 程序能正常跑（示例里开了 3 个线程各发 ping），但效率账很难看：假设每秒 1000 个循环、10 个连接，就是每秒上万次函数调用，其中**绝大多数是白问**。

—— §1.5

本章金句第 6 条把两半合在一起：

> **非阻塞 = 不睡觉但白问**——轮询保证了响应性，代价是海量无效调用，这就是下一章 select/epoll 存在的理由。

—— §1 收尾

## 跑法

```bash
bash tools/run_all.sh

cd books/game-dev/blocking-vs-nonblocking
g++ -std=c++20 -O2 -Wall -Wextra -pthread 01-blocking-echo.cpp -o /tmp/01 && /tmp/01
g++ -std=c++20 -O2 -Wall -Wextra -pthread 02-nonblocking-poll-loop.cpp -o /tmp/02 && /tmp/02
```

**平台说明**：两个文件都走 POSIX socket（`127.0.0.1` 回环，端口由内核临时分配），
非 Linux 平台只打印一行提示就退出。进程内埋了 `alarm(15)` 兜底，
任何异常都不会留下挂起进程或占用端口。
